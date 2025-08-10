// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
 *  Copyright (C) 2020-2025 Bryan Biedenkapp, N2PLL
 *  Copyright (C) 2024 Caleb, KO4UYJ
 *
 */
#include "Defines.h"
#include "common/analog/AnalogDefines.h"
#include "common/dmr/DMRDefines.h"
#include "common/p25/P25Defines.h"
#include "common/nxdn/NXDNDefines.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/p25/dfsi/LC.h"
#include "common/p25/kmm/KMMModifyKey.h"
#include "network/BaseNetwork.h"
#include "Utils.h"

using namespace network;
using namespace network::frame;

#include <cassert>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define NET_RING_BUF_SIZE 4098U

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the BaseNetwork class. */

BaseNetwork::BaseNetwork(uint32_t peerId, bool duplex, bool debug, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, uint16_t localPort) :
    m_peerId(peerId),
    m_status(NET_STAT_INVALID),
    m_addr(),
    m_addrLen(0U),
    m_slot1(slot1),
    m_slot2(slot2),
    m_duplex(duplex),
    m_useAlternatePortForDiagnostics(false),
    m_allowActivityTransfer(allowActivityTransfer),
    m_allowDiagnosticTransfer(allowDiagnosticTransfer),
    m_debug(debug),
    m_socket(nullptr),
    m_frameQueue(nullptr),
    m_rxDMRData(NET_RING_BUF_SIZE, "DMR Net Buffer"),
    m_rxP25Data(NET_RING_BUF_SIZE, "P25 Net Buffer"),
    m_rxNXDNData(NET_RING_BUF_SIZE, "NXDN Net Buffer"),
    m_rxAnalogData(NET_RING_BUF_SIZE, "Analog Net Buffer"),
    m_random(),
    m_dmrStreamId(nullptr),
    m_p25StreamId(0U),
    m_nxdnStreamId(0U),
    m_analogStreamId(0U),
    m_pktSeq(0U),
    m_audio()
{
    assert(peerId < 999999999U);

    m_socket = new udp::Socket(localPort);
    m_frameQueue = new FrameQueue(m_socket, peerId, debug);

    std::random_device rd;
    std::mt19937 mt(rd());
    m_random = mt;

    m_dmrStreamId = new uint32_t[2U];
    m_dmrStreamId[0U] = createStreamId();
    m_dmrStreamId[1U] = createStreamId();
    m_p25StreamId = createStreamId();
    m_nxdnStreamId = createStreamId();
    m_analogStreamId = createStreamId();
}

/* Finalizes a instance of the BaseNetwork class. */

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

/* Writes grant request to the network. */

bool BaseNetwork::writeGrantReq(const uint8_t mode, const uint32_t srcId, const uint32_t dstId, const uint8_t slot, const bool unitToUnit)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    uint8_t buffer[MSG_HDR_SIZE];
    ::memset(buffer, 0x00U, MSG_HDR_SIZE);

    SET_UINT32(srcId, buffer, 11U);                                                 // Source Address
    SET_UINT32(dstId, buffer, 15U);                                                 // Destination Address
    buffer[19U] = slot;                                                             // Slot Number

    if (unitToUnit)
        buffer[19U] |= 0x80U;

    buffer[20U] = mode;                                                             // DVM Mode State

    return writeMaster({ NET_FUNC::GRANT_REQ, NET_SUBFUNC::NOP }, buffer, MSG_HDR_SIZE, RTP_END_OF_CALL_SEQ, 0U);
}

/* Writes enc. key request to the network. */

bool BaseNetwork::writeKeyReq(const uint16_t kId, const uint8_t algId)
{
    using namespace p25::defines;
    using namespace p25::kmm;

    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    KMMModifyKey modifyKeyCmd = KMMModifyKey();
    modifyKeyCmd.setDecryptInfoFmt(KMM_DECRYPT_INSTRUCT_NONE);
    modifyKeyCmd.setAlgId(algId);
    modifyKeyCmd.setKId(kId);

    KeysetItem ks = KeysetItem();
    ks.keysetId(0U);
    ks.algId(algId);
    ks.keyLength(P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
    modifyKeyCmd.setKeysetItem(ks);

    modifyKeyCmd.encode(buffer + 11U);

    //Utils::dump("BaseNetwork::writeKeyReq(), KMM Buffer", buffer, modifyKeyCmd.length() + 11U);

    return writeMaster({ NET_FUNC::KEY_REQ, NET_SUBFUNC::NOP }, buffer, modifyKeyCmd.length() + 11U, RTP_END_OF_CALL_SEQ, 0U);
}

/* Writes the local activity log to the network. */

bool BaseNetwork::writeActLog(const char* message)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    if (!m_allowActivityTransfer)
        return false;

    assert(message != nullptr);

    char buffer[DATA_PACKET_LENGTH];
    uint32_t len = ::strlen(message);

    ::strncpy(buffer + 11U, message, len);

    return writeMaster({ NET_FUNC::TRANSFER, NET_SUBFUNC::TRANSFER_SUBFUNC_ACTIVITY }, (uint8_t*)buffer, (uint32_t)len + 11U,
        RTP_END_OF_CALL_SEQ, 0U, false, m_useAlternatePortForDiagnostics);
}

/* Writes the local diagnostics log to the network. */

bool BaseNetwork::writeDiagLog(const char* message)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    if (!m_allowDiagnosticTransfer)
        return false;

    assert(message != nullptr);

    char buffer[DATA_PACKET_LENGTH];
    uint32_t len = ::strlen(message);

    ::strncpy(buffer + 11U, message, len);

    return writeMaster({ NET_FUNC::TRANSFER, NET_SUBFUNC::TRANSFER_SUBFUNC_DIAG }, (uint8_t*)buffer, (uint32_t)len + 11U,
        RTP_END_OF_CALL_SEQ, 0U, false, m_useAlternatePortForDiagnostics);
}

/* Writes the local status to the network. */

bool BaseNetwork::writePeerStatus(json::object obj)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    if (!m_allowActivityTransfer)
        return false;
    if (!m_useAlternatePortForDiagnostics)
        return false; // this is intentional -- peer status is a noisy message and it shouldn't be done
                      // when the FNE is configured for main port transfers

    json::value v = json::value(obj);
    std::string json = std::string(v.serialize());

    char buffer[DATA_PACKET_LENGTH];
    uint32_t len = ::strlen(json.c_str());

    ::strncpy(buffer + 11U, json.c_str(), len);

    return writeMaster({ NET_FUNC::TRANSFER, NET_SUBFUNC::TRANSFER_SUBFUNC_STATUS }, (uint8_t*)buffer, (uint32_t)len + 11U,
        RTP_END_OF_CALL_SEQ, 0U, false, m_useAlternatePortForDiagnostics);
}

/* Writes a group affiliation to the network. */

bool BaseNetwork::announceGroupAffiliation(uint32_t srcId, uint32_t dstId)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    uint8_t buffer[DATA_PACKET_LENGTH];

    SET_UINT24(srcId, buffer, 0U);
    SET_UINT24(dstId, buffer, 3U);

    return writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_GRP_AFFIL }, buffer, MSG_ANNC_GRP_AFFIL, RTP_END_OF_CALL_SEQ, 0U);
}

/* Writes a group affiliation removal to the network. */

bool BaseNetwork::announceGroupAffiliationRemoval(uint32_t srcId)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    uint8_t buffer[DATA_PACKET_LENGTH];

    SET_UINT24(srcId, buffer, 0U);

    return writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_GRP_UNAFFIL }, buffer, MSG_ANNC_GRP_UNAFFIL, RTP_END_OF_CALL_SEQ, 0U);
}

/* Writes a unit registration to the network. */

bool BaseNetwork::announceUnitRegistration(uint32_t srcId)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    uint8_t buffer[DATA_PACKET_LENGTH];

    SET_UINT24(srcId, buffer, 0U);

    return writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_UNIT_REG }, buffer, MSG_ANNC_UNIT_REG, RTP_END_OF_CALL_SEQ, 0U);
}

/* Writes a unit deregistration to the network. */

bool BaseNetwork::announceUnitDeregistration(uint32_t srcId)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    uint8_t buffer[DATA_PACKET_LENGTH];

    SET_UINT24(srcId, buffer, 0U);

    return writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_UNIT_DEREG }, buffer, MSG_ANNC_UNIT_REG, RTP_END_OF_CALL_SEQ, 0U);
}

/* Writes a complete update of the peer affiliation list to the network. */

bool BaseNetwork::announceAffiliationUpdate(const std::unordered_map<uint32_t, uint32_t> affs)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    DECLARE_UINT8_ARRAY(buffer, 4U + (affs.size() * 8U));

    SET_UINT32(affs.size(), buffer, 0U);

    // write talkgroup IDs to active TGID payload
    uint32_t offs = 4U;
    for (auto it : affs) {
        SET_UINT24(it.first, buffer, offs);
        SET_UINT24(it.second, buffer, offs + 4U);
        offs += 8U;
    }

    return writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_AFFILS }, buffer, 4U + (affs.size() * 8U), RTP_END_OF_CALL_SEQ, 0U);
}

/* Writes a complete update of the peer's voice channel list to the network. */

bool BaseNetwork::announceSiteVCs(const std::vector<uint32_t> peers)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    DECLARE_UINT8_ARRAY(buffer, 4U + (peers.size() * 4U));

    SET_UINT32(peers.size(), buffer, 0U);

    // write peer IDs to active TGID payload
    uint32_t offs = 4U;
    for (auto it : peers) {
        SET_UINT32(it, buffer, offs);
        offs += 4U;
    }

    return writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_SITE_VC }, buffer, 4U + (peers.size() * 4U), RTP_END_OF_CALL_SEQ, 0U);
}

/* Resets the DMR ring buffer for the given slot. */

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

/* Resets the P25 ring buffer. */

void BaseNetwork::resetP25()
{
    m_p25StreamId = createStreamId();
    m_pktSeq = 0U;
    m_rxP25Data.clear();
}

/* Resets the NXDN ring buffer. */

void BaseNetwork::resetNXDN()
{
    m_nxdnStreamId = createStreamId();
    m_pktSeq = 0U;
    m_rxNXDNData.clear();
}

/* Resets the analog ring buffer. */

void BaseNetwork::resetAnalog()
{
    m_analogStreamId = createStreamId();
    m_pktSeq = 0U;
    m_rxAnalogData.clear();
}

/* Gets the current DMR stream ID. */

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

/* Helper to send a data message to the master. */

bool BaseNetwork::writeMaster(FrameQueue::OpcodePair opcode, const uint8_t* data, uint32_t length, uint16_t pktSeq, uint32_t streamId, 
    bool queueOnly, bool useAlternatePort, uint32_t peerId, uint32_t ssrc)
{
    if (peerId == 0U)
        peerId = m_peerId;
    if (ssrc == 0U)
        ssrc = m_peerId;

    if (useAlternatePort) {
        sockaddr_storage addr;
        uint32_t addrLen;

        std::string address = udp::Socket::address(m_addr);
        uint16_t port = udp::Socket::port(m_addr) + 1U;

        if (udp::Socket::lookup(address, port, addr, addrLen) == 0) {
            if (!queueOnly)
                return m_frameQueue->write(data, length, streamId, peerId, ssrc, opcode, pktSeq, addr, addrLen);
            else
                m_frameQueue->enqueueMessage(data, length, streamId, m_peerId, opcode, pktSeq, addr, addrLen);
        }
    }
    else {
        if (!queueOnly)
            return m_frameQueue->write(data, length, streamId, peerId, ssrc, opcode, pktSeq, m_addr, m_addrLen);
        else
            m_frameQueue->enqueueMessage(data, length, streamId, m_peerId, opcode, pktSeq, m_addr, m_addrLen);
    }

    return true;
}

/* Reads DMR raw frame data from the DMR ring buffer. */

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
    m_rxDMRData.get(&length, 1U);
    if (length == 0U) {
        ret = false;
        return nullptr;
    }

    UInt8Array buffer;
    frameLength = length;
    buffer = std::unique_ptr<uint8_t[]>(new uint8_t[length]);
    ::memset(buffer.get(), 0x00U, length);
    m_rxDMRData.get(buffer.get(), length);

    return buffer;
}

/* Writes DMR frame data to the network. */

bool BaseNetwork::writeDMR(const dmr::data::NetData& data, bool noSequence)
{
    using namespace dmr::defines;
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    uint32_t slotNo = data.getSlotNo();

    // individual slot disabling
    if (slotNo == 1U && !m_slot1)
        return false;
    if (slotNo == 2U && !m_slot2)
        return false;

    DataType::E dataType = data.getDataType();

    uint32_t slotIndex = slotNo - 1U;

    bool resetSeq = false;
    if (dataType == DataType::VOICE_LC_HEADER) {
        resetSeq = true;
        m_dmrStreamId[slotIndex] = createStreamId();
    }

    if (dataType == DataType::CSBK || dataType == DataType::DATA_HEADER) {
        resetSeq = true;
        m_dmrStreamId[slotIndex] = createStreamId();
    }

    uint32_t messageLength = 0U;
    UInt8Array message = createDMR_Message(messageLength, m_dmrStreamId[slotIndex], data);
    if (message == nullptr) {
        return false;
    }

    uint16_t seq = pktSeq(resetSeq);
    if (dataType == DataType::TERMINATOR_WITH_LC) {
        seq = RTP_END_OF_CALL_SEQ;
    }

    if (noSequence) {
        seq = RTP_END_OF_CALL_SEQ;
    }

    return writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR }, message.get(), messageLength, seq, m_dmrStreamId[slotIndex]);
}

/* Helper to test if the DMR ring buffer has data. */

bool BaseNetwork::hasDMRData() const
{
    if (m_rxDMRData.isEmpty())
        return false;

    return true;
}

/* Reads P25 raw frame data from the P25 ring buffer. */

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
    m_rxP25Data.get(&length, 1U);
    if (length == 0U) {
        ret = false;
        return nullptr;
    }

    if (length == 254U) {
        m_rxP25Data.get(&length, 1U); // read the next byte for the actual length
        length += 254U; // a packet length of 254 is a special case for P25 frames, so we need to add the 254 to the length
    }

    UInt8Array buffer;
    frameLength = length;
    buffer = std::unique_ptr<uint8_t[]>(new uint8_t[length]);
    ::memset(buffer.get(), 0x00U, length);
    m_rxP25Data.get(buffer.get(), length);

    return buffer;
}

/* Writes P25 LDU1 frame data to the network. */

bool BaseNetwork::writeP25LDU1(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t* data, 
    P25DEF::FrameType::E frameType, uint8_t controlByte)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    bool resetSeq = false;
    if (m_p25StreamId == 0U) {
        resetSeq = true;
        m_p25StreamId = createStreamId();
    }

    uint32_t messageLength = 0U;
    UInt8Array message = createP25_LDU1Message(messageLength, control, lsd, data, frameType, controlByte);
    if (message == nullptr) {
        return false;
    }

    return writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, pktSeq(resetSeq), m_p25StreamId);
}

/* Writes P25 LDU2 frame data to the network. */

bool BaseNetwork::writeP25LDU2(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t* data,
    uint8_t controlByte)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    bool resetSeq = false;
    if (m_p25StreamId == 0U) {
        resetSeq = true;
        m_p25StreamId = createStreamId();
    }

    uint32_t messageLength = 0U;
    UInt8Array message = createP25_LDU2Message(messageLength, control, lsd, data, controlByte);
    if (message == nullptr) {
        return false;
    }

    return writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, pktSeq(resetSeq), m_p25StreamId);
}

/* Writes P25 TDU frame data to the network. */

bool BaseNetwork::writeP25TDU(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t controlByte)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    if (m_p25StreamId == 0U) {
        m_p25StreamId = createStreamId();
    }

    uint32_t messageLength = 0U;
    UInt8Array message = createP25_TDUMessage(messageLength, control, lsd, controlByte);
    if (message == nullptr) {
        return false;
    }

    return writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, RTP_END_OF_CALL_SEQ, m_p25StreamId);
}

/* Writes P25 TSDU frame data to the network. */

bool BaseNetwork::writeP25TSDU(const p25::lc::LC& control, const uint8_t* data)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    if (m_p25StreamId == 0U) {
        m_p25StreamId = createStreamId();
    }

    uint32_t messageLength = 0U;
    UInt8Array message = createP25_TSDUMessage(messageLength, control, data);
    if (message == nullptr) {
        return false;
    }

    return writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, RTP_END_OF_CALL_SEQ, m_p25StreamId);
}

/* Writes P25 TDULC frame data to the network. */

bool BaseNetwork::writeP25TDULC(const p25::lc::LC& control, const uint8_t* data)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    if (m_p25StreamId == 0U) {
        m_p25StreamId = createStreamId();
    }

    uint32_t messageLength = 0U;
    UInt8Array message = createP25_TDULCMessage(messageLength, control, data);
    if (message == nullptr) {
        return false;
    }

    return writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, RTP_END_OF_CALL_SEQ, m_p25StreamId);
}

/* Writes P25 PDU frame data to the network. */

bool BaseNetwork::writeP25PDU(const p25::data::DataHeader& header, const uint8_t currentBlock, const uint8_t* data,
    const uint32_t len, bool lastBlock)
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

    uint16_t seq = pktSeq(resetSeq);
    if (lastBlock) {
        seq = RTP_END_OF_CALL_SEQ;
    }

    return writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, seq, m_p25StreamId);
}

/* Helper to test if the P25 ring buffer has data. */

bool BaseNetwork::hasP25Data() const
{
    if (m_rxP25Data.isEmpty())
        return false;

    return true;
}

/* Helper to validate a P25 network frame length. */

bool BaseNetwork::validateP25FrameLength(uint8_t& frameLength, uint32_t len, const P25DEF::DUID::E duid)
{
    using namespace p25::defines;

    // P25 network frame should never be less then 24 bytes
    if (len < 24U) {
        LogError(LOG_NET, "malformed P25 packet, len < 24, shouldn't happen");
        return false;
    }

    // frame length should never be 0
    if (frameLength == 0U) {
        LogError(LOG_NET, "DUID $%02X, sent with frame length of 0?", duid);
        return false;
    }

    // frame length should never be larger then the network packet length
    if (frameLength > len) {
        LogError(LOG_NET, "malformed P25 packet, frameLength > len (%u > %u), shouldn't happen", frameLength, len);
        return false;
    }

    // validate frame length, because P25 has variable network frame lengths we should be validating
    // the actual frame length to ensure we don't have buffer overflow vulnerabilities
    switch (duid) {
    case DUID::HDU:
        // HDU's aren't actually ever sent over the network, they are packaged with the first LDU1 for the
        // initating superframe
        return false;
    case DUID::TDU:
        // TDUs are sent with the P25 message header only
        if (frameLength != 24U) { 
            LogError(LOG_NET, P25_TDU_STR ", malformed TDU, discard.");
            break;
        }
        break;

    case DUID::LDU1:
        // LDU1 with message header only, this shouldn't happen
        if (frameLength <= 24U) { 
            LogError(LOG_NET, P25_LDU1_STR ", malformed LDU1, discard.");
            break;
        }
        break;
    case DUID::VSELP1:
        // VSELP1 frames aren't actually sent over the network right now
        return false;

    case DUID::TSDU:
        // oversized TSDU -- this shouldn't happen, truncate and only handle the size of the TSDU frame length
        if (frameLength > P25_TSDU_FRAME_LENGTH_BYTES)
            frameLength = P25_TSDU_FRAME_LENGTH_BYTES;

        // TSDU with message header only, this shouldn't happen
        if (frameLength <= 24U) { 
            LogError(LOG_NET, P25_TSDU_STR ", malformed TSDU, discard.");
            break;
        }
        break;

    case DUID::LDU2:
        // LDU2 with message header only, this shouldn't happen
        if (frameLength <= 24U) { 
            LogError(LOG_NET, P25_LDU2_STR ", malformed LDU2, discard.");
            break;
        }
        break;
    case DUID::VSELP2:
        // VSELP2 frames aren't actually sent over the network right now
        return false;

    case DUID::PDU:
        // PDU with message header only, this shouldn't happen
        if (frameLength <= 24U) { 
            LogError(LOG_NET, P25_PDU_STR ", malformed PDU, discard.");
            break;
        }
        break;
        

    case DUID::TDULC:
        // oversized TDULC -- this shouldn't happen, truncate and only handle the size of the TSDU frame length
        if (frameLength > P25_TDULC_FRAME_LENGTH_BYTES)
            frameLength = P25_TDULC_FRAME_LENGTH_BYTES;

        // TDULC with message header only, this shouldn't happen
        if (frameLength <= 24U) { 
            LogError(LOG_NET, P25_TSDU_STR ", malformed TDULC, discard.");
            break;
        }
        break;

    default:
        LogError(LOG_NET, "unsupported DUID $%02X", duid);
        return false;
    }

    return true;
}

/* Reads NXDN raw frame data from the NXDN ring buffer. */

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
    m_rxNXDNData.get(&length, 1U);
    if (length == 0U) {
        ret = false;
        return nullptr;
    }

    UInt8Array buffer;
    frameLength = length;
    buffer = std::unique_ptr<uint8_t[]>(new uint8_t[length]);
    ::memset(buffer.get(), 0x00U, length);
    m_rxNXDNData.get(buffer.get(), length);

    return buffer;
}

/* Writes NXDN frame data to the network. */

bool BaseNetwork::writeNXDN(const nxdn::lc::RTCH& lc, const uint8_t* data, const uint32_t len, bool noSequence)
{
    using namespace nxdn::defines;
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

    uint16_t seq = pktSeq(resetSeq);
    if (lc.getMessageType() == MessageType::RTCH_TX_REL ||
        lc.getMessageType() == MessageType::RTCH_TX_REL_EX) {
        seq = RTP_END_OF_CALL_SEQ;
    }

    if (noSequence) {
        seq = RTP_END_OF_CALL_SEQ;
    }

    return writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN }, message.get(), messageLength, seq, m_nxdnStreamId);
}

/* Helper to test if the NXDN ring buffer has data. */

bool BaseNetwork::hasNXDNData() const
{
    if (m_rxNXDNData.isEmpty())
        return false;

    return true;
}

/* Reads analog raw frame data from the analog ring buffer. */

UInt8Array BaseNetwork::readAnalog(bool& ret, uint32_t& frameLength)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return nullptr;

    ret = true;
    if (m_rxAnalogData.isEmpty()) {
        ret = false;
        return nullptr;
    }

    uint8_t length = 0U;
    m_rxAnalogData.get(&length, 1U);
    if (length == 0U) {
        ret = false;
        return nullptr;
    }

    if (length < 254U) {
        // if the length is less than 254, the analog packet is malformed, analog packets should never be less than 254 bytes
        LogError(LOG_NET, "malformed analog packet, length < 254 (%u), shouldn't happen", length);
        ret = false;
        return nullptr;
    }

    if (length == 254U) {
        m_rxAnalogData.get(&length, 1U); // read the next byte for the actual length
        length += 254U; // a packet length of 254 is a special case for P25 frames, so we need to add the 254 to the length
    }

    UInt8Array buffer;
    frameLength = length;
    buffer = std::unique_ptr<uint8_t[]>(new uint8_t[length]);
    ::memset(buffer.get(), 0x00U, length);
    m_rxAnalogData.get(buffer.get(), length);

    return buffer;
}

/* Writes analog frame data to the network. */

bool BaseNetwork::writeAnalog(const analog::data::NetData& data, bool noSequence)
{
    using namespace analog::defines;
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    AudioFrameType::E frameType = data.getFrameType();

    bool resetSeq = false;
    if (m_analogStreamId == 0U) {
        resetSeq = true;
        m_analogStreamId = createStreamId();
    }

    uint32_t messageLength = 0U;
    UInt8Array message = createAnalog_Message(messageLength, m_analogStreamId, data);
    if (message == nullptr) {
        return false;
    }

    uint16_t seq = pktSeq(resetSeq);
    if (frameType == AudioFrameType::TERMINATOR) {
        seq = RTP_END_OF_CALL_SEQ;
    }

    if (noSequence) {
        seq = RTP_END_OF_CALL_SEQ;
    }

    return writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG }, message.get(), messageLength, seq, m_analogStreamId);
}

/* Helper to test if the analog ring buffer has data. */

bool BaseNetwork::hasAnalogData() const
{
    if (m_rxAnalogData.isEmpty())
        return false;

    return true;
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Helper to update the RTP packet sequence. */

uint16_t BaseNetwork::pktSeq(bool reset)
{
    if (reset) {
        m_pktSeq = 0U;
    }

    uint16_t curr = m_pktSeq;
    ++m_pktSeq;
    if (m_pktSeq > (RTP_END_OF_CALL_SEQ - 1U)) {
        m_pktSeq = 0U;
    }

    return curr;
}

/* Creates an DMR frame message. */

UInt8Array BaseNetwork::createDMR_Message(uint32_t& length, const uint32_t streamId, const dmr::data::NetData& data)
{
    using namespace dmr::defines;
    uint8_t* buffer = new uint8_t[DMR_PACKET_LENGTH + PACKET_PAD];
    ::memset(buffer, 0x00U, DMR_PACKET_LENGTH + PACKET_PAD);

    // construct DMR message header
    ::memcpy(buffer + 0U, TAG_DMR_DATA, 4U);

    uint32_t srcId = data.getSrcId();                                               // Source Address
    SET_UINT24(srcId, buffer, 5U);

    uint32_t dstId = data.getDstId();                                               // Target Address
    SET_UINT24(dstId, buffer, 8U);

    uint32_t slotNo = data.getSlotNo();

    buffer[14U] = data.getControl();                                                // Control Bits

    // Individual slot disabling
    if (slotNo == 1U && !m_slot1) {
        delete[] buffer;
        return nullptr;
    }
    if (slotNo == 2U && !m_slot2) {
        delete[] buffer;
        return nullptr;
    }

    buffer[15U] = slotNo == 1U ? 0x00U : 0x80U;                                     // Slot Number

    FLCO::E flco = data.getFLCO();
    buffer[15U] |= flco == FLCO::GROUP ? 0x00U : 0x40U;                             // Group

    DataType::E dataType = data.getDataType();
    if (dataType == DataType::VOICE_SYNC) {
        buffer[15U] |= 0x10U;
    }
    else if (dataType == DataType::VOICE) {
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
        Utils::dump(1U, "BaseNetwork::createDMR_Message(), Message", buffer, (DMR_PACKET_LENGTH + PACKET_PAD));

    length = (DMR_PACKET_LENGTH + PACKET_PAD);
    return UInt8Array(buffer);
}

/* Creates an P25 frame message header. */

void BaseNetwork::createP25_MessageHdr(uint8_t* buffer, p25::defines::DUID::E duid, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd,
    p25::defines::FrameType::E frameType, uint8_t controlByte)
{
    using namespace p25::defines;
    assert(buffer != nullptr);

    // construct P25 message header
    ::memcpy(buffer + 0U, TAG_P25_DATA, 4U);

    buffer[4U] = control.getLCO();                                                  // LCO

    uint32_t srcId = control.getSrcId();                                            // Source Address
    SET_UINT24(srcId, buffer, 5U);

    uint32_t dstId = control.getDstId();                                            // Target Address
    SET_UINT24(dstId, buffer, 8U);

    uint16_t sysId = control.getSiteData().sysId();                                 // System ID
    SET_UINT16(sysId, buffer, 11U);

    buffer[14U] = controlByte;                                                      // Control Bits

    buffer[15U] = control.getMFId();                                                // MFId

    uint32_t netId = control.getSiteData().netId();                                 // Network ID
    SET_UINT24(netId, buffer, 16U);

    buffer[20U] = lsd.getLSD1();                                                    // LSD 1
    buffer[21U] = lsd.getLSD2();                                                    // LSD 2

    buffer[22U] = duid;                                                             // DUID

    if (frameType != FrameType::TERMINATOR) {
        buffer[180U] = frameType;                                                   // DVM Frame Type
    }

    // is this the first frame of a call?
    if (frameType == FrameType::HDU_VALID) {
        buffer[181U] = control.getAlgId();                                          // Algorithm ID

        uint32_t kid = control.getKId();
        SET_UINT16(kid, buffer, 182U);                                              // Key ID

        // copy MI data
        uint8_t mi[MI_LENGTH_BYTES];
        ::memset(mi, 0x00U, MI_LENGTH_BYTES);
        control.getMI(mi);

        if (m_debug) {
            Utils::dump(1U, "BaseNetwork::createP25_Message(), HDU MI", mi, MI_LENGTH_BYTES);
        }

        for (uint8_t i = 0; i < MI_LENGTH_BYTES; i++) {
            buffer[184U + i] = mi[i];                                               // Message Indicator
        }
    }
}

/* Creates an P25 LDU1 frame message. */

UInt8Array BaseNetwork::createP25_LDU1Message(uint32_t& length, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, 
    const uint8_t* data, p25::defines::FrameType::E frameType, uint8_t controlByte)
{
    using namespace p25::defines;
    using namespace p25::dfsi::defines;
    assert(data != nullptr);

    p25::dfsi::LC dfsiLC = p25::dfsi::LC(control, lsd);

    uint8_t* buffer = new uint8_t[P25_LDU1_PACKET_LENGTH + PACKET_PAD];
    ::memset(buffer, 0x00U, P25_LDU1_PACKET_LENGTH + PACKET_PAD);

    // construct P25 message header
    createP25_MessageHdr(buffer, DUID::LDU1, control, lsd, frameType, controlByte);

    // pack DFSI data
    uint32_t count = MSG_HDR_SIZE;
    uint8_t imbe[RAW_IMBE_LENGTH_BYTES];

    dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE1);
    m_audio.decode(data, imbe, 0U);
    dfsiLC.encodeLDU1(buffer + 24U, imbe);
    count += DFSI_LDU1_VOICE1_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE2);
    m_audio.decode(data, imbe, 1U);
    dfsiLC.encodeLDU1(buffer + 46U, imbe);
    count += DFSI_LDU1_VOICE2_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE3);
    m_audio.decode(data, imbe, 2U);
    dfsiLC.encodeLDU1(buffer + 60U, imbe);
    count += DFSI_LDU1_VOICE3_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE4);
    m_audio.decode(data, imbe, 3U);
    dfsiLC.encodeLDU1(buffer + 77U, imbe);
    count += DFSI_LDU1_VOICE4_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE5);
    m_audio.decode(data, imbe, 4U);
    dfsiLC.encodeLDU1(buffer + 94U, imbe);
    count += DFSI_LDU1_VOICE5_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE6);
    m_audio.decode(data, imbe, 5U);
    dfsiLC.encodeLDU1(buffer + 111U, imbe);
    count += DFSI_LDU1_VOICE6_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE7);
    m_audio.decode(data, imbe, 6U);
    dfsiLC.encodeLDU1(buffer + 128U, imbe);
    count += DFSI_LDU1_VOICE7_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE8);
    m_audio.decode(data, imbe, 7U);
    dfsiLC.encodeLDU1(buffer + 145U, imbe);
    count += DFSI_LDU1_VOICE8_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE9);
    m_audio.decode(data, imbe, 8U);
    dfsiLC.encodeLDU1(buffer + 162U, imbe);
    count += DFSI_LDU1_VOICE9_FRAME_LENGTH_BYTES;

    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "BaseNetwork::createP25_LDU1Message(), Message, LDU1", buffer, (P25_LDU1_PACKET_LENGTH + PACKET_PAD));

    length = (P25_LDU1_PACKET_LENGTH + PACKET_PAD);
    return UInt8Array(buffer);
}

/* Creates an P25 LDU2 frame message. */

UInt8Array BaseNetwork::createP25_LDU2Message(uint32_t& length, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, 
    const uint8_t* data, uint8_t controlByte)
{
    using namespace p25::defines;
    using namespace p25::dfsi::defines;
    assert(data != nullptr);

    p25::dfsi::LC dfsiLC = p25::dfsi::LC(control, lsd);

    uint8_t* buffer = new uint8_t[P25_LDU2_PACKET_LENGTH + PACKET_PAD];
    ::memset(buffer, 0x00U, P25_LDU2_PACKET_LENGTH + PACKET_PAD);

    // construct P25 message header
    createP25_MessageHdr(buffer, DUID::LDU2, control, lsd, FrameType::DATA_UNIT, controlByte);

    // pack DFSI data
    uint32_t count = MSG_HDR_SIZE;
    uint8_t imbe[RAW_IMBE_LENGTH_BYTES];

    dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE10);
    m_audio.decode(data, imbe, 0U);
    dfsiLC.encodeLDU2(buffer + 24U, imbe);
    count += DFSI_LDU2_VOICE10_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE11);
    m_audio.decode(data, imbe, 1U);
    dfsiLC.encodeLDU2(buffer + 46U, imbe);
    count += DFSI_LDU2_VOICE11_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE12);
    m_audio.decode(data, imbe, 2U);
    dfsiLC.encodeLDU2(buffer + 60U, imbe);
    count += DFSI_LDU2_VOICE12_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE13);
    m_audio.decode(data, imbe, 3U);
    dfsiLC.encodeLDU2(buffer + 77U, imbe);
    count += DFSI_LDU2_VOICE13_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE14);
    m_audio.decode(data, imbe, 4U);
    dfsiLC.encodeLDU2(buffer + 94U, imbe);
    count += DFSI_LDU2_VOICE14_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE15);
    m_audio.decode(data, imbe, 5U);
    dfsiLC.encodeLDU2(buffer + 111U, imbe);
    count += DFSI_LDU2_VOICE15_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE16);
    m_audio.decode(data, imbe, 6U);
    dfsiLC.encodeLDU2(buffer + 128U, imbe);
    count += DFSI_LDU2_VOICE16_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE17);
    m_audio.decode(data, imbe, 7U);
    dfsiLC.encodeLDU2(buffer + 145U, imbe);
    count += DFSI_LDU2_VOICE17_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE18);
    m_audio.decode(data, imbe, 8U);
    dfsiLC.encodeLDU2(buffer + 162U, imbe);
    count += DFSI_LDU2_VOICE18_FRAME_LENGTH_BYTES;

    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "BaseNetwork::createP25_LDU2Message(), Message, LDU2", buffer, (P25_LDU2_PACKET_LENGTH + PACKET_PAD));

    length = (P25_LDU2_PACKET_LENGTH + PACKET_PAD);
    return UInt8Array(buffer);
}

/* Creates an P25 TDU frame message. */

UInt8Array BaseNetwork::createP25_TDUMessage(uint32_t& length, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t controlByte)
{
    using namespace p25::defines;
    uint8_t* buffer = new uint8_t[MSG_HDR_SIZE + PACKET_PAD];
    ::memset(buffer, 0x00U, MSG_HDR_SIZE + PACKET_PAD);

    // construct P25 message header
    createP25_MessageHdr(buffer, DUID::TDU, control, lsd, FrameType::TERMINATOR);

    buffer[14U] = controlByte;
    buffer[23U] = MSG_HDR_SIZE;

    if (m_debug)
        Utils::dump(1U, "BaseNetwork::createP25_TDUMessage(), Message, TDU", buffer, (MSG_HDR_SIZE + PACKET_PAD));

    length = (MSG_HDR_SIZE + PACKET_PAD);
    return UInt8Array(buffer);
}

/* Creates an P25 TSDU frame message. */

UInt8Array BaseNetwork::createP25_TSDUMessage(uint32_t& length, const p25::lc::LC& control, const uint8_t* data)
{
    using namespace p25::defines;
    assert(data != nullptr);

    uint8_t* buffer = new uint8_t[P25_TSDU_PACKET_LENGTH + PACKET_PAD];
    ::memset(buffer, 0x00U, P25_TSDU_PACKET_LENGTH + PACKET_PAD);

    // construct P25 message header
    p25::data::LowSpeedData lsd = p25::data::LowSpeedData();
    createP25_MessageHdr(buffer, DUID::TSDU, control, lsd, FrameType::TERMINATOR);

    // pack raw P25 TSDU bytes
    ::memcpy(buffer + 24U, data, P25_TSDU_FRAME_LENGTH_BYTES);

    buffer[23U] = P25_TSDU_FRAME_LENGTH_BYTES;

    if (m_debug)
        Utils::dump(1U, "BaseNetwork::createP25_TSDUMessage(), Message, TDSU", buffer, (P25_TSDU_PACKET_LENGTH + PACKET_PAD));

    length = (P25_TSDU_PACKET_LENGTH + PACKET_PAD);
    return UInt8Array(buffer);
}

/* Creates an P25 TDULC frame message. */

UInt8Array BaseNetwork::createP25_TDULCMessage(uint32_t& length, const p25::lc::LC& control, const uint8_t* data)
{
    using namespace p25::defines;
    assert(data != nullptr);

    uint8_t* buffer = new uint8_t[P25_TDULC_PACKET_LENGTH + PACKET_PAD];
    ::memset(buffer, 0x00U, P25_TDULC_PACKET_LENGTH + PACKET_PAD);

    // construct P25 message header
    p25::data::LowSpeedData lsd = p25::data::LowSpeedData();
    createP25_MessageHdr(buffer, DUID::TDULC, control, lsd, FrameType::TERMINATOR);

    // pack raw P25 TSDU bytes
    ::memcpy(buffer + 24U, data, P25_TDULC_FRAME_LENGTH_BYTES);

    buffer[23U] = P25_TDULC_FRAME_LENGTH_BYTES;

    if (m_debug)
        Utils::dump(1U, "BaseNetwork::createP25_TDULCMessage(), Message, TDULC", buffer, (P25_TDULC_PACKET_LENGTH + PACKET_PAD));

    length = (P25_TDULC_PACKET_LENGTH + PACKET_PAD);
    return UInt8Array(buffer);
}

/* Writes P25 PDU frame data to the network. */

UInt8Array BaseNetwork::createP25_PDUMessage(uint32_t& length, const p25::data::DataHeader& header,
    const uint8_t currentBlock, const uint8_t* data, const uint32_t len)
{
    using namespace p25::defines;
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
    if (header.getFormat() == PDUFormatType::CONFIRMED) {
        buffer[4U] |= 0x80U;
    }

    SET_UINT24(len, buffer, 8U);                                                    // PDU Length [bytes]

    buffer[15U] = header.getMFId();                                                 // MFId

    buffer[20U] = header.getBlocksToFollow();                                       // Blocks To Follow
    buffer[21U] = currentBlock;                                                     // Current Block

    buffer[22U] = DUID::PDU;                                                        // DUID

    // pack raw P25 PDU bytes
    uint32_t count = MSG_HDR_SIZE;

    ::memcpy(buffer + 24U, data, len);
    count += len;

    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "BaseNetwork::createP25_PDUMessage(), Message, PDU", buffer, (count + PACKET_PAD));

    length = (count + PACKET_PAD);
    return UInt8Array(buffer);
}

/* Writes NXDN frame data to the network. */

UInt8Array BaseNetwork::createNXDN_Message(uint32_t& length, const nxdn::lc::RTCH& lc, const uint8_t* data, const uint32_t len)
{
    assert(data != nullptr);

    uint8_t* buffer = new uint8_t[NXDN_PACKET_LENGTH + PACKET_PAD];
    ::memset(buffer, 0x00U, NXDN_PACKET_LENGTH + PACKET_PAD);

    // construct NXDN message header
    ::memcpy(buffer + 0U, TAG_NXDN_DATA, 4U);

    buffer[4U] = lc.getMessageType();                                           // Message Type

    uint32_t srcId = lc.getSrcId();                                             // Source Address
    SET_UINT24(srcId, buffer, 5U);

    uint32_t dstId = lc.getDstId();                                             // Target Address
    SET_UINT24(dstId, buffer, 8U);

    buffer[14U] = 0U;                                                           // Control Bits

    buffer[15U] |= lc.getGroup() ? 0x00U : 0x40U;                               // Group

    // pack raw NXDN message bytes
    uint32_t count = MSG_HDR_SIZE;

    ::memcpy(buffer + 24U, data, len);
    count += len;

    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "BaseNetwork::createNXDN_Message(), Message", buffer, (NXDN_PACKET_LENGTH + PACKET_PAD));

    length = (NXDN_PACKET_LENGTH + PACKET_PAD);
    return UInt8Array(buffer);
}

/* Creates an analog frame message. */

UInt8Array BaseNetwork::createAnalog_Message(uint32_t& length, const uint32_t streamId, const analog::data::NetData& data)
{
    using namespace analog::defines;
    uint8_t* buffer = new uint8_t[ANALOG_PACKET_LENGTH + PACKET_PAD];
    ::memset(buffer, 0x00U, ANALOG_PACKET_LENGTH + PACKET_PAD);

    // construct analog message header
    ::memcpy(buffer + 0U, TAG_ANALOG_DATA, 4U);

    uint32_t srcId = data.getSrcId();                                               // Source Address
    SET_UINT24(srcId, buffer, 5U);

    uint32_t dstId = data.getDstId();                                               // Target Address
    SET_UINT24(dstId, buffer, 8U);

    buffer[14U] = data.getControl();                                                // Control Bits

    AudioFrameType::E frameType = data.getFrameType();
    buffer[15U] = (uint8_t)frameType;                                               // Audio Frame Type
    buffer[15U] |= data.getGroup() ? 0x00U : 0x40U;                                 // Group

    buffer[4U] = data.getSeqNo();                                                   // Sequence Number

    // pack raw audio message bytes
    data.getAudio(buffer + 20U);

    if (m_debug)
        Utils::dump(1U, "BaseNetwork::createAnalog_Message(), Message", buffer, (ANALOG_PACKET_LENGTH + PACKET_PAD));

    length = (ANALOG_PACKET_LENGTH + PACKET_PAD);
    return UInt8Array(buffer);
}
