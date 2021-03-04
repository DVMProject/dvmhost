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
*   Copyright (C) 2011-2017 by Jonathan Naylor G4KLX
*   Copyright (C) 2017-2020 by Bryan Biedenkapp N2PLL
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
#include "dmr/DMRDefines.h"
#include "p25/P25Defines.h"
#include "modem/Modem.h"
#include "modem/NullModem.h"
#include "Log.h"
#include "Thread.h"
#include "Utils.h"

using namespace modem;

#include <cmath>
#include <cstdio>
#include <cassert>
#include <cstdint>
#include <ctime>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <unistd.h>
#endif

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the Modem class.
/// </summary>
/// <param name="port">Serial port the modem DSP is connected to.</param>
/// <param name="duplex">Flag indicating the modem is operating in duplex mode.</param>
/// <param name="rxInvert">Flag indicating the Rx polarity should be inverted.</param>
/// <param name="txInvert">Flag indicating the Tx polarity should be inverted.</param>
/// <param name="pttInvert">Flag indicating the PTT polarity should be inverted.</param>
/// <param name="dcBlocker">Flag indicating whether the DSP DC-level blocking should be enabled.</param>
/// <param name="cosLockout">Flag indicating whether the COS signal should be used to lockout the modem.</param>
/// <param name="fdmaPreamble">Count of FDMA preambles to transmit before data. (P25/DMR DMO)</param>
/// <param name="dmrRxDelay">Compensate for delay in receiver audio chain in ms. Usually DSP based.</param>
/// <param name="disableOFlowReset">Flag indicating whether the ADC/DAC overflow reset logic is disabled.</param>
/// <param name="packetPlayoutTime">Length of time in MS between packets to send to modem.</param>
/// <param name="trace">Flag indicating whether modem DSP trace is enabled.</param>
/// <param name="debug">Flag indicating whether modem DSP debug is enabled.</param>
Modem::Modem(const std::string& port, bool duplex, bool rxInvert, bool txInvert, bool pttInvert, bool dcBlocker,
    bool cosLockout, uint8_t fdmaPreamble, uint8_t dmrRxDelay, uint8_t packetPlayoutTime, bool disableOFlowReset, bool trace, bool debug) :
    m_port(port),
    m_dmrColorCode(0U),
    m_p25NAC(0x293U),
    m_duplex(duplex),
    m_rxInvert(rxInvert),
    m_txInvert(txInvert),
    m_pttInvert(pttInvert),
    m_dcBlocker(dcBlocker),
    m_cosLockout(cosLockout),
    m_fdmaPreamble(fdmaPreamble),
    m_dmrRxDelay(dmrRxDelay),
    m_rxLevel(0U),
    m_cwIdTXLevel(0U),
    m_dmrTXLevel(0U),
    m_p25TXLevel(0U),
    m_disableOFlowReset(disableOFlowReset),
    m_trace(trace),
    m_debug(debug),
    m_dmrEnabled(false),
    m_p25Enabled(false),
    m_rxDCOffset(0),
    m_txDCOffset(0),
    m_dmrSymLevel3Adj(0),
    m_dmrSymLevel1Adj(0),
    m_p25SymLevel3Adj(0),
    m_p25SymLevel1Adj(0),
    m_adcOverFlowCount(0U),
    m_dacOverFlowCount(0U),
    m_serial(port, SERIAL_115200, true),
    m_buffer(NULL),
    m_length(0U),
    m_offset(0U),
    m_rxDMRData1(1000U, "Modem RX DMR1"),
    m_rxDMRData2(1000U, "Modem RX DMR2"),
    m_txDMRData1(1000U, "Modem TX DMR1"),
    m_txDMRData2(1000U, "Modem TX DMR2"),
    m_rxP25Data(1000U, "Modem RX P25"),
    m_txP25Data(1000U, "Modem TX P25"),
    m_statusTimer(1000U, 0U, 250U),
    m_inactivityTimer(1000U, 4U),
    m_playoutTimer(1000U, 0U, packetPlayoutTime),
    m_dmrSpace1(0U),
    m_dmrSpace2(0U),
    m_p25Space(0U),
    m_tx(false),
    m_cd(false),
    m_lockout(false),
    m_error(false)
{
    assert(!port.empty());

    m_buffer = new uint8_t[BUFFER_LENGTH];
}

/// <summary>
/// Finalizes a instance of the Modem class.
/// </summary>
Modem::~Modem()
{
    delete[] m_buffer;
}

/// <summary>
/// Sets the modem DSP RF DC offset parameters.
/// </summary>
/// <param name="txDCOffset"></param>
/// <param name="rxDCOffset"></param>
void Modem::setDCOffsetParams(int txDCOffset, int rxDCOffset)
{
    m_txDCOffset = txDCOffset;
    m_rxDCOffset = rxDCOffset;
}

/// <summary>
/// Sets the modem DSP enabled modes.
/// </summary>
/// <param name="dmrEnabled"></param>
/// <param name="p25Enabled"></param>
void Modem::setModeParams(bool dmrEnabled, bool p25Enabled)
{
    m_dmrEnabled = dmrEnabled;
    m_p25Enabled = p25Enabled;
}

/// <summary>
/// Sets the modem DSP RF deviation levels.
/// </summary>
/// <param name="rxLevel"></param>
/// <param name="cwIdTXLevel"></param>
/// <param name="dmrTXLevel"></param>
/// <param name="p25TXLevel"></param>
void Modem::setLevels(float rxLevel, float cwIdTXLevel, float dmrTXLevel, float p25TXLevel)
{
    m_rxLevel = rxLevel;
    m_cwIdTXLevel = cwIdTXLevel;
    m_dmrTXLevel = dmrTXLevel;
    m_p25TXLevel = p25TXLevel;
}

/// <summary>
/// Sets the modem DSP Symbol adjustment levels
/// </summary>
/// <param name="dmrSymLevel3Adj"></param>
/// <param name="dmrSymLevel1Adj"></param>
/// <param name="p25SymLevel3Adj"></param>
/// <param name="p25SymLevel1Adj"></param>
void Modem::setSymbolAdjust(int dmrSymLevel3Adj, int dmrSymLevel1Adj, int p25SymLevel3Adj, int p25SymLevel1Adj)
{
    m_dmrSymLevel3Adj = dmrSymLevel3Adj;
    if (dmrSymLevel3Adj > 128)
        m_dmrSymLevel3Adj = 0;
    if (dmrSymLevel3Adj < -128)
        m_dmrSymLevel3Adj = 0;

    m_dmrSymLevel1Adj = dmrSymLevel1Adj;
    if (dmrSymLevel1Adj > 128)
        m_dmrSymLevel1Adj = 0;
    if (dmrSymLevel1Adj < -128)
        m_dmrSymLevel1Adj = 0;

    m_p25SymLevel3Adj = p25SymLevel3Adj;
    if (p25SymLevel3Adj > 128)
        m_p25SymLevel3Adj = 0;
    if (p25SymLevel3Adj < -128)
        m_p25SymLevel3Adj = 0;

    m_p25SymLevel1Adj = p25SymLevel1Adj;
    if (p25SymLevel1Adj > 128)
        m_p25SymLevel1Adj = 0;
    if (p25SymLevel1Adj < -128)
        m_p25SymLevel1Adj = 0;
}

/// <summary>
/// Sets the modem DSP DMR color code.
/// </summary>
/// <param name="colorCode"></param>
void Modem::setDMRColorCode(uint32_t colorCode)
{
    assert(colorCode < 16U);

    m_dmrColorCode = colorCode;
}

/// <summary>
/// Sets the modem DSP P25 NAC.
/// </summary>
/// <param name="nac"></param>
void Modem::setP25NAC(uint32_t nac)
{
    assert(nac < 0xFFFU);

    m_p25NAC = nac;
}

/// <summary>
/// Sets the modem DSP RF receive deviation levels.
/// </summary>
/// <param name="rxLevel"></param>
void Modem::setRXLevel(float rxLevel)
{
    m_rxLevel = rxLevel;

    uint8_t buffer[4U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 16U;
    buffer[2U] = CMD_SET_RXLEVEL;

    buffer[3U] = (uint8_t)(m_rxLevel * 2.55F + 0.5F);

    // Utils::dump(1U, "Written", buffer, 16U);

    int ret = m_serial.write(buffer, 16U);
    if (ret != 16)
        return;

    uint32_t count = 0U;
    RESP_TYPE_DVM resp;
    do {
        Thread::sleep(10U);

        resp = getResponse();
        if (resp == RTM_OK && m_buffer[2U] != CMD_ACK && m_buffer[2U] != CMD_NAK) {
            count++;
            if (count >= MAX_RESPONSES) {
                LogError(LOG_MODEM, "No response, SET_RXLEVEL command");
                return;
            }
        }
    } while (resp == RTM_OK && m_buffer[2U] != CMD_ACK && m_buffer[2U] != CMD_NAK);

    // Utils::dump(1U, "Response", m_buffer, m_length);

    if (resp == RTM_OK && m_buffer[2U] == CMD_NAK) {
        LogError(LOG_MODEM, "NAK to the SET_RXLEVEL command from the modem");
    }
}

/// <summary>
/// Opens connection to the modem DSP.
/// </summary>
/// <returns>True, if connection to modem is established, otherwise false.</returns>
bool Modem::open()
{
    LogMessage(LOG_MODEM, "Initializing HW modem");

    bool ret = m_serial.open();
    if (!ret)
        return false;

    ret = getFirmwareVersion();
    if (!ret) {
        m_serial.close();
        return false;
    }
    else {
        // Stopping the inactivity timer here when a firmware version has been
        // successfuly read prevents the death spiral of "no reply from modem..."
        m_inactivityTimer.stop();
    }

    ret = writeConfig();
    if (!ret) {
        LogError(LOG_MODEM, "Modem is unresponsive");
        m_serial.close();
        return false;
    }

    writeSymbolAdjust();

    m_statusTimer.start();

    m_error = false;
    m_offset = 0U;

    LogMessage(LOG_MODEM, "Modem Ready");
    return true;
}

/// <summary>
/// Updates the timer by the passed number of milliseconds.
/// </summary>
/// <param name="ms"></param>
void Modem::clock(uint32_t ms)
{
    // poll the modem status every 250ms
    m_statusTimer.clock(ms);
    if (m_statusTimer.hasExpired()) {
        getStatus();
        m_statusTimer.start();
    }

    m_inactivityTimer.clock(ms);
    if (m_inactivityTimer.hasExpired()) {
        LogError(LOG_MODEM, "No reply from the modem for some time, resetting it");

        m_error = true;
        m_adcOverFlowCount = 0U;
        m_dacOverFlowCount = 0U;

        close();

        Thread::sleep(2000U);        // 2s
        while (!open())
            Thread::sleep(5000U);    // 5s
    }

    bool forceModemReset = false;
    RESP_TYPE_DVM type = getResponse();

    if (type == RTM_TIMEOUT) {
        // Nothing to do
    }
    else if (type == RTM_ERROR) {
        // Nothing to do
    }
    else {
        // type == RTM_OK
        switch (m_buffer[2U]) {
            /** Digital Mobile Radio */
            case CMD_DMR_DATA1:
                {
                    if (m_trace)
                        Utils::dump(1U, "RX DMR Data 1", m_buffer, m_length);

                    uint8_t data = m_length - 2U;
                    m_rxDMRData1.addData(&data, 1U);

                    if (m_buffer[3U] == (dmr::DMR_SYNC_DATA | dmr::DT_TERMINATOR_WITH_LC))
                        data = TAG_EOT;
                    else
                        data = TAG_DATA;
                    m_rxDMRData1.addData(&data, 1U);

                    m_rxDMRData1.addData(m_buffer + 3U, m_length - 3U);
                }
                break;

            case CMD_DMR_DATA2:
                {
                    if (m_trace)
                        Utils::dump(1U, "RX DMR Data 2", m_buffer, m_length);

                    uint8_t data = m_length - 2U;
                    m_rxDMRData2.addData(&data, 1U);

                    if (m_buffer[3U] == (dmr::DMR_SYNC_DATA | dmr::DT_TERMINATOR_WITH_LC))
                        data = TAG_EOT;
                    else
                        data = TAG_DATA;
                    m_rxDMRData2.addData(&data, 1U);

                    m_rxDMRData2.addData(m_buffer + 3U, m_length - 3U);
                }
                break;

            case CMD_DMR_LOST1:
                {
                    if (m_trace)
                        Utils::dump(1U, "RX DMR Lost 1", m_buffer, m_length);

                    uint8_t data = 1U;
                    m_rxDMRData1.addData(&data, 1U);

                    data = TAG_LOST;
                    m_rxDMRData1.addData(&data, 1U);
                }
                break;

            case CMD_DMR_LOST2:
                {
                    if (m_trace)
                        Utils::dump(1U, "RX DMR Lost 2", m_buffer, m_length);

                    uint8_t data = 1U;
                    m_rxDMRData2.addData(&data, 1U);

                    data = TAG_LOST;
                    m_rxDMRData2.addData(&data, 1U);
                }
                break;

            /** Project 25 */
            case CMD_P25_DATA:
                {
                    if (m_trace)
                        Utils::dump(1U, "RX P25 Data", m_buffer, m_length);

                    uint8_t data = m_length - 2U;
                    m_rxP25Data.addData(&data, 1U);

                    data = TAG_DATA;
                    m_rxP25Data.addData(&data, 1U);

                    m_rxP25Data.addData(m_buffer + 3U, m_length - 3U);
                }
                break;

            case CMD_P25_LOST:
                {
                    if (m_trace)
                        Utils::dump(1U, "RX P25 Lost", m_buffer, m_length);

                    uint8_t data = 1U;
                    m_rxP25Data.addData(&data, 1U);

                    data = TAG_LOST;
                    m_rxP25Data.addData(&data, 1U);
                }
                break;

            /** General */
            case CMD_GET_STATUS:
                {
                    // if (m_trace)
                    //    Utils::dump(1U, "Get Status", m_buffer, m_length);

                    m_tx = (m_buffer[5U] & 0x01U) == 0x01U;

                    bool adcOverflow = (m_buffer[5U] & 0x02U) == 0x02U;
                    if (adcOverflow) {
                        //LogError(LOG_MODEM, "ADC levels have overflowed");
                        m_adcOverFlowCount++;

                        if (m_adcOverFlowCount >= MAX_ADC_OVERFLOW / 2U) {
                            LogWarning(LOG_MODEM, "ADC overflow count > %u!", MAX_ADC_OVERFLOW / 2U);
                        }

                        if (!m_disableOFlowReset) {
                            if (m_adcOverFlowCount > MAX_ADC_OVERFLOW) {
                                LogError(LOG_MODEM, "ADC overflow count > %u, resetting modem", MAX_ADC_OVERFLOW);
                                forceModemReset = true;
                            }
                        } else {
                            m_adcOverFlowCount = 0U;
                        }
                    }
                    else {
                        if (m_adcOverFlowCount != 0U) {
                            m_adcOverFlowCount--;
                        }
                    }

                    bool rxOverflow = (m_buffer[5U] & 0x04U) == 0x04U;
                    if (rxOverflow)
                        LogError(LOG_MODEM, "RX buffer has overflowed");

                    bool txOverflow = (m_buffer[5U] & 0x08U) == 0x08U;
                    if (txOverflow)
                        LogError(LOG_MODEM, "TX buffer has overflowed");

                    m_lockout = (m_buffer[5U] & 0x10U) == 0x10U;

                    bool dacOverflow = (m_buffer[5U] & 0x20U) == 0x20U;
                    if (dacOverflow) {
                        //LogError(LOG_MODEM, "DAC levels have overflowed");
                        m_dacOverFlowCount++;

                        if (m_dacOverFlowCount > MAX_DAC_OVERFLOW / 2U) {
                            LogWarning(LOG_MODEM, "DAC overflow count > %u!", MAX_DAC_OVERFLOW / 2U);
                        }

                        if (!m_disableOFlowReset) {
                            if (m_dacOverFlowCount > MAX_DAC_OVERFLOW) {
                                LogError(LOG_MODEM, "DAC overflow count > %u, resetting modem", MAX_DAC_OVERFLOW);
                                forceModemReset = true;
                            }
                        } else {
                            m_dacOverFlowCount = 0U;
                        }
                    }
                    else {
                        if (m_dacOverFlowCount != 0U) {
                            m_dacOverFlowCount--;
                        }
                    }

                    m_cd = (m_buffer[5U] & 0x40U) == 0x40U;

                    m_dmrSpace1 = m_buffer[7U];
                    m_dmrSpace2 = m_buffer[8U];
                    m_p25Space = m_buffer[10U];

                    m_inactivityTimer.start();
                }
                break;

            case CMD_GET_VERSION:
            case CMD_ACK:
                break;

            case CMD_NAK:
                LogWarning(LOG_MODEM, "NAK, command = 0x%02X, reason = %u", m_buffer[3U], m_buffer[4U]);
                break;

            case CMD_DEBUG1:
            case CMD_DEBUG2:
            case CMD_DEBUG3:
            case CMD_DEBUG4:
            case CMD_DEBUG5:
                printDebug();
                break;

            default:
                LogWarning(LOG_MODEM, "Unknown message, type = %02X", m_buffer[2U]);
                Utils::dump("Buffer dump", m_buffer, m_length);
                break;
        }
    }

    // force a modem reset because of a error condition
    if (forceModemReset) {
        m_error = true;
        forceModemReset = false;
        m_adcOverFlowCount = 0U;
        m_dacOverFlowCount = 0U;

        close();

        Thread::sleep(2000U);        // 2s
        while (!open())
            Thread::sleep(5000U);    // 5s
    }

    // Only feed data to the modem if the playout timer has expired
    m_playoutTimer.clock(ms);
    if (!m_playoutTimer.hasExpired())
        return;

    if (m_dmrSpace1 > 1U && !m_txDMRData1.isEmpty()) {
        uint8_t len = 0U;
        m_txDMRData1.getData(&len, 1U);
        m_txDMRData1.getData(m_buffer, len);

        if (m_trace)
            Utils::dump(1U, "TX DMR Data 1", m_buffer, len);

        int ret = m_serial.write(m_buffer, len);
        if (ret != int(len))
            LogError(LOG_MODEM, "Error writing DMR slot 1 data");

        m_playoutTimer.start();

        m_dmrSpace1--;
    }

    if (m_dmrSpace2 > 1U && !m_txDMRData2.isEmpty()) {
        uint8_t len = 0U;
        m_txDMRData2.getData(&len, 1U);
        m_txDMRData2.getData(m_buffer, len);

        if (m_trace)
            Utils::dump(1U, "TX DMR Data 2", m_buffer, len);

        int ret = m_serial.write(m_buffer, len);
        if (ret != int(len))
            LogError(LOG_MODEM, "Error writing DMR slot 2 data");

        m_playoutTimer.start();

        m_dmrSpace2--;
    }

    if (m_p25Space > 1U && !m_txP25Data.isEmpty()) {
        uint8_t len = 0U;
        m_txP25Data.getData(&len, 1U);
        m_txP25Data.getData(m_buffer, len);

        if (m_trace) {
            Utils::dump(1U, "TX P25 Data", m_buffer, len);
        }

        int ret = m_serial.write(m_buffer, len);
        if (ret != int(len))
            LogError(LOG_MODEM, "Error writing P25 data");

        m_playoutTimer.start();

        m_p25Space--;
    }
}

/// <summary>
/// Closes connection to the modem DSP.
/// </summary>
void Modem::close()
{
    LogDebug(LOG_MODEM, "Closing the modem");
    m_serial.close();
}

/// <summary>
/// Reads DMR Slot 1 frame data from the DMR Slot 1 ring buffer.
/// </summary>
/// <param name="data">Buffer to write frame data to.</param>
/// <returns>Length of data read from ring buffer.</returns>
uint32_t Modem::readDMRData1(uint8_t* data)
{
    assert(data != NULL);

    if (m_rxDMRData1.isEmpty())
        return 0U;

    uint8_t len = 0U;
    m_rxDMRData1.getData(&len, 1U);
    m_rxDMRData1.getData(data, len);

    return len;
}

/// <summary>
/// Reads DMR Slot 2 frame data from the DMR Slot 2 ring buffer.
/// </summary>
/// <param name="data">Buffer to write frame data to.</param>
/// <returns>Length of data read from ring buffer.</returns>
uint32_t Modem::readDMRData2(uint8_t* data)
{
    assert(data != NULL);

    if (m_rxDMRData2.isEmpty())
        return 0U;

    uint8_t len = 0U;
    m_rxDMRData2.getData(&len, 1U);
    m_rxDMRData2.getData(data, len);

    return len;
}

/// <summary>
/// Reads P25 frame data from the P25 ring buffer.
/// </summary>
/// <param name="data">Buffer to write frame data to.</param>
/// <returns>Length of data read from ring buffer.</returns>
uint32_t Modem::readP25Data(uint8_t* data)
{
    assert(data != NULL);

    if (m_rxP25Data.isEmpty())
        return 0U;

    uint8_t len = 0U;
    m_rxP25Data.getData(&len, 1U);
    m_rxP25Data.getData(data, len);

    return len;
}

/// <summary>
/// Helper to test if the DMR Slot 1 ring buffer has free space.
/// </summary>
/// <returns>True, if the DMR Slot 1 ring buffer has free space, otherwise false.</returns>
bool Modem::hasDMRSpace1() const
{
    uint32_t space = m_txDMRData1.freeSpace() / (dmr::DMR_FRAME_LENGTH_BYTES + 4U);
    return space > 1U;
}

/// <summary>
/// Helper to test if the DMR Slot 2 ring buffer has free space.
/// </summary>
/// <returns>True, if the DMR Slot 2 ring buffer has free space, otherwise false.</returns>
bool Modem::hasDMRSpace2() const
{
    uint32_t space = m_txDMRData2.freeSpace() / (dmr::DMR_FRAME_LENGTH_BYTES + 4U);
    return space > 1U;
}

/// <summary>
/// Helper to test if the P25 ring buffer has free space.
/// </summary>
/// <returns>True, if the P25 ring buffer has free space, otherwise false.</returns>
bool Modem::hasP25Space() const
{
    uint32_t space = m_txP25Data.freeSpace() / (p25::P25_LDU_FRAME_LENGTH_BYTES + 4U);
    return space > 1U;
}

/// <summary>
/// Flag indicating whether or not the modem DSP is transmitting.
/// </summary>
/// <returns>True, if modem DSP is transmitting, otherwise false.</returns>
bool Modem::hasTX() const
{
    return m_tx;
}

/// <summary>
/// Flag indicating whether or not the modem DSP has carrier detect.
/// </summary>
/// <returns>True, if modem DSP has carrier detect, otherwise false.</returns>
bool Modem::hasCD() const
{
    return m_cd;
}

/// <summary>
/// Flag indicating whether or not the modem DSP is currently locked out.
/// </summary>
/// <returns>True, if modem DSP is currently locked out, otherwise false.</returns>
bool Modem::hasLockout() const
{
    return m_lockout;
}

/// <summary>
/// Flag indicating whether or not the modem DSP is currently in an error condition.
/// </summary>
/// <returns>True, if the modem DSP is current in an error condition, otherwise false.</returns>
bool Modem::hasError() const
{
    return m_error;
}

/// <summary>
/// Clears any buffered DMR Slot 1 frame data to be sent to the modem DSP.
/// </summary>
void Modem::clearDMRData1()
{
    if (!m_txDMRData1.isEmpty()) {
        m_txDMRData1.clear();
    }
}

/// <summary>
/// Clears any buffered DMR Slot 2 frame data to be sent to the modem DSP.
/// </summary>
void Modem::clearDMRData2()
{
    if (!m_txDMRData2.isEmpty()) {
        m_txDMRData2.clear();
    }
}

/// <summary>
/// Clears any buffered P25 frame data to be sent to the modem DSP.
/// </summary>
void Modem::clearP25Data()
{
    if (!m_txP25Data.isEmpty()) {
        m_txP25Data.clear();

        uint8_t buffer[3U];

        buffer[0U] = DVM_FRAME_START;
        buffer[1U] = 3U;
        buffer[2U] = CMD_P25_CLEAR;

        // Utils::dump(1U, "Written", buffer, 3U);

        m_serial.write(buffer, 3U);
    }
}

/// <summary>
/// Internal helper to inject DMR Slot 1 frame data as if it came from the modem DSP.
/// </summary>
/// <param name="data">Data to write to ring buffer.</param>
/// <param name="length">Length of data to write.</param>
void Modem::injectDMRData1(const uint8_t* data, uint32_t length)
{
    assert(data != NULL);
    assert(length > 0U);

    if (m_trace)
        Utils::dump(1U, "Injected DMR Slot 1 Data", data, length);

    uint8_t val = length;
    m_rxDMRData1.addData(&val, 1U);

    val = TAG_DATA;
    m_rxDMRData1.addData(&val, 1U);
    val = dmr::DMR_SYNC_VOICE & dmr::DMR_SYNC_DATA;    // valid sync
    m_rxDMRData1.addData(&val, 1U);

    m_rxDMRData1.addData(data, length);
}

/// <summary>
/// Internal helper to inject DMR Slot 2 frame data as if it came from the modem DSP.
/// </summary>
/// <param name="data">Data to write to ring buffer.</param>
/// <param name="length">Length of data to write.</param>
void Modem::injectDMRData2(const uint8_t* data, uint32_t length)
{
    assert(data != NULL);
    assert(length > 0U);

    if (m_trace)
        Utils::dump(1U, "Injected DMR Slot 2 Data", data, length);

    uint8_t val = length;
    m_rxDMRData2.addData(&val, 1U);

    val = TAG_DATA;
    m_rxDMRData2.addData(&val, 1U);
    val = dmr::DMR_SYNC_VOICE & dmr::DMR_SYNC_DATA;    // valid sync
    m_rxDMRData2.addData(&val, 1U);

    m_rxDMRData2.addData(data, length);
}

/// <summary>
/// Internal helper to inject P25 frame data as if it came from the modem DSP.
/// </summary>
/// <param name="data">Data to write to ring buffer.</param>
/// <param name="length">Length of data to write.</param>
void Modem::injectP25Data(const uint8_t* data, uint32_t length)
{
    assert(data != NULL);
    assert(length > 0U);

    if (m_trace)
        Utils::dump(1U, "Injected P25 Data", data, length);

    uint8_t val = length;
    m_rxP25Data.addData(&val, 1U);

    val = TAG_DATA;
    m_rxP25Data.addData(&val, 1U);
    val = 0x01U;    // valid sync
    m_rxP25Data.addData(&val, 1U);

    m_rxP25Data.addData(data, length);
}

/// <summary>
/// Writes DMR Slot 1 frame data to the DMR Slot 1 ring buffer.
/// </summary>
/// <param name="data">Data to write to ring buffer.</param>
/// <param name="length">Length of data to write.</param>
/// <returns>True, if data is written, otherwise false.</returns>
bool Modem::writeDMRData1(const uint8_t* data, uint32_t length)
{
    assert(data != NULL);
    assert(length > 0U);

    if (data[0U] != TAG_DATA && data[0U] != TAG_EOT)
        return false;

    uint8_t buffer[40U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = length + 2U;
    buffer[2U] = CMD_DMR_DATA1;

    ::memcpy(buffer + 3U, data + 1U, length - 1U);

    uint8_t len = length + 2U;
    m_txDMRData1.addData(&len, 1U);
    m_txDMRData1.addData(buffer, len);

    return true;
}

/// <summary>
/// Writes DMR Slot 2 frame data to the DMR Slot 2 ring buffer.
/// </summary>
/// <param name="data">Data to write to ring buffer.</param>
/// <param name="length">Length of data to write.</param>
/// <returns>True, if data is written, otherwise false.</returns>
bool Modem::writeDMRData2(const uint8_t* data, uint32_t length)
{
    assert(data != NULL);
    assert(length > 0U);

    if (data[0U] != TAG_DATA && data[0U] != TAG_EOT)
        return false;

    uint8_t buffer[40U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = length + 2U;
    buffer[2U] = CMD_DMR_DATA2;

    ::memcpy(buffer + 3U, data + 1U, length - 1U);

    uint8_t len = length + 2U;
    m_txDMRData2.addData(&len, 1U);
    m_txDMRData2.addData(buffer, len);

    return true;
}

/// <summary>
/// Writes P25 frame data to the P25 ring buffer.
/// </summary>
/// <param name="data">Data to write to ring buffer.</param>
/// <param name="length">Length of data to write.</param>
/// <returns>True, if data is written, otherwise false.</returns>
bool Modem::writeP25Data(const uint8_t* data, uint32_t length)
{
    assert(data != NULL);
    assert(length > 0U);

    if (data[0U] != TAG_DATA && data[0U] != TAG_EOT)
        return false;

    uint8_t buffer[250U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = length + 2U;
    buffer[2U] = CMD_P25_DATA;

    ::memcpy(buffer + 3U, data + 1U, length - 1U);

    uint8_t len = length + 2U;
    m_txP25Data.addData(&len, 1U);
    m_txP25Data.addData(buffer, len);

    return true;
}

/// <summary>
/// Triggers the start of DMR transmit.
/// </summary>
/// <param name="tx"></param>
/// <returns>True, if DMR transmit started, otherwise false.</returns>
bool Modem::writeDMRStart(bool tx)
{
    if (tx && m_tx)
        return true;
    if (!tx && !m_tx)
        return true;

    uint8_t buffer[4U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 4U;
    buffer[2U] = CMD_DMR_START;
    buffer[3U] = tx ? 0x01U : 0x00U;

    // Utils::dump(1U, "Written", buffer, 4U);

    return m_serial.write(buffer, 4U) == 4;
}

/// <summary>
/// Writes a DMR short LC to the modem DSP.
/// </summary>
/// <param name="lc"></param>
/// <returns>True, if DMR LC is written, otherwise false.</returns>
bool Modem::writeDMRShortLC(const uint8_t* lc)
{
    assert(lc != NULL);

    uint8_t buffer[12U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 12U;
    buffer[2U] = CMD_DMR_SHORTLC;
    buffer[3U] = lc[0U];
    buffer[4U] = lc[1U];
    buffer[5U] = lc[2U];
    buffer[6U] = lc[3U];
    buffer[7U] = lc[4U];
    buffer[8U] = lc[5U];
    buffer[9U] = lc[6U];
    buffer[10U] = lc[7U];
    buffer[11U] = lc[8U];

    // Utils::dump(1U, "Written", buffer, 12U);

    return m_serial.write(buffer, 12U) == 12;
}

/// <summary>
/// Writes a DMR abort message for the given slot to the modem DSP.
/// </summary>
/// <param name="slotNo">DMR slot to write abort for.</param>
/// <returns>True, if DMR abort is written, otherwise false.</returns>
bool Modem::writeDMRAbort(uint32_t slotNo)
{
    if (slotNo == 1U)
        m_txDMRData1.clear();
    else
        m_txDMRData2.clear();

    uint8_t buffer[4U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 4U;
    buffer[2U] = CMD_DMR_ABORT;
    buffer[3U] = slotNo;

    // Utils::dump(1U, "Written", buffer, 4U);

    return m_serial.write(buffer, 4U) == 4;
}

/// <summary>
/// Sets the current operating mode for the modem DSP.
/// </summary>
/// <param name="state"></param>
/// <returns></returns>
bool Modem::setMode(DVM_STATE state)
{
    uint8_t buffer[4U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 4U;
    buffer[2U] = CMD_SET_MODE;
    buffer[3U] = state;

    // Utils::dump(1U, "Written", buffer, 4U);

    return m_serial.write(buffer, 4U) == 4;
}

/// <summary>
/// Transmits the given string as CW morse.
/// </summary>
/// <param name="callsign"></param>
/// <returns></returns>
bool Modem::sendCWId(const std::string& callsign)
{
    LogDebug(LOG_MODEM, "sending CW ID");

    uint32_t length = (uint32_t)callsign.length();
    if (length > 200U)
        length = 200U;

    uint8_t buffer[205U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = length + 3U;
    buffer[2U] = CMD_SEND_CWID;

    for (uint32_t i = 0U; i < length; i++)
        buffer[i + 3U] = callsign.at(i);

    if (m_trace) {
        Utils::dump(1U, "CW ID Data", buffer, length + 3U);
    }

    return m_serial.write(buffer, length + 3U) == int(length + 3U);
}

/// <summary>
/// Helper to create an instance of the Modem class.
/// </summary>
/// <param name="port">Serial port the modem DSP is connected to.</param>
/// <param name="duplex">Flag indicating the modem is operating in duplex mode.</param>
/// <param name="rxInvert">Flag indicating the Rx polarity should be inverted.</param>
/// <param name="txInvert">Flag indicating the Tx polarity should be inverted.</param>
/// <param name="pttInvert">Flag indicating the PTT polarity should be inverted.</param>
/// <param name="dcBlocker">Flag indicating whether the DSP DC-level blocking should be enabled.</param>
/// <param name="cosLockout">Flag indicating whether the COS signal should be used to lockout the modem.</param>
/// <param name="fdmaPreamble">Count of FDMA preambles to transmit before data. (P25/DMR DMO)</param>
/// <param name="dmrRxDelay">Compensate for delay in receiver audio chain in ms. Usually DSP based.</param>
/// <param name="packetPlayoutTime">Length of time in MS between packets to send to modem.</param>
/// <param name="disableOFlowReset">Flag indicating whether the ADC/DAC overflow reset logic is disabled.</param>
/// <param name="trace">Flag indicating whether modem DSP trace is enabled.</param>
/// <param name="debug">Flag indicating whether modem DSP debug is enabled.</param>
Modem* Modem::createModem(const std::string& port, bool duplex, bool rxInvert, bool txInvert, bool pttInvert, bool dcBlocker,
    bool cosLockout, uint8_t fdmaPreamble, uint8_t dmrRxDelay, uint8_t packetPlayoutTime, bool disableOFlowReset, bool trace, bool debug)
{
    if (port == NULL_MODEM) {
        return new NullModem(port, duplex, rxInvert, txInvert, pttInvert, dcBlocker, cosLockout, fdmaPreamble, dmrRxDelay, packetPlayoutTime, disableOFlowReset, trace, debug);
    }
    else {
        return new Modem(port, duplex, rxInvert, txInvert, pttInvert, dcBlocker, cosLockout, fdmaPreamble, dmrRxDelay, packetPlayoutTime, disableOFlowReset, trace, debug);
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Retrieve the modem DSP version.
/// </summary>
/// <returns></returns>
bool Modem::getFirmwareVersion()
{
    Thread::sleep(2000U);    // 2s

    for (uint32_t i = 0U; i < 6U; i++) {
        uint8_t buffer[3U];

        buffer[0U] = DVM_FRAME_START;
        buffer[1U] = 3U;
        buffer[2U] = CMD_GET_VERSION;

        // Utils::dump(1U, "Written", buffer, 3U);

        int ret = m_serial.write(buffer, 3U);
        if (ret != 3)
            return false;

        for (uint32_t count = 0U; count < MAX_RESPONSES; count++) {
            Thread::sleep(10U);
            RESP_TYPE_DVM resp = getResponse();
            if (resp == RTM_OK && m_buffer[2U] == CMD_GET_VERSION) {
                uint8_t protoVer = m_buffer[3U];

                switch (protoVer) {
                case PROTOCOL_VERSION:
                    LogInfoEx(LOG_MODEM, MODEM_VERSION_STR, m_length - 21U, m_buffer + 21U, protoVer);
                    switch (m_buffer[4U]) {
                    case 0U:
                        LogMessage(LOG_MODEM, "Atmel ARM, UDID: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X", m_buffer[5U], m_buffer[6U], m_buffer[7U], m_buffer[8U], m_buffer[9U], m_buffer[10U], m_buffer[11U], m_buffer[12U], m_buffer[13U], m_buffer[14U], m_buffer[15U], m_buffer[16U], m_buffer[17U], m_buffer[18U], m_buffer[19U], m_buffer[20U]);
                        break;
                    case 1U:
                        LogMessage(LOG_MODEM, "NXP ARM, UDID: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X", m_buffer[5U], m_buffer[6U], m_buffer[7U], m_buffer[8U], m_buffer[9U], m_buffer[10U], m_buffer[11U], m_buffer[12U], m_buffer[13U], m_buffer[14U], m_buffer[15U], m_buffer[16U], m_buffer[17U], m_buffer[18U], m_buffer[19U], m_buffer[20U]);
                        break;
                    case 2U:
                        LogMessage(LOG_MODEM, "ST-Micro ARM, UDID: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X", m_buffer[5U], m_buffer[6U], m_buffer[7U], m_buffer[8U], m_buffer[9U], m_buffer[10U], m_buffer[11U], m_buffer[12U], m_buffer[13U], m_buffer[14U], m_buffer[15U], m_buffer[16U]);
                        break;
                    default:
                        LogMessage(LOG_MODEM, "Unknown CPU type: %u", m_buffer[4U]);
                        break;
                    }
                    return true;

                default:
                    LogError(LOG_MODEM, MODEM_UNSUPPORTED_STR, protoVer);
                    return false;
                }
            }
        }

        Thread::sleep(1500U);
    }

    LogError(LOG_MODEM, "Unable to read the firmware version after 6 attempts");

    return false;
}

/// <summary>
/// Retrieve the current status from the modem DSP.
/// </summary>
/// <returns></returns>
bool Modem::getStatus()
{
    uint8_t buffer[3U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 3U;
    buffer[2U] = CMD_GET_STATUS;

    // Utils::dump(1U, "Written", buffer, 3U);

    return m_serial.write(buffer, 3U) == 3;
}

/// <summary>
/// Write configuration to the modem DSP.
/// </summary>
/// <returns></returns>
bool Modem::writeConfig()
{
    uint8_t buffer[20U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 17U;
    buffer[2U] = CMD_SET_CONFIG;

    buffer[3U] = 0x00U;
    if (m_rxInvert)
        buffer[3U] |= 0x01U;
    if (m_txInvert)
        buffer[3U] |= 0x02U;
    if (m_pttInvert)
        buffer[3U] |= 0x04U;
    if (m_debug)
        buffer[3U] |= 0x10U;
    if (!m_duplex)
        buffer[3U] |= 0x80U;

    buffer[4U] = 0x00U;
    if (m_dcBlocker)
        buffer[4U] |= 0x01U;
    if (m_cosLockout)
        buffer[4U] |= 0x04U;

    if (m_dmrEnabled)
        buffer[4U] |= 0x02U;
    if (m_p25Enabled)
        buffer[4U] |= 0x08U;

    if (m_fdmaPreamble > MAX_FDMA_PREAMBLE) {
        LogWarning(LOG_P25, "oversized FDMA preamble count, reducing to maximum %u", MAX_FDMA_PREAMBLE);
        m_fdmaPreamble = MAX_FDMA_PREAMBLE;
    }

    buffer[5U] = m_fdmaPreamble;

    buffer[6U] = STATE_IDLE;

    buffer[7U] = (uint8_t)(m_rxLevel * 2.55F + 0.5F);

    buffer[8U] = (uint8_t)(m_cwIdTXLevel * 2.55F + 0.5F);

    buffer[9U] = m_dmrColorCode;

    buffer[10U] = m_dmrRxDelay;

    buffer[11U] = (m_p25NAC >> 4) & 0xFFU;
    buffer[12U] = (m_p25NAC << 4) & 0xF0U;

    buffer[13U] = (uint8_t)(m_dmrTXLevel * 2.55F + 0.5F);
    buffer[15U] = (uint8_t)(m_p25TXLevel * 2.55F + 0.5F);

    buffer[16U] = (uint8_t)(m_txDCOffset + 128);
    buffer[17U] = (uint8_t)(m_rxDCOffset + 128);

    // Utils::dump(1U, "Written", buffer, 17U);

    int ret = m_serial.write(buffer, 17U);
    if (ret != 17)
        return false;

    uint32_t count = 0U;
    RESP_TYPE_DVM resp;
    do {
        Thread::sleep(10U);

        resp = getResponse();
        if (resp == RTM_OK && m_buffer[2U] != CMD_ACK && m_buffer[2U] != CMD_NAK) {
            count++;
            if (count >= MAX_RESPONSES) {
                LogError(LOG_MODEM, "No response, SET_CONFIG command");
                return false;
            }
        }
    } while (resp == RTM_OK && m_buffer[2U] != CMD_ACK && m_buffer[2U] != CMD_NAK);

    // Utils::dump(1U, "Response", m_buffer, m_length);

    if (resp == RTM_OK && m_buffer[2U] == CMD_NAK) {
        LogError(LOG_MODEM, "NAK to the SET_CONFIG command from the modem");
        return false;
    }

    m_playoutTimer.start();

    return true;
}

/// <summary>
/// Write symbol level adjustments to the modem DSP.
/// </summary>
/// <returns>True, if level adjustments are written, otherwise false.</returns>
bool Modem::writeSymbolAdjust()
{
    uint8_t buffer[10U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 7U;
    buffer[2U] = CMD_SET_SYMLVLADJ;

    buffer[3U] = (uint8_t)(m_dmrSymLevel3Adj + 128);
    buffer[4U] = (uint8_t)(m_dmrSymLevel1Adj + 128);

    buffer[5U] = (uint8_t)(m_p25SymLevel3Adj + 128);
    buffer[6U] = (uint8_t)(m_p25SymLevel1Adj + 128);

    int ret = m_serial.write(buffer, 7U);
    if (ret <= 0)
        return false;

    uint32_t count = 0U;
    RESP_TYPE_DVM resp;
    do {
        Thread::sleep(10U);

        resp = getResponse();
        if (resp == RTM_OK && m_buffer[2U] != CMD_ACK && m_buffer[2U] != CMD_NAK) {
            count++;
            if (count >= MAX_RESPONSES) {
                LogError(LOG_MODEM, "No response, SET_SYMLVLADJ command");
                return false;
            }
        }
    } while (resp == RTM_OK && m_buffer[2U] != CMD_ACK && m_buffer[2U] != CMD_NAK);

    // Utils::dump(1U, "Response", m_buffer, m_length);

    if (resp == RTM_OK && m_buffer[2U] == CMD_NAK) {
        LogError(LOG_MODEM, "NAK to the SET_SYMLVLADJ command from the modem");
        return false;
    }

    m_playoutTimer.start();

    return true;
}

/// <summary>
/// 
/// </summary>
void Modem::printDebug()
{
    if (m_buffer[2U] == CMD_DEBUG1) {
        LogDebug(LOG_MODEM, "M: %.*s", m_length - 3U, m_buffer + 3U);
    }
    else if (m_buffer[2U] == CMD_DEBUG2) {
        short val1 = (m_buffer[m_length - 2U] << 8) | m_buffer[m_length - 1U];
        LogDebug(LOG_MODEM, "M: %.*s %X", m_length - 5U, m_buffer + 3U, val1);
    }
    else if (m_buffer[2U] == CMD_DEBUG3) {
        short val1 = (m_buffer[m_length - 4U] << 8) | m_buffer[m_length - 3U];
        short val2 = (m_buffer[m_length - 2U] << 8) | m_buffer[m_length - 1U];
        LogDebug(LOG_MODEM, "M: %.*s %X %X", m_length - 7U, m_buffer + 3U, val1, val2);
    }
    else if (m_buffer[2U] == CMD_DEBUG4) {
        short val1 = (m_buffer[m_length - 6U] << 8) | m_buffer[m_length - 5U];
        short val2 = (m_buffer[m_length - 4U] << 8) | m_buffer[m_length - 3U];
        short val3 = (m_buffer[m_length - 2U] << 8) | m_buffer[m_length - 1U];
        LogDebug(LOG_MODEM, "M: %.*s %X %X %X", m_length - 9U, m_buffer + 3U, val1, val2, val3);
    }
    else if (m_buffer[2U] == CMD_DEBUG5) {
        short val1 = (m_buffer[m_length - 8U] << 8) | m_buffer[m_length - 7U];
        short val2 = (m_buffer[m_length - 6U] << 8) | m_buffer[m_length - 5U];
        short val3 = (m_buffer[m_length - 4U] << 8) | m_buffer[m_length - 3U];
        short val4 = (m_buffer[m_length - 2U] << 8) | m_buffer[m_length - 1U];
        LogDebug(LOG_MODEM, "M: %.*s %X %X %X %X", m_length - 11U, m_buffer + 3U, val1, val2, val3, val4);
    }
}

/// <summary>
/// 
/// </summary>
/// <returns></returns>
RESP_TYPE_DVM Modem::getResponse()
{
    if (m_offset == 0U) {
        // Get the start of the frame or nothing at all
        int ret = m_serial.read(m_buffer + 0U, 1U);
        if (ret < 0) {
            LogError(LOG_MODEM, "Error reading from the modem, ret = %d", ret);
            return RTM_ERROR;
        }

        if (ret == 0)
            return RTM_TIMEOUT;

        if (m_buffer[0U] != DVM_FRAME_START)
            return RTM_TIMEOUT;

        m_offset = 1U;
    }

    if (m_offset == 1U) {
        // Get the length of the frame
        int ret = m_serial.read(m_buffer + 1U, 1U);
        if (ret < 0) {
            LogError(LOG_MODEM, "Error reading from the modem, ret = %d", ret);
            m_offset = 0U;
            return RTM_ERROR;
        }

        if (ret == 0)
            return RTM_TIMEOUT;

        if (m_buffer[1U] >= 250U) {
            LogError(LOG_MODEM, "Invalid length received from the modem, len = %u", m_buffer[1U]);
            m_offset = 0U;
            return RTM_ERROR;
        }

        m_length = m_buffer[1U];
        m_offset = 2U;
    }

    if (m_offset == 2U) {
        // Get the frame type
        int ret = m_serial.read(m_buffer + 2U, 1U);
        if (ret < 0) {
            LogError(LOG_MODEM, "Error reading from the modem, ret = %d", ret);
            m_offset = 0U;
            return RTM_ERROR;
        }

        if (ret == 0)
            return RTM_TIMEOUT;

        m_offset = 3U;
    }

    if (m_offset >= 3U) {
        // Use later two byte length field
        if (m_length == 0U) {
            int ret = m_serial.read(m_buffer + 3U, 2U);
            if (ret < 0) {
                LogError(LOG_MODEM, "Error reading from the modem, ret = %d", ret);
                m_offset = 0U;
                return RTM_ERROR;
            }

            if (ret == 0)
                return RTM_TIMEOUT;

            m_length = (m_buffer[3U] << 8) | m_buffer[4U];
            m_offset = 5U;
        }

        while (m_offset < m_length) {
            int ret = m_serial.read(m_buffer + m_offset, m_length - m_offset);
            if (ret < 0) {
                LogError(LOG_MODEM, "Error reading from the modem, ret = %d", ret);
                m_offset = 0U;
                return RTM_ERROR;
            }

            if (ret == 0)
                return RTM_TIMEOUT;

            if (ret > 0)
                m_offset += ret;
        }
    }

    m_offset = 0U;

    // Utils::dump(1U, "Received", m_buffer, m_length);

    return RTM_OK;
}
