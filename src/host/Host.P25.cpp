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

/* Entry point to read P25 frames from modem Rx queue. */

void* Host::threadP25Reader(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
        ::pthread_detach(th->thread);

        std::string threadName("p25d:frame-r");
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

        if (host->m_p25 != nullptr) {
            while (!g_killed) {
                // scope is intentional
                {
                    // ------------------------------------------------------
                    //  -- Read from Modem Processing                     --
                    // ------------------------------------------------------

                    uint8_t data[P25DEF::P25_PDU_FRAME_LENGTH_BYTES * 2U];
                    auto afterReadCallback = [&]() {
                        if (host->m_dmr != nullptr) {
                            host->interruptDMRBeacon();
                        }

                        // if there is a NXDN CC running; halt the CC
                        if (host->m_nxdn != nullptr) {
                            if (host->m_nxdn->getCCRunning() && !host->m_nxdn->getCCHalted()) {
                                host->interruptNXDNControl();
                            }
                        }
                    };

                    // read P25 frames from modem, and if there are frames
                    // write those frames to the P25 controller
                    if (host->m_p25 != nullptr) {
                        uint8_t nextLen = host->m_modem->peekP25FrameLength();
                        if (nextLen > 0U) {
                            uint32_t len = host->m_modem->readP25Frame(data);
                            if (len > 0U) {
                                if (host->m_state == STATE_IDLE) {
                                    // process P25 frames
                                    bool ret = host->m_p25->processFrame(data, len);
                                    if (ret) {
                                        host->m_modeTimer.setTimeout(host->m_rfModeHang);
                                        host->setState(STATE_P25);

                                        afterReadCallback();
                                    }
                                    else {
                                        ret = host->m_p25->writeRF_VoiceEnd();
                                        if (ret) {
                                            afterReadCallback();

                                            if (host->m_state == STATE_IDLE) {
                                                host->m_modeTimer.setTimeout(host->m_rfModeHang);
                                                host->setState(STATE_P25);
                                            }

                                            if (host->m_state == STATE_P25) {
                                                host->m_modeTimer.start();
                                            }

                                            // if the modem is in duplex -- handle P25 CC burst m_p25
                                            if (host->m_duplex) {
                                                if (host->m_p25BcastDurationTimer.isPaused() && !host->m_p25->getCCHalted()) {
                                                    host->m_p25BcastDurationTimer.resume();
                                                }

                                                if (host->m_p25->getCCHalted()) {
                                                    host->m_p25->setCCHalted(false);
                                                }

                                                if (g_fireP25Control) {
                                                    host->m_modeTimer.stop();
                                                }
                                            }
                                            else {
                                                host->m_p25BcastDurationTimer.stop();
                                            }
                                        }
                                    }
                                }
                                else if (host->m_state == STATE_P25) {
                                    // process P25 frames
                                    bool ret = host->m_p25->processFrame(data, len);
                                    if (ret) {
                                        host->m_modeTimer.start();
                                    }
                                    else {
                                        ret = host->m_p25->writeRF_VoiceEnd();
                                        if (ret) {
                                            host->m_modeTimer.start();
                                        }
                                    }
                                }
                                else if (host->m_state != HOST_STATE_LOCKOUT) {
                                    LogWarning(LOG_HOST, "P25 modem data received, state = %u", host->m_state);
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

/* Entry point to write P25 frames to modem. */

void* Host::threadP25Writer(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
        ::pthread_detach(th->thread);

        std::string threadName("p25d:frame-w");
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

        if (host->m_p25 != nullptr) {
            while (!g_killed) {
                host->m_p25TxWatchdogTimer.start();

                uint32_t ms = stopWatch.elapsed();
                stopWatch.start();
                host->m_p25TxLoopMS = ms;

                // scope is intentional
                {
                    std::lock_guard<std::mutex> lock(m_clockingMutex);

                    // ------------------------------------------------------
                    //  -- Write to Modem Processing                      --
                    // ------------------------------------------------------

                    uint8_t data[P25DEF::P25_PDU_FRAME_LENGTH_BYTES * 2U];
                    auto afterWriteCallback = [&]() {
                        if (host->m_dmr != nullptr) {
                            host->interruptDMRBeacon();
                        }

                        // if there is a NXDN CC running; halt the CC
                        if (host->m_nxdn != nullptr) {
                            if (host->m_nxdn->getCCRunning() && !host->m_nxdn->getCCHalted()) {
                                host->interruptNXDNControl();
                            }
                        }
                    };

                    // check if there is space on the modem for P25 frames,
                    // if there is read frames from the P25 controller and write it
                    // to the modem
                    if (host->m_p25 != nullptr) {
                        uint8_t nextLen = host->m_p25->peekFrameLength();
                        if (host->m_p25CtrlChannel) {
                            if (host->m_p25DedicatedTxTestTimer.hasExpired() && !host->m_p25DedicatedTxTestTimer.isPaused()) {
                                host->m_p25DedicatedTxTestTimer.pause();
                                if (!host->m_modem->hasTX() && host->m_modem->gotModemStatus() && host->m_state == STATE_P25 && host->m_p25->getCCRunning()) {
                                    LogError(LOG_HOST, "P25 dedicated m_p25 not transmitting, running = %u, halted = %u, frameLength = %u", host->m_p25->getCCRunning(), host->m_p25->getCCHalted(), nextLen);
                                }
                            }
                        }

                        if (nextLen > 0U) {
                            bool ret = host->m_modem->hasP25Space(nextLen);
                            if (ret) {
                                uint32_t len = host->m_p25->getFrame(data);
                                if (len > 0U) {
                                    // if the state is idle; set to P25 and start mode timer
                                    if (host->m_state == STATE_IDLE) {
                                        host->m_modeTimer.setTimeout(host->m_netModeHang);
                                        host->setState(STATE_P25);
                                    }

                                    // if the state is P25; write P25 frame data
                                    if (host->m_state == STATE_P25) {
                                        host->m_modem->writeP25Frame(data, len);

                                        afterWriteCallback();

                                        host->m_modeTimer.start();
                                    }

                                    host->m_lastDstId = host->m_p25->getLastDstId();
                                    host->m_lastSrcId = host->m_p25->getLastSrcId();
                                }
                                else {
                                    nextLen = 0U;
                                }
                            }
                        }

                        if (nextLen == 0U) {
                            // if we have no P25 data, and we're either idle or P25 state, check if we
                            // need to be starting the CC running flag or writing end of voice call data
                            if (host->m_state == STATE_IDLE || host->m_state == STATE_P25) {
                                if (host->m_p25->getCCHalted()) {
                                    host->m_p25->setCCHalted(false);
                                }

                                // write end of voice if necessary
                                bool ret = host->m_p25->writeRF_VoiceEnd();
                                if (ret) {
                                    if (host->m_state == STATE_IDLE) {
                                        host->m_modeTimer.setTimeout(host->m_netModeHang);
                                        host->setState(STATE_P25);
                                    }

                                    if (host->m_state == STATE_P25) {
                                        host->m_modeTimer.start();
                                    }
                                }
                            }
                        }

                        // if the modem is in duplex -- handle P25 CC burst
                        if (host->m_duplex) {
                            if (host->m_p25BcastDurationTimer.isPaused() && !host->m_p25->getCCHalted()) {
                                host->m_p25BcastDurationTimer.resume();
                            }

                            if (host->m_p25->getCCHalted()) {
                                host->m_p25->setCCHalted(false);
                            }

                            if (g_fireP25Control) {
                                host->m_modeTimer.stop();
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

/* Helper to interrupt a running P25 control channel. */

void Host::interruptP25Control()
{
    if (m_p25 != nullptr) {
        LogDebug(LOG_HOST, "interrupt P25 m_p25, m_state = %u", m_state);
        m_p25->setCCHalted(true);

        if (m_p25BcastDurationTimer.isRunning() && !m_p25BcastDurationTimer.isPaused()) {
            m_p25BcastDurationTimer.pause();
        }
    }
}
