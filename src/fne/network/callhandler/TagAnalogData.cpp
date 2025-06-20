// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/Defines.h"
#include "common/analog/AnalogDefines.h"
#include "common/analog/data/NetData.h"
#include "common/Clock.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "network/FNENetwork.h"
#include "network/callhandler/TagAnalogData.h"
#include "HostFNE.h"

using namespace system_clock;
using namespace network;
using namespace network::callhandler;
using namespace network::callhandler::packetdata;
using namespace analog;
using namespace analog::defines;

#include <cassert>
#include <chrono>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t CALL_COLL_TIMEOUT = 10U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the TagAnalogData class. */

TagAnalogData::TagAnalogData(FNENetwork* network, bool debug) :
    m_network(network),
    m_parrotFrames(),
    m_parrotFramesReady(false),
    m_status(),
    m_debug(debug)
{
    assert(network != nullptr);
}

/* Finalizes a instance of the TagAnalogData class. */

TagAnalogData::~TagAnalogData() = default;

/* Process a data frame from the network. */

bool TagAnalogData::processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint32_t ssrc, uint16_t pktSeq, uint32_t streamId, bool external)
{
    hrc::hrc_t pktTime = hrc::now();

    DECLARE_UINT8_ARRAY(buffer, len);
    ::memcpy(buffer, data, len);

    uint8_t seqNo = data[4U];

    uint32_t srcId = GET_UINT24(data, 5U);
    uint32_t dstId = GET_UINT24(data, 8U);

    bool individual = (data[15] & 0x40U) == 0x40U;

    AudioFrameType::E frameType = (AudioFrameType::E)(data[15U] & 0x0FU);

    data::NetData analogData;
    analogData.setSeqNo(seqNo);
    analogData.setSrcId(srcId);
    analogData.setDstId(dstId);
    analogData.setFrameType(frameType);

    analogData.setAudio(data + 20U);

    uint8_t frame[AUDIO_SAMPLES_LENGTH_BYTES];
    analogData.getAudio(frame);

    // perform TGID route rewrites if configured
    routeRewrite(buffer, peerId, dstId, false);
    dstId = GET_UINT24(buffer, 8U);

    // is the stream valid?
    if (validate(peerId, analogData, streamId)) {
        // is this peer ignored?
        if (!isPeerPermitted(peerId, analogData, streamId, external)) {
            return false;
        }

        // is this the end of the call stream?
        if (frameType == AudioFrameType::TERMINATOR) {
            if (srcId == 0U && dstId == 0U) {
                LogWarning(LOG_NET, "Analog, invalid TERMINATOR, peer = %u, ssrc = %u, srcId = %u, dstId = %u, slot = %u, streamId = %u, external = %u", peerId, ssrc, srcId, dstId, streamId, external);
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
                        LogMessage(LOG_NET, "Analog, Parrot Playback will Start, peer = %u, ssrc = %u, srcId = %u", peerId, ssrc, srcId);
                        m_network->m_parrotDelayTimer.start();
                    }
                }

                LogMessage(LOG_NET, "Analog, Call End, peer = %u, ssrc = %u, srcId = %u, dstId = %u, duration = %u, streamId = %u, external = %u",
                            peerId, ssrc, srcId, dstId, duration / 1000, streamId, external);

                // report call event to InfluxDB
                if (m_network->m_enableInfluxDB) {
                    influxdb::QueryBuilder()
                        .meas("call_event")
                            .tag("peerId", std::to_string(peerId))
                            .tag("mode", "Analog")
                            .tag("streamId", std::to_string(streamId))
                            .tag("srcId", std::to_string(srcId))
                            .tag("dstId", std::to_string(dstId))
                                .field("duration", duration)
                            .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                        .requestAsync(m_network->m_influxServer);
                }

                m_network->eraseStreamPktSeq(peerId, streamId);
                m_network->m_callInProgress = false;
            }
        }

        // is this a new call stream?
        if (frameType == AudioFrameType::VOICE_START) {
            if (srcId == 0U && dstId == 0U) {
                LogWarning(LOG_NET, "Analog, invalid call, peer = %u, ssrc = %u, srcId = %u, dstId = %u, streamId = %u, external = %u", peerId, ssrc, srcId, dstId, streamId, external);
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
                RxStatus status = it->second;
                if (streamId != status.streamId) {
                    if (status.srcId != 0U && status.srcId != srcId) {
                        uint64_t lastPktDuration = hrc::diff(hrc::now(), status.lastPacket);
                        if ((lastPktDuration / 1000) > CALL_COLL_TIMEOUT) {
                            LogWarning(LOG_NET, "Analog, Call Collision, lasted more then %us with no further updates, forcibly ending call");
                            m_status[dstId].reset();
                            m_network->m_callInProgress = false;
                        }

                        LogWarning(LOG_NET, "Analog, Call Collision, peer = %u, ssrc = %u, srcId = %u, dstId = %u, streamId = %u, rxPeer = %u, rxSrcId = %u, rxDstId = %u, rxStreamId = %u, external = %u",
                            peerId, ssrc, srcId, dstId, streamId, status.peerId, status.srcId, status.dstId, status.streamId, external);
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
                // bryanb: this could be problematic and is naive, if a dstId appears on both slots (which shouldn't happen)
                m_status[dstId].callStartTime = pktTime;
                m_status[dstId].srcId = srcId;
                m_status[dstId].dstId = dstId;
                m_status[dstId].streamId = streamId;
                m_status[dstId].peerId = peerId;
                m_status[dstId].activeCall = true;

                LogMessage(LOG_NET, "Analog, Call Start, peer = %u, ssrc = %u, srcId = %u, dstId = %u, streamId = %u, external = %u", peerId, ssrc, srcId, dstId, streamId, external);

                m_network->m_callInProgress = true;
            }
        }

        // is this a parrot talkgroup?
        lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(dstId);
        if (tg.config().parrot()) {
            uint8_t* copy = new uint8_t[len];
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
                    if (ssrc == peer.first) {
                        // skip the peer if it is the source peer
                        continue;
                    }

                    // is this peer ignored?
                    if (!isPeerPermitted(peer.first, analogData, streamId)) {
                        continue;
                    }

                    // every 5 peers flush the queue
                    if (i % 5U == 0U) {
                        m_network->m_frameQueue->flushQueue();
                    }

                    DECLARE_UINT8_ARRAY(outboundPeerBuffer, len);
                    ::memcpy(outboundPeerBuffer, buffer, len);

                    // perform TGID route rewrites if configured
                    routeRewrite(outboundPeerBuffer, peer.first, dstId);

                    m_network->writePeer(peer.first, ssrc, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG }, outboundPeerBuffer, len, pktSeq, streamId, true);
                    if (m_network->m_debug) {
                        LogDebug(LOG_NET, "Analog, ssrc = %u, srcPeer = %u, dstPeer = %u, seqNo = %u, srcId = %u, dstId = %u, len = %u, pktSeq = %u, stream = %u, external = %u", 
                            ssrc, peerId, peer.first, seqNo, srcId, dstId, len, pktSeq, streamId, external);
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
                    if (ssrc == dstPeerId) {
                        // skip the peer if it is the source peer
                        continue;
                    }

                    // is this peer ignored?
                    if (!isPeerPermitted(dstPeerId, analogData, streamId, true)) {
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

                    DECLARE_UINT8_ARRAY(outboundPeerBuffer, len);
                    ::memcpy(outboundPeerBuffer, buffer, len);

                    // perform TGID route rewrites if configured
                    routeRewrite(outboundPeerBuffer, dstPeerId, dstId);

                    // are we a peer link?
                    if (peer.second->isPeerLink())
                        peer.second->writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG }, outboundPeerBuffer, len, pktSeq, streamId, false, false, 0U, ssrc);
                    else
                        peer.second->writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG }, outboundPeerBuffer, len, pktSeq, streamId);
                    if (m_network->m_debug) {
                        LogDebug(LOG_NET, "Analog, ssrc = %u, srcPeer = %u, dstPeer = %u, seqNo = %u, srcId = %u, dstId = %u, len = %u, pktSeq = %u, stream = %u, external = %u", 
                            ssrc, peerId, dstPeerId, seqNo, srcId, dstId, len, pktSeq, streamId, external);
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

/* Helper to playback a parrot frame to the network. */

void TagAnalogData::playbackParrot()
{
    if (m_parrotFrames.size() == 0) {
        m_parrotFramesReady = false;
        return;
    }

    auto& pkt = m_parrotFrames[0];
    if (pkt.buffer != nullptr) {
        if (m_network->m_parrotOnlyOriginating) {
            m_network->writePeer(pkt.peerId, pkt.peerId, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG }, pkt.buffer, pkt.bufferLen, pkt.pktSeq, pkt.streamId, false);
            if (m_network->m_debug) {
                LogDebug(LOG_NET, "Analog, parrot, dstPeer = %u, len = %u, pktSeq = %u, streamId = %u", 
                    pkt.peerId, pkt.bufferLen, pkt.pktSeq, pkt.streamId);
            }
        }
        else {
            // repeat traffic to the connected peers
            for (auto peer : m_network->m_peers) {
                m_network->writePeer(peer.first, pkt.peerId, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG }, pkt.buffer, pkt.bufferLen, pkt.pktSeq, pkt.streamId, false);
                if (m_network->m_debug) {
                    LogDebug(LOG_NET, "Analog, parrot, dstPeer = %u, len = %u, pktSeq = %u, streamId = %u", 
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

void TagAnalogData::routeRewrite(uint8_t* buffer, uint32_t peerId, uint32_t dstId, bool outbound)
{
    uint32_t rewriteDstId = dstId;

    // does the data require route writing?
    if (peerRewrite(peerId, rewriteDstId, outbound)) {
        // rewrite destination TGID in the frame
        SET_UINT24(rewriteDstId, buffer, 8U);
    }
}

/* Helper to route rewrite destination ID and slot. */

bool TagAnalogData::peerRewrite(uint32_t peerId, uint32_t& dstId, bool outbound)
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

bool TagAnalogData::isPeerPermitted(uint32_t peerId, data::NetData& data, uint32_t streamId, bool external)
{
    if (!data.getGroup()) {
        if (m_network->m_disallowU2U)
            return false;
        if (!m_network->checkU2UDroppedPeer(peerId))
            return true;
        return false;
    }

    FNEPeerConnection* connection = nullptr; // bryanb: this is a possible null ref concurrency issue
                                             //     it is possible if the timing is just right to get a valid 
                                             //     connection back initially, and then for it to be deleted
    if (peerId > 0 && (m_network->m_peers.find(peerId) != m_network->m_peers.end())) {
        connection = m_network->m_peers[peerId];
    }

    // is this peer a Peer-Link peer?
    if (connection != nullptr) {
        if (connection->isPeerLink()) {
            return true; // Peer Link peers are *always* allowed to receive traffic and no other rules may filter
                         // these peers
        }
    }

    // is this a group call?
    if (data.getGroup()) {
        lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(data.getDstId());

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
                //LogError(LOG_NET, "PEER %u (%s) has an invalid affiliations lookup? This shouldn't happen BUGBUG.", lookupPeerId, peerIdentity.c_str());
                return false; // this will cause no traffic to pass for this peer now...I'm not sure this is good behavior
            }
            else {
                if (!aff->hasGroupAff(data.getDstId())) {
                    return false;
                }
            }
        }
    }

    return true;
}

/* Helper to validate the analog call stream. */

bool TagAnalogData::validate(uint32_t peerId, data::NetData& data, uint32_t streamId)
{
    // is the source ID a blacklisted ID?
    bool rejectUnknownBadCall = false;
    lookups::RadioId rid = m_network->m_ridLookup->find(data.getSrcId());
    if (!rid.radioDefault()) {
        if (!rid.radioEnabled()) {
            // report error event to InfluxDB
            if (m_network->m_enableInfluxDB) {
                influxdb::QueryBuilder()
                    .meas("call_error_event")
                        .tag("peerId", std::to_string(peerId))
                        .tag("streamId", std::to_string(streamId))
                        .tag("srcId", std::to_string(data.getSrcId()))
                        .tag("dstId", std::to_string(data.getDstId()))
                            .field("message", std::string(INFLUXDB_ERRSTR_DISABLED_SRC_RID))
                        .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                    .requestAsync(m_network->m_influxServer);
            }

            // report In-Call Control to the peer sending traffic
            m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG, NET_ICC::REJECT_TRAFFIC, data.getDstId());
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
    if (data.getFrameType() == AudioFrameType::TERMINATOR)
        return true;

    // is this a private call?
    if (!data.getGroup()) {
        // is the destination ID a blacklisted ID?
        lookups::RadioId rid = m_network->m_ridLookup->find(data.getDstId());
        if (!rid.radioDefault()) {
            if (!rid.radioEnabled()) {
                // report error event to InfluxDB
                if (m_network->m_enableInfluxDB) {
                    influxdb::QueryBuilder()
                        .meas("call_error_event")
                            .tag("peerId", std::to_string(peerId))
                            .tag("streamId", std::to_string(streamId))
                            .tag("srcId", std::to_string(data.getSrcId()))
                            .tag("dstId", std::to_string(data.getDstId()))
                                .field("message", std::string(INFLUXDB_ERRSTR_DISABLED_DST_RID))
                            .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                        .requestAsync(m_network->m_influxServer);
                }

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
                            .tag("srcId", std::to_string(data.getSrcId()))
                            .tag("dstId", std::to_string(data.getDstId()))
                                .field("message", std::string(INFLUXDB_ERRSTR_DISABLED_SRC_RID))
                            .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                        .requestAsync(m_network->m_influxServer);
                }

                LogWarning(LOG_NET, "Analog, illegal/unknown RID attempted access, srcId = %u, dstId = %u", data.getSrcId(), data.getDstId());

                // report In-Call Control to the peer sending traffic
                m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG, NET_ICC::REJECT_TRAFFIC, data.getDstId());
                return false;
            }
        }
    }

    // is this a group call?
    if (data.getGroup()) {
        lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(data.getDstId());
        if (tg.isInvalid()) {
            // report error event to InfluxDB
            if (m_network->m_enableInfluxDB) {
                influxdb::QueryBuilder()
                    .meas("call_error_event")
                        .tag("peerId", std::to_string(peerId))
                        .tag("streamId", std::to_string(streamId))
                        .tag("srcId", std::to_string(data.getSrcId()))
                        .tag("dstId", std::to_string(data.getDstId()))
                            .field("message", std::string(INFLUXDB_ERRSTR_INV_TALKGROUP))
                        .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                    .requestAsync(m_network->m_influxServer);
            }

            // report In-Call Control to the peer sending traffic
            m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG, NET_ICC::REJECT_TRAFFIC, data.getDstId());
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
                        .tag("srcId", std::to_string(data.getSrcId()))
                        .tag("dstId", std::to_string(data.getDstId()))
                            .field("message", std::string(INFLUXDB_ERRSTR_DISABLED_SRC_RID))
                        .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                    .requestAsync(m_network->m_influxServer);
            }

            LogWarning(LOG_NET, "Analog, illegal/unknown RID attempted access, srcId = %u, dstId = %u", data.getSrcId(), data.getDstId());

            // report In-Call Control to the peer sending traffic
            m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG, NET_ICC::REJECT_TRAFFIC, data.getDstId());
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
                        .tag("srcId", std::to_string(data.getSrcId()))
                        .tag("dstId", std::to_string(data.getDstId()))
                            .field("message", std::string(INFLUXDB_ERRSTR_DISABLED_TALKGROUP))
                        .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                    .requestAsync(m_network->m_influxServer);
            }

            // report In-Call Control to the peer sending traffic
            m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG, NET_ICC::REJECT_TRAFFIC, data.getDstId());
            return false;
        }

        // always peers can violate the rules...hurray
        if (!isAlwaysPeer) {
            // does the TGID have a permitted RID list?
            if (tg.config().permittedRIDs().size() > 0) {
                // does the transmitting RID have permission?
                std::vector<uint32_t> permittedRIDs = tg.config().permittedRIDs();
                if (std::find(permittedRIDs.begin(), permittedRIDs.end(), data.getSrcId()) == permittedRIDs.end()) {
                    // report error event to InfluxDB
                    if (m_network->m_enableInfluxDB) {
                        influxdb::QueryBuilder()
                            .meas("call_error_event")
                                .tag("peerId", std::to_string(peerId))
                                .tag("streamId", std::to_string(streamId))
                                .tag("srcId", std::to_string(data.getSrcId()))
                                .tag("dstId", std::to_string(data.getDstId()))
                                    .field("message", std::string(INFLUXDB_ERRSTR_RID_NOT_PERMITTED))
                                .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                            .requestAsync(m_network->m_influxServer);
                    }

                    // report In-Call Control to the peer sending traffic
                    m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG, NET_ICC::REJECT_TRAFFIC, data.getDstId());
                    return false;
                }
            }
        }
    }

    return true;
}
