// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Modem Host Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*  Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "Host.h"
#include "HostMain.h"

using namespace modem;

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

// Macro to start DMR duplex idle transmission (or beacon)
#define START_DMR_DUPLEX_IDLE(x)                                                                                        \
    if (host->m_dmr != nullptr) {                                                                                       \
        if (host->m_duplex && !host->m_dmrTXTimer.isRunning()) {                                                        \
            host->m_modem->writeDMRStart(x);                                                                            \
            host->m_dmrTXTimer.start();                                                                                 \
        }                                                                                                               \
    }

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Entry point to read DMR slot 1 frames from modem Rx queue. */

void* Host::threadDMRReader1(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
        ::pthread_detach(th->thread);

        std::string threadName("dmrd:frame1-r");
        Host* host = static_cast<Host*>(th->obj);
        if (host == nullptr) {
            g_killed = true;
            LogDebug(LOG_HOST, "[FAIL] %s", threadName.c_str());
        }

        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogDebug(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        if (host->m_dmr != nullptr) {
            while (!g_killed) {
                // scope is intentional
                {
                    // ------------------------------------------------------
                    //  -- Read from Modem Processing                     --
                    // ------------------------------------------------------

                    uint8_t data[DMRDEF::DMR_FRAME_LENGTH_BYTES * 2U];
                    auto afterReadCallback = [&]() {
                        if (host->m_dmr != nullptr) {
                            host->interruptDMRBeacon();
                        }

                        // if there is a P25 CC running; halt the CC
                        if (host->m_p25 != nullptr) {
                            if (host->m_p25->getCCRunning() && !host->m_p25->getCCHalted()) {
                                host->interruptP25Control();
                            }
                        }

                        // if there is a NXDN CC running; halt the CC
                        if (host->m_nxdn != nullptr) {
                            if (host->m_nxdn->getCCRunning() && !host->m_nxdn->getCCHalted()) {
                                host->interruptNXDNControl();
                            }
                        }
                    };

                    if (host->m_dmr != nullptr) {
                        uint8_t nextLen = host->m_modem->peekDMRFrame1Length();
                        if (nextLen > 0U) {
                            // read DMR slot 1 frames from the modem, and if there is any
                            // write those frames to the DMR controller
                            uint32_t len = host->m_modem->readDMRFrame1(data);
                            if (len > 0U) {
                                if (host->m_state == STATE_IDLE) {
                                    // if the modem is in duplex -- process wakeup CSBKs
                                    if (host->m_duplex) {
                                        bool ret = host->m_dmr->processWakeup(data);
                                        if (ret) {
                                            host->m_modeTimer.setTimeout(host->m_rfModeHang);
                                            host->setState(STATE_DMR);

                                            START_DMR_DUPLEX_IDLE(true);

                                            afterReadCallback();
                                        }
                                    }
                                    else {
                                        // in simplex directly process slot 1 frames
                                        host->m_modeTimer.setTimeout(host->m_rfModeHang);
                                        host->setState(STATE_DMR);
                                        START_DMR_DUPLEX_IDLE(true);

                                        host->m_dmr->processFrame(1U, data, len);

                                        afterReadCallback();
                                    }
                                }
                                else if (host->m_state == STATE_DMR) {
                                    // if the modem is in duplex, and hasn't started transmitting
                                    // process wakeup CSBKs
                                    if (host->m_duplex && !host->m_modem->hasTX()) {
                                        bool ret = host->m_dmr->processWakeup(data);
                                        if (ret) {
                                            host->m_modem->writeDMRStart(true);
                                            host->m_dmrTXTimer.start();
                                        }
                                    }
                                    else {
                                        // process slot 1 frames
                                        bool ret = host->m_dmr->processFrame(1U, data, len);
                                        if (ret) {
                                            afterReadCallback();

                                            host->m_modeTimer.start();
                                            if (host->m_duplex)
                                                host->m_dmrTXTimer.start();
                                        }
                                    }
                                }
                                else if (host->m_state != HOST_STATE_LOCKOUT) {
                                    LogWarning(LOG_HOST, "DMR modem data received, state = %u", host->m_state);
                                }
                            }
                        }
                    }
                }

                if (host->m_state != STATE_IDLE)
                    Thread::sleep(m_activeTickDelay);
                if (host->m_state == STATE_IDLE)
                    Thread::sleep(m_idleTickDelay);
            }
        }

        LogDebug(LOG_HOST, "[STOP] %s", threadName.c_str());
        delete th;
    }

    return nullptr;
}

/* Entry point to write DMR slot 1 frames to modem. */

void* Host::threadDMRWriter1(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
        ::pthread_detach(th->thread);

        std::string threadName("dmrd:frame1-w");
        Host* host = static_cast<Host*>(th->obj);
        if (host == nullptr) {
            g_killed = true;
            LogDebug(LOG_HOST, "[FAIL] %s", threadName.c_str());
        }

        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogDebug(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        StopWatch stopWatch;
        stopWatch.start();

        if (host->m_dmr != nullptr) {
            while (!g_killed) {
                host->m_dmrTx1WatchdogTimer.start();

                uint32_t ms = stopWatch.elapsed();
                stopWatch.start();
                host->m_dmrTx1LoopMS = ms;

                // scope is intentional
                {
                    std::lock_guard<std::mutex> lock(m_clockingMutex);

                    // ------------------------------------------------------
                    //  -- Write to Modem Processing                      --
                    // ------------------------------------------------------

                    uint8_t data[DMRDEF::DMR_FRAME_LENGTH_BYTES * 2U];
                    auto afterWriteCallback = [&]() {
                        // if there is a P25 CC running; halt the CC
                        if (host->m_p25 != nullptr) {
                            if (host->m_p25->getCCRunning() && !host->m_p25->getCCHalted()) {
                                host->interruptP25Control();
                            }
                        }

                        // if there is a NXDN CC running; halt the CC
                        if (host->m_nxdn != nullptr) {
                            if (host->m_nxdn->getCCRunning() && !host->m_nxdn->getCCHalted()) {
                                host->interruptNXDNControl();
                            }
                        }
                    };

                    if (host->m_dmr != nullptr) {
                        // check if there is space on the modem for DMR slot 1 frames,
                        // if there is read frames from the DMR controller and write it
                        // to the modem
                        bool ret = host->m_modem->hasDMRSpace1();
                        if (ret) {
                            uint32_t nextLen = host->m_dmr->peekFrameLength(1U);
                            if (host->m_dmrCtrlChannel) {
                                if (host->m_dmrDedicatedTxTestTimer.hasExpired() && !host->m_dmrDedicatedTxTestTimer.isPaused()) {
                                    host->m_dmrDedicatedTxTestTimer.pause();
                                    if (!host->m_modem->hasTX() && host->m_modem->gotModemStatus() && host->m_state == STATE_DMR && (host->m_dmr->getTSCCSlotNo() == 1U) && host->m_dmr->getCCRunning()) {
                                        LogError(LOG_HOST, "DMR dedicated m_dmr not transmitting, running = %u, halted = %u, frameLength1 = %u", host->m_dmr->getCCRunning(), host->m_dmr->getCCHalted(), nextLen);
                                    }
                                }
                            }

                            uint32_t len = host->m_dmr->getFrame(1U, data);
                            if (len > 0U) {
                                // if the state is idle; set to DMR, start mode timer and start DMR idle frames
                                if (host->m_state == STATE_IDLE) {
                                    host->m_modeTimer.setTimeout(host->m_netModeHang);
                                    host->setState(STATE_DMR);
                                    START_DMR_DUPLEX_IDLE(true);
                                }

                                // if the state is DMR; start DMR idle frames and write DMR slot 1 data
                                if (host->m_state == STATE_DMR) {
                                    START_DMR_DUPLEX_IDLE(true);

                                    host->m_modem->writeDMRFrame1(data, len);

                                    // if there is no DMR CC running; run the interrupt macro to stop
                                    // any running DMR beacon
                                    if (!host->m_dmr->getCCRunning()) {
                                        host->interruptDMRBeacon();
                                    }

                                    afterWriteCallback();

                                    host->m_modeTimer.start();
                                }

                                host->m_lastDstId = host->m_dmr->getLastDstId(1U);
                                host->m_lastSrcId = host->m_dmr->getLastSrcId(1U);
                            }
                        }
                    }
                }

                if (host->m_state != STATE_IDLE)
                    Thread::sleep(m_activeTickDelay);
                if (host->m_state == STATE_IDLE)
                    Thread::sleep(m_idleTickDelay);
            }
        }

        LogDebug(LOG_HOST, "[STOP] %s", threadName.c_str());
        delete th;
    }

    return nullptr;
}

/* Entry point to read DMR slot 2 frames from modem Rx queue. */

void* Host::threadDMRReader2(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
        ::pthread_detach(th->thread);

        std::string threadName("dmrd:frame2-r");
        Host* host = static_cast<Host*>(th->obj);
        if (host == nullptr) {
            g_killed = true;
            LogDebug(LOG_HOST, "[FAIL] %s", threadName.c_str());
        }

        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogDebug(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        if (host->m_dmr != nullptr) {
            while (!g_killed) {
                // scope is intentional
                {
                    // ------------------------------------------------------
                    //  -- Read from Modem Processing                     --
                    // ------------------------------------------------------

                    uint8_t data[DMRDEF::DMR_FRAME_LENGTH_BYTES * 2U];
                    auto afterReadCallback = [&]() {
                        if (host->m_dmr != nullptr) {
                            host->interruptDMRBeacon();
                        }

                        // if there is a P25 CC running; halt the CC
                        if (host->m_p25 != nullptr) {
                            if (host->m_p25->getCCRunning() && !host->m_p25->getCCHalted()) {
                                host->interruptP25Control();
                            }
                        }

                        // if there is a NXDN CC running; halt the CC
                        if (host->m_nxdn != nullptr) {
                            if (host->m_nxdn->getCCRunning() && !host->m_nxdn->getCCHalted()) {
                                host->interruptNXDNControl();
                            }
                        }
                    };

                    if (host->m_dmr != nullptr) {
                        uint8_t nextLen = host->m_modem->peekDMRFrame2Length();
                        if (nextLen > 0U) {
                            // read DMR slot 2 frames from the modem, and if there is any
                            // write those frames to the DMR controller
                            uint32_t len = host->m_modem->readDMRFrame2(data);
                            if (len > 0U) {
                                if (host->m_state == STATE_IDLE) {
                                    // if the modem is in duplex -- process wakeup CSBKs
                                    if (host->m_duplex) {
                                        bool ret = host->m_dmr->processWakeup(data);
                                        if (ret) {
                                            host->m_modeTimer.setTimeout(host->m_rfModeHang);
                                            host->setState(STATE_DMR);
                                            START_DMR_DUPLEX_IDLE(true);

                                            afterReadCallback();
                                        }
                                    }
                                    else {
                                        // in simplex -- directly process slot 2 frames
                                        host->m_modeTimer.setTimeout(host->m_rfModeHang);
                                        host->setState(STATE_DMR);
                                        START_DMR_DUPLEX_IDLE(true);

                                        host->m_dmr->processFrame(2U, data, len);

                                        afterReadCallback();
                                    }
                                }
                                else if (host->m_state == STATE_DMR) {
                                    // if the modem is in duplex, and hasn't started transmitting
                                    // process wakeup CSBKs
                                    if (host->m_duplex && !host->m_modem->hasTX()) {
                                        bool ret = host->m_dmr->processWakeup(data);
                                        if (ret) {
                                            host->m_modem->writeDMRStart(true);
                                            host->m_dmrTXTimer.start();
                                        }
                                    }
                                    else {
                                        // process slot 2 frames
                                        bool ret = host->m_dmr->processFrame(2U, data, len);
                                        if (ret) {
                                            afterReadCallback();

                                            host->m_modeTimer.start();
                                            if (host->m_duplex)
                                                host->m_dmrTXTimer.start();
                                        }
                                    }
                                }
                                else if (host->m_state != HOST_STATE_LOCKOUT) {
                                    LogWarning(LOG_HOST, "DMR modem data received, state = %u", host->m_state);
                                }
                            }
                        }
                    }
                }

                if (host->m_state != STATE_IDLE)
                    Thread::sleep(m_activeTickDelay);
                if (host->m_state == STATE_IDLE)
                    Thread::sleep(m_idleTickDelay);
            }
        }

        LogDebug(LOG_HOST, "[STOP] %s", threadName.c_str());
        delete th;
    }

    return nullptr;
}

/* Entry point to write DMR slot 2 frames to modem. */

void* Host::threadDMRWriter2(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
        ::pthread_detach(th->thread);

        std::string threadName("dmrd:frame2-w");
        Host* host = static_cast<Host*>(th->obj);
        if (host == nullptr) {
            g_killed = true;
            LogDebug(LOG_HOST, "[FAIL] %s", threadName.c_str());
        }

        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogDebug(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        StopWatch stopWatch;
        stopWatch.start();

        if (host->m_dmr != nullptr) {
            while (!g_killed) {
                host->m_dmrTx2WatchdogTimer.start();

                uint32_t ms = stopWatch.elapsed();
                stopWatch.start();
                host->m_dmrTx2LoopMS = ms;

                // scope is intentional
                {
                    std::lock_guard<std::mutex> lock(m_clockingMutex);

                    // ------------------------------------------------------
                    //  -- Write to Modem Processing                      --
                    // ------------------------------------------------------

                    uint8_t data[DMRDEF::DMR_FRAME_LENGTH_BYTES * 2U];
                    auto afterWriteCallback = [&]() {
                        // if there is a P25 CC running; halt the CC
                        if (host->m_p25 != nullptr) {
                            if (host->m_p25->getCCRunning() && !host->m_p25->getCCHalted()) {
                                host->interruptP25Control();
                            }
                        }

                        // if there is a NXDN CC running; halt the CC
                        if (host->m_nxdn != nullptr) {
                            if (host->m_nxdn->getCCRunning() && !host->m_nxdn->getCCHalted()) {
                                host->interruptNXDNControl();
                            }
                        }
                    };

                    if (host->m_dmr != nullptr) {
                        // check if there is space on the modem for DMR slot 2 frames,
                        // if there is read frames from the DMR controller and write it
                        // to the modem
                        bool ret = host->m_modem->hasDMRSpace2();
                        if (ret) {
                            uint32_t nextLen = host->m_dmr->peekFrameLength(1U);
                            if (host->m_dmrCtrlChannel) {
                                if (host->m_dmrDedicatedTxTestTimer.hasExpired() && !host->m_dmrDedicatedTxTestTimer.isPaused()) {
                                    host->m_dmrDedicatedTxTestTimer.pause();
                                    if (!host->m_modem->hasTX() && host->m_modem->gotModemStatus() && host->m_state == STATE_DMR && (host->m_dmr->getTSCCSlotNo() == 2U) && host->m_dmr->getCCRunning()) {
                                        LogError(LOG_HOST, "DMR dedicated m_dmr not transmitting, running = %u, halted = %u, frameLength2 = %u", host->m_dmr->getCCRunning(), host->m_dmr->getCCHalted(), nextLen);
                                    }
                                }
                            }

                            uint32_t len = host->m_dmr->getFrame(2U, data);
                            if (len > 0U) {
                                // if the state is idle; set to DMR, start mode timer and start DMR idle frames
                                if (host->m_state == STATE_IDLE) {
                                    host->m_modeTimer.setTimeout(host->m_netModeHang);
                                    host->setState(STATE_DMR);
                                    START_DMR_DUPLEX_IDLE(true);
                                }

                                // if the state is DMR; start DMR idle frames and write DMR slot 2 data
                                if (host->m_state == STATE_DMR) {
                                    START_DMR_DUPLEX_IDLE(true);

                                    host->m_modem->writeDMRFrame2(data, len);

                                    // if there is no DMR CC running; run the interrupt macro to stop
                                    // any running DMR beacon
                                    if (!host->m_dmr->getCCRunning()) {
                                        host->interruptDMRBeacon();
                                    }

                                    afterWriteCallback();

                                    host->m_modeTimer.start();
                                }

                                host->m_lastDstId = host->m_dmr->getLastDstId(2U);
                                host->m_lastSrcId = host->m_dmr->getLastSrcId(2U);
                            }
                        }
                    }
                }

                if (host->m_state != STATE_IDLE)
                    Thread::sleep(m_activeTickDelay);
                if (host->m_state == STATE_IDLE)
                    Thread::sleep(m_idleTickDelay);
            }
        }

        LogDebug(LOG_HOST, "[STOP] %s", threadName.c_str());
        delete th;
    }

    return nullptr;
}

/* Helper to interrupt a running DMR beacon. */

void Host::interruptDMRBeacon()
{
    if (m_dmr != nullptr) {
        if (m_dmrBeaconDurationTimer.isRunning() && !m_dmrBeaconDurationTimer.hasExpired()) {
            if (m_dmrTSCCData && !m_dmrCtrlChannel) {
                LogDebug(LOG_HOST, "interrupt DMR m_dmr, m_state = %u", m_state);
                m_dmr->setCCHalted(true);
                m_dmr->setCCRunning(false);
            }
        }

        m_dmrBeaconDurationTimer.stop();
    }
}
