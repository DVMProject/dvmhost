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

using namespace network;
using namespace modem;
using namespace p25;

SerialService::SerialService(const std::string& portName, uint32_t baudrate, DfsiPeerNetwork* network, uint32_t p25TxQueueSize, uint32_t p25RxQueueSize, bool debug, bool trace) :
    m_portName(portName),
    m_baudrate(baudrate),
    m_port(),
    m_debug(debug),
    m_trace(trace),
    m_network(network),
    m_lastIMBE(nullptr),
    m_streamTimestamps(),
    m_msgBuffer(nullptr),
    m_msgState(RESP_START),
    m_msgLength(0U),
    m_msgOffset(0U),
    m_msgType(CMD_GET_STATUS),
    m_msgDoubleLength(false),
    m_netFrames(0U),
    m_netLost(0U),
    m_rxP25Queue(p25RxQueueSize, "RX P25 Queue"),
    m_txP25Queue(p25TxQueueSize, "TX P25 Queue")
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

    delete[] m_lastIMBE;

    delete[] m_msgBuffer;
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
                if (m_debug) {
                    LogDebug("SERIAL", "Got P25 data from V24 board");
                }
                
                // Get length
                uint8_t length[2U];
                if (m_msgLength > 255U)
                    length[0U] = ((m_msgLength - cmdOffset) >> 8U) & 0xFFU;
                else
                    length[0U] = 0x00U;
                length[1U] = (m_msgLength - cmdOffset) && 0xFFU;
                m_rxP25Queue.addData(length, 2U);

                // Add data tag to queue
                uint8_t data = TAG_DATA;
                m_rxP25Queue.addData(&data, 1U);

                // Add P25 data to buffer
                m_rxP25Queue.addData(m_msgBuffer + (cmdOffset + 1U), m_msgLength - (cmdOffset + 1U));
            }
            break;

            // P25 data lost is also handled, though the V24 board doesn't implement it (yet?)
            case CMD_P25_LOST:
            {
                if (m_debug)
                    LogDebug("SERIAL", "Got P25 lost msg from V24 board");

                if (m_msgDoubleLength) {
                    LogError("SERIAL", "CMD_P25_LOST got double length byte?");
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
                LogError("SERIAL", "Unhandled command from V24 board: %02X", m_msgBuffer[2U]);
            }
        }
    }
}

bool SerialService::open()
{
    LogMessage("SERIAL", "Opening port %s at %u baud", m_portName.c_str(), m_baudrate);

    bool ret = m_port->open();

    if (!ret) {
        LogError("SERIAL", "Failed to open port!");
        return false;
    }

    m_msgState = RESP_START;

    return true;
}

void SerialService::close()
{
    LogMessage("SERIAL", "Closing port");
    m_port->close();
}

/// <summary>
/// Process P25 data from the peer network and send to writeP25Frame()
/// </summary>
void SerialService::processP25(UInt8Array p25Buffer, uint32_t length)
{
    // Get the current timestamp
    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // Decode grant info
    bool grantDemand = (p25Buffer[14U] & 0x80U) == 0x80U;
    bool grantDenial = (p25Buffer[14U] & 0x40U) == 0x40U;
    bool unitToUnit = (p25Buffer[14U] & 0x01U) == 0x01U;

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
        LogMessage("SERIAL", "Got P25 PDU, we don't handle these (yet)");
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
                LogDebug(LOG_NET, "P25, HDU algId = $%02X, kId = $%02X", algId, kid);
                Utils::dump(1U, "P25 HDU Network MI", mi, p25::P25_MI_LENGTH_BYTES);
            }

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

    if (m_debug) {
        Utils::dump(2U, "!!! *P25 Network Frame", data.get(), frameLength);
    }

    uint8_t* message = data.get();

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
                    LogMessage(LOG_NET, P25_LDU1_STR " audio, srcId = %u, dstId = %u, group = %u, emerg = %u, encrypt = %u, prio = %u",
                        control.getSrcId(), control.getDstId(), control.getGroup(), control.getEmergency(), control.getEncrypted(), control.getPriority());

                    insertMissingAudio(netLDU1, m_netLost);

                    writeP25Frame(netLDU1, 225U);
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
                    LogMessage(LOG_NET, P25_LDU2_STR " audio, algo = $%02X, kid = $%04X", control.getAlgId(), control.getKId());

                    insertMissingAudio(netLDU2, m_netLost);

                    writeP25Frame(netLDU2, 225U);
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

                //writeNet_TDU(control.getSrcId(), control.getDstId());
            }
            break;
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
            LogError("SERIAL", "Error reading from serial port, ret = %d", ret);
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
            LogError("SERIAL", "Error reading from serial port, ret = %d", ret);
            m_msgState = RESP_START;
            return RTM_ERROR;
        }

        // Handle no data
        if (ret == 0) {
            return RTM_TIMEOUT;
        }

        // Handle invalid length
        if (m_msgBuffer[1U] >= 250U && !m_msgDoubleLength) {
            LogError("SERIAL", "Invalid length received from the modem, len = %u", m_msgBuffer[1U]);
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
            LogError("SERIAL", "Error reading from serial port, ret = %d", ret);
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
            LogError("SERIAL", "Error reading from serial port, ret = %d", ret);
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
        if (m_debug) {
            LogDebug("SERIAL", "readSerial(), RESP_DATA, len = %u, offset = %u, type = %02X", m_msgLength, m_msgOffset, m_msgType);
        }

        // Get the data based on the earlier length
        while (m_msgOffset < m_msgLength) {
            int ret = m_port->read(m_msgBuffer + m_msgOffset, m_msgLength - m_msgOffset);

            if (ret < 0) {
                LogError("SERIAL", "Error reading from serial port, ret = %d", ret);
                m_msgState = RESP_START;
                return RTM_ERROR;
            }

            if (ret == 0)
                return RTM_TIMEOUT;

            if (ret > 0)
                m_msgOffset += ret;
        }

        if (m_debug && m_trace)
            Utils::dump(1U, "Serial readSerial()", m_msgBuffer, m_msgLength);
    }

    m_msgState = RESP_START;
    m_msgOffset = 0U;

    return RTM_OK;
}

int SerialService::writeSerial(const uint8_t* data, uint32_t length)
{
    return m_port->write(data, length);
}

uint32_t SerialService::readP25Frame(uint8_t* data)
{
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

void SerialService::writeP25Frame(const uint8_t* data, uint32_t length)
{
    assert(data != nullptr);
    assert(length > 0U);

    // TODO: do the RS encoding/FEC calculations n stuff (borrow this from the modem class?)

    // Buffer for outgoing message (padded for the DVM signalling bytes)
    uint8_t buffer[length + 4];
    ::memset(buffer, 0x00, length + 4);

    buffer[0] = DVM_SHORT_FRAME_START;
    buffer[1] = (uint8_t)(length + 4);
    buffer[2] = CMD_P25_DATA;
    buffer[3] = 0x00;

    // Copy the P25 data
    ::memcpy(buffer + 4, data, length);

    // Write 
    // TODO: we need to create a jitter buffer that propely doles out the P25 frames at 20ms increments in our clock() thread
    //       look at the C# SerialService Send() function to get an idea on how

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
            LogError("SERIAL", "Invalid debug data received from the V24 board, len = %u", len);
            return;
        }
    }

    // Handle the individual debug types
    if (buffer[2U] == CMD_DEBUG1) {
        LogDebug("SERIAL", "V24: %.*s", len - 3U, buffer + 3U);
    }
    else if (buffer[2U] == CMD_DEBUG2) {
        short val1 = (buffer[len - 2U] << 8) | buffer[len - 1U];
        LogDebug("SERIAL", "V24: %.*s %X", len - 5U, buffer + 3U, val1);
    }
    else if (buffer[2U] == CMD_DEBUG3) {
        short val1 = (buffer[len - 4U] << 8) | buffer[len - 3U];
        short val2 = (buffer[len - 2U] << 8) | buffer[len - 1U];
        LogDebug("SERIAL", "V24: %.*s %X %X", len - 7U, buffer + 3U, val1, val2);
    }
    else if (buffer[2U] == CMD_DEBUG4) {
        short val1 = (buffer[len - 6U] << 8) | buffer[len - 5U];
        short val2 = (buffer[len - 4U] << 8) | buffer[len - 3U];
        short val3 = (buffer[len - 2U] << 8) | buffer[len - 1U];
        LogDebug("SERIAL", "V24: %.*s %X %X %X", len - 9U, buffer + 3U, val1, val2, val3);
    }
    else if (buffer[2U] == CMD_DEBUG5) {
        short val1 = (buffer[len - 8U] << 8) | buffer[len - 7U];
        short val2 = (buffer[len - 6U] << 8) | buffer[len - 5U];
        short val3 = (buffer[len - 4U] << 8) | buffer[len - 3U];
        short val4 = (buffer[len - 2U] << 8) | buffer[len - 1U];
        LogDebug("SERIAL", "V24: %.*s %X %X %X %X", len - 11U, buffer + 3U, val1, val2, val3, val4);
    }
    else if (buffer[2U] == CMD_DEBUG_DUMP) {
        uint8_t data[255U];
        ::memset(data, 0x00U, 255U);
        ::memcpy(data, buffer, len);
        Utils::dump(1U, "V24 Debug Dump", data, len);
    }
}