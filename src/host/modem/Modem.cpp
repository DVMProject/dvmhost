// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2011-2021 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
 *  Copyright (C) 2021 Nat Moore
 *
 */
#include "Defines.h"
#include "common/dmr/DMRDefines.h"
#include "common/p25/P25Defines.h"
#include "common/nxdn/NXDNDefines.h"
#include "common/edac/CRC.h"
#include "common/Log.h"
#include "common/Thread.h"
#include "common/Utils.h"
#include "modem/Modem.h"

using namespace modem;

#include <cassert>

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

/* Initializes a new instance of the Modem class. */

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
    m_forceHotspot(false),
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
    m_dmrFifoLength(DMR_TX_BUFFER_LEN),
    m_p25FifoLength(P25_TX_BUFFER_LEN),
    m_nxdnFifoLength(NXDN_TX_BUFFER_LEN),
    m_adcOverFlowCount(0U),
    m_dacOverFlowCount(0U),
    m_v24Connected(true),
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
    m_rxDMRQueue1(dmrQueueSize, "Modem RX DMR1"),
    m_rxDMRQueue2(dmrQueueSize, "Modem RX DMR2"),
    m_rxP25Queue(p25QueueSize, "Modem RX P25"),
    m_rxNXDNQueue(nxdnQueueSize, "Modem RX NXDN"),
    m_statusTimer(1000U, 0U, MODEM_POLL_TIME),
    m_inactivityTimer(1000U, 8U),
    m_dmrSpace1(0U),
    m_dmrSpace2(0U),
    m_p25Space(0U),
    m_nxdnSpace(0U),
    m_tx(false),
    m_cd(false),
    m_lockout(false),
    m_error(false),
    m_dmr1ReadLock(),
    m_dmr2ReadLock(),
    m_p25ReadLock(),
    m_nxdnReadLock(),
    m_ignoreModemConfigArea(ignoreModemConfigArea),
    m_flashDisabled(false),
    m_gotModemStatus(false),
    m_dumpModemStatus(dumpModemStatus),
    m_respTrace(false),
    m_trace(trace),
    m_debug(debug)
{
    assert(port != nullptr);

    m_buffer = new uint8_t[BUFFER_LENGTH];
}

/* Finalizes a instance of the Modem class. */

Modem::~Modem()
{
    delete m_port;
    delete[] m_buffer;
}

/* Sets the RF DC offset parameters. */

void Modem::setDCOffsetParams(int txDCOffset, int rxDCOffset)
{
    m_txDCOffset = txDCOffset;
    m_rxDCOffset = rxDCOffset;
}

/* Sets the enabled modes. */

void Modem::setModeParams(bool dmrEnabled, bool p25Enabled, bool nxdnEnabled)
{
    m_dmrEnabled = dmrEnabled;
    m_p25Enabled = p25Enabled;
    m_nxdnEnabled = nxdnEnabled;
}

/* Sets the RF deviation levels. */

void Modem::setLevels(float rxLevel, float cwIdTXLevel, float dmrTXLevel, float p25TXLevel, float nxdnTXLevel)
{
    m_rxLevel = rxLevel;
    m_cwIdTXLevel = cwIdTXLevel;
    m_dmrTXLevel = dmrTXLevel;
    m_p25TXLevel = p25TXLevel;
    m_nxdnTXLevel = nxdnTXLevel;
}

/* Sets the symbol adjustment levels */

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

/* Sets the RF parameters. */

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

/* Sets the softpot parameters. */

void Modem::setSoftPot(uint8_t rxCoarse, uint8_t rxFine, uint8_t txCoarse, uint8_t txFine, uint8_t rssiCoarse, uint8_t rssiFine)
{
    m_rxCoarsePot = rxCoarse;
    m_rxFinePot = rxFine;

    m_txCoarsePot = txCoarse;
    m_txFinePot = txFine;

    m_rssiCoarsePot = rssiCoarse;
    m_rssiFinePot = rssiFine;
}

/* Sets the DMR color code. */

void Modem::setDMRColorCode(uint32_t colorCode)
{
    assert(colorCode < 16U);

    m_dmrColorCode = colorCode;
}

/* Sets the P25 NAC. */

void Modem::setP25NAC(uint32_t nac)
{
    assert(nac < 0xFFFU);

    m_p25NAC = nac;
}

/* Sets the RF receive deviation levels. */

void Modem::setRXLevel(float rxLevel)
{
    m_rxLevel = rxLevel;

    uint8_t buffer[4U];

    buffer[0U] = DVM_SHORT_FRAME_START;
    buffer[1U] = 4U;
    buffer[2U] = CMD_SET_RXLEVEL;

    buffer[3U] = (uint8_t)(m_rxLevel * 2.55F + 0.5F);
#if DEBUG_MODEM
    Utils::dump(1U, "Modem::setRXLevel(), Written", buffer, 4U);
#endif
    int ret = write(buffer, 4U);
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
                LogError(LOG_MODEM, "No response, %s command", cmdToString(CMD_SET_RXLEVEL).c_str());
                return;
            }
        }
    } while (resp == RTM_OK && m_buffer[2U] != CMD_ACK && m_buffer[2U] != CMD_NAK);
#if DEBUG_MODEM
    Utils::dump(1U, "Modem::setRXLevel(), Response", m_buffer, m_length);
#endif
    if (resp == RTM_OK && m_buffer[2U] == CMD_NAK) {
        LogError(LOG_MODEM, "NAK, %s, command = 0x%02X, reason = %u", cmdToString(CMD_SET_RXLEVEL).c_str(), m_buffer[3U], m_buffer[4U]);
    }
}

/* Sets the modem transmit FIFO buffer lengths. */

void Modem::setFifoLength(uint16_t dmrLength, uint16_t p25Length, uint16_t nxdnLength)
{
    m_dmrFifoLength = dmrLength;
    m_p25FifoLength = p25Length;
    m_nxdnFifoLength = nxdnLength;

    // ensure DMR fifo length is not less then the minimum
    if (m_dmrFifoLength < DMR_TX_BUFFER_LEN)
        m_dmrFifoLength = DMR_TX_BUFFER_LEN;
    
    // ensure P25 fifo length is not less then the minimum
    if (m_p25FifoLength < P25_TX_BUFFER_LEN)
        m_p25FifoLength = P25_TX_BUFFER_LEN;
    
    // ensure NXDN fifo length is not less then the minimum
    if (m_nxdnFifoLength < NXDN_TX_BUFFER_LEN)
        m_nxdnFifoLength = NXDN_TX_BUFFER_LEN;

    if (!m_dmrEnabled && m_dmrFifoLength > 0U)
        m_dmrFifoLength = 0U;
    if (!m_p25Enabled && m_p25FifoLength > 0U)
        m_p25FifoLength = 0U;
    if (!m_nxdnEnabled && m_nxdnFifoLength > 0U)
        m_nxdnFifoLength = 0U;

    uint8_t buffer[9U];

    buffer[0U] = DVM_SHORT_FRAME_START;
    buffer[1U] = 9U;
    buffer[2U] = CMD_SET_BUFFERS;

    buffer[3U] = (uint8_t)(m_dmrFifoLength >> 8) & 0xFFU;
    buffer[4U] = (uint8_t)(m_dmrFifoLength & 0xFFU);
    buffer[5U] = (uint8_t)(m_p25FifoLength >> 8) & 0xFFU;
    buffer[6U] = (uint8_t)(m_p25FifoLength & 0xFFU);
    buffer[7U] = (uint8_t)(m_nxdnFifoLength >> 8) & 0xFFU;
    buffer[8U] = (uint8_t)(m_nxdnFifoLength & 0xFFU);

#if DEBUG_MODEM
    Utils::dump(1U, "Modem::setFifoLength(), Written", buffer, 9U);
#endif
    int ret = write(buffer, 9U);
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
                LogError(LOG_MODEM, "No response, %s command", cmdToString(CMD_SET_BUFFERS).c_str());
                return;
            }
        }
    } while (resp == RTM_OK && m_buffer[2U] != CMD_ACK && m_buffer[2U] != CMD_NAK);
#if DEBUG_MODEM
    Utils::dump(1U, "Modem::setFifoLength(), Response", m_buffer, m_length);
#endif
    if (resp == RTM_OK && m_buffer[2U] == CMD_NAK) {
        LogError(LOG_MODEM, "NAK, %s, command = 0x%02X, reason = %u", cmdToString(CMD_SET_BUFFERS).c_str(), m_buffer[3U], m_buffer[4U]);
    }
}

/* Sets a custom modem response handler. */
/* If the response handler returns true, processing will stop, otherwise it will continue. */

void Modem::setResponseHandler(std::function<MODEM_RESP_HANDLER> handler)
{
    assert(handler != nullptr);

    m_rspHandler = handler;
}

/* Sets a custom modem open port handler. */
/* If the open handler is set, it is the responsibility of the handler to complete air interface 
   initialization (i.e. write configuration, etc). */

void Modem::setOpenHandler(std::function<MODEM_OC_PORT_HANDLER> handler)
{
    assert(handler != nullptr);

    m_openPortHandler = handler;
}

/* Sets a custom modem close port handler. */

void Modem::setCloseHandler(std::function<MODEM_OC_PORT_HANDLER> handler)
{
    assert(handler != nullptr);

    m_closePortHandler = handler;
}

/* Opens connection to the air interface modem. */

bool Modem::open()
{
    LogMessage(LOG_MODEM, "Initializing modem");
    m_gotModemStatus = false;

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

/* Updates the timer by the passed number of milliseconds. */

void Modem::clock(uint32_t ms)
{
    // poll the modem status
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
        uint8_t cmdOffset = 2U;
        if (m_rspDoubleLength) {
            cmdOffset = 3U;
        }

        switch (m_buffer[cmdOffset]) {
        /** Digital Mobile Radio */
        case CMD_DMR_DATA1:
        {
            if (m_dmrEnabled) {
                std::lock_guard<std::mutex> lock(m_dmr1ReadLock);

                if (m_rspDoubleLength) {
                    LogError(LOG_MODEM, "CMD_DMR_DATA1 double length?; len = %u", m_length);
                    break;
                }

                uint8_t data = m_length - 2U;
                m_rxDMRQueue1.addData(&data, 1U);

                if (m_buffer[3U] == (DMRDEF::SYNC_DATA | DMRDEF::DataType::TERMINATOR_WITH_LC))
                    data = TAG_EOT;
                else
                    data = TAG_DATA;
                m_rxDMRQueue1.addData(&data, 1U);

                m_rxDMRQueue1.addData(m_buffer + 3U, m_length - 3U);
                if (m_trace)
                    Utils::dump(1U, "Modem::clock() RX DMR Data 1", m_buffer + 3U, m_length - 3U);
            }
        }
        break;

        case CMD_DMR_DATA2:
        {
            if (m_dmrEnabled) {
                std::lock_guard<std::mutex> lock(m_dmr2ReadLock);

                if (m_rspDoubleLength) {
                    LogError(LOG_MODEM, "CMD_DMR_DATA2 double length?; len = %u", m_length);
                    break;
                }

                uint8_t data = m_length - 2U;
                m_rxDMRQueue2.addData(&data, 1U);

                if (m_buffer[3U] == (DMRDEF::SYNC_DATA | DMRDEF::DataType::TERMINATOR_WITH_LC))
                    data = TAG_EOT;
                else
                    data = TAG_DATA;
                m_rxDMRQueue2.addData(&data, 1U);

                m_rxDMRQueue2.addData(m_buffer + 3U, m_length - 3U);
                if (m_trace)
                    Utils::dump(1U, "Modem::clock() RX DMR Data 2", m_buffer + 3U, m_length - 3U);
            }
        }
        break;

        case CMD_DMR_LOST1:
        {
            if (m_dmrEnabled) {
                std::lock_guard<std::mutex> lock(m_dmr1ReadLock);

                if (m_rspDoubleLength) {
                    LogError(LOG_MODEM, "CMD_DMR_LOST1 double length?; len = %u", m_length);
                    break;
                }

                uint8_t data = 1U;
                m_rxDMRQueue1.addData(&data, 1U);

                data = TAG_LOST;
                m_rxDMRQueue1.addData(&data, 1U);
            }
        }
        break;

        case CMD_DMR_LOST2:
        {
            if (m_dmrEnabled) {
                std::lock_guard<std::mutex> lock(m_dmr2ReadLock);

                if (m_rspDoubleLength) {
                    LogError(LOG_MODEM, "CMD_DMR_LOST2 double length?; len = %u", m_length);
                    break;
                }

                uint8_t data = 1U;
                m_rxDMRQueue2.addData(&data, 1U);

                data = TAG_LOST;
                m_rxDMRQueue2.addData(&data, 1U);
            }
        }
        break;

        /** Project 25 */
        case CMD_P25_DATA:
        {
            if (m_p25Enabled) {
                std::lock_guard<std::mutex> lock(m_p25ReadLock);

                uint8_t length[2U];
                if (m_length > 255U)
                    length[0U] = ((m_length - cmdOffset) >> 8U) & 0xFFU;
                else
                    length[0U] = 0x00U;
                length[1U] = (m_length - cmdOffset) & 0xFFU;
                m_rxP25Queue.addData(length, 2U);

                uint8_t data = TAG_DATA;
                m_rxP25Queue.addData(&data, 1U);

                m_rxP25Queue.addData(m_buffer + (cmdOffset + 1U), m_length - (cmdOffset + 1U));
                if (m_trace)
                    Utils::dump(1U, "Modem::clock() RX P25 Data", m_buffer + (cmdOffset + 1U), m_length - (cmdOffset + 1U));
            }
        }
        break;

        case CMD_P25_LOST:
        {
            if (m_p25Enabled) {
                std::lock_guard<std::mutex> lock(m_p25ReadLock);

                if (m_rspDoubleLength) {
                    LogError(LOG_MODEM, "CMD_P25_LOST double length?; len = %u", m_length);
                    break;
                }

                uint8_t data = 1U;
                m_rxP25Queue.addData(&data, 1U);

                data = TAG_LOST;
                m_rxP25Queue.addData(&data, 1U);
            }
        }
        break;

        /** Next Generation Digital Narrowband */
        case CMD_NXDN_DATA:
        {
            if (m_nxdnEnabled) {
                std::lock_guard<std::mutex> lock(m_nxdnReadLock);

                if (m_rspDoubleLength) {
                    LogError(LOG_MODEM, "CMD_NXDN_DATA double length?; len = %u", m_length);
                    break;
                }

                uint8_t data = m_length - 2U;
                m_rxNXDNQueue.addData(&data, 1U);

                data = TAG_DATA;
                m_rxNXDNQueue.addData(&data, 1U);

                m_rxNXDNQueue.addData(m_buffer + 3U, m_length - 3U);
                if (m_trace)
                    Utils::dump(1U, "Modem::clock() RX NXDN Data", m_buffer + 3U, m_length - 3U);
            }
        }
        break;

        case CMD_NXDN_LOST:
        {
            if (m_nxdnEnabled) {
                std::lock_guard<std::mutex> lock(m_nxdnReadLock);

                if (m_rspDoubleLength) {
                    LogError(LOG_MODEM, "CMD_NXDN_LOST double length?; len = %u", m_length);
                    break;
                }

                uint8_t data = 1U;
                m_rxNXDNQueue.addData(&data, 1U);

                data = TAG_LOST;
                m_rxNXDNQueue.addData(&data, 1U);
            }
        }
        break;

        /** General */
        case CMD_GET_STATUS:
        {
            m_isHotspot = (m_buffer[3U] & 0x01U) == 0x01U;

            // override hotspot flag if we're forcing hotspot
            if (m_forceHotspot) {
                m_isHotspot = m_forceHotspot;
            }

            bool dmrEnable = (m_buffer[3U] & 0x02U) == 0x02U;
            bool p25Enable = (m_buffer[3U] & 0x08U) == 0x08U;
            bool nxdnEnable = (m_buffer[3U] & 0x10U) == 0x10U;

            // flag indicating if free space is being reported in 16-byte blocks instead of LDUs
            bool spaceInBlocks = (m_buffer[3U] & 0x80U) == 0x80U;

            m_v24Connected = true;
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

            // spaces from the modem are returned in "logical" frame count, or a block size, not raw byte size
            // for DMR and NXDN, becuase the protocols use fixed length frames we always return
            // space in frame count
            m_dmrSpace1 = m_buffer[7U] * (DMRDEF::DMR_FRAME_LENGTH_BYTES + 2U);
            m_dmrSpace2 = m_buffer[8U] * (DMRDEF::DMR_FRAME_LENGTH_BYTES + 2U);
            m_nxdnSpace = m_buffer[11U] * (NXDDEF::NXDN_FRAME_LENGTH_BYTES);

            // P25 free space can be reported as 16-byte blocks or frames based on the flag above
            if (spaceInBlocks)
                m_p25Space = m_buffer[10U] * P25_BUFFER_BLOCK_SIZE;
            else
                m_p25Space = m_buffer[10U] * (P25DEF::P25_LDU_FRAME_LENGTH_BYTES);

            if (m_dumpModemStatus) {
                LogDebug(LOG_MODEM, "Modem::clock(), CMD_GET_STATUS, isHotspot = %u, dmr = %u / %u, p25 = %u / %u, nxdn = %u / %u, modemState = %u, tx = %u, adcOverflow = %u, rxOverflow = %u, txOverflow = %u, dacOverflow = %u, dmrSpace1 = %u, dmrSpace2 = %u, p25Space = %u, nxdnSpace = %u",
                    m_isHotspot, dmrEnable, m_dmrEnabled, p25Enable, m_p25Enabled, nxdnEnable, m_nxdnEnabled, m_modemState, m_tx, adcOverflow, rxOverflow, txOverflow, dacOverflow, m_dmrSpace1, m_dmrSpace2, m_p25Space, m_nxdnSpace);
                LogDebug(LOG_MODEM, "Modem::clock(), CMD_GET_STATUS, rxDMRData1 size = %u, len = %u, free = %u; rxDMRData2 size = %u, len = %u, free = %u, rxP25Data size = %u, len = %u, free = %u, rxNXDNData size = %u, len = %u, free = %u",
                    m_rxDMRQueue1.length(), m_rxDMRQueue1.dataSize(), m_rxDMRQueue1.freeSpace(), m_rxDMRQueue2.length(), m_rxDMRQueue2.dataSize(), m_rxDMRQueue2.freeSpace(),
                    m_rxP25Queue.length(), m_rxP25Queue.dataSize(), m_rxP25Queue.freeSpace(), m_rxNXDNQueue.length(), m_rxNXDNQueue.dataSize(), m_rxNXDNQueue.freeSpace());
            }

            m_gotModemStatus = true;
            m_inactivityTimer.start();
        }
        break;

        case CMD_GET_VERSION:
        case CMD_ACK:
            break;

        case CMD_NAK:
        {
            LogWarning(LOG_MODEM, "NAK, command = 0x%02X (%s), reason = %u (%s)", m_buffer[3U], cmdToString(m_buffer[3U]).c_str(), m_buffer[4U], rsnToString(m_buffer[4U]).c_str());
            switch (m_buffer[4U]) {
                case RSN_RINGBUFF_FULL:
                {
                    switch (m_buffer[3U]) {
                        case CMD_DMR_DATA1:
                            LogWarning(LOG_MODEM, "NAK, %s, dmrSpace1 = %u", rsnToString(m_buffer[4U]).c_str(), m_dmrSpace1);
                            break;
                        case CMD_DMR_DATA2:
                            LogWarning(LOG_MODEM, "NAK, %s, dmrSpace2 = %u", rsnToString(m_buffer[4U]).c_str(), m_dmrSpace2);
                            break;

                        case CMD_P25_DATA:
                            LogWarning(LOG_MODEM, "NAK, %s, p25Space = %u", rsnToString(m_buffer[4U]).c_str(), m_p25Space);
                            break;

                        case CMD_NXDN_DATA:
                            LogWarning(LOG_MODEM, "NAK, %s, nxdnSpace = %u", rsnToString(m_buffer[4U]).c_str(), m_nxdnSpace);
                            break;
                        }
                }
                break;
            }
        }
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
            if (m_rspState != RESP_START)
                m_rspState = RESP_START;
            break;
        }
    }

    // force a modem reset because of a error condition
    if (forceModemReset) {
        forceModemReset = false;
        reset();
    }
}

/* Closes connection to the air interface modem. */

void Modem::close()
{
    LogDebug(LOG_MODEM, "Closing the modem");
    m_port->close();

    m_gotModemStatus = false;

    // do we have a close port handler?
    if (m_closePortHandler != nullptr) {
        m_closePortHandler(this);
    }
}

/* Get the frame data length for the next frame in the DMR Slot 1 ring buffer. */

uint32_t Modem::peekDMRFrame1Length()
{
    if (m_rxDMRQueue1.isEmpty())
        return 0U;

    uint8_t len = 0U;
    m_rxDMRQueue1.peek(&len, 1U);
#if DEBUG_MODEM
    LogDebug(LOG_MODEM, "Modem::peekDMRFrame1Length() len = %u, dataSize = %u", len, m_rxDMRQueue1.dataSize());
#endif
    // this ensures we never get in a situation where we have length stuck on the queue
    if (m_rxDMRQueue1.dataSize() == 1U && len > m_rxDMRQueue1.dataSize()) {
        m_rxDMRQueue1.get(&len, 1U); // ensure we pop the length off
        return 0U;
    }

    if (m_rxDMRQueue1.dataSize() >= len) {
        return len;
    }

    return 0U;
}

/* Reads DMR Slot 1 frame data from the DMR Slot 1 ring buffer. */

uint32_t Modem::readDMRFrame1(uint8_t* data)
{
    assert(data != nullptr);
    std::lock_guard<std::mutex> lock(m_dmr1ReadLock);

    if (m_rxDMRQueue1.isEmpty())
        return 0U;

    uint8_t len = 0U;
    m_rxDMRQueue1.peek(&len, 1U);

    // this ensures we never get in a situation where we have length stuck on the queue
    if (m_rxDMRQueue1.dataSize() == 1U && len > m_rxDMRQueue1.dataSize()) {
        m_rxDMRQueue1.get(&len, 1U); // ensure we pop the length off
        return 0U;
    }

    if (m_rxDMRQueue1.dataSize() >= len) {
        m_rxDMRQueue1.get(&len, 1U); // ensure we pop the length off
        m_rxDMRQueue1.get(data, len);
    
        return len;
    }

    return 0U;
}

/* Get the frame data length for the next frame in the DMR Slot 2 ring buffer. */

uint32_t Modem::peekDMRFrame2Length()
{
    if (m_rxDMRQueue2.isEmpty())
        return 0U;

    uint8_t len = 0U;
    m_rxDMRQueue2.peek(&len, 1U);
#if DEBUG_MODEM
    LogDebug(LOG_MODEM, "Modem::peekDMRFrame2Length() len = %u, dataSize = %u", len, m_rxDMRQueue2.dataSize());
#endif
    // this ensures we never get in a situation where we have length stuck on the queue
    if (m_rxDMRQueue2.dataSize() == 1U && len > m_rxDMRQueue2.dataSize()) {
        m_rxDMRQueue2.get(&len, 1U); // ensure we pop the length off
        return 0U;
    }

    if (m_rxDMRQueue2.dataSize() >= len) {
        return len;
    }

    return 0U;
}

/* Reads DMR Slot 2 frame data from the DMR Slot 2 ring buffer. */

uint32_t Modem::readDMRFrame2(uint8_t* data)
{
    assert(data != nullptr);
    std::lock_guard<std::mutex> lock(m_dmr2ReadLock);

    if (m_rxDMRQueue2.isEmpty())
        return 0U;

    uint8_t len = 0U;
    m_rxDMRQueue2.peek(&len, 1U);

    // this ensures we never get in a situation where we have length stuck on the queue
    if (m_rxDMRQueue2.dataSize() == 1U && len > m_rxDMRQueue2.dataSize()) {
        m_rxDMRQueue2.get(&len, 1U); // ensure we pop the length off
        return 0U;
    }

    if (m_rxDMRQueue2.dataSize() >= len) {
        m_rxDMRQueue2.get(&len, 1U); // ensure we pop the length off
        m_rxDMRQueue2.get(data, len);

        return len;
    }

    return 0U;
}

/* Get the frame data length for the next frame in the P25 ring buffer. */

uint32_t Modem::peekP25FrameLength()
{
    if (m_rxP25Queue.isEmpty())
        return 0U;

    uint8_t length[2U];
    ::memset(length, 0x00U, 2U);
    m_rxP25Queue.peek(length, 2U);

    uint16_t len = 0U;
    len = (length[0U] << 8) + length[1U];
#if DEBUG_MODEM
    LogDebug(LOG_MODEM, "Modem::peekP25FrameLength() len = %u, dataSize = %u", len, m_rxP25Queue.dataSize());
#endif
    // this ensures we never get in a situation where we have length stuck on the queue
    if (m_rxP25Queue.dataSize() == 2U && len > m_rxP25Queue.dataSize()) {
        m_rxP25Queue.get(length, 2U); // ensure we pop the length off
        return 0U;
    }

    if (m_rxP25Queue.dataSize() >= len) {
        return len;
    }

    return 0U;
}

/* Reads P25 frame data from the P25 ring buffer. */

uint32_t Modem::readP25Frame(uint8_t* data)
{
    assert(data != nullptr);
    std::lock_guard<std::mutex> lock(m_p25ReadLock);

    if (m_rxP25Queue.isEmpty())
        return 0U;

    uint8_t length[2U];
    ::memset(length, 0x00U, 2U);
    m_rxP25Queue.peek(length, 2U);

    uint16_t len = 0U;
    len = (length[0U] << 8) + length[1U];

    // this ensures we never get in a situation where we have length stuck on the queue
    if (m_rxP25Queue.dataSize() == 2U && len > m_rxP25Queue.dataSize()) {
        m_rxP25Queue.get(length, 2U); // ensure we pop the length off
        return 0U;
    }

    if (m_rxP25Queue.dataSize() >= len) {
        m_rxP25Queue.get(length, 2U); // ensure we pop the length off
        m_rxP25Queue.get(data, len);
        
        return len;
    }

    return 0U;
}

/* Get the frame data length for the next frame in the NXDN ring buffer. */

uint32_t Modem::peekNXDNFrameLength()
{
    if (m_rxNXDNQueue.isEmpty())
        return 0U;

    uint8_t len = 0U;
    m_rxNXDNQueue.peek(&len, 1U);
#if DEBUG_MODEM
    LogDebug(LOG_MODEM, "Modem::peekNXDNFrameLength() len = %u, dataSize = %u", len, m_rxNXDNQueue.dataSize());
#endif
    // this ensures we never get in a situation where we have length stuck on the queue
    if (m_rxNXDNQueue.dataSize() == 1U && len > m_rxNXDNQueue.dataSize()) {
        m_rxNXDNQueue.get(&len, 1U); // ensure we pop the length off
        return 0U;
    }

    if (m_rxNXDNQueue.dataSize() >= len) {
        return len;
    }

    return 0U;
}

/* Reads NXDN frame data from the NXDN ring buffer. */

uint32_t Modem::readNXDNFrame(uint8_t* data)
{
    assert(data != nullptr);
    std::lock_guard<std::mutex> lock(m_nxdnReadLock);

    if (m_rxNXDNQueue.isEmpty())
        return 0U;

    uint8_t len = 0U;
    m_rxNXDNQueue.peek(&len, 1U);

    // this ensures we never get in a situation where we have length stuck on the queue
    if (m_rxNXDNQueue.dataSize() == 1U && len > m_rxNXDNQueue.dataSize()) {
        m_rxNXDNQueue.get(&len, 1U); // ensure we pop the length off
        return 0U;
    }

    if (m_rxNXDNQueue.dataSize() >= len) {
        m_rxNXDNQueue.get(&len, 1U); // ensure we pop the length off
        m_rxNXDNQueue.get(data, len);

        return len;
    }

    return 0U;
}

/* Helper to test if the DMR Slot 1 ring buffer has free space. */

bool Modem::hasDMRSpace1() const
{
    return m_dmrSpace1 >= (DMRDEF::DMR_FRAME_LENGTH_BYTES + 2U);
}

/* Helper to test if the DMR Slot 2 ring buffer has free space. */

bool Modem::hasDMRSpace2() const
{
    return m_dmrSpace2 >= (DMRDEF::DMR_FRAME_LENGTH_BYTES + 2U);
}

/* Helper to test if the P25 ring buffer has free space. */

bool Modem::hasP25Space(uint32_t length) const
{
    return m_p25Space >= length;
}

/* Helper to test if the NXDN ring buffer has free space. */

bool Modem::hasNXDNSpace() const
{
    return m_nxdnSpace >= NXDDEF::NXDN_FRAME_LENGTH_BYTES;
}

/* Helper to test if the modem is a hotspot. */

bool Modem::isHotspot() const
{
    return m_isHotspot;
}

/* Flag indicating whether or not the air interface modem is transmitting. */

bool Modem::hasTX() const
{
    return m_tx;
}

/* Flag indicating whether or not the air interface modem has carrier detect. */

bool Modem::hasCD() const
{
    return m_cd;
}

/* Flag indicating whether or not the air interface modem is currently locked out. */

bool Modem::hasLockout() const
{
    return m_lockout;
}

/* Flag indicating whether or not the air interface modem is currently in an error condition. */

bool Modem::hasError() const
{
    return m_error;
}

/* Flag indicating whether or not the air interface modem has sent the initial modem status. */

bool Modem::gotModemStatus() const
{
    return m_gotModemStatus;
}

/* Clears any buffered DMR Slot 1 frame data to be sent to the air interface modem. */

void Modem::clearDMRFrame1()
{
    uint8_t buffer[3U];

    buffer[0U] = DVM_SHORT_FRAME_START;
    buffer[1U] = 3U;
    buffer[2U] = CMD_DMR_CLEAR1;
#if DEBUG_MODEM
    Utils::dump(1U, "Modem::clearDMRFrame1(), Written", buffer, 3U);
#endif
    write(buffer, 3U);
    Thread::sleep(5); // 5ms delay
}

/* Clears any buffered DMR Slot 2 frame data to be sent to the air interface modem. */

void Modem::clearDMRFrame2()
{
    uint8_t buffer[3U];

    buffer[0U] = DVM_SHORT_FRAME_START;
    buffer[1U] = 3U;
    buffer[2U] = CMD_DMR_CLEAR2;
#if DEBUG_MODEM
    Utils::dump(1U, "Modem::clearDMRFrame2(), Written", buffer, 3U);
#endif
    write(buffer, 3U);
    Thread::sleep(5); // 5ms delay
}

/* Clears any buffered P25 frame data to be sent to the air interface modem. */

void Modem::clearP25Frame()
{
    uint8_t buffer[3U];

    buffer[0U] = DVM_SHORT_FRAME_START;
    buffer[1U] = 3U;
    buffer[2U] = CMD_P25_CLEAR;
#if DEBUG_MODEM
    Utils::dump(1U, "Modem::clearP25Data(), Written", buffer, 3U);
#endif
    write(buffer, 3U);
    Thread::sleep(5); // 5ms delay
}

/* Clears any buffered NXDN frame data to be sent to the air interface modem. */

void Modem::clearNXDNFrame()
{
    uint8_t buffer[3U];

    buffer[0U] = DVM_SHORT_FRAME_START;
    buffer[1U] = 3U;
    buffer[2U] = CMD_NXDN_CLEAR;
#if DEBUG_MODEM
    Utils::dump(1U, "Modem::clearNXDNFrame(), Written", buffer, 3U);
#endif
    write(buffer, 3U);
    Thread::sleep(5); // 5ms delay
}

/* Internal helper to inject DMR Slot 1 frame data as if it came from the air interface modem. */

void Modem::injectDMRFrame1(const uint8_t* data, uint32_t length)
{
    assert(data != nullptr);
    assert(length > 0U);

    if (m_dmrEnabled) {
        if (m_trace)
            Utils::dump(1U, "Injected DMR Slot 1 Data", data, length);

        uint8_t val = length;
        m_rxDMRQueue1.addData(&val, 1U);

        val = TAG_DATA;
        m_rxDMRQueue1.addData(&val, 1U);
        val = DMRDEF::SYNC_VOICE & DMRDEF::SYNC_DATA; // valid sync
        m_rxDMRQueue1.addData(&val, 1U);

        m_rxDMRQueue1.addData(data, length);
    }
}

/* Internal helper to inject DMR Slot 2 frame data as if it came from the air interface modem. */

void Modem::injectDMRFrame2(const uint8_t* data, uint32_t length)
{
    assert(data != nullptr);
    assert(length > 0U);

    if (m_dmrEnabled) {
        if (m_trace)
            Utils::dump(1U, "Injected DMR Slot 2 Data", data, length);

        uint8_t val = length;
        m_rxDMRQueue2.addData(&val, 1U);

        val = TAG_DATA;
        m_rxDMRQueue2.addData(&val, 1U);
        val = DMRDEF::SYNC_VOICE & DMRDEF::SYNC_DATA; // valid sync
        m_rxDMRQueue2.addData(&val, 1U);

        m_rxDMRQueue2.addData(data, length);
    }
}

/* Internal helper to inject P25 frame data as if it came from the air interface modem. */

void Modem::injectP25Frame(const uint8_t* data, uint32_t length)
{
    assert(data != nullptr);
    assert(length > 0U);

    if (m_p25Enabled) {
        if (m_trace)
            Utils::dump(1U, "Injected P25 Data", data, length);

        uint8_t val = length;
        m_rxP25Queue.addData(&val, 1U);

        val = TAG_DATA;
        m_rxP25Queue.addData(&val, 1U);
        val = 0x01U;    // valid sync
        m_rxP25Queue.addData(&val, 1U);

        m_rxP25Queue.addData(data, length);
    }
}

/* Internal helper to inject NXDN frame data as if it came from the air interface modem. */

void Modem::injectNXDNFrame(const uint8_t* data, uint32_t length)
{
    assert(data != nullptr);
    assert(length > 0U);

    if (m_nxdnEnabled) {
        if (m_trace)
            Utils::dump(1U, "Injected NXDN Data", data, length);

        uint8_t val = length;
        m_rxNXDNQueue.addData(&val, 1U);

        val = TAG_DATA;
        m_rxNXDNQueue.addData(&val, 1U);
        val = 0x01U;    // valid sync
        m_rxNXDNQueue.addData(&val, 1U);

        m_rxNXDNQueue.addData(data, length);
    }
}

/* Writes DMR Slot 1 frame data to the DMR Slot 1 ring buffer. */

bool Modem::writeDMRFrame1(const uint8_t* data, uint32_t length)
{
    assert(data != nullptr);
    assert(length > 0U);

    if (m_dmrEnabled) {
        const uint8_t MAX_LENGTH = 40U;

        if (data[0U] != TAG_DATA && data[0U] != TAG_EOT)
            return false;
        if (length > MAX_LENGTH) {
            LogError(LOG_MODEM, "Modem::writeDMRData1(); request data to write >%u?, len = %u", MAX_LENGTH, length);
            Utils::dump(1U, "Modem::writeDMRData1(); Attmpted Data", data, length);
            return false;
        }

        uint8_t buffer[MAX_LENGTH];

        buffer[0U] = DVM_SHORT_FRAME_START;
        buffer[1U] = length + 2U;
        buffer[2U] = CMD_DMR_DATA1;

        ::memcpy(buffer + 3U, data + 1U, length - 1U);

        uint8_t len = length + 2U;

        // write or buffer DMR slot 1 data to air interface
        if (m_dmrSpace1 >= length) {
            if (m_debug)
                LogDebug(LOG_MODEM, "Modem::writeDMRData1(); immediate write (len %u)", length);
            if (m_trace)
                Utils::dump(1U, "Modem::writeDMRData1() Immediate TX DMR Data 1", buffer + 3U, length - 1U);

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
    } 
    else {
        return false;
    }
}

/* Writes DMR Slot 2 frame data to the DMR Slot 2 ring buffer. */

bool Modem::writeDMRFrame2(const uint8_t* data, uint32_t length)
{
    assert(data != nullptr);
    assert(length > 0U);

    if (m_dmrEnabled) {
        const uint8_t MAX_LENGTH = 40U;

        if (data[0U] != TAG_DATA && data[0U] != TAG_EOT)
            return false;
        if (length > MAX_LENGTH) {
            LogError(LOG_MODEM, "Modem::writeDMRData2(); request data to write >%u?, len = %u", MAX_LENGTH, length);
            Utils::dump(1U, "Modem::writeDMRData2(); Attmpted Data", data, length);
            return false;
        }

        uint8_t buffer[MAX_LENGTH];

        buffer[0U] = DVM_SHORT_FRAME_START;
        buffer[1U] = length + 2U;
        buffer[2U] = CMD_DMR_DATA2;

        ::memcpy(buffer + 3U, data + 1U, length - 1U);

        uint8_t len = length + 2U;

        // write or buffer DMR slot 2 data to air interface
        if (m_dmrSpace2 >= length) {
            if (m_debug)
                LogDebug(LOG_MODEM, "Modem::writeDMRData2(); immediate write (len %u)", length);
            if (m_trace)
                Utils::dump(1U, "Modem::writeDMRData2() Immediate TX DMR Data 2", buffer + 3U, length - 1U);

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
    }
    else {
        return false;
    }
}

/* Writes P25 frame data to the P25 ring buffer. */

bool Modem::writeP25Frame(const uint8_t* data, uint32_t length)
{
    assert(data != nullptr);
    assert(length > 0U);

    if (m_p25Enabled) {
        uint16_t MAX_LENGTH = 520U;
        if (m_protoVer <= 3U) {
            MAX_LENGTH = 251U; // for older firmware always ensure frames are shorter then 252 bytes
        }

        if (data[0U] != TAG_DATA && data[0U] != TAG_EOT)
            return false;
        if (length > MAX_LENGTH) {
            LogError(LOG_MODEM, "Modem::writeP25Data(); request data to write >%u?, len = %u", MAX_LENGTH, length);
            Utils::dump(1U, "Modem::writeP25Data(); Attmpted Data", data, length);
            return false;
        }

        UInt8Array __buffer = std::make_unique<uint8_t[]>(MAX_LENGTH);
        uint8_t* buffer = __buffer.get();

        if (length < 252U) {
            buffer[0U] = DVM_SHORT_FRAME_START;
            buffer[1U] = length + 2U;
            buffer[2U] = CMD_P25_DATA;
            ::memcpy(buffer + 3U, data + 1U, length - 1U);
        } else {
            length += 3U;
            buffer[0U] = DVM_LONG_FRAME_START;
            buffer[1U] = (length >> 8U) & 0xFFU;
            buffer[2U] = (length & 0xFFU);
            buffer[3U] = CMD_P25_DATA;
            ::memcpy(buffer + 4U, data + 1U, length - 1U);
        }

        uint8_t len = length + 2U;

        // write or buffer P25 data to air interface
        if (m_p25Space >= length) {
            if (m_debug)
                LogDebug(LOG_MODEM, "Modem::writeP25Data(); immediate write (len %u)", length);
            if (m_trace)
                Utils::dump(1U, "Modem::writeP25Data() Immediate TX P25 Data", buffer + 3U, length - 3U);

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
    }
    else {
        return false;
    }
}

/* Writes NXDN frame data to the NXDN ring buffer. */

bool Modem::writeNXDNFrame(const uint8_t* data, uint32_t length)
{
    assert(data != nullptr);
    assert(length > 0U);

    const uint8_t MAX_LENGTH = 250U;

    if (m_nxdnEnabled) {
        if (data[0U] != TAG_DATA && data[0U] != TAG_EOT)
            return false;
        if (length > MAX_LENGTH) {
            LogError(LOG_MODEM, "Modem::writeNXDNData(); request data to write >%u?, len = %u", MAX_LENGTH, length);
            Utils::dump(1U, "Modem::writeNXDNData(); Attmpted Data", data, length);
            return false;
        }

        uint8_t buffer[MAX_LENGTH];

        buffer[0U] = DVM_SHORT_FRAME_START;
        buffer[1U] = length + 2U;
        buffer[2U] = CMD_NXDN_DATA;

        ::memcpy(buffer + 3U, data + 1U, length - 1U);

        uint8_t len = length + 2U;

        // write or buffer NXDN data to air interface
        if (m_nxdnSpace >= length) {
            if (m_debug)
                LogDebug(LOG_MODEM, "Modem::writeNXDNData(); immediate write (len %u)", length);
            if (m_trace)
                Utils::dump(1U, "Modem::writeNXDNData() Immediate TX NXDN Data", buffer + 3U, length - 1U);

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
    }
    else {
        return false;
    }
}

/* Triggers the start of DMR transmit. */

bool Modem::writeDMRStart(bool tx)
{
    if (m_dmrEnabled) {
        if (tx && m_tx)
            return true;
        if (!tx && !m_tx)
            return true;

        uint8_t buffer[4U];

        buffer[0U] = DVM_SHORT_FRAME_START;
        buffer[1U] = 4U;
        buffer[2U] = CMD_DMR_START;
        buffer[3U] = tx ? 0x01U : 0x00U;
#if DEBUG_MODEM
        Utils::dump(1U, "Modem::writeDMRStart(), Written", buffer, 4U);
#endif
        return write(buffer, 4U) == 4;
    }
    else {
        return false;
    }
}

/* Writes a DMR short LC to the air interface modem. */

bool Modem::writeDMRShortLC(const uint8_t* lc)
{
    assert(lc != nullptr);

    if (m_dmrEnabled) {
        uint8_t buffer[12U];

        buffer[0U] = DVM_SHORT_FRAME_START;
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
    }
    else {
        return false;
    }
}

/* Writes a DMR abort message for the given slot to the air interface modem. */

bool Modem::writeDMRAbort(uint32_t slotNo)
{
    if (m_dmrEnabled) {
        uint8_t buffer[4U];

        buffer[0U] = DVM_SHORT_FRAME_START;
        buffer[1U] = 4U;
        buffer[2U] = CMD_DMR_ABORT;
        buffer[3U] = slotNo;
#if DEBUG_MODEM
        Utils::dump(1U, "Modem::writeDMRAbort(), Written", buffer, 4U);
#endif
        return write(buffer, 4U) == 4;
    }
    else {
        return false;
    }
}

/* Sets the ignore flags for setting the CACH Access Type bit on the air interface modem. */

bool Modem::setDMRIgnoreCACH_AT(uint8_t slotNo)
{
    if (m_dmrEnabled) {
        uint8_t buffer[4U];

        buffer[0U] = DVM_SHORT_FRAME_START;
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
    }
    else {
        return false;
    }
}

/* Writes raw data to the air interface modem. */

int Modem::write(const uint8_t* data, uint32_t length)
{
    return m_port->write(data, length);
}

/* Gets the flag for the V.24 connection state. */

bool Modem::isV24Connected() const
{
    return m_v24Connected;
}

/* Gets the current operating state for the air interface modem. */

DVM_STATE Modem::getState() const
{
    return m_modemState;
}

/* Sets the current operating state for the air interface modem. */

bool Modem::setState(DVM_STATE state)
{
    uint8_t buffer[4U];

    buffer[0U] = DVM_SHORT_FRAME_START;
    buffer[1U] = 4U;
    buffer[2U] = CMD_SET_MODE;
    buffer[3U] = state;
#if DEBUG_MODEM
    Utils::dump(1U, "Modem::setState(), Written", buffer, 4U);
#endif
    return write(buffer, 4U) == 4;
}

/* Transmits the given string as CW morse. */

bool Modem::sendCWId(const std::string& callsign)
{
    LogDebug(LOG_MODEM, "sending CW ID");

    uint32_t length = (uint32_t)callsign.length();
    if (length > 200U)
        length = 200U;

    uint8_t buffer[205U];

    buffer[0U] = DVM_SHORT_FRAME_START;
    buffer[1U] = length + 3U;
    buffer[2U] = CMD_SEND_CWID;

    for (uint32_t i = 0U; i < length; i++)
        buffer[i + 3U] = callsign.at(i);

    if (m_trace)
        Utils::dump(1U, "CW ID Data", buffer, length + 3U);

    return write(buffer, length + 3U) == int(length + 3U);
}

/* Returns the protocol version of the connected modem. */

uint8_t Modem::getVersion() const
{
    return m_protoVer;
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to warm reset the connection to the modem. */

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

/* Retrieve the air interface modem version. */

bool Modem::getFirmwareVersion()
{
    Thread::sleep(2000U);    // 2s

    for (uint32_t i = 0U; i < 6U; i++) {
        uint8_t buffer[3U];

        buffer[0U] = DVM_SHORT_FRAME_START;
        buffer[1U] = 3U;
        buffer[2U] = CMD_GET_VERSION;

        int ret = write(buffer, 3U);
        if (ret != 3)
            return false;

        for (uint32_t count = 0U; count < MAX_RESPONSES; count++) {
            Thread::sleep(10U);
            RESP_TYPE_DVM resp = getResponse();

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

/* Retrieve the current status from the air interface modem. */

bool Modem::getStatus()
{
    uint8_t buffer[3U];

    buffer[0U] = DVM_SHORT_FRAME_START;
    buffer[1U] = 3U;
    buffer[2U] = CMD_GET_STATUS;

    //LogDebug(LOG_MODEM, "getStatus(), polling modem status");

    return write(buffer, 3U) == 3;
}

/* Write configuration to the air interface modem. */

bool Modem::writeConfig()
{
    uint8_t buffer[25U];
    ::memset(buffer, 0x00U, 25U);
    uint8_t lengthToWrite = 17U;

    buffer[0U] = DVM_SHORT_FRAME_START;
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

    buffer[14U] = m_p25CorrCount;

    // are we on a protocol version 3 firmware?
    if (m_protoVer >= 3U) {
        lengthToWrite = 24U;

        if (m_nxdnEnabled)
            buffer[4U] |= 0x10U;

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
                LogError(LOG_MODEM, "No response, %s command", cmdToString(CMD_SET_CONFIG).c_str());
                return false;
            }
        }
    } while (resp == RTM_OK && m_buffer[2U] != CMD_ACK && m_buffer[2U] != CMD_NAK);
#if DEBUG_MODEM
    Utils::dump(1U, "Modem::writeConfig(), Response", m_buffer, m_length);
#endif
    if (resp == RTM_OK && m_buffer[2U] == CMD_NAK) {
        LogError(LOG_MODEM, "NAK, %s, command = 0x%02X, reason = %u (%s)", cmdToString(CMD_SET_CONFIG).c_str(), m_buffer[3U], m_buffer[4U], rsnToString(m_buffer[4U]).c_str());
        return false;
    }

    return true;
}

/* Write symbol level adjustments to the air interface modem. */

bool Modem::writeSymbolAdjust()
{
    uint8_t buffer[20U];
    ::memset(buffer, 0x00U, 20U);
    uint8_t lengthToWrite = 7U;

    buffer[0U] = DVM_SHORT_FRAME_START;
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
                LogError(LOG_MODEM, "No response, %s command", cmdToString(CMD_SET_SYMLVLADJ).c_str());
                return false;
            }
        }
    } while (resp == RTM_OK && m_buffer[2U] != CMD_ACK && m_buffer[2U] != CMD_NAK);

    if (resp == RTM_OK && m_buffer[2U] == CMD_NAK) {
        LogError(LOG_MODEM, "NAK, %s, command = 0x%02X, reason = %u (%s)", cmdToString(CMD_SET_SYMLVLADJ).c_str(), m_buffer[3U], m_buffer[4U], rsnToString(m_buffer[4U]).c_str());
        return false;
    }

    return true;
}

/* Write RF parameters to the air interface modem. */

bool Modem::writeRFParams()
{
    uint8_t buffer[22U];
    ::memset(buffer, 0x00U, 22U);
    uint8_t lengthToWrite = 18U;

    buffer[0U] = DVM_SHORT_FRAME_START;
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
                LogError(LOG_MODEM, "No response, %s command", cmdToString(CMD_SET_RFPARAMS).c_str());
                return false;
            }
        }
    } while (resp == RTM_OK && m_buffer[2U] != RSN_OK && m_buffer[2U] != RSN_NAK);

    // CUtils::dump(1U, "Response", m_buffer, m_length);

    if (resp == RTM_OK && m_buffer[2U] == RSN_NAK) {
        LogError(LOG_MODEM, "NAK, %s, command = 0x%02X, reason = %u (%s)", cmdToString(CMD_SET_RFPARAMS).c_str(), m_buffer[3U], m_buffer[4U], rsnToString(m_buffer[4U]).c_str());
        return false;
    }

    return true;
}

/* Retrieve the data from the configuration area on the air interface modem. */

bool Modem::readFlash()
{
    Thread::sleep(2000U);    // 2s

    for (uint32_t i = 0U; i < 6U; i++) {
        uint8_t buffer[3U];

        buffer[0U] = DVM_SHORT_FRAME_START;
        buffer[1U] = 3U;
        buffer[2U] = CMD_FLSH_READ;

        int ret = write(buffer, 3U);
        if (ret != 3)
            return false;

        for (uint32_t count = 0U; count < MAX_RESPONSES; count++) {
            Thread::sleep(10U);
            RESP_TYPE_DVM resp = getResponse();

            if (resp == RTM_ERROR)
                continue;

            if (resp == RTM_OK && m_buffer[2U] == CMD_NAK) {
                LogWarning(LOG_MODEM, "%s, old modem that doesn't support flash commands?", cmdToString(CMD_FLSH_READ).c_str());
                m_flashDisabled = true;
                return false;
            }

            m_flashDisabled = false;
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

/* Process the configuration data from the air interface modem. */

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

/* Print debug air interface messages to the host log. */

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
        LogDebug(LOG_MODEM, "DSP_FW_API %.*s", len - 3U, buffer + 3U);
    }
    else if (buffer[2U] == CMD_DEBUG2) {
        short val1 = (buffer[len - 2U] << 8) | buffer[len - 1U];
        LogDebug(LOG_MODEM, "DSP_FW_API %.*s %X", len - 5U, buffer + 3U, val1);
    }
    else if (buffer[2U] == CMD_DEBUG3) {
        short val1 = (buffer[len - 4U] << 8) | buffer[len - 3U];
        short val2 = (buffer[len - 2U] << 8) | buffer[len - 1U];
        LogDebug(LOG_MODEM, "DSP_FW_API %.*s %X %X", len - 7U, buffer + 3U, val1, val2);
    }
    else if (buffer[2U] == CMD_DEBUG4) {
        short val1 = (buffer[len - 6U] << 8) | buffer[len - 5U];
        short val2 = (buffer[len - 4U] << 8) | buffer[len - 3U];
        short val3 = (buffer[len - 2U] << 8) | buffer[len - 1U];
        LogDebug(LOG_MODEM, "DSP_FW_API %.*s %X %X %X", len - 9U, buffer + 3U, val1, val2, val3);
    }
    else if (buffer[2U] == CMD_DEBUG5) {
        short val1 = (buffer[len - 8U] << 8) | buffer[len - 7U];
        short val2 = (buffer[len - 6U] << 8) | buffer[len - 5U];
        short val3 = (buffer[len - 4U] << 8) | buffer[len - 3U];
        short val4 = (buffer[len - 2U] << 8) | buffer[len - 1U];
        LogDebug(LOG_MODEM, "DSP_FW_API %.*s %X %X %X %X", len - 11U, buffer + 3U, val1, val2, val3, val4);
    }
    else if (buffer[2U] == CMD_DEBUG_DUMP) {
        uint8_t data[255U];
        ::memset(data, 0x00U, 255U);
        ::memcpy(data, buffer, len);

        Utils::dump(1U, "Modem::printDebug() DSP_FW_API Debug Dump", data, len);
    }
}

/* Helper to get the raw response packet from modem. */

RESP_TYPE_DVM Modem::getResponse()
{
    m_rspDoubleLength = false;

    //LogDebug(LOG_MODEM, "Modem::getResponse(), checking if we have data");

    // get the start of the frame or nothing at all
    if (m_rspState == RESP_START) {
        int ret = m_port->read(m_buffer + 0U, 1U);
        if (ret < 0) {
            LogError(LOG_MODEM, "Error reading from the modem, ret = %d", ret);
            m_rspState = RESP_START;
            return RTM_ERROR;
        }

        if (ret == 0) {
            //LogDebug(LOG_MODEM, "Modem::getResponse(), no data available");
            return RTM_TIMEOUT;
        }

        if (m_buffer[0U] != DVM_SHORT_FRAME_START &&
            m_buffer[0U] != DVM_LONG_FRAME_START) {
            //LogError(LOG_MODEM, "Modem::getResponse(), illegal response, first byte not a frame start; byte = %02X", m_buffer[0U]);
            ::memset(m_buffer, 0x00U, BUFFER_LENGTH);
            m_rspState = RESP_START;
            return RTM_ERROR;
        }

        if (m_buffer[0U] == DVM_LONG_FRAME_START) {
            m_rspDoubleLength = true;
        }

        //LogDebug(LOG_MODEM, "Modem::getResponse(), RESP_START");

        m_rspState = RESP_LENGTH1;
    }

    //LogDebug(LOG_MODEM, "Modem::getResponse(), getting frame length 1/2, rspDoubleLength = %u", m_rspDoubleLength);
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

        if (m_buffer[1U] >= 250U && !m_rspDoubleLength) {
            LogError(LOG_MODEM, "Invalid length received from the modem, len = %u", m_buffer[1U]);
            m_rspState = RESP_START;
            return RTM_ERROR;
        }

        if (m_rspDoubleLength) {
            m_rspState = RESP_LENGTH2;
            m_length = ((m_buffer[1U] & 0xFFU) << 8);
        } else {
            m_rspState = RESP_TYPE;
            m_length = m_buffer[1U];
        }

        //LogDebug(LOG_MODEM, "Modem::getResponse(), RESP_LENGTH1, len = %u", m_length);

        m_rspOffset = 2U;
    }

    //LogDebug(LOG_MODEM, "Modem::getResponse(), getting frame length 2/2");
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

        m_length = (m_length + (m_buffer[2U] & 0xFFU));
        m_rspState = RESP_TYPE;

        //LogDebug(LOG_MODEM, "Modem::getResponse(), RESP_LENGTH2, len = %u", m_length);

        m_rspDoubleLength = true;
        m_rspOffset = 3U;
    }

    //LogDebug(LOG_MODEM, "Modem::getResponse(), getting frame type");
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

        //LogDebug(LOG_MODEM, "Modem::getResponse(), RESP_TYPE, len = %u, type = %u", m_length, m_rspType);

        m_rspState = RESP_DATA;
        m_rspOffset++;
    }

    //LogDebug(LOG_MODEM, "Modem::getResponse(), getting frame data");

    // get the frame data
    if (m_rspState == RESP_DATA) {
        if (m_respTrace)
            LogDebug(LOG_MODEM, "Modem::getResponse(), RESP_DATA, len = %u, offset = %u, type = %02X", m_length, m_rspOffset, m_rspType);

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

        if (m_respTrace)
            Utils::dump(1U, "Modem::getResponse() Buffer", m_buffer, m_length);
    }

    m_rspState = RESP_START;
    m_rspOffset = 0U;

    return RTM_OK;
}

/* Helper to convert a serial opcode to a string. */

std::string Modem::cmdToString(uint8_t opcode)
{
    switch (opcode) {
    case CMD_GET_VERSION:
        return std::string("GET_VERSION");
    case CMD_GET_STATUS:
        return std::string("GET_STATUS");
    case CMD_SET_CONFIG:
        return std::string("SET_CONFIG");
    case CMD_SET_MODE:
        return std::string("SET_MODE");

    case CMD_SET_SYMLVLADJ:
        return std::string("SET_SYMLVLADJ");
    case CMD_SET_RXLEVEL:
        return std::string("SET_RXLEVEL");
    case CMD_SET_RFPARAMS:
        return std::string("SET_RFPARAMS");

    case CMD_CAL_DATA:
        return std::string("CAL_DATA");
    case CMD_RSSI_DATA:
        return std::string("RSSI_DATA");

    case CMD_SEND_CWID:
        return std::string("SEND_CWID");

    case CMD_SET_BUFFERS:
        return std::string("SET_BUFFERS");

    case CMD_DMR_DATA1:
        return std::string("DMR_DATA1");
    case CMD_DMR_LOST1:
        return std::string("DMR_LOST1");
    case CMD_DMR_DATA2:
        return std::string("DMR_DATA2");
    case CMD_DMR_LOST2:
        return std::string("DMR_LOST2");
    case CMD_DMR_SHORTLC:
        return std::string("DMR_SHORTLC");
    case CMD_DMR_START:
        return std::string("DMR_START");
    case CMD_DMR_ABORT:
        return std::string("DMR_ABORT");
    case CMD_DMR_CACH_AT_CTRL:
        return std::string("DMR_CACH_AT_CTRL");
    case CMD_DMR_CLEAR1:
        return std::string("DMR_CLEAR1");
    case CMD_DMR_CLEAR2:
        return std::string("DMR_CLEAR2");

    case CMD_P25_DATA:
        return std::string("P25_DATA");
    case CMD_P25_LOST:
        return std::string("P25_LOST");
    case CMD_P25_CLEAR:
        return std::string("P25_CLEAR");

    case CMD_NXDN_DATA:
        return std::string("NXDN_DATA");
    case CMD_NXDN_LOST:
        return std::string("NXDN_LOST");
    case CMD_NXDN_CLEAR:
        return std::string("NXDN_CLEAR");

    case CMD_ACK:
        return std::string("ACK");
    case CMD_NAK:
        return std::string("NAK");

    case CMD_FLSH_READ:
        return std::string("FLSH_READ");
    case CMD_FLSH_WRITE:
        return std::string("FLSH_WRITE");

    case CMD_RESET_MCU:
        return std::string("CMD_RESET_MCU");

    default:
        return std::string();
    }
}

/* Helper to convert a serial reason code to a string. */

std::string Modem::rsnToString(uint8_t reason)
{
    switch (reason) {
    case RSN_OK:
        return std::string("OK");
    case RSN_NAK:
        return std::string("NAK");

    case RSN_ILLEGAL_LENGTH:
        return std::string("ILLEGAL_LENGTH");
    case RSN_INVALID_REQUEST:
        return std::string("INVALID_REQUEST");
    case RSN_RINGBUFF_FULL:
        return std::string("RINGBUFF_FULL");

    case RSN_INVALID_FDMA_PREAMBLE:
        return std::string("INVALID_FDMA_PREAMBLE");
    case RSN_INVALID_MODE:
        return std::string("INVALID_MODE");

    case RSN_INVALID_DMR_CC:
        return std::string("INVALID_DMR_CC");
    case RSN_INVALID_DMR_SLOT:
        return std::string("INVALID_DMR_SLOT");
    case RSN_INVALID_DMR_START:
        return std::string("INVALID_DMR_START");
    case RSN_INVALID_DMR_RX_DELAY:
        return std::string("INVALID_DMR_RX_DELAY");

    case RSN_INVALID_P25_CORR_COUNT:
        return std::string("INVALID_P25_CORR_COUNT");

    case RSN_NO_INTERNAL_FLASH:
        return std::string("NO_INTERNAL_FLASH");
    case RSN_FAILED_ERASE_FLASH:
        return std::string("FAILED_ERASE_FLASH");
    case RSN_FAILED_WRITE_FLASH:
        return std::string("FAILED_WRITE_FLASH");
    case RSN_FLASH_WRITE_TOO_BIG:
        return std::string("FLASH_WRITE_TOO_BIG");

    case RSN_HS_NO_DUAL_MODE:
        return std::string("HS_NO_DUAL_MODE");

    case RSN_DMR_DISABLED:
        return std::string("DMR_DISABLED");
    case RSN_P25_DISABLED:
        return std::string("P25_DISABLED");
    case RSN_NXDN_DISABLED:
        return std::string("NXDN_DISABLED");

    default:
        return std::string();
    }
}
