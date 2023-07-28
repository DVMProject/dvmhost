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
*   Copyright (C) 2015,2016,2017 by Jonathan Naylor G4KLX
*   Copyright (C) 2020-2023 by Bryan Biedenkapp N2PLL
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
#include "edac/SHA256.h"
#include "network/BaseNetwork.h"
#include "Log.h"
#include "Utils.h"

using namespace network;
using namespace network::frame;

#include <cstdio>
#include <cassert>
#include <cstdlib>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the BaseNetwork class.
/// </summary>
/// <param name="peerId">Unique ID of this modem on the network.</param>
/// <param name="duplex">Flag indicating full-duplex operation.</param>
/// <param name="debug">Flag indicating whether network debug is enabled.</param>
/// <param name="slot1">Flag indicating whether DMR slot 1 is enabled for network traffic.</param>
/// <param name="slot2">Flag indicating whether DMR slot 2 is enabled for network traffic.</param>
/// <param name="allowActivityTransfer">Flag indicating that the system activity logs will be sent to the network.</param>
/// <param name="allowDiagnosticTransfer">Flag indicating that the system diagnostic logs will be sent to the network.</param>
/// <param name="localPort">Local port used to listen for incoming data.</param>
BaseNetwork::BaseNetwork(uint32_t peerId, bool duplex, bool debug, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, uint16_t localPort) :
    m_peerId(peerId),
    m_status(NET_STAT_INVALID),
    m_addr(),
    m_addrLen(0U),
    m_slot1(slot1),
    m_slot2(slot2),
    m_duplex(duplex),
    m_allowActivityTransfer(allowActivityTransfer),
    m_allowDiagnosticTransfer(allowDiagnosticTransfer),
    m_debug(debug),
    m_socket(nullptr),
    m_frameQueue(nullptr),
    m_rxDMRData(4000U, "DMR Net Buffer"),
    m_rxP25Data(4000U, "P25 Net Buffer"),
    m_rxNXDNData(4000U, "NXDN Net Buffer"),
    m_random(),
    m_dmrStreamId(nullptr),
    m_p25StreamId(0U),
    m_nxdnStreamId(0U),
    m_pktSeq(0U),
    m_audio()
{
    assert(peerId < 999999999U);

    m_socket = new UDPSocket(localPort);
    m_frameQueue = new FrameQueue(m_socket, peerId, debug);

    std::random_device rd;
    std::mt19937 mt(rd());
    m_random = mt;

    m_dmrStreamId = new uint32_t[2U];
    m_dmrStreamId[0U] = createStreamId();
    m_dmrStreamId[1U] = createStreamId();
    m_p25StreamId = createStreamId();
    m_nxdnStreamId = createStreamId();
}

/// <summary>
/// Finalizes a instance of the BaseNetwork class.
/// </summary>
BaseNetwork::~BaseNetwork()
{
    if (m_frameQueue != nullptr) {
        delete m_frameQueue;
    }

    if (m_socket != nullptr) {
        delete m_socket;
    }

    delete[] m_dmrStreamId;
}

/// <summary>
/// Writes grant request to the network.
/// </summary>
/// <param name="mode"></param>
/// <param name="srcId"></param>
/// <param name="dstId"></param>
/// <param name="slot"></param>
/// <param name="unitToUnit"></param>
/// <returns></returns>
bool BaseNetwork::writeGrantReq(const uint8_t mode, const uint32_t srcId, const uint32_t dstId, const uint8_t slot, const bool unitToUnit)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    uint8_t buffer[MSG_HDR_SIZE];
    ::memset(buffer, 0x00U, MSG_HDR_SIZE);

    __SET_UINT32(srcId, buffer, 11U);                                               // Source Address
    __SET_UINT32(dstId, buffer, 15U);                                               // Destination Address
    buffer[19U] = slot;                                                             // Slot Number

    if (unitToUnit)
        buffer[19U] |= 0x80U;

    buffer[20U] = mode;                                                             // DVM Mode State

    return writeMaster({ NET_FUNC_GRANT_REQ, NET_SUBFUNC_NOP }, buffer, MSG_HDR_SIZE, 0U, 0U);
}

/// <summary>
/// Writes the local activity log to the network.
/// </summary>
/// <param name="message"></param>
/// <returns></returns>
bool BaseNetwork::writeActLog(const char* message)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    if (!m_allowActivityTransfer)
        return false;

    assert(message != nullptr);

    char buffer[DATA_PACKET_LENGTH];
    uint32_t len = ::strlen(message);

    ::strcpy(buffer + 11U, message);

    return writeMaster({ NET_FUNC_TRANSFER, NET_TRANSFER_SUBFUNC_ACTIVITY }, (uint8_t*)buffer, (uint32_t)len + 12U,
        0U, 0U);
}

/// <summary>
/// Writes the local diagnostics log to the network.
/// </summary>
/// <param name="message"></param>
/// <returns></returns>
bool BaseNetwork::writeDiagLog(const char* message)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    if (!m_allowDiagnosticTransfer)
        return false;

    assert(message != nullptr);

    char buffer[DATA_PACKET_LENGTH];
    uint32_t len = ::strlen(message);

    ::strcpy(buffer + 11U, message);

    return writeMaster({ NET_FUNC_TRANSFER, NET_TRANSFER_SUBFUNC_DIAG }, (uint8_t*)buffer, (uint32_t)len + 12U,
        0U, 0U);
}

/// <summary>
/// Resets the DMR ring buffer for the given slot.
/// </summary>
/// <param name="slotNo">DMR slot ring buffer to reset.</param>
void BaseNetwork::resetDMR(uint32_t slotNo)
{
    assert(slotNo == 1U || slotNo == 2U);

    if (slotNo == 1U) {
        m_dmrStreamId[0U] = createStreamId();
    }
    else {
        m_dmrStreamId[1U] = createStreamId();
    }

    m_pktSeq = 0U;
    m_rxDMRData.clear();
}

/// <summary>
/// Resets the P25 ring buffer.
/// </summary>
void BaseNetwork::resetP25()
{
    m_p25StreamId = createStreamId();
    m_pktSeq = 0U;
    m_rxP25Data.clear();
}

/// <summary>
/// Resets the NXDN ring buffer.
/// </summary>
void BaseNetwork::resetNXDN()
{
    m_nxdnStreamId = createStreamId();
    m_pktSeq = 0U;
    m_rxNXDNData.clear();
}

/// <summary>
/// Gets the current DMR stream ID.
/// </summary>
/// <param name="slotNo">DMR slot to get stream ID for.</param>
/// <returns>Stream ID for the given DMR slot.<returns>
uint32_t BaseNetwork::getDMRStreamId(uint32_t slotNo) const
{
    assert(slotNo == 1U || slotNo == 2U);

    if (slotNo == 1U) {
        return m_dmrStreamId[0U];
    }
    else {
        return m_dmrStreamId[1U];
    }
}

/// <summary>
/// Helper to send a data message to the master.
/// </summary>
/// <param name="opcode">Opcode.</param>
/// <param name="data">Buffer to write to the network.</param>
/// <param name="length">Length of buffer to write.</param>
/// <param name="pktSeq"></param>
/// <param name="streamId"></param>
bool BaseNetwork::writeMaster(FrameQueue::OpcodePair opcode, const uint8_t* data, uint32_t length, uint16_t pktSeq, uint32_t streamId)
{
    m_frameQueue->enqueueMessage(data, length, streamId, m_peerId, opcode, pktSeq, m_addr, m_addrLen);
    return m_frameQueue->flushQueue();
}

/// <summary>
/// Reads DMR raw frame data from the DMR ring buffer.
/// </summary>
/// <param name="ret"></param>
/// <param name="length"></param>
/// <returns></returns>
UInt8Array BaseNetwork::readDMR(bool& ret, uint32_t& frameLength)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return nullptr;

    ret = true;
    if (m_rxDMRData.isEmpty()) {
        ret = false;
        return nullptr;
    }

    uint8_t length = 0U;
    m_rxDMRData.getData(&length, 1U);
    if (length == 0U) {
        ret = false;
        return nullptr;
    }

    UInt8Array buffer;
    frameLength = length;
    buffer = std::unique_ptr<uint8_t[]>(new uint8_t[length]);
    ::memset(buffer.get(), 0x00U, length);
    m_rxDMRData.getData(buffer.get(), length);

    return buffer;
}

/// <summary>
/// Writes DMR frame data to the network.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
bool BaseNetwork::writeDMR(const dmr::data::Data& data)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    uint32_t slotNo = data.getSlotNo();

    // individual slot disabling
    if (slotNo == 1U && !m_slot1)
        return false;
    if (slotNo == 2U && !m_slot2)
        return false;

    uint8_t dataType = data.getDataType();

    uint32_t slotIndex = slotNo - 1U;

    bool resetSeq = false;
    if (dataType == dmr::DT_VOICE_LC_HEADER) {
        resetSeq = true;
        m_dmrStreamId[slotIndex] = createStreamId();
    }

    if (dataType == dmr::DT_CSBK || dataType == dmr::DT_DATA_HEADER) {
        resetSeq = true;
        m_dmrStreamId[slotIndex] = createStreamId();
    }

    uint32_t messageLength = 0U;
    UInt8Array message = createDMR_Message(messageLength, m_dmrStreamId[slotIndex], data);
    if (message == nullptr) {
        return false;
    }

    return writeMaster({ NET_FUNC_PROTOCOL, NET_PROTOCOL_SUBFUNC_DMR }, message.get(), messageLength, pktSeq(resetSeq), m_dmrStreamId[slotIndex]);
}

/// <summary>
/// Helper to test if the DMR ring buffer has data.
/// </summary>
/// <returns>True if ring buffer contains data, otherwise false.</returns>
bool BaseNetwork::hasDMRData() const
{
    if (m_rxDMRData.isEmpty())
        return false;

    return true;
}

/// <summary>
/// Reads P25 raw frame data from the P25 ring buffer.
/// </summary>
/// <param name="ret"></param>
/// <param name="length"></param>
/// <returns></returns>
UInt8Array BaseNetwork::readP25(bool& ret, uint32_t& frameLength)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return nullptr;

    ret = true;
    if (m_rxP25Data.isEmpty()) {
        ret = false;
        return nullptr;
    }

    uint8_t length = 0U;
    m_rxP25Data.getData(&length, 1U);
    if (length == 0U) {
        ret = false;
        return nullptr;
    }

    UInt8Array buffer;
    frameLength = length;
    buffer = std::unique_ptr<uint8_t[]>(new uint8_t[length]);
    ::memset(buffer.get(), 0x00U, length);
    m_rxP25Data.getData(buffer.get(), length);

    return buffer;
}

/// <summary>
/// Writes P25 LDU1 frame data to the network.
/// </summary>
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <param name="data"></param>
/// <param name="frameType"></param>
/// <returns></returns>
bool BaseNetwork::writeP25LDU1(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t* data, uint8_t frameType)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    bool resetSeq = false;
    if (m_p25StreamId == 0U) {
        resetSeq = true;
        m_p25StreamId = createStreamId();
    }

    uint32_t messageLength = 0U;
    UInt8Array message = createP25_LDU1Message(messageLength, control, lsd, data, frameType);
    if (message == nullptr) {
        return false;
    }

    return writeMaster({ NET_FUNC_PROTOCOL, NET_PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, pktSeq(resetSeq), m_p25StreamId);
}

/// <summary>
/// Writes P25 LDU2 frame data to the network.
/// </summary>
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <param name="data"></param>
/// <returns></returns>
bool BaseNetwork::writeP25LDU2(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t* data)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    bool resetSeq = false;
    if (m_p25StreamId == 0U) {
        resetSeq = true;
        m_p25StreamId = createStreamId();
    }

    uint32_t messageLength = 0U;
    UInt8Array message = createP25_LDU2Message(messageLength, control, lsd, data);
    if (message == nullptr) {
        return false;
    }

    return writeMaster({ NET_FUNC_PROTOCOL, NET_PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, pktSeq(resetSeq), m_p25StreamId);
}

/// <summary>
/// Writes P25 TDU frame data to the network.
/// </summary>
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <returns></returns>
bool BaseNetwork::writeP25TDU(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    bool resetSeq = false;
    if (m_p25StreamId == 0U) {
        resetSeq = true;
        m_p25StreamId = createStreamId();
    }

    uint32_t messageLength = 0U;
    UInt8Array message = createP25_TDUMessage(messageLength, control, lsd);
    if (message == nullptr) {
        return false;
    }

    return writeMaster({ NET_FUNC_PROTOCOL, NET_PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, pktSeq(resetSeq), m_p25StreamId);
}

/// <summary>
/// Writes P25 TSDU frame data to the network.
/// </summary>
/// <param name="control"></param>
/// <param name="data"></param>
/// <returns></returns>
bool BaseNetwork::writeP25TSDU(const p25::lc::LC& control, const uint8_t* data)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    bool resetSeq = false;
    if (m_p25StreamId == 0U) {
        resetSeq = true;
        m_p25StreamId = createStreamId();
    }

    uint32_t messageLength = 0U;
    UInt8Array message = createP25_TSDUMessage(messageLength, control, data);
    if (message == nullptr) {
        return false;
    }

    return writeMaster({ NET_FUNC_PROTOCOL, NET_PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, pktSeq(resetSeq), m_p25StreamId);
}

/// <summary>
/// Writes P25 PDU frame data to the network.
/// </summary>
/// <param name="header"></param>
/// <param name="currentBlock"></param>
/// <param name="data"></param>
/// <param name="len"></param>
/// <returns></returns>
bool BaseNetwork::writeP25PDU(const p25::data::DataHeader& header, const uint8_t currentBlock, const uint8_t* data,
    const uint32_t len)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    bool resetSeq = false;
    if (m_p25StreamId == 0U) {
        resetSeq = true;
        m_p25StreamId = createStreamId();
    }

    uint32_t messageLength = 0U;
    UInt8Array message = createP25_PDUMessage(messageLength, header, currentBlock, data, len);
    if (message == nullptr) {
        return false;
    }

    return writeMaster({ NET_FUNC_PROTOCOL, NET_PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, pktSeq(resetSeq), m_p25StreamId);
}

/// <summary>
/// Helper to test if the P25 ring buffer has data.
/// </summary>
/// <returns>True if ring buffer contains data, otherwise false.</returns>
bool BaseNetwork::hasP25Data() const
{
    if (m_rxP25Data.isEmpty())
        return false;

    return true;
}

/// <summary>
/// Reads NXDN raw frame data from the NXDN ring buffer.
/// </summary>
/// <param name="ret"></param>
/// <param name="length"></param>
/// <returns></returns>
UInt8Array BaseNetwork::readNXDN(bool& ret, uint32_t& frameLength)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return nullptr;

    ret = true;
    if (m_rxNXDNData.isEmpty()) {
        ret = false;
        return nullptr;
    }

    uint8_t length = 0U;
    m_rxNXDNData.getData(&length, 1U);
    if (length == 0U) {
        ret = false;
        return nullptr;
    }

    UInt8Array buffer;
    frameLength = length;
    buffer = std::unique_ptr<uint8_t[]>(new uint8_t[length]);
    ::memset(buffer.get(), 0x00U, length);
    m_rxNXDNData.getData(buffer.get(), length);

    return buffer;
}

/// <summary>
/// Writes NXDN frame data to the network.
/// </summary>
/// <param name="lc"></param>
/// <param name="data"></param>
/// <param name="len"></param>
/// <returns></returns>
bool BaseNetwork::writeNXDN(const nxdn::lc::RTCH& lc, const uint8_t* data, const uint32_t len)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    bool resetSeq = false;
    if (m_nxdnStreamId == 0U) {
        resetSeq = true;
        m_nxdnStreamId = createStreamId();
    }

    uint32_t messageLength = 0U;
    UInt8Array message = createNXDN_Message(messageLength, lc, data, len);
    if (message == nullptr) {
        return false;
    }

    return writeMaster({ NET_FUNC_PROTOCOL, NET_PROTOCOL_SUBFUNC_NXDN }, message.get(), messageLength, pktSeq(resetSeq), m_nxdnStreamId);
}

/// <summary>
/// Helper to test if the NXDN ring buffer has data.
/// </summary>
/// <returns>True if ring buffer contains data, otherwise false.</returns>
bool BaseNetwork::hasNXDNData() const
{
    if (m_rxNXDNData.isEmpty())
        return false;

    return true;
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Helper to update the RTP packet sequence.
/// </summary>
/// <param name="reset"></param>
/// <returns>RTP packet sequence.</returns>
uint16_t BaseNetwork::pktSeq(bool reset)
{
    if (reset) {
        m_pktSeq = 0U;
    }

    uint16_t curr = m_pktSeq;
    ++m_pktSeq;
    if (m_pktSeq > UINT16_MAX) {
        m_pktSeq = 0U;
    }

    return curr;
}

/// <summary>
/// Creates an DMR frame message.
/// </summary>
/// <param name="length"></param>
/// <param name="streamId"></param>
/// <param name="data"></param>
/// <returns></returns>
UInt8Array BaseNetwork::createDMR_Message(uint32_t& length, const uint32_t streamId, const dmr::data::Data& data)
{
    uint8_t* buffer = new uint8_t[DMR_PACKET_LENGTH + PACKET_PAD];
    ::memset(buffer, 0x00U, DMR_PACKET_LENGTH + PACKET_PAD);

    // construct DMR message header
    ::memcpy(buffer + 0U, TAG_DMR_DATA, 4U);

    uint32_t srcId = data.getSrcId();                                               // Source Address
    __SET_UINT16(srcId, buffer, 5U);

    uint32_t dstId = data.getDstId();                                               // Target Address
    __SET_UINT16(dstId, buffer, 8U);

    uint32_t slotNo = data.getSlotNo();

    buffer[14U] = 0U;                                                               // Control Bits

    // Individual slot disabling
    if (slotNo == 1U && !m_slot1)
        return nullptr;
    if (slotNo == 2U && !m_slot2)
        return nullptr;

    buffer[15U] = slotNo == 1U ? 0x00U : 0x80U;                                     // Slot Number

    uint8_t flco = data.getFLCO();
    buffer[15U] |= flco == dmr::FLCO_GROUP ? 0x00U : 0x40U;                         // Group

    uint8_t dataType = data.getDataType();
    if (dataType == dmr::DT_VOICE_SYNC) {
        buffer[15U] |= 0x10U;
    }
    else if (dataType == dmr::DT_VOICE) {
        buffer[15U] |= data.getN();
    }
    else {
        buffer[15U] |= (0x20U | dataType);
    }

    buffer[4U] = data.getSeqNo();                                                   // Sequence Number

    buffer[53U] = data.getBER();                                                    // Bit Error Rate
    buffer[54U] = data.getRSSI();                                                   // RSSI

    // pack raw DMR message bytes
    data.getData(buffer + 20U);

    if (m_debug)
        Utils::dump(1U, "Network Message, DMR", buffer, (DMR_PACKET_LENGTH + PACKET_PAD));

    length = (DMR_PACKET_LENGTH + PACKET_PAD);
    return UInt8Array(buffer);
}

/// <summary>
/// Creates an P25 frame message header.
/// </summary>
/// <param name="buffer"></param>
/// <param name="duid"></param>
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <param name="frameType"></param>
void BaseNetwork::createP25_MessageHdr(uint8_t* buffer, uint8_t duid, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd,
    uint8_t frameType)
{
    assert(buffer != nullptr);

    // construct P25 message header
    ::memcpy(buffer + 0U, TAG_P25_DATA, 4U);

    buffer[4U] = control.getLCO();                                                  // LCO

    uint32_t srcId = control.getSrcId();                                            // Source Address
    __SET_UINT16(srcId, buffer, 5U);

    uint32_t dstId = control.getDstId();                                            // Target Address
    __SET_UINT16(dstId, buffer, 8U);

    uint16_t sysId = control.getSiteData().sysId();                                 // System ID
    buffer[11U] = (sysId >> 8) & 0xFFU;
    buffer[12U] = (sysId >> 0) & 0xFFU;

    buffer[14U] = 0U;                                                               // Control Bits

    buffer[15U] = control.getMFId();                                                // MFId

    uint32_t netId = control.getSiteData().netId();                                 // Network ID
    __SET_UINT16(netId, buffer, 16U);

    buffer[20U] = lsd.getLSD1();                                                    // LSD 1
    buffer[21U] = lsd.getLSD2();                                                    // LSD 2

    buffer[22U] = duid;                                                             // DUID

    if (frameType != p25::P25_FT_TERMINATOR) {
        buffer[180U] = frameType;                                                   // DVM Frame Type
    }

    // is this the first frame of a call?
    if (frameType == p25::P25_FT_HDU_VALID) {
        buffer[181U] = control.getAlgId();                                          // Algorithm ID

        uint32_t kid = control.getKId();
        buffer[182U] = (kid >> 8) & 0xFFU;                                          // Key ID
        buffer[183U] = (kid >> 0) & 0xFFU;

        // copy MI data
        uint8_t mi[p25::P25_MI_LENGTH_BYTES];
        ::memset(mi, 0x00U, p25::P25_MI_LENGTH_BYTES);
        control.getMI(mi);

        if (m_debug) {
            Utils::dump(1U, "P25 HDU MI written to network", mi, p25::P25_MI_LENGTH_BYTES);
        }

        for (uint8_t i = 0; i < p25::P25_MI_LENGTH_BYTES; i++) {
            buffer[184U + i] = mi[i];                                               // Message Indicator
        }
    }
}

/// <summary>
/// Creates an P25 LDU1 frame message.
/// </summary>
/// <param name="length"></param>
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <param name="data"></param>
/// <param name="frameType"></param>
/// <returns></returns>
UInt8Array BaseNetwork::createP25_LDU1Message(uint32_t& length, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, 
    const uint8_t* data, uint8_t frameType)
{
    assert(data != nullptr);

    p25::dfsi::LC dfsiLC = p25::dfsi::LC(control, lsd);

    uint8_t* buffer = new uint8_t[P25_LDU1_PACKET_LENGTH + PACKET_PAD];
    ::memset(buffer, 0x00U, P25_LDU1_PACKET_LENGTH + PACKET_PAD);

    // construct P25 message header
    createP25_MessageHdr(buffer, p25::P25_DUID_LDU1, control, lsd, frameType);

    // pack DFSI data
    uint32_t count = MSG_HDR_SIZE;
    uint8_t imbe[p25::P25_RAW_IMBE_LENGTH_BYTES];

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE1);
    m_audio.decode(data, imbe, 0U);
    dfsiLC.encodeLDU1(buffer + 24U, imbe);
    count += p25::dfsi::P25_DFSI_LDU1_VOICE1_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE2);
    m_audio.decode(data, imbe, 1U);
    dfsiLC.encodeLDU1(buffer + 46U, imbe);
    count += p25::dfsi::P25_DFSI_LDU1_VOICE2_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE3);
    m_audio.decode(data, imbe, 2U);
    dfsiLC.encodeLDU1(buffer + 60U, imbe);
    count += p25::dfsi::P25_DFSI_LDU1_VOICE3_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE4);
    m_audio.decode(data, imbe, 3U);
    dfsiLC.encodeLDU1(buffer + 77U, imbe);
    count += p25::dfsi::P25_DFSI_LDU1_VOICE4_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE5);
    m_audio.decode(data, imbe, 4U);
    dfsiLC.encodeLDU1(buffer + 94U, imbe);
    count += p25::dfsi::P25_DFSI_LDU1_VOICE5_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE6);
    m_audio.decode(data, imbe, 5U);
    dfsiLC.encodeLDU1(buffer + 111U, imbe);
    count += p25::dfsi::P25_DFSI_LDU1_VOICE6_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE7);
    m_audio.decode(data, imbe, 6U);
    dfsiLC.encodeLDU1(buffer + 128U, imbe);
    count += p25::dfsi::P25_DFSI_LDU1_VOICE7_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE8);
    m_audio.decode(data, imbe, 7U);
    dfsiLC.encodeLDU1(buffer + 145U, imbe);
    count += p25::dfsi::P25_DFSI_LDU1_VOICE8_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE9);
    m_audio.decode(data, imbe, 8U);
    dfsiLC.encodeLDU1(buffer + 162U, imbe);
    count += p25::dfsi::P25_DFSI_LDU1_VOICE9_FRAME_LENGTH_BYTES;

    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "Network Message, P25 LDU1", buffer, (P25_LDU1_PACKET_LENGTH + PACKET_PAD));

    length = (P25_LDU1_PACKET_LENGTH + PACKET_PAD);
    return UInt8Array(buffer);
}

/// <summary>
/// Creates an P25 LDU2 frame message.
/// </summary>
/// <param name="length"></param>
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <param name="data"></param>
/// <returns></returns>
UInt8Array BaseNetwork::createP25_LDU2Message(uint32_t& length, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, 
    const uint8_t* data)
{
    assert(data != nullptr);

    p25::dfsi::LC dfsiLC = p25::dfsi::LC(control, lsd);

    uint8_t* buffer = new uint8_t[P25_LDU2_PACKET_LENGTH + PACKET_PAD];
    ::memset(buffer, 0x00U, P25_LDU2_PACKET_LENGTH + PACKET_PAD);

    // construct P25 message header
    createP25_MessageHdr(buffer, p25::P25_DUID_LDU2, control, lsd, p25::P25_FT_DATA_UNIT);

    // pack DFSI data
    uint32_t count = MSG_HDR_SIZE;
    uint8_t imbe[p25::P25_RAW_IMBE_LENGTH_BYTES];

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE10);
    m_audio.decode(data, imbe, 0U);
    dfsiLC.encodeLDU2(buffer + 24U, imbe);
    count += p25::dfsi::P25_DFSI_LDU2_VOICE10_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE11);
    m_audio.decode(data, imbe, 1U);
    dfsiLC.encodeLDU2(buffer + 46U, imbe);
    count += p25::dfsi::P25_DFSI_LDU2_VOICE11_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE12);
    m_audio.decode(data, imbe, 2U);
    dfsiLC.encodeLDU2(buffer + 60U, imbe);
    count += p25::dfsi::P25_DFSI_LDU2_VOICE12_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE13);
    m_audio.decode(data, imbe, 3U);
    dfsiLC.encodeLDU2(buffer + 77U, imbe);
    count += p25::dfsi::P25_DFSI_LDU2_VOICE13_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE14);
    m_audio.decode(data, imbe, 4U);
    dfsiLC.encodeLDU2(buffer + 94U, imbe);
    count += p25::dfsi::P25_DFSI_LDU2_VOICE14_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE15);
    m_audio.decode(data, imbe, 5U);
    dfsiLC.encodeLDU2(buffer + 111U, imbe);
    count += p25::dfsi::P25_DFSI_LDU2_VOICE15_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE16);
    m_audio.decode(data, imbe, 6U);
    dfsiLC.encodeLDU2(buffer + 128U, imbe);
    count += p25::dfsi::P25_DFSI_LDU2_VOICE16_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE17);
    m_audio.decode(data, imbe, 7U);
    dfsiLC.encodeLDU2(buffer + 145U, imbe);
    count += p25::dfsi::P25_DFSI_LDU2_VOICE17_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE18);
    m_audio.decode(data, imbe, 8U);
    dfsiLC.encodeLDU2(buffer + 162U, imbe);
    count += p25::dfsi::P25_DFSI_LDU2_VOICE18_FRAME_LENGTH_BYTES;

    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "Network Message, P25 LDU2", buffer, (P25_LDU2_PACKET_LENGTH + PACKET_PAD));

    length = (P25_LDU2_PACKET_LENGTH + PACKET_PAD);
    return UInt8Array(buffer);
}

/// <summary>
/// Creates an P25 TDU frame message.
/// </summary>
/// <param name="length"></param>
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <returns></returns>
UInt8Array BaseNetwork::createP25_TDUMessage(uint32_t& length, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd)
{
    uint8_t* buffer = new uint8_t[MSG_HDR_SIZE + PACKET_PAD];
    ::memset(buffer, 0x00U, MSG_HDR_SIZE + PACKET_PAD);

    // construct P25 message header
    createP25_MessageHdr(buffer, p25::P25_DUID_TDU, control, lsd, p25::P25_FT_TERMINATOR);

    buffer[23U] = MSG_HDR_SIZE;

    if (m_debug)
        Utils::dump(1U, "Network Message, P25 TDU", buffer, (MSG_HDR_SIZE + PACKET_PAD));

    length = (MSG_HDR_SIZE + PACKET_PAD);
    return UInt8Array(buffer);
}

/// <summary>
/// Creates an P25 TSDU frame message.
/// </summary>
/// <param name="length"></param>
/// <param name="control"></param>
/// <param name="data"></param>
/// <returns></returns>
UInt8Array BaseNetwork::createP25_TSDUMessage(uint32_t& length, const p25::lc::LC& control, const uint8_t* data)
{
    assert(data != nullptr);

    uint8_t* buffer = new uint8_t[P25_TSDU_PACKET_LENGTH + PACKET_PAD];
    ::memset(buffer, 0x00U, P25_TSDU_PACKET_LENGTH + PACKET_PAD);

    // construct P25 message header
    p25::data::LowSpeedData lsd = p25::data::LowSpeedData();
    createP25_MessageHdr(buffer, p25::P25_DUID_TSDU, control, lsd, p25::P25_FT_TERMINATOR);

    // pack raw P25 TSDU bytes
    uint32_t count = MSG_HDR_SIZE;

    ::memcpy(buffer + 24U, data, p25::P25_TSDU_FRAME_LENGTH_BYTES);
    count += p25::P25_TSDU_FRAME_LENGTH_BYTES;

    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "Network Message, P25 TDSU", buffer, (P25_TSDU_PACKET_LENGTH + PACKET_PAD));

    length = (P25_TSDU_PACKET_LENGTH + PACKET_PAD);
    return UInt8Array(buffer);
}

/// <summary>
/// Writes P25 PDU frame data to the network.
/// </summary>
/// <param name="length"></param>
/// <param name="header"></param>
/// <param name="currentBlock"></param>
/// <param name="data"></param>
/// <param name="len"></param>
/// <returns></returns>
UInt8Array BaseNetwork::createP25_PDUMessage(uint32_t& length, const p25::data::DataHeader& header,
    const uint8_t currentBlock, const uint8_t* data, const uint32_t len)
{
    assert(data != nullptr);

    uint8_t* buffer = new uint8_t[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    /*
    ** PDU packs different bytes into the P25 message header space from the rest of the
    ** P25 DUIDs
    */

    // construct P25 message header
    ::memcpy(buffer + 0U, TAG_P25_DATA, 4U);

    buffer[4U] = header.getSAP();                                                   // Service Access Point
    if (header.getFormat() == p25::PDU_FMT_CONFIRMED) {
        buffer[4U] |= 0x80U;
    }

    __SET_UINT16(len, buffer, 8U);                                                  // PDU Length [bytes]

    buffer[15U] = header.getMFId();                                                 // MFId

    buffer[20U] = header.getBlocksToFollow();                                       // Blocks To Follow
    buffer[21U] = currentBlock;                                                     // Current Block

    buffer[22U] = p25::P25_DUID_PDU;                                                // DUID

    // pack raw P25 PDU bytes
    uint32_t count = MSG_HDR_SIZE;

    ::memcpy(buffer + 24U, data, len);
    count += len;

    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "Network Message, P25 PDU", buffer, (count + PACKET_PAD));

    length = (count + PACKET_PAD);
    return UInt8Array(buffer);
}

/// <summary>
/// Writes NXDN frame data to the network.
/// </summary>
/// <param name="length"></param>
/// <param name="lc"></param>
/// <param name="data"></param>
/// <param name="len"></param>
/// <returns></returns>
UInt8Array BaseNetwork::createNXDN_Message(uint32_t& length, const nxdn::lc::RTCH& lc, const uint8_t* data, const uint32_t len)
{
    assert(data != nullptr);

    uint8_t* buffer = new uint8_t[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    // construct NXDN message header
    ::memcpy(buffer + 0U, TAG_NXDN_DATA, 4U);

    buffer[4U] = lc.getMessageType();                                           // Message Type

    uint32_t srcId = lc.getSrcId();                                             // Source Address
    __SET_UINT16(srcId, buffer, 5U);

    uint32_t dstId = lc.getDstId();                                             // Target Address
    __SET_UINT16(dstId, buffer, 8U);

    buffer[14U] = 0U;                                                           // Control Bits

    buffer[15U] |= lc.getGroup() ? 0x00U : 0x40U;                               // Group

    // pack raw NXDN message bytes
    uint32_t count = MSG_HDR_SIZE;

    ::memcpy(buffer + 24U, data, len);
    count += len;

    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "Network Message, NXDN", buffer, (count + PACKET_PAD));

    length = (count + PACKET_PAD);
    return UInt8Array(buffer);
}
