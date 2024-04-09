// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Converged FNE Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Converged FNE Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023-2024 Bryan Biedenkapp, N2PLL
*
*/
#include "fne/Defines.h"
#include "common/nxdn/NXDNDefines.h"
#include "common/nxdn/channel/LICH.h"
#include "common/nxdn/channel/CAC.h"
#include "common/nxdn/lc/rcch/RCCHFactory.h"
#include "common/nxdn/lc/RTCH.h"
#include "common/nxdn/NXDNUtils.h"
#include "common/nxdn/Sync.h"
#include "common/Clock.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "network/FNENetwork.h"
#include "network/fne/TagNXDNData.h"
#include "HostFNE.h"

using namespace system_clock;
using namespace network;
using namespace network::fne;
using namespace nxdn;

#include <cassert>
#include <chrono>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the TagNXDNData class.
/// </summary>
/// <param name="network"></param>
/// <param name="debug"></param>
TagNXDNData::TagNXDNData(FNENetwork* network, bool debug) :
    m_network(network),
    m_parrotFrames(),
    m_parrotFramesReady(false),
    m_status(),
   m_debug(debug)
{
    assert(network != nullptr);
}

/// <summary>
/// Finalizes a instance of the TagNXDNData class.
/// </summary>
TagNXDNData::~TagNXDNData() = default;

/// <summary>
/// Process a data frame from the network.
/// </summary>
/// <param name="data">Network data buffer.</param>
/// <param name="len">Length of data.</param>
/// <param name="peerId">Peer ID</param>
/// <param name="pktSeq"></param>
/// <param name="streamId">Stream ID</param>
/// <param name="external">Flag indicating traffic is from an external peer.</param>
/// <returns></returns>
bool TagNXDNData::processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint16_t pktSeq, uint32_t streamId, bool external)
{
    hrc::hrc_t pktTime = hrc::now();

    uint8_t buffer[len];
    ::memset(buffer, 0x00U, len);
    ::memcpy(buffer, data, len);

    uint8_t messageType = data[4U];

    uint32_t srcId = __GET_UINT16(data, 5U);
    uint32_t dstId = __GET_UINT16(data, 8U);

    // perform TGID route rewrites if configured
    routeRewrite(buffer, peerId, messageType, dstId, false);
    dstId = __GET_UINT16(buffer, 8U);

    lc::RTCH lc;

    lc.setMessageType(messageType);
    lc.setSrcId((uint16_t)srcId & 0xFFFFU);
    lc.setDstId((uint16_t)dstId & 0xFFFFU);

    bool group = (data[15U] & 0x40U) == 0x40U ? false : true;
    lc.setGroup(group);

    // is the stream valid?
    if (validate(peerId, lc, messageType, streamId)) {
        // is this peer ignored?
        if (!isPeerPermitted(peerId, lc, messageType, streamId)) {
            return false;
        }

        // specifically only check the following logic for end of call, voice or data frames
        if ((messageType == RTCH_MESSAGE_TYPE_TX_REL || messageType == RTCH_MESSAGE_TYPE_TX_REL_EX) ||
            (messageType == RTCH_MESSAGE_TYPE_VCALL || messageType == RTCH_MESSAGE_TYPE_DCALL_HDR ||
             messageType == RTCH_MESSAGE_TYPE_DCALL_DATA)) {
            // is this the end of the call stream?
            if (messageType == RTCH_MESSAGE_TYPE_TX_REL || messageType == RTCH_MESSAGE_TYPE_TX_REL_EX) {
                if (srcId == 0U && dstId == 0U) {
                    LogWarning(LOG_NET, "NXDN, invalid TX_REL, peer = %u, srcId = %u, dstId = %u, streamId = %u, external = %u", peerId, srcId, dstId, streamId, external);
                    return false;
                }

                RxStatus status = m_status[dstId];
                uint64_t duration = hrc::diff(pktTime, status.callStartTime);

                if (std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) { return x.second.dstId == dstId; }) != m_status.end()) {
                    m_status.erase(dstId);

                    // is this a parrot talkgroup? if so, clear any remaining frames from the buffer
                    lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(dstId);
                    if (tg.config().parrot()) {
                        if (m_parrotFrames.size() > 0) {
                            m_parrotFramesReady = true;
                            LogMessage(LOG_NET, "NXDN, Parrot Playback will Start, peer = %u, srcId = %u", peerId, srcId);
                            m_network->m_parrotDelayTimer.start();
                        }
                    }

                    LogMessage(LOG_NET, "NXDN, Call End, peer = %u, srcId = %u, dstId = %u, duration = %u, streamId = %u, external = %u",
                        peerId, srcId, dstId, duration / 1000, streamId, external);

                    // report call event to InfluxDB
                    if (m_network->m_enableInfluxDB) {
                        influxdb::QueryBuilder()
                            .meas("call_event")
                                .tag("peerId", std::to_string(peerId))
                                .tag("mode", "NXDN")
                                .tag("streamId", std::to_string(streamId))
                                .tag("srcId", std::to_string(srcId))
                                .tag("dstId", std::to_string(dstId))
                                    .field("duration", duration)
                                .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                            .request(m_network->m_influxServer);
                    }

                    m_network->m_callInProgress = false;
                }
            }

            // is this a new call stream?
            if ((messageType != RTCH_MESSAGE_TYPE_TX_REL && messageType != RTCH_MESSAGE_TYPE_TX_REL_EX)) {
                if (srcId == 0U && dstId == 0U) {
                    LogWarning(LOG_NET, "NXDN, invalid call, peer = %u, srcId = %u, dstId = %u, streamId = %u, external = %u", peerId, srcId, dstId, streamId, external);
                    return false;
                }

                auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) { return x.second.dstId == dstId; });
                if (it != m_status.end()) {
                    RxStatus status = m_status[dstId];
                    if (streamId != status.streamId) {
                        if (status.srcId != 0U && status.srcId != srcId) {
                            LogWarning(LOG_NET, "NXDN, Call Collision, peer = %u, srcId = %u, dstId = %u, streamId = %u, external = %u", peerId, srcId, dstId, streamId, external);
                            return false;
                        }
                    }
                }
                else {
                    // is this a parrot talkgroup? if so, clear any remaining frames from the buffer
                    lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(dstId);
                    if (tg.config().parrot()) {                    
                        m_parrotFramesReady = false;
                        if (m_parrotFrames.size() > 0) {
                            for (auto& pkt : m_parrotFrames) {
                                if (std::get<0>(pkt) != nullptr) {
                                    delete std::get<0>(pkt);
                                }
                            }
                            m_parrotFrames.clear();
                        }
                    }

                    // this is a new call stream
                    RxStatus status = RxStatus();
                    status.callStartTime = pktTime;
                    status.srcId = srcId;
                    status.dstId = dstId;
                    status.streamId = streamId;
                    m_status[dstId] = status;

                    LogMessage(LOG_NET, "NXDN, Call Start, peer = %u, srcId = %u, dstId = %u, streamId = %u, external = %u", peerId, srcId, dstId, streamId, external);

                    m_network->m_callInProgress = true;
                }
            }
        }

        // is this a parrot talkgroup?
        lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(dstId);
        if (tg.config().parrot()) {
            uint8_t *copy = new uint8_t[len];
            ::memcpy(copy, buffer, len);
            m_parrotFrames.push_back(std::make_tuple(copy, len, pktSeq, streamId));
        }

        // repeat traffic to the connected peers
        if (m_network->m_peers.size() > 0U) {
            uint32_t i = 0U;
            for (auto peer : m_network->m_peers) {
                if (peerId != peer.first) {
                    // is this peer ignored?
                    if (!isPeerPermitted(peer.first, lc, messageType, streamId)) {
                        continue;
                    }

                    // every 5 peers flush the queue
                    if (i % 5U == 0U) {
                        m_network->m_frameQueue->flushQueue();
                    }

                    uint8_t outboundPeerBuffer[len];
                    ::memset(outboundPeerBuffer, 0x00U, len);
                    ::memcpy(outboundPeerBuffer, buffer, len);

                    // perform TGID route rewrites if configured
                    routeRewrite(outboundPeerBuffer, peer.first, messageType, dstId);

                    m_network->writePeer(peer.first, { NET_FUNC_PROTOCOL, NET_PROTOCOL_SUBFUNC_NXDN }, outboundPeerBuffer, len, pktSeq, streamId, true);
                    if (m_network->m_debug) {
                        LogDebug(LOG_NET, "NXDN, srcPeer = %u, dstPeer = %u, messageType = $%02X, srcId = %u, dstId = %u, len = %u, pktSeq = %u, streamId = %u, external = %u", 
                            peerId, peer.first, messageType, srcId, dstId, len, pktSeq, streamId, external);
                    }

                    if (!m_network->m_callInProgress)
                        m_network->m_callInProgress = true;
                    i++;
                }
            }
            m_network->m_frameQueue->flushQueue();
        }

        // repeat traffic to external peers
        if (m_network->m_host->m_peerNetworks.size() > 0U && !tg.config().parrot()) {
            for (auto peer : m_network->m_host->m_peerNetworks) {
                uint32_t dstPeerId = peer.second->getPeerId();

                // don't try to repeat traffic to the source peer...if this traffic
                // is coming from a external peer
                if (dstPeerId != peerId) {
                    // is this peer ignored?
                    if (!isPeerPermitted(dstPeerId, lc, messageType, streamId, true)) {
                        continue;
                    }

                    // check if the source peer is blocked from sending to this peer
                    if (peer.second->checkBlockedPeer(peerId)) {
                        continue;
                    }

                    uint8_t outboundPeerBuffer[len];
                    ::memset(outboundPeerBuffer, 0x00U, len);
                    ::memcpy(outboundPeerBuffer, buffer, len);

                    // perform TGID route rewrites if configured
                    routeRewrite(outboundPeerBuffer, dstPeerId, messageType, dstId);

                    peer.second->writeMaster({ NET_FUNC_PROTOCOL, NET_PROTOCOL_SUBFUNC_NXDN }, outboundPeerBuffer, len, pktSeq, streamId);
                    if (m_network->m_debug) {
                        LogDebug(LOG_NET, "NXDN, srcPeer = %u, dstPeer = %u, messageType = $%02X, srcId = %u, dstId = %u, len = %u, pktSeq = %u, streamId = %u, external = %u", 
                            peerId, dstPeerId, messageType, srcId, dstId, len, pktSeq, streamId, external);
                    }

                    if (!m_network->m_callInProgress)
                        m_network->m_callInProgress = true;
                }
            }
        }

        return true;
    }

    return false;
}

/// <summary>
/// Process a grant request frame from the network.
/// </summary>
/// <param name="srcId"></param>
/// <param name="dstId"></param>
/// <param name="unitToUnit"></param>
/// <param name="peerId">Peer ID</param>
/// <param name="pktSeq"></param>
/// <param name="streamId">Stream ID</param>
/// <returns></returns>
bool TagNXDNData::processGrantReq(uint32_t srcId, uint32_t dstId, bool unitToUnit, uint32_t peerId, uint16_t pktSeq, uint32_t streamId)
{
    // bryanb: TODO TODO TODO
    return false;
}

/// <summary>
/// Helper to playback a parrot frame to the network.
/// </summary>
void TagNXDNData::playbackParrot()
{
    if (m_parrotFrames.size() == 0) {
        m_parrotFramesReady = false;
        return;
    }

    auto& pkt = m_parrotFrames[0];
    if (std::get<0>(pkt) != nullptr) {
        // repeat traffic to the connected peers
        for (auto peer : m_network->m_peers) {
            m_network->writePeer(peer.first, { NET_FUNC_PROTOCOL, NET_PROTOCOL_SUBFUNC_NXDN }, std::get<0>(pkt), std::get<1>(pkt), std::get<2>(pkt), std::get<3>(pkt), false);
            if (m_network->m_debug) {
                LogDebug(LOG_NET, "NXDN, parrot, dstPeer = %u, len = %u, pktSeq = %u, streamId = %u", 
                    peer.first, std::get<1>(pkt), std::get<2>(pkt), std::get<3>(pkt));
            }
        }

        delete std::get<0>(pkt);
    }
    Thread::sleep(60);
    m_parrotFrames.pop_front();
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Helper to route rewrite the network data buffer.
/// </summary>
/// <param name="buffer"></param>
/// <param name="peerId">Peer ID</param>
/// <param name="duid"></param>
/// <param name="dstId"></param>
/// <param name="outbound"></param>
void TagNXDNData::routeRewrite(uint8_t* buffer, uint32_t peerId, uint8_t messageType, uint32_t dstId, bool outbound)
{
    uint32_t rewriteDstId = dstId;

    // does the data require route writing?
    if (peerRewrite(peerId, rewriteDstId, outbound)) {
        // rewrite destination TGID in the frame
        __SET_UINT16(rewriteDstId, buffer, 8U);
    }
}

/// <summary>
/// Helper to route rewrite destination ID.
/// </summary>
/// <param name="peerId">Peer ID</param>
/// <param name="dstId"></param>
/// <param name="outbound"></param>
bool TagNXDNData::peerRewrite(uint32_t peerId, uint32_t& dstId, bool outbound)
{
    lookups::TalkgroupRuleGroupVoice tg;
    if (outbound) {
        tg = m_network->m_tidLookup->find(dstId);
    }
    else {
        tg = m_network->m_tidLookup->findByRewrite(peerId, dstId);
    }

    bool rewrote = false;
    if (tg.config().rewriteSize() > 0) {
        std::vector<lookups::TalkgroupRuleRewrite> rewrites = tg.config().rewrite();
        for (auto entry : rewrites) {
            if (entry.peerId() == peerId) {
                if (outbound) {
                    dstId = entry.tgId();
                }
                else {
                    dstId = tg.source().tgId();
                }
                rewrote = true;
                break;
            }
        }
    }

    return rewrote;
}

/// <summary>
/// Helper to determine if the peer is permitted for traffic.
/// </summary>
/// <param name="peerId">Peer ID</param>
/// <param name="lc"></param>
/// <param name="messageType"></param>
/// <param name="streamId">Stream ID</param>
/// <param name="external"></param>
/// <returns></returns>
bool TagNXDNData::isPeerPermitted(uint32_t peerId, lc::RTCH& lc, uint8_t messageType, uint32_t streamId, bool external)
{
    // private calls are always permitted
    if (!lc.getGroup()) {
        return true;
    }

    // is this a group call?
    if (lc.getGroup()) {
        lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(lc.getDstId());

        std::vector<uint32_t> inclusion = tg.config().inclusion();
        std::vector<uint32_t> exclusion = tg.config().exclusion();

        // peer inclusion lists take priority over exclusion lists
        if (inclusion.size() > 0) {
            auto it = std::find(inclusion.begin(), inclusion.end(), peerId);
            if (it == inclusion.end()) {
                return false;
            }
        }
        else {
            if (exclusion.size() > 0) {
                auto it = std::find(exclusion.begin(), exclusion.end(), peerId);
                if (it != exclusion.end()) {
                    return false;
                }
            }
        }

        FNEPeerConnection* connection = nullptr;
        if (peerId > 0 && (m_network->m_peers.find(peerId) != m_network->m_peers.end())) {
            connection = m_network->m_peers[peerId];
        }

        // is this peer a conventional peer?
        if (m_network->m_allowConvSiteAffOverride) {
            if (connection != nullptr) {
                if (connection->isConventionalPeer()) {
                    external = true; // we'll just set the external flag to disable the affiliation check
                                     // for conventional peers
                }
            }
        }

        // is this a TG that requires affiliations to repeat?
        // NOTE: external peers *always* repeat traffic regardless of affiliation
        if (tg.config().affiliated() && !external) {
            uint32_t lookupPeerId = peerId;
            if (connection != nullptr) {
                if (connection->ccPeerId() > 0U)
                    lookupPeerId = connection->ccPeerId();
            }

            // check the affiliations for this peer to see if we can repeat traffic
            lookups::AffiliationLookup* aff = m_network->m_peerAffiliations[lookupPeerId];
            if (aff == nullptr) {
                LogError(LOG_NET, "PEER %u has an invalid affiliations lookup? This shouldn't happen BUGBUG.", lookupPeerId);
                return false; // this will cause no traffic to pass for this peer now...I'm not sure this is good behavior
            }
            else {
                if (!aff->hasGroupAff(lc.getDstId())) {
                    return false;
                }
            }
        }
    }

    return true;
}

/// <summary>
/// Helper to validate the DMR call stream.
/// </summary>
/// <param name="peerId">Peer ID</param>
/// <param name="lc"></param>
/// <param name="messageType"></param>
/// <param name="streamId">Stream ID</param>
/// <returns></returns>
bool TagNXDNData::validate(uint32_t peerId, lc::RTCH& lc, uint8_t messageType, uint32_t streamId)
{
    // is the source ID a blacklisted ID?
    lookups::RadioId rid = m_network->m_ridLookup->find(lc.getSrcId());
    if (!rid.radioDefault()) {
        if (!rid.radioEnabled()) {
            // report error event to InfluxDB
            if (m_network->m_enableInfluxDB) {
                influxdb::QueryBuilder()
                    .meas("call_error_event")
                        .tag("peerId", std::to_string(peerId))
                        .tag("streamId", std::to_string(streamId))
                        .tag("srcId", std::to_string(lc.getSrcId()))
                        .tag("dstId", std::to_string(lc.getDstId()))
                            .field("message", INFLUXDB_ERRSTR_DISABLED_SRC_RID)
                        .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                    .request(m_network->m_influxServer);
            }

            return false;
        }
    }

    // always validate a terminator if the source is valid
    if (messageType == RTCH_MESSAGE_TYPE_TX_REL || messageType == RTCH_MESSAGE_TYPE_TX_REL_EX)
        return true;

    // is this a private call?
    if (!lc.getGroup()) {
        // is the destination ID a blacklisted ID?
        lookups::RadioId rid = m_network->m_ridLookup->find(lc.getDstId());
        if (!rid.radioDefault()) {
            if (!rid.radioEnabled()) {
                // report error event to InfluxDB
                if (m_network->m_enableInfluxDB) {
                    influxdb::QueryBuilder()
                        .meas("call_error_event")
                            .tag("peerId", std::to_string(peerId))
                            .tag("streamId", std::to_string(streamId))
                            .tag("srcId", std::to_string(lc.getSrcId()))
                            .tag("dstId", std::to_string(lc.getDstId()))
                                .field("message", INFLUXDB_ERRSTR_DISABLED_DST_RID)
                            .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                        .request(m_network->m_influxServer);
                }

                return false;
            }
        }

        return true;
    }

    lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(lc.getDstId());

    // check TGID validity
    if (tg.isInvalid()) {
        // report error event to InfluxDB
        if (m_network->m_enableInfluxDB) {
            influxdb::QueryBuilder()
                .meas("call_error_event")
                    .tag("peerId", std::to_string(peerId))
                    .tag("streamId", std::to_string(streamId))
                    .tag("srcId", std::to_string(lc.getSrcId()))
                    .tag("dstId", std::to_string(lc.getDstId()))
                        .field("message", INFLUXDB_ERRSTR_INV_TALKGROUP)
                    .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                .request(m_network->m_influxServer);
        }

        return false;
    }

    if (!tg.config().active()) {
        // report error event to InfluxDB
        if (m_network->m_enableInfluxDB) {
            influxdb::QueryBuilder()
                .meas("call_error_event")
                    .tag("peerId", std::to_string(peerId))
                    .tag("streamId", std::to_string(streamId))
                    .tag("srcId", std::to_string(lc.getSrcId()))
                    .tag("dstId", std::to_string(lc.getDstId()))
                        .field("message", INFLUXDB_ERRSTR_DISABLED_TALKGROUP)
                    .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                .request(m_network->m_influxServer);
        }

        return false;
    }

    return true;
}

/// <summary>
/// Helper to write a deny packet.
/// </summary>
/// <param name="peerId"></param>
/// <param name="srcId"></param>
/// <param name="dstId"></param>
/// <param name="reason"></param>
/// <param name="service"></param>
void TagNXDNData::write_Message_Deny(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t reason, uint8_t service)
{
    std::unique_ptr<lc::RCCH> rcch = nullptr;

    switch (service) {
    case RTCH_MESSAGE_TYPE_VCALL:
        rcch = std::make_unique<lc::rcch::MESSAGE_TYPE_VCALL_CONN>();
        rcch->setMessageType(RTCH_MESSAGE_TYPE_VCALL);
    default:
        return;
    }

    rcch->setCauseResponse(reason);
    rcch->setSrcId(srcId);
    rcch->setDstId(dstId);

    if (m_network->m_verbose) {
        LogMessage(LOG_RF, "NXDN, MSG_DENIAL (Message Denial), reason = $%02X, service = $%02X, srcId = %u, dstId = %u",
            service, srcId, dstId);
    }

    write_Message(peerId, rcch.get());
}

/// <summary>
/// Helper to write a network RCCH.
/// </summary>
/// <param name="peerId"></param>
/// <param name="rcch"></param>
void TagNXDNData::write_Message(uint32_t peerId, lc::RCCH* rcch)
{
    uint8_t data[NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, NXDN_FRAME_LENGTH_BYTES);

    Sync::addNXDNSync(data + 2U);

    // generate the LICH
    channel::LICH lich;
    lich.setRFCT(NXDN_LICH_RFCT_RCCH);
    lich.setFCT(NXDN_LICH_CAC_OUTBOUND);
    lich.setOption(NXDN_LICH_DATA_COMMON);
    lich.setOutbound(true);
    lich.encode(data + 2U);

    uint8_t buffer[NXDN_RCCH_LC_LENGTH_BYTES];
    ::memset(buffer, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES);

    rcch->encode(buffer, NXDN_RCCH_LC_LENGTH_BITS);

    // generate the CAC
    channel::CAC cac;
    cac.setRAN(0U);
    cac.setStructure(NXDN_SR_RCCH_SINGLE);
    cac.setData(buffer);
    cac.encode(data + 2U);

    NXDNUtils::scrambler(data + 2U);
    NXDNUtils::addPostBits(data + 2U);

    lc::RTCH lc = lc::RTCH();
    lc.setMessageType(rcch->getMessageType());
    lc.setSrcId(rcch->getSrcId());
    lc.setDstId(rcch->getDstId());

    uint32_t messageLength = 0U;
    UInt8Array message = m_network->createNXDN_Message(messageLength, lc, data, NXDN_FRAME_LENGTH_BYTES + 2U);
    if (message == nullptr) {
        return;
    }

    uint32_t streamId = m_network->createStreamId();
    m_network->writePeer(peerId, { NET_FUNC_PROTOCOL, NET_PROTOCOL_SUBFUNC_NXDN }, message.get(), messageLength, RTP_END_OF_CALL_SEQ, streamId, false, true);
}
