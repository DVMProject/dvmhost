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
*   Copyright (C) 2011-2021 by Jonathan Naylor G4KLX
*   Copyright (C) 2017-2022 by Bryan Biedenkapp N2PLL
*   Copyright (C) 2021 by Nat Moore <https://github.com/jelimoore>
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
#include "nxdn/NXDNDefines.h"
#include "modem/Modem.h"
#include "edac/CRC.h"
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
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define CONFIG_OPT_MISMATCH_STR "Configuration option mismatch; "
#define CONFIG_OPT_ALTERED_STR "Configuration option manually altered; "
#define MODEM_CONFIG_AREA_DISAGREE_STR "modem configuration area disagreement, "

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

// Check flash configuration value against class value.
#define FLASH_VALUE_CHECK(_CLASS_VAL, _FLASH_VAL, _DEFAULT, _STR)                        \
    if (_CLASS_VAL == _DEFAULT && _CLASS_VAL != _FLASH_VAL) {                            \
        LogWarning(LOG_MODEM, CONFIG_OPT_MISMATCH_STR MODEM_CONFIG_AREA_DISAGREE_STR _STR " = %u, " _STR " (flash) = %u", _CLASS_VAL, _FLASH_VAL); \
        _CLASS_VAL = _FLASH_VAL;                                                         \
    } else {                                                                             \
        if (_CLASS_VAL != _DEFAULT && _CLASS_VAL != _FLASH_VAL) {                        \
            LogWarning(LOG_MODEM, CONFIG_OPT_ALTERED_STR MODEM_CONFIG_AREA_DISAGREE_STR _STR " = %u, " _STR " (flash) = %u", _CLASS_VAL, _FLASH_VAL); \
        }                                                                                \
    }

// Check flash configuration value against class value.
#define FLASH_VALUE_CHECK_FLOAT(_CLASS_VAL, _FLASH_VAL, _DEFAULT, _STR)                  \
    if (_CLASS_VAL == _DEFAULT && _CLASS_VAL != _FLASH_VAL) {                            \
        LogWarning(LOG_MODEM, CONFIG_OPT_MISMATCH_STR MODEM_CONFIG_AREA_DISAGREE_STR _STR " = %f, " _STR " (flash) = %f", _CLASS_VAL, _FLASH_VAL); \
        _CLASS_VAL = _FLASH_VAL;                                                         \
    } else {                                                                             \
        if (_CLASS_VAL != _DEFAULT && _CLASS_VAL != _FLASH_VAL) {                        \
            LogWarning(LOG_MODEM, CONFIG_OPT_ALTERED_STR MODEM_CONFIG_AREA_DISAGREE_STR _STR " = %f, " _STR " (flash) = %f", _CLASS_VAL, _FLASH_VAL); \
        }                                                                                \
    }

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the Modem class.
/// </summary>
/// <param name="port">Port the air interface modem is connected to.</param>
/// <param name="duplex">Flag indicating the modem is operating in duplex mode.</param>
/// <param name="rxInvert">Flag indicating the Rx polarity should be inverted.</param>
/// <param name="txInvert">Flag indicating the Tx polarity should be inverted.</param>
/// <param name="pttInvert">Flag indicating the PTT polarity should be inverted.</param>
/// <param name="dcBlocker">Flag indicating whether the DSP DC-level blocking should be enabled.</param>
/// <param name="cosLockout">Flag indicating whether the COS signal should be used to lockout the modem.</param>
/// <param name="fdmaPreamble">Count of FDMA preambles to transmit before data. (P25/DMR DMO)</param>
/// <param name="dmrRxDelay">Compensate for delay in receiver audio chain in ms. Usually DSP based.</param>
/// <param name="p25CorrCount">P25 Correlation Countdown.</param>
/// <param name="dmrQueueSize">Modem DMR frame buffer queue size (bytes).</param>
/// <param name="p25QueueSize">Modem P25 frame buffer queue size (bytes).</param>
/// <param name="nxdnQueueSize">Modem NXDN frame buffer queue size (bytes).</param>
/// <param name="disableOFlowReset">Flag indicating whether the ADC/DAC overflow reset logic is disabled.</param>
/// <param name="ignoreModemConfigArea">Flag indicating whether the modem configuration area is ignored.</param>
/// <param name="dumpModemStatus">Flag indicating whether the modem status is dumped to the log.</param>
/// <param name="trace">Flag indicating whether air interface modem trace is enabled.</param>
/// <param name="debug">Flag indicating whether air interface modem debug is enabled.</param>
Modem::Modem(port::IModemPort* port, bool duplex, bool rxInvert, bool txInvert, bool pttInvert, bool dcBlocker, bool cosLockout,
    uint8_t fdmaPreamble, uint8_t dmrRxDelay, uint8_t p25CorrCount, uint32_t dmrQueueSize, uint32_t p25QueueSize, uint32_t nxdnQueueSize,
    bool disableOFlowReset, bool ignoreModemConfigArea, bool dumpModemStatus, bool trace, bool debug) :
    m_port(port),
    m_protoVer(0U),
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
    m_p25CorrCount(p25CorrCount),
    m_rxLevel(0U),
    m_cwIdTXLevel(0U),
    m_dmrTXLevel(0U),
    m_p25TXLevel(0U),
    m_nxdnTXLevel(0U),
    m_disableOFlowReset(disableOFlowReset),
    m_dmrEnabled(false),
    m_p25Enabled(false),
    m_nxdnEnabled(false),
    m_rxDCOffset(0),
    m_txDCOffset(0),
    m_isHotspot(false),
    m_rxFrequency(0U),
    m_rxTuning(0),
    m_txFrequency(0U),
    m_txTuning(0),
    m_rfPower(0U),
    m_dmrDiscBWAdj(0),
    m_p25DiscBWAdj(0),
    m_nxdnDiscBWAdj(0),
    m_dmrPostBWAdj(0),
    m_p25PostBWAdj(0),
    m_nxdnPostBWAdj(0),
    m_adfGainMode(ADF_GAIN_AUTO),
    m_afcEnable(false),
    m_afcKI(11U),
    m_afcKP(4U),
    m_afcRange(1U),
    m_dmrSymLevel3Adj(0),
    m_dmrSymLevel1Adj(0),
    m_p25SymLevel3Adj(0),
    m_p25SymLevel1Adj(0),
    m_nxdnSymLevel3Adj(0),
    m_nxdnSymLevel1Adj(0),
    m_rxCoarsePot(127U),
    m_rxFinePot(127U),
    m_txCoarsePot(127U),
    m_txFinePot(127U),
    m_rssiCoarsePot(127U),
    m_rssiFinePot(127U),
    m_adcOverFlowCount(0U),
    m_dacOverFlowCount(0U),
    m_modemState(STATE_IDLE),
    m_buffer(nullptr),
    m_length(0U),
    m_rspOffset(0U),
    m_rspState(RESP_START),
    m_rspDoubleLength(false),
    m_rspType(CMD_GET_STATUS),
    m_openPortHandler(nullptr),
    m_closePortHandler(nullptr),
    m_rspHandler(nullptr),
    m_rxDMRData1(dmrQueueSize, "Modem RX DMR1"),
    m_rxDMRData2(dmrQueueSize, "Modem RX DMR2"),
    m_rxP25Data(p25QueueSize, "Modem RX P25"),
    m_rxNXDNData(nxdnQueueSize, "Modem RX NXDN"),
    m_useDFSI(false),
    m_statusTimer(1000U, 0U, 250U),
    m_inactivityTimer(1000U, 4U),
    m_dmrSpace1(0U),
    m_dmrSpace2(0U),
    m_p25Space(0U),
    m_nxdnSpace(0U),
    m_tx(false),
    m_cd(false),
    m_lockout(false),
    m_error(false),
    m_ignoreModemConfigArea(ignoreModemConfigArea),
    m_flashDisabled(false),
    m_dumpModemStatus(dumpModemStatus),
    m_trace(trace),
    m_debug(debug)
{
    assert(port != nullptr);

    m_buffer = new uint8_t[BUFFER_LENGTH];
}

/// <summary>
/// Finalizes a instance of the Modem class.
/// </summary>
Modem::~Modem()
{
    delete m_port;
    delete[] m_buffer;
}

/// <summary>
/// Sets the RF DC offset parameters.
/// </summary>
/// <param name="txDCOffset"></param>
/// <param name="rxDCOffset"></param>
void Modem::setDCOffsetParams(int txDCOffset, int rxDCOffset)
{
    m_txDCOffset = txDCOffset;
    m_rxDCOffset = rxDCOffset;
}

/// <summary>
/// Sets the enabled modes.
/// </summary>
/// <param name="dmrEnabled"></param>
/// <param name="p25Enabled"></param>
/// <param name="nxdnEnabled"></param>
void Modem::setModeParams(bool dmrEnabled, bool p25Enabled, bool nxdnEnabled)
{
    m_dmrEnabled = dmrEnabled;
    m_p25Enabled = p25Enabled;
    m_nxdnEnabled = nxdnEnabled;
}

/// <summary>
/// Sets the RF deviation levels.
/// </summary>
/// <param name="rxLevel"></param>
/// <param name="cwIdTXLevel"></param>
/// <param name="dmrTXLevel"></param>
/// <param name="p25TXLevel"></param>
/// <param name="nxdnTXLevel"></param>
void Modem::setLevels(float rxLevel, float cwIdTXLevel, float dmrTXLevel, float p25TXLevel, float nxdnTXLevel)
{
    m_rxLevel = rxLevel;
    m_cwIdTXLevel = cwIdTXLevel;
    m_dmrTXLevel = dmrTXLevel;
    m_p25TXLevel = p25TXLevel;
    m_nxdnTXLevel = nxdnTXLevel;
}

/// <summary>
/// Sets the symbol adjustment levels
/// </summary>
/// <param name="dmrSymLevel3Adj"></param>
/// <param name="dmrSymLevel1Adj"></param>
/// <param name="p25SymLevel3Adj"></param>
/// <param name="p25SymLevel1Adj"></param>
/// <param name="nxdnSymLevel3Adj"></param>
/// <param name="nxdnSymLevel1Adj"></param>
void Modem::setSymbolAdjust(int dmrSymLevel3Adj, int dmrSymLevel1Adj, int p25SymLevel3Adj, int p25SymLevel1Adj, int nxdnSymLevel3Adj, int nxdnSymLevel1Adj)
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

    m_nxdnSymLevel3Adj = nxdnSymLevel3Adj;
    if (nxdnSymLevel3Adj > 128)
        m_nxdnSymLevel3Adj = 0;
    if (nxdnSymLevel3Adj < -128)
        m_nxdnSymLevel3Adj = 0;

    m_nxdnSymLevel1Adj = nxdnSymLevel1Adj;
    if (nxdnSymLevel1Adj > 128)
        m_nxdnSymLevel1Adj = 0;
    if (nxdnSymLevel1Adj < -128)
        m_nxdnSymLevel1Adj = 0;
}

/// <summary>
/// Sets the RF parameters.
/// </summary>
/// <param name="rxFreq"></param>
/// <param name="txFreq"></param>
/// <param name="rfPower"></param>
/// <param name="dmrDiscBWAdj"></param>
/// <param name="p25DiscBWAdj"></param>
/// <param name="nxdnDiscBWAdj"></param>
/// <param name="dmrPostBWAdj"></param>
/// <param name="p25PostBWAdj"></param>
/// <param name="nxdnPostBWAdj"></param>
/// <param name="gainMode"></param>
/// <param name="afcEnable"></param>
/// <param name="afcKI"></param>
/// <param name="afcKP"></param>
/// <param name="afcRange"></param>
void Modem::setRFParams(uint32_t rxFreq, uint32_t txFreq, int rxTuning, int txTuning, uint8_t rfPower,
    int8_t dmrDiscBWAdj, int8_t p25DiscBWAdj, int8_t nxdnDiscBWAdj,
    int8_t dmrPostBWAdj, int8_t p25PostBWAdj, int8_t nxdnPostBWAdj,
    ADF_GAIN_MODE gainMode,
    bool afcEnable, uint8_t afcKI, uint8_t afcKP, uint8_t afcRange)
{
    m_adfGainMode = gainMode;
    m_rfPower = rfPower;
    m_rxFrequency = rxFreq;
    m_rxTuning = rxTuning;
    m_txFrequency = txFreq;
    m_txTuning = txTuning;

    m_dmrDiscBWAdj = dmrDiscBWAdj;
    m_p25DiscBWAdj = p25DiscBWAdj;
    m_nxdnDiscBWAdj = nxdnDiscBWAdj;
    m_dmrPostBWAdj = dmrPostBWAdj;
    m_p25PostBWAdj = p25PostBWAdj;
    m_nxdnPostBWAdj = nxdnPostBWAdj;

    m_afcEnable = afcEnable;
    m_afcKI = afcKI;
    m_afcKP = afcKP;
    m_afcRange = afcRange;
}

/// <summary>
/// Sets the softpot parameters.
/// </summary>
/// <param name="rxCoarse"></param>
/// <param name="rxFine"></param>
/// <param name="txCoarse"></param>
/// <param name="txFine"></param>
/// <param name="rssiCoarse"></param>
/// <param name="rssiFine"></param>
void Modem::setSoftPot(uint8_t rxCoarse, uint8_t rxFine, uint8_t txCoarse, uint8_t txFine, uint8_t rssiCoarse, uint8_t rssiFine)
{
    m_rxCoarsePot = rxCoarse;
    m_rxFinePot = rxFine;

    m_txCoarsePot = txCoarse;
    m_txFinePot = txFine;

    m_rssiCoarsePot = rssiCoarse;
    m_rssiFinePot = rssiFine;
}

/// <summary>
/// Sets the DMR color code.
/// </summary>
/// <param name="colorCode"></param>
void Modem::setDMRColorCode(uint32_t colorCode)
{
    assert(colorCode < 16U);

    m_dmrColorCode = colorCode;
}

/// <summary>
/// Sets the P25 NAC.
/// </summary>
/// <param name="nac"></param>
void Modem::setP25NAC(uint32_t nac)
{
    assert(nac < 0xFFFU);

    m_p25NAC = nac;
}

/// <summary>
/// Sets the P25 DFSI data mode.
/// </summary>
/// <param name="nac"></param>
void Modem::setP25DFSI(bool dfsi)
{
    m_useDFSI = dfsi;
}

/// <summary>
/// Sets the RF receive deviation levels.
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
#if DEBUG_MODEM
    Utils::dump(1U, "Modem::setRXLevel(), Written", buffer, 16U);
#endif
    int ret = write(buffer, 16U);
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
#if DEBUG_MODEM
    Utils::dump(1U, "Modem::setRXLevel(), Response", m_buffer, m_length);
#endif
    if (resp == RTM_OK && m_buffer[2U] == CMD_NAK) {
        LogError(LOG_MODEM, "NAK, SET_RXLEVEL, command = 0x%02X, reason = %u", m_buffer[3U], m_buffer[4U]);
    }
}

/// <summary>
/// Sets a custom modem response handler.
/// </summary>
/// <remarks>
/// If the response handler returns true, processing will stop, otherwise it will continue.
/// </remarks>
/// <param name="handler"></param>
void Modem::setResponseHandler(std::function<MODEM_RESP_HANDLER> handler)
{
    assert(handler != nullptr);

    m_rspHandler = handler;
}

/// <summary>
/// Sets a custom modem open port handler.
/// </summary>
/// <remarks>
/// If the open handler is set, it is the responsibility of the handler to complete air interface
/// initialization (i.e. write configuration, etc).
/// </remarks>
/// <param name="handler"></param>
void Modem::setOpenHandler(std::function<MODEM_OC_PORT_HANDLER> handler)
{
    assert(handler != nullptr);

    m_openPortHandler = handler;
}

/// <summary>
/// Sets a custom modem close port handler.
/// </summary>
/// <param name="handler"></param>
void Modem::setCloseHandler(std::function<MODEM_OC_PORT_HANDLER> handler)
{
    assert(handler != nullptr);

    m_closePortHandler = handler;
}

/// <summary>
/// Opens connection to the air interface modem.
/// </summary>
/// <returns>True, if connection to modem is established, otherwise false.</returns>
bool Modem::open()
{
    LogMessage(LOG_MODEM, "Initializing modem");

    bool ret = m_port->open();
    if (!ret)
        return false;

    ret = getFirmwareVersion();
    if (!ret) {
        m_port->close();
        return false;
    }
    else {
        // Stopping the inactivity timer here when a firmware version has been
        // successfuly read prevents the death spiral of "no reply from modem..."
        m_inactivityTimer.stop();
    }

    m_rspOffset = 0U;
    m_rspState = RESP_START;

    ret = readFlash();
    if (!ret) {
        LogError(LOG_MODEM, "Unable to read configuration on modem flash device! Using local configuration.");
        m_flashDisabled = true;
    }

    // do we have an open port handler?
    if (m_openPortHandler) {
        ret = m_openPortHandler(this);
        if (!ret)
            return false;

        m_error = false;
        return true;
    }

    ret = writeRFParams();
    if (!ret) {
        ret = writeRFParams();
        if (!ret) {
            LogError(LOG_MODEM, "Modem unresponsive to RF parameters set after 2 attempts. Stopping.");
            m_port->close();
            return false;
        }
    }

    ret = writeConfig();
    if (!ret) {
        ret = writeConfig();
        if (!ret) {
            LogError(LOG_MODEM, "Modem unresponsive to configuration set after 2 attempts. Stopping.");
            m_port->close();
            return false;
        }
    }

    writeSymbolAdjust();

    m_statusTimer.start();

    m_error = false;

    LogMessage(LOG_MODEM, "Modem Ready [Direct Mode]");
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
        reset();
    }

    bool forceModemReset = false;
    RESP_TYPE_DVM type = getResponse();

    // do we have a custom response handler?
    if (m_rspHandler != nullptr) {
        // execute custom response handler
        if (m_rspHandler(this, ms, type, m_rspDoubleLength, m_buffer, m_length)) {
            // all logic handled by handler -- return
            return;
        }
    }

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
#if defined(ENABLE_DMR)
            //if (m_trace)
            //    Utils::dump(1U, "RX DMR Data 1", m_buffer, m_length);

            if (m_rspDoubleLength) {
                LogError(LOG_MODEM, "CMD_DMR_DATA1 double length?; len = %u", m_length);
                break;
            }

            uint8_t data = m_length - 2U;
            m_rxDMRData1.addData(&data, 1U);

            if (m_buffer[3U] == (dmr::DMR_SYNC_DATA | dmr::DT_TERMINATOR_WITH_LC))
                data = TAG_EOT;
            else
                data = TAG_DATA;
            m_rxDMRData1.addData(&data, 1U);

            m_rxDMRData1.addData(m_buffer + 3U, m_length - 3U);
#endif // defined(ENABLE_DMR)
        }
        break;

        case CMD_DMR_DATA2:
        {
#if defined(ENABLE_DMR)
            //if (m_trace)
            //    Utils::dump(1U, "RX DMR Data 2", m_buffer, m_length);

            if (m_rspDoubleLength) {
                LogError(LOG_MODEM, "CMD_DMR_DATA2 double length?; len = %u", m_length);
                break;
            }

            uint8_t data = m_length - 2U;
            m_rxDMRData2.addData(&data, 1U);

            if (m_buffer[3U] == (dmr::DMR_SYNC_DATA | dmr::DT_TERMINATOR_WITH_LC))
                data = TAG_EOT;
            else
                data = TAG_DATA;
            m_rxDMRData2.addData(&data, 1U);

            m_rxDMRData2.addData(m_buffer + 3U, m_length - 3U);
#endif // defined(ENABLE_DMR)
        }
        break;

        case CMD_DMR_LOST1:
        {
#if defined(ENABLE_DMR)
            //if (m_trace)
            //    Utils::dump(1U, "RX DMR Lost 1", m_buffer, m_length);

            if (m_rspDoubleLength) {
                LogError(LOG_MODEM, "CMD_DMR_LOST1 double length?; len = %u", m_length);
                break;
            }

            uint8_t data = 1U;
            m_rxDMRData1.addData(&data, 1U);

            data = TAG_LOST;
            m_rxDMRData1.addData(&data, 1U);
#endif // defined(ENABLE_DMR)
        }
        break;

        case CMD_DMR_LOST2:
        {
#if defined(ENABLE_DMR)
            //if (m_trace)
            //    Utils::dump(1U, "RX DMR Lost 2", m_buffer, m_length);

            if (m_rspDoubleLength) {
                LogError(LOG_MODEM, "CMD_DMR_LOST2 double length?; len = %u", m_length);
                break;
            }

            uint8_t data = 1U;
            m_rxDMRData2.addData(&data, 1U);

            data = TAG_LOST;
            m_rxDMRData2.addData(&data, 1U);
#endif // defined(ENABLE_DMR)
        }
        break;

        /** Project 25 */
        case CMD_P25_DATA:
        {
#if defined(ENABLE_P25)
            //if (m_trace)
            //    Utils::dump(1U, "RX P25 Data", m_buffer, m_length);

            if (m_rspDoubleLength) {
                LogError(LOG_MODEM, "CMD_P25_DATA double length?; len = %u", m_length);
                break;
            }

            uint8_t data = m_length - 2U;
            m_rxP25Data.addData(&data, 1U);

            data = TAG_DATA;
            m_rxP25Data.addData(&data, 1U);

            m_rxP25Data.addData(m_buffer + 3U, m_length - 3U);
#endif // defined(ENABLE_P25)
        }
        break;

        case CMD_P25_LOST:
        {
#if defined(ENABLE_P25)
            //if (m_trace)
            //    Utils::dump(1U, "RX P25 Lost", m_buffer, m_length);

            if (m_rspDoubleLength) {
                LogError(LOG_MODEM, "CMD_P25_LOST double length?; len = %u", m_length);
                break;
            }

            uint8_t data = 1U;
            m_rxP25Data.addData(&data, 1U);

            data = TAG_LOST;
            m_rxP25Data.addData(&data, 1U);
#endif // defined(ENABLE_P25)
        }
        break;

        /** Next Generation Digital Narrowband */
        case CMD_NXDN_DATA:
        {
#if defined(ENABLE_NXDN)
            //if (m_trace)
            //    Utils::dump(1U, "RX NXDN Data", m_buffer, m_length);

            if (m_rspDoubleLength) {
                LogError(LOG_MODEM, "CMD_NXDN_DATA double length?; len = %u", m_length);
                break;
            }

            uint8_t data = m_length - 2U;
            m_rxNXDNData.addData(&data, 1U);

            data = TAG_DATA;
            m_rxNXDNData.addData(&data, 1U);

            m_rxNXDNData.addData(m_buffer + 3U, m_length - 3U);
#endif // defined(ENABLE_NXDN)
        }
        break;

        case CMD_NXDN_LOST:
        {
#if defined(ENABLE_NXDN)
            //if (m_trace)
            //    Utils::dump(1U, "RX NXDN Lost", m_buffer, m_length);

            if (m_rspDoubleLength) {
                LogError(LOG_MODEM, "CMD_NXDN_LOST double length?; len = %u", m_length);
                break;
            }

            uint8_t data = 1U;
            m_rxNXDNData.addData(&data, 1U);

            data = TAG_LOST;
            m_rxNXDNData.addData(&data, 1U);
#endif // defined(ENABLE_NXDN)
        }
        break;

        /** General */
        case CMD_GET_STATUS:
        {
            //if (m_trace)
            //   Utils::dump(1U, "Get Status", m_buffer, m_length);

            m_isHotspot = (m_buffer[3U] & 0x01U) == 0x01U;

            bool dmrEnable = (m_buffer[3U] & 0x02U) == 0x02U;
            bool p25Enable = (m_buffer[3U] & 0x08U) == 0x08U;
            bool nxdnEnable = (m_buffer[3U] & 0x10U) == 0x10U;

            m_modemState = (DVM_STATE)m_buffer[4U];

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
                }
                else {
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
                }
                else {
                    m_dacOverFlowCount = 0U;
                }
            }
            else {
                if (m_dacOverFlowCount != 0U) {
                    m_dacOverFlowCount--;
                }
            }

            m_cd = (m_buffer[5U] & 0x40U) == 0x40U;

            // spaces from the modem are returned in "logical" frame count, not raw byte size
            m_dmrSpace1 = m_buffer[7U] * (dmr::DMR_FRAME_LENGTH_BYTES + 2U);
            m_dmrSpace2 = m_buffer[8U] * (dmr::DMR_FRAME_LENGTH_BYTES + 2U);
            m_p25Space = m_buffer[10U] * (p25::P25_LDU_FRAME_LENGTH_BYTES);
            m_nxdnSpace = m_buffer[11U] * (nxdn::NXDN_FRAME_LENGTH_BYTES);

            if (m_dumpModemStatus) {
                LogDebug(LOG_MODEM, "Modem::clock(), CMD_GET_STATUS, isHotspot = %u, dmr = %u / %u, p25 = %u / %u, nxdn = %u / %u, modemState = %u, tx = %u, adcOverflow = %u, rxOverflow = %u, txOverflow = %u, dacOverflow = %u, dmrSpace1 = %u, dmrSpace2 = %u, p25Space = %u, nxdnSpace = %u",
                    m_isHotspot, dmrEnable, m_dmrEnabled, p25Enable, m_p25Enabled, nxdnEnable, m_nxdnEnabled, m_modemState, m_tx, adcOverflow, rxOverflow, txOverflow, dacOverflow, m_dmrSpace1, m_dmrSpace2, m_p25Space, m_nxdnSpace);
                LogDebug(LOG_MODEM, "Modem::clock(), CMD_GET_STATUS, rxDMRData1 size = %u, len = %u, free = %u; rxDMRData2 size = %u, len = %u, free = %u, rxP25Data size = %u, len = %u, free = %u, rxNXDNData size = %u, len = %u, free = %u",
                    m_rxDMRData1.length(), m_rxDMRData1.dataSize(), m_rxDMRData1.freeSpace(), m_rxDMRData2.length(), m_rxDMRData2.dataSize(), m_rxDMRData2.freeSpace(),
                    m_rxP25Data.length(), m_rxP25Data.dataSize(), m_rxP25Data.freeSpace(), m_rxNXDNData.length(), m_rxNXDNData.dataSize(), m_rxNXDNData.freeSpace());
            }

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
        case CMD_DEBUG_DUMP:
            printDebug(m_buffer, m_length);
            break;

        default:
            LogWarning(LOG_MODEM, "Unknown message, type = %02X", m_buffer[2U]);
            Utils::dump("Buffer dump", m_buffer, m_length);
            break;
        }
    }

    // force a modem reset because of a error condition
    if (forceModemReset) {
        forceModemReset = false;
        reset();
    }
}

/// <summary>
/// Closes connection to the air interface modem.
/// </summary>
void Modem::close()
{
    LogDebug(LOG_MODEM, "Closing the modem");
    m_port->close();

    // do we have a close port handler?
    if (m_closePortHandler != nullptr) {
        m_closePortHandler(this);
    }
}

/// <summary>
/// Reads DMR Slot 1 frame data from the DMR Slot 1 ring buffer.
/// </summary>
/// <param name="data">Buffer to write frame data to.</param>
/// <returns>Length of data read from ring buffer.</returns>
uint32_t Modem::readDMRData1(uint8_t* data)
{
    assert(data != nullptr);

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
    assert(data != nullptr);

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
    assert(data != nullptr);

    if (m_rxP25Data.isEmpty())
        return 0U;

    uint8_t len = 0U;
    m_rxP25Data.getData(&len, 1U);
    m_rxP25Data.getData(data, len);

    return len;
}

/// <summary>
/// Reads NXDN frame data from the NXDN ring buffer.
/// </summary>
/// <param name="data">Buffer to write frame data to.</param>
/// <returns>Length of data read from ring buffer.</returns>
uint32_t Modem::readNXDNData(uint8_t* data)
{
    assert(data != nullptr);

    if (m_rxNXDNData.isEmpty())
        return 0U;

    uint8_t len = 0U;
    m_rxNXDNData.getData(&len, 1U);
    m_rxNXDNData.getData(data, len);

    return len;
}

/// <summary>
/// Helper to test if the DMR Slot 1 ring buffer has free space.
/// </summary>
/// <returns>True, if the DMR Slot 1 ring buffer has free space, otherwise false.</returns>
bool Modem::hasDMRSpace1() const
{
    return m_dmrSpace1 >= (dmr::DMR_FRAME_LENGTH_BYTES + 2U);
}

/// <summary>
/// Helper to test if the DMR Slot 2 ring buffer has free space.
/// </summary>
/// <returns>True, if the DMR Slot 2 ring buffer has free space, otherwise false.</returns>
bool Modem::hasDMRSpace2() const
{
    return m_dmrSpace2 >= (dmr::DMR_FRAME_LENGTH_BYTES + 2U);
}

/// <summary>
/// Helper to test if the P25 ring buffer has free space.
/// </summary>
/// <param name="length"></param>
/// <returns>True, if the P25 ring buffer has free space, otherwise false.</returns>
bool Modem::hasP25Space(uint32_t length) const
{
    return m_p25Space >= length;
}

/// <summary>
/// Helper to test if the NXDN ring buffer has free space.
/// </summary>
/// <returns>True, if the NXDN ring buffer has free space, otherwise false.</returns>
bool Modem::hasNXDNSpace() const
{
    return m_nxdnSpace >= nxdn::NXDN_FRAME_LENGTH_BYTES;
}

/// <summary>
/// Helper to test if the modem is a hotspot.
/// </summary>
/// <returns>True, if the modem is a hotspot, otherwise false.</returns>
bool Modem::isHotspot() const
{
    return m_isHotspot;
}

/// <summary>
/// Helper to test if the modem is in P25 DFSI data mode.
/// </summary>
/// <returns>True, if the modem is in P25 DFSI data mode, otherwise false.</returns>
bool Modem::isP25DFSI() const
{
    return m_useDFSI;
}

/// <summary>
/// Flag indicating whether or not the air interface modem is transmitting.
/// </summary>
/// <returns>True, if air interface modem is transmitting, otherwise false.</returns>
bool Modem::hasTX() const
{
    return m_tx;
}

/// <summary>
/// Flag indicating whether or not the air interface modem has carrier detect.
/// </summary>
/// <returns>True, if air interface modem has carrier detect, otherwise false.</returns>
bool Modem::hasCD() const
{
    return m_cd;
}

/// <summary>
/// Flag indicating whether or not the air interface modem is currently locked out.
/// </summary>
/// <returns>True, if air interface modem is currently locked out, otherwise false.</returns>
bool Modem::hasLockout() const
{
    return m_lockout;
}

/// <summary>
/// Flag indicating whether or not the air interface modem is currently in an error condition.
/// </summary>
/// <returns>True, if the air interface modem is current in an error condition, otherwise false.</returns>
bool Modem::hasError() const
{
    return m_error;
}

/// <summary>
/// Clears any buffered DMR Slot 1 frame data to be sent to the air interface modem.
/// </summary>
void Modem::clearDMRData1()
{
    // TODO -- implement modem side buffer clear
}

/// <summary>
/// Clears any buffered DMR Slot 2 frame data to be sent to the air interface modem.
/// </summary>
void Modem::clearDMRData2()
{
    // TODO -- implement modem side buffer clear
}

/// <summary>
/// Clears any buffered P25 frame data to be sent to the air interface modem.
/// </summary>
void Modem::clearP25Data()
{
    uint8_t buffer[3U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 3U;
    buffer[2U] = CMD_P25_CLEAR;
#if DEBUG_MODEM
    Utils::dump(1U, "Modem::clearP25Data(), Written", buffer, 3U);
#endif
    write(buffer, 3U);
}

/// <summary>
/// Clears any buffered NXDN frame data to be sent to the air interface modem.
/// </summary>
void Modem::clearNXDNData()
{
    // TODO -- implement modem side buffer clear
}

/// <summary>
/// Internal helper to inject DMR Slot 1 frame data as if it came from the air interface modem.
/// </summary>
/// <param name="data">Data to write to ring buffer.</param>
/// <param name="length">Length of data to write.</param>
void Modem::injectDMRData1(const uint8_t* data, uint32_t length)
{
#if defined(ENABLE_DMR)
    assert(data != nullptr);
    assert(length > 0U);

    if (m_useDFSI) {
        LogWarning(LOG_MODEM, "Cannot inject DMR Slot 1 Data in DFSI mode");
        return;
    }

    if (m_trace)
        Utils::dump(1U, "Injected DMR Slot 1 Data", data, length);

    uint8_t val = length;
    m_rxDMRData1.addData(&val, 1U);

    val = TAG_DATA;
    m_rxDMRData1.addData(&val, 1U);
    val = dmr::DMR_SYNC_VOICE & dmr::DMR_SYNC_DATA;    // valid sync
    m_rxDMRData1.addData(&val, 1U);

    m_rxDMRData1.addData(data, length);
#endif // defined(ENABLE_DMR)
}

/// <summary>
/// Internal helper to inject DMR Slot 2 frame data as if it came from the air interface modem.
/// </summary>
/// <param name="data">Data to write to ring buffer.</param>
/// <param name="length">Length of data to write.</param>
void Modem::injectDMRData2(const uint8_t* data, uint32_t length)
{
#if defined(ENABLE_DMR)
    assert(data != nullptr);
    assert(length > 0U);

    if (m_useDFSI) {
        LogWarning(LOG_MODEM, "Cannot inject DMr Slot 2 Data in DFSI mode");
        return;
    }

    if (m_trace)
        Utils::dump(1U, "Injected DMR Slot 2 Data", data, length);

    uint8_t val = length;
    m_rxDMRData2.addData(&val, 1U);

    val = TAG_DATA;
    m_rxDMRData2.addData(&val, 1U);
    val = dmr::DMR_SYNC_VOICE & dmr::DMR_SYNC_DATA;    // valid sync
    m_rxDMRData2.addData(&val, 1U);

    m_rxDMRData2.addData(data, length);
#endif // defined(ENABLE_DMR)
}

/// <summary>
/// Internal helper to inject P25 frame data as if it came from the air interface modem.
/// </summary>
/// <param name="data">Data to write to ring buffer.</param>
/// <param name="length">Length of data to write.</param>
void Modem::injectP25Data(const uint8_t* data, uint32_t length)
{
#if defined(ENABLE_P25)
    assert(data != nullptr);
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
#endif // defined(ENABLE_P25)
}

/// <summary>
/// Internal helper to inject NXDN frame data as if it came from the air interface modem.
/// </summary>
/// <param name="data">Data to write to ring buffer.</param>
/// <param name="length">Length of data to write.</param>
void Modem::injectNXDNData(const uint8_t* data, uint32_t length)
{
#if defined(ENABLE_NXDN)
    assert(data != nullptr);
    assert(length > 0U);

    if (m_trace)
        Utils::dump(1U, "Injected NXDN Data", data, length);

    uint8_t val = length;
    m_rxNXDNData.addData(&val, 1U);

    val = TAG_DATA;
    m_rxNXDNData.addData(&val, 1U);
    val = 0x01U;    // valid sync
    m_rxNXDNData.addData(&val, 1U);

    m_rxNXDNData.addData(data, length);
#endif // defined(ENABLE_NXDN)
}

/// <summary>
/// Writes DMR Slot 1 frame data to the DMR Slot 1 ring buffer.
/// </summary>
/// <param name="data">Data to write to ring buffer.</param>
/// <param name="length">Length of data to write.</param>
/// <returns>True, if data is written, otherwise false.</returns>
bool Modem::writeDMRData1(const uint8_t* data, uint32_t length)
{
#if defined(ENABLE_DMR)
    assert(data != nullptr);
    assert(length > 0U);

    const uint8_t MAX_LENGTH = 40U;

    if (data[0U] != TAG_DATA && data[0U] != TAG_EOT)
        return false;
    if (length > MAX_LENGTH) {
        LogError(LOG_MODEM, "Modem::writeDMRData1(); request data to write >%u?, len = %u", MAX_LENGTH, length);
        Utils::dump(1U, "Modem::writeDMRData1(); Attmpted Data", data, length);
        return false;
    }

    uint8_t buffer[MAX_LENGTH];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = length + 2U;
    buffer[2U] = CMD_DMR_DATA1;

    ::memcpy(buffer + 3U, data + 1U, length - 1U);

    uint8_t len = length + 2U;

    // write or buffer DMR slot 1 data to air interface
    if (m_dmrSpace1 >= length) {
        if (m_debug)
            LogDebug(LOG_MODEM, "Modem::writeDMRData1(); immediate write (len %u)", length);
        //if (m_trace)
        //    Utils::dump(1U, "Immediate TX DMR Data 1", buffer, len);

        int ret = write(buffer, len);
        if (ret != int(len)) {
            LogError(LOG_MODEM, "Error writing DMR slot 1 data");
            return false;
        }

        m_dmrSpace1 -= length;
    }
    else {
        return false;
    }

    return true;
#else
    return false;
#endif // defined(ENABLE_DMR)
}

/// <summary>
/// Writes DMR Slot 2 frame data to the DMR Slot 2 ring buffer.
/// </summary>
/// <param name="data">Data to write to ring buffer.</param>
/// <param name="length">Length of data to write.</param>
/// <returns>True, if data is written, otherwise false.</returns>
bool Modem::writeDMRData2(const uint8_t* data, uint32_t length)
{
#if defined(ENABLE_DMR)
    assert(data != nullptr);
    assert(length > 0U);

    const uint8_t MAX_LENGTH = 40U;

    if (data[0U] != TAG_DATA && data[0U] != TAG_EOT)
        return false;
    if (length > MAX_LENGTH) {
        LogError(LOG_MODEM, "Modem::writeDMRData2(); request data to write >%u?, len = %u", MAX_LENGTH, length);
        Utils::dump(1U, "Modem::writeDMRData2(); Attmpted Data", data, length);
        return false;
    }

    uint8_t buffer[MAX_LENGTH];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = length + 2U;
    buffer[2U] = CMD_DMR_DATA2;

    ::memcpy(buffer + 3U, data + 1U, length - 1U);

    uint8_t len = length + 2U;

    // write or buffer DMR slot 2 data to air interface
    if (m_dmrSpace2 >= length) {
        if (m_debug)
            LogDebug(LOG_MODEM, "Modem::writeDMRData2(); immediate write (len %u)", length);
        //if (m_trace)
        //    Utils::dump(1U, "Immediate TX DMR Data 2", buffer, len);

        int ret = write(buffer, len);
        if (ret != int(len)) {
            LogError(LOG_MODEM, "Error writing DMR slot 2 data");
            return false;
        }

        m_dmrSpace2 -= length;
    }
    else {
        return false;
    }

    return true;
#else
    return false;
#endif // defined(ENABLE_DMR)
}

/// <summary>
/// Writes P25 frame data to the P25 ring buffer.
/// </summary>
/// <param name="data">Data to write to ring buffer.</param>
/// <param name="length">Length of data to write.</param>
/// <returns>True, if data is written, otherwise false.</returns>
bool Modem::writeP25Data(const uint8_t* data, uint32_t length)
{
#if defined(ENABLE_P25)
    assert(data != nullptr);
    assert(length > 0U);

    const uint8_t MAX_LENGTH = 250U;

    if (data[0U] != TAG_DATA && data[0U] != TAG_EOT)
        return false;
    if (length > MAX_LENGTH) {
        LogError(LOG_MODEM, "Modem::writeP25Data(); request data to write >%u?, len = %u", MAX_LENGTH, length);
        Utils::dump(1U, "Modem::writeP25Data(); Attmpted Data", data, length);
        return false;
    }

    uint8_t buffer[MAX_LENGTH];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = length + 2U;
    buffer[2U] = CMD_P25_DATA;

    ::memcpy(buffer + 3U, data + 1U, length - 1U);

    uint8_t len = length + 2U;

    // write or buffer P25 data to air interface
    if (m_p25Space >= length) {
        if (m_debug)
            LogDebug(LOG_MODEM, "Modem::writeP25Data(); immediate write (len %u)", length);
        //if (m_trace)
        //    Utils::dump(1U, "Immediate TX P25 Data", buffer, len);

        int ret = write(buffer, len);
        if (ret != int(len)) {
            LogError(LOG_MODEM, "Error writing P25 data");
            return false;
        }

        m_p25Space -= length;
    }
    else {
        return false;
    }

    return true;
#else
    return false;
#endif // defined(ENABLE_P25)
}

/// <summary>
/// Writes NXDN frame data to the NXDN ring buffer.
/// </summary>
/// <param name="data">Data to write to ring buffer.</param>
/// <param name="length">Length of data to write.</param>
/// <returns>True, if data is written, otherwise false.</returns>
bool Modem::writeNXDNData(const uint8_t* data, uint32_t length)
{
#if defined(ENABLE_NXDN)
    assert(data != nullptr);
    assert(length > 0U);

    const uint8_t MAX_LENGTH = 250U;

    if (data[0U] != TAG_DATA && data[0U] != TAG_EOT)
        return false;
    if (length > MAX_LENGTH) {
        LogError(LOG_MODEM, "Modem::writeNXDNData(); request data to write >%u?, len = %u", MAX_LENGTH, length);
        Utils::dump(1U, "Modem::writeNXDNData(); Attmpted Data", data, length);
        return false;
    }

    uint8_t buffer[MAX_LENGTH];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = length + 2U;
    buffer[2U] = CMD_NXDN_DATA;

    ::memcpy(buffer + 3U, data + 1U, length - 1U);

    uint8_t len = length + 2U;

    // write or buffer NXDN data to air interface
    if (m_nxdnSpace >= length) {
        if (m_debug)
            LogDebug(LOG_MODEM, "Modem::writeNXDNData(); immediate write (len %u)", length);
        //if (m_trace)
        //    Utils::dump(1U, "Immediate TX NXDN Data", buffer, len);

        int ret = write(buffer, len);
        if (ret != int(len)) {
            LogError(LOG_MODEM, "Error writing NXDN data");
            return false;
        }

        m_nxdnSpace -= length;
    }
    else {
        return false;
    }

    return true;
#else
    return false;
#endif // defined(ENABLE_NXDN)
}

/// <summary>
/// Triggers the start of DMR transmit.
/// </summary>
/// <param name="tx"></param>
/// <returns>True, if DMR transmit started, otherwise false.</returns>
bool Modem::writeDMRStart(bool tx)
{
#if defined(ENABLE_DMR)
    if (tx && m_tx)
        return true;
    if (!tx && !m_tx)
        return true;

    uint8_t buffer[4U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 4U;
    buffer[2U] = CMD_DMR_START;
    buffer[3U] = tx ? 0x01U : 0x00U;
#if DEBUG_MODEM
    Utils::dump(1U, "Modem::writeDMRStart(), Written", buffer, 4U);
#endif
    return write(buffer, 4U) == 4;
#else
    return false;
#endif // defined(ENABLE_DMR)
}

/// <summary>
/// Writes a DMR short LC to the air interface modem.
/// </summary>
/// <param name="lc"></param>
/// <returns>True, if DMR LC is written, otherwise false.</returns>
bool Modem::writeDMRShortLC(const uint8_t* lc)
{
#if defined(ENABLE_DMR)
    assert(lc != nullptr);

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
#if DEBUG_MODEM
    Utils::dump(1U, "Modem::writeDMRShortLC(), Written", buffer, 12U);
#endif
    return write(buffer, 12U) == 12;
#else
    return false;
#endif // defined(ENABLE_DMR)
}

/// <summary>
/// Writes a DMR abort message for the given slot to the air interface modem.
/// </summary>
/// <param name="slotNo">DMR slot to write abort for.</param>
/// <returns>True, if DMR abort is written, otherwise false.</returns>
bool Modem::writeDMRAbort(uint32_t slotNo)
{
#if defined(ENABLE_DMR)
    uint8_t buffer[4U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 4U;
    buffer[2U] = CMD_DMR_ABORT;
    buffer[3U] = slotNo;
#if DEBUG_MODEM
    Utils::dump(1U, "Modem::writeDMRAbort(), Written", buffer, 4U);
#endif
    return write(buffer, 4U) == 4;
#else
    return false;
#endif // defined(ENABLE_DMR)
}

/// <summary>
/// Sets the ignore flags for setting the CACH Access Type bit on the air interface modem.
/// </summary>
/// <param name="slotNo">DMR slot to set ignore CACH AT flag for.</param>
/// <returns>True, if set flag is written, otherwise false.</returns>
bool Modem::setDMRIgnoreCACH_AT(uint8_t slotNo)
{
#if defined(ENABLE_DMR)
    uint8_t buffer[4U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 4U;
    buffer[2U] = CMD_DMR_CACH_AT_CTRL;
    buffer[3U] = slotNo;

    // are we on a protocol version 3 firmware?
    if (m_protoVer >= 3U) {
#if DEBUG_MODEM
        Utils::dump(1U, "Modem::setDMRIgnoreCACH_AT(), Written", buffer, 4U);
#endif
        return write(buffer, 4U) == 4;
    } else {
        LogWarning(LOG_MODEM, "Modem::setDMRIgnoreCACH_AT(), ignoring CACH AT for slot %u is not supported on this modem!", slotNo);
        return false;
    }
#else
    return false;
#endif // defined(ENABLE_DMR)
}

/// <summary>
/// Writes raw data to the air interface modem.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
/// <returns></returns>
int Modem::write(const uint8_t* data, uint32_t length)
{
    return m_port->write(data, length);
}

/// <summary>
/// Gets the current operating state for the air interface modem.
/// </summary>
/// <returns></returns>
DVM_STATE Modem::getState() const
{
    return m_modemState;
}

/// <summary>
/// Sets the current operating state for the air interface modem.
/// </summary>
/// <param name="state"></param>
/// <returns></returns>
bool Modem::setState(DVM_STATE state)
{
    uint8_t buffer[4U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 4U;
    buffer[2U] = CMD_SET_MODE;
    buffer[3U] = state;
#if DEBUG_MODEM
    Utils::dump(1U, "Modem::setState(), Written", buffer, 4U);
#endif
    return write(buffer, 4U) == 4;
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

    //if (m_trace)
    //    Utils::dump(1U, "CW ID Data", buffer, length + 3U);

    return write(buffer, length + 3U) == int(length + 3U);
}

/// <summary>
/// Returns the protocol version of the connected modem.
/// </param>
/// <returns></returns>
uint8_t Modem::getVersion() const
{
    return m_protoVer;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to warm reset the connection to the modem.
/// </summary>
void Modem::reset()
{
    m_error = true;
    m_adcOverFlowCount = 0U;
    m_dacOverFlowCount = 0U;

    close();

    ::memset(m_buffer, 0x00U, BUFFER_LENGTH);

    Thread::sleep(2000U);        // 2s
    while (!open()) {
        Thread::sleep(5000U);    // 5s
        close();
    }

    // reset modem to last state
    setState(m_modemState);
}

/// <summary>
/// Retrieve the air interface modem version.
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

        int ret = write(buffer, 3U);
        if (ret != 3)
            return false;

        for (uint32_t count = 0U; count < MAX_RESPONSES; count++) {
            Thread::sleep(10U);
            RESP_TYPE_DVM resp = getResponse(true);

            if (resp == RTM_ERROR)
                continue;

            if (resp == RTM_OK && m_buffer[2U] == CMD_GET_VERSION) {
                LogMessage(LOG_MODEM, "Protocol: %02x, CPU: %02X", m_buffer[3U], m_buffer[4U]);
                m_protoVer = m_buffer[3U];

                if (m_protoVer >= 2U) {
                    LogInfoEx(LOG_MODEM, MODEM_VERSION_STR, m_length - 21U, m_buffer + 21U, m_protoVer);
                    if (m_protoVer < 3U) {
                        LogWarning(LOG_MODEM, "Legacy firmware detected; this version of the firmware will not support NXDN or any future enhancments.");
                    }

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
                    case 15U:
                        LogMessage(LOG_MODEM, "Null Modem, UDID: N/A");
                        break;
                    default:
                        LogMessage(LOG_MODEM, "Unknown CPU type: %u", m_buffer[4U]);
                        break;
                    }

                    return true;
                }
                else {
                    LogError(LOG_MODEM, MODEM_UNSUPPORTED_STR, m_protoVer);
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
/// Retrieve the current status from the air interface modem.
/// </summary>
/// <returns></returns>
bool Modem::getStatus()
{
    uint8_t buffer[3U];

    buffer[0U] = DVM_FRAME_START;
    buffer[1U] = 3U;
    buffer[2U] = CMD_GET_STATUS;

    //LogDebug(LOG_MODEM, "getStatus(), polling modem status");

    return write(buffer, 3U) == 3;
}

/// <summary>
/// Write configuration to the air interface modem.
/// </summary>
/// <returns></returns>
bool Modem::writeConfig()
{
    uint8_t buffer[25U];
    ::memset(buffer, 0x00U, 25U);
    uint8_t lengthToWrite = 17U;

    buffer[0U] = DVM_FRAME_START;
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

#if defined(ENABLE_DMR)
    if (m_dmrEnabled)
        buffer[4U] |= 0x02U;
#endif // defined(ENABLE_DMR)
#if defined(ENABLE_P25)
    if (m_p25Enabled)
        buffer[4U] |= 0x08U;
#endif // defined(ENABLE_P25)

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

    buffer[14U] = m_p25CorrCount;

    // are we on a protocol version 3 firmware?
    if (m_protoVer >= 3U) {
        lengthToWrite = 24U;

#if defined(ENABLE_NXDN)
        if (m_nxdnEnabled)
            buffer[4U] |= 0x10U;
#endif // defined(ENABLE_NXDN)

        buffer[18U] = (uint8_t)(m_nxdnTXLevel * 2.55F + 0.5F);

        buffer[19U] = m_rxCoarsePot;
        buffer[20U] = m_rxFinePot;
        buffer[21U] = m_txCoarsePot;
        buffer[22U] = m_txFinePot;
        buffer[23U] = m_rssiCoarsePot;
        buffer[24U] = m_rssiFinePot;
    }

    buffer[1U] = lengthToWrite;

#if DEBUG_MODEM
    Utils::dump(1U, "Modem::writeConfig(), Written", buffer, lengthToWrite);
#endif

    int ret = write(buffer, lengthToWrite);
    if (ret != lengthToWrite)
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
#if DEBUG_MODEM
    Utils::dump(1U, "Modem::writeConfig(), Response", m_buffer, m_length);
#endif
    if (resp == RTM_OK && m_buffer[2U] == CMD_NAK) {
        LogError(LOG_MODEM, "NAK, SET_CONFIG, command = 0x%02X, reason = %u", m_buffer[3U], m_buffer[4U]);

        switch (m_buffer[4U]) {
        case RSN_INVALID_FDMA_PREAMBLE:
            LogError(LOG_MODEM, "Invalid FDMA preamble");
            break;
        case RSN_INVALID_DMR_CC:
            LogError(LOG_MODEM, "Invalid DMR Color Code");
            break;
        case RSN_INVALID_DMR_RX_DELAY:
            LogError(LOG_MODEM, "Invalid DMR Rx Delay");
            break;
        case RSN_INVALID_P25_CORR_COUNT:
            LogError(LOG_MODEM, "Invalid P25 correlation count");
            break;
        case RSN_HS_NO_DUAL_MODE:
            LogError(LOG_MODEM, "Cannot multi-mode, DMR, P25 and NXDN when using hotspot!");
            break;
        case RSN_INVALID_REQUEST:
            LogError(LOG_MODEM, "Invalid SET_CONFIG request");
            break;
        default:
            break;
        }

        return false;
    }

    return true;
}

/// <summary>
/// Write symbol level adjustments to the air interface modem.
/// </summary>
/// <returns>True, if level adjustments are written, otherwise false.</returns>
bool Modem::writeSymbolAdjust()
{
    uint8_t buffer[20U];
    ::memset(buffer, 0x00U, 20U);
    uint8_t lengthToWrite = 7U;

    buffer[0U] = DVM_FRAME_START;
    buffer[2U] = CMD_SET_SYMLVLADJ;

    buffer[3U] = (uint8_t)(m_dmrSymLevel3Adj + 128);
    buffer[4U] = (uint8_t)(m_dmrSymLevel1Adj + 128);

    buffer[5U] = (uint8_t)(m_p25SymLevel3Adj + 128);
    buffer[6U] = (uint8_t)(m_p25SymLevel1Adj + 128);

    // are we on a protocol version 3 firmware?
    if (m_protoVer >= 3U) {
        lengthToWrite = 9U;

        buffer[7U] = (uint8_t)(m_nxdnSymLevel3Adj + 128);
        buffer[8U] = (uint8_t)(m_nxdnSymLevel1Adj + 128);
    }

    buffer[1U] = lengthToWrite;

    int ret = write(buffer, lengthToWrite);
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

    if (resp == RTM_OK && m_buffer[2U] == CMD_NAK) {
        LogError(LOG_MODEM, "NAK, SET_SYMLVLADJ, command = 0x%02X, reason = %u", m_buffer[3U], m_buffer[4U]);

        switch (m_buffer[4U]) {
        case RSN_INVALID_REQUEST:
            LogError(LOG_MODEM, "Invalid SET_SYMLVLADJ request");
            break;
        default:
            break;
        }

        return false;
    }

    return true;
}

/// <summary>
/// Write RF parameters to the air interface modem.
/// </summary>
/// <returns></returns>
bool Modem::writeRFParams()
{
    uint8_t buffer[22U];
    ::memset(buffer, 0x00U, 22U);
    uint8_t lengthToWrite = 18U;

    buffer[0U] = DVM_FRAME_START;
    buffer[2U] = CMD_SET_RFPARAMS;

    buffer[3U] = 0x00U;

    uint32_t rxActualFreq = m_rxFrequency + m_rxTuning;
    buffer[4U] = (rxActualFreq >> 0) & 0xFFU;
    buffer[5U] = (rxActualFreq >> 8) & 0xFFU;
    buffer[6U] = (rxActualFreq >> 16) & 0xFFU;
    buffer[7U] = (rxActualFreq >> 24) & 0xFFU;

    uint32_t txActualFreq = m_txFrequency + m_txTuning;
    buffer[8U] = (txActualFreq >> 0) & 0xFFU;
    buffer[9U] = (txActualFreq >> 8) & 0xFFU;
    buffer[10U] = (txActualFreq >> 16) & 0xFFU;
    buffer[11U] = (txActualFreq >> 24) & 0xFFU;

    buffer[12U] = (unsigned char)(m_rfPower * 2.55F + 0.5F);

    buffer[13U] = (uint8_t)(m_dmrDiscBWAdj + 128);
    buffer[14U] = (uint8_t)(m_p25DiscBWAdj + 128);
    buffer[15U] = (uint8_t)(m_dmrPostBWAdj + 128);
    buffer[16U] = (uint8_t)(m_p25PostBWAdj + 128);

    buffer[17U] = (uint8_t)m_adfGainMode;

    // are we on a protocol version 3 firmware?
    if (m_protoVer >= 3U) {
        lengthToWrite = 22U;

        buffer[18U] = (uint8_t)(m_nxdnDiscBWAdj + 128);
        buffer[19U] = (uint8_t)(m_nxdnPostBWAdj + 128);

        // support optional AFC parameters
        buffer[20U] = (m_afcEnable ? 0x80 : 0x00) +
            (m_afcKP << 4) + (m_afcKI);
        buffer[21U] = m_afcRange;
    }

    buffer[1U] = lengthToWrite;

    int ret = m_port->write(buffer, lengthToWrite);
    if (ret <= 0)
        return false;

    unsigned int count = 0U;
    RESP_TYPE_DVM resp;
    do {
        Thread::sleep(10U);

        resp = getResponse();
        if (resp == RTM_OK && m_buffer[2U] != RSN_OK && m_buffer[2U] != RSN_NAK) {
            count++;
            if (count >= MAX_RESPONSES) {
                LogError(LOG_MODEM, "No response, SET_RFPARAMS command");
                return false;
            }
        }
    } while (resp == RTM_OK && m_buffer[2U] != RSN_OK && m_buffer[2U] != RSN_NAK);

    // CUtils::dump(1U, "Response", m_buffer, m_length);

    if (resp == RTM_OK && m_buffer[2U] == RSN_NAK) {
        LogError(LOG_MODEM, "NAK, SET_RFPARAMS, command = 0x%02X, reason = %u", m_buffer[3U], m_buffer[4U]);

        switch (m_buffer[4U]) {
        case RSN_INVALID_REQUEST:
            LogError(LOG_MODEM, "Invalid SET_RFPARAMS request");
            break;
        default:
            break;
        }

        return false;
    }

    return true;
}

/// <summary>
/// Retrieve the data from the configuration area on the air interface modem.
/// </summary>
/// <returns></returns>
bool Modem::readFlash()
{
    Thread::sleep(2000U);    // 2s

    for (uint32_t i = 0U; i < 6U; i++) {
        uint8_t buffer[3U];

        buffer[0U] = DVM_FRAME_START;
        buffer[1U] = 3U;
        buffer[2U] = CMD_FLSH_READ;

        int ret = write(buffer, 3U);
        if (ret != 3)
            return false;

        for (uint32_t count = 0U; count < MAX_RESPONSES; count++) {
            Thread::sleep(10U);
            RESP_TYPE_DVM resp = getResponse(true);

            if (resp == RTM_ERROR)
                continue;

            if (resp == RTM_OK && m_buffer[2U] == CMD_NAK) {
                LogWarning(LOG_MODEM, "Modem::readFlash(), old modem that doesn't support flash commands?");
                return false;
            }

            if (resp == RTM_OK && m_buffer[2U] == CMD_FLSH_READ) {
                uint8_t len = m_buffer[1U];
                if (m_debug) {
                    Utils::dump(1U, "Modem Flash Contents", m_buffer + 3U, len - 3U);
                }

                if (len == 249U) {
                    bool ret = edac::CRC::checkCCITT162(m_buffer + 3U, DVM_CONF_AREA_LEN);
                    if (!ret) {
                        LogWarning(LOG_MODEM, "Modem configuration area does not contain a valid configuration!");
                    }
                    else {
                        bool isErased = (m_buffer[DVM_CONF_AREA_LEN] & 0x80U) == 0x80U;
                        uint8_t confAreaVersion = m_buffer[DVM_CONF_AREA_LEN] & 0x7FU;

                        if (!isErased) {
                            if (confAreaVersion != DVM_CONF_AREA_VER) {
                                LogError(LOG_MODEM, "Invalid version for configuration area, %02X != %02X", DVM_CONF_AREA_VER, confAreaVersion);
                            }
                            else {
                                processFlashConfig(m_buffer);
                            }
                        }
                        else {
                            LogWarning(LOG_MODEM, "Modem configuration area was erased and does not contain active configuration!");
                        }
                    }
                }
                else {
                    LogWarning(LOG_MODEM, "Incorrect length for configuration area! Ignoring.");
                }

                return true;
            }
        }

        Thread::sleep(1500U);
    }

    LogError(LOG_MODEM, "Unable to read the configuration flash after 6 attempts");

    return false;
}

/// <summary>
/// Process the configuration data from the air interface modem.
/// </summary>
/// <param name="buffer"></param>
void Modem::processFlashConfig(const uint8_t *buffer)
{
    if (m_ignoreModemConfigArea) {
        LogMessage(LOG_MODEM, "Modem configuration area checking is disabled!");
        return;
    }

    // general config
    bool rxInvert = (buffer[3U] & 0x01U) == 0x01U;
    FLASH_VALUE_CHECK(m_rxInvert, rxInvert, false, "rxInvert");
    bool txInvert = (buffer[3U] & 0x02U) == 0x02U;
    FLASH_VALUE_CHECK(m_txInvert, txInvert, false, "txInvert");
    bool pttInvert = (buffer[3U] & 0x04U) == 0x04U;
    FLASH_VALUE_CHECK(m_pttInvert, pttInvert, false, "pttInvert");

    bool dcBlocker = (buffer[4U] & 0x01U) == 0x01U;
    FLASH_VALUE_CHECK(m_dcBlocker, dcBlocker, true, "dcBlocker");

    uint8_t fdmaPreamble = buffer[5U];
    FLASH_VALUE_CHECK(m_fdmaPreamble, fdmaPreamble, 80U, "fdmaPreamble");

    // levels
    float rxLevel = (float(buffer[7U]) - 0.5F) / 2.55F;
    FLASH_VALUE_CHECK_FLOAT(m_rxLevel, rxLevel, 50.0F, "rxLevel");

    float txLevel = (float(buffer[8U]) - 0.5F) / 2.55F;
    FLASH_VALUE_CHECK_FLOAT(m_cwIdTXLevel, txLevel, 50.0F, "cwIdTxLevel");
    FLASH_VALUE_CHECK_FLOAT(m_dmrTXLevel, txLevel, 50.0F, "dmrTxLevel");
    FLASH_VALUE_CHECK_FLOAT(m_p25TXLevel, txLevel, 50.0F, "p25TxLevel");
    FLASH_VALUE_CHECK_FLOAT(m_nxdnTXLevel, txLevel, 50.0F, "nxdnTxLevel");

    uint8_t dmrRxDelay = buffer[10U];
    FLASH_VALUE_CHECK(m_dmrRxDelay, dmrRxDelay, 7U, "dmrRxDelay");

    uint8_t p25CorrCount = buffer[11U];
    FLASH_VALUE_CHECK(m_p25CorrCount, p25CorrCount, 8U, "p25CorrCount");

    int txDCOffset = int(buffer[16U]) - 128;
    FLASH_VALUE_CHECK(m_txDCOffset, txDCOffset, 0, "txDCOffset");

    int rxDCOffset = int(buffer[17U]) - 128;
    FLASH_VALUE_CHECK(m_rxDCOffset, rxDCOffset, 0, "rxDCOffset");

    // RF parameters
    int8_t dmrDiscBWAdj = int8_t(buffer[20U]) - 128;
    FLASH_VALUE_CHECK(m_dmrDiscBWAdj, dmrDiscBWAdj, 0, "dmrDiscBWAdj");
    int8_t p25DiscBWAdj = int8_t(buffer[21U]) - 128;
    FLASH_VALUE_CHECK(m_p25DiscBWAdj, p25DiscBWAdj, 0, "p25DiscBWAdj");
    int8_t dmrPostBWAdj = int8_t(buffer[22U]) - 128;
    FLASH_VALUE_CHECK(m_dmrPostBWAdj, dmrPostBWAdj, 0, "dmrPostBWAdj");
    int8_t p25PostBWAdj = int8_t(buffer[23U]) - 128;
    FLASH_VALUE_CHECK(m_p25PostBWAdj, p25PostBWAdj, 0, "p25PostBWAdj");

    // are we on a protocol version 3 firmware?
    if (m_protoVer >= 3U) {
        int8_t nxdnDiscBWAdj = int8_t(buffer[39U]) - 128;
        FLASH_VALUE_CHECK(m_nxdnDiscBWAdj, nxdnDiscBWAdj, 0, "nxdnDiscBWAdj");
        int8_t nxdnPostBWAdj = int8_t(buffer[40U]) - 128;
        FLASH_VALUE_CHECK(m_nxdnPostBWAdj, nxdnPostBWAdj, 0, "nxdnPostBWAdj");
    }

    ADF_GAIN_MODE adfGainMode = (ADF_GAIN_MODE)buffer[24U];
    FLASH_VALUE_CHECK(m_adfGainMode, adfGainMode, ADF_GAIN_AUTO, "adfGainMode");

    uint32_t txTuningRaw = __GET_UINT32(buffer, 25U);
    int txTuning = int(txTuningRaw);
    FLASH_VALUE_CHECK(m_txTuning, txTuning, 0, "txTuning");
    uint32_t rxTuningRaw = __GET_UINT32(buffer, 29U);
    int rxTuning = int(rxTuningRaw);
    FLASH_VALUE_CHECK(m_rxTuning, rxTuning, 0, "rxTuning");

    // symbol adjust
    int dmrSymLevel3Adj = int(buffer[35U]) - 128;
    FLASH_VALUE_CHECK(m_dmrSymLevel3Adj, dmrSymLevel3Adj, 0, "dmrSymLevel3Adj");
    int dmrSymLevel1Adj = int(buffer[36U]) - 128;
    FLASH_VALUE_CHECK(m_dmrSymLevel1Adj, dmrSymLevel1Adj, 0, "dmrSymLevel1Adj");

    int p25SymLevel3Adj = int(buffer[37U]) - 128;
    FLASH_VALUE_CHECK(m_p25SymLevel3Adj, p25SymLevel3Adj, 0, "p25SymLevel3Adj");
    int p25SymLevel1Adj = int(buffer[38U]) - 128;
    FLASH_VALUE_CHECK(m_p25SymLevel1Adj, p25SymLevel1Adj, 0, "p25SymLevel1Adj");

    // are we on a protocol version 3 firmware?
    if (m_protoVer >= 3U) {
        int nxdnSymLevel3Adj = int(buffer[41U]) - 128;
        FLASH_VALUE_CHECK(m_nxdnSymLevel3Adj, nxdnSymLevel3Adj, 0, "nxdnSymLevel3Adj");
        int nxdnSymLevel1Adj = int(buffer[42U]) - 128;
        FLASH_VALUE_CHECK(m_nxdnSymLevel1Adj, nxdnSymLevel1Adj, 0, "nxdnSymLevel1Adj");
    }

    // are we on a protocol version 3 firmware?
    if (m_protoVer >= 3U) {
        uint8_t rxCoarse = buffer[43U];
        FLASH_VALUE_CHECK(m_rxCoarsePot, rxCoarse, 7U, "rxCoarse");
        uint8_t rxFine = buffer[44U];
        FLASH_VALUE_CHECK(m_rxFinePot, rxFine, 7U, "rxFine");

        uint8_t txCoarse = buffer[45U];
        FLASH_VALUE_CHECK(m_txCoarsePot, txCoarse, 7U, "txCoarse");
        uint8_t txFine = buffer[46U];
        FLASH_VALUE_CHECK(m_txFinePot, txFine, 7U, "txFine");

        uint8_t rssiCoarse = buffer[47U];
        FLASH_VALUE_CHECK(m_rssiCoarsePot, rssiCoarse, 7U, "rssiCoarse");
        uint8_t rssiFine = buffer[48U];
        FLASH_VALUE_CHECK(m_rssiFinePot, rssiFine, 7U, "rssiFine");
    }
}

/// <summary>
/// Print debug air interface messages to the host log.
/// </summary>
/// <param name="buffer"></param>
/// <param name="len"></param>
void Modem::printDebug(const uint8_t* buffer, uint16_t len)
{
    if (m_rspDoubleLength && buffer[3U] == CMD_DEBUG_DUMP) {
        uint8_t data[512U];
        ::memset(data, 0x00U, 512U);
        ::memcpy(data, buffer, len);

        Utils::dump(1U, "Modem Debug Dump", data, len);
        return;
    }
    else {
        if (m_rspDoubleLength) {
            LogError(LOG_MODEM, "Invalid debug data received from the modem, len = %u", len);
            return;
        }
    }

    if (buffer[2U] == CMD_DEBUG1) {
        LogDebug(LOG_MODEM, "M: %.*s", len - 3U, buffer + 3U);
    }
    else if (buffer[2U] == CMD_DEBUG2) {
        short val1 = (buffer[len - 2U] << 8) | buffer[len - 1U];
        LogDebug(LOG_MODEM, "M: %.*s %X", len - 5U, buffer + 3U, val1);
    }
    else if (buffer[2U] == CMD_DEBUG3) {
        short val1 = (buffer[len - 4U] << 8) | buffer[len - 3U];
        short val2 = (buffer[len - 2U] << 8) | buffer[len - 1U];
        LogDebug(LOG_MODEM, "M: %.*s %X %X", len - 7U, buffer + 3U, val1, val2);
    }
    else if (buffer[2U] == CMD_DEBUG4) {
        short val1 = (buffer[len - 6U] << 8) | buffer[len - 5U];
        short val2 = (buffer[len - 4U] << 8) | buffer[len - 3U];
        short val3 = (buffer[len - 2U] << 8) | buffer[len - 1U];
        LogDebug(LOG_MODEM, "M: %.*s %X %X %X", len - 9U, buffer + 3U, val1, val2, val3);
    }
    else if (buffer[2U] == CMD_DEBUG5) {
        short val1 = (buffer[len - 8U] << 8) | buffer[len - 7U];
        short val2 = (buffer[len - 6U] << 8) | buffer[len - 5U];
        short val3 = (buffer[len - 4U] << 8) | buffer[len - 3U];
        short val4 = (buffer[len - 2U] << 8) | buffer[len - 1U];
        LogDebug(LOG_MODEM, "M: %.*s %X %X %X %X", len - 11U, buffer + 3U, val1, val2, val3, val4);
    }
    else if (buffer[2U] == CMD_DEBUG_DUMP) {
        uint8_t data[255U];
        ::memset(data, 0x00U, 255U);
        ::memcpy(data, buffer, len);

        Utils::dump(1U, "Modem Debug Dump", data, len);
    }
}

/// <summary>
/// Helper to get the raw response packet from modem.
/// </summary>
/// <param name="noReportInvalid">Ignores invalid frame start and does not report as error.</param>
/// <returns>Response type from modem.</returns>
RESP_TYPE_DVM Modem::getResponse(bool noReportInvalid)
{
    m_rspDoubleLength = false;

    //LogDebug(LOG_MODEM, "getResponse(), checking if we have data");

    // get the start of the frame or nothing at all
    if (m_rspState == RESP_START) {
        int ret = m_port->read(m_buffer + 0U, 1U);
        if (ret < 0) {
            LogError(LOG_MODEM, "Error reading from the modem, ret = %d", ret);
            m_rspState = RESP_START;
            return RTM_ERROR;
        }

        if (ret == 0) {
            //LogDebug(LOG_MODEM, "getResponse(), no data available");
            return RTM_TIMEOUT;
        }

        if (m_buffer[0U] != DVM_FRAME_START) {
            if (!noReportInvalid) {
                LogError(LOG_MODEM, "Modem::getResponse(), illegal response, first byte not a frame start; byte = %02X", m_buffer[0U]);
                Utils::dump(1U, "Modem Invalid Frame", m_buffer, 250U);
            }

            ::memset(m_buffer, 0x00U, BUFFER_LENGTH);
            return RTM_ERROR;
        }

        //LogDebug(LOG_MODEM, "getResponse(), RESP_START");

        m_rspState = RESP_LENGTH1;
    }

    //LogDebug(LOG_MODEM, "getResponse(), getting frame length 1/2");
    // get the length of the frame, 1/2
    if (m_rspState == RESP_LENGTH1) {
        int ret = m_port->read(m_buffer + 1U, 1U);
        if (ret < 0) {
            LogError(LOG_MODEM, "Error reading from the modem, ret = %d", ret);
            m_rspState = RESP_START;
            return RTM_ERROR;
        }

        if (ret == 0)
            return RTM_TIMEOUT;

        if (m_buffer[1U] >= 250U) {
            LogError(LOG_MODEM, "Invalid length received from the modem, len = %u", m_buffer[1U]);
            return RTM_ERROR;
        }

        m_length = m_buffer[1U];

        if (m_length == 0U)
            m_rspState = RESP_LENGTH2;
        else
            m_rspState = RESP_TYPE;

        //LogDebug(LOG_MODEM, "getResponse(), RESP_LENGTH1, len = %u", m_length);

        m_rspDoubleLength = false;
        m_rspOffset = 2U;
    }

    //LogDebug(LOG_MODEM, "getResponse(), getting frame length 2/2");
    // get the length of the frame, 2/2
    if (m_rspState == RESP_LENGTH2) {
        int ret = m_port->read(m_buffer + 2U, 1U);
        if (ret < 0) {
            LogError(LOG_MODEM, "Error reading from the modem, ret = %d", ret);
            m_rspState = RESP_START;
            return RTM_ERROR;
        }

        if (ret == 0)
            return RTM_TIMEOUT;

        m_length = m_buffer[2U] + 255U;
        m_rspState = RESP_TYPE;

        //LogDebug(LOG_MODEM, "getResponse(), RESP_LENGTH2, len = %u", m_length);

        m_rspDoubleLength = true;
        m_rspOffset = 3U;
    }

    //LogDebug(LOG_MODEM, "getResponse(), getting frame type");
    // get the frame type
    if (m_rspState == RESP_TYPE) {
        int ret = m_port->read(m_buffer + m_rspOffset, 1U);
        if (ret < 0) {
            LogError(LOG_MODEM, "Error reading from the modem, ret = %d", ret);
            m_rspState = RESP_START;
            return RTM_ERROR;
        }

        if (ret == 0)
            return RTM_TIMEOUT;

        m_rspType = (DVM_COMMANDS)m_buffer[m_rspOffset];

        //LogDebug(LOG_MODEM, "getResponse(), RESP_TYPE, len = %u, type = %u", m_length, m_rspType);

        m_rspState = RESP_DATA;
        m_rspOffset++;
    }

    //LogDebug(LOG_MODEM, "getResponse(), getting frame data");

    // get the frame data
    if (m_rspState == RESP_DATA) {
        if (m_debug && m_trace)
            LogDebug(LOG_MODEM, "getResponse(), RESP_DATA, len = %u, offset = %u, type = %02X", m_length, m_rspOffset, m_rspType);

        while (m_rspOffset < m_length) {
            int ret = m_port->read(m_buffer + m_rspOffset, m_length - m_rspOffset);
            if (ret < 0) {
                LogError(LOG_MODEM, "Error reading from the modem, ret = %d", ret);
                m_rspState = RESP_START;
                return RTM_ERROR;
            }

            if (ret == 0)
                return RTM_TIMEOUT;

            if (ret > 0)
                m_rspOffset += ret;
        }

        if (m_debug && m_trace)
            Utils::dump(1U, "Modem getResponse()", m_buffer, m_length);
    }

    m_rspState = RESP_START;
    m_rspOffset = 0U;

    return RTM_OK;
}

