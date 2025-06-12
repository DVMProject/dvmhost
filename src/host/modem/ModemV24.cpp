// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
 *  Copyright (C) 2024 Patrick McDonnell, W3AXL
 *
 */
#include "Defines.h"
#include "common/p25/P25Defines.h"
#include "common/p25/data/DataHeader.h"
#include "common/p25/data/DataBlock.h"
#include "common/p25/data/LowSpeedData.h"
#include "common/p25/dfsi/LC.h"
#include "common/p25/dfsi/frames/Frames.h"
#include "common/p25/lc/LC.h"
#include "common/p25/lc/tsbk/TSBKFactory.h"
#include "common/p25/NID.h"
#include "common/p25/P25Utils.h"
#include "common/p25/Sync.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "modem/ModemV24.h"

using namespace modem;
using namespace p25;
using namespace p25::defines;
using namespace p25::dfsi::defines;
using namespace p25::dfsi::frames;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the ModemV24 class. */

ModemV24::ModemV24(port::IModemPort* port, bool duplex, uint32_t p25QueueSize, uint32_t p25TxQueueSize,
    bool rtrt, bool diu, uint16_t jitter, bool dumpModemStatus, bool trace, bool debug) :
    Modem(port, duplex, false, false, false, false, false, 80U, 7U, 8U, 1U, p25QueueSize, 1U,
        false, false, dumpModemStatus, trace, debug),
    m_rtrt(rtrt),
    m_diu(diu),
    m_superFrameCnt(0U),
    m_audio(),
    m_nid(nullptr),
    m_txP25Queue(p25TxQueueSize, "TX P25 Queue"),
    m_txCall(),
    m_rxCall(),
    m_txCallInProgress(false),
    m_rxCallInProgress(false),
    m_txLastFrameTime(0U),
    m_rxLastFrameTime(0U),
    m_callTimeout(200U),
    m_jitter(jitter),
    m_lastP25Tx(0U),
    m_rs(),
    m_useTIAFormat(false)
{
    m_v24Connected = false; // defaulted to false for V.24 modems

    // Init m_call
    m_txCall = new DFSICallData();
    m_rxCall = new DFSICallData();
}

/* Finalizes a instance of the Modem class. */

ModemV24::~ModemV24()
{
    delete m_nid;
    delete m_txCall;
    delete m_rxCall;
}

/* Sets the call timeout. */

void ModemV24::setCallTimeout(uint16_t timeout)
{
    m_callTimeout = timeout;
}

/* Sets the P25 NAC. */

void ModemV24::setP25NAC(uint32_t nac)
{
    Modem::setP25NAC(nac);
    m_nid = new NID(nac);
}

/* Helper to set the TIA-102 format DFSI frame flag. */

void ModemV24::setTIAFormat(bool set)
{
    m_useTIAFormat = set;
}

/* Opens connection to the air interface modem. */

bool ModemV24::open()
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
    } else {
        // Stopping the inactivity timer here when a firmware version has been
        // successfuly read prevents the death spiral of "no reply from modem..."
        m_inactivityTimer.stop();
    }

    m_rspOffset = 0U;
    m_rspState = RESP_START;

    // do we have an open port handler?
    if (m_openPortHandler) {
        ret = m_openPortHandler(this);
        if (!ret)
            return false;

        m_error = false;
        return true;
    }

    m_statusTimer.start();

    m_error = false;

    if (m_useTIAFormat)
        LogMessage(LOG_MODEM, "Modem Ready [Direct Mode / TIA-102]");
    else
        LogMessage(LOG_MODEM, "Modem Ready [Direct Mode / V.24]");
    return true;
}

/* Updates the timer by the passed number of milliseconds. */

void ModemV24::clock(uint32_t ms)
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

    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
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
        /** Project 25 */
        case CMD_P25_DATA:
        {
            if (m_p25Enabled) {
                std::lock_guard<std::mutex> lock(m_p25ReadLock);
            
                // convert data from V.24/DFSI formatting to TIA-102 air formatting
                if (m_useTIAFormat)
                    convertToAirTIA(m_buffer + (cmdOffset + 1U), m_length - (cmdOffset + 1U));
                else
                    convertToAir(m_buffer + (cmdOffset + 1U), m_length - (cmdOffset + 1U));
                if (m_trace)
                    Utils::dump(1U, "ModemV24::clock() RX P25 Data", m_buffer + (cmdOffset + 1U), m_length - (cmdOffset + 1U));
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

            m_v24Connected = (m_buffer[3U] & 0x40U) == 0x40U;
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
            // DMR and NXDN space are always 0U since the board doesn't support them
            m_dmrSpace1 = 0U;
            m_dmrSpace2 = 0U;
            m_nxdnSpace = 0U;

            // P25 free space can be reported as 16-byte blocks or frames based on the flag above
            if (spaceInBlocks)
                m_p25Space = m_buffer[10U] * P25_BUFFER_BLOCK_SIZE;
            else
                m_p25Space = m_buffer[10U] * (P25DEF::P25_LDU_FRAME_LENGTH_BYTES);

            if (m_dumpModemStatus) {
                LogDebugEx(LOG_MODEM, "ModemV24::clock()", "CMD_GET_STATUS, isHotspot = %u, v24Connected = %u, dmr = %u / %u, p25 = %u / %u, nxdn = %u / %u, modemState = %u, tx = %u, adcOverflow = %u, rxOverflow = %u, txOverflow = %u, dacOverflow = %u, dmrSpace1 = %u, dmrSpace2 = %u, p25Space = %u, nxdnSpace = %u",
                    m_isHotspot, m_v24Connected, dmrEnable, m_dmrEnabled, p25Enable, m_p25Enabled, nxdnEnable, m_nxdnEnabled, m_modemState, m_tx, adcOverflow, rxOverflow, txOverflow, dacOverflow, m_dmrSpace1, m_dmrSpace2, m_p25Space, m_nxdnSpace);
                LogDebugEx(LOG_MODEM, "ModemV24::clock()", "CMD_GET_STATUS, rxDMRData1 size = %u, len = %u, free = %u; rxDMRData2 size = %u, len = %u, free = %u, rxP25Data size = %u, len = %u, free = %u, rxNXDNData size = %u, len = %u, free = %u",
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

    // write anything waiting to the serial port
    int len = writeSerial();
    if (m_debug && len > 0) {
        LogDebug(LOG_MODEM, "Wrote %u-byte message to the serial V24 device", len);
    } else if (len < 0) {
        LogError(LOG_MODEM, "Failed to write to serial port!");
    }

    // clear an RX call in progress flag if we're longer than our timeout value
    now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (m_rxCallInProgress && (now - m_rxLastFrameTime > m_callTimeout)) {
        m_rxCallInProgress = false;
        m_rxCall->resetCallData();
        LogWarning(LOG_MODEM, "No call data received from V24 for %u ms, resetting RX call", (now - m_rxLastFrameTime));
    }
}

/* Closes connection to the air interface modem. */

void ModemV24::close()
{
    LogMessage(LOG_MODEM, "Closing the modem");
    m_port->close();

    m_gotModemStatus = false;

    // do we have a close port handler?
    if (m_closePortHandler != nullptr) {
        m_closePortHandler(this);
    }
}

/* Helper to test if the P25 ring buffer has free space. */

bool ModemV24::hasP25Space(uint32_t length) const
{
    return Modem::hasP25Space(length);
}

/* Writes raw data to the air interface modem. */

int ModemV24::write(const uint8_t* data, uint32_t length)
{
    assert(data != nullptr);

    uint8_t modemCommand = CMD_GET_VERSION;
    if (data[0U] == DVM_SHORT_FRAME_START) {
        modemCommand = data[2U];
    } else if (data[0U] == DVM_LONG_FRAME_START) {
        modemCommand = data[3U];
    }

    if (modemCommand == CMD_P25_DATA) {
        DECLARE_UINT8_ARRAY(buffer, length);
        ::memcpy(buffer, data + 2U, length);

        if (m_useTIAFormat)
            convertFromAirTIA(buffer, length);
        else
            convertFromAir(buffer, length);
        return length;
    } else {
        return Modem::write(data, length);
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Helper to write data from the P25 Tx queue to the serial interface. */

int ModemV24::writeSerial()
{
    /*
     *  Serial TX ringbuffer format:
     * 
     *  | 0x01 | 0x02 | 0x03 | 0x04 | 0x05 | 0x06 | 0x07 | 0x08 | 0x09 | 0x0A | 0x0B | 0x0C | ... |
     *  |   Length    | Tag  |               int64_t timestamp in ms                 |   data     |
     */

    // check empty
    if (m_txP25Queue.isEmpty())
        return 0U;

    // get length
    uint8_t length[2U];
    ::memset(length, 0x00U, 2U);
    m_txP25Queue.peek(length, 2U);

    // convert length byets to int
    uint16_t len = 0U;
    len = (length[0U] << 8) + length[1U];

    // this ensures we never get in a situation where we have length & type bytes stuck in the queue by themselves
    if (m_txP25Queue.dataSize() == 2U && len > m_txP25Queue.dataSize()) {
        m_txP25Queue.get(length, 2U); // ensure we pop bytes off
        return 0U;
    }

    // get current timestamp
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // peek the timestamp to see if we should wait
    if (m_txP25Queue.dataSize() >= 11U) {
        uint8_t lengthTagTs[11U];
        ::memset(lengthTagTs, 0x00U, 11U);
        m_txP25Queue.peek(lengthTagTs, 11U);

        // get the timestamp
        int64_t ts;
        assert(sizeof ts == 8);
        ::memcpy(&ts, lengthTagTs + 3U, 8U);

        // if it's not time to send, return
        if (ts > now) {
            return 0U;
        }
    }

    // check if we have enough data to get everything - len + 2U (length bytes) + 1U (tag) + 8U (timestamp)
    if (m_txP25Queue.dataSize() >= len + 11U) {
        // Get the length, tag and timestamp
        uint8_t lengthTagTs[11U];
        m_txP25Queue.get(lengthTagTs, 11U);
        
        // Get the actual data
        DECLARE_UINT8_ARRAY(buffer, len);
        m_txP25Queue.get(buffer, len);
        
        // Sanity check on data tag
        uint8_t tag = lengthTagTs[2U];
        if (tag != TAG_DATA) {
            LogError(LOG_MODEM, "Got unexpected data tag from TX P25 ringbuffer! %02X", tag);
            return 0U;
        }

        // we already checked the timestamp above, so we just get the data and write it
        return m_port->write(buffer, len);
    }

    return 0U;
}

/* Helper to store converted Rx frames. */

void ModemV24::storeConvertedRx(const uint8_t* buffer, uint32_t length)
{
    // store converted frame into the Rx modem queue
    uint8_t storedLen[2U];
    if (length > 255U)
        storedLen[0U] = (length >> 8U) & 0xFFU;
    else
        storedLen[0U] = 0x00U;
    storedLen[1U] = length & 0xFFU;
    m_rxP25Queue.addData(storedLen, 2U);

    //Utils::dump("Storing converted RX data", buffer, length);

    m_rxP25Queue.addData(buffer, length);
}

/* Helper to generate a P25 TDU packet. */

void ModemV24::create_TDU(uint8_t* buffer)
{
    assert(buffer != nullptr);

    uint8_t data[P25_TDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, P25_TDU_FRAME_LENGTH_BYTES);

    // generate Sync
    Sync::addP25Sync(data + 2U);

    // generate NID
    m_nid->encode(data + 2U, DUID::TDU);

    // add status bits
    P25Utils::addStatusBits(data + 2U, P25_TDU_FRAME_LENGTH_BITS, false, false);

    buffer[0U] = modem::TAG_EOT;
    buffer[1U] = 0x01U;
    ::memcpy(buffer, data, P25_TDU_FRAME_LENGTH_BYTES + 2U);
}

/* Internal helper to convert from V.24/DFSI to TIA-102 air interface. */

void ModemV24::convertToAir(const uint8_t *data, uint32_t length)
{
    assert(data != nullptr);
    assert(length > 0U);

    uint8_t buffer[P25_PDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(buffer, 0x00U, P25_PDU_FRAME_LENGTH_BYTES + 2U);

    // get the DFSI data (skip the 0x00 padded byte at the start)
    DECLARE_UINT8_ARRAY(dfsiData, length - 1U);
    ::memcpy(dfsiData, data + 1U, length - 1U);

    if (m_debug)
        Utils::dump("V24 RX data from board", dfsiData, length - 1U);

    DFSIFrameType::E frameType = (DFSIFrameType::E)dfsiData[0U];
    m_rxLastFrameTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // Switch based on DFSI frame type
    switch (frameType) {
        case DFSIFrameType::MOT_START_STOP:
        {
            MotStartOfStream start = MotStartOfStream(dfsiData);
            if (start.getStartStop() == StartStopFlag::START) {
                m_rxCall->resetCallData();
                m_rxCallInProgress = true;
                if (m_debug) {
                    LogDebug(LOG_MODEM, "V24 RX, ICW START, RT = $%02X, Type = $%02X", dfsiData[2U], dfsiData[4U]);
                }
            } else {
                if (m_rxCallInProgress) {
                    m_rxCall->resetCallData();
                    m_rxCallInProgress = false;
                    if (m_debug) {
                        LogDebug(LOG_MODEM, "V24 RX, ICW STOP, RT = $%02X, Type = $%02X", dfsiData[2U], dfsiData[4U]);
                    }
                    // generate a TDU
                    create_TDU(buffer);
                    storeConvertedRx(buffer, P25_TDU_FRAME_LENGTH_BYTES + 2U);
                }
            }
        }
        break;

        case DFSIFrameType::MOT_VHDR_1:
        {
            MotVoiceHeader1 vhdr1 = MotVoiceHeader1(dfsiData);

            // copy to call data VHDR1
            ::memset(m_rxCall->VHDR1, 0x00U, MotVoiceHeader1::HCW_LENGTH);
            ::memcpy(m_rxCall->VHDR1, vhdr1.header, MotVoiceHeader1::HCW_LENGTH);
        }
        break;
        case DFSIFrameType::MOT_VHDR_2:
        {
            MotVoiceHeader2 vhdr2 = MotVoiceHeader2(dfsiData);

            // copy to call data VHDR2
            ::memset(m_rxCall->VHDR2, 0x00U, MotVoiceHeader2::HCW_LENGTH);
            ::memcpy(m_rxCall->VHDR2, vhdr2.header, MotVoiceHeader2::HCW_LENGTH);

            // buffer for raw VHDR data
            uint8_t raw[DFSI_VHDR_RAW_LEN];
            ::memset(raw, 0x00U, DFSI_VHDR_RAW_LEN);

            ::memcpy(raw,       m_rxCall->VHDR1, 8U);
            ::memcpy(raw + 8U,  m_rxCall->VHDR1 + 9U, 8U);
            ::memcpy(raw + 16U, m_rxCall->VHDR1 + 18U, 2U);

            ::memcpy(raw + 18U, m_rxCall->VHDR2, 8U);
            ::memcpy(raw + 26U, m_rxCall->VHDR2 + 9U, 8U);
            ::memcpy(raw + 34U, m_rxCall->VHDR2 + 18U, 2U);

            // buffer for decoded VHDR data
            uint8_t vhdr[DFSI_VHDR_LEN];

            uint32_t offset = 0U;
            for (uint32_t i = 0; i < DFSI_VHDR_RAW_LEN; i++, offset += 6)
                Utils::hex2Bin(raw[i], vhdr, offset);

            // try to decode the RS data
            try {
                bool ret = m_rs.decode362017(vhdr);
                if (!ret) {
                    LogError(LOG_MODEM, "V.24/DFSI traffic failed to decode RS (36,20,17) FEC");
                } else {
                    // late entry?
                    if (!m_rxCallInProgress) {
                        m_rxCallInProgress = true;
                        m_rxCall->resetCallData();
                        if (m_debug)
                            LogDebug(LOG_MODEM, "V24 RX VHDR late entry, resetting call data");
                    }

                    ::memcpy(m_rxCall->MI, vhdr, MI_LENGTH_BYTES);

                    m_rxCall->mfId = vhdr[9U];
                    m_rxCall->algoId = vhdr[10U];
                    m_rxCall->kId = GET_UINT16(vhdr, 11U);
                    m_rxCall->dstId = GET_UINT16(vhdr, 13U);

                    if (m_debug) {
                        LogDebug(LOG_MODEM, "P25, VHDR algId = $%02X, kId = $%04X, dstId = $%04X", m_rxCall->algoId, m_rxCall->kId, m_rxCall->dstId);
                    }

                    // generate a HDU
                    lc::LC lc = lc::LC();
                    lc.setDstId(m_rxCall->dstId);
                    lc.setAlgId(m_rxCall->algoId);
                    lc.setKId(m_rxCall->kId);
                    lc.setMI(m_rxCall->MI);

                    // generate Sync
                    Sync::addP25Sync(buffer + 2U);

                    // generate NID
                    m_nid->encode(buffer + 2U, DUID::HDU);

                    // generate HDU
                    lc.encodeHDU(buffer + 2U);

                    // add status bits
                    P25Utils::addStatusBits(buffer + 2U, P25_HDU_FRAME_LENGTH_BITS, true, false);

                    buffer[0U] = modem::TAG_DATA;
                    buffer[1U] = 0x01U;
                    storeConvertedRx(buffer, P25_HDU_FRAME_LENGTH_BYTES + 2U);
                }
            }
            catch (...) {
                LogError(LOG_MODEM, "V.24/DFSI RX traffic got exception while trying to decode RS data for VHDR");
            }
        }
        break;

        // VOICE1/10 create a start voice frame
        case DFSIFrameType::LDU1_VOICE1:
        {
            MotStartVoiceFrame svf = MotStartVoiceFrame(dfsiData);
            ::memcpy(m_rxCall->netLDU1 + 10U, svf.fullRateVoice->imbeData, RAW_IMBE_LENGTH_BYTES);
            m_rxCall->n++;
        }
        break;
        case DFSIFrameType::LDU2_VOICE10:
        {
            MotStartVoiceFrame svf = MotStartVoiceFrame(dfsiData);
            ::memcpy(m_rxCall->netLDU2 + 10U, svf.fullRateVoice->imbeData, RAW_IMBE_LENGTH_BYTES);
            m_rxCall->n++;
        }
        break;

        case DFSIFrameType::PDU:
        {
            // bryanb: this is gonna be a complete clusterfuck...
            MotPDUFrame pf = MotPDUFrame(dfsiData);
            data::DataHeader dataHeader = data::DataHeader();
            if (!dataHeader.decode(pf.pduHeaderData, true)) {
                LogError(LOG_MODEM, "V.24/DFSI traffic failed to decode PDU FEC");
            } else {
                uint32_t bitLength = ((dataHeader.getBlocksToFollow() + 1U) * P25_PDU_FEC_LENGTH_BITS) + P25_PREAMBLE_LENGTH_BITS;
                uint32_t offset = P25_PREAMBLE_LENGTH_BITS;

                DECLARE_UINT8_ARRAY(data, (bitLength / 8U) + 1U);

                uint8_t block[P25_PDU_FEC_LENGTH_BYTES];
                ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

                uint32_t blocksToFollow = dataHeader.getBlocksToFollow();

                if (blocksToFollow > 0U) {
                    uint32_t dataOffset = MotPDUFrame::LENGTH + 1U;

                    // generate the PDU data
                    for (uint32_t i = 0U; i < blocksToFollow; i++) {
                        data::DataBlock dataBlock = data::DataBlock();
                        dataBlock.setFormat(dataHeader);
                        dataBlock.setSerialNo(i);
                        dataBlock.setData(dfsiData + dataOffset);

                        ::memset(block, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
                        dataBlock.encode(block);
                        Utils::setBitRange(block, data, offset, P25_PDU_FEC_LENGTH_BITS);

                        offset += P25_PDU_FEC_LENGTH_BITS;
                        dataOffset += ((dataHeader.getFormat() == PDUFormatType::CONFIRMED) ? P25_PDU_CONFIRMED_DATA_LENGTH_BYTES : P25_PDU_UNCONFIRMED_LENGTH_BYTES) + 1U;
                    }
                }

                uint8_t buffer[P25_PDU_FRAME_LENGTH_BYTES + 2U];
                ::memset(buffer, 0x00U, P25_PDU_FRAME_LENGTH_BYTES + 2U);

                // add the data
                uint32_t newBitLength = P25Utils::encodeByLength(data, buffer + 2U, bitLength);
                uint32_t newByteLength = newBitLength / 8U;
                if ((newBitLength % 8U) > 0U)
                    newByteLength++;

                // generate Sync
                Sync::addP25Sync(buffer + 2U);

                // generate NID
                m_nid->encode(buffer + 2U, DUID::PDU);

                // add status bits
                P25Utils::addStatusBits(buffer + 2U, newBitLength, false, false);
                P25Utils::addIdleStatusBits(buffer + 2U, newBitLength);
                P25Utils::setStatusBitsStartIdle(buffer + 2U);

                storeConvertedRx(buffer, P25_PDU_FRAME_LENGTH_BYTES + 2U);
            }
        }
        break;

        case DFSIFrameType::TSBK:
        {
            MotTSBKFrame tf = MotTSBKFrame(dfsiData);
            lc::tsbk::OSP_TSBK_RAW tsbk = lc::tsbk::OSP_TSBK_RAW();
            if (!tsbk.decode(tf.tsbkData, true)) {
                LogError(LOG_MODEM, "V.24/DFSI traffic failed to decode TSBK FEC");
            } else {
                uint8_t buffer[P25_TSDU_FRAME_LENGTH_BYTES + 2U];
                ::memset(buffer, 0x00U, P25_TSDU_FRAME_LENGTH_BYTES + 2U);

                buffer[0U] = modem::TAG_DATA;
                buffer[1U] = 0x00U;

                // generate Sync
                Sync::addP25Sync(buffer + 2U);

                // generate NID
                m_nid->encode(buffer + 2U, DUID::TSDU);

                // regenerate TSDU Data
                tsbk.setLastBlock(true); // always set last block -- this a Single Block TSDU
                tsbk.encode(buffer + 2U);

                // add status bits
                P25Utils::addStatusBits(buffer + 2U, P25_TSDU_FRAME_LENGTH_BYTES, false, true);
                P25Utils::addIdleStatusBits(buffer + 2U, P25_TSDU_FRAME_LENGTH_BYTES);
                P25Utils::setStatusBitsStartIdle(buffer + 2U);

                storeConvertedRx(buffer, P25_TSDU_FRAME_LENGTH_BYTES + 2U);
            }
        }
        break;

        // The remaining LDUs all create full rate voice frames so we do that here
        default:
        {
            MotFullRateVoice voice = MotFullRateVoice(dfsiData);

            if (m_debug) {
                LogDebugEx(LOG_MODEM, "ModemV24::convertToAir()", "Full Rate Voice, frameType = %x, source = %u", voice.getFrameType(), voice.getSource());
                Utils::dump(1U, "Full Rate Voice IMBE", voice.imbeData, RAW_IMBE_LENGTH_BYTES);
            }

            switch (frameType) {
                case DFSIFrameType::LDU1_VOICE2:
                {
                    ::memcpy(m_rxCall->netLDU1 + 26U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                }
                break;
                case DFSIFrameType::LDU1_VOICE3:
                {
                    ::memcpy(m_rxCall->netLDU1 + 55U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                    if (voice.additionalData != nullptr) {
                        m_rxCall->lco = voice.additionalData[0U];
                        m_rxCall->mfId = voice.additionalData[1U];
                        m_rxCall->serviceOptions = voice.additionalData[2U];
                    } else {
                        LogWarning(LOG_MODEM, "V.24/DFSI VC3 traffic missing metadata");
                    }
                }
                break;
                case DFSIFrameType::LDU1_VOICE4:
                {
                    ::memcpy(m_rxCall->netLDU1 + 80U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                    if (voice.additionalData != nullptr) {
                        m_rxCall->dstId = GET_UINT24(voice.additionalData, 0U);
                    } else {
                        LogWarning(LOG_MODEM, "V.24/DFSI VC4 traffic missing metadata");
                    }
                }
                break;
                case DFSIFrameType::LDU1_VOICE5:
                {
                    ::memcpy(m_rxCall->netLDU1 + 105U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                    if (voice.additionalData != nullptr) {
                        m_rxCall->srcId = GET_UINT24(voice.additionalData, 0U);
                    } else {
                        LogWarning(LOG_MODEM, "V.24/DFSI VC5 traffic missing metadata");
                    }
                }
                break;
                case DFSIFrameType::LDU1_VOICE6:
                {
                    ::memcpy(m_rxCall->netLDU1 + 130U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                }
                break;
                case DFSIFrameType::LDU1_VOICE7:
                {
                    ::memcpy(m_rxCall->netLDU1 + 155U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                }
                break;
                case DFSIFrameType::LDU1_VOICE8:
                {
                    ::memcpy(m_rxCall->netLDU1 + 180U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                }
                break;
                case DFSIFrameType::LDU1_VOICE9:
                {
                    ::memcpy(m_rxCall->netLDU1 + 204U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                    if (voice.additionalData != nullptr) {
                        m_rxCall->lsd1 = voice.additionalData[0U];
                        m_rxCall->lsd2 = voice.additionalData[1U];
                    } else {
                        LogWarning(LOG_MODEM, "V.24/DFSI VC9 traffic missing metadata");
                    }
                }
                break;
                case DFSIFrameType::LDU2_VOICE11:
                {
                    ::memcpy(m_rxCall->netLDU2 + 26U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                }
                break;
                case DFSIFrameType::LDU2_VOICE12:
                {
                    ::memcpy(m_rxCall->netLDU2 + 55U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                    if (voice.additionalData != nullptr) {
                        ::memcpy(m_rxCall->MI, voice.additionalData, 3U);
                    } else {
                        LogWarning(LOG_MODEM, "V.24/DFSI VC12 traffic missing metadata");
                    }
                }
                break;
                case DFSIFrameType::LDU2_VOICE13:
                {
                    ::memcpy(m_rxCall->netLDU2 + 80U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                    if (voice.additionalData != nullptr) {
                        ::memcpy(m_rxCall->MI + 3U, voice.additionalData, 3U);
                    } else {
                        LogWarning(LOG_MODEM, "V.24/DFSI VC13 traffic missing metadata");
                    }
                }
                break;
                case DFSIFrameType::LDU2_VOICE14:
                {
                    ::memcpy(m_rxCall->netLDU2 + 105U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                    if (voice.additionalData != nullptr) {
                        ::memcpy(m_rxCall->MI + 6U, voice.additionalData, 3U);
                    } else {
                        LogWarning(LOG_MODEM, "V.24/DFSI VC14 traffic missing metadata");
                    }
                }
                break;
                case DFSIFrameType::LDU2_VOICE15:
                {
                    ::memcpy(m_rxCall->netLDU2 + 130U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                    if (voice.additionalData != nullptr) {
                        m_rxCall->algoId = voice.additionalData[0U];
                        m_rxCall->kId = GET_UINT16(voice.additionalData, 1U);
                    } else {
                        LogWarning(LOG_MODEM, "V.24/DFSI VC15 traffic missing metadata");
                    }
                }
                break;
                case DFSIFrameType::LDU2_VOICE16:
                {
                    ::memcpy(m_rxCall->netLDU2 + 155U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                }
                break;
                case DFSIFrameType::LDU2_VOICE17:
                {
                    ::memcpy(m_rxCall->netLDU2 + 180U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                }
                break;
                case DFSIFrameType::LDU2_VOICE18:
                {
                    ::memcpy(m_rxCall->netLDU2 + 204U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                    if (voice.additionalData != nullptr) {
                        m_rxCall->lsd1 = voice.additionalData[0U];
                        m_rxCall->lsd2 = voice.additionalData[1U];
                    } else {
                        LogWarning(LOG_MODEM, "V.24/DFSI VC18 traffic missing metadata");
                    }
                }
                break;

                default:
                    break;
            }

            // increment our voice frame counter
            m_rxCall->n++;
        }
        break;
    }

    // encode LDU1 if ready
    if (m_rxCall->n == 9U) {
        lc::LC lc = lc::LC();
        lc.setLCO(m_rxCall->lco);
        lc.setMFId(m_rxCall->mfId);

        if (lc.isStandardMFId()) {
            lc.setSrcId(m_rxCall->srcId);
            lc.setDstId(m_rxCall->dstId);
        } else {
            uint8_t rsBuffer[P25_LDU_LC_FEC_LENGTH_BYTES];
            ::memset(rsBuffer, 0x00U, P25_LDU_LC_FEC_LENGTH_BYTES);

            rsBuffer[0U] = m_rxCall->lco;
            rsBuffer[1U] = m_rxCall->mfId;
            rsBuffer[2U] = m_rxCall->serviceOptions;
            rsBuffer[3U] = (m_rxCall->dstId >> 16) & 0xFFU;
            rsBuffer[4U] = (m_rxCall->dstId >> 8) & 0xFFU;
            rsBuffer[5U] = (m_rxCall->dstId >> 0) & 0xFFU;
            rsBuffer[6U] = (m_rxCall->srcId >> 16) & 0xFFU;
            rsBuffer[7U] = (m_rxCall->srcId >> 8) & 0xFFU;
            rsBuffer[8U] = (m_rxCall->srcId >> 0) & 0xFFU;

            // combine bytes into ulong64_t (8 byte) value
            ulong64_t rsValue = 0U;
            rsValue = rsBuffer[1U];
            rsValue = (rsValue << 8) + rsBuffer[2U];
            rsValue = (rsValue << 8) + rsBuffer[3U];
            rsValue = (rsValue << 8) + rsBuffer[4U];
            rsValue = (rsValue << 8) + rsBuffer[5U];
            rsValue = (rsValue << 8) + rsBuffer[6U];
            rsValue = (rsValue << 8) + rsBuffer[7U];
            rsValue = (rsValue << 8) + rsBuffer[8U];

            lc.setRS(rsValue);
        }

        bool emergency = ((m_rxCall->serviceOptions & 0xFFU) & 0x80U) == 0x80U;    // Emergency Flag
        bool encryption = ((m_rxCall->serviceOptions & 0xFFU) & 0x40U) == 0x40U;   // Encryption Flag
        uint8_t priority = ((m_rxCall->serviceOptions & 0xFFU) & 0x07U);           // Priority
        lc.setEmergency(emergency);
        lc.setEncrypted(encryption);
        lc.setPriority(priority);

        data::LowSpeedData lsd = data::LowSpeedData();
        lsd.setLSD1(m_rxCall->lsd1);
        lsd.setLSD2(m_rxCall->lsd2);

        // generate Sync
        Sync::addP25Sync(buffer + 2U);

        // generate NID
        m_nid->encode(buffer + 2U, DUID::LDU1);

        // generate LDU1 Data
        lc.encodeLDU1(buffer + 2U);

        // generate Low Speed Data
        lsd.process(buffer + 2U);

        // generate audio
        m_audio.encode(buffer + 2U, m_rxCall->netLDU1 + 10U, 0U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU1 + 26U, 1U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU1 + 55U, 2U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU1 + 80U, 3U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU1 + 105U, 4U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU1 + 130U, 5U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU1 + 155U, 6U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU1 + 180U, 7U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU1 + 204U, 8U);

        // add status bits
        P25Utils::addStatusBits(buffer + 2U, P25_LDU_FRAME_LENGTH_BITS, true, false);

        buffer[0U] = modem::TAG_DATA;
        buffer[1U] = 0x01U;
        storeConvertedRx(buffer, P25_LDU_FRAME_LENGTH_BYTES + 2U);
    }
    
    // encode LDU2 if ready
    if (m_rxCall->n == 18U) {
        lc::LC lc = lc::LC();
        lc.setMI(m_rxCall->MI);
        lc.setAlgId(m_rxCall->algoId);
        lc.setKId(m_rxCall->kId);

        data::LowSpeedData lsd = data::LowSpeedData();
        lsd.setLSD1(m_rxCall->lsd1);
        lsd.setLSD2(m_rxCall->lsd2);

        // generate Sync
        Sync::addP25Sync(buffer + 2U);

        // generate NID
        m_nid->encode(buffer + 2U, DUID::LDU2);

        // generate LDU2 data
        lc.encodeLDU2(buffer + 2U);

        // generate Low Speed Data
        lsd.process(buffer + 2U);

        // generate audio
        m_audio.encode(buffer + 2U, m_rxCall->netLDU2 + 10U, 0U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU2 + 26U, 1U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU2 + 55U, 2U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU2 + 80U, 3U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU2 + 105U, 4U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU2 + 130U, 5U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU2 + 155U, 6U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU2 + 180U, 7U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU2 + 204U, 8U);

        // add status bits
        P25Utils::addStatusBits(buffer + 2U, P25_LDU_FRAME_LENGTH_BITS, true, false);

        buffer[0U] = modem::TAG_DATA;
        buffer[1U] = 0x01U;
        storeConvertedRx(buffer, P25_LDU_FRAME_LENGTH_BYTES + 2U);

        m_rxCall->n = 0;
    }
}

/* Internal helper to convert from TIA-102 DFSI to TIA-102 air interface. */

void ModemV24::convertToAirTIA(const uint8_t *data, uint32_t length)
{
    assert(data != nullptr);
    assert(length > 0U);

    uint8_t buffer[P25_PDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(buffer, 0x00U, P25_PDU_FRAME_LENGTH_BYTES + 2U);

    // get the DFSI data (skip the 0x00 padded byte at the start)
    DECLARE_UINT8_ARRAY(dfsiData, length - 1U);
    ::memcpy(dfsiData, data + 1U, length - 1U);

    if (m_debug)
        Utils::dump("DFSI RX data from UDP", dfsiData, length - 1U);

    ControlOctet ctrl = ControlOctet();
    ctrl.decode(dfsiData);

    uint8_t blockCnt = ctrl.getBlockHeaderCnt();

    if (m_debug)
        ::LogDebugEx(LOG_MODEM, "ModemV24::convertToAirTIA()", "blockCnt = %u", blockCnt);

    // iterate through blocks
    uint8_t hdrOffs = 1U, dataOffs = blockCnt + 1U;
    for (uint8_t i = 0U; i < blockCnt; i++) {
        BlockHeader hdr = BlockHeader();
        hdr.decode(dfsiData + hdrOffs);

        if (m_debug)
            ::LogDebugEx(LOG_MODEM, "ModemV24::convertToAirTIA()", "block = %u, blockType = $%02X", i, hdr.getBlockType());

        BlockType::E blockType = hdr.getBlockType();
        switch (blockType) {
        case BlockType::START_OF_STREAM:
        {
            StartOfStream start = StartOfStream();
            start.decode(dfsiData + dataOffs);

            uint16_t nac = ((start.getNID() & 0xFFFFU) >> 4) & 0xFFFU;
            if (m_debug)
                ::LogDebugEx(LOG_MODEM, "ModemV24::convertToAirTIA()", "Start of Stream, nac = $%03X, errs: %u", nac, start.getErrorCount());

            // bryanb: maybe compare the NACs?

            dataOffs += StartOfStream::LENGTH;

            // ack start of stream
            ackStartOfStreamTIA(); 
        }
        break;
        case BlockType::END_OF_STREAM:
        {
            dataOffs += 1U;

            // generate Sync
            Sync::addP25Sync(buffer + 2U);

            // generate NID
            m_nid->encode(buffer + 2U, DUID::TDU);

            // add status bits
            P25Utils::setStatusBitsAllIdle(buffer + 2U, P25_TDU_FRAME_LENGTH_BITS);

            buffer[0U] = modem::TAG_DATA;
            buffer[1U] = 0x01U;
            storeConvertedRx(buffer, P25_TDU_FRAME_LENGTH_BITS + 2U);
        }
        break;

        case BlockType::START_OF_STREAM_ACK:
        {
            if (m_debug)
                ::LogDebugEx(LOG_MODEM, "ModemV24::convertToAirTIA()", "Start of Stream ACK");

            // do nothing with the start of stream ack
        }
        break;

        case BlockType::VOICE_HEADER_P1:
        {
            // copy to call data VHDR1
            ::memset(m_rxCall->VHDR1, 0x00U, 18U);
            ::memcpy(m_rxCall->VHDR1, dfsiData + dataOffs + 1U, 18U);

            dataOffs += 19U; // 18 Golay + Block Type Marker
        }
        break;
        case BlockType::VOICE_HEADER_P2:
        {
            // copy to call data VHDR2
            ::memset(m_rxCall->VHDR2, 0x00U, 18U);
            ::memcpy(m_rxCall->VHDR2, dfsiData + dataOffs + 1U, 18U);

            dataOffs += 19U; // 18 Golay + Block Type Marker

            // buffer for raw VHDR data
            uint8_t raw[DFSI_VHDR_RAW_LEN];
            ::memset(raw, 0x00U, DFSI_VHDR_RAW_LEN);

            ::memcpy(raw, m_rxCall->VHDR1, 18U);
            ::memcpy(raw + 18U, m_rxCall->VHDR2, 18U);

            assert(raw != nullptr);

            // buffer for decoded VHDR data
            uint8_t vhdr[P25_HDU_LENGTH_BYTES];
            ::memset(vhdr, 0x00U, P25_HDU_LENGTH_BYTES);

            assert(vhdr != nullptr);

            uint32_t offset = 6U; // skip the first 6 bits  (extremely strange bit offset for TIA because of status bits)
            for (uint32_t i = 0; i < DFSI_VHDR_RAW_LEN; i++, offset += 6) {
                Utils::hex2Bin(raw[i], vhdr, offset);
            }

            // try to decode the RS data
            try {
                bool ret = m_rs.decode362017(vhdr);
                if (!ret) {
                    LogError(LOG_MODEM, "V.24/DFSI traffic failed to decode RS (36,20,17) FEC");
                } else {
                    // late entry?
                    if (!m_rxCallInProgress) {
                        m_rxCallInProgress = true;
                        m_rxCall->resetCallData();
                        if (m_debug)
                            LogDebug(LOG_MODEM, "V24 RX VHDR late entry, resetting call data");
                    }

                    ::memcpy(m_rxCall->MI, vhdr, MI_LENGTH_BYTES);

                    m_rxCall->mfId = vhdr[9U];
                    m_rxCall->algoId = vhdr[10U];
                    m_rxCall->kId = GET_UINT16(vhdr, 11U);
                    m_rxCall->dstId = GET_UINT16(vhdr, 13U);

                    if (m_debug) {
                        LogDebug(LOG_MODEM, "P25, VHDR algId = $%02X, kId = $%04X, dstId = $%04X", m_rxCall->algoId, m_rxCall->kId, m_rxCall->dstId);
                    }

                    // generate a HDU
                    lc::LC lc = lc::LC();
                    lc.setDstId(m_rxCall->dstId);
                    lc.setAlgId(m_rxCall->algoId);
                    lc.setKId(m_rxCall->kId);
                    lc.setMI(m_rxCall->MI);

                    // generate Sync
                    Sync::addP25Sync(buffer + 2U);

                    // generate NID
                    m_nid->encode(buffer + 2U, DUID::HDU);

                    // generate HDU
                    lc.encodeHDU(buffer + 2U);

                    // add status bits
                    P25Utils::addStatusBits(buffer + 2U, P25_HDU_FRAME_LENGTH_BITS, true, false);

                    buffer[0U] = modem::TAG_DATA;
                    buffer[1U] = 0x01U;
                    storeConvertedRx(buffer, P25_HDU_FRAME_LENGTH_BYTES + 2U);
                }
            }
            catch (...) {
                LogError(LOG_MODEM, "V.24/DFSI RX traffic got exception while trying to decode RS data for VHDR");
            }
        }
        break;
    
        case BlockType::FULL_RATE_VOICE:
        {
            FullRateVoice voice = FullRateVoice();
            //m_superFrameCnt = voice.getSuperframeCnt();
            voice.decode(dfsiData + dataOffs);

            if (m_debug) {
                LogDebugEx(LOG_MODEM, "ModemV24::convertToAirTIA()", "Full Rate Voice, frameType = %x, busy = %u, lostFrame = %u, muteFrame = %u, superFrameCnt = %u", voice.getFrameType(), voice.getBusy(), voice.getLostFrame(), voice.getMuteFrame(), voice.getSuperframeCnt(), voice.getTotalErrors());
                Utils::dump(1U, "Full Rate Voice IMBE", voice.imbeData, RAW_IMBE_LENGTH_BYTES);
            }

            dataOffs += voice.getLength();

            DFSIFrameType::E frameType = voice.getFrameType();
            switch (frameType) {
            case DFSIFrameType::LDU1_VOICE1:
            {
                ::memcpy(m_rxCall->netLDU1 + 10U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
            }
            break;
            case DFSIFrameType::LDU1_VOICE2:
            {
                ::memcpy(m_rxCall->netLDU1 + 26U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
            }
            break;
            case DFSIFrameType::LDU1_VOICE3:
            {
                ::memcpy(m_rxCall->netLDU1 + 55U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                if (voice.additionalData != nullptr) {
                    m_rxCall->lco = voice.additionalData[0U];
                    m_rxCall->mfId = voice.additionalData[1U];
                    m_rxCall->serviceOptions = voice.additionalData[2U];
                } else {
                    LogWarning(LOG_MODEM, "V.24/DFSI VC3 traffic missing metadata");
                }
            }
            break;
            case DFSIFrameType::LDU1_VOICE4:
            {
                ::memcpy(m_rxCall->netLDU1 + 80U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                if (voice.additionalData != nullptr) {
                    m_rxCall->dstId = GET_UINT24(voice.additionalData, 0U);
                } else {
                    LogWarning(LOG_MODEM, "V.24/DFSI VC4 traffic missing metadata");
                }
            }
            break;
            case DFSIFrameType::LDU1_VOICE5:
            {
                ::memcpy(m_rxCall->netLDU1 + 105U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                if (voice.additionalData != nullptr) {
                    m_rxCall->srcId = GET_UINT24(voice.additionalData, 0U);
                } else {
                    LogWarning(LOG_MODEM, "V.24/DFSI VC5 traffic missing metadata");
                }
            }
            break;
            case DFSIFrameType::LDU1_VOICE6:
            {
                ::memcpy(m_rxCall->netLDU1 + 130U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
            }
            break;
            case DFSIFrameType::LDU1_VOICE7:
            {
                ::memcpy(m_rxCall->netLDU1 + 155U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
            }
            break;
            case DFSIFrameType::LDU1_VOICE8:
            {
                ::memcpy(m_rxCall->netLDU1 + 180U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
            }
            break;
            case DFSIFrameType::LDU1_VOICE9:
            {
                ::memcpy(m_rxCall->netLDU1 + 204U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                if (voice.additionalData != nullptr) {
                    m_rxCall->lsd1 = voice.additionalData[0U];
                    m_rxCall->lsd2 = voice.additionalData[1U];
                } else {
                    LogWarning(LOG_MODEM, "V.24/DFSI VC9 traffic missing metadata");
                }
            }
            break;

            case DFSIFrameType::LDU2_VOICE10:
            {
                ::memcpy(m_rxCall->netLDU2 + 10U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
            }
            break;
            case DFSIFrameType::LDU2_VOICE11:
            {
                ::memcpy(m_rxCall->netLDU2 + 26U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
            }
            break;
            case DFSIFrameType::LDU2_VOICE12:
            {
                ::memcpy(m_rxCall->netLDU2 + 55U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                if (voice.additionalData != nullptr) {
                    ::memcpy(m_rxCall->MI, voice.additionalData, 3U);
                } else {
                    LogWarning(LOG_MODEM, "V.24/DFSI VC12 traffic missing metadata");
                }
            }
            break;
            case DFSIFrameType::LDU2_VOICE13:
            {
                ::memcpy(m_rxCall->netLDU2 + 80U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                if (voice.additionalData != nullptr) {
                    ::memcpy(m_rxCall->MI + 3U, voice.additionalData, 3U);
                } else {
                    LogWarning(LOG_MODEM, "V.24/DFSI VC13 traffic missing metadata");
                }
            }
            break;
            case DFSIFrameType::LDU2_VOICE14:
            {
                ::memcpy(m_rxCall->netLDU2 + 105U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                if (voice.additionalData != nullptr) {
                    ::memcpy(m_rxCall->MI + 6U, voice.additionalData, 3U);
                } else {
                    LogWarning(LOG_MODEM, "V.24/DFSI VC14 traffic missing metadata");
                }
            }
            break;
            case DFSIFrameType::LDU2_VOICE15:
            {
                ::memcpy(m_rxCall->netLDU2 + 130U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                if (voice.additionalData != nullptr) {
                    m_rxCall->algoId = voice.additionalData[0U];
                    m_rxCall->kId = GET_UINT16(voice.additionalData, 1U);
                } else {
                    LogWarning(LOG_MODEM, "V.24/DFSI VC15 traffic missing metadata");
                }
            }
            break;
            case DFSIFrameType::LDU2_VOICE16:
            {
                ::memcpy(m_rxCall->netLDU2 + 155U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
            }
            break;
            case DFSIFrameType::LDU2_VOICE17:
            {
                ::memcpy(m_rxCall->netLDU2 + 180U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
            }
            break;
            case DFSIFrameType::LDU2_VOICE18:
            {
                ::memcpy(m_rxCall->netLDU2 + 204U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                if (voice.additionalData != nullptr) {
                    m_rxCall->lsd1 = voice.additionalData[0U];
                    m_rxCall->lsd2 = voice.additionalData[1U];
                } else {
                    LogWarning(LOG_MODEM, "V.24/DFSI VC18 traffic missing metadata");
                }
            }
            break;

            default:
                break;
            }

            // increment our voice frame counter
            m_rxCall->n++;
        }
        break;
        
        default:
            break;
        }

        hdrOffs += BlockHeader::LENGTH;
    }

    m_rxLastFrameTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // encode LDU1 if ready
    if (m_rxCall->n == 9U) {
        lc::LC lc = lc::LC();
        lc.setLCO(m_rxCall->lco);
        lc.setMFId(m_rxCall->mfId);

        if (lc.isStandardMFId()) {
            lc.setSrcId(m_rxCall->srcId);
            lc.setDstId(m_rxCall->dstId);
        } else {
            uint8_t rsBuffer[P25_LDU_LC_FEC_LENGTH_BYTES];
            ::memset(rsBuffer, 0x00U, P25_LDU_LC_FEC_LENGTH_BYTES);

            rsBuffer[0U] = m_rxCall->lco;
            rsBuffer[1U] = m_rxCall->mfId;
            rsBuffer[2U] = m_rxCall->serviceOptions;
            rsBuffer[3U] = (m_rxCall->dstId >> 16) & 0xFFU;
            rsBuffer[4U] = (m_rxCall->dstId >> 8) & 0xFFU;
            rsBuffer[5U] = (m_rxCall->dstId >> 0) & 0xFFU;
            rsBuffer[6U] = (m_rxCall->srcId >> 16) & 0xFFU;
            rsBuffer[7U] = (m_rxCall->srcId >> 8) & 0xFFU;
            rsBuffer[8U] = (m_rxCall->srcId >> 0) & 0xFFU;

            // combine bytes into ulong64_t (8 byte) value
            ulong64_t rsValue = 0U;
            rsValue = rsBuffer[1U];
            rsValue = (rsValue << 8) + rsBuffer[2U];
            rsValue = (rsValue << 8) + rsBuffer[3U];
            rsValue = (rsValue << 8) + rsBuffer[4U];
            rsValue = (rsValue << 8) + rsBuffer[5U];
            rsValue = (rsValue << 8) + rsBuffer[6U];
            rsValue = (rsValue << 8) + rsBuffer[7U];
            rsValue = (rsValue << 8) + rsBuffer[8U];

            lc.setRS(rsValue);
        }

        bool emergency = ((m_rxCall->serviceOptions & 0xFFU) & 0x80U) == 0x80U;    // Emergency Flag
        bool encryption = ((m_rxCall->serviceOptions & 0xFFU) & 0x40U) == 0x40U;   // Encryption Flag
        uint8_t priority = ((m_rxCall->serviceOptions & 0xFFU) & 0x07U);           // Priority
        lc.setEmergency(emergency);
        lc.setEncrypted(encryption);
        lc.setPriority(priority);

        data::LowSpeedData lsd = data::LowSpeedData();
        lsd.setLSD1(m_rxCall->lsd1);
        lsd.setLSD2(m_rxCall->lsd2);

        // generate Sync
        Sync::addP25Sync(buffer + 2U);

        // generate NID
        m_nid->encode(buffer + 2U, DUID::LDU1);

        // generate LDU1 Data
        lc.encodeLDU1(buffer + 2U);

        // generate Low Speed Data
        lsd.process(buffer + 2U);

        // generate audio
        m_audio.encode(buffer + 2U, m_rxCall->netLDU1 + 10U, 0U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU1 + 26U, 1U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU1 + 55U, 2U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU1 + 80U, 3U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU1 + 105U, 4U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU1 + 130U, 5U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU1 + 155U, 6U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU1 + 180U, 7U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU1 + 204U, 8U);

        // add status bits
        P25Utils::addStatusBits(buffer + 2U, P25_LDU_FRAME_LENGTH_BITS, true, false);

        buffer[0U] = modem::TAG_DATA;
        buffer[1U] = 0x01U;
        storeConvertedRx(buffer, P25_LDU_FRAME_LENGTH_BYTES + 2U);
    }
    
    // encode LDU2 if ready
    if (m_rxCall->n == 18U) {
        lc::LC lc = lc::LC();
        lc.setMI(m_rxCall->MI);
        lc.setAlgId(m_rxCall->algoId);
        lc.setKId(m_rxCall->kId);

        data::LowSpeedData lsd = data::LowSpeedData();
        lsd.setLSD1(m_rxCall->lsd1);
        lsd.setLSD2(m_rxCall->lsd2);

        // generate Sync
        Sync::addP25Sync(buffer + 2U);

        // generate NID
        m_nid->encode(buffer + 2U, DUID::LDU2);

        // generate LDU2 data
        lc.encodeLDU2(buffer + 2U);

        // generate Low Speed Data
        lsd.process(buffer + 2U);

        // generate audio
        m_audio.encode(buffer + 2U, m_rxCall->netLDU2 + 10U, 0U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU2 + 26U, 1U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU2 + 55U, 2U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU2 + 80U, 3U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU2 + 105U, 4U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU2 + 130U, 5U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU2 + 155U, 6U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU2 + 180U, 7U);
        m_audio.encode(buffer + 2U, m_rxCall->netLDU2 + 204U, 8U);

        // add status bits
        P25Utils::addStatusBits(buffer + 2U, P25_LDU_FRAME_LENGTH_BITS, true, false);

        buffer[0U] = modem::TAG_DATA;
        buffer[1U] = 0x01U;
        storeConvertedRx(buffer, P25_LDU_FRAME_LENGTH_BYTES + 2U);

        m_rxCall->n = 0;
    }
}

/* Helper to add a V.24 data frame to the P25 TX queue with the proper timestamp and formatting */

void ModemV24::queueP25Frame(uint8_t* data, uint16_t len, SERIAL_TX_TYPE msgType)
{
    assert(data != nullptr);
    assert(len > 0U);

    if (m_debug)
        LogDebugEx(LOG_MODEM, "ModemV24::queueP25Frame()", "msgType = $%02X", msgType);
    if (m_trace)
        Utils::dump(1U, "[ModemV24::queueP25Frame()] data", data, len);

    // get current time in ms
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // timestamp for this message (in ms)
    uint64_t msgTime = 0U;

    // if this is our first message, timestamp is just now + the jitter buffer offset in ms
    if (m_lastP25Tx == 0U) {
        msgTime = now + m_jitter;

        // if the message type requests no jitter delay -- just set the message time to now
        if (msgType == STT_NON_IMBE_NO_JITTER)
            msgTime = now;
    }
    // if we had a message before this, calculate the new timestamp dynamically
    else {
        // if the last message occurred longer than our jitter buffer delay, we restart the sequence and calculate the same as above
        if ((int64_t)(now - m_lastP25Tx) > m_jitter) {
            msgTime = now + m_jitter;
        }
        // otherwise, we time out messages as required by the message type
        else {
            if (msgType == STT_IMBE) {
                // IMBEs must go out at 20ms intervals
                msgTime = m_lastP25Tx + 20U;
            } else {
                // Otherwise we don't care, we use 5ms since that's the theoretical minimum time a 9600 baud message can take
                msgTime = m_lastP25Tx + 5U;
            }
        }
    }

    len += 4U;

    // convert 16-bit length to 2 bytes
    uint8_t length[2U];
    if (len > 255U)
        length[0U] = (len >> 8U) & 0xFFU;
    else
        length[0U] = 0x00U;
    length[1U] = len & 0xFFU;

    m_txP25Queue.addData(length, 2U);

    // add the data tag
    uint8_t tag = TAG_DATA;
    m_txP25Queue.addData(&tag, 1U);

    // convert 64-bit timestamp to 8 bytes and add
    uint8_t tsBytes[8U];
    assert(sizeof msgTime == 8U);
    ::memcpy(tsBytes, &msgTime, 8U);
    m_txP25Queue.addData(tsBytes, 8U);

    // add the DVM start byte, length byte, CMD byte, and padding 0
    uint8_t header[4U];
    header[0U] = DVM_SHORT_FRAME_START;
    header[1U] = len & 0xFFU;
    header[2U] = CMD_P25_DATA;
    header[3U] = 0x00U;
    m_txP25Queue.addData(header, 4U);

    // add the data
    m_txP25Queue.addData(data, len - 4U);

    // update the last message time
    m_lastP25Tx = msgTime;
}

/* Send a start of stream sequence (HDU, etc) to the connected serial V.24 device */

void ModemV24::startOfStream(const p25::lc::LC& control)
{
    m_txCallInProgress = true;

    MotStartOfStream start = MotStartOfStream();
    start.setStartStop(StartStopFlag::START);
    start.setRT(m_rtrt ? RTFlag::ENABLED : RTFlag::DISABLED);

    // create buffer for bytes and encode
    uint8_t startBuf[start.LENGTH];
    ::memset(startBuf, 0x00U, start.LENGTH);
    start.encode(startBuf);

    if (m_trace)
        Utils::dump(1U, "ModemV24::startOfStream() MotStartOfStream", startBuf, MotStartOfStream::LENGTH);

    queueP25Frame(startBuf, MotStartOfStream::LENGTH, STT_NON_IMBE);

    uint8_t mi[MI_LENGTH_BYTES];
    ::memset(mi, 0x00U, MI_LENGTH_BYTES);
    control.getMI(mi);

    uint8_t vhdr[DFSI_VHDR_LEN];
    ::memset(vhdr, 0x00U, DFSI_VHDR_LEN);

    ::memcpy(vhdr, mi, MI_LENGTH_BYTES);

    vhdr[9U] = control.getMFId();
    vhdr[10U] = control.getAlgId();
    SET_UINT16(control.getKId(), vhdr, 11U);
    SET_UINT16(control.getDstId(), vhdr, 13U);

    // perform RS encoding
    m_rs.encode362017(vhdr);

    // convert the binary bytes to hex bytes
    uint8_t raw[DFSI_VHDR_RAW_LEN];
    uint32_t offset = 0;
    for (uint8_t i = 0; i < DFSI_VHDR_RAW_LEN; i++, offset += 6) {
        raw[i] = Utils::bin2Hex(vhdr, offset);
    }

    // prepare VHDR1
    MotVoiceHeader1 vhdr1 = MotVoiceHeader1();
    vhdr1.startOfStream->setStartStop(StartStopFlag::START);
    vhdr1.startOfStream->setRT(m_rtrt ? RTFlag::ENABLED : RTFlag::DISABLED);
    vhdr1.setICW(m_diu ? ICWFlag::DIU : ICWFlag::QUANTAR);

    ::memcpy(vhdr1.header, raw, 8U);
    ::memcpy(vhdr1.header + 9U, raw + 8U, 8U);
    ::memcpy(vhdr1.header + 18U, raw + 16U, 2U);

    // encode VHDR1 and send
    uint8_t vhdr1Buf[vhdr1.LENGTH];
    ::memset(vhdr1Buf, 0x00U, vhdr1.LENGTH);
    vhdr1.encode(vhdr1Buf);

    if (m_trace)
        Utils::dump(1U, "ModemV24::startOfStream() MotVoiceHeader1", vhdr1Buf, MotVoiceHeader1::LENGTH);

    queueP25Frame(vhdr1Buf, MotVoiceHeader1::LENGTH, STT_NON_IMBE);

    // prepare VHDR2
    MotVoiceHeader2 vhdr2 = MotVoiceHeader2();
    ::memcpy(vhdr2.header, raw + 18U, 8U);
    ::memcpy(vhdr2.header + 9U, raw + 26U, 8U);
    ::memcpy(vhdr2.header + 18U, raw + 34U, 2U);

    // encode VHDR2 and send
    uint8_t vhdr2Buf[vhdr2.LENGTH];
    ::memset(vhdr2Buf, 0x00U, vhdr2.LENGTH);
    vhdr2.encode(vhdr2Buf);

    if (m_trace)
        Utils::dump(1U, "ModemV24::startOfStream() MotVoiceHeader2", vhdr2Buf, MotVoiceHeader2::LENGTH);

    queueP25Frame(vhdr2Buf, MotVoiceHeader2::LENGTH, STT_NON_IMBE);
}

/* Send an end of stream sequence (TDU, etc) to the connected serial V.24 device */

void ModemV24::endOfStream()
{
    MotStartOfStream end = MotStartOfStream();
    end.setStartStop(StartStopFlag::STOP);

    // create buffer and encode
    uint8_t endBuf[MotStartOfStream::LENGTH];
    ::memset(endBuf, 0x00U, MotStartOfStream::LENGTH);
    end.encode(endBuf);

    if (m_trace)
        Utils::dump(1U, "ModemV24::endOfStream() MotStartOfStream", endBuf, MotStartOfStream::LENGTH);

    queueP25Frame(endBuf, MotStartOfStream::LENGTH, STT_NON_IMBE);

    m_txCallInProgress = false;
}

/* Helper to generate the NID value. */

uint16_t ModemV24::generateNID(DUID::E duid)
{
    uint8_t nid[2U];
    ::memset(nid, 0x00U, 2U);

    nid[0U] = (m_p25NAC >> 4) & 0xFFU;
    nid[1U] = (m_p25NAC << 4) & 0xF0U;
    nid[1U] |= duid;

    return GET_UINT16(nid, 0U);
}

/* Send a start of stream sequence (HDU, etc) to the connected UDP TIA-102 device. */

void ModemV24::startOfStreamTIA(const p25::lc::LC& control)
{
    m_txCallInProgress = true;
    m_superFrameCnt = 0U;

    p25::lc::LC lc = p25::lc::LC(control);
    
    uint16_t length = 0U;
    uint8_t buffer[P25_HDU_LENGTH_BYTES];
    ::memset(buffer, 0x00U, P25_HDU_LENGTH_BYTES);

    // generate control octet
    ControlOctet ctrl = ControlOctet();
    ctrl.setBlockHeaderCnt(1U);
    ctrl.encode(buffer);
    length += ControlOctet::LENGTH;

    // generate block header
    BlockHeader hdr = BlockHeader();
    hdr.setBlockType(BlockType::START_OF_STREAM);
    hdr.encode(buffer + 1U);
    length += BlockHeader::LENGTH;

    // generate start of stream
    StartOfStream start = StartOfStream();
    start.setNID(generateNID());
    start.encode(buffer + 2U);
    length += StartOfStream::LENGTH;

    if (m_trace)
        Utils::dump(1U, "ModemV24::startOfStreamTIA() StartOfStream", buffer, length);

    queueP25Frame(buffer, length, STT_NON_IMBE);

    uint8_t mi[MI_LENGTH_BYTES];
    ::memset(mi, 0x00U, MI_LENGTH_BYTES);
    control.getMI(mi);

    uint8_t vhdr[P25_HDU_LENGTH_BYTES];
    ::memset(vhdr, 0x00U, P25_HDU_LENGTH_BYTES);

    ::memcpy(vhdr, mi, MI_LENGTH_BYTES);

    vhdr[9U] = control.getMFId();
    vhdr[10U] = control.getAlgId();
    SET_UINT16(control.getKId(), vhdr, 11U);
    SET_UINT16(control.getDstId(), vhdr, 13U);

    // perform RS encoding
    m_rs.encode362017(vhdr);

    // convert the binary bytes to hex bytes
    uint8_t raw[DFSI_VHDR_RAW_LEN];
    uint32_t offset = 6U; // skip the first 6 bits (extremely strange bit offset for TIA because of status bits)
    for (uint8_t i = 0; i < DFSI_VHDR_RAW_LEN; i++, offset += 6) {
        raw[i] = Utils::bin2Hex(vhdr, offset);
    }

    ::memset(buffer, 0x00U, P25_HDU_LENGTH_BYTES);
    length = 0U;

    // generate control octet
    ctrl.setBlockHeaderCnt(2U);
    ctrl.encode(buffer);
    length += ControlOctet::LENGTH;

    // generate block header 1
    hdr.setBlockType(BlockType::VOICE_HEADER_P1);
    hdr.encode(buffer + 1U);
    length += BlockHeader::LENGTH;
    hdr.setBlockType(BlockType::START_OF_STREAM);
    hdr.encode(buffer + 2U);
    length += BlockHeader::LENGTH;

    // generate voice header 1
    uint8_t hdu[P25_HDU_LENGTH_BYTES];
    ::memset(hdu, 0x00U, P25_HDU_LENGTH_BYTES);
    lc.encodeHDU(hdu, true);

    // prepare VHDR1
    buffer[3U] = DFSIFrameType::MOT_VHDR_1;
    ::memcpy(buffer + 4U, raw, 18U);
    length += DFSI_TIA_VHDR_LEN; // 18 Golay + Block Type Marker

    start.encode(buffer + length);
    length += StartOfStream::LENGTH;

    if (m_trace)
        Utils::dump(1U, "ModemV24::startOfStreamTIA() VoiceHeader1", buffer, length);

    queueP25Frame(buffer, length, STT_NON_IMBE);

    ::memset(buffer, 0x00U, P25_HDU_LENGTH_BYTES);
    length = 0U;

    // generate control octet
    ctrl.setBlockHeaderCnt(2U);
    ctrl.encode(buffer);
    length += ControlOctet::LENGTH;

    // generate block header 1
    hdr.setBlockType(BlockType::VOICE_HEADER_P2);
    hdr.encode(buffer + 1U);
    length += BlockHeader::LENGTH;
    hdr.setBlockType(BlockType::START_OF_STREAM);
    hdr.encode(buffer + 2U);
    length += BlockHeader::LENGTH;

    // prepare VHDR2
    buffer[3U] = DFSIFrameType::MOT_VHDR_2;
    ::memcpy(buffer + 4U, raw + 18U, 18U);
    length += DFSI_TIA_VHDR_LEN; // 18 Golay + Block Type Marker

    start.encode(buffer + length);
    length += StartOfStream::LENGTH;

    if (m_trace)
        Utils::dump(1U, "ModemV24::startOfStreamTIA() VoiceHeader2", buffer, length);

    queueP25Frame(buffer, length, STT_NON_IMBE);
}

/* Send an end of stream sequence (TDU, etc) to the connected UDP TIA-102 device. */

void ModemV24::endOfStreamTIA()
{
    m_superFrameCnt = 0U;

    uint16_t length = 0U;
    uint8_t buffer[2U];
    ::memset(buffer, 0x00U, 2U);

    // generate control octet
    ControlOctet ctrl = ControlOctet();
    ctrl.setBlockHeaderCnt(1U);
    ctrl.encode(buffer);
    length += ControlOctet::LENGTH;

    // generate block header
    BlockHeader hdr = BlockHeader();
    hdr.setBlockType(BlockType::END_OF_STREAM);
    hdr.encode(buffer + 1U);
    length += BlockHeader::LENGTH;

    if (m_trace)
        Utils::dump(1U, "ModemV24::endOfStreamTIA() EndOfStream", buffer, length);

    queueP25Frame(buffer, length, STT_NON_IMBE);

    m_txCallInProgress = false;
}

/* Send a start of stream ACK. */

void ModemV24::ackStartOfStreamTIA()
{
    uint16_t length = 0U;
    uint8_t buffer[P25_HDU_LENGTH_BYTES];
    ::memset(buffer, 0x00U, P25_HDU_LENGTH_BYTES);

    // generate control octet
    ControlOctet ctrl = ControlOctet();
    ctrl.setBlockHeaderCnt(1U);
    ctrl.encode(buffer);
    length += ControlOctet::LENGTH;

    // generate block header
    BlockHeader hdr = BlockHeader();
    hdr.setBlockType(BlockType::START_OF_STREAM_ACK);
    hdr.encode(buffer + 1U);
    length += BlockHeader::LENGTH;

    if (m_trace)
        Utils::dump(1U, "ModemV24::ackStartOfStreamTIA() Ack StartOfStream", buffer, length);

    queueP25Frame(buffer, length, STT_NON_IMBE_NO_JITTER);
}

/* Internal helper to convert from TIA-102 air interface to V.24/DFSI. */

void ModemV24::convertFromAir(uint8_t* data, uint32_t length)
{
    assert(data != nullptr);
    assert(length > 0U);

    if (m_trace)
        Utils::dump(1U, "ModemV24::convertFromAir() data", data, length);

    uint8_t ldu[9U * 25U];
    ::memset(ldu, 0x00U, 9 * 25U);

    // decode the NID
    bool valid = m_nid->decode(data + 2U);
    if (!valid)
        return;

    DUID::E duid = m_nid->getDUID();

    // handle individual DUIDs
    lc::LC lc = lc::LC();
    data::LowSpeedData lsd = data::LowSpeedData();
    switch (duid) {
        case DUID::HDU:
        {
            bool ret = lc.decodeHDU(data + 2U);
            if (!ret) {
                LogWarning(LOG_MODEM, P25_HDU_STR ", undecodable LC");
            }

            startOfStream(lc);
        }
        break;
        case DUID::LDU1:
        {
            bool ret = lc.decodeLDU1(data + 2U, true);
            if (!ret) {
                LogWarning(LOG_MODEM, P25_LDU1_STR ", undecodable LC");
                return;
            }

            lsd.process(data + 2U);

            // late entry?
            if (!m_txCallInProgress) {
                startOfStream(lc);
                if (m_debug)
                    LogDebug(LOG_MODEM, "V24 TX VHDR late entry, resetting TX call data");
            }

            // generate audio
            m_audio.decode(data + 2U, ldu + 10U, 0U);
            m_audio.decode(data + 2U, ldu + 26U, 1U);
            m_audio.decode(data + 2U, ldu + 55U, 2U);
            m_audio.decode(data + 2U, ldu + 80U, 3U);
            m_audio.decode(data + 2U, ldu + 105U, 4U);
            m_audio.decode(data + 2U, ldu + 130U, 5U);
            m_audio.decode(data + 2U, ldu + 155U, 6U);
            m_audio.decode(data + 2U, ldu + 180U, 7U);
            m_audio.decode(data + 2U, ldu + 204U, 8U);
        }
        break;
        case DUID::LDU2:
        {
            bool ret = lc.decodeLDU2(data + 2U);
            if (!ret) {
                LogWarning(LOG_MODEM, P25_LDU2_STR ", undecodable LC");
                return;
            }

            lsd.process(data + 2U);

            // generate audio
            m_audio.decode(data + 2U, ldu + 10U, 0U);
            m_audio.decode(data + 2U, ldu + 26U, 1U);
            m_audio.decode(data + 2U, ldu + 55U, 2U);
            m_audio.decode(data + 2U, ldu + 80U, 3U);
            m_audio.decode(data + 2U, ldu + 105U, 4U);
            m_audio.decode(data + 2U, ldu + 130U, 5U);
            m_audio.decode(data + 2U, ldu + 155U, 6U);
            m_audio.decode(data + 2U, ldu + 180U, 7U);
            m_audio.decode(data + 2U, ldu + 204U, 8U);
        }
        break;

        case DUID::TDU:
        case DUID::TDULC:
            if (m_txCallInProgress)
                endOfStream();
            break;

        case DUID::PDU:
            break;

        case DUID::TSDU:
        {
            lc::tsbk::OSP_TSBK_RAW tsbk = lc::tsbk::OSP_TSBK_RAW();
            if (!tsbk.decode(data + 2U)) {
                LogWarning(LOG_MODEM, P25_TSDU_STR ", undecodable LC");
                return;
            }

            MotStartOfStream startOfStream = MotStartOfStream();
            startOfStream.setStartStop(StartStopFlag::START);
            startOfStream.setRT(m_rtrt ? RTFlag::ENABLED : RTFlag::DISABLED);
            startOfStream.setStreamType(StreamTypeFlag::TSBK);

            // create buffer and encode
            uint8_t startBuf[MotStartOfStream::LENGTH];
            ::memset(startBuf, 0x00U, MotStartOfStream::LENGTH);
            startOfStream.encode(startBuf);

            queueP25Frame(startBuf, MotStartOfStream::LENGTH, STT_NON_IMBE_NO_JITTER);

            MotTSBKFrame tf = MotTSBKFrame();
            tf.startOfStream->setStartStop(StartStopFlag::START);
            tf.startOfStream->setRT(m_rtrt ? RTFlag::ENABLED : RTFlag::DISABLED);
            tf.startOfStream->setStreamType(StreamTypeFlag::TSBK);
            delete[] tf.tsbkData;

            tf.tsbkData = new uint8_t[P25_TSBK_LENGTH_BYTES];
            ::memset(tf.tsbkData, 0x00U, P25_TSBK_LENGTH_BYTES);
            ::memcpy(tf.tsbkData, tsbk.getDecodedRaw(), P25_TSBK_LENGTH_BYTES);

            // create buffer and encode
            uint8_t tsbkBuf[MotTSBKFrame::LENGTH];
            ::memset(tsbkBuf, 0x00U, MotTSBKFrame::LENGTH);
            tf.encode(tsbkBuf);

            if (m_trace)
                Utils::dump(1U, "ModemV24::convertFromAir() MotTSBKFrame", tsbkBuf, MotTSBKFrame::LENGTH);

            queueP25Frame(tsbkBuf, MotTSBKFrame::LENGTH, STT_NON_IMBE_NO_JITTER);

            MotStartOfStream endOfStream = MotStartOfStream();
            endOfStream.setStartStop(StartStopFlag::STOP);
            endOfStream.setRT(m_rtrt ? RTFlag::ENABLED : RTFlag::DISABLED);
            endOfStream.setStreamType(StreamTypeFlag::TSBK);

            // create buffer and encode
            uint8_t endBuf[MotStartOfStream::LENGTH];
            ::memset(endBuf, 0x00U, MotStartOfStream::LENGTH);
            endOfStream.encode(endBuf);

            queueP25Frame(endBuf, MotStartOfStream::LENGTH, STT_NON_IMBE_NO_JITTER);
            queueP25Frame(endBuf, MotStartOfStream::LENGTH, STT_NON_IMBE_NO_JITTER);
        }
        break;

        default:
            break;
    }

    if (duid == DUID::LDU1 || duid == DUID::LDU2) {
        uint8_t rs[P25_LDU_LC_FEC_LENGTH_BYTES];
        ::memset(rs, 0x00U, P25_LDU_LC_FEC_LENGTH_BYTES);

        if (duid == DUID::LDU1) {
            rs[0U] = lc.getLCO();                                               // LCO

            // split ulong64_t (8 byte) value into bytes
            rs[1U] = (uint8_t)((lc.getRS() >> 56) & 0xFFU);
            rs[2U] = (uint8_t)((lc.getRS() >> 48) & 0xFFU);
            rs[3U] = (uint8_t)((lc.getRS() >> 40) & 0xFFU);
            rs[4U] = (uint8_t)((lc.getRS() >> 32) & 0xFFU);
            rs[5U] = (uint8_t)((lc.getRS() >> 24) & 0xFFU);
            rs[6U] = (uint8_t)((lc.getRS() >> 16) & 0xFFU);
            rs[7U] = (uint8_t)((lc.getRS() >> 8) & 0xFFU);
            rs[8U] = (uint8_t)((lc.getRS() >> 0) & 0xFFU);

            // encode RS (24,12,13) FEC
            m_rs.encode241213(rs);
        } else {
            // generate MI data
            uint8_t mi[MI_LENGTH_BYTES];
            ::memset(mi, 0x00U, MI_LENGTH_BYTES);
            lc.getMI(mi);

            for (uint32_t i = 0; i < MI_LENGTH_BYTES; i++)
                rs[i] = mi[i];                                                      // Message Indicator

            rs[9U] = lc.getAlgId();                                                 // Algorithm ID
            rs[10U] = (lc.getKId() >> 8) & 0xFFU;                                   // Key ID
            rs[11U] = (lc.getKId() >> 0) & 0xFFU;                                   // ...

            // encode RS (24,16,9) FEC
            m_rs.encode24169(rs);
        }

        for (int n = 0; n < 9; n++) {
            uint8_t* buffer = nullptr;
            uint16_t bufferSize = 0;
            MotFullRateVoice voice = MotFullRateVoice();

            switch (n) {
                case 0: // VOICE1/10
                {
                    voice.setFrameType((duid == DUID::LDU1) ? DFSIFrameType::LDU1_VOICE1 : DFSIFrameType::LDU2_VOICE10);

                    MotStartVoiceFrame svf = MotStartVoiceFrame();
                    svf.startOfStream->setStartStop(StartStopFlag::START);
                    svf.startOfStream->setRT(m_rtrt ? RTFlag::ENABLED : RTFlag::DISABLED);
                    svf.fullRateVoice->setFrameType(voice.getFrameType());
                    svf.fullRateVoice->setSource(m_diu ? SourceFlag::DIU : SourceFlag::QUANTAR);
                    svf.setICW(m_diu ? ICWFlag::DIU : ICWFlag::QUANTAR);

                    ::memcpy(svf.fullRateVoice->imbeData, ldu + 10U, RAW_IMBE_LENGTH_BYTES);

                    buffer = new uint8_t[MotStartVoiceFrame::LENGTH];
                    ::memset(buffer, 0x00U, MotStartVoiceFrame::LENGTH);
                    svf.encode(buffer);
                    bufferSize = MotStartVoiceFrame::LENGTH;
                }
                break;
                case 1: // VOICE2/11
                {
                    voice.setFrameType((duid == DUID::LDU1) ? DFSIFrameType::LDU1_VOICE2 : DFSIFrameType::LDU2_VOICE11);
                    voice.setSource(m_diu ? SourceFlag::DIU : SourceFlag::QUANTAR);
                    ::memcpy(voice.imbeData, ldu + 26U, RAW_IMBE_LENGTH_BYTES);
                }
                break;
                case 2: // VOICE3/12
                {
                    voice.setFrameType((duid == DUID::LDU1) ? DFSIFrameType::LDU1_VOICE3 : DFSIFrameType::LDU2_VOICE12);
                    ::memcpy(voice.imbeData, ldu + 55U, RAW_IMBE_LENGTH_BYTES);

                    voice.additionalData = new uint8_t[voice.ADDITIONAL_LENGTH];
                    ::memset(voice.additionalData, 0x00U, voice.ADDITIONAL_LENGTH);

                    // copy additional data
                    voice.additionalData[0U] = rs[0U];
                    voice.additionalData[1U] = rs[1U];
                    voice.additionalData[2U] = rs[2U];
                }
                break;
                case 3: // VOICE4/13
                {
                    voice.setFrameType((duid == DUID::LDU1) ? DFSIFrameType::LDU1_VOICE4 : DFSIFrameType::LDU2_VOICE13);
                    ::memcpy(voice.imbeData, ldu + 80U, RAW_IMBE_LENGTH_BYTES);

                    voice.additionalData = new uint8_t[voice.ADDITIONAL_LENGTH];
                    ::memset(voice.additionalData, 0x00U, voice.ADDITIONAL_LENGTH);

                    // copy additional data
                    voice.additionalData[0U] = rs[3U];
                    voice.additionalData[1U] = rs[4U];
                    voice.additionalData[2U] = rs[5U];
                }
                break;
                case 4: // VOICE5/14
                {
                    voice.setFrameType((duid == DUID::LDU1) ? DFSIFrameType::LDU1_VOICE5 : DFSIFrameType::LDU2_VOICE14);
                    ::memcpy(voice.imbeData, ldu + 105U, RAW_IMBE_LENGTH_BYTES);

                    voice.additionalData = new uint8_t[voice.ADDITIONAL_LENGTH];
                    ::memset(voice.additionalData, 0x00U, voice.ADDITIONAL_LENGTH);

                    voice.additionalData[0U] = rs[6U];
                    voice.additionalData[1U] = rs[7U];
                    voice.additionalData[2U] = rs[8U];
                }
                break;
                case 5: // VOICE6/15
                {
                    voice.setFrameType((duid == DUID::LDU1) ? DFSIFrameType::LDU1_VOICE6 : DFSIFrameType::LDU2_VOICE15);
                    ::memcpy(voice.imbeData, ldu + 130U, RAW_IMBE_LENGTH_BYTES);

                    voice.additionalData = new uint8_t[voice.ADDITIONAL_LENGTH];
                    ::memset(voice.additionalData, 0x00U, voice.ADDITIONAL_LENGTH);

                    voice.additionalData[0U] = rs[9U];
                    voice.additionalData[1U] = rs[10U];
                    voice.additionalData[2U] = rs[11U];
                }
                break;
                case 6: // VOICE7/16
                {
                    voice.setFrameType((duid == DUID::LDU1) ? DFSIFrameType::LDU1_VOICE7 : DFSIFrameType::LDU2_VOICE16);
                    ::memcpy(voice.imbeData, ldu + 155U, RAW_IMBE_LENGTH_BYTES);

                    voice.additionalData = new uint8_t[voice.ADDITIONAL_LENGTH];
                    ::memset(voice.additionalData, 0x00U, voice.ADDITIONAL_LENGTH);

                    voice.additionalData[0U] = rs[12U];
                    voice.additionalData[1U] = rs[13U];
                    voice.additionalData[2U] = rs[14U];
                }
                break;
                case 7: // VOICE8/17
                {
                    voice.setFrameType((duid == DUID::LDU1) ? DFSIFrameType::LDU1_VOICE8 : DFSIFrameType::LDU2_VOICE17);
                    ::memcpy(voice.imbeData, ldu + 180U, RAW_IMBE_LENGTH_BYTES);

                    voice.additionalData = new uint8_t[voice.ADDITIONAL_LENGTH];
                    ::memset(voice.additionalData, 0x00U, voice.ADDITIONAL_LENGTH);

                    voice.additionalData[0U] = rs[15U];
                    voice.additionalData[1U] = rs[16U];
                    voice.additionalData[2U] = rs[17U];
                }
                break;
                case 8: // VOICE9/18
                {
                    voice.setFrameType((duid == DUID::LDU1) ? DFSIFrameType::LDU1_VOICE9 : DFSIFrameType::LDU2_VOICE18);
                    ::memcpy(voice.imbeData, ldu + 204U, RAW_IMBE_LENGTH_BYTES);

                    voice.additionalData = new uint8_t[voice.ADDITIONAL_LENGTH];
                    ::memset(voice.additionalData, 0x00U, voice.ADDITIONAL_LENGTH);

                    voice.additionalData[0U] = lsd.getLSD1();
                    voice.additionalData[1U] = lsd.getLSD2();
                }
                break;
            }

            // For n=0 (VHDR1/10) case we create the buffer in the switch, for all other frame types we do that here
            if (n != 0) {
                buffer = new uint8_t[voice.size()];
                ::memset(buffer, 0x00U, voice.size());
                voice.encode(buffer);
                bufferSize = voice.size();
            }

            if (buffer != nullptr) {
                if (m_trace) {
                    Utils::dump("ModemV24::convertFromAir() Encoded V.24 Voice Frame Data", buffer, bufferSize);
                }

                queueP25Frame(buffer, bufferSize, STT_IMBE);
                delete[] buffer;
            }
        }
    }
}

/* Internal helper to convert from TIA-102 air interface to TIA-102 DFSI. */

void ModemV24::convertFromAirTIA(uint8_t* data, uint32_t length)
{
    assert(data != nullptr);
    assert(length > 0U);

    if (m_trace)
        Utils::dump(1U, "ModemV24::convertFromAirTIA() data", data, length);

    uint8_t ldu[9U * 25U];
    ::memset(ldu, 0x00U, 9 * 25U);

    // decode the NID
    bool valid = m_nid->decode(data + 2U);
    if (!valid)
        return;

    DUID::E duid = m_nid->getDUID();

    // handle individual DUIDs
    lc::LC lc = lc::LC();
    data::LowSpeedData lsd = data::LowSpeedData();
    switch (duid) {
        case DUID::HDU:
        {
            bool ret = lc.decodeHDU(data + 2U);
            if (!ret) {
                LogWarning(LOG_MODEM, P25_HDU_STR ", undecodable LC");
            }

            startOfStreamTIA(lc);
        }
        break;
        case DUID::LDU1:
        {
            bool ret = lc.decodeLDU1(data + 2U, true);
            if (!ret) {
                LogWarning(LOG_MODEM, P25_LDU1_STR ", undecodable LC");
                return;
            }

            lsd.process(data + 2U);

            // late entry?
            if (!m_txCallInProgress) {
                startOfStreamTIA(lc);
                if (m_debug)
                    LogDebug(LOG_MODEM, "V24 TX VHDR late entry, resetting TX call data");
            }

            // generate audio
            m_audio.decode(data + 2U, ldu + 10U, 0U);
            m_audio.decode(data + 2U, ldu + 26U, 1U);
            m_audio.decode(data + 2U, ldu + 55U, 2U);
            m_audio.decode(data + 2U, ldu + 80U, 3U);
            m_audio.decode(data + 2U, ldu + 105U, 4U);
            m_audio.decode(data + 2U, ldu + 130U, 5U);
            m_audio.decode(data + 2U, ldu + 155U, 6U);
            m_audio.decode(data + 2U, ldu + 180U, 7U);
            m_audio.decode(data + 2U, ldu + 204U, 8U);
        }
        break;
        case DUID::LDU2:
        {
            bool ret = lc.decodeLDU2(data + 2U);
            if (!ret) {
                LogWarning(LOG_MODEM, P25_LDU2_STR ", undecodable LC");
                return;
            }

            lsd.process(data + 2U);

            // generate audio
            m_audio.decode(data + 2U, ldu + 10U, 0U);
            m_audio.decode(data + 2U, ldu + 26U, 1U);
            m_audio.decode(data + 2U, ldu + 55U, 2U);
            m_audio.decode(data + 2U, ldu + 80U, 3U);
            m_audio.decode(data + 2U, ldu + 105U, 4U);
            m_audio.decode(data + 2U, ldu + 130U, 5U);
            m_audio.decode(data + 2U, ldu + 155U, 6U);
            m_audio.decode(data + 2U, ldu + 180U, 7U);
            m_audio.decode(data + 2U, ldu + 204U, 8U);
        }
        break;

        case DUID::TDU:
        case DUID::TDULC:
            if (m_txCallInProgress)
                endOfStreamTIA();
            break;

        case DUID::PDU:
            break;

        case DUID::TSDU:
            break;

        default:
            break;
    }

    if (duid == DUID::LDU1 || duid == DUID::LDU2) {
        uint8_t rs[P25_LDU_LC_FEC_LENGTH_BYTES];
        ::memset(rs, 0x00U, P25_LDU_LC_FEC_LENGTH_BYTES);

        if (duid == DUID::LDU1) {
            rs[0U] = lc.getLCO();                                               // LCO

            // split ulong64_t (8 byte) value into bytes
            rs[1U] = (uint8_t)((lc.getRS() >> 56) & 0xFFU);
            rs[2U] = (uint8_t)((lc.getRS() >> 48) & 0xFFU);
            rs[3U] = (uint8_t)((lc.getRS() >> 40) & 0xFFU);
            rs[4U] = (uint8_t)((lc.getRS() >> 32) & 0xFFU);
            rs[5U] = (uint8_t)((lc.getRS() >> 24) & 0xFFU);
            rs[6U] = (uint8_t)((lc.getRS() >> 16) & 0xFFU);
            rs[7U] = (uint8_t)((lc.getRS() >> 8) & 0xFFU);
            rs[8U] = (uint8_t)((lc.getRS() >> 0) & 0xFFU);

            // encode RS (24,12,13) FEC
            m_rs.encode241213(rs);
        } else {
            // generate MI data
            uint8_t mi[MI_LENGTH_BYTES];
            ::memset(mi, 0x00U, MI_LENGTH_BYTES);
            lc.getMI(mi);

            for (uint32_t i = 0; i < MI_LENGTH_BYTES; i++)
                rs[i] = mi[i];                                                      // Message Indicator

            rs[9U] = lc.getAlgId();                                                 // Algorithm ID
            rs[10U] = (lc.getKId() >> 8) & 0xFFU;                                   // Key ID
            rs[11U] = (lc.getKId() >> 0) & 0xFFU;                                   // ...

            // encode RS (24,16,9) FEC
            m_rs.encode24169(rs);
        }

        for (int n = 0; n < 9; n++) {
            uint8_t* buffer = nullptr;
            uint16_t bufferSize = 0;
            FullRateVoice voice = FullRateVoice();

            switch (n) {
                case 0: // VOICE1/10
                {
                    voice.setFrameType((duid == DUID::LDU1) ? DFSIFrameType::LDU1_VOICE1 : DFSIFrameType::LDU2_VOICE10);
                    ::memcpy(voice.imbeData, ldu + 10U, RAW_IMBE_LENGTH_BYTES);
                }
                break;
                case 1: // VOICE2/11
                {
                    voice.setFrameType((duid == DUID::LDU1) ? DFSIFrameType::LDU1_VOICE2 : DFSIFrameType::LDU2_VOICE11);
                    ::memcpy(voice.imbeData, ldu + 26U, RAW_IMBE_LENGTH_BYTES);
                }
                break;
                case 2: // VOICE3/12
                {
                    voice.setFrameType((duid == DUID::LDU1) ? DFSIFrameType::LDU1_VOICE3 : DFSIFrameType::LDU2_VOICE12);
                    ::memcpy(voice.imbeData, ldu + 55U, RAW_IMBE_LENGTH_BYTES);

                    voice.additionalData = new uint8_t[voice.ADDITIONAL_LENGTH];
                    ::memset(voice.additionalData, 0x00U, voice.ADDITIONAL_LENGTH);

                    // copy additional data
                    voice.additionalData[0U] = rs[0U];
                    voice.additionalData[1U] = rs[1U];
                    voice.additionalData[2U] = rs[2U];
                }
                break;
                case 3: // VOICE4/13
                {
                    voice.setFrameType((duid == DUID::LDU1) ? DFSIFrameType::LDU1_VOICE4 : DFSIFrameType::LDU2_VOICE13);
                    ::memcpy(voice.imbeData, ldu + 80U, RAW_IMBE_LENGTH_BYTES);

                    voice.additionalData = new uint8_t[voice.ADDITIONAL_LENGTH];
                    ::memset(voice.additionalData, 0x00U, voice.ADDITIONAL_LENGTH);

                    // copy additional data
                    voice.additionalData[0U] = rs[3U];
                    voice.additionalData[1U] = rs[4U];
                    voice.additionalData[2U] = rs[5U];
                }
                break;
                case 4: // VOICE5/14
                {
                    voice.setFrameType((duid == DUID::LDU1) ? DFSIFrameType::LDU1_VOICE5 : DFSIFrameType::LDU2_VOICE14);
                    ::memcpy(voice.imbeData, ldu + 105U, RAW_IMBE_LENGTH_BYTES);

                    voice.additionalData = new uint8_t[voice.ADDITIONAL_LENGTH];
                    ::memset(voice.additionalData, 0x00U, voice.ADDITIONAL_LENGTH);

                    voice.additionalData[0U] = rs[6U];
                    voice.additionalData[1U] = rs[7U];
                    voice.additionalData[2U] = rs[8U];
                }
                break;
                case 5: // VOICE6/15
                {
                    voice.setFrameType((duid == DUID::LDU1) ? DFSIFrameType::LDU1_VOICE6 : DFSIFrameType::LDU2_VOICE15);
                    ::memcpy(voice.imbeData, ldu + 130U, RAW_IMBE_LENGTH_BYTES);

                    voice.additionalData = new uint8_t[voice.ADDITIONAL_LENGTH];
                    ::memset(voice.additionalData, 0x00U, voice.ADDITIONAL_LENGTH);

                    voice.additionalData[0U] = rs[9U];
                    voice.additionalData[1U] = rs[10U];
                    voice.additionalData[2U] = rs[11U];
                }
                break;
                case 6: // VOICE7/16
                {
                    voice.setFrameType((duid == DUID::LDU1) ? DFSIFrameType::LDU1_VOICE7 : DFSIFrameType::LDU2_VOICE16);
                    ::memcpy(voice.imbeData, ldu + 155U, RAW_IMBE_LENGTH_BYTES);

                    voice.additionalData = new uint8_t[voice.ADDITIONAL_LENGTH];
                    ::memset(voice.additionalData, 0x00U, voice.ADDITIONAL_LENGTH);

                    voice.additionalData[0U] = rs[12U];
                    voice.additionalData[1U] = rs[13U];
                    voice.additionalData[2U] = rs[14U];
                }
                break;
                case 7: // VOICE8/17
                {
                    voice.setFrameType((duid == DUID::LDU1) ? DFSIFrameType::LDU1_VOICE8 : DFSIFrameType::LDU2_VOICE17);
                    ::memcpy(voice.imbeData, ldu + 180U, RAW_IMBE_LENGTH_BYTES);

                    voice.additionalData = new uint8_t[voice.ADDITIONAL_LENGTH];
                    ::memset(voice.additionalData, 0x00U, voice.ADDITIONAL_LENGTH);

                    voice.additionalData[0U] = rs[15U];
                    voice.additionalData[1U] = rs[16U];
                    voice.additionalData[2U] = rs[17U];
                }
                break;
                case 8: // VOICE9/18
                {
                    voice.setFrameType((duid == DUID::LDU1) ? DFSIFrameType::LDU1_VOICE9 : DFSIFrameType::LDU2_VOICE18);
                    ::memcpy(voice.imbeData, ldu + 204U, RAW_IMBE_LENGTH_BYTES);

                    voice.additionalData = new uint8_t[voice.ADDITIONAL_LENGTH];
                    ::memset(voice.additionalData, 0x00U, voice.ADDITIONAL_LENGTH);

                    voice.additionalData[0U] = lsd.getLSD1();
                    voice.additionalData[1U] = lsd.getLSD2();
                }
                break;
            }

            buffer = new uint8_t[P25_PDU_FRAME_LENGTH_BYTES];
            ::memset(buffer, 0x00U, P25_PDU_FRAME_LENGTH_BYTES);

            // generate control octet
            ControlOctet ctrl = ControlOctet();
            ctrl.setBlockHeaderCnt(1U);
            ctrl.encode(buffer);
            bufferSize += ControlOctet::LENGTH;

            // generate block header
            BlockHeader hdr = BlockHeader();
            hdr.setBlockType(BlockType::FULL_RATE_VOICE);
            hdr.encode(buffer + 1U);
            bufferSize += BlockHeader::LENGTH;

            voice.setSuperframeCnt(m_superFrameCnt);
            voice.setBusy(1U); // Inbound Channel is Busy
            voice.encode(buffer + bufferSize);
            bufferSize += voice.getLength(); // 18, 17 or 14 depending on voice frame type

            if (buffer != nullptr) {
                if (m_trace) {
                    Utils::dump("ModemV24::convertFromAirTIA() Encoded V.24 Voice Frame Data", buffer, bufferSize);
                }

                queueP25Frame(buffer, bufferSize, STT_IMBE);
                delete[] buffer;
            }
        }

        // bryanb: this is a naive way of incrementing the superframe counter, we basically just increment it after
        // processing and LDU2
        if (duid == DUID::LDU2) {
            if (m_superFrameCnt == 255U)
                m_superFrameCnt = 0U;
            else
                m_superFrameCnt++;
        }
    }
}
