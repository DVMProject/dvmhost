/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2017-2023 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include "Defines.h"
#include "host/Host.h"
#include "HostMain.h"

using namespace modem;

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Helper to interrupt a running P25 control channel.
/// </summary>
/// <param name="control"></param>
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

/// <summary>
/// Helper to read P25 frames from modem.
/// </summary>
/// <param name="control"></param>
/// <param name="afterReadCallback"></param>
void Host::readFramesP25(p25::Control* control, std::function<void()>&& afterReadCallback)
{
    uint8_t data[p25::P25_LDU_FRAME_LENGTH_BYTES * 2U];

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

/// <summary>
/// Helper to write P25 frames to modem.
/// </summary>
/// <param name="control"></param>
/// <param name="afterWriteCallback"></param>
void Host::writeFramesP25(p25::Control* control, std::function<void()>&& afterWriteCallback)
{
    uint8_t data[p25::P25_LDU_FRAME_LENGTH_BYTES * 2U];

    // check if there is space on the modem for P25 frames,
    // if there is read frames from the P25 controller and write it
    // to the modem
    if (control != nullptr) {
        uint8_t nextLen = control->peekFrameLength();
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
