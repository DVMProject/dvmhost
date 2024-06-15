// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / DFSI peer application
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Patrick McDonnell, W3AXL
*
*/

#include "common/p25/lc/tdulc/TDULCFactory.h"

#include "network/SerialService.h"

#include "dfsi/ActivityLog.h"

using namespace network;
using namespace modem;
using namespace p25;
using namespace dfsi;

SerialService::SerialService(const std::string& portName, uint32_t baudrate, bool rtrt, bool diu, uint16_t jitter, DfsiPeerNetwork* network, uint32_t p25TxQueueSize, uint32_t p25RxQueueSize, bool debug, bool trace) :
    m_portName(portName),
    m_baudrate(baudrate),
    m_rtrt(rtrt),
    m_diu(diu),
    m_port(),
    m_jitter(jitter),
    m_debug(debug),
    m_trace(trace),
    m_network(network),
    m_lastIMBE(nullptr),
    m_lastHeard(),
    m_sequences(),
    m_msgBuffer(nullptr),
    m_msgState(RESP_START),
    m_msgLength(0U),
    m_msgOffset(0U),
    m_msgType(CMD_GET_STATUS),
    m_msgDoubleLength(false),
    m_netFrames(0U),
    m_netLost(0U),
    m_rxP25Queue(p25RxQueueSize, "RX P25 Queue"),
    m_txP25Queue(p25TxQueueSize, "TX P25 Queue"),
    m_lastP25Tx(0U),
    m_rs(),
    m_rxP25LDUCounter(0U),
    m_netCallInProgress(false),
    m_lclCallInProgress(false),
    m_rxVoiceControl(nullptr),
    m_rxVoiceLsd(nullptr),
    m_rxVoiceCallData(nullptr)
{
    assert(!portName.empty());
    assert(baudrate > 0U);

    // Setup serial
    port::SERIAL_SPEED serialSpeed = port::SERIAL_115200;

    m_port = new port::UARTPort(portName, serialSpeed, false);

    m_lastIMBE = new uint8_t[11U];
    ::memcpy(m_lastIMBE, P25_NULL_IMBE, 11U);

    m_msgBuffer = new uint8_t[BUFFER_LENGTH];
}

SerialService::~SerialService()
{
    if (m_port != nullptr) {
        delete m_port;
    }

    // Delete our buffers
    delete[] m_lastIMBE;
    delete[] m_msgBuffer;

    // Delete our rx P25 objects
    delete m_rxVoiceControl;
    delete m_rxVoiceLsd;
    delete m_rxVoiceCallData;
}

void SerialService::clock(uint32_t ms)
{
    // Get data from serial port
    RESP_TYPE_DVM type = readSerial();

    // Handle what we got
    if (type == RTM_TIMEOUT) {
        // Do nothing
    }
    else if (type == RTM_ERROR) {
        // Also do nothing
    }
    else {
        // Get cmd byte offset
        uint8_t cmdOffset = 2U;
        if (m_msgDoubleLength)
            cmdOffset = 3U;
        
        // Get command type
        switch (m_msgBuffer[2U]) {

            // P25 data is handled identically to the dvmhost modem
            case CMD_P25_DATA:
            {   
                // Get length
                uint8_t length[2U];
                if (m_msgLength > 255U)
                    length[0U] = ((m_msgLength - cmdOffset) >> 8U) & 0xFFU;
                else
                    length[0U] = 0x00U;
                length[1U] = (m_msgLength - cmdOffset) & 0xFFU;
                m_rxP25Queue.addData(length, 2U);

                // Add data tag to queue
                uint8_t data = TAG_DATA;
                m_rxP25Queue.addData(&data, 1U);

                // Add P25 data to buffer
                m_rxP25Queue.addData(m_msgBuffer + (cmdOffset + 1U), m_msgLength - (cmdOffset + 1U));

                if (m_debug) {
                    LogDebug(LOG_SERIAL, "Got P25 data from V24 board (len: %u)", m_msgLength);
                }
            }
            break;

            // P25 data lost is also handled, though the V24 board doesn't implement it (yet?)
            case CMD_P25_LOST:
            {
                if (m_debug)
                    LogDebug(LOG_SERIAL, "Got P25 lost msg from V24 board");

                if (m_msgDoubleLength) {
                    LogError(LOG_SERIAL, "CMD_P25_LOST got double length byte?");
                    break;
                }

                uint8_t data = 1U;
                m_rxP25Queue.addData(&data, 1U);

                data = TAG_LOST;
                m_rxP25Queue.addData(&data, 1U);
            }
            break;

            // Handle debug messages
            case CMD_DEBUG1:
            case CMD_DEBUG2:
            case CMD_DEBUG3:
            case CMD_DEBUG4:
            case CMD_DEBUG5:
            case CMD_DEBUG_DUMP:
                printDebug(m_msgBuffer, m_msgLength);
                break;

            // Fallback if we get a message we have no clue how to handle
            default:
            {
                LogError(LOG_SERIAL, "Unhandled command from V24 board: %02X", m_msgBuffer[2U]);
            }
        }
    }

    // Write anything waiting to the serial port
    int out = writeSerial();
    if (m_trace && out > 0) {
        LogDebug(LOG_SERIAL, "Wrote %u-byte message to the serial V24 device", out);
    } else if (out < 0) {
        LogError(LOG_SERIAL, "Failed to write to serial port!");
    }
}

bool SerialService::open()
{
    LogInfoEx(LOG_SERIAL, "Opening port %s at %u baud", m_portName.c_str(), m_baudrate);

    bool ret = m_port->open();

    if (!ret) {
        LogError(LOG_SERIAL, "Failed to open port!");
        return false;
    }

    m_msgState = RESP_START;

    return true;
}

void SerialService::close()
{
    LogInfoEx(LOG_SERIAL, "Closing port");
    m_port->close();
}

/// <summary>
/// Process P25 data from the peer network and send to writeP25Frame()
/// </summary>
void SerialService::processP25FromNet(UInt8Array p25Buffer, uint32_t length)
{
    // If there's a local call in progress, ignore the frames
    if (m_lclCallInProgress) {
        LogWarning(LOG_SERIAL, "Local call in progress, ignoring frames from network");
        return;
    }

    // Decode grant info
    bool grantDemand = (p25Buffer[14U] & 0x80U) == 0x80U;
    
    // We don't use these (yet?)
    //bool grantDenial = (p25Buffer[14U] & 0x40U) == 0x40U;
    //bool unitToUnit = (p25Buffer[14U] & 0x01U) == 0x01U;

    // Decode network header
    uint8_t duid = p25Buffer[22U];
    uint8_t mfid = p25Buffer[15U];

    // Setup P25 data handlers
    UInt8Array data;
    uint8_t frameLength = p25Buffer[23U];

    // Handle PDUs
    if (duid == p25::P25_DUID_PDU) {
        frameLength = length;
        data = std::unique_ptr<uint8_t[]>(new uint8_t[length]);
        ::memset(data.get(), 0x00U, length);
        ::memcpy(data.get(), p25Buffer.get(), length);
        LogInfoEx(LOG_SERIAL, "Got P25 PDU, we don't handle these (yet)");
    }
    // Handle everything else
    else {
        if (frameLength <= 24) {
            data = std::unique_ptr<uint8_t[]>(new uint8_t[frameLength]);
            ::memset(data.get(), 0x00U, frameLength);
        }
        else {
            data = std::unique_ptr<uint8_t[]>(new uint8_t[frameLength]);
            ::memset(data.get(), 0x00U, frameLength);
            ::memcpy(data.get(), p25Buffer.get() + 24U, frameLength);
        }
    }

    // Get basic info
    uint8_t lco = p25Buffer[4U];
    uint32_t srcId = __GET_UINT16(p25Buffer, 5U);
    uint32_t dstId = __GET_UINT16(p25Buffer, 8U);
    uint32_t sysId = (p25Buffer[11U] << 8) | (p25Buffer[12U] << 0);
    uint32_t netId = __GET_UINT16(p25Buffer, 16U);
    uint8_t lsd1 = p25Buffer[20U];
    uint8_t lsd2 = p25Buffer[21U];
    uint8_t frameType = p25::P25_FT_DATA_UNIT;

    // Default any 0's
    if (netId == 0U) {
        netId = lc::LC::getSiteData().netId();
    }

    if (sysId == 0U) {
        sysId = lc::LC::getSiteData().sysId();
    }

    if (m_debug) {
        LogDebug(LOG_NET, "P25, duid = $%02X, lco = $%02X, MFId = $%02X, srcId = %u, dstId = %u, len = %u", duid, lco, mfid, srcId, dstId, length);
    }

    lc::LC control;
    data::LowSpeedData lsd;

    // is this a LDU1, is this the first of a call?
    if (duid == p25::P25_DUID_LDU1) {
        frameType = p25Buffer[180U];

        if (m_debug) {
            LogDebug(LOG_NET, "P25, frameType = $%02X", frameType);
        }

        if (frameType == p25::P25_FT_HDU_VALID) {
            uint8_t algId = p25Buffer[181U];
            uint32_t kid = (p25Buffer[182U] << 8) | (p25Buffer[183U] << 0);

            // copy MI data
            uint8_t mi[p25::P25_MI_LENGTH_BYTES];
            ::memset(mi, 0x00U, p25::P25_MI_LENGTH_BYTES);

            for (uint8_t i = 0; i < p25::P25_MI_LENGTH_BYTES; i++) {
                mi[i] = p25Buffer[184U + i];
            }

            if (m_debug) {
                LogDebug(LOG_NET, "P25, HDU algId = $%02X, kId = $%04X", algId, kid);
            }
            /*if (m_trace) {
                Utils::dump(1U, "P25 HDU Network MI", mi, p25::P25_MI_LENGTH_BYTES);
            }*/

            control.setAlgId(algId);
            control.setKId(kid);
            control.setMI(mi);
        }
    }

    control.setLCO(lco);
    control.setSrcId(srcId);
    control.setDstId(dstId);
    control.setMFId(mfid);

    control.setNetId(netId);
    control.setSysId(sysId);

    lsd.setLSD1(lsd1);
    lsd.setLSD2(lsd2);

    /*if (m_trace) {
        Utils::dump(2U, "!!! *P25 Network Frame", data.get(), frameLength);
    }*/

    uint8_t* message = data.get();

    //Utils::dump(2U, "!!! *P25 Network Frame", message, frameLength);

    // forward onto the specific processor for final processing and delivery
    switch (duid) {
        case P25_DUID_LDU1:
            {
                if ((message[0U] == dfsi::P25_DFSI_LDU1_VOICE1) && (message[22U] == dfsi::P25_DFSI_LDU1_VOICE2) &&
                    (message[36U] == dfsi::P25_DFSI_LDU1_VOICE3) && (message[53U] == dfsi::P25_DFSI_LDU1_VOICE4) &&
                    (message[70U] == dfsi::P25_DFSI_LDU1_VOICE5) && (message[87U] == dfsi::P25_DFSI_LDU1_VOICE6) &&
                    (message[104U] == dfsi::P25_DFSI_LDU1_VOICE7) && (message[121U] == dfsi::P25_DFSI_LDU1_VOICE8) &&
                    (message[138U] == dfsi::P25_DFSI_LDU1_VOICE9)) {
                    uint32_t count = 0U;
                    dfsi::LC dfsiLC = dfsi::LC(control, lsd);

                    uint8_t netLDU1[9U * 25U];
                    ::memset(netLDU1, 0x00U, 9U * 25U);

                    dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE1);
                    dfsiLC.decodeLDU1(message + count, netLDU1 + 10U);
                    count += dfsi::P25_DFSI_LDU1_VOICE1_FRAME_LENGTH_BYTES;

                    dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE2);
                    dfsiLC.decodeLDU1(message + count, netLDU1 + 26U);
                    count += dfsi::P25_DFSI_LDU1_VOICE2_FRAME_LENGTH_BYTES;

                    dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE3);
                    dfsiLC.decodeLDU1(message + count, netLDU1 + 55U);
                    count += dfsi::P25_DFSI_LDU1_VOICE3_FRAME_LENGTH_BYTES;

                    dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE4);
                    dfsiLC.decodeLDU1(message + count, netLDU1 + 80U);
                    count += dfsi::P25_DFSI_LDU1_VOICE4_FRAME_LENGTH_BYTES;

                    dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE5);
                    dfsiLC.decodeLDU1(message + count, netLDU1 + 105U);
                    count += dfsi::P25_DFSI_LDU1_VOICE5_FRAME_LENGTH_BYTES;

                    dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE6);
                    dfsiLC.decodeLDU1(message + count, netLDU1 + 130U);
                    count += dfsi::P25_DFSI_LDU1_VOICE6_FRAME_LENGTH_BYTES;

                    dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE7);
                    dfsiLC.decodeLDU1(message + count, netLDU1 + 155U);
                    count += dfsi::P25_DFSI_LDU1_VOICE7_FRAME_LENGTH_BYTES;

                    dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE8);
                    dfsiLC.decodeLDU1(message + count, netLDU1 + 180U);
                    count += dfsi::P25_DFSI_LDU1_VOICE8_FRAME_LENGTH_BYTES;

                    dfsiLC.setFrameType(dfsi::P25_DFSI_LDU1_VOICE9);
                    dfsiLC.decodeLDU1(message + count, netLDU1 + 204U);
                    count += dfsi::P25_DFSI_LDU1_VOICE9_FRAME_LENGTH_BYTES;

                    control = lc::LC(*dfsiLC.control());

                    // Override the source/destination from the FNE RTP header (handles rewrites properly)
                    control.setSrcId(srcId);
                    control.setDstId(dstId);

                    LogInfoEx(LOG_NET, P25_LDU1_STR " audio, srcId = %u, dstId = %u, group = %u, emerg = %u, encrypt = %u, prio = %u",
                        control.getSrcId(), control.getDstId(), control.getGroup(), control.getEmergency(), control.getEncrypted(), control.getPriority());

                    //Utils::dump("P25 LDU1 from net", netLDU1, 9U * 25U);

                    writeP25Frame(duid, dfsiLC, netLDU1);
                }
            }
            break;
        case P25_DUID_LDU2:
            {
                if ((message[0U] == dfsi::P25_DFSI_LDU2_VOICE10) && (message[22U] == dfsi::P25_DFSI_LDU2_VOICE11) &&
                    (message[36U] == dfsi::P25_DFSI_LDU2_VOICE12) && (message[53U] == dfsi::P25_DFSI_LDU2_VOICE13) &&
                    (message[70U] == dfsi::P25_DFSI_LDU2_VOICE14) && (message[87U] == dfsi::P25_DFSI_LDU2_VOICE15) &&
                    (message[104U] == dfsi::P25_DFSI_LDU2_VOICE16) && (message[121U] == dfsi::P25_DFSI_LDU2_VOICE17) &&
                    (message[138U] == dfsi::P25_DFSI_LDU2_VOICE18)) {
                    uint32_t count = 0U;
                    dfsi::LC dfsiLC = dfsi::LC(control, lsd);

                    uint8_t netLDU2[9U * 25U];
                    ::memset(netLDU2, 0x00U, 9U * 25U);

                    dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE10);
                    dfsiLC.decodeLDU2(message + count, netLDU2 + 10U);
                    count += dfsi::P25_DFSI_LDU2_VOICE10_FRAME_LENGTH_BYTES;

                    dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE11);
                    dfsiLC.decodeLDU2(message + count, netLDU2 + 26U);
                    count += dfsi::P25_DFSI_LDU2_VOICE11_FRAME_LENGTH_BYTES;

                    dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE12);
                    dfsiLC.decodeLDU2(message + count, netLDU2 + 55U);
                    count += dfsi::P25_DFSI_LDU2_VOICE12_FRAME_LENGTH_BYTES;

                    dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE13);
                    dfsiLC.decodeLDU2(message + count, netLDU2 + 80U);
                    count += dfsi::P25_DFSI_LDU2_VOICE13_FRAME_LENGTH_BYTES;

                    dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE14);
                    dfsiLC.decodeLDU2(message + count, netLDU2 + 105U);
                    count += dfsi::P25_DFSI_LDU2_VOICE14_FRAME_LENGTH_BYTES;

                    dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE15);
                    dfsiLC.decodeLDU2(message + count, netLDU2 + 130U);
                    count += dfsi::P25_DFSI_LDU2_VOICE15_FRAME_LENGTH_BYTES;

                    dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE16);
                    dfsiLC.decodeLDU2(message + count, netLDU2 + 155U);
                    count += dfsi::P25_DFSI_LDU2_VOICE16_FRAME_LENGTH_BYTES;

                    dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE17);
                    dfsiLC.decodeLDU2(message + count, netLDU2 + 180U);
                    count += dfsi::P25_DFSI_LDU2_VOICE17_FRAME_LENGTH_BYTES;

                    dfsiLC.setFrameType(dfsi::P25_DFSI_LDU2_VOICE18);
                    dfsiLC.decodeLDU2(message + count, netLDU2 + 204U);
                    count += dfsi::P25_DFSI_LDU2_VOICE18_FRAME_LENGTH_BYTES;

                    control = lc::LC(*dfsiLC.control());
                    LogInfoEx(LOG_NET, P25_LDU2_STR " audio, algo = $%02X, kid = $%04X", control.getAlgId(), control.getKId());

                    //Utils::dump("P25 LDU2 from net", netLDU2, 9U * 25U);

                    writeP25Frame(duid, dfsiLC, netLDU2);
                }
            }
            break;

        case P25_DUID_TSDU:
            //processTSDU(data.get(), frameLength, control, lsd); // We don't handle TSDUs right now
            break;

        case P25_DUID_TDU:
        case P25_DUID_TDULC:
            {
                if (duid == P25_DUID_TDULC) {
                    std::unique_ptr<lc::TDULC> tdulc = lc::tdulc::TDULCFactory::createTDULC(data.get());
                    if (tdulc == nullptr) {
                        LogWarning(LOG_NET, P25_TDULC_STR ", undecodable TDULC");
                    }
                    else {
                        if (tdulc->getLCO() != LC_CALL_TERM)
                            break;
                    }
                }

                // is this an TDU with a grant demand?
                if (duid == P25_DUID_TDU && grantDemand) {
                    break; // ignore grant demands
                }

                // Log print
                LogInfoEx(LOG_NET, P25_TDU_STR ", srcId = %u, dstId = %u", srcId, dstId);

                // End the call
                endOfStream();

                // Update our sequence number
                m_sequences[dstId] = RTP_END_OF_CALL_SEQ;
            }
            break;
    }
}

/// <summary>
/// Retrieve and process a P25 frame from the rx P25 queue
/// </summary>
/// This function pieces together LDU1/LDU2 messages from individual DFSI frames received over the serial port
/// It's called multiple times before an LDU is sent, and each time adds more data pieces to the LDUs
void SerialService::processP25ToNet()
{

    // Buffer to store the retrieved P25 frame
    uint8_t data[P25_PDU_FRAME_LENGTH_BYTES * 2U];

    // Get a P25 frame from the RX queue
    uint32_t len = readP25Frame(data);

    // If we didn't read anything, return
    if (len <= 0U) {
        return;
    }

    // If there's already a call from the network in progress, ignore any additional frames comfing from the V24 interface
    if (m_netCallInProgress) {
        LogWarning(LOG_SERIAL, "Remote call in progress, ignoring frames from V24");
        return;
    }

    //LogDebug(LOG_SERIAL, "processP25ToNet() got data (len: %u)", len);

    /*if (m_trace) {
        Utils::dump(1U, "data: ", data, len);
    }*/

    // Create a new link control object if needed
    if (m_rxVoiceControl == nullptr) {
        m_rxVoiceControl = new lc::LC();
    }

    // Create a new lsd object if needed
    if (m_rxVoiceLsd == nullptr) {
        m_rxVoiceLsd = new data::LowSpeedData();
    }

    // Create a new call data object if needed
    if (m_rxVoiceCallData == nullptr) {
        m_rxVoiceCallData = new VoiceCallData();
    }

    // Parse out the data
    uint8_t tag = data[0U];

    // Sanity check
    if (tag != TAG_DATA) {
        LogError(LOG_SERIAL, "Unexpected data tag in RX P25 frame buffer: 0x%02X", tag);
        return;
    }

    // Get the DFSI data (skip the 0x00 padded byte at the start)
    uint8_t dfsiData[len - 2];
    ::memset(dfsiData, 0x00U, len - 2);
    ::memcpy(dfsiData, data + 2U, len - 2);

    // Extract DFSI frame type
    uint8_t frameType = dfsiData[0U];

    //LogDebug(LOG_SERIAL, "Handling DFSI frameType 0x%02X", frameType);

    // Switch based on DFSI frame type
    switch (frameType) {
        // Start/Stop Frame
        case P25_DFSI_MOT_START_STOP:
        {
            // Decode the frame
            MotStartOfStream start = MotStartOfStream(dfsiData);
            // Handle start/stop
            if (start.startStop == StartStopFlag::START) {
                // Flag we have a local call (i.e. from V24) in progress
                m_lclCallInProgress = true;
                // Reset the call data (just in case)
                m_rxVoiceCallData->resetCallData();
                // Generate a new random stream ID
                m_rxVoiceCallData->newStreamId();
                // Log
                LogInfoEx(LOG_SERIAL, "V24 CALL START [STREAM ID %u]", m_rxVoiceCallData->streamId);
            } else {
                if (m_lclCallInProgress) {
                    // Flag call over
                    m_lclCallInProgress = false;
                    // Log
                    LogInfoEx(LOG_SERIAL, "V24 CALL END");
                    // Send the TDU (using call data which we hope has been filled earlier)
                    m_network->writeP25TDU(*m_rxVoiceControl, *m_rxVoiceLsd);
                    // Reset
                    m_rxVoiceCallData->resetCallData();
                }
            }
        }
        break;
        // VHDR 1 Frame
        case P25_DFSI_MOT_VHDR_1:
        {
            // Decode
            MotVoiceHeader1 vhdr1 = MotVoiceHeader1(dfsiData);

            // Copy to call data VHDR1
            m_rxVoiceCallData->VHDR1 = new uint8_t[vhdr1.HCW_LENGTH];
            ::memcpy(m_rxVoiceCallData->VHDR1, vhdr1.header, vhdr1.HCW_LENGTH);
            // Debug Log
            if (m_debug) {
                LogDebug(LOG_SERIAL, "V24 VHDR1 [STREAM ID %u]", m_rxVoiceCallData->streamId);
            }
        }
        break;
        // VHDR 2 Frame
        case P25_DFSI_MOT_VHDR_2:
        {
            // Decode
            MotVoiceHeader2 vhdr2 = MotVoiceHeader2(dfsiData);
            // Copy to call data
            ::memcpy(m_rxVoiceCallData->VHDR2, vhdr2.header, vhdr2.HCW_LENGTH);
            // Debug Log
            if (m_debug) {
                LogDebug(LOG_SERIAL, "V24 VHDR2 [STREAM ID %u]", m_rxVoiceCallData->streamId);
            }

            // Buffer for raw VHDR data
            uint8_t raw[P25_DFSI_VHDR_RAW_LEN];
            // Get VHDR1 data
            ::memcpy(raw, m_rxVoiceCallData->VHDR1, 8U);
            ::memcpy(raw + 8U, m_rxVoiceCallData->VHDR1 + 9U, 8U);
            ::memcpy(raw + 16U, m_rxVoiceCallData->VHDR1 + 18U, 2U);
            // Get VHDR2 data
            ::memcpy(raw + 18U, m_rxVoiceCallData->VHDR2, 8U);
            ::memcpy(raw + 26U, m_rxVoiceCallData->VHDR2 + 9U, 8U);
            ::memcpy(raw + 34U, m_rxVoiceCallData->VHDR2 + 18U, 2U);

            // Buffer for decoded VHDR data
            uint8_t vhdr[P25_DFSI_VHDR_LEN];

            // Copy over the data, decoding hex with the weird bit stuffing nonsense
            uint offset = 0U;
            for (uint32_t i = 0; i < P25_DFSI_VHDR_RAW_LEN; i++, offset += 6)
                Utils::hex2Bin(raw[i], vhdr, offset);

            // Try to decode the RS data
            try {
                bool ret = m_rs.decode362017(vhdr);
                if (!ret) {
                    LogError(LOG_SERIAL, "V24 traffic failed to decode RS (36,20,17) FEC [STREAM ID %u]", m_rxVoiceCallData->streamId);
                } else {
                    // Copy Message Indicator
                    ::memcpy(m_rxVoiceCallData->mi, vhdr, P25_MI_LENGTH_BYTES);
                    // Get additional info
                    m_rxVoiceCallData->mfId = vhdr[9U];
                    m_rxVoiceCallData->algoId = vhdr[10U];
                    m_rxVoiceCallData->kId = __GET_UINT16B(vhdr, 11U);
                    m_rxVoiceCallData->dstId = __GET_UINT16B(vhdr, 13U);
                }
                // Log if we decoded succesfully
                if (m_debug) {
                    LogDebug(LOG_SERIAL, "P25, HDU algId = $%02X, kId = $%04X, dstId = $%04X", m_rxVoiceCallData->algoId, m_rxVoiceCallData->kId, m_rxVoiceCallData->dstId);
                }
            }
            catch (...) {
                LogError(LOG_SERIAL, "V24 traffic got exception while trying to decode RS data for VHDR [STREAM ID %u]", m_rxVoiceCallData->streamId);
            }
        }
        break;
        // VOICE1/10 create a start voice frame
        case P25_DFSI_LDU1_VOICE1:
        {
            // Decode
            MotStartVoiceFrame svf = MotStartVoiceFrame(dfsiData);
            // Copy
            ::memcpy(m_rxVoiceCallData->netLDU1 + 10U, svf.fullRateVoice->imbeData, svf.fullRateVoice->IMBE_BUF_LEN);
            // Increment our voice frame counter
            m_rxVoiceCallData->n++;
        }
        break;
        case P25_DFSI_LDU2_VOICE10:
        {
            // Decode
            MotStartVoiceFrame svf = MotStartVoiceFrame(dfsiData);
            // Copy
            ::memcpy(m_rxVoiceCallData->netLDU2 + 10U, svf.fullRateVoice->imbeData, svf.fullRateVoice->IMBE_BUF_LEN);
            // Increment our voice frame counter
            m_rxVoiceCallData->n++;
        }
        break;
        // The remaining LDUs all create full rate voice frames so we do that here
        default:
        {
            // Decode
            MotFullRateVoice voice = MotFullRateVoice(dfsiData);
            // Copy based on frame type
            switch (frameType) {
                // VOICE2
                case P25_DFSI_LDU1_VOICE2:
                {
                    ::memcpy(m_rxVoiceCallData->netLDU1 + 26U, voice.imbeData, voice.IMBE_BUF_LEN);
                }
                break;
                // VOICE3
                case P25_DFSI_LDU1_VOICE3:
                {
                    ::memcpy(m_rxVoiceCallData->netLDU1 + 55U, voice.imbeData, voice.IMBE_BUF_LEN);
                    if (voice.additionalData != nullptr) {
                        m_rxVoiceCallData->lco = voice.additionalData[0U];
                        m_rxVoiceCallData->mfId = voice.additionalData[1U];
                        m_rxVoiceCallData->serviceOptions = voice.additionalData[2U];
                    } else {
                        LogWarning(LOG_SERIAL, "V24 VC3 traffic missing metadata [STREAM ID %u]", m_rxVoiceCallData->streamId);
                    }
                }
                break;
                // VOICE4
                case P25_DFSI_LDU1_VOICE4:
                {
                    ::memcpy(m_rxVoiceCallData->netLDU1 + 80U, voice.imbeData, voice.IMBE_BUF_LEN);
                    if (voice.additionalData != nullptr) {
                        m_rxVoiceCallData->dstId = __GET_UINT16(voice.additionalData, 0U);
                    } else {
                        LogWarning(LOG_SERIAL, "V24 VC4 traffic missing metadata [STREAM ID %u]", m_rxVoiceCallData->streamId);
                    }
                }
                break;
                // VOICE5
                case P25_DFSI_LDU1_VOICE5:
                {
                    ::memcpy(m_rxVoiceCallData->netLDU1 + 105U, voice.imbeData, voice.IMBE_BUF_LEN);
                    if (voice.additionalData != nullptr) {
                        m_rxVoiceCallData->srcId = __GET_UINT16(voice.additionalData, 0U);
                    } else {
                        LogWarning(LOG_SERIAL, "V24 VC5 traffic missing metadata [STREAM ID %u]", m_rxVoiceCallData->streamId);
                    }
                }
                break;
                // VOICE6
                case P25_DFSI_LDU1_VOICE6:
                {
                    ::memcpy(m_rxVoiceCallData->netLDU1 + 130U, voice.imbeData, voice.IMBE_BUF_LEN);
                }
                break;
                // VOICE7
                case P25_DFSI_LDU1_VOICE7:
                {
                    ::memcpy(m_rxVoiceCallData->netLDU1 + 155U, voice.imbeData, voice.IMBE_BUF_LEN);
                }
                break;
                // VOICE8
                case P25_DFSI_LDU1_VOICE8:
                {
                    ::memcpy(m_rxVoiceCallData->netLDU1 + 180U, voice.imbeData, voice.IMBE_BUF_LEN);
                }
                break;
                // VOICE9
                case P25_DFSI_LDU1_VOICE9:
                {
                    ::memcpy(m_rxVoiceCallData->netLDU1 + 204U, voice.imbeData, voice.IMBE_BUF_LEN);
                    if (voice.additionalData != nullptr) {
                        m_rxVoiceCallData->lsd1 = voice.additionalData[0U];
                        m_rxVoiceCallData->lsd2 = voice.additionalData[1U];
                    } else {
                        LogWarning(LOG_SERIAL, "V24 VC9 traffic missing metadata [STREAM ID %u]", m_rxVoiceCallData->streamId);
                    }
                }
                break;
                // VOICE11
                case P25_DFSI_LDU2_VOICE11:
                {
                    ::memcpy(m_rxVoiceCallData->netLDU2 + 26U, voice.imbeData, voice.IMBE_BUF_LEN);
                }
                break;
                // VOICE12
                case P25_DFSI_LDU2_VOICE12:
                {
                    ::memcpy(m_rxVoiceCallData->netLDU2 + 55U, voice.imbeData, voice.IMBE_BUF_LEN);
                    if (voice.additionalData != nullptr) {
                        ::memcpy(m_rxVoiceCallData->mi, voice.additionalData, 3U);
                    } else {
                        LogWarning(LOG_SERIAL, "V24 VC12 traffic missing metadata [STREAM ID %u]", m_rxVoiceCallData->streamId);
                    }
                }
                break;
                // VOICE13
                case P25_DFSI_LDU2_VOICE13:
                {
                    ::memcpy(m_rxVoiceCallData->netLDU2 + 80U, voice.imbeData, voice.IMBE_BUF_LEN);
                    if (voice.additionalData != nullptr) {
                        ::memcpy(m_rxVoiceCallData->mi + 3U, voice.additionalData, 3U);
                    } else {
                        LogWarning(LOG_SERIAL, "V24 VC13 traffic missing metadata [STREAM ID %u]", m_rxVoiceCallData->streamId);
                    }
                }
                break;
                // VOICE14
                case P25_DFSI_LDU2_VOICE14:
                {
                    ::memcpy(m_rxVoiceCallData->netLDU2 + 105U, voice.imbeData, voice.IMBE_BUF_LEN);
                    if (voice.additionalData != nullptr) {
                        ::memcpy(m_rxVoiceCallData->mi + 6U, voice.additionalData, 3U);
                    } else {
                        LogWarning(LOG_SERIAL, "V24 VC14 traffic missing metadata [STREAM ID %u]", m_rxVoiceCallData->streamId);
                    }
                }
                break;
                // VOICE15
                case P25_DFSI_LDU2_VOICE15:
                {
                    ::memcpy(m_rxVoiceCallData->netLDU2 + 130U, voice.imbeData, voice.IMBE_BUF_LEN);
                    if (voice.additionalData != nullptr) {
                        m_rxVoiceCallData->algoId = voice.additionalData[0U];
                        m_rxVoiceCallData->kId = __GET_UINT16B(voice.additionalData, 1U);
                    } else {
                        LogWarning(LOG_SERIAL, "V24 VC15 traffic missing metadata [STREAM ID %u]", m_rxVoiceCallData->streamId);
                    }
                }
                break;
                // VOICE16
                case P25_DFSI_LDU2_VOICE16:
                {
                    ::memcpy(m_rxVoiceCallData->netLDU2 + 155U, voice.imbeData, voice.IMBE_BUF_LEN);
                }
                break;
                // VOICE17
                case P25_DFSI_LDU2_VOICE17:
                {
                    ::memcpy(m_rxVoiceCallData->netLDU2 + 180U, voice.imbeData, voice.IMBE_BUF_LEN);
                }
                break;
                // VOICE18
                case P25_DFSI_LDU2_VOICE18:
                {
                    ::memcpy(m_rxVoiceCallData->netLDU2 + 204U, voice.imbeData, voice.IMBE_BUF_LEN);
                    if (voice.additionalData != nullptr) {
                        m_rxVoiceCallData->lsd1 = voice.additionalData[0U];
                        m_rxVoiceCallData->lsd2 = voice.additionalData[1U];
                    } else {
                        LogWarning(LOG_SERIAL, "V24 VC18 traffic missing metadata [STREAM ID %u]", m_rxVoiceCallData->streamId);
                    }
                }
                break;
            }

            // Increment our voice frame counter
            m_rxVoiceCallData->n++;
        }
        break;
    }

    // Get LC & LSD data if we're ready for either LDU1 or LDU2 (don't do this every frame to be more efficient)

    if (m_rxVoiceCallData->n == 9U || m_rxVoiceCallData->n == 18U) {
        // Create LC
        m_rxVoiceControl->setSrcId(m_rxVoiceCallData->srcId);
        m_rxVoiceControl->setDstId(m_rxVoiceCallData->dstId);
        // Get service options
        bool emergency = ((m_rxVoiceCallData->serviceOptions & 0xFFU) & 0x80U) == 0x80U;    // Emergency Flag
        bool encryption = ((m_rxVoiceCallData->serviceOptions & 0xFFU) & 0x40U) == 0x40U;   // Encryption Flag
        uint8_t priority = ((m_rxVoiceCallData->serviceOptions & 0xFFU) & 0x07U);           // Priority
        m_rxVoiceControl->setEmergency(emergency);
        m_rxVoiceControl->setEncrypted(encryption);
        m_rxVoiceControl->setPriority(priority);
        // Get more data
        m_rxVoiceControl->setMI(m_rxVoiceCallData->mi);
        m_rxVoiceControl->setAlgId(m_rxVoiceCallData->algoId);
        m_rxVoiceControl->setKId(m_rxVoiceCallData->kId);
        // Get LSD
        m_rxVoiceLsd->setLSD1(m_rxVoiceCallData->lsd1);
        m_rxVoiceLsd->setLSD2(m_rxVoiceCallData->lsd2);
    }

    // Send LDU1 if ready
    if (m_rxVoiceCallData->n == 9U) {
        // Send (TODO: dynamically set HDU_VALID or DATA_VALID depending on start of call or not)
        bool ret = m_network->writeP25LDU1(*m_rxVoiceControl, *m_rxVoiceLsd, m_rxVoiceCallData->netLDU1, P25_FT_HDU_VALID);
        // Print
        LogInfoEx(LOG_NET, P25_LDU1_STR " audio, srcId = %u, dstId = %u", m_rxVoiceCallData->srcId, m_rxVoiceCallData->dstId);
        // Optional Debug
        if (ret) {
            if (m_debug)
                LogDebug(LOG_SERIAL, "V24 LDU1 [STREAM ID %u, SRC %u, DST %u]", m_rxVoiceCallData->streamId, m_rxVoiceCallData->srcId, m_rxVoiceCallData->dstId);
            /*if (m_trace)
                Utils::dump(1U, "LDU1 to net", m_rxVoiceCallData->netLDU1, 9U * 25U);*/
        }
        else {
            LogError(LOG_SERIAL, "V24 LDU1 failed to write to network");
        }
    }
    
    // Send LDU2 if ready
    if (m_rxVoiceCallData->n == 18U) {
        // Send
        bool ret = m_network->writeP25LDU2(*m_rxVoiceControl, *m_rxVoiceLsd, m_rxVoiceCallData->netLDU2);
        // Print
        LogInfoEx(LOG_SERIAL, P25_LDU2_STR " audio, algo = $%02X, kid = $%04X", m_rxVoiceCallData->algoId, m_rxVoiceCallData->kId);
        // Optional Debug
        if (ret) {
            if (m_debug)
                LogDebug(LOG_SERIAL, "V24 LDU2 [STREAM ID %u, SRC %u, DST %u]", m_rxVoiceCallData->streamId, m_rxVoiceCallData->srcId, m_rxVoiceCallData->dstId);
            /*if (m_trace)
                Utils::dump(1U, "LDU2 to net", m_rxVoiceCallData->netLDU2, 9U * 25U);*/
        }
        else {
            LogError(LOG_SERIAL, "V24 LDU2 failed to write to network");
        }
        // Reset counter since we've sent both frames
        m_rxVoiceCallData->n = 0;
    }
}

/// <summary>Read a data message from the serial port</summary>
/// This is borrowed from the Modem::getResponse() function
/// <returns>Response type</returns>
RESP_TYPE_DVM SerialService::readSerial()
{
    // Flag for a 16-bit (i.e. 2-byte) length
    m_msgDoubleLength = false;

    // If we're waiting for a message start byte, read a single byte
    if (m_msgState == RESP_START) {
        // Try and read a byte from the serial port
        int ret = m_port->read(m_msgBuffer + 0U, 1U);

        // Catch read error
        if (ret < 0) {
            LogError(LOG_SERIAL, "Error reading from serial port, ret = %d", ret);
            m_msgState = RESP_START;
            return RTM_ERROR;
        }

        // Handle no data
        if (ret == 0) {
            return RTM_TIMEOUT;
        }

        // Handle anything other than a start flag while in RESP_START mode
        if (m_msgBuffer[0U] != DVM_SHORT_FRAME_START &&
            m_msgBuffer[0U] != DVM_LONG_FRAME_START) {
            ::memset(m_msgBuffer, 0x00U, BUFFER_LENGTH);
            return RTM_ERROR;
        }

        // Detect short vs long frame
        if (m_msgBuffer[0U] == DVM_LONG_FRAME_START) {
            m_msgDoubleLength = true;
        }

        // Advance state machine
        m_msgState = RESP_LENGTH1;
    }

    // Check length byte (1/2)
    if (m_msgState == RESP_LENGTH1) {
        // Read length
        int ret = m_port->read(m_msgBuffer + 1U, 1U);
        
        // Catch read error
        if (ret < 0) {
            LogError(LOG_SERIAL, "Error reading from serial port, ret = %d", ret);
            m_msgState = RESP_START;
            return RTM_ERROR;
        }

        // Handle no data
        if (ret == 0) {
            return RTM_TIMEOUT;
        }

        // Handle invalid length
        if (m_msgBuffer[1U] >= 250U && !m_msgDoubleLength) {
            LogError(LOG_SERIAL, "Invalid length received from the modem, len = %u", m_msgBuffer[1U]);
            return RTM_ERROR;
        }

        // Handle double-byte length
        if (m_msgDoubleLength) {
            m_msgState = RESP_LENGTH2;
            m_msgLength = ( (m_msgBuffer[1U] & 0xFFU) << 8 );
        } else {
            // Advnace to type byte if single-byte length
            m_msgState = RESP_TYPE;
            m_msgLength = m_msgBuffer[1U];
        }

        // Set proper data offset
        m_msgOffset = 2U;
    }

    // Check length byte (2/2)
    if (m_msgState == RESP_LENGTH2) {
        // Read
        int ret = m_port->read(m_msgBuffer + 2U, 1U);

        // Catch read error
        if (ret < 0) {
            LogError(LOG_SERIAL, "Error reading from serial port, ret = %d", ret);
            m_msgState = RESP_START;
            return RTM_ERROR;
        }

        // Handle no data
        if (ret == 0) {
            return RTM_TIMEOUT;
        }

        // Calculate total length
        m_msgLength = (m_msgLength + (m_msgBuffer[2U] & 0xFFU) );
        
        // Advance state machine
        m_msgState = RESP_TYPE;

        // Set flag
        m_msgDoubleLength = true;
        
        // Set proper data offset
        m_msgOffset = 3U;
    }

    if (m_msgState == RESP_TYPE) {
        // Read type
        int ret = m_port->read(m_msgBuffer + m_msgOffset, 1U);
        
        // Catch read error
        if (ret < 0) {
            LogError(LOG_SERIAL, "Error reading from serial port, ret = %d", ret);
            m_msgState = RESP_START;
            return RTM_ERROR;
        }

        // Handle no data
        if (ret == 0) {
            return RTM_TIMEOUT;
        }

        // Get command type
        m_msgType = (DVM_COMMANDS)m_msgBuffer[m_msgOffset];

        // Move on
        m_msgState = RESP_DATA;
        m_msgOffset++;
    }

    // Get the data
    if (m_msgState == RESP_DATA) {
        if (m_trace) {
            LogDebug(LOG_SERIAL, "readSerial(), RESP_DATA, len = %u, offset = %u, type = %02X", m_msgLength, m_msgOffset, m_msgType);
        }

        // Get the data based on the earlier length
        while (m_msgOffset < m_msgLength) {
            int ret = m_port->read(m_msgBuffer + m_msgOffset, m_msgLength - m_msgOffset);

            if (ret < 0) {
                LogError(LOG_SERIAL, "Error reading from serial port, ret = %d", ret);
                m_msgState = RESP_START;
                return RTM_ERROR;
            }

            if (ret == 0)
                return RTM_TIMEOUT;

            if (ret > 0)
                m_msgOffset += ret;
        }

        if (m_debug && m_trace)
            Utils::dump(1U, "Serial RX Data", m_msgBuffer, m_msgLength);
    }

    m_msgState = RESP_START;
    m_msgOffset = 0U;

    return RTM_OK;
}

/// <summary>Called from clock thread, checks for an available P25 frame to write and sends it based on jitter timing requirements</summary>
/// Very similar to the readP25Frame function below
///
/// Note: the length encoded at the start does not include the length, tag, or timestamp bytes
int SerialService::writeSerial()
{
    /**
     *  Serial TX ringbuffer format:
     * 
     *  | 0x01 | 0x02 | 0x03 | 0x04 | 0x05 | 0x06 | 0x07 | 0x08 | 0x09 | 0x0A | 0x0B | 0x0C | ... |
     *  |   Length    | Tag  |               int64_t timestamp in ms                 |   data     |
    */

    // Check empty
    if (m_txP25Queue.isEmpty())
        return 0U;

    // Get length
    uint8_t length[2U];
    ::memset(length, 0x00U, 2U);
    m_txP25Queue.peek(length, 2U);

    // Convert length byets to int
    uint16_t len = 0U;
    len = (length[0U] << 8) + length[1U];

    // This ensures we never get in a situation where we have length & type bytes stuck in the queue by themselves
    if (m_txP25Queue.dataSize() == 2U && len > m_txP25Queue.dataSize()) {
        m_txP25Queue.get(length, 2U); // ensure we pop bytes off
        return 0U;
    }

    // Get current timestamp
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // Peek the timestamp to see if we should wait
    if (m_txP25Queue.dataSize() >= 11U) {
        // Peek everything up to the timestamp
        uint8_t lengthTagTs[11U];
        ::memset(lengthTagTs, 0x00U, 11U);
        m_txP25Queue.peek(lengthTagTs, 11U);
        // Get the timestamp
        int64_t ts;
        assert(sizeof ts == 8);
        ::memcpy(&ts, lengthTagTs + 3U, 8U);
        // If it's not time to send, return
        if (ts > now) {
            return 0U;
        }
    }

    // Check if we have enough data to get everything - len + 2U (length bytes) + 1U (tag) + 8U (timestamp)
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

        // We already checked the timestamp above, so we just get the data and write it
        return m_port->write(buffer, len);
    }

    return 0U;
}

/// <summary>
/// Gets a frame of P25 data from the RX queue 
/// <summary>
/// <param name="*data">The data buffer to populate</param>
/// <returns>The size of the P25 data retreived, including the leading data tag
uint32_t SerialService::readP25Frame(uint8_t* data)
{

    /**
     *  Serial RX ringbuffer format:
     * 
     *  | 0x01 | 0x02 | 0x03 | 0x04 | ... |
     *  |   Length    | Tag  |   data     |
    */

    // sanity check
    assert(data != nullptr);

    // Check empty
    if (m_rxP25Queue.isEmpty())
        return 0U;

    // Get length bytes
    uint8_t length[2U];
    ::memset(length, 0x00U, 2U);
    m_rxP25Queue.peek(length, 2U);

    // Convert length bytes to a length int
    uint16_t len = 0U;
    len = (length[0] << 8) + length[1U];

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

/// <summary>
/// Break apart a P25 LDU and add to the TX queue, timed appropriately
/// </summary>
/// <param name="duid">DUID flag for the LDU</param>
/// <param name="lc">Link Control data</param>
/// <param name="ldu">LDU data</param>
///
/// This is very similar to the C# Mot_DFSISendFrame functions, we don't implement the TIA DFSI sendframe in serial
/// because the only devices we connect to over serial V24 are Moto
void SerialService::writeP25Frame(uint8_t duid, dfsi::LC& lc, uint8_t* ldu)
{
    // Sanity check
    assert(ldu != nullptr);

    // Get now
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // Break out the control components
    lc::LC control = lc::LC(*lc.control());
    data::LowSpeedData lsd = data::LowSpeedData(*lc.lsd());

    // Get the service options
    uint8_t serviceOptions = 
        (control.getEmergency() ? 0x80U : 0x00U) +
        (control.getEncrypted() ? 0x40U : 0x00U) +
        (control.getPriority() & 0x07U);

    // Get the MI
    uint8_t mi[P25_MI_LENGTH_BYTES];
    control.getMI(mi);

    // Calculate reed-solomon encoding depending on DUID type
    uint8_t rs[P25_LDU_LC_FEC_LENGTH_BYTES];
    ::memset(rs, 0x00U, P25_LDU_LC_FEC_LENGTH_BYTES);
    switch(duid) {
        case P25_DUID_LDU1:
        {
            rs[0U] = control.getLCO();                                  // LCO
            rs[1U] = control.getMFId();                                 // MFId
            rs[2U] = serviceOptions;                                    // Service Options
            uint32_t dstId = control.getDstId();
            rs[3U] = (dstId >> 16) & 0xFFU;                             // Target Address
            rs[4U] = (dstId >> 8) & 0xFFU;
            rs[5U] = (dstId >> 0) & 0xFFU;
            uint32_t srcId = control.getSrcId();
            rs[6U] = (srcId >> 16) & 0xFFU;                             // Source Address
            rs[7U] = (srcId >> 8) & 0xFFU;
            rs[8U] = (srcId >> 0) & 0xFFU;

            // encode RS (24,12,13) FEC
            m_rs.encode241213(rs);
        }
        break;
        case P25_DUID_LDU2:
        {
            for (uint32_t i = 0; i < P25_MI_LENGTH_BYTES; i++)
                rs[i] = mi[i];                                          // Message Indicator

            rs[9U] = control.getAlgId();                                // Algorithm ID
            rs[10U] = (control.getKId() >> 8) & 0xFFU;                  // Key ID
            rs[11U] = (control.getKId() >> 0) & 0xFFU;                  // ...

            // encode RS (24,16,9) FEC
            m_rs.encode24169(rs);
        }
        break;
    }

    // Placeholder for last call timestamp retrieved below (not yet used)
    //int64_t lastHeard = 0U;

    // Placeholder for sequence number retrieved below 
    uint32_t sequence = 0U;

    // Get the last heard value (not used currently, TODO: use this for covnentional TGID timeout purposes)
    /*{
        auto entry = m_lastHeard.find(control.getDstId());
        if (entry != m_lastHeard.end()) {
            lastHeard = m_lastHeard[control.getDstId()];
        }
    }*/

    // Get the last sequence number
    {
        auto entry = m_sequences.find(control.getDstId());
        if (entry != m_sequences.end()) {
            sequence = m_sequences[control.getDstId()];
        }
    }

    // Check if we need to start a new data stream
    if (duid == P25_DUID_LDU1 && ((sequence == 0U) || (sequence == RTP_END_OF_CALL_SEQ))) {
        // Start the new stream
        startOfStream(lc);
        // Update our call entries
        m_lastHeard[control.getDstId()] = now;
        m_sequences[control.getDstId()] = ++sequence;

        // Log
        LogInfoEx(LOG_SERIAL, "CALL START: %svoice call from %u to TG %u", (control.getAlgId() != P25_ALGO_UNENCRYPT) ? "encrypted " : "", control.getSrcId(), control.getDstId());

        ActivityLog("network %svoice transmission call from %u to TG %u", (control.getAlgId() != P25_ALGO_UNENCRYPT) ? "encrypted " : "", control.getSrcId(), control.getDstId());
    } else {
        // If this TGID isn't either lookup, consider it a late entry and start a new stream
        if ( (m_sequences.find(control.getDstId()) == m_sequences.end()) || (m_lastHeard.find(control.getDstId()) == m_lastHeard.end()) ) {
            // Start the stream, same as above
            startOfStream(lc);
            m_lastHeard[control.getDstId()] = now;
            m_sequences[control.getDstId()] = ++sequence;

            LogInfoEx(LOG_SERIAL, "LATE CALL START: %svoice call from %u to TG %u", (control.getAlgId() != P25_ALGO_UNENCRYPT) ? "encrypted " : "", control.getSrcId(), control.getDstId());
            ActivityLog("network %svoice transmission late entry from %u to TG %u", (control.getAlgId() != P25_ALGO_UNENCRYPT) ? "encrypted " : "", control.getSrcId(), control.getDstId());
        }
    }

    // Check if we need to end the call
    if ( (duid == P25_DUID_TDU) || (duid == P25_DUID_TDULC) ) {
        // Stop
        endOfStream();
        // Log
        LogInfoEx(LOG_SERIAL, "CALL END: %svoice call from %u to TG %u", (control.getAlgId() != P25_ALGO_UNENCRYPT) ? "encrypted " : "", control.getSrcId(), control.getDstId());
        // Clear our counters
        m_sequences[control.getDstId()] = RTP_END_OF_CALL_SEQ;
    }

    // Break out the 9 individual P25 packets
    for (int n = 0; n < 9; n++) {
        
        // We use this buffer and fullrate voice object to slim down the code to a common sendFrame routine at the bottom
        uint8_t* buffer = nullptr;
        uint16_t bufferSize = 0;
        MotFullRateVoice voice = MotFullRateVoice();

        switch (n) {
            case 0: // VOICE1/10
            {
                // Set frametype
                voice.frameType = (duid == P25_DUID_LDU1) ? P25_DFSI_LDU1_VOICE1 : P25_DFSI_LDU2_VOICE10;
                // Create the new frame objects
                MotStartVoiceFrame svf = MotStartVoiceFrame();
                svf.startOfStream = new MotStartOfStream();
                svf.fullRateVoice = new MotFullRateVoice();
                // Set values appropriately
                svf.startOfStream->startStop = StartStopFlag::START;
                svf.startOfStream->rt = m_rtrt ? RTFlag::ENABLED : RTFlag::DISABLED;
                // Set frame type
                svf.fullRateVoice->frameType = voice.frameType;
                // Set source flag & ICW flag
                svf.fullRateVoice->source = m_diu ? SOURCE_DIU : SOURCE_QUANTAR;
                svf.icw = m_diu ? ICW_DIU : ICW_QUANTAR;
                // Copy data
                ::memcpy(svf.fullRateVoice->imbeData, ldu + 10U, svf.fullRateVoice->IMBE_BUF_LEN);
                // Encode
                buffer = new uint8_t[svf.LENGTH];
                ::memset(buffer, 0x00U, svf.LENGTH);
                svf.encode(buffer);
                bufferSize = svf.LENGTH;
            }
            break;
            case 1: // VOICE2/11
            {
                voice.frameType = (duid == P25_DUID_LDU1) ? P25_DFSI_LDU1_VOICE2 : P25_DFSI_LDU2_VOICE11;
                // Set source flag
                voice.source = m_diu ? SOURCE_DIU : SOURCE_QUANTAR;
                ::memcpy(voice.imbeData, ldu + 26U, voice.IMBE_BUF_LEN);
            }
            break;
            case 2: // VOICE3/12
            {
                voice.frameType = (duid == P25_DUID_LDU1) ? P25_DFSI_LDU1_VOICE3 : P25_DFSI_LDU2_VOICE12;
                ::memcpy(voice.imbeData, ldu + 55U, voice.IMBE_BUF_LEN);
                // Create the additional data array
                voice.additionalData = new uint8_t[voice.ADDITIONAL_LENGTH];
                ::memset(voice.additionalData, 0x00U, voice.ADDITIONAL_LENGTH);
                // Copy additional data
                if (voice.frameType == P25_DUID_LDU1) {
                    voice.additionalData[0U] = control.getLCO();
                    voice.additionalData[1U] = control.getMFId();
                    voice.additionalData[2U] = serviceOptions;
                } else {
                    voice.additionalData[0U] = mi[0U];
                    voice.additionalData[1U] = mi[1U];
                    voice.additionalData[2U] = mi[2U];
                }
            }
            break;
            case 3: // VOICE4/13
            {
                voice.frameType = (duid == P25_DUID_LDU1) ? P25_DFSI_LDU1_VOICE4 : P25_DFSI_LDU2_VOICE13;
                ::memcpy(voice.imbeData, ldu + 80U, voice.IMBE_BUF_LEN);
                // Create the additional data array
                voice.additionalData = new uint8_t[voice.ADDITIONAL_LENGTH];
                ::memset(voice.additionalData, 0x00U, voice.ADDITIONAL_LENGTH);
                // We set the additional data based on LDU1/2
                switch (duid) {
                    case P25_DUID_LDU1:
                    {
                        // Destination address (3 bytes)
                        __SET_UINT16(control.getDstId(), voice.additionalData, 0U);
                    }
                    break;
                    case P25_DUID_LDU2:
                    {
                        // Message Indicator
                        voice.additionalData[0U] = mi[3U];
                        voice.additionalData[1U] = mi[4U];
                        voice.additionalData[2U] = mi[5U];
                    }
                    break;
                }
            }
            break;
            case 4: // VOICE5/14
            {
                voice.frameType = (duid == P25_DUID_LDU1) ? P25_DFSI_LDU1_VOICE5 : P25_DFSI_LDU2_VOICE14;
                ::memcpy(voice.imbeData, ldu + 105U, voice.IMBE_BUF_LEN);
                // Create the additional data array
                voice.additionalData = new uint8_t[voice.ADDITIONAL_LENGTH];
                ::memset(voice.additionalData, 0x00U, voice.ADDITIONAL_LENGTH);
                // Same as case 3 above
                switch (duid) {
                    case P25_DUID_LDU1:
                    {
                        // Source address (3 bytes)
                        __SET_UINT16(control.getSrcId(), voice.additionalData, 0U);
                    }
                    break;
                    case P25_DUID_LDU2:
                    {
                        // Message Indicator
                        voice.additionalData[0U] = mi[6U];
                        voice.additionalData[1U] = mi[7U];
                        voice.additionalData[2U] = mi[8U];
                    }
                    break;
                }
            }
            break;
            case 5: // VOICE6/15
            {
                voice.frameType = (duid == P25_DUID_LDU1) ? P25_DFSI_LDU1_VOICE6 : P25_DFSI_LDU2_VOICE15;
                ::memcpy(voice.imbeData, ldu + 130U, voice.IMBE_BUF_LEN);
                // Create the additional data array
                voice.additionalData = new uint8_t[voice.ADDITIONAL_LENGTH];
                ::memset(voice.additionalData, 0x00U, voice.ADDITIONAL_LENGTH);
                // Another switch on LDU1/2
                switch (duid) {
                    case P25_DUID_LDU1:
                    {
                        // RS encoding
                        voice.additionalData[0U] = rs[9U];
                        voice.additionalData[1U] = rs[10U];
                        voice.additionalData[2U] = rs[11U];
                    }
                    break;
                    case P25_DUID_LDU2:
                    {
                        voice.additionalData[0U] = control.getAlgId();              // Algo ID
                        __SET_UINT16B(control.getKId(), voice.additionalData, 1U);  // Key ID
                    }
                    break;
                }
            }
            break;
            case 6: // VOICE7/16
            {
                voice.frameType = (duid == P25_DUID_LDU1) ? P25_DFSI_LDU1_VOICE7 : P25_DFSI_LDU2_VOICE16;
                ::memcpy(voice.imbeData, ldu + 155U, voice.IMBE_BUF_LEN);
                // Create the additional data array
                voice.additionalData = new uint8_t[voice.ADDITIONAL_LENGTH];
                ::memset(voice.additionalData, 0x00U, voice.ADDITIONAL_LENGTH);
                // RS data offsets are the same regardless of LDU
                voice.additionalData[0U] = rs[12U];
                voice.additionalData[1U] = rs[13U];
                voice.additionalData[2U] = rs[14U];
            }
            break;
            case 7: // VOICE8/17
            {
                voice.frameType = (duid == P25_DUID_LDU1) ? P25_DFSI_LDU1_VOICE8 : P25_DFSI_LDU2_VOICE17;
                ::memcpy(voice.imbeData, ldu + 180U, voice.IMBE_BUF_LEN);
                // Create the additional data array
                voice.additionalData = new uint8_t[voice.ADDITIONAL_LENGTH];
                ::memset(voice.additionalData, 0x00U, voice.ADDITIONAL_LENGTH);
                // RS data offsets are the same regardless of LDU
                voice.additionalData[0U] = rs[15U];
                voice.additionalData[1U] = rs[16U];
                voice.additionalData[2U] = rs[17U];
            }
            break;
            case 8: // VOICE9/18
            {
                voice.frameType = (duid == P25_DUID_LDU1) ? P25_DFSI_LDU1_VOICE9 : P25_DFSI_LDU2_VOICE18;
                ::memcpy(voice.imbeData, ldu + 204U, voice.IMBE_BUF_LEN);
                // Create the additional data array
                voice.additionalData = new uint8_t[voice.ADDITIONAL_LENGTH];
                ::memset(voice.additionalData, 0x00U, voice.ADDITIONAL_LENGTH);
                // Get low speed data bytes
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

        // Debug logging
        if (m_trace) {
            Utils::dump("Encoded V24 voice frame data", buffer, bufferSize);
        }

        // Send if we have data (which we always should)
        if (buffer != nullptr) {
            addTxToQueue(buffer, bufferSize, SERIAL_TX_TYPE::IMBE);
        }

    }
}

/// <summary>
/// Send a start of stream sequence (HDU, etc) to the connected serial V24 device
/// </summary>
/// <param name="lc">Link control data object</param>
void SerialService::startOfStream(const LC& lc)
{
    // Flag that we have a network call in progress
    m_netCallInProgress = true;

    // Create new start of stream
    MotStartOfStream start = MotStartOfStream();
    start.startStop = StartStopFlag::START;
    start.rt = m_rtrt ? RTFlag::ENABLED : RTFlag::DISABLED;

    // Create buffer for bytes and encode
    uint8_t buffer[start.LENGTH];
    ::memset(buffer, 0x00U, start.LENGTH);
    start.encode(buffer);
    
    // Optional debug & trace
    if (m_debug)
        LogDebug(LOG_SERIAL, "encoded mot p25 start frame");
    if (m_trace)
        Utils::dump(1U, "data", buffer, start.LENGTH);
        
    // Send start frame
    addTxToQueue(buffer, start.LENGTH, network::SERIAL_TX_TYPE::NONIMBE);

    // Break out the control components
    lc::LC control = lc::LC(*lc.control());
    data::LowSpeedData lsd = data::LowSpeedData(*lc.lsd());
    
    // Init message indicator & get
    uint8_t mi[P25_MI_LENGTH_BYTES];
    ::memset(mi, 0x00U, P25_MI_LENGTH_BYTES);
    control.getMI(mi);

    // Init VHDR data array
    uint8_t vhdr[P25_DFSI_VHDR_LEN];
    ::memset(vhdr, 0x00U, P25_DFSI_VHDR_LEN);

    // Copy MI to VHDR
    ::memcpy(vhdr, mi, P25_MI_LENGTH_BYTES);

    // Set values
    vhdr[9U] = control.getMFId();
    vhdr[10U] = control.getAlgId();
    __SET_UINT16B(control.getKId(), vhdr, 11U);
    __SET_UINT16B(control.getDstId(), vhdr, 13U);

    // Perform RS encoding
    m_rs.encode362017(vhdr);

    // Convert the binary bytes to hex bytes (some kind of bit packing thing I don't understand)
    uint8_t raw[P25_DFSI_VHDR_RAW_LEN];
    uint32_t offset = 0;
    for (uint8_t i = 0; i < P25_DFSI_VHDR_RAW_LEN; i++, offset += 6) {
        raw[i] = Utils::bin2Hex(vhdr, offset);
    }

    // Prepare VHDR1
    MotVoiceHeader1 vhdr1 = MotVoiceHeader1();
    vhdr1.startOfStream = new MotStartOfStream();
    vhdr1.startOfStream->startStop = StartStopFlag::START;
    vhdr1.startOfStream->rt = m_rtrt ? RTFlag::ENABLED : RTFlag::DISABLED;
    vhdr1.icw = m_diu ? ICW_DIU : ICW_QUANTAR;
    ::memcpy(vhdr1.header, raw, 8U);
    ::memcpy(vhdr1.header + 9U, raw + 8U, 8U);
    ::memcpy(vhdr1.header + 18U, raw + 16U, 2U);

    // Encode VHDR1 and send
    uint8_t buffer1[vhdr1.LENGTH];
    ::memset(buffer1, 0x00U, vhdr1.LENGTH);
    vhdr1.encode(buffer1);
    
    if (m_debug)
        LogDebug(LOG_SERIAL, "encoded mot VHDR1 p25 frame");
    if (m_trace)
        Utils::dump(1U, "data", buffer1, vhdr1.LENGTH);
        
    addTxToQueue(buffer1, vhdr1.LENGTH, SERIAL_TX_TYPE::NONIMBE);

    // Prepare VHDR2
    MotVoiceHeader2 vhdr2 = MotVoiceHeader2();
    ::memcpy(vhdr2.header, raw + 18U, 8U);
    ::memcpy(vhdr2.header + 9U, raw + 26U, 8U);
    ::memcpy(vhdr2.header + 18U, raw + 34U, 2U);

    // Encode VHDR2 and send
    uint8_t buffer2[vhdr2.LENGTH];
    ::memset(buffer2, 0x00U, vhdr2.LENGTH);
    vhdr2.encode(buffer2);
    
    if (m_debug)
        LogDebug(LOG_SERIAL, "encoded mot VHDR2 p25 frame");
    if (m_trace)
        Utils::dump(1U, "data", buffer2, vhdr2.LENGTH);
    
    addTxToQueue(buffer2, vhdr2.LENGTH, SERIAL_TX_TYPE::NONIMBE);
}

/// <summary>
/// Send an end of stream sequence (TDU, etc) to the connected serial V24 device
/// </summary>
/// <param name="lc">Link control data object</param>
void SerialService::endOfStream()
{
    // Create the new end of stream (which looks like a start of stream with the stop flag)
    MotStartOfStream end = MotStartOfStream();
    end.startStop = StartStopFlag::STOP;

    // Create buffer and encode
    uint8_t buffer[end.LENGTH];
    ::memset(buffer, 0x00U, end.LENGTH);
    end.encode(buffer);

    if (m_trace) {
        LogDebug(LOG_SERIAL, "encoded mot p25 end frame");
        Utils::dump(1U, "data", buffer, end.LENGTH);
    }

    // Send start frame
    addTxToQueue(buffer, end.LENGTH, SERIAL_TX_TYPE::NONIMBE);

    // Our net call is done
    m_netCallInProgress = false;
}

/// <summary>
/// Helper to add a V24 dataframe to the P25 TX queue with the proper timestamp and formatting
/// </summary>
/// <param name="data">Data array to send</param>
/// <param name="len">Length of data in array</param>
/// <param name="msgType">Type of message to send (used for proper jitter clocking)</param>
void SerialService::addTxToQueue(uint8_t* data, uint16_t len, SERIAL_TX_TYPE msgType)
{
    // If the port isn't connected, just return
    if (m_port == nullptr) { return; }

    // Get current time in ms
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // Timestamp for this message (in ms)
    uint64_t msgTime = 0U;

    // If this is our first message, timestamp is just now + the jitter buffer offset in ms
    if (m_lastP25Tx == 0U) {  
        msgTime = now + m_jitter;
    } 
    // If we had a message before this, calculate the new timestamp dynamically
    else {
        // If the last message occurred longer than our jitter buffer delay, we restart the sequence and calculate the same as above
        if ((int64_t)(now - m_lastP25Tx) > m_jitter) {
            msgTime = now + m_jitter;
        } 
        // Otherwise, we time out messages as required by the message type
        else {
            if (msgType == IMBE) {
                // IMBEs must go out at 20ms intervals
                msgTime = m_lastP25Tx + 20;
            } else {
                // Otherwise we don't care, we use 5ms since that's the theoretical minimum time a 9600 baud message can take
                msgTime = m_lastP25Tx + 5;
            }
        }
    }

    // Increment the length by 4 for the header bytes
    len += 4U;

    // Convert 16-bit length to 2 bytes
    uint8_t length[2U];
    if (len > 255U)
        length[0U] = (len >> 8U) & 0xFFU;
    else
        length[0U] = 0x00U;
    length[1U] = len & 0xFFU;
                
    m_txP25Queue.addData(length, 2U);

    // Add the data tag
    uint8_t tag = TAG_DATA;
    m_txP25Queue.addData(&tag, 1U);

    // Convert 64-bit timestamp to 8 bytes and add
    uint8_t tsBytes[8U];
    assert(sizeof msgTime == 8U);
    ::memcpy(tsBytes, &msgTime, 8U);
    m_txP25Queue.addData(tsBytes, 8U);

    // Add the DVM start byte, length byte, CMD byte, and padding 0
    uint8_t header[4U];
    header[0U] = DVM_SHORT_FRAME_START;
    header[1U] = len & 0xFFU;
    header[2U] = CMD_P25_DATA;
    header[3U] = 0x00U;
    m_txP25Queue.addData(header, 4U);

    // Add the data
    m_txP25Queue.addData(data, len - 4U);

    //Utils::dump(1U, "SERIAL TX DATA", data, len - 4U);

    // Update the last message time
    m_lastP25Tx = msgTime;
}

/// <summary>
/// Helper to insert IMBE silence frames for missing audio.
/// </summary>
/// <param name="data"></param>
/// <param name="lost"></param>
void SerialService::insertMissingAudio(uint8_t *data, uint32_t& lost)
{
    if (data[10U] == 0x00U) {
        ::memcpy(data + 10U, m_lastIMBE, 11U);
        lost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 10U, 11U);
    }

    if (data[26U] == 0x00U) {
        ::memcpy(data + 26U, m_lastIMBE, 11U);
        lost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 26U, 11U);
    }

    if (data[55U] == 0x00U) {
        ::memcpy(data + 55U, m_lastIMBE, 11U);
        lost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 55U, 11U);
    }

    if (data[80U] == 0x00U) {
        ::memcpy(data + 80U, m_lastIMBE, 11U);
        lost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 80U, 11U);
    }

    if (data[105U] == 0x00U) {
        ::memcpy(data + 105U, m_lastIMBE, 11U);
        lost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 105U, 11U);
    }

    if (data[130U] == 0x00U) {
        ::memcpy(data + 130U, m_lastIMBE, 11U);
        lost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 130U, 11U);
    }

    if (data[155U] == 0x00U) {
        ::memcpy(data + 155U, m_lastIMBE, 11U);
        lost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 155U, 11U);
    }

    if (data[180U] == 0x00U) {
        ::memcpy(data + 180U, m_lastIMBE, 11U);
        lost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 180U, 11U);
    }

    if (data[204U] == 0x00U) {
        ::memcpy(data + 204U, m_lastIMBE, 11U);
        lost++;
    }
    else {
        ::memcpy(m_lastIMBE, data + 204U, 11U);
    }
}

void SerialService::printDebug(const uint8_t* buffer, uint16_t len)
{
    if (m_msgDoubleLength && buffer[3U] == CMD_DEBUG_DUMP) {
        uint8_t data[512U];
        ::memset(data, 0x00U, 512U);
        ::memcpy(data, buffer, len);

        Utils::dump(1U, "V24 Debug Dump", data, len);
        return;
    }
    else {
        if (m_msgDoubleLength) {
            LogError(LOG_SERIAL, "Invalid debug data received from the V24 board, len = %u", len);
            return;
        }
    }

    // Handle the individual debug types
    if (buffer[2U] == CMD_DEBUG1) {
        LogDebug(LOG_SERIAL, "V24 USB: %.*s", len - 3U, buffer + 3U);
    }
    else if (buffer[2U] == CMD_DEBUG2) {
        short val1 = (buffer[len - 2U] << 8) | buffer[len - 1U];
        LogDebug(LOG_SERIAL, "V24 USB: %.*s %X", len - 5U, buffer + 3U, val1);
    }
    else if (buffer[2U] == CMD_DEBUG3) {
        short val1 = (buffer[len - 4U] << 8) | buffer[len - 3U];
        short val2 = (buffer[len - 2U] << 8) | buffer[len - 1U];
        LogDebug(LOG_SERIAL, "V24 USB: %.*s %X %X", len - 7U, buffer + 3U, val1, val2);
    }
    else if (buffer[2U] == CMD_DEBUG4) {
        short val1 = (buffer[len - 6U] << 8) | buffer[len - 5U];
        short val2 = (buffer[len - 4U] << 8) | buffer[len - 3U];
        short val3 = (buffer[len - 2U] << 8) | buffer[len - 1U];
        LogDebug(LOG_SERIAL, "V24 USB: %.*s %X %X %X", len - 9U, buffer + 3U, val1, val2, val3);
    }
    else if (buffer[2U] == CMD_DEBUG5) {
        short val1 = (buffer[len - 8U] << 8) | buffer[len - 7U];
        short val2 = (buffer[len - 6U] << 8) | buffer[len - 5U];
        short val3 = (buffer[len - 4U] << 8) | buffer[len - 3U];
        short val4 = (buffer[len - 2U] << 8) | buffer[len - 1U];
        LogDebug(LOG_SERIAL, "V24 USB: %.*s %X %X %X %X", len - 11U, buffer + 3U, val1, val2, val3, val4);
    }
    else if (buffer[2U] == CMD_DEBUG_DUMP) {
        uint8_t data[255U];
        ::memset(data, 0x00U, 255U);
        ::memcpy(data, buffer, len);
        Utils::dump(1U, "V24 USB Debug Dump", data, len);
    }
}