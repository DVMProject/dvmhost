// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Modem Host Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2017-2023 Bryan Biedenkapp, N2PLL
*
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
            uint32_t nextLen = control->peekFrameLength(1U);
            if (m_dmrCtrlChannel) {
                if (m_dmrDedicatedTxTestTimer.hasExpired() && !m_dmrDedicatedTxTestTimer.isPaused()) {
                    m_dmrDedicatedTxTestTimer.pause();
                    if (!m_modem->hasTX() && m_modem->gotModemStatus() && m_state == STATE_DMR && (control->getTSCCSlotNo() == 1U) && control->getCCRunning()) {
                        LogError(LOG_HOST, "DMR dedicated control not transmitting, running = %u, halted = %u, frameLength1 = %u", control->getCCRunning(), control->getCCHalted(), nextLen);
                    }
                }
            }

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
            uint32_t nextLen = control->peekFrameLength(1U);
            if (m_dmrCtrlChannel) {
                if (m_dmrDedicatedTxTestTimer.hasExpired() && !m_dmrDedicatedTxTestTimer.isPaused()) {
                    m_dmrDedicatedTxTestTimer.pause();
                    if (!m_modem->hasTX() && m_modem->gotModemStatus() && m_state == STATE_DMR && (control->getTSCCSlotNo() == 2U) && control->getCCRunning()) {
                        LogError(LOG_HOST, "DMR dedicated control not transmitting, running = %u, halted = %u, frameLength2 = %u", control->getCCRunning(), control->getCCHalted(), nextLen);
                    }
                }
            }

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
