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
*   Copyright (C) 2020-2022 by Bryan Biedenkapp N2PLL
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
/// <param name="debug"></param>
/// <param name="slot1">Flag indicating whether DMR slot 1 is enabled for network traffic.</param>
/// <param name="slot2">Flag indicating whether DMR slot 2 is enabled for network traffic.</param>
/// <param name="allowActivityTransfer">Flag indicating that the system activity logs will be sent to the network.</param>
/// <param name="allowDiagnosticTransfer">Flag indicating that the system diagnostic logs will be sent to the network.</param>
/// <param name="localPort">Local port used to listen for incoming data.</param>
BaseNetwork::BaseNetwork(uint32_t peerId, bool duplex, bool debug, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, uint16_t localPort) :
    m_peerId(peerId),
    m_slot1(slot1),
    m_slot2(slot2),
    m_allowActivityTransfer(allowActivityTransfer),
    m_allowDiagnosticTransfer(allowDiagnosticTransfer),
    m_duplex(duplex),
    m_debug(debug),
    m_addr(),
    m_addrLen(0U),
    m_socket(localPort),
    m_frameQueue(m_socket, peerId, debug),
    m_dmrStreamId(nullptr),
    m_p25StreamId(0U),
    m_nxdnStreamId(0U),
    m_rxDMRData(4000U, "DMR Net Buffer"),
    m_rxP25Data(4000U, "P25 Net Buffer"),
    m_rxNXDNData(4000U, "NXDN Net Buffer"),
    m_rxGrantData(4000U, "Grant Net Buffer"),
    m_audio(),
    m_random()
{
    assert(peerId > 1000U);

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
    delete[] m_dmrStreamId;
}

/// <summary>
/// Reads DMR frame data from the DMR ring buffer.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
bool BaseNetwork::readDMR(dmr::data::Data& data)
{
    if (m_rxDMRData.isEmpty())
        return false;

    uint8_t length = 0U;
    m_rxDMRData.getData(&length, 1U);
    if (length == 0U)
        return false;

    uint8_t buffer[length];
    ::memset(buffer, 0x00U, length);
    m_rxDMRData.getData(buffer, length);

    uint8_t seqNo = buffer[4U];

    uint32_t srcId = __GET_UINT16(buffer, 5U);
    uint32_t dstId = __GET_UINT16(buffer, 8U);

    uint8_t flco = (buffer[15U] & 0x40U) == 0x40U ? dmr::FLCO_PRIVATE : dmr::FLCO_GROUP;

    uint32_t slotNo = (buffer[15U] & 0x80U) == 0x80U ? 2U : 1U;

    // DMO mode slot disabling
    if (slotNo == 1U && !m_duplex)
        return false;

    // Individual slot disabling
    if (slotNo == 1U && !m_slot1)
        return false;
    if (slotNo == 2U && !m_slot2)
        return false;

    data.setSeqNo(seqNo);
    data.setSlotNo(slotNo);
    data.setSrcId(srcId);
    data.setDstId(dstId);
    data.setFLCO(flco);

    bool dataSync = (buffer[15U] & 0x20U) == 0x20U;
    bool voiceSync = (buffer[15U] & 0x10U) == 0x10U;

    if (m_debug) {
        LogDebug(LOG_NET, "DMR, seqNo = %u, srcId = %u, dstId = %u, flco = $%02X, slotNo = %u, len = %u", seqNo, srcId, dstId, flco, slotNo, length);
    }

    if (dataSync) {
        uint8_t dataType = buffer[15U] & 0x0FU;
        data.setData(buffer + 20U);
        data.setDataType(dataType);
        data.setN(0U);
    }
    else if (voiceSync) {
        data.setData(buffer + 20U);
        data.setDataType(dmr::DT_VOICE_SYNC);
        data.setN(0U);
    }
    else {
        uint8_t n = buffer[15U] & 0x0FU;
        data.setData(buffer + 20U);
        data.setDataType(dmr::DT_VOICE);
        data.setN(n);
    }

    return true;
}

/// <summary>
/// Writes DMR frame data to the network.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
bool BaseNetwork::writeDMR(const dmr::data::Data& data)
{
    uint32_t slotNo = data.getSlotNo();

    // individual slot disabling
    if (slotNo == 1U && !m_slot1)
        return false;
    if (slotNo == 2U && !m_slot2)
        return false;

    uint8_t dataType = data.getDataType();

    uint32_t slotIndex = slotNo - 1U;

    if (dataType == dmr::DT_VOICE_LC_HEADER) {
        m_dmrStreamId[slotIndex] = createStreamId();
    }

    if (dataType == dmr::DT_CSBK || dataType == dmr::DT_DATA_HEADER) {
        m_dmrStreamId[slotIndex] = createStreamId();
    }

    uint32_t messageLength = 0U;
    UInt8Array message = createDMR_Message(messageLength, m_dmrStreamId[slotIndex], data);
    if (message == nullptr) {
        return false;
    }

    m_frameQueue.enqueueMessage(message.get(), messageLength, m_dmrStreamId[slotIndex], m_peerId);
    return m_frameQueue.flushQueue(m_addr, m_addrLen);
}

/// <summary>
/// Reads P25 frame data from the P25 ring buffer.
/// </summary>
/// <param name="ret"></param>
/// <param name="frameLength"></param>
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <param name="duid"></param>
/// <param name="frameType"></param>
/// <returns></returns>
UInt8Array BaseNetwork::readP25(bool& ret, uint32_t& frameLength, p25::lc::LC& control, p25::data::LowSpeedData& lsd, 
    uint8_t& duid, uint8_t& frameType)
{
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

    uint8_t buffer[length];
    ::memset(buffer, 0x00U, length);
    m_rxP25Data.getData(buffer, length);

    uint8_t lco = buffer[4U];

    uint32_t srcId = __GET_UINT16(buffer, 5U);
    uint32_t dstId = __GET_UINT16(buffer, 8U);

    uint8_t MFId = buffer[15U];

    uint8_t lsd1 = buffer[20U];
    uint8_t lsd2 = buffer[21U];

    duid = buffer[22U];
    frameType = p25::P25_FT_DATA_UNIT;

    if (m_debug) {
        LogDebug(LOG_NET, "P25, duid = $%02X, lco = $%02X, MFId = $%02X, srcId = %u, dstId = %u, len = %u", duid, lco, MFId, srcId, dstId, length);
    }

    // is this a LDU1, is this the first of a call?
    if (duid == p25::P25_DUID_LDU1) {
        frameType = buffer[180U];

        if (m_debug) {
            LogDebug(LOG_NET, "P25, frameType = $%02X", frameType);
        }

        if (frameType == p25::P25_FT_HDU_VALID) {
            uint8_t algId = buffer[181U];
            uint32_t kid = (buffer[182U] << 8) | (buffer[183U] << 0);

            // copy MI data
            uint8_t mi[p25::P25_MI_LENGTH_BYTES];
            ::memset(mi, 0x00U, p25::P25_MI_LENGTH_BYTES);

            for (uint8_t i = 0; i < p25::P25_MI_LENGTH_BYTES; i++) {
                mi[i] = buffer[184U + i];
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
    control.setMFId(MFId);

    lsd.setLSD1(lsd1);
    lsd.setLSD2(lsd2);

    UInt8Array data;
    frameLength = buffer[23U];
    if (duid == p25::P25_DUID_PDU) {
        data = std::unique_ptr<uint8_t[]>(new uint8_t[length]);
        ::memset(data.get(), 0x00U, length);
        ::memcpy(data.get(), buffer, length);
    }
    else {
        if (frameLength <= 24) {
            data = std::unique_ptr<uint8_t[]>(new uint8_t[frameLength]);
            ::memset(data.get(), 0x00U, frameLength);
        }
        else {
            data = std::unique_ptr<uint8_t[]>(new uint8_t[frameLength]);
            ::memset(data.get(), 0x00U, frameLength);
            ::memcpy(data.get(), buffer + 24U, frameLength);
        }
    }

    ret = true;
    return data;
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
    if (m_p25StreamId == 0U)
        m_p25StreamId = createStreamId();

    uint32_t messageLength = 0U;
    UInt8Array message = createP25_LDU1Message(messageLength, control, lsd, data, frameType);
    if (message == nullptr) {
        return false;
    }

    m_frameQueue.enqueueMessage(message.get(), messageLength, m_p25StreamId, m_peerId);
    return m_frameQueue.flushQueue(m_addr, m_addrLen);
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
    if (m_p25StreamId == 0U)
        m_p25StreamId = createStreamId();

    uint32_t messageLength = 0U;
    UInt8Array message = createP25_LDU2Message(messageLength, control, lsd, data);
    if (message == nullptr) {
        return false;
    }

    m_frameQueue.enqueueMessage(message.get(), messageLength, m_p25StreamId, m_peerId);
    return m_frameQueue.flushQueue(m_addr, m_addrLen);
}

/// <summary>
/// Writes P25 TDU frame data to the network.
/// </summary>
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <returns></returns>
bool BaseNetwork::writeP25TDU(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd)
{
    if (m_p25StreamId == 0U)
        m_p25StreamId = createStreamId();

    uint32_t messageLength = 0U;
    UInt8Array message = createP25_TDUMessage(messageLength, control, lsd);
    if (message == nullptr) {
        return false;
    }

    m_frameQueue.enqueueMessage(message.get(), messageLength, m_p25StreamId, m_peerId);
    return m_frameQueue.flushQueue(m_addr, m_addrLen);
}

/// <summary>
/// Writes P25 TSDU frame data to the network.
/// </summary>
/// <param name="control"></param>
/// <param name="data"></param>
/// <returns></returns>
bool BaseNetwork::writeP25TSDU(const p25::lc::LC& control, const uint8_t* data)
{
    if (m_p25StreamId == 0U)
        m_p25StreamId = createStreamId();

    uint32_t messageLength = 0U;
    UInt8Array message = createP25_TSDUMessage(messageLength, control, data);
    if (message == nullptr) {
        return false;
    }

    m_frameQueue.enqueueMessage(message.get(), messageLength, m_p25StreamId, m_peerId);
    return m_frameQueue.flushQueue(m_addr, m_addrLen);
}

/// <summary>
/// Writes P25 PDU frame data to the network.
/// </summary>
/// <param name="header"></param>
/// <param name="secHeader"></param>
/// <param name="currentBlock"></param>
/// <param name="data"></param>
/// <param name="len"></param>
/// <returns></returns>
bool BaseNetwork::writeP25PDU(const p25::data::DataHeader& header, const p25::data::DataHeader& secHeader, const uint8_t currentBlock,
    const uint8_t* data, const uint32_t len)
{
    if (m_p25StreamId == 0U)
        m_p25StreamId = createStreamId();

    uint32_t messageLength = 0U;
    UInt8Array message = createP25_PDUMessage(messageLength, header, secHeader, currentBlock, data, len);
    if (message == nullptr) {
        return false;
    }

    m_frameQueue.enqueueMessage(message.get(), messageLength, m_p25StreamId, m_peerId);
    return m_frameQueue.flushQueue(m_addr, m_addrLen);
}

/// <summary>
/// Reads NXDN frame data from the NXDN ring buffer.
/// </summary>
/// <param name="ret"></param>
/// <param name="frameLength"></param>
/// <param name="lc"></param>
/// <returns></returns>
UInt8Array BaseNetwork::readNXDN(bool& ret, uint32_t& frameLength, nxdn::lc::RTCH& lc)
{
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

    uint8_t buffer[length];
    ::memset(buffer, 0x00U, length);
    m_rxNXDNData.getData(buffer, length);

    uint8_t messageType = buffer[4U];

    uint32_t srcId = __GET_UINT16(buffer, 5U);
    uint32_t dstId = __GET_UINT16(buffer, 8U);

    if (m_debug) {
        LogDebug(LOG_NET, "NXDN, messageType = $%02X, srcId = %u, dstId = %u, len = %u", messageType, srcId, dstId, length);
    }

    lc.setMessageType(messageType);
    lc.setSrcId((uint16_t)srcId & 0xFFFFU);
    lc.setDstId((uint16_t)dstId & 0xFFFFU);

    bool group = (buffer[15U] & 0x40U) == 0x40U ? false : true;
    lc.setGroup(group);

    UInt8Array data;
    frameLength = buffer[23U];
    if (frameLength <= 24) {
        data = std::unique_ptr<uint8_t[]>(new uint8_t[frameLength]);
        ::memset(data.get(), 0x00U, frameLength);
    }
    else {
        data = std::unique_ptr<uint8_t[]>(new uint8_t[frameLength]);
        ::memset(data.get(), 0x00U, frameLength);
        ::memcpy(data.get(), buffer + 24U, frameLength);
    }

    ret = true;
    return data;
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
    if (m_nxdnStreamId == 0U)
        m_nxdnStreamId = createStreamId();

    uint32_t messageLength = 0U;
    UInt8Array message = createNXDN_Message(messageLength, lc, data, len);
    if (message == nullptr) {
        return false;
    }

    m_frameQueue.enqueueMessage(message.get(), messageLength, m_nxdnStreamId, m_peerId);
    return m_frameQueue.flushQueue(m_addr, m_addrLen);
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
    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    ::memcpy(buffer + 0U, TAG_REPEATER_GRANT, 7U);
    __SET_UINT32(m_peerId, buffer, 7U);                                             // Peer ID

    __SET_UINT32(srcId, buffer, 11U);                                               // Source Address
    __SET_UINT32(dstId, buffer, 15U);                                               // Destination Address
    buffer[19U] = slot;                                                             // Slot Number

    if (unitToUnit)
        buffer[19U] |= 0x80U;

    buffer[20U] = mode;                                                             // DVM Mode State

    m_frameQueue.enqueueMessage(buffer, 21U, createStreamId(), m_peerId);
    return m_frameQueue.flushQueue(m_addr, m_addrLen);
}

/// <summary>
/// Writes the local activity log to the network.
/// </summary>
/// <param name="message"></param>
/// <returns></returns>
bool BaseNetwork::writeActLog(const char* message)
{
    if (!m_allowActivityTransfer)
        return false;

    assert(message != nullptr);

    char buffer[DATA_PACKET_LENGTH];
    uint32_t len = ::strlen(message);

    ::memcpy(buffer + 0U, TAG_TRANSFER_ACT_LOG, 7U);
    __SET_UINT32(m_peerId, buffer, 7U);                                             // Peer ID
    ::strcpy(buffer + 11U, message);

    m_frameQueue.enqueueMessage((uint8_t*)buffer, (uint32_t)len + 12U, createStreamId(), m_peerId);
    return m_frameQueue.flushQueue(m_addr, m_addrLen);
}

/// <summary>
/// Writes the local diagnostics log to the network.
/// </summary>
/// <param name="message"></param>
/// <returns></returns>
bool BaseNetwork::writeDiagLog(const char* message)
{
    if (!m_allowDiagnosticTransfer)
        return false;

    assert(message != nullptr);

    char buffer[DATA_PACKET_LENGTH];
    uint32_t len = ::strlen(message);

    ::memcpy(buffer + 0U, TAG_TRANSFER_DIAG_LOG, 8U);
    __SET_UINT32(m_peerId, buffer, 8U);                                             // Peer ID
    ::strcpy(buffer + 12U, message);

    m_frameQueue.enqueueMessage((uint8_t*)buffer, (uint32_t)len + 13U, createStreamId(), m_peerId);
    return m_frameQueue.flushQueue(m_addr, m_addrLen);
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

    m_rxDMRData.clear();
}

/// <summary>
/// Resets the P25 ring buffer.
/// </summary>
void BaseNetwork::resetP25()
{
    m_p25StreamId = createStreamId();
    m_rxP25Data.clear();
}

/// <summary>
/// Resets the NXDN ring buffer.
/// </summary>
void BaseNetwork::resetNXDN()
{
    m_nxdnStreamId = createStreamId();
    m_rxNXDNData.clear();
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Creates an DMR frame message.
/// </summary>
/// <param name="length"></param>
/// <param name="streamId"></param>
/// <param name="data"></param>
/// <returns></returns>
UInt8Array BaseNetwork::createDMR_Message(uint32_t& length, const uint32_t streamId, const dmr::data::Data& data)
{
    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    ::memcpy(buffer + 0U, TAG_DMR_DATA, 4U);

    uint32_t srcId = data.getSrcId();                                               // Source Address
    __SET_UINT16(srcId, buffer, 5U);

    uint32_t dstId = data.getDstId();                                               // Target Address
    __SET_UINT16(dstId, buffer, 8U);

    __SET_UINT32(m_peerId, buffer, 11U);                                            // Peer ID

    uint32_t slotNo = data.getSlotNo();

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

    __SET_UINT32(streamId, buffer, 16U);                                            // Stream ID

    data.getData(buffer + 20U);

    buffer[53U] = data.getBER();                                                    // Bit Error Rate
    buffer[54U] = data.getRSSI();                                                   // RSSI

    if (m_debug)
        Utils::dump(1U, "Network Message, DMR", buffer, (DMR_PACKET_SIZE + PACKET_PAD));

    length = (DMR_PACKET_SIZE + PACKET_PAD);
    return UInt8Array(buffer);
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

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    ::memcpy(buffer + 0U, TAG_P25_DATA, 4U);

    buffer[4U] = control.getLCO();                                                  // LCO

    uint32_t srcId = control.getSrcId();                                            // Source Address
    __SET_UINT16(srcId, buffer, 5U);

    uint32_t dstId = control.getDstId();                                            // Target Address
    __SET_UINT16(dstId, buffer, 8U);

    __SET_UINT32(m_peerId, buffer, 11U);                                            // Peer ID

    buffer[15U] = control.getMFId();                                                // MFId

    __SET_UINT32(m_p25StreamId, buffer, 16U);                                       // Stream ID

    buffer[20U] = lsd.getLSD1();                                                    // LSD 1
    buffer[21U] = lsd.getLSD2();                                                    // LSD 2

    buffer[22U] = p25::P25_DUID_LDU1;                                               // DUID

    buffer[180U] = frameType;                                                       // DVM Frame Type

    // is this the first frame of a call?
    if (frameType == p25::P25_FT_HDU_VALID) {
        buffer[180U] = 0x01U;                                                       // First LDU1 Marker
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

    uint32_t count = 24U;
    uint8_t imbe[p25::P25_RAW_IMBE_LENGTH_BYTES];

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE1);
    m_audio.decode(data, imbe, 0U);
    dfsiLC.encodeLDU1(buffer + 24U, imbe);
    count += 22U;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE2);
    m_audio.decode(data, imbe, 1U);
    dfsiLC.encodeLDU1(buffer + 46U, imbe);
    count += 14U;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE3);
    m_audio.decode(data, imbe, 2U);
    dfsiLC.encodeLDU1(buffer + 60U, imbe);
    count += 17U;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE4);
    m_audio.decode(data, imbe, 3U);
    dfsiLC.encodeLDU1(buffer + 77U, imbe);
    count += 17U;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE5);
    m_audio.decode(data, imbe, 4U);
    dfsiLC.encodeLDU1(buffer + 94U, imbe);
    count += 17U;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE6);
    m_audio.decode(data, imbe, 5U);
    dfsiLC.encodeLDU1(buffer + 111U, imbe);
    count += 17U;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE7);
    m_audio.decode(data, imbe, 6U);
    dfsiLC.encodeLDU1(buffer + 128U, imbe);
    count += 17U;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE8);
    m_audio.decode(data, imbe, 7U);
    dfsiLC.encodeLDU1(buffer + 145U, imbe);
    count += 17U;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE9);
    m_audio.decode(data, imbe, 8U);
    dfsiLC.encodeLDU1(buffer + 162U, imbe);
    count += 16U;

    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "Network Message, P25 LDU1", buffer, (count + 15U + PACKET_PAD));

    length = (count + 15U + PACKET_PAD);
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

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    ::memcpy(buffer + 0U, TAG_P25_DATA, 4U);

    buffer[4U] = control.getLCO();                                                  // LCO

    uint32_t srcId = control.getSrcId();                                            // Source Address
    __SET_UINT16(srcId, buffer, 5U);

    uint32_t dstId = control.getDstId();                                            // Target Address
    __SET_UINT16(dstId, buffer, 8U);

    __SET_UINT32(m_peerId, buffer, 11U);                                            // Peer ID

    buffer[15U] = control.getMFId();                                                // MFId

    __SET_UINT32(m_p25StreamId, buffer, 16U);                                       // Stream ID

    buffer[20U] = lsd.getLSD1();                                                    // LSD 1
    buffer[21U] = lsd.getLSD2();                                                    // LSD 2

    buffer[22U] = p25::P25_DUID_LDU2;                                               // DUID

    uint32_t count = 24U;
    uint8_t imbe[p25::P25_RAW_IMBE_LENGTH_BYTES];

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE10);
    m_audio.decode(data, imbe, 0U);
    dfsiLC.encodeLDU2(buffer + 24U, imbe);
    count += 22U;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE11);
    m_audio.decode(data, imbe, 1U);
    dfsiLC.encodeLDU2(buffer + 46U, imbe);
    count += 14U;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE12);
    m_audio.decode(data, imbe, 2U);
    dfsiLC.encodeLDU2(buffer + 60U, imbe);
    count += 17U;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE13);
    m_audio.decode(data, imbe, 3U);
    dfsiLC.encodeLDU2(buffer + 77U, imbe);
    count += 17U;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE14);
    m_audio.decode(data, imbe, 4U);
    dfsiLC.encodeLDU2(buffer + 94U, imbe);
    count += 17U;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE15);
    m_audio.decode(data, imbe, 5U);
    dfsiLC.encodeLDU2(buffer + 111U, imbe);
    count += 17U;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE16);
    m_audio.decode(data, imbe, 6U);
    dfsiLC.encodeLDU2(buffer + 128U, imbe);
    count += 17U;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE17);
    m_audio.decode(data, imbe, 7U);
    dfsiLC.encodeLDU2(buffer + 145U, imbe);
    count += 17U;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE18);
    m_audio.decode(data, imbe, 8U);
    dfsiLC.encodeLDU2(buffer + 162U, imbe);
    count += 16U;

    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "Network Message, P25 LDU2", buffer, (count + PACKET_PAD));

    length = (count + PACKET_PAD);
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
    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    ::memcpy(buffer + 0U, TAG_P25_DATA, 4U);

    buffer[4U] = control.getLCO();                                                  // LCO

    uint32_t srcId = control.getSrcId();                                            // Source Address
    __SET_UINT16(srcId, buffer, 5U);

    uint32_t dstId = control.getDstId();                                            // Target Address
    __SET_UINT16(dstId, buffer, 8U);

    __SET_UINT32(m_peerId, buffer, 11U);                                            // Peer ID

    buffer[15U] = control.getMFId();                                                // MFId

    __SET_UINT32(m_p25StreamId, buffer, 16U);                                       // Stream ID

    buffer[20U] = lsd.getLSD1();                                                    // LSD 1
    buffer[21U] = lsd.getLSD2();                                                    // LSD 2

    buffer[22U] = p25::P25_DUID_TDU;                                                // DUID

    uint32_t count = 24U;
    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "Network Message, P25 TDU", buffer, (count + PACKET_PAD));

    length = (count + PACKET_PAD);
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

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    ::memcpy(buffer + 0U, TAG_P25_DATA, 4U);

    buffer[4U] = control.getLCO();                                                  // LCO

    uint32_t srcId = control.getSrcId();                                            // Source Address
    __SET_UINT16(srcId, buffer, 5U);

    uint32_t dstId = control.getDstId();                                            // Target Address
    __SET_UINT16(dstId, buffer, 8U);

    __SET_UINT32(m_peerId, buffer, 11U);                                            // Peer ID

    buffer[15U] = control.getMFId();                                                // MFId

    __SET_UINT32(m_p25StreamId, buffer, 16U);                                       // Stream ID

    buffer[20U] = 0U;                                                               // Reserved (LSD 1)
    buffer[21U] = 0U;                                                               // Reserved (LSD 2)

    buffer[22U] = p25::P25_DUID_TSDU;                                               // DUID

    uint32_t count = 24U;

    ::memcpy(buffer + 24U, data, p25::P25_TSDU_FRAME_LENGTH_BYTES);
    count += p25::P25_TSDU_FRAME_LENGTH_BYTES;

    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "Network Message, P25 TDSU", buffer, (count + PACKET_PAD));

    length = (count + PACKET_PAD);
    return UInt8Array(buffer);
}

/// <summary>
/// Writes P25 PDU frame data to the network.
/// </summary>
/// <param name="length"></param>
/// <param name="header"></param>
/// <param name="secHeader"></param>
/// <param name="currentBlock"></param>
/// <param name="data"></param>
/// <param name="len"></param>
/// <returns></returns>
UInt8Array BaseNetwork::createP25_PDUMessage(uint32_t& length, const p25::data::DataHeader& header, const p25::data::DataHeader& secHeader,
    const uint8_t currentBlock, const uint8_t* data, const uint32_t len)
{
    bool useSecondHeader = false;

    // process second header if we're using enhanced addressing
    if (header.getSAP() == p25::PDU_SAP_EXT_ADDR &&
        header.getFormat() == p25::PDU_FMT_UNCONFIRMED) {
        useSecondHeader = true;
    }

    assert(data != nullptr);

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    ::memcpy(buffer + 0U, TAG_P25_DATA, 4U);

    buffer[4U] = header.getSAP();                                                   // Service Access Point
    if (header.getFormat() == p25::PDU_FMT_CONFIRMED) {
        buffer[4U] |= 0x80U;
    }

    uint32_t llId = (useSecondHeader) ? secHeader.getLLId() : header.getLLId();
    __SET_UINT16(llId, buffer, 5U);                                                 // Logical Link Address

    __SET_UINT16(len, buffer, 8U);                                                  // PDU Length [bytes]

    __SET_UINT32(m_peerId, buffer, 11U);                                            // Peer ID

    buffer[15U] = header.getMFId();                                                 // MFId

    __SET_UINT32(m_p25StreamId, buffer, 16U);                                       // Stream ID

    buffer[20U] = header.getBlocksToFollow();                                       // Blocks To Follow
    buffer[21U] = currentBlock;                                                     // Current Block

    buffer[22U] = p25::P25_DUID_PDU;                                                // DUID

    uint32_t count = 24U;

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

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    ::memcpy(buffer + 0U, TAG_NXDN_DATA, 4U);

    buffer[4U] = lc.getMessageType();                                           // Message Type

    uint32_t srcId = lc.getSrcId();                                             // Source Address
    __SET_UINT16(srcId, buffer, 5U);

    uint32_t dstId = lc.getDstId();                                             // Target Address
    __SET_UINT16(dstId, buffer, 8U);

    __SET_UINT32(m_peerId, buffer, 11U);                                        // Peer ID

    buffer[15U] |= lc.getGroup() ? 0x00U : 0x40U;                               // Group

    __SET_UINT32(m_nxdnStreamId, buffer, 16U);                                  // Stream ID

    uint32_t count = 24U;

    ::memcpy(buffer + 24U, data, len);
    count += len;

    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "Network Message, NXDN", buffer, (count + PACKET_PAD));

    length = (count + PACKET_PAD);
    return UInt8Array(buffer);
}
