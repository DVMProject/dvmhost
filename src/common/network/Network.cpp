// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2017-2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/edac/SHA256.h"
#include "common/network/RTPHeader.h"
#include "common/network/RTPFNEHeader.h"
#include "common/network/json/json.h"
#include "common/p25/kmm/KMMFactory.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "network/Network.h"

using namespace network;

#include <cstdio>
#include <cassert>
#include <cmath>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define MAX_SERVER_DIFF 360ULL // maximum difference in time between a server timestamp and local timestamp in milliseconds

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the Network class. */

Network::Network(const std::string& address, uint16_t port, uint16_t localPort, uint32_t peerId, const std::string& password,
    bool duplex, bool debug, bool dmr, bool p25, bool nxdn, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, bool updateLookup, bool saveLookup) :
    BaseNetwork(peerId, duplex, debug, slot1, slot2, allowActivityTransfer, allowDiagnosticTransfer, localPort),
    m_pktLastSeq(0U),
    m_address(address),
    m_port(port),
    m_password(password),
    m_enabled(false),
    m_dmrEnabled(dmr),
    m_p25Enabled(p25),
    m_nxdnEnabled(nxdn),
    m_updateLookup(updateLookup),
    m_saveLookup(saveLookup),
    m_ridLookup(nullptr),
    m_tidLookup(nullptr),
    m_salt(nullptr),
    m_retryTimer(1000U, 10U),
    m_timeoutTimer(1000U, 60U),
    m_pktSeq(0U),
    m_loginStreamId(0U),
    m_metadata(nullptr),
    m_remotePeerId(0U),
    m_promiscuousPeer(false),
    m_userHandleProtocol(false),
    m_neverDisableOnACLNAK(false),
    m_peerConnectedCallback(nullptr),
    m_peerDisconnectedCallback(nullptr),
    m_dmrInCallCallback(nullptr),
    m_p25InCallCallback(nullptr),
    m_nxdnInCallCallback(nullptr),
    m_keyRespCallback(nullptr)
{
    assert(!address.empty());
    assert(port > 0U);
    assert(!password.empty());

    m_salt = new uint8_t[sizeof(uint32_t)];

    m_rxDMRStreamId = new uint32_t[2U];
    m_rxDMRStreamId[0U] = 0U;
    m_rxDMRStreamId[1U] = 0U;
    m_rxP25StreamId = 0U;
    m_rxNXDNStreamId = 0U;

    m_metadata = new PeerMetadata();
}

/* Finalizes a instance of the Network class. */

Network::~Network()
{
    delete[] m_salt;
    delete[] m_rxDMRStreamId;
    delete m_metadata;
}

/* Resets the DMR ring buffer for the given slot. */

void Network::resetDMR(uint32_t slotNo)
{
    assert(slotNo == 1U || slotNo == 2U);

    BaseNetwork::resetDMR(slotNo);
    if (slotNo == 1U) {
        m_rxDMRStreamId[0U] = 0U;
    }
    else {
        m_rxDMRStreamId[1U] = 0U;
    }
}

/* Resets the P25 ring buffer. */

void Network::resetP25()
{
    BaseNetwork::resetP25();
    m_rxP25StreamId = 0U;
}

/* Resets the NXDN ring buffer. */

void Network::resetNXDN()
{
    BaseNetwork::resetNXDN();
    m_rxNXDNStreamId = 0U;
}

/* Sets the instances of the Radio ID and Talkgroup ID lookup tables. */

void Network::setLookups(lookups::RadioIdLookup* ridLookup, lookups::TalkgroupRulesLookup* tidLookup)
{
    m_ridLookup = ridLookup;
    m_tidLookup = tidLookup;
}

/* Sets metadata configuration settings from the modem. */

void Network::setMetadata(const std::string& identity, uint32_t rxFrequency, uint32_t txFrequency, float txOffsetMhz, float chBandwidthKhz,
    uint8_t channelId, uint32_t channelNo, uint32_t power, float latitude, float longitude, int height, const std::string& location)
{
    m_metadata->identity = identity;

    m_metadata->rxFrequency = rxFrequency;
    m_metadata->txFrequency = txFrequency;

    m_metadata->txOffsetMhz = txOffsetMhz;
    m_metadata->chBandwidthKhz = chBandwidthKhz;
    m_metadata->channelId = channelId;
    m_metadata->channelNo = channelNo;

    m_metadata->power = power;
    m_metadata->latitude = latitude;
    m_metadata->longitude = longitude;
    m_metadata->height = height;
    m_metadata->location = location;
}

/* Sets REST API configuration settings from the modem. */

void Network::setRESTAPIData(const std::string& password, uint16_t port)
{
    m_metadata->restApiPassword = password;
    m_metadata->restApiPort = port;
}

/* Sets endpoint preshared encryption key. */

void Network::setPresharedKey(const uint8_t* presharedKey)
{
    m_socket->setPresharedKey(presharedKey);
}

/* Updates the timer by the passed number of milliseconds. */

void Network::clock(uint32_t ms)
{
    if (m_status == NET_STAT_WAITING_CONNECT) {
        m_retryTimer.clock(ms);
        if (m_retryTimer.isRunning() && m_retryTimer.hasExpired()) {
            if (m_enabled) {
                bool ret = m_socket->open(m_addr.ss_family);
                if (ret) {
                    ret = writeLogin();
                    if (!ret) {
                        m_retryTimer.start();
                        return;
                    }

                    m_status = NET_STAT_WAITING_LOGIN;
                    m_timeoutTimer.start();
                }
            }

            m_retryTimer.start();
        }

        return;
    }

    // if we aren't enabled -- bail
    if (!m_enabled) {
        return;
    }

    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // roll the RTP timestamp if no call is in progress
    if ((m_status == NET_STAT_RUNNING) &&
        (m_rxDMRStreamId[0U] == 0U && m_rxDMRStreamId[1U] == 0U) &&
        m_rxP25StreamId == 0U && m_rxNXDNStreamId == 0U) {
        frame::RTPHeader::resetStartTime();
    }

    sockaddr_storage address;
    uint32_t addrLen;

    frame::RTPHeader rtpHeader;
    frame::RTPFNEHeader fneHeader;
    int length = 0U;

    // read message
    UInt8Array buffer = m_frameQueue->read(length, address, addrLen, &rtpHeader, &fneHeader);
    if (length > 0) {
        if (!udp::Socket::match(m_addr, address)) {
            LogError(LOG_NET, "Packet received from an invalid source");
            return;
        }

        if (m_debug) {
            LogDebugEx(LOG_NET, "Network::clock()", "RTP, peerId = %u, seq = %u, streamId = %u, func = %02X, subFunc = %02X", fneHeader.getPeerId(), rtpHeader.getSequence(),
                fneHeader.getStreamId(), fneHeader.getFunction(), fneHeader.getSubFunction());
        }

        // ensure the RTP synchronization source ID matches the FNE peer ID
        if (m_remotePeerId != 0U && rtpHeader.getSSRC() != m_remotePeerId) {
            LogWarning(LOG_NET, "RTP header and traffic session do not agree on remote peer ID? %u != %u", rtpHeader.getSSRC(), m_remotePeerId);
            // should this be a fatal error?
        }

        // is this RTP packet destined for us?
        uint32_t peerId = fneHeader.getPeerId();
        if ((m_peerId != peerId) && !m_promiscuousPeer) {
            LogError(LOG_NET, "Packet received was not destined for us? peerId = %u", peerId);
            return;
        }

        // peer connections should never encounter no stream ID
        uint32_t streamId = fneHeader.getStreamId();
        if (streamId == 0U) {
            LogWarning(LOG_NET, "BUGBUG: strange RTP packet with no stream ID?");
        }

        m_pktSeq = rtpHeader.getSequence();
        
        if (m_pktSeq == RTP_END_OF_CALL_SEQ) {
            m_pktSeq = 0U;
            m_pktLastSeq = 0U;
        }

        // process incoming message function opcodes
        switch (fneHeader.getFunction()) {
        case NET_FUNC::PROTOCOL:                                        // Protocol
            {
                // are protocol messages being user handled?
                if (m_userHandleProtocol) {
                    userPacketHandler(fneHeader.getPeerId(), { fneHeader.getFunction(), fneHeader.getSubFunction() }, 
                        buffer.get(), length, fneHeader.getStreamId());
                    break;
                }

                // process incomfing message subfunction opcodes
                switch (fneHeader.getSubFunction()) {
                case NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR:                 // Encapsulated DMR data frame
                    {
                        if (m_enabled && m_dmrEnabled) {
                            uint32_t slotNo = (buffer[15U] & 0x80U) == 0x80U ? 1U : 0U; // this is the raw index for the stream ID array

                            if (m_debug) {
                                LogDebug(LOG_NET, "DMR Slot %u, peer = %u, len = %u, pktSeq = %u, streamId = %u", 
                                    slotNo + 1U, peerId, length, rtpHeader.getSequence(), streamId);
                            }

                            if (m_promiscuousPeer) {
                                m_rxDMRStreamId[slotNo] = streamId;
                                m_pktLastSeq = m_pktSeq;
                            }
                            else {
                                if (m_rxDMRStreamId[slotNo] == 0U) {
                                    if (rtpHeader.getSequence() == RTP_END_OF_CALL_SEQ) {
                                        m_rxDMRStreamId[slotNo] = 0U;
                                    }
                                    else {
                                        m_rxDMRStreamId[slotNo] = streamId;
                                    }

                                    m_pktLastSeq = m_pktSeq;
                                }
                                else {
                                    if (m_rxDMRStreamId[slotNo] == streamId) {
                                        if (m_pktSeq != 0U && m_pktLastSeq != 0U) {
                                            if (m_pktSeq >= 1U && ((m_pktSeq != m_pktLastSeq + 1) && (m_pktSeq - 1 != m_pktLastSeq + 1))) {
                                                LogWarning(LOG_NET, "DMR Stream %u out-of-sequence; %u != %u", streamId, m_pktSeq, m_pktLastSeq + 1);
                                            }
                                        }

                                        m_pktLastSeq = m_pktSeq;
                                    }

                                    if (rtpHeader.getSequence() == RTP_END_OF_CALL_SEQ) {
                                        m_rxDMRStreamId[slotNo] = 0U;
                                    }
                                }
                            }

                            if (m_debug)
                                Utils::dump(1U, "[Network::clock()] Network Received, DMR", buffer.get(), length);
                            if (length > 255)
                                LogError(LOG_NET, "DMR Stream %u, frame oversized? this shouldn't happen, pktSeq = %u, len = %u", streamId, m_pktSeq, length);

                            uint8_t len = length;
                            m_rxDMRData.addData(&len, 1U);
                            m_rxDMRData.addData(buffer.get(), len);
                        }
                    }
                    break;

                case NET_SUBFUNC::PROTOCOL_SUBFUNC_P25:                 // Encapsulated P25 data frame
                    {
                        if (m_enabled && m_p25Enabled) {
                            if (m_debug) {
                                LogDebug(LOG_NET, "P25, peer = %u, len = %u, pktSeq = %u, streamId = %u", 
                                    peerId, length, rtpHeader.getSequence(), streamId);
                            }

                            if (m_promiscuousPeer) {
                                m_rxP25StreamId = streamId;
                                m_pktLastSeq = m_pktSeq;
                            }
                            else {
                                if (m_rxP25StreamId == 0U) {
                                    if (rtpHeader.getSequence() == RTP_END_OF_CALL_SEQ) {
                                        m_rxP25StreamId = 0U;
                                    }
                                    else {
                                        m_rxP25StreamId = streamId;
                                    }

                                    m_pktLastSeq = m_pktSeq;
                                }
                                else {
                                    if (m_rxP25StreamId == streamId) {
                                        if (m_pktSeq != 0U && m_pktLastSeq != 0U) {
                                            if (m_pktSeq >= 1U && ((m_pktSeq != m_pktLastSeq + 1) && (m_pktSeq - 1 != m_pktLastSeq + 1))) {
                                                LogWarning(LOG_NET, "P25 Stream %u out-of-sequence; %u != %u", streamId, m_pktSeq, m_pktLastSeq + 1);
                                            }
                                        }

                                        m_pktLastSeq = m_pktSeq;
                                    }

                                    if (rtpHeader.getSequence() == RTP_END_OF_CALL_SEQ) {
                                        m_rxP25StreamId = 0U;
                                    }
                                }
                            }

                            if (m_debug)
                                Utils::dump(1U, "[Network::clock()] Network Received, P25", buffer.get(), length);
                            if (length > 255)
                                LogError(LOG_NET, "P25 Stream %u, frame oversized? this shouldn't happen, pktSeq = %u, len = %u", streamId, m_pktSeq, length);

                            uint8_t len = length;
                            m_rxP25Data.addData(&len, 1U);
                            m_rxP25Data.addData(buffer.get(), len);
                        }
                    }
                    break;

                case NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN:                // Encapsulated NXDN data frame
                    {
                        if (m_enabled && m_nxdnEnabled) {
                            if (m_debug) {
                                LogDebug(LOG_NET, "NXDN, peer = %u, len = %u, pktSeq = %u, streamId = %u", 
                                    peerId, length, rtpHeader.getSequence(), streamId);
                            }

                            if (m_promiscuousPeer) {
                                m_rxNXDNStreamId = streamId;
                                m_pktLastSeq = m_pktSeq;
                            }
                            else {
                                if (m_rxNXDNStreamId == 0U) {
                                    if (rtpHeader.getSequence() == RTP_END_OF_CALL_SEQ) {
                                        m_rxNXDNStreamId = 0U;
                                    }
                                    else {
                                        m_rxNXDNStreamId = streamId;
                                    }

                                    m_pktLastSeq = m_pktSeq;
                                }
                                else {
                                    if (m_rxNXDNStreamId == streamId) {
                                        if (m_pktSeq != 0U && m_pktLastSeq != 0U) {
                                            if (m_pktSeq >= 1U && ((m_pktSeq != m_pktLastSeq + 1) && (m_pktSeq - 1 != m_pktLastSeq + 1))) {
                                                LogWarning(LOG_NET, "NXDN Stream %u out-of-sequence; %u != %u", streamId, m_pktSeq, m_pktLastSeq + 1);
                                            }
                                        }

                                        m_pktLastSeq = m_pktSeq;
                                    }

                                    if (rtpHeader.getSequence() == RTP_END_OF_CALL_SEQ) {
                                        m_rxNXDNStreamId = 0U;
                                    }
                                }
                            }

                            if (m_debug)
                                Utils::dump(1U, "[Network::clock()] Network Received, NXDN", buffer.get(), length);
                            if (length > 255)
                                LogError(LOG_NET, "NXDN Stream %u, frame oversized? this shouldn't happen, pktSeq = %u, len = %u", streamId, m_pktSeq, length);

                            uint8_t len = length;
                            m_rxNXDNData.addData(&len, 1U);
                            m_rxNXDNData.addData(buffer.get(), len);
                        }
                    }
                    break;

                default:
                    Utils::dump("unknown protocol opcode from the master", buffer.get(), length);
                    break;
                }
            }
            break;

        case NET_FUNC::MASTER:                                          // Master
            {
                // process incomfing message subfunction opcodes
                switch (fneHeader.getSubFunction()) {
                case NET_SUBFUNC::MASTER_SUBFUNC_WL_RID:                // Radio ID Whitelist
                    {
                        if (m_enabled && m_updateLookup) {
                            if (m_debug)
                                Utils::dump(1U, "Network Received, WL RID", buffer.get(), length);

                            if (m_ridLookup != nullptr) {
                                // update RID lists
                                uint32_t len = GET_UINT32(buffer, 6U);
                                uint32_t offs = 11U;
                                for (uint32_t i = 0; i < len; i++) {
                                    uint32_t id = GET_UINT24(buffer, offs);
                                    m_ridLookup->toggleEntry(id, true);
                                    offs += 4U;
                                }

                                LogMessage(LOG_NET, "Network Announced %u whitelisted RIDs", len);

                                // save to file if enabled and we got RIDs
                                if (m_saveLookup && len > 0) {
                                    m_ridLookup->commit();
                                }
                            }
                        }
                    }
                    break;
                case NET_SUBFUNC::MASTER_SUBFUNC_BL_RID:                // Radio ID Blacklist
                    {
                        if (m_enabled && m_updateLookup) {
                            if (m_debug)
                                Utils::dump(1U, "Network Received, BL RID", buffer.get(), length);

                            if (m_ridLookup != nullptr) {
                                // update RID lists
                                uint32_t len = GET_UINT32(buffer, 6U);
                                uint32_t offs = 11U;
                                for (uint32_t i = 0; i < len; i++) {
                                    uint32_t id = GET_UINT24(buffer, offs);
                                    m_ridLookup->toggleEntry(id, false);
                                    offs += 4U;
                                }

                                LogMessage(LOG_NET, "Network Announced %u blacklisted RIDs", len);

                                // save to file if enabled and we got RIDs
                                if (m_saveLookup && len > 0) {
                                    m_ridLookup->commit();
                                }
                            }
                        }
                    }
                    break;

                case NET_SUBFUNC::MASTER_SUBFUNC_ACTIVE_TGS:            // Talkgroup Active IDs
                    {
                        if (m_enabled && m_updateLookup) {
                            if (m_debug)
                                Utils::dump(1U, "Network Received, ACTIVE TGS", buffer.get(), length);

                            if (m_tidLookup != nullptr) {
                                // update TGID lists
                                uint32_t len = GET_UINT32(buffer, 6U);
                                uint32_t offs = 11U;
                                for (uint32_t i = 0; i < len; i++) {
                                    uint32_t id = GET_UINT24(buffer, offs);
                                    uint8_t slot = (buffer[offs + 3U]) & 0x03U;
                                    bool affiliated = (buffer[offs + 3U] & 0x40U) == 0x40U;
                                    bool nonPreferred = (buffer[offs + 3U] & 0x80U) == 0x80U;

                                    lookups::TalkgroupRuleGroupVoice tid = m_tidLookup->find(id, slot);

                                    // if the TG is marked as non-preferred, and the TGID exists in the local entries
                                    // erase the local and overwrite with the FNE data
                                    if (nonPreferred) {
                                        if (!tid.isInvalid()) {
                                            m_tidLookup->eraseEntry(id, slot);
                                            tid = m_tidLookup->find(id, slot);
                                        }
                                    }

                                    if (tid.isInvalid()) {
                                        if (!tid.config().active()) {
                                            m_tidLookup->eraseEntry(id, slot);
                                        }

                                        LogMessage(LOG_NET, "Activated%s%s TG %u TS %u in TGID table", 
                                            (nonPreferred) ? " non-preferred" : "", (affiliated) ? " affiliated" : "", id, slot);
                                        m_tidLookup->addEntry(id, slot, true, affiliated, nonPreferred);
                                    }

                                    offs += 5U;
                                }

                                LogMessage(LOG_NET, "Activated %u TGs; loaded %u entries into talkgroup rules table", len, m_tidLookup->groupVoice().size());

                                // save if saving from network is enabled
                                if (m_saveLookup && len > 0) {
                                    m_tidLookup->commit();
                                }
                            }
                        }
                    }
                    break;
                case NET_SUBFUNC::MASTER_SUBFUNC_DEACTIVE_TGS:          // Talkgroup Deactivated IDs
                    {
                        if (m_enabled && m_updateLookup) {
                            if (m_debug)
                                Utils::dump(1U, "Network Received, DEACTIVE TGS", buffer.get(), length);

                            if (m_tidLookup != nullptr) {
                                // update TGID lists
                                uint32_t len = GET_UINT32(buffer, 6U);
                                uint32_t offs = 11U;
                                for (uint32_t i = 0; i < len; i++) {
                                    uint32_t id = GET_UINT24(buffer, offs);
                                    uint8_t slot = (buffer[offs + 3U]);

                                    lookups::TalkgroupRuleGroupVoice tid = m_tidLookup->find(id, slot);
                                    if (!tid.isInvalid()) {
                                        LogMessage(LOG_NET, "Deactivated TG %u TS %u in TGID table", id, slot);
                                        m_tidLookup->eraseEntry(id, slot);
                                    }

                                    offs += 5U;
                                }

                                LogMessage(LOG_NET, "Deactivated %u TGs; loaded %u entries into talkgroup rules table", len, m_tidLookup->groupVoice().size());

                                // save if saving from network is enabled
                                if (m_saveLookup && len > 0) {
                                    m_tidLookup->commit();
                                }
                            }
                        }
                    }
                    break;

                default:
                    Utils::dump("unknown master control opcode from the master", buffer.get(), length);
                    break;
                }
            }
            break;

        case NET_FUNC::INCALL_CTRL:                                     // In-Call Control
            {
                // process incomfing message subfunction opcodes
                switch (fneHeader.getSubFunction()) {
                case NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR:                 // DMR In-Call Control
                    {
                        if (m_enabled && m_dmrEnabled) {
                            NET_ICC::ENUM command = (NET_ICC::ENUM)buffer[10U];
                            uint32_t dstId = GET_UINT24(buffer, 11U);
                            uint8_t slot = buffer[14U];

                            // fire off DMR in-call callback if we have one
                            if (m_dmrInCallCallback != nullptr) {
                                m_dmrInCallCallback(command, dstId, slot);
                            }
                        }
                    }
                    break;
                case NET_SUBFUNC::PROTOCOL_SUBFUNC_P25:                 // P25 In-Call Control
                    {
                        if (m_enabled && m_p25Enabled) {
                            NET_ICC::ENUM command = (NET_ICC::ENUM)buffer[10U];
                            uint32_t dstId = GET_UINT24(buffer, 11U);

                            // fire off P25 in-call callback if we have one
                            if (m_p25InCallCallback != nullptr) {
                                m_p25InCallCallback(command, dstId);
                            }
                        }    
                    }
                    break;
                case NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN:                // NXDN In-Call Control
                    {
                        if (m_enabled && m_nxdnEnabled) {
                            NET_ICC::ENUM command = (NET_ICC::ENUM)buffer[10U];
                            uint32_t dstId = GET_UINT24(buffer, 11U);

                            // fire off NXDN in-call callback if we have one
                            if (m_nxdnInCallCallback != nullptr) {
                                m_nxdnInCallCallback(command, dstId);
                            }
                        }
                    }
                    break;

                default:
                    Utils::dump("unknown incall control opcode from the master", buffer.get(), length);
                    break;
                }
            }
            break;

        case NET_FUNC::KEY_RSP:                                         // Enc. Key Response
            {
                if (m_enabled) {
                    using namespace p25::kmm;

                    std::unique_ptr<KMMFrame> frame = KMMFactory::create(buffer.get() + 11U);
                    if (frame == nullptr) {
                        LogWarning(LOG_NET, "PEER %u, undecodable KMM frame from master", m_peerId);
                        break;
                    }

                    switch (frame->getMessageId()) {
                    case P25DEF::KMM_MessageType::MODIFY_KEY_CMD:
                        {
                            KMMModifyKey* modifyKey = static_cast<KMMModifyKey*>(frame.get());
                            if (modifyKey->getAlgId() > 0U) {
                                KeysetItem ks = modifyKey->getKeysetItem();
                                if (ks.keys().size() > 0U) {
                                    // fetch first key (a master response should never really send back more then one key)
                                    KeyItem ki = ks.keys()[0];
                                    LogMessage(LOG_NET, "PEER %u, master reported enc. key, algId = $%02X, kID = $%04X", m_peerId,
                                        ks.algId(), ki.kId());

                                    // fire off key response callback if we have one
                                    if (m_keyRespCallback != nullptr) {
                                        m_keyRespCallback(ki, ks.algId(), ks.keyLength());
                                    }
                                }
                            }
                        }
                        break;

                    default:
                        break;
                    }
                }
            }
            break;

        case NET_FUNC::NAK:                                             // Master Negative Ack
            {
                // DVM 3.6 adds support to respond with a NAK reason, as such we just check if the NAK response is greater
                // then 10 bytes and process the reason value
                uint16_t reason = 0U;
                if (length > 10) {
                    reason = GET_UINT16(buffer, 10U);
                    switch (reason) {
                    case NET_CONN_NAK_MODE_NOT_ENABLED:
                        LogWarning(LOG_NET, "PEER %u master NAK; digital mode not enabled on FNE, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                        break;
                    case NET_CONN_NAK_ILLEGAL_PACKET:
                        LogWarning(LOG_NET, "PEER %u master NAK; illegal/unknown packet, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                        break;
                    case NET_CONN_NAK_FNE_UNAUTHORIZED:
                        LogWarning(LOG_NET, "PEER %u master NAK; unauthorized, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                        break;
                    case NET_CONN_NAK_BAD_CONN_STATE:
                        LogWarning(LOG_NET, "PEER %u master NAK; bad connection state, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                        break;
                    case NET_CONN_NAK_INVALID_CONFIG_DATA:
                        LogWarning(LOG_NET, "PEER %u master NAK; invalid configuration data, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                        break;
                    case NET_CONN_NAK_FNE_MAX_CONN:
                        LogWarning(LOG_NET, "PEER %u master NAK; FNE has reached maximum permitted connections, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                        break;
                    case NET_CONN_NAK_PEER_RESET:
                        LogWarning(LOG_NET, "PEER %u master NAK; FNE demanded connection reset, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                        break;
                    case NET_CONN_NAK_PEER_ACL:
                        LogError(LOG_NET, "PEER %u master NAK; ACL rejection, network disabled, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                        if (!m_neverDisableOnACLNAK) {
                            m_status = NET_STAT_WAITING_LOGIN;
                            m_enabled = false; // ACL rejection give up stop trying to connect
                        }
                        break;

                    case NET_CONN_NAK_GENERAL_FAILURE:
                    default:
                        LogWarning(LOG_NET, "PEER %u master NAK; general failure, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                        break;
                    }
                }

                if (m_status == NET_STAT_RUNNING && (reason == NET_CONN_NAK_FNE_MAX_CONN)) {
                    LogWarning(LOG_NET, "PEER %u master NAK; attemping to relogin, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                    m_status = NET_STAT_WAITING_LOGIN;
                    m_timeoutTimer.start();
                    m_retryTimer.start();
                }
                else {
                    if (m_enabled) {
                        LogError(LOG_NET, "PEER %u master NAK; network reconnect, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                        close();
                        open();
                    }
                    return;
                }
            }
            break;
        case NET_FUNC::ACK:                                             // Repeater Ack
            {
                switch (m_status) {
                    case NET_STAT_WAITING_LOGIN:
                        LogMessage(LOG_NET, "PEER %u RPTL ACK, performing login exchange, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());

                        ::memcpy(m_salt, buffer.get() + 6U, sizeof(uint32_t));
                        writeAuthorisation();

                        m_status = NET_STAT_WAITING_AUTHORISATION;
                        m_timeoutTimer.start();
                        m_retryTimer.start();
                        break;
                    case NET_STAT_WAITING_AUTHORISATION:
                        LogMessage(LOG_NET, "PEER %u RPTK ACK, performing configuration exchange, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());

                        writeConfig();

                        m_status = NET_STAT_WAITING_CONFIG;
                        m_timeoutTimer.start();
                        m_retryTimer.start();
                        break;
                    case NET_STAT_WAITING_CONFIG:
                        LogMessage(LOG_NET, "PEER %u RPTC ACK, logged into the master successfully, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                        m_loginStreamId = 0U;
                        m_remotePeerId = rtpHeader.getSSRC();

                        pktSeq(true);

                        // fire off peer connected callback if we have one
                        if (m_peerConnectedCallback != nullptr) {
                            m_peerConnectedCallback();
                        }

                        m_status = NET_STAT_RUNNING;
                        m_timeoutTimer.start();
                        m_retryTimer.start();

                        if (length > 6) {
                            m_useAlternatePortForDiagnostics = (buffer[6U] & 0x80U) == 0x80U;
                            if (m_useAlternatePortForDiagnostics) {
                                LogMessage(LOG_NET, "PEER %u RPTC ACK, master commanded alternate port for diagnostics and activity logging, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                            } else {
                                // disable diagnostic and activity logging automatically if the master doesn't utilize the alternate port
                                m_allowDiagnosticTransfer = false;
                                m_allowActivityTransfer = false;
                                LogWarning(LOG_NET, "PEER %u RPTC ACK, master does not enable alternate port for diagnostics and activity logging, diagnostic and activity logging are disabled, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
            break;
        case NET_FUNC::MST_DISC:                                        // Master Disconnect
            {
                LogError(LOG_NET, "PEER %u master disconnect, remotePeerId = %u", m_peerId, m_remotePeerId);
                m_status = NET_STAT_WAITING_CONNECT;

                // fire off peer disconnected callback if we have one
                if (m_peerDisconnectedCallback != nullptr) {
                    m_peerDisconnectedCallback();
                }

                close();
                open();
            }
            break;
        case NET_FUNC::PONG:                                            // Master Ping Response
            m_timeoutTimer.start();
            if (length >= 14) {
                if (m_debug)
                    Utils::dump(1U, "Network Received, PONG", buffer.get(), length);

                ulong64_t serverNow = 0U;

                // combine bytes into ulong64_t (8 byte) value
                serverNow = buffer[6U];
                serverNow = (serverNow << 8) + buffer[7U];
                serverNow = (serverNow << 8) + buffer[8U];
                serverNow = (serverNow << 8) + buffer[9U];
                serverNow = (serverNow << 8) + buffer[10U];
                serverNow = (serverNow << 8) + buffer[11U];
                serverNow = (serverNow << 8) + buffer[12U];
                serverNow = (serverNow << 8) + buffer[13U];

                // check the ping RTT and report any over the maximum defined time
                uint64_t dt = (uint64_t)fabs((double)now - (double)serverNow);
                if (dt > MAX_SERVER_DIFF)
                    LogWarning(LOG_NET, "PEER %u pong, time delay greater than %llums, now = %llu, server = %llu, dt = %llu", m_peerId, MAX_SERVER_DIFF, now, serverNow, dt);
            }
            break;
        default:
            userPacketHandler(fneHeader.getPeerId(), { fneHeader.getFunction(), fneHeader.getSubFunction() }, 
                buffer.get(), length, fneHeader.getStreamId());
            break;
        }
    }

    m_retryTimer.clock(ms);
    if (m_retryTimer.isRunning() && m_retryTimer.hasExpired()) {
        switch (m_status) {
            case NET_STAT_WAITING_LOGIN:
                LogError(LOG_NET, "PEER %u, retrying master login, remotePeerId = %u", m_peerId, m_remotePeerId);
                writeLogin();
                break;
            case NET_STAT_WAITING_AUTHORISATION:
                writeAuthorisation();
                break;
            case NET_STAT_WAITING_CONFIG:
                writeConfig();
                break;
            case NET_STAT_RUNNING:
                writePing();
                break;
            default:
                break;
        }

        m_retryTimer.start();
    }

    m_timeoutTimer.clock(ms);
    if (m_timeoutTimer.isRunning() && m_timeoutTimer.hasExpired()) {
        LogError(LOG_NET, "PEER %u connection to the master has timed out, retrying connection, remotePeerId = %u", m_peerId, m_remotePeerId);

        // fire off peer disconnected callback if we have one
        if (m_peerDisconnectedCallback != nullptr) {
            m_peerDisconnectedCallback();
        }

        close();
        open();
    }
}

/* Opens connection to the network. */

bool Network::open()
{
    if (!m_enabled)
        return false;
    if (m_debug)
        LogMessage(LOG_NET, "PEER %u opening network", m_peerId);

    if (udp::Socket::lookup(m_address, m_port, m_addr, m_addrLen) != 0) {
        LogMessage(LOG_NET, "!!! Could not lookup the address of the master!");
        return false;
    }

    m_status = NET_STAT_WAITING_CONNECT;
    m_timeoutTimer.start();
    m_retryTimer.start();

    return true;
}

/* Closes connection to the network. */

void Network::close()
{
    if (m_debug)
        LogMessage(LOG_NET, "PEER %u closing Network", m_peerId);

    if (m_status == NET_STAT_RUNNING) {
        uint8_t buffer[1U];
        ::memset(buffer, 0x00U, 1U);

        writeMaster({ NET_FUNC::RPT_DISC, NET_SUBFUNC::NOP }, buffer, 1U, pktSeq(true), createStreamId());
    }

    m_socket->close();

    m_retryTimer.stop();
    m_timeoutTimer.stop();

    m_status = NET_STAT_WAITING_CONNECT;
}

/* Sets flag enabling network communication. */

void Network::enable(bool enabled)
{
    m_enabled = enabled;
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* User overrideable handler that allows user code to process network packets not handled by this class. */

void Network::userPacketHandler(uint32_t peerId, FrameQueue::OpcodePair opcode, const uint8_t* data, uint32_t length, uint32_t streamId)
{
    Utils::dump("unknown opcode from the master", data, length);
}

/* Writes login request to the network. */

bool Network::writeLogin()
{
    if (!m_enabled) {
        return false;
    }

    uint8_t buffer[8U];
    ::memcpy(buffer + 0U, TAG_REPEATER_LOGIN, 4U);
    SET_UINT32(m_peerId, buffer, 4U);                                               // Peer ID

    if (m_debug)
        Utils::dump(1U, "Network Message, Login", buffer, 8U);

    m_loginStreamId = createStreamId();
    m_remotePeerId = 0U;
    return writeMaster({ NET_FUNC::RPTL, NET_SUBFUNC::NOP }, buffer, 8U, pktSeq(true), m_loginStreamId);
}

/* Writes network authentication challenge. */

bool Network::writeAuthorisation()
{
    if (m_loginStreamId == 0U) {
        LogWarning(LOG_NET, "BUGBUG: tried to write network authorisation with no stream ID?");
        return false;
    }

    size_t size = m_password.size();

    uint8_t* in = new uint8_t[size + sizeof(uint32_t)];
    ::memcpy(in, m_salt, sizeof(uint32_t));
    for (size_t i = 0U; i < size; i++)
        in[i + sizeof(uint32_t)] = m_password.at(i);

    uint8_t out[40U];
    ::memcpy(out + 0U, TAG_REPEATER_AUTH, 4U);
    SET_UINT32(m_peerId, out, 4U);                                                  // Peer ID

    edac::SHA256 sha256;
    sha256.buffer(in, (uint32_t)(size + sizeof(uint32_t)), out + 8U);

    delete[] in;

    if (m_debug)
        Utils::dump(1U, "Network Message, Authorisation", out, 40U);

    return writeMaster({ NET_FUNC::RPTK, NET_SUBFUNC::NOP }, out, 40U, pktSeq(), m_loginStreamId);
}

/* Writes modem configuration to the network. */

bool Network::writeConfig()
{
    if (m_loginStreamId == 0U) {
        LogWarning(LOG_NET, "BUGBUG: tried to write network authorisation with no stream ID?");
        return false;
    }

    const char* software = __NETVER__;

    json::object config = json::object();

    // identity and frequency
    config["identity"].set<std::string>(m_metadata->identity);                      // Identity
    config["rxFrequency"].set<uint32_t>(m_metadata->rxFrequency);                   // Rx Frequency
    config["txFrequency"].set<uint32_t>(m_metadata->txFrequency);                   // Tx Frequency

    // system info
    json::object sysInfo = json::object();
    sysInfo["latitude"].set<float>(m_metadata->latitude);                           // Latitude
    sysInfo["longitude"].set<float>(m_metadata->longitude);                         // Longitude

    sysInfo["height"].set<int>(m_metadata->height);                                 // Height
    sysInfo["location"].set<std::string>(m_metadata->location);                     // Location
    config["info"].set<json::object>(sysInfo);

    // channel data
    json::object channel = json::object();
    channel["txPower"].set<uint32_t>(m_metadata->power);                            // Tx Power
    channel["txOffsetMhz"].set<float>(m_metadata->txOffsetMhz);                     // Tx Offset (Mhz)
    channel["chBandwidthKhz"].set<float>(m_metadata->chBandwidthKhz);               // Ch. Bandwidth (khz)
    channel["channelId"].set<uint8_t>(m_metadata->channelId);                       // Channel ID
    channel["channelNo"].set<uint32_t>(m_metadata->channelNo);                      // Channel No
    config["channel"].set<json::object>(channel);

    // RCON
    json::object rcon = json::object();
    rcon["password"].set<std::string>(m_metadata->restApiPassword);                 // REST API Password
    rcon["port"].set<uint16_t>(m_metadata->restApiPort);                            // REST API Port
    config["rcon"].set<json::object>(rcon);

    // Flags
    config["conventionalPeer"].set<bool>(m_metadata->isConventional);               // Conventional Peer Marker

    config["software"].set<std::string>(std::string(software));

    json::value v = json::value(config);
    std::string json = v.serialize();

    DECLARE_CHAR_ARRAY(buffer, json.length() + 9U);

    ::memcpy(buffer + 0U, TAG_REPEATER_CONFIG, 4U);
    ::snprintf(buffer + 8U, json.length() + 1U, "%s", json.c_str());

    if (m_debug) {
        Utils::dump(1U, "Network Message, Configuration", (uint8_t*)buffer, json.length() + 8U);
    }

    return writeMaster({ NET_FUNC::RPTC, NET_SUBFUNC::NOP }, (uint8_t*)buffer, json.length() + 8U, RTP_END_OF_CALL_SEQ, m_loginStreamId);
}

/* Writes a network stay-alive ping. */

bool Network::writePing()
{
    uint8_t buffer[1U];
    ::memset(buffer, 0x00U, 1U);

    if (m_debug)
        Utils::dump(1U, "Network Message, Ping", buffer, 11U);

    return writeMaster({ NET_FUNC::PING, NET_SUBFUNC::NOP }, buffer, 1U, RTP_END_OF_CALL_SEQ, createStreamId());
}
