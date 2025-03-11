// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023-2025 Bryan Biedenkapp, N2PLL
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
#include "network/callhandler/TagNXDNData.h"
#include "HostFNE.h"

using namespace system_clock;
using namespace network;
using namespace network::callhandler;
using namespace nxdn;
using namespace nxdn::defines;

#include <cassert>
#include <chrono>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t CALL_COLL_TIMEOUT = 10U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the TagNXDNData class. */

TagNXDNData::TagNXDNData(FNENetwork* network, bool debug) :
    m_network(network),
    m_parrotFrames(),
    m_parrotFramesReady(false),
    m_status(),
   m_debug(debug)
{
    assert(network != nullptr);
}

/* Finalizes a instance of the TagNXDNData class. */

TagNXDNData::~TagNXDNData() = default;

/* Process a data frame from the network. */

bool TagNXDNData::processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint16_t pktSeq, uint32_t streamId, bool external)
{
    hrc::hrc_t pktTime = hrc::now();

    UInt8Array __buffer = std::make_unique<uint8_t[]>(len);
    uint8_t* buffer = __buffer.get();
    ::memset(buffer, 0x00U, len);
    ::memcpy(buffer, data, len);

    uint8_t messageType = data[4U];

    uint32_t srcId = __GET_UINT16(data, 5U);
    uint32_t dstId = __GET_UINT16(data, 8U);

    if (messageType == MessageType::RTCH_DCALL_HDR ||
        messageType == MessageType::RTCH_DCALL_DATA) {
        if (m_network->m_disablePacketData)
            return false;
    }

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
        if (!isPeerPermitted(peerId, lc, messageType, streamId, external)) {
            return false;
        }

        // specifically only check the following logic for end of call, voice or data frames
        if ((messageType == MessageType::RTCH_TX_REL || messageType == MessageType::RTCH_TX_REL_EX) ||
            (messageType == MessageType::RTCH_VCALL || messageType == MessageType::RTCH_DCALL_HDR ||
             messageType == MessageType::RTCH_DCALL_DATA)) {
            // is this the end of the call stream?
            if (messageType == MessageType::RTCH_TX_REL || messageType == MessageType::RTCH_TX_REL_EX) {
                if (srcId == 0U && dstId == 0U) {
                    LogWarning(LOG_NET, "NXDN, invalid TX_REL, peer = %u, srcId = %u, dstId = %u, streamId = %u, external = %u", peerId, srcId, dstId, streamId, external);
                    return false;
                }

                RxStatus status = m_status[dstId];
                uint64_t duration = hrc::diff(pktTime, status.callStartTime);

                auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) {
                    if (x.second.dstId == dstId) {
                        if (x.second.activeCall)
                            return true;
                    }
                    return false;
                });
                if (it != m_status.end()) {
                    m_status[dstId].reset();

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

                    m_network->eraseStreamPktSeq(peerId, streamId);
                    m_network->m_callInProgress = false;
                }
            }

            // is this a new call stream?
            if ((messageType != MessageType::RTCH_TX_REL && messageType != MessageType::RTCH_TX_REL_EX)) {
                if (srcId == 0U && dstId == 0U) {
                    LogWarning(LOG_NET, "NXDN, invalid call, peer = %u, srcId = %u, dstId = %u, streamId = %u, external = %u", peerId, srcId, dstId, streamId, external);
                    return false;
                }

                auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) {
                    if (x.second.dstId == dstId) {
                        if (x.second.activeCall)
                            return true;
                    }
                    return false;
                });
                if (it != m_status.end()) {
                    RxStatus status = m_status[dstId];
                    if (streamId != status.streamId) {
                        if (status.srcId != 0U && status.srcId != srcId) {
                            uint64_t lastPktDuration = hrc::diff(hrc::now(), status.lastPacket);
                            if ((lastPktDuration / 1000) > CALL_COLL_TIMEOUT) {
                                LogWarning(LOG_NET, "NXDN, Call Collision, lasted more then %us with no further updates, forcibly ending call");
                                m_status[dstId].reset();
                                m_network->m_callInProgress = false;
                            }

                            LogWarning(LOG_NET, "NXDN, Call Collision, peer = %u, srcId = %u, dstId = %u, streamId = %u, rxPeer = %u, rxSrcId = %u, rxDstId = %u, rxStreamId = %u, external = %u",
                                peerId, srcId, dstId, streamId, status.peerId, status.srcId, status.dstId, status.streamId, external);
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
                                if (pkt.buffer != nullptr) {
                                    delete[] pkt.buffer;
                                }
                            }
                            m_parrotFrames.clear();
                        }
                    }

                    // this is a new call stream
                    m_status[dstId].callStartTime = pktTime;
                    m_status[dstId].srcId = srcId;
                    m_status[dstId].dstId = dstId;
                    m_status[dstId].streamId = streamId;
                    m_status[dstId].peerId = peerId;
                    m_status[dstId].activeCall = true;

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

            ParrotFrame parrotFrame = ParrotFrame();
            parrotFrame.buffer = copy;
            parrotFrame.bufferLen = len;

            parrotFrame.pktSeq = pktSeq;
            parrotFrame.streamId = streamId;
            parrotFrame.peerId = peerId;

            parrotFrame.srcId = srcId;
            parrotFrame.dstId = dstId;

            m_parrotFrames.push_back(parrotFrame);

            if (m_network->m_parrotOnlyOriginating) {
                return true; // end here because parrot calls should never repeat anywhere
            }
        }

        m_status[dstId].lastPacket = hrc::now();

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

                    UInt8Array __outboundPeerBuffer = std::make_unique<uint8_t[]>(len);
                    uint8_t* outboundPeerBuffer = __outboundPeerBuffer.get();
                    ::memset(outboundPeerBuffer, 0x00U, len);
                    ::memcpy(outboundPeerBuffer, buffer, len);

                    // perform TGID route rewrites if configured
                    routeRewrite(outboundPeerBuffer, peer.first, messageType, dstId);

                    m_network->writePeer(peer.first, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN }, outboundPeerBuffer, len, pktSeq, streamId, true);
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

                    // skip peer if it isn't enabled
                    if (!peer.second->isEnabled()) {
                        continue;
                    }

                    UInt8Array __outboundPeerBuffer = std::make_unique<uint8_t[]>(len);
                    uint8_t* outboundPeerBuffer = __outboundPeerBuffer.get();
                    ::memset(outboundPeerBuffer, 0x00U, len);
                    ::memcpy(outboundPeerBuffer, buffer, len);

                    // perform TGID route rewrites if configured
                    routeRewrite(outboundPeerBuffer, dstPeerId, messageType, dstId);

                    peer.second->writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN }, outboundPeerBuffer, len, pktSeq, streamId);
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

/* Process a grant request frame from the network. */

bool TagNXDNData::processGrantReq(uint32_t srcId, uint32_t dstId, bool unitToUnit, uint32_t peerId, uint16_t pktSeq, uint32_t streamId)
{
    // if we have an Rx status for the destination deny the grant
    auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) {
        if (x.second.dstId == dstId) {
            if (x.second.activeCall)
                return true;
        }
        return false;
    });
    if (it != m_status.end()) {
        return false;
    }

    // is the source ID a blacklisted ID?
    lookups::RadioId rid = m_network->m_ridLookup->find(srcId);
    if (!rid.radioDefault()) {
        if (!rid.radioEnabled()) {
            return false;
        }
    }

    lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(dstId);

    // check TGID validity
    if (tg.isInvalid()) {
        return false;
    }

    if (!tg.config().active()) {
        return false;
    }

    // repeat traffic to the connected peers
    if (m_network->m_peers.size() > 0U) {
        for (auto peer : m_network->m_peers) {
            if (peerId != peer.first) {
                write_Message_Grant(peer.first, srcId, dstId, 4U, !unitToUnit);
            }
        }
    }

    return true;
}

/* Helper to playback a parrot frame to the network. */

void TagNXDNData::playbackParrot()
{
    if (m_parrotFrames.size() == 0) {
        m_parrotFramesReady = false;
        return;
    }

    auto& pkt = m_parrotFrames[0];
    if (pkt.buffer != nullptr) {
        if (m_network->m_parrotOnlyOriginating) {
            m_network->writePeer(pkt.peerId, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN }, pkt.buffer, pkt.bufferLen, pkt.pktSeq, pkt.streamId, false);
            if (m_network->m_debug) {
                LogDebug(LOG_NET, "NXDN, parrot, dstPeer = %u, len = %u, pktSeq = %u, streamId = %u", 
                    pkt.peerId, pkt.bufferLen, pkt.pktSeq, pkt.streamId);
            }
        }
        else {
            // repeat traffic to the connected peers
            for (auto peer : m_network->m_peers) {
                m_network->writePeer(peer.first, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN }, pkt.buffer, pkt.bufferLen, pkt.pktSeq, pkt.streamId, false);
                if (m_network->m_debug) {
                    LogDebug(LOG_NET, "NXDN, parrot, dstPeer = %u, len = %u, pktSeq = %u, streamId = %u", 
                        peer.first, pkt.bufferLen, pkt.pktSeq, pkt.streamId);
                }
            }
        }

        delete[] pkt.buffer;
    }
    Thread::sleep(60);
    m_parrotFrames.pop_front();
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Helper to route rewrite the network data buffer. */

void TagNXDNData::routeRewrite(uint8_t* buffer, uint32_t peerId, uint8_t messageType, uint32_t dstId, bool outbound)
{
    uint32_t rewriteDstId = dstId;

    // does the data require route writing?
    if (peerRewrite(peerId, rewriteDstId, outbound)) {
        // rewrite destination TGID in the frame
        __SET_UINT16(rewriteDstId, buffer, 8U);
    }
}

/* Helper to route rewrite destination ID. */

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

/* Helper to determine if the peer is permitted for traffic. */

bool TagNXDNData::isPeerPermitted(uint32_t peerId, lc::RTCH& lc, uint8_t messageType, uint32_t streamId, bool external)
{
    if (!lc.getGroup()) {
        if (m_network->m_disallowU2U)
            return false;
        if (!m_network->checkU2UDroppedPeer(peerId))
            return true;
        return false;
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

        // peer always send list takes priority over any following affiliation rules
        std::vector<uint32_t> alwaysSend = tg.config().alwaysSend();
        if (alwaysSend.size() > 0) {
            auto it = std::find(alwaysSend.begin(), alwaysSend.end(), peerId);
            if (it != alwaysSend.end()) {
                return true; // skip any following checks and always send traffic
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

        // is this peer a SysView peer?
        if (connection != nullptr) {
            if (connection->isSysView()) {
                external = true; // we'll just set the external flag to disable the affiliation check
                                 // for SysView peers
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
                std::string peerIdentity = m_network->resolvePeerIdentity(lookupPeerId);
                LogError(LOG_NET, "PEER %u (%s) has an invalid affiliations lookup? This shouldn't happen BUGBUG.", lookupPeerId, peerIdentity.c_str());
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

/* Helper to validate the DMR call stream. */

bool TagNXDNData::validate(uint32_t peerId, lc::RTCH& lc, uint8_t messageType, uint32_t streamId)
{
    // is the source ID a blacklisted ID?
    bool rejectUnknownBadCall = false;
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

            // report In-Call Control to the peer sending traffic
            m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN, NET_ICC::REJECT_TRAFFIC, lc.getDstId());
            return false;
        }
    }
    else {
        // if this is a default radio -- and we are rejecting undefined radios
        // report call error
        if (m_network->m_rejectUnknownRID) {
            rejectUnknownBadCall = true;
        }
    }

    // always validate a terminator if the source is valid
    if (messageType == MessageType::RTCH_TX_REL || messageType == MessageType::RTCH_TX_REL_EX)
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

                // report In-Call Control to the peer sending traffic
                m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN, NET_ICC::REJECT_TRAFFIC, lc.getDstId());
                return false;
            }
        }
        else {
            // if this is a default radio -- and we are rejecting undefined radios
            // report call error
            if (m_network->m_rejectUnknownRID) {
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

                LogWarning(LOG_NET, "NXDN, illegal/unknown RID attempted access, srcId = %u, dstId = %u", lc.getSrcId(), lc.getDstId());

                // report In-Call Control to the peer sending traffic
                m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN, NET_ICC::REJECT_TRAFFIC, lc.getDstId());
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

        // report In-Call Control to the peer sending traffic
        m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN, NET_ICC::REJECT_TRAFFIC, lc.getDstId());
        return false;
    }

    // peer always send list takes priority over any following affiliation rules
    bool isAlwaysPeer = false;
    std::vector<uint32_t> alwaysSend = tg.config().alwaysSend();
    if (alwaysSend.size() > 0) {
        auto it = std::find(alwaysSend.begin(), alwaysSend.end(), peerId);
        if (it != alwaysSend.end()) {
            isAlwaysPeer = true; // skip any following checks and always send traffic
            rejectUnknownBadCall = false;
        }
    }

    // fail call if the reject flag is set
    if (rejectUnknownBadCall) {
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

        LogWarning(LOG_NET, "NXDN, illegal/unknown RID attempted access, srcId = %u, dstId = %u", lc.getSrcId(), lc.getDstId());

        // report In-Call Control to the peer sending traffic
        m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN, NET_ICC::REJECT_TRAFFIC, lc.getDstId());
        return false;
    }

    // is the TGID active?
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

        // report In-Call Control to the peer sending traffic
        m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN, NET_ICC::REJECT_TRAFFIC, lc.getDstId());
        return false;
    }

    // always peers can violate the rules...hurray
    if (!isAlwaysPeer) {
        // does the TGID have a permitted RID list?
        if (tg.config().permittedRIDs().size() > 0) {
            // does the transmitting RID have permission?
            std::vector<uint32_t> permittedRIDs = tg.config().permittedRIDs();
            if (std::find(permittedRIDs.begin(), permittedRIDs.end(), lc.getSrcId()) == permittedRIDs.end()) {
                // report error event to InfluxDB
                if (m_network->m_enableInfluxDB) {
                    influxdb::QueryBuilder()
                        .meas("call_error_event")
                            .tag("peerId", std::to_string(peerId))
                            .tag("streamId", std::to_string(streamId))
                            .tag("srcId", std::to_string(lc.getSrcId()))
                            .tag("dstId", std::to_string(lc.getDstId()))
                                .field("message", INFLUXDB_ERRSTR_RID_NOT_PERMITTED)
                            .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                        .request(m_network->m_influxServer);
                }

                // report In-Call Control to the peer sending traffic
                m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN, NET_ICC::REJECT_TRAFFIC, lc.getDstId());
                return false;
            }
        }
    }

    return true;
}

/* Helper to write a grant packet. */

bool TagNXDNData::write_Message_Grant(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t serviceOptions, bool grp)
{
    bool emergency = ((serviceOptions & 0xFFU) & 0x80U) == 0x80U;           // Emergency Flag
    bool encryption = ((serviceOptions & 0xFFU) & 0x40U) == 0x40U;          // Encryption Flag
    uint8_t priority = ((serviceOptions & 0xFFU) & 0x07U);                  // Priority

    std::unique_ptr<lc::rcch::MESSAGE_TYPE_VCALL_CONN> rcch = std::make_unique<lc::rcch::MESSAGE_TYPE_VCALL_CONN>();

    // check the affiliations for this peer to see if we can grant traffic
    lookups::AffiliationLookup* aff = m_network->m_peerAffiliations[peerId];
    if (aff == nullptr) {
        std::string peerIdentity = m_network->resolvePeerIdentity(peerId);
        LogError(LOG_NET, "PEER %u (%s) has an invalid affiliations lookup? This shouldn't happen BUGBUG.", peerId, peerIdentity.c_str());
        return false; // this will cause no traffic to pass for this peer now...I'm not sure this is good behavior
    }
    else {
        if (!aff->hasGroupAff(dstId)) {
            return false;
        }
    }

    rcch->setMessageType(MessageType::RTCH_VCALL);
    rcch->setGrpVchNo(0U);
    rcch->setGroup(grp);
    rcch->setSrcId(srcId);
    rcch->setDstId(dstId);

    rcch->setEmergency(emergency);
    rcch->setEncrypted(encryption);
    rcch->setPriority(priority);

    if (m_network->m_verbose) {
        LogMessage(LOG_NET, "NXDN, %s, emerg = %u, encrypt = %u, prio = %u, chNo = %u, srcId = %u, dstId = %u, peerId = %u",
            rcch->toString().c_str(), rcch->getEmergency(), rcch->getEncrypted(), rcch->getPriority(), rcch->getGrpVchNo(), rcch->getSrcId(), rcch->getDstId(), peerId);
    }

    write_Message(peerId, rcch.get());
    return true;
}

/* Helper to write a deny packet. */

void TagNXDNData::write_Message_Deny(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t reason, uint8_t service)
{
    std::unique_ptr<lc::RCCH> rcch = nullptr;

    switch (service) {
    case MessageType::RTCH_VCALL:
        rcch = std::make_unique<lc::rcch::MESSAGE_TYPE_VCALL_CONN>();
        rcch->setMessageType(MessageType::RTCH_VCALL);
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

/* Helper to write a network RCCH. */

void TagNXDNData::write_Message(uint32_t peerId, lc::RCCH* rcch)
{
    uint8_t data[NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, NXDN_FRAME_LENGTH_BYTES);

    Sync::addNXDNSync(data + 2U);

    // generate the LICH
    channel::LICH lich;
    lich.setRFCT(RFChannelType::RCCH);
    lich.setFCT(FuncChannelType::CAC_OUTBOUND);
    lich.setOption(ChOption::DATA_COMMON);
    lich.setOutbound(true);
    lich.encode(data + 2U);

    uint8_t buffer[NXDN_RCCH_LC_LENGTH_BYTES];
    ::memset(buffer, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES);

    rcch->encode(buffer, NXDN_RCCH_LC_LENGTH_BITS);

    // generate the CAC
    channel::CAC cac;
    cac.setRAN(0U);
    cac.setStructure(ChStructure::SR_RCCH_SINGLE);
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
    m_network->writePeer(peerId, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN }, message.get(), messageLength, RTP_END_OF_CALL_SEQ, streamId, false);
}
