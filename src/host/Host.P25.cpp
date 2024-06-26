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

/* Helper to interrupt a running P25 control channel. */

void Host::interruptP25Control(p25::Control* control)
{
    if (control != nullptr) {
        LogDebug(LOG_HOST, "interrupt P25 control, m_state = %u", m_state);
        control->setCCHalted(true);

        if (m_p25BcastDurationTimer.isRunning() && !m_p25BcastDurationTimer.isPaused()) {
            m_p25BcastDurationTimer.pause();
        }
    }
}

/* Helper to read P25 frames from modem. */

void Host::readFramesP25(p25::Control* control, std::function<void()>&& afterReadCallback)
{
    uint8_t data[P25DEF::P25_PDU_FRAME_LENGTH_BYTES * 2U];

    // read P25 frames from modem, and if there are frames
    // write those frames to the P25 controller
    if (control != nullptr) {
        uint32_t len = m_modem->readP25Frame(data);
        if (len > 0U) {
            if (m_state == STATE_IDLE) {
                // process P25 frames
                bool ret = control->processFrame(data, len);
                if (ret) {
                    m_modeTimer.setTimeout(m_rfModeHang);
                    setState(STATE_P25);

                    if (afterReadCallback != nullptr) {
                        afterReadCallback();
                    }
                }
                else {
                    ret = control->writeRF_VoiceEnd();
                    if (ret) {
                        if (afterReadCallback != nullptr) {
                            afterReadCallback();
                        }

                        if (m_state == STATE_IDLE) {
                            m_modeTimer.setTimeout(m_rfModeHang);
                            setState(STATE_P25);
                        }

                        if (m_state == STATE_P25) {
                            m_modeTimer.start();
                        }

                        // if the modem is in duplex -- handle P25 CC burst control
                        if (m_duplex) {
                            if (m_p25BcastDurationTimer.isPaused() && !control->getCCHalted()) {
                                m_p25BcastDurationTimer.resume();
                            }

                            if (control->getCCHalted()) {
                                control->setCCHalted(false);
                            }

                            if (g_fireP25Control) {
                                m_modeTimer.stop();
                            }
                        }
                        else {
                            m_p25BcastDurationTimer.stop();
                        }
                    }
                }
            }
            else if (m_state == STATE_P25) {
                // process P25 frames
                bool ret = control->processFrame(data, len);
                if (ret) {
                    m_modeTimer.start();
                }
                else {
                    ret = control->writeRF_VoiceEnd();
                    if (ret) {
                        m_modeTimer.start();
                    }
                }
            }
            else if (m_state != HOST_STATE_LOCKOUT) {
                LogWarning(LOG_HOST, "P25 modem data received, state = %u", m_state);
            }
        }
    }
}

/* Helper to write P25 frames to modem. */

void Host::writeFramesP25(p25::Control* control, std::function<void()>&& afterWriteCallback)
{
    uint8_t data[P25DEF::P25_PDU_FRAME_LENGTH_BYTES * 2U];

    // check if there is space on the modem for P25 frames,
    // if there is read frames from the P25 controller and write it
    // to the modem
    if (control != nullptr) {
        uint8_t nextLen = control->peekFrameLength();
        if (m_p25CtrlChannel) {
            if (m_p25DedicatedTxTestTimer.hasExpired() && !m_p25DedicatedTxTestTimer.isPaused()) {
                m_p25DedicatedTxTestTimer.pause();
                if (!m_modem->hasTX() && m_modem->gotModemStatus() && m_state == STATE_P25 && control->getCCRunning()) {
                    LogError(LOG_HOST, "P25 dedicated control not transmitting, running = %u, halted = %u, frameLength = %u", control->getCCRunning(), control->getCCHalted(), nextLen);
                }
            }
        }

        if (nextLen > 0U) {
            bool ret = m_modem->hasP25Space(nextLen);
            if (ret) {
                uint32_t len = control->getFrame(data);
                if (len > 0U) {
                    // if the state is idle; set to P25 and start mode timer
                    if (m_state == STATE_IDLE) {
                        m_modeTimer.setTimeout(m_netModeHang);
                        setState(STATE_P25);
                    }

                    // if the state is P25; write P25 frame data
                    if (m_state == STATE_P25) {
                        m_modem->writeP25Frame(data, len);

                        if (afterWriteCallback != nullptr) {
                            afterWriteCallback();
                        }

                        m_modeTimer.start();
                    }

                    m_lastDstId = control->getLastDstId();
                    m_lastSrcId = control->getLastSrcId();
                }
                else {
                    nextLen = 0U;
                }
            }
        }

        if (nextLen == 0U) {
            // if we have no P25 data, and we're either idle or P25 state, check if we
            // need to be starting the CC running flag or writing end of voice call data
            if (m_state == STATE_IDLE || m_state == STATE_P25) {
                if (control->getCCHalted()) {
                    control->setCCHalted(false);
                }

                // write end of voice if necessary
                bool ret = control->writeRF_VoiceEnd();
                if (ret) {
                    if (m_state == STATE_IDLE) {
                        m_modeTimer.setTimeout(m_netModeHang);
                        setState(STATE_P25);
                    }

                    if (m_state == STATE_P25) {
                        m_modeTimer.start();
                    }
                }
            }
        }

        // if the modem is in duplex -- handle P25 CC burst control
        if (m_duplex) {
            if (m_p25BcastDurationTimer.isPaused() && !control->getCCHalted()) {
                m_p25BcastDurationTimer.resume();
            }

            if (control->getCCHalted()) {
                control->setCCHalted(false);
            }

            if (g_fireP25Control) {
                m_modeTimer.stop();
            }
        }
    }
}
