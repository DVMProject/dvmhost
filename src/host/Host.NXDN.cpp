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
//  Private Class Members
// ---------------------------------------------------------------------------

/* Entry point to read NXDN frames from modem Rx queue. */

void* Host::threadNXDNReader(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
#if defined(_WIN32)
        ::CloseHandle(th->thread);
#else
        ::pthread_detach(th->thread);
#endif // defined(_WIN32)

        std::string threadName("nxdd:frame-r");
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

        if (host->m_nxdn != nullptr) {
            while (!g_killed) {
                uint32_t ms = stopWatch.elapsed();
                stopWatch.start();

                // scope is intentional
                {
                    // ------------------------------------------------------
                    //  -- Read from Modem Processing                     --
                    // ------------------------------------------------------

                    uint8_t data[NXDDEF::NXDN_FRAME_LENGTH_BYTES * 2U];
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
                    };

                    if (host->m_nxdn != nullptr) {
                        uint8_t nextLen = host->m_modem->peekNXDNFrameLength();
                        if (nextLen > 0U) {
                            uint32_t len = host->m_modem->readNXDNFrame(data);
                            if (len > 0U) {
                                if (host->m_state == STATE_IDLE) {
                                    // process NXDN frames
                                    bool ret = host->m_nxdn->processFrame(data, len);
                                    if (ret) {
                                        host->m_modeTimer.setTimeout(host->m_rfModeHang);
                                        host->setState(STATE_NXDN);

                                        afterReadCallback();
                                    }
                                }
                                else if (host->m_state == STATE_NXDN) {
                                    // process NXDN frames
                                    bool ret = host->m_nxdn->processFrame(data, len);
                                    if (ret) {
                                        host->m_modeTimer.start();
                                    }
                                }
                                else if (host->m_state != HOST_STATE_LOCKOUT) {
                                    LogWarning(LOG_HOST, "NXDN modem data received, state = %u", host->m_state);
                                }
                            }

                            // were frames received while still in an reject state? if so, reset the timer
                            if (host->m_nxdn->getRFState() == RS_RF_REJECTED) {
                                host->m_nxdnRejectTimer.start();
                                host->m_nxdnRejCnt++;
                            }
                        } else {
                            // if we're receiving no more frames, and we're in a reject state, clear the state
                            if (host->m_nxdn->getRFState() == RS_RF_REJECTED) {
                                host->m_nxdnRejectTimer.clock(ms);
                                if (host->m_nxdnRejectTimer.hasExpired()) {
                                    LogMessage(LOG_HOST, "NXDN, reset from previous call reject, frames = %u", host->m_nxdnRejCnt);
                                    host->m_nxdnRejectTimer.stop();
                                    host->m_nxdn->clearRFReject();
                                    host->m_nxdnRejCnt = 0U;
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

/* Entry point to write NXDN frames to modem. */

void* Host::threadNXDNWriter(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
#if defined(_WIN32)
        ::CloseHandle(th->thread);
#else
        ::pthread_detach(th->thread);
#endif // defined(_WIN32)

        std::string threadName("nxdd:frame-w");
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

        if (host->m_nxdn != nullptr) {
            while (!g_killed) {
                host->m_nxdnTxWatchdogTimer.start();

                uint32_t ms = stopWatch.elapsed();
                stopWatch.start();
                host->m_nxdnTxLoopMS = ms;

                // scope is intentional
                {
                    std::lock_guard<std::mutex> lock(m_clockingMutex);

                    // ------------------------------------------------------
                    //  -- Write to Modem Processing                      --
                    // ------------------------------------------------------

                    uint8_t data[NXDDEF::NXDN_FRAME_LENGTH_BYTES * 2U];
                    auto afterWriteCallback = [&]() {
                        if (host->m_dmr != nullptr) {
                            host->interruptDMRBeacon();
                        }

                        // if there is a P25 CC running; halt the CC
                        if (host->m_p25 != nullptr) {
                            if (host->m_p25->getCCRunning() && !host->m_p25->getCCHalted()) {
                                host->interruptP25Control();
                            }
                        }
                    };

                    // check if there is space on the modem for NXDN frames,
                    // if there is read frames from the NXDN controller and write it
                    // to the modem
                    if (host->m_nxdn != nullptr) {
                        bool ret = host->m_modem->hasNXDNSpace();
                        if (ret) {
                            uint32_t nextLen = host->m_nxdn->peekFrameLength();
                            if (host->m_nxdnCtrlChannel) {
                                if (host->m_nxdnDedicatedTxTestTimer.hasExpired() && !host->m_nxdnDedicatedTxTestTimer.isPaused()) {
                                    host->m_nxdnDedicatedTxTestTimer.pause();
                                    if (!host->m_modem->hasTX() && host->m_modem->gotModemStatus() && host->m_state == STATE_NXDN && host->m_nxdn->getCCRunning()) {
                                        LogError(LOG_HOST, "NXDN dedicated CC stopped transmitting, running = %u, halted = %u, frameLength = %u", host->m_nxdn->getCCRunning(), host->m_nxdn->getCCHalted(), nextLen);
                                    }
                                }
                            }

                            uint32_t len = host->m_nxdn->getFrame(data);
                            if (len > 0U) {
                                // if the state is idle; set to NXDN and start mode timer
                                if (host->m_state == STATE_IDLE) {
                                    host->m_modeTimer.setTimeout(host->m_netModeHang);
                                    host->setState(STATE_NXDN);
                                }

                                // if the state is NXDN; write NXDN data
                                if (host->m_state == STATE_NXDN) {
                                    host->m_modem->writeNXDNFrame(data, len);

                                    afterWriteCallback();

                                    host->m_modeTimer.start();
                                }

                                host->m_lastDstId = host->m_nxdn->getLastDstId();
                                host->m_lastSrcId = host->m_nxdn->getLastSrcId();
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

/* Helper to interrupt a running NXDN control channel. */

void Host::interruptNXDNControl()
{
    if (m_nxdn != nullptr) {
        LogDebug(LOG_HOST, "interrupt NXDN m_nxdn, m_state = %u", m_state);
        m_nxdn->setCCHalted(true);

        if (m_nxdnBcastDurationTimer.isRunning() && !m_nxdnBcastDurationTimer.isPaused()) {
            m_nxdnBcastDurationTimer.pause();
        }
    }
}
