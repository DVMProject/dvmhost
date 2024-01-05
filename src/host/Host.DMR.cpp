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
#include "Host.h"

using namespace modem;

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

// Macro to start DMR duplex idle transmission (or beacon)
#define START_DMR_DUPLEX_IDLE(x)                                                                                    \
    if (control != nullptr) {                                                                                       \
        if (m_duplex && !m_dmrTXTimer.isRunning()) {                                                                \
            m_modem->writeDMRStart(x);                                                                              \
            m_dmrTXTimer.start();                                                                                   \
        }                                                                                                           \
    }

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Helper to interrupt a running DMR beacon.
/// </summary>
/// <param name="control"></param>
void Host::interruptDMRBeacon(dmr::Control* control)
{
    if (control != nullptr) {
        if (m_dmrBeaconDurationTimer.isRunning() && !m_dmrBeaconDurationTimer.hasExpired()) {
            if (m_dmrTSCCData && !m_dmrCtrlChannel) {
                LogDebug(LOG_HOST, "interrupt DMR control, m_state = %u", m_state);
                control->setCCHalted(true);
                control->setCCRunning(false);
            }
        }

        m_dmrBeaconDurationTimer.stop();
    }
}

/// <summary>
/// Helper to read DMR slot 1 frames from modem.
/// </summary>
/// <param name="control"></param>
/// <param name="afterReadCallback"></param>
void Host::readFramesDMR1(dmr::Control* control, std::function<void()>&& afterReadCallback)
{
    uint8_t data[dmr::DMR_FRAME_LENGTH_BYTES * 2U];

    if (control != nullptr) {
        // read DMR slot 1 frames from the modem, and if there is any
        // write those frames to the DMR controller
        uint32_t len = m_modem->readDMRFrame1(data);
        if (len > 0U) {
            if (m_state == STATE_IDLE) {
                // if the modem is in duplex -- process wakeup CSBKs
                if (m_duplex) {
                    bool ret = control->processWakeup(data);
                    if (ret) {
                        m_modeTimer.setTimeout(m_rfModeHang);
                        setState(STATE_DMR);

                        START_DMR_DUPLEX_IDLE(true);

                        if (afterReadCallback != nullptr) {
                            afterReadCallback();
                        }
                    }
                }
                else {
                    // in simplex directly process slot 1 frames
                    m_modeTimer.setTimeout(m_rfModeHang);
                    setState(STATE_DMR);
                    START_DMR_DUPLEX_IDLE(true);

                    control->processFrame(1U, data, len);

                    if (afterReadCallback != nullptr) {
                        afterReadCallback();
                    }
                }
            }
            else if (m_state == STATE_DMR) {
                // if the modem is in duplex, and hasn't started transmitting
                // process wakeup CSBKs
                if (m_duplex && !m_modem->hasTX()) {
                    bool ret = control->processWakeup(data);
                    if (ret) {
                        m_modem->writeDMRStart(true);
                        m_dmrTXTimer.start();
                    }
                }
                else {
                    // process slot 1 frames
                    bool ret = control->processFrame(1U, data, len);
                    if (ret) {
                        if (afterReadCallback != nullptr) {
                            afterReadCallback();
                        }

                        m_modeTimer.start();
                        if (m_duplex)
                            m_dmrTXTimer.start();
                    }
                }
            }
            else if (m_state != HOST_STATE_LOCKOUT) {
                LogWarning(LOG_HOST, "DMR modem data received, state = %u", m_state);
            }
        }
    }
}

/// <summary>
/// Helper to write DMR slot 1 frames to modem.
/// </summary>
/// <param name="control"></param>
/// <param name="afterWriteCallback"></param>
void Host::writeFramesDMR1(dmr::Control* control, std::function<void()>&& afterWriteCallback)
{
    uint8_t data[dmr::DMR_FRAME_LENGTH_BYTES * 2U];

    if (control != nullptr) {
        // check if there is space on the modem for DMR slot 1 frames,
        // if there is read frames from the DMR controller and write it
        // to the modem
        bool ret = m_modem->hasDMRSpace1();
        if (ret) {
            uint32_t len = control->getFrame(1U, data);
            if (len > 0U) {
                // if the state is idle; set to DMR, start mode timer and start DMR idle frames
                if (m_state == STATE_IDLE) {
                    m_modeTimer.setTimeout(m_netModeHang);
                    setState(STATE_DMR);
                    START_DMR_DUPLEX_IDLE(true);
                }

                // if the state is DMR; start DMR idle frames and write DMR slot 1 data
                if (m_state == STATE_DMR) {
                    START_DMR_DUPLEX_IDLE(true);

                    m_modem->writeDMRFrame1(data, len);

                    // if there is no DMR CC running; run the interrupt macro to stop
                    // any running DMR beacon
                    if (!control->getCCRunning()) {
                        interruptDMRBeacon(control);
                    }

                    if (afterWriteCallback != nullptr) {
                        afterWriteCallback();
                    }

                    m_modeTimer.start();
                }

                m_lastDstId = control->getLastDstId(1U);
                m_lastSrcId = control->getLastSrcId(1U);
            }
        }
    }
}

/// <summary>
/// Helper to read DMR slot 2 frames from modem.
/// </summary>
/// <param name="control"></param>
/// <param name="afterReadCallback"></param>
void Host::readFramesDMR2(dmr::Control* control, std::function<void()>&& afterReadCallback)
{
     uint8_t data[dmr::DMR_FRAME_LENGTH_BYTES * 2U];

    if (control != nullptr) {
        // read DMR slot 2 frames from the modem, and if there is any
        // write those frames to the DMR controller
        uint32_t len = m_modem->readDMRFrame2(data);
        if (len > 0U) {
            if (m_state == STATE_IDLE) {
                // if the modem is in duplex -- process wakeup CSBKs
                if (m_duplex) {
                    bool ret = control->processWakeup(data);
                    if (ret) {
                        m_modeTimer.setTimeout(m_rfModeHang);
                        setState(STATE_DMR);
                        START_DMR_DUPLEX_IDLE(true);

                        if (afterReadCallback != nullptr) {
                            afterReadCallback();
                        }
                    }
                }
                else {
                    // in simplex -- directly process slot 2 frames
                    m_modeTimer.setTimeout(m_rfModeHang);
                    setState(STATE_DMR);
                    START_DMR_DUPLEX_IDLE(true);

                    control->processFrame(2U, data, len);

                    if (afterReadCallback != nullptr) {
                        afterReadCallback();
                    }
                }
            }
            else if (m_state == STATE_DMR) {
                // if the modem is in duplex, and hasn't started transmitting
                // process wakeup CSBKs
                if (m_duplex && !m_modem->hasTX()) {
                    bool ret = control->processWakeup(data);
                    if (ret) {
                        m_modem->writeDMRStart(true);
                        m_dmrTXTimer.start();
                    }
                }
                else {
                    // process slot 2 frames
                    bool ret = control->processFrame(2U, data, len);
                    if (ret) {
                        if (afterReadCallback != nullptr) {
                            afterReadCallback();
                        }

                        m_modeTimer.start();
                        if (m_duplex)
                            m_dmrTXTimer.start();
                    }
                }
            }
            else if (m_state != HOST_STATE_LOCKOUT) {
                LogWarning(LOG_HOST, "DMR modem data received, state = %u", m_state);
            }
        }
    }
}

/// <summary>
/// Helper to write DMR slot 2 frames to modem.
/// </summary>
/// <param name="control"></param>
/// <param name="afterWriteCallback"></param>
void Host::writeFramesDMR2(dmr::Control* control, std::function<void()>&& afterWriteCallback)
{
    uint8_t data[dmr::DMR_FRAME_LENGTH_BYTES * 2U];

    if (control != nullptr) {
        // check if there is space on the modem for DMR slot 2 frames,
        // if there is read frames from the DMR controller and write it
        // to the modem
        bool ret = m_modem->hasDMRSpace2();
        if (ret) {
            uint32_t len = control->getFrame(2U, data);
            if (len > 0U) {
                // if the state is idle; set to DMR, start mode timer and start DMR idle frames
                if (m_state == STATE_IDLE) {
                    m_modeTimer.setTimeout(m_netModeHang);
                    setState(STATE_DMR);
                    START_DMR_DUPLEX_IDLE(true);
                }

                // if the state is DMR; start DMR idle frames and write DMR slot 2 data
                if (m_state == STATE_DMR) {
                    START_DMR_DUPLEX_IDLE(true);

                    m_modem->writeDMRFrame2(data, len);

                    // if there is no DMR CC running; run the interrupt macro to stop
                    // any running DMR beacon
                    if (!control->getCCRunning()) {
                        interruptDMRBeacon(control);
                    }

                    if (afterWriteCallback != nullptr) {
                        afterWriteCallback();
                    }

                    m_modeTimer.start();
                }

                m_lastDstId = control->getLastDstId(2U);
                m_lastSrcId = control->getLastSrcId(2U);
            }
        }
    }
}
