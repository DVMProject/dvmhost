// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *  Copyright (C) 2024 Patrick McDonnell, W3AXL
 *
 */
#include "Defines.h"
#include "common/p25/P25Defines.h"
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
    m_audio(),
    m_nid(DEFAULT_NAC),
    m_txP25Queue(p25TxQueueSize, "TX P25 Queue"),
    m_call(),
    m_callInProgress(false),
    m_lastFrameTime(0U),
    m_callTimeout(200U),
    m_jitter(jitter),
    m_lastP25Tx(0U),
    m_rs()
{
    /* stub */
}

/* Finalizes a instance of the Modem class. */

ModemV24::~ModemV24() = default;

/* Sets the call timeout. */

void ModemV24::setCallTimeout(uint16_t timeout)
{
    m_callTimeout = timeout;
}

/* Sets the P25 NAC. */

void ModemV24::setP25NAC(uint32_t nac)
{
    Modem::setP25NAC(nac);
    m_nid = NID(nac);
}

/* Opens connection to the air interface modem. */

bool ModemV24::open()
{
    LogMessage(LOG_MODEM, "Initializing modem");
    m_gotModemStatus = false;

    bool ret = m_port->open();
    if (!ret)
        return false;

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

    LogMessage(LOG_MODEM, "Modem Ready [Direct Mode]");
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
                convertToAir(m_buffer + (cmdOffset + 1U), m_length - (cmdOffset + 1U));
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
            m_dmrSpace1 = 0U;
            m_dmrSpace2 = 0U;
            m_p25Space = m_buffer[10U] * (P25DEF::P25_LDU_FRAME_LENGTH_BYTES);//(P25DEF::P25_PDU_FRAME_LENGTH_BYTES);
            m_nxdnSpace = 0U;

            if (m_dumpModemStatus) {
                LogDebug(LOG_MODEM, "ModemV24::clock(), CMD_GET_STATUS, isHotspot = %u, dmr = %u / %u, p25 = %u / %u, nxdn = %u / %u, modemState = %u, tx = %u, adcOverflow = %u, rxOverflow = %u, txOverflow = %u, dacOverflow = %u, dmrSpace1 = %u, dmrSpace2 = %u, p25Space = %u, nxdnSpace = %u",
                    m_isHotspot, dmrEnable, m_dmrEnabled, p25Enable, m_p25Enabled, nxdnEnable, m_nxdnEnabled, m_modemState, m_tx, adcOverflow, rxOverflow, txOverflow, dacOverflow, m_dmrSpace1, m_dmrSpace2, m_p25Space, m_nxdnSpace);
                LogDebug(LOG_MODEM, "ModemV24::clock(), CMD_GET_STATUS, rxDMRData1 size = %u, len = %u, free = %u; rxDMRData2 size = %u, len = %u, free = %u, rxP25Data size = %u, len = %u, free = %u, rxNXDNData size = %u, len = %u, free = %u",
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
    if (m_trace && len > 0) {
        LogDebug(LOG_MODEM, "Wrote %u-byte message to the serial V24 device", len);
    } else if (len < 0) {
        LogError(LOG_MODEM, "Failed to write to serial port!");
    }

    // clear a call in progress flag if we're longer than our timeout value
    if (m_callInProgress && (now - m_lastFrameTime > m_callTimeout)) {
        m_callInProgress = false;
        m_call->resetCallData();
    }
}

/* Closes connection to the air interface modem. */

void ModemV24::close()
{
    LogDebug(LOG_MODEM, "Closing the modem");
    m_port->close();

    m_gotModemStatus = false;

    // do we have a close port handler?
    if (m_closePortHandler != nullptr) {
        m_closePortHandler(this);
    }
}

/* Writes raw data to the air interface modem. */

int ModemV24::write(const uint8_t* data, uint32_t length)
{
    uint8_t buffer[length];
    ::memset(buffer, 0x00U, length);
    ::memcpy(buffer, data, length);

    // convert data from TIA-102 air formatting to V.24/DFSI formatting
    convertFromAir(buffer, length);
    return length;
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
        uint8_t buffer[len];
        m_txP25Queue.get(buffer, len);
        
        // Sanity check on data tag
        uint8_t tag = lengthTagTs[2U];
        if (tag != TAG_DATA) {
            LogError(LOG_SERIAL, "Got unexpected data tag from TX P25 ringbuffer! %02X", tag);
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
    storedLen[0U] = 0x00U;
    storedLen[1U] = length;
    m_rxP25Queue.addData(storedLen, 2U);

    uint8_t tagData = TAG_DATA;
    m_rxP25Queue.addData(&tagData, 1U);

    m_rxP25Queue.addData(buffer, length);
}

/* Helper to generate a P25 TDU packet. */

void ModemV24::create_TDU(uint8_t* buffer)
{
    assert(buffer != nullptr);

    uint8_t data[P25_TDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, P25_TDU_FRAME_LENGTH_BYTES);

    // Generate Sync
    Sync::addP25Sync(data + 2U);

    // Generate NID
    m_nid.encode(data + 2U, DUID::TDU);

    // Add busy bits
    P25Utils::addStatusBits(data + 2U, P25_TDU_FRAME_LENGTH_BITS, false);

    buffer[0U] = modem::TAG_EOT;
    buffer[1U] = 0x01U;
    ::memcpy(buffer, data, P25_TDU_FRAME_LENGTH_BYTES + 2U);
}

/* Internal helper to convert from V.24/DFSI to TIA-102 air interface. */

void ModemV24::convertToAir(const uint8_t *data, uint32_t length)
{
    uint8_t buffer[P25_PDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(buffer, 0x00U, P25_PDU_FRAME_LENGTH_BYTES + 2U);

    uint8_t tag = data[0U];
    if (tag != TAG_DATA) {
        LogError(LOG_SERIAL, "Unexpected data tag in RX P25 frame buffer: 0x%02X", tag);
        return;
    }

    // get the DFSI data (skip the 0x00 padded byte at the start)
    uint8_t dfsiData[length - 2U];
    ::memset(dfsiData, 0x00U, length - 2U);
    ::memcpy(dfsiData, data + 2U, length - 2U);

    DFSIFrameType::E frameType = (DFSIFrameType::E)dfsiData[0U];
    m_lastFrameTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // Switch based on DFSI frame type
    switch (frameType) {
        case DFSIFrameType::MOT_START_STOP:
        {
            MotStartOfStream start = MotStartOfStream(dfsiData);
            if (start.getStartStop() == StartStopFlag::START) {
                m_callInProgress = true;
                m_call->resetCallData();
            } else {
                if (m_callInProgress) {
                    m_callInProgress = false;
                    m_call->resetCallData();

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
            ::memset(m_call->VHDR1, 0x00U, MotVoiceHeader1::HCW_LENGTH);
            ::memcpy(m_call->VHDR1, vhdr1.header, MotVoiceHeader1::HCW_LENGTH);
        }
        break;
        case DFSIFrameType::MOT_VHDR_2:
        {
            MotVoiceHeader2 vhdr2 = MotVoiceHeader2(dfsiData);

            // copy to call data VHDR2
            ::memset(m_call->VHDR1, 0x00U, MotVoiceHeader2::HCW_LENGTH);
            ::memcpy(m_call->VHDR1, vhdr2.header, MotVoiceHeader2::HCW_LENGTH);

            // buffer for raw VHDR data
            uint8_t raw[DFSI_VHDR_RAW_LEN];
            ::memset(raw, 0x00U, DFSI_VHDR_RAW_LEN);

            ::memcpy(raw,       m_call->VHDR1, 8U);
            ::memcpy(raw + 8U,  m_call->VHDR1 + 9U, 8U);
            ::memcpy(raw + 16U, m_call->VHDR1 + 18U, 2U);

            ::memcpy(raw + 18U, m_call->VHDR2, 8U);
            ::memcpy(raw + 26U, m_call->VHDR2 + 9U, 8U);
            ::memcpy(raw + 34U, m_call->VHDR2 + 18U, 2U);

            // buffer for decoded VHDR data
            uint8_t vhdr[DFSI_VHDR_LEN];

            uint offset = 0U;
            for (uint32_t i = 0; i < DFSI_VHDR_RAW_LEN; i++, offset += 6)
                Utils::hex2Bin(raw[i], vhdr, offset);

            // try to decode the RS data
            try {
                bool ret = m_rs.decode362017(vhdr);
                if (!ret) {
                    LogError(LOG_MODEM, "V.24/DFSI traffic failed to decode RS (36,20,17) FEC");
                } else {
                    // late entry?
                    if (!m_callInProgress) {
                        m_callInProgress = true;
                        m_call->resetCallData();
                    }

                    ::memcpy(m_call->MI, vhdr, MI_LENGTH_BYTES);

                    m_call->mfId = vhdr[9U];
                    m_call->algoId = vhdr[10U];
                    m_call->kId = __GET_UINT16B(vhdr, 11U);
                    m_call->dstId = __GET_UINT16B(vhdr, 13U);

                    if (m_debug) {
                        LogDebug(LOG_MODEM, "P25, VHDR algId = $%02X, kId = $%04X, dstId = $%04X", m_call->algoId, m_call->kId, m_call->dstId);
                    }

                    // generate a HDU
                    lc::LC lc = lc::LC();
                    lc.setDstId(m_call->dstId);
                    lc.setAlgId(m_call->algoId);
                    lc.setKId(m_call->kId);
                    lc.setMI(m_call->MI);

                    // Generate Sync
                    Sync::addP25Sync(buffer + 2U);

                    // Generate NID
                    m_nid.encode(buffer + 2U, DUID::HDU);

                    // Generate HDU
                    lc.encodeHDU(buffer + 2U);

                    // Add busy bits
                    P25Utils::addStatusBits(buffer + 2U, P25_HDU_FRAME_LENGTH_BITS, true);

                    buffer[0U] = modem::TAG_DATA;
                    buffer[1U] = 0x01U;
                    storeConvertedRx(buffer, P25_HDU_FRAME_LENGTH_BYTES + 2U);
                }
            }
            catch (...) {
                LogError(LOG_MODEM, "V.24/DFSI traffic got exception while trying to decode RS data for VHDR");
            }
        }
        break;

        // VOICE1/10 create a start voice frame
        case DFSIFrameType::LDU1_VOICE1:
        {
            MotStartVoiceFrame svf = MotStartVoiceFrame(dfsiData);
            ::memcpy(m_call->netLDU1 + 10U, svf.fullRateVoice->imbeData, RAW_IMBE_LENGTH_BYTES);
            m_call->n++;
        }
        break;
        case DFSIFrameType::LDU2_VOICE10:
        {
            MotStartVoiceFrame svf = MotStartVoiceFrame(dfsiData);
            ::memcpy(m_call->netLDU2 + 10U, svf.fullRateVoice->imbeData, RAW_IMBE_LENGTH_BYTES);
            m_call->n++;
        }
        break;

        case DFSIFrameType::TSBK:
        {
            MotTSBKFrame tf = MotTSBKFrame(dfsiData);
            std::unique_ptr<lc::TSBK> tsbk = lc::tsbk::TSBKFactory::createTSBK(tf.tsbkData, true);

            if (tsbk != nullptr) {
                LogError(LOG_MODEM, "V.24/DFSI traffic failed to decode TSBK FEC");
            } else {
                uint8_t buffer[P25_TSDU_FRAME_LENGTH_BYTES + 2U];
                ::memset(buffer, 0x00U, P25_TSDU_FRAME_LENGTH_BYTES + 2U);

                buffer[0U] = modem::TAG_DATA;
                buffer[1U] = 0x00U;

                // Generate Sync
                Sync::addP25Sync(buffer + 2U);

                // Generate NID
                m_nid.encode(buffer + 2U, DUID::TSDU);

                // Regenerate TSDU Data
                tsbk->setLastBlock(true); // always set last block -- this a Single Block TSDU
                tsbk->encode(buffer + 2U);

                // Add busy bits
                P25Utils::addStatusBits(buffer + 2U, P25_TSDU_FRAME_LENGTH_BYTES, false, true);
                P25Utils::addTrunkSlotStatusBits(buffer + 2U, P25_TSDU_FRAME_LENGTH_BYTES);

                // Set first busy bits to 1,1
                P25Utils::setStatusBits(buffer + 2U, P25_SS0_START, true, true);

                storeConvertedRx(buffer, P25_TSDU_FRAME_LENGTH_BYTES + 2U);
            }
        }
        break;

        // The remaining LDUs all create full rate voice frames so we do that here
        default:
        {
            MotFullRateVoice voice = MotFullRateVoice(dfsiData);
            switch (frameType) {
                case DFSIFrameType::LDU1_VOICE2:
                {
                    ::memcpy(m_call->netLDU1 + 26U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                }
                break;
                case DFSIFrameType::LDU1_VOICE3:
                {
                    ::memcpy(m_call->netLDU1 + 55U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                    if (voice.additionalData != nullptr) {
                        m_call->lco = voice.additionalData[0U];
                        m_call->mfId = voice.additionalData[1U];
                        m_call->serviceOptions = voice.additionalData[2U];
                    } else {
                        LogWarning(LOG_MODEM, "V.24/DFSI VC3 traffic missing metadata");
                    }
                }
                break;
                case DFSIFrameType::LDU1_VOICE4:
                {
                    ::memcpy(m_call->netLDU1 + 80U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                    if (voice.additionalData != nullptr) {
                        m_call->dstId = __GET_UINT16(voice.additionalData, 0U);
                    } else {
                        LogWarning(LOG_MODEM, "V.24/DFSI VC4 traffic missing metadata");
                    }
                }
                break;
                case DFSIFrameType::LDU1_VOICE5:
                {
                    ::memcpy(m_call->netLDU1 + 105U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                    if (voice.additionalData != nullptr) {
                        m_call->srcId = __GET_UINT16(voice.additionalData, 0U);
                    } else {
                        LogWarning(LOG_MODEM, "V.24/DFSI VC5 traffic missing metadata");
                    }
                }
                break;
                case DFSIFrameType::LDU1_VOICE6:
                {
                    ::memcpy(m_call->netLDU1 + 130U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                }
                break;
                case DFSIFrameType::LDU1_VOICE7:
                {
                    ::memcpy(m_call->netLDU1 + 155U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                }
                break;
                case DFSIFrameType::LDU1_VOICE8:
                {
                    ::memcpy(m_call->netLDU1 + 180U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                }
                break;
                case DFSIFrameType::LDU1_VOICE9:
                {
                    ::memcpy(m_call->netLDU1 + 204U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                    if (voice.additionalData != nullptr) {
                        m_call->lsd1 = voice.additionalData[0U];
                        m_call->lsd2 = voice.additionalData[1U];
                    } else {
                        LogWarning(LOG_MODEM, "V.24/DFSI VC9 traffic missing metadata");
                    }
                }
                break;
                case DFSIFrameType::LDU2_VOICE11:
                {
                    ::memcpy(m_call->netLDU2 + 26U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                }
                break;
                case DFSIFrameType::LDU2_VOICE12:
                {
                    ::memcpy(m_call->netLDU2 + 55U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                    if (voice.additionalData != nullptr) {
                        ::memcpy(m_call->MI, voice.additionalData, 3U);
                    } else {
                        LogWarning(LOG_MODEM, "V.24/DFSI VC12 traffic missing metadata");
                    }
                }
                break;
                case DFSIFrameType::LDU2_VOICE13:
                {
                    ::memcpy(m_call->netLDU2 + 80U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                    if (voice.additionalData != nullptr) {
                        ::memcpy(m_call->MI + 3U, voice.additionalData, 3U);
                    } else {
                        LogWarning(LOG_MODEM, "V.24/DFSI VC13 traffic missing metadata");
                    }
                }
                break;
                case DFSIFrameType::LDU2_VOICE14:
                {
                    ::memcpy(m_call->netLDU2 + 105U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                    if (voice.additionalData != nullptr) {
                        ::memcpy(m_call->MI + 6U, voice.additionalData, 3U);
                    } else {
                        LogWarning(LOG_MODEM, "V.24/DFSI VC14 traffic missing metadata");
                    }
                }
                break;
                case DFSIFrameType::LDU2_VOICE15:
                {
                    ::memcpy(m_call->netLDU2 + 130U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                    if (voice.additionalData != nullptr) {
                        m_call->algoId = voice.additionalData[0U];
                        m_call->kId = __GET_UINT16B(voice.additionalData, 1U);
                    } else {
                        LogWarning(LOG_MODEM, "V.24/DFSI VC15 traffic missing metadata");
                    }
                }
                break;
                case DFSIFrameType::LDU2_VOICE16:
                {
                    ::memcpy(m_call->netLDU2 + 155U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                }
                break;
                case DFSIFrameType::LDU2_VOICE17:
                {
                    ::memcpy(m_call->netLDU2 + 180U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                }
                break;
                case DFSIFrameType::LDU2_VOICE18:
                {
                    ::memcpy(m_call->netLDU2 + 204U, voice.imbeData, RAW_IMBE_LENGTH_BYTES);
                    if (voice.additionalData != nullptr) {
                        m_call->lsd1 = voice.additionalData[0U];
                        m_call->lsd2 = voice.additionalData[1U];
                    } else {
                        LogWarning(LOG_SERIAL, "V.24/DFSI VC18 traffic missing metadata");
                    }
                }
                break;
                default:
                break;
            }

            // increment our voice frame counter
            m_call->n++;
        }
        break;
    }

    // encode LDU1 if ready
    if (m_call->n == 9U) {
        lc::LC lc = lc::LC();
        lc.setLCO(m_call->lco);
        lc.setMFId(m_call->mfId);

        if (lc.isStandardMFId()) {
            lc.setSrcId(m_call->srcId);
            lc.setDstId(m_call->dstId);
        } else {
            uint8_t rsBuffer[P25_LDU_LC_FEC_LENGTH_BYTES];
            ::memset(rsBuffer, 0x00U, P25_LDU_LC_FEC_LENGTH_BYTES);

            rsBuffer[0U] = m_call->lco;
            rsBuffer[1U] = m_call->mfId;
            rsBuffer[2U] = m_call->serviceOptions;
            rsBuffer[3U] = (m_call->dstId >> 16) & 0xFFU;
            rsBuffer[4U] = (m_call->dstId >> 8) & 0xFFU;
            rsBuffer[5U] = (m_call->dstId >> 0) & 0xFFU;
            rsBuffer[6U] = (m_call->srcId >> 16) & 0xFFU;
            rsBuffer[7U] = (m_call->srcId >> 8) & 0xFFU;
            rsBuffer[8U] = (m_call->srcId >> 0) & 0xFFU;

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

        bool emergency = ((m_call->serviceOptions & 0xFFU) & 0x80U) == 0x80U;    // Emergency Flag
        bool encryption = ((m_call->serviceOptions & 0xFFU) & 0x40U) == 0x40U;   // Encryption Flag
        uint8_t priority = ((m_call->serviceOptions & 0xFFU) & 0x07U);           // Priority
        lc.setEmergency(emergency);
        lc.setEncrypted(encryption);
        lc.setPriority(priority);

        data::LowSpeedData lsd = data::LowSpeedData();
        lsd.setLSD1(m_call->lsd1);
        lsd.setLSD2(m_call->lsd2);

        // generate Sync
        Sync::addP25Sync(buffer + 2U);

        // generate NID
        m_nid.encode(buffer + 2U, DUID::LDU1);

        // generate LDU1 Data
        lc.encodeLDU1(buffer + 2U);

        // generate Low Speed Data
        lsd.process(buffer + 2U);

        // generate audio
        m_audio.encode(buffer + 2U, m_call->netLDU1 + 10U, 0U);
        m_audio.encode(buffer + 2U, m_call->netLDU1 + 26U, 1U);
        m_audio.encode(buffer + 2U, m_call->netLDU1 + 55U, 2U);
        m_audio.encode(buffer + 2U, m_call->netLDU1 + 80U, 3U);
        m_audio.encode(buffer + 2U, m_call->netLDU1 + 105U, 4U);
        m_audio.encode(buffer + 2U, m_call->netLDU1 + 130U, 5U);
        m_audio.encode(buffer + 2U, m_call->netLDU1 + 155U, 6U);
        m_audio.encode(buffer + 2U, m_call->netLDU1 + 180U, 7U);
        m_audio.encode(buffer + 2U, m_call->netLDU1 + 204U, 8U);

        // add busy bits
        P25Utils::addStatusBits(buffer + 2U, P25_LDU_FRAME_LENGTH_BITS, true);

        buffer[0U] = modem::TAG_DATA;
        buffer[1U] = 0x01U;
        storeConvertedRx(buffer, P25_LDU_FRAME_LENGTH_BYTES + 2U);
    }
    
    // encode LDU2 if ready
    if (m_call->n == 18U) {
        lc::LC lc = lc::LC();
        lc.setMI(m_call->MI);
        lc.setAlgId(m_call->algoId);
        lc.setKId(m_call->kId);

        data::LowSpeedData lsd = data::LowSpeedData();
        lsd.setLSD1(m_call->lsd1);
        lsd.setLSD2(m_call->lsd2);

        // generate Sync
        Sync::addP25Sync(buffer + 2U);

        // generate NID
        m_nid.encode(buffer + 2U, DUID::LDU2);

        // generate LDU2 data
        lc.encodeLDU2(buffer + 2U);

        // generate Low Speed Data
        lsd.process(buffer + 2U);

        // generate audio
        m_audio.encode(buffer + 2U, m_call->netLDU2 + 10U, 0U);
        m_audio.encode(buffer + 2U, m_call->netLDU2 + 26U, 1U);
        m_audio.encode(buffer + 2U, m_call->netLDU2 + 55U, 2U);
        m_audio.encode(buffer + 2U, m_call->netLDU2 + 80U, 3U);
        m_audio.encode(buffer + 2U, m_call->netLDU2 + 105U, 4U);
        m_audio.encode(buffer + 2U, m_call->netLDU2 + 130U, 5U);
        m_audio.encode(buffer + 2U, m_call->netLDU2 + 155U, 6U);
        m_audio.encode(buffer + 2U, m_call->netLDU2 + 180U, 7U);
        m_audio.encode(buffer + 2U, m_call->netLDU2 + 204U, 8U);

        // add busy bits
        P25Utils::addStatusBits(buffer + 2U, P25_LDU_FRAME_LENGTH_BITS, true);

        buffer[0U] = modem::TAG_DATA;
        buffer[1U] = 0x00U;
        storeConvertedRx(buffer, P25_LDU_FRAME_LENGTH_BYTES + 2U);

        m_call->n = 0;
    }
}

/* Helper to add a V.24 data frame to the P25 TX queue with the proper timestamp and formatting */

void ModemV24::queueP25Frame(uint8_t* data, uint16_t len, SERIAL_TX_TYPE msgType)
{
    assert(data != nullptr);
    assert(len > 0U);

    // get current time in ms
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // timestamp for this message (in ms)
    uint64_t msgTime = 0U;

    // if this is our first message, timestamp is just now + the jitter buffer offset in ms
    if (m_lastP25Tx == 0U) {
        msgTime = now + m_jitter;
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
    m_callInProgress = true;

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
    __SET_UINT16B(control.getKId(), vhdr, 11U);
    __SET_UINT16B(control.getDstId(), vhdr, 13U);

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

    m_callInProgress = false;
}

/* Internal helper to convert from TIA-102 air interface to V.24/DFSI. */

void ModemV24::convertFromAir(uint8_t* data, uint32_t length)
{
    uint8_t ldu[9U * 25U];
    ::memset(ldu, 0x00U, 9 * 25U);

    // decode the NID
    bool valid = m_nid.decode(data + 2U);
    if (!valid)
        return;

    DUID::E duid = m_nid.getDUID();

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
            bool ret = lc.decodeLDU1(data + 2U);
            if (!ret) {
                LogWarning(LOG_MODEM, P25_LDU1_STR ", undecodable LC");
                return;
            }

            lsd.process(data + 2U);

            // late entry?
            if (!m_callInProgress) {
                startOfStream(lc);
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
            endOfStream();
            break;

        case DUID::PDU:
            break;

        case DUID::TSDU:
        {
            std::unique_ptr<lc::TSBK> tsbk = lc::tsbk::TSBKFactory::createTSBK(data + 2U);
            if (tsbk == nullptr) {
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

            queueP25Frame(startBuf, MotStartOfStream::LENGTH, STT_NON_IMBE);

            MotTSBKFrame tf = MotTSBKFrame();
            tf.startOfStream->setStartStop(StartStopFlag::START);
            tf.startOfStream->setRT(m_rtrt ? RTFlag::ENABLED : RTFlag::DISABLED);
            tf.startOfStream->setStreamType(StreamTypeFlag::TSBK);
            delete[] tf.tsbkData;
            tf.tsbkData = tsbk->getDecodedRaw();

            // create buffer and encode
            uint8_t tsbkBuf[MotTSBKFrame::LENGTH];
            ::memset(tsbkBuf, 0x00U, MotTSBKFrame::LENGTH);
            tf.encode(tsbkBuf);

            if (m_trace)
                Utils::dump(1U, "ModemV24::convertFromAir() MotTSBKFrame", tsbkBuf, MotTSBKFrame::LENGTH);

            queueP25Frame(tsbkBuf, MotTSBKFrame::LENGTH, STT_NON_IMBE);

            MotStartOfStream endOfStream = MotStartOfStream();
            endOfStream.setStartStop(StartStopFlag::STOP);
            endOfStream.setRT(m_rtrt ? RTFlag::ENABLED : RTFlag::DISABLED);
            endOfStream.setStreamType(StreamTypeFlag::TSBK);

            // create buffer and encode
            uint8_t endBuf[MotStartOfStream::LENGTH];
            ::memset(endBuf, 0x00U, MotStartOfStream::LENGTH);
            endOfStream.encode(endBuf);

            queueP25Frame(endBuf, MotStartOfStream::LENGTH, STT_NON_IMBE);
            queueP25Frame(endBuf, MotStartOfStream::LENGTH, STT_NON_IMBE);
        }
        break;

        default:
            break;
    }

    if (duid == DUID::LDU1 || duid == DUID::LDU2) {
        uint8_t rs[P25_LDU_LC_FEC_LENGTH_BYTES];
        ::memset(rs, 0x00U, P25_LDU_LC_FEC_LENGTH_BYTES);

        if (duid == DUID::LDU1) {
            if (lc.isStandardMFId()) {
                uint8_t serviceOptions =
                    (lc.getEmergency() ? 0x80U : 0x00U) +
                    (lc.getEncrypted() ? 0x40U : 0x00U) +
                    (lc.getPriority() & 0x07U);

                rs[0U] = lc.getLCO();                                               // LCO
                rs[1U] = lc.getMFId();                                              // MFId
                rs[2U] = serviceOptions;                                            // Service Options
                uint32_t dstId = lc.getDstId();
                rs[3U] = (dstId >> 16) & 0xFFU;                                     // Target Address
                rs[4U] = (dstId >> 8) & 0xFFU;
                rs[5U] = (dstId >> 0) & 0xFFU;
                uint32_t srcId = lc.getSrcId();
                rs[6U] = (srcId >> 16) & 0xFFU;                                     // Source Address
                rs[7U] = (srcId >> 8) & 0xFFU;
                rs[8U] = (srcId >> 0) & 0xFFU;
            } else {
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
            }

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
