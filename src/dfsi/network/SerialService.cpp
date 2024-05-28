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

SerialService::SerialService(const std::string& portName, uint16_t baudrate, DfsiPeerNetwork* network, bool debug) :
    m_portName(portName),
    m_baudrate(baudrate),
    m_port(),
    m_debug(debug),
    m_network(network),
    m_lastIMBE(nullptr),
    m_streamTimestamps(),
    m_serialBuffer(nullptr),
    m_serialState(RESP_START),
    m_msgLength(0U),
    m_msgOffset(0U),
    m_msgType(CMD_GET_STATUS),
    m_netFrames(0U),
    m_netLost(0U)
{
    assert(!portName.empty());
    assert(baudrate > 0U);

    // Setup serial
    port::SERIAL_SPEED serialSpeed = port::SERIAL_115200;

    m_port = new port::UARTPort(portName, serialSpeed, false);

    m_lastIMBE = new uint8_t[11U];
    ::memcpy(m_lastIMBE, P25_NULL_IMBE, 11U);

    m_serialBuffer = new uint8_t[BUFFER_LENGTH];
}

SerialService::~SerialService()
{
    if (m_port != nullptr) {
        delete m_port;
    }

    delete[] m_lastIMBE;

    delete[] m_serialBuffer;
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
        // Get command type
        switch (m_serialBuffer[2U]) {
            case CMD_P25_DATA:
            {
                // Get the length
                uint8_t length = m_serialBuffer[1U];
                
                // Extract the P25 data
                uint8_t dfsiData[length - 4];
                ::memset(dfsiData, 0x00, length - 4);
                ::memcpy(dfsiData, m_serialBuffer + 4, length - 4);

                // Process the DFSI data TODO
            }
            break;

            default:
            {
                LogError("SERIAL", "Unhandled command from V24 board: %02X", m_serialBuffer[2U]);
            }
        }
    }
}

bool SerialService::open()
{
    LogMessage("SERIAL", "Opening port %s at %u baud", m_portName, m_baudrate);

    bool ret = m_port->open();

    if (!ret) {
        LogError("SERIAL", "Failed to open port!");
        return false;
    }

    m_serialState = RESP_START;

    return true;
}

void SerialService::close()
{
    LogMessage("SERIAL", "Closing port");
    m_port->close();
}

/// <summary>
/// Process P25 data from the peer network
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
            //processTSDU(data.get(), frameLength, control, lsd);
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
    // If we're waiting for a message start byte, read a single byte
    if (m_serialState == RESP_START) {
        // Try and read a byte from the serial port
        int ret = m_port->read(m_serialBuffer + 0U, 1U);

        // Catch read error
        if (ret < 0) {
            LogError("SERIAL", "Error reading from serial port, ret = %d", ret);
            m_serialState = RESP_START;
            return RTM_ERROR;
        }

        // Handle no data
        if (ret == 0) {
            return RTM_TIMEOUT;
        }

        // Handle anything other than a start flag while in RESP_START mode
        if (m_serialBuffer[0U] != DVM_SHORT_FRAME_START &&
            m_serialBuffer[0U] != DVM_LONG_FRAME_START) {
            ::memset(m_serialBuffer, 0x00U, BUFFER_LENGTH);
            return RTM_ERROR;
        }

        // The V24 serial adapter currently doesn't send a long frame
        if (m_serialBuffer[0U] == DVM_LONG_FRAME_START) {
            ::memset(m_serialBuffer, 0x00U, BUFFER_LENGTH);
            LogError("SERIAL", "Got long start frame flag, not currently supported");
            return RTM_ERROR;
        }

        // By default, we assume we got a short start frame
        m_serialState = RESP_LENGTH1;
    }

    // Receive a short frame
    if (m_serialState == RESP_LENGTH1) {
        // Read length
        int ret = m_port->read(m_serialBuffer + 1U, 1U);
        
        // Catch read error
        if (ret < 0) {
            LogError("SERIAL", "Error reading from serial port, ret = %d", ret);
            m_serialState = RESP_START;
            return RTM_ERROR;
        }

        // Handle no data
        if (ret == 0) {
            return RTM_TIMEOUT;
        }

        // Handle invalid length
        if (m_serialBuffer[1U] >= 250U) {
            LogError("SERIAL", "Invalid length received from the modem, len = %u", m_serialBuffer[1U]);
            return RTM_ERROR;
        }

        // Process the length and continue
        m_msgLength = m_serialBuffer[1U];
        m_msgOffset = 2U;
        m_serialState = RESP_TYPE;
    }

    if (m_serialState == RESP_TYPE) {
        // Read type
        int ret = m_port->read(m_serialBuffer + m_msgOffset, 1U);
        
        // Catch read error
        if (ret < 0) {
            LogError("SERIAL", "Error reading from serial port, ret = %d", ret);
            m_serialState = RESP_START;
            return RTM_ERROR;
        }

        // Handle no data
        if (ret == 0) {
            return RTM_TIMEOUT;
        }

        // Get command type
        m_msgType = (DVM_COMMANDS)m_serialBuffer[m_msgOffset];

        // Move on
        m_serialState = RESP_DATA;
        m_msgOffset++;
    }

    // Get the data
    if (m_serialState == RESP_DATA) {
        if (m_debug) {
            LogDebug("SERIAL", "readSerial(), RESP_DATA, len = %u, offset = %u, type = %02X", m_msgLength, m_msgOffset, m_msgType);
        }

        // Get the data based on the earlier length
        while (m_msgOffset < m_msgLength) {
            int ret = m_port->read(m_serialBuffer + m_msgOffset, m_msgLength - m_msgOffset);

            if (ret < 0) {
                LogError("SERIAL", "Error reading from serial port, ret = %d", ret);
                m_serialState = RESP_START;
                return RTM_ERROR;
            }

            if (ret == 0)
                return RTM_TIMEOUT;

            if (ret > 0)
                m_msgOffset += ret;
        }

        if (m_debug)
            Utils::dump(1U, "Serial readSerial()", m_serialBuffer, m_msgLength);
    }

    m_serialState = RESP_START;
    m_msgOffset = 0U;

    return RTM_OK;
}

int SerialService::writeSerial(const uint8_t* data, uint32_t length)
{
    return m_port->write(data, length);
}

void SerialService::writeP25Frame(const uint8_t* data, uint32_t length)
{
    assert(data != nullptr);
    assert(length > 0U);

    // Buffer for outgoing message (padded for the DVM signalling bytes)
    uint8_t buffer[length + 4];
    ::memset(buffer, 0x00, length + 4);

    buffer[0] = DVM_SHORT_FRAME_START;
    buffer[1] = (uint8_t)(length + 4);
    buffer[2] = CMD_P25_DATA;
    buffer[3] = 0x00;

    // Copy the P25 data
    ::memcpy(buffer + 4, data, length);

    // Write TODO

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