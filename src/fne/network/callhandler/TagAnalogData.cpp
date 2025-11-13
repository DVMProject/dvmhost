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
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the TagAnalogData class. */

TagAnalogData::TagAnalogData(FNENetwork* network, bool debug) :
    m_network(network),
    m_parrotFrames(),
    m_parrotFramesReady(false),
    m_parrotPlayback(false),
    m_lastParrotPeerId(0U),
    m_lastParrotSrcId(0U),
    m_lastParrotDstId(0U),
    m_status(),
    m_debug(debug)
{
    assert(network != nullptr);
}

/* Finalizes a instance of the TagAnalogData class. */

TagAnalogData::~TagAnalogData() = default;

/* Process a data frame from the network. */

bool TagAnalogData::processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint32_t ssrc, uint16_t pktSeq, uint32_t streamId, bool fromUpstream)
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
        if (!isPeerPermitted(peerId, analogData, streamId, fromUpstream)) {
            return false;
        }

        // is this the end of the call stream?
        if (frameType == AudioFrameType::TERMINATOR) {
            if (srcId == 0U && dstId == 0U) {
                LogWarning(LOG_NET, "Analog, invalid TERMINATOR, peer = %u, ssrc = %u, srcId = %u, dstId = %u, slot = %u, streamId = %u, fromUpstream = %u", peerId, ssrc, srcId, dstId, streamId, fromUpstream);
                return false;
            }

            RxStatus status = m_status[dstId];
            uint64_t duration = hrc::diff(pktTime, status.callStartTime);

            auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair& x) {
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
                if (tg.config().parrot() && !m_parrotPlayback) {
                    if (m_parrotFrames.size() > 0) {
                        m_parrotFramesReady = true;
                        LogInfoEx(LOG_NET, "Analog, Parrot Playback will Start, peer = %u, ssrc = %u, srcId = %u", peerId, ssrc, srcId);
                        m_network->m_parrotDelayTimer.start();
                    }
                }

                #define CALL_END_LOG "Analog, Call End, peer = %u, ssrc = %u, srcId = %u, dstId = %u, duration = %u, streamId = %u, fromUpstream = %u", peerId, ssrc, srcId, dstId, duration / 1000, streamId, fromUpstream
                if (m_network->m_logUpstreamCallStartEnd && fromUpstream)
                    LogInfoEx(LOG_PEER, CALL_END_LOG);
                else if (!fromUpstream)
                    LogInfoEx(LOG_MASTER, CALL_END_LOG);

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
            }
        }

        // is this a new call stream?
        if (frameType == AudioFrameType::VOICE_START) {
            if (srcId == 0U && dstId == 0U) {
                LogWarning(LOG_NET, "Analog, invalid call, peer = %u, ssrc = %u, srcId = %u, dstId = %u, streamId = %u, fromUpstream = %u", peerId, ssrc, srcId, dstId, streamId, fromUpstream);
                return false;
            }

            auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair& x) {
                if (x.second.dstId == dstId) {
                    if (x.second.activeCall)
                        return true;
                }
                return false;
            });
            if (it != m_status.end()) {
                RxStatus status = it->second;

                // is the call being taken over?
                if (status.callTakeover) {
                    LogInfoEx((fromUpstream) ? LOG_PEER : LOG_MASTER, "Analog, Call Source Switched (Takeover), peer = %u, ssrc = %u, srcId = %u, dstId = %u, streamId = %u, rxPeer = %u, rxSrcId = %u, rxDstId = %u, rxStreamId = %u, fromUpstream = %u",
                        peerId, ssrc, srcId, dstId, streamId, status.peerId, status.srcId, status.dstId, status.streamId, fromUpstream);

                    m_status.lock(false);
                    m_status[dstId].streamId = streamId;
                    m_status[dstId].srcId = srcId;
                    m_status[dstId].ssrc = ssrc;
                    m_status[dstId].callTakeover = false; // reset takeover flag
                    m_status.unlock();

                    status = m_status[dstId];
                }

                if (streamId != status.streamId) {
                    if (status.srcId != 0U && status.srcId != srcId) {
                        bool hasCallPriority = false;

                        // determine if the peer trying to transmit has call priority
                        if (m_network->m_callCollisionTimeout > 0U) {
                            m_network->m_peers.shared_lock();
                            for (auto peer : m_network->m_peers) {
                                if (peerId == peer.first) {
                                    FNEPeerConnection* conn = peer.second;
                                    if (conn != nullptr) {
                                        hasCallPriority = conn->hasCallPriority();
                                        break;
                                    }
                                }
                            }
                            m_network->m_peers.shared_unlock();
                        }

                        // perform standard call collision if the call collision timeout is set *and*
                        //  the peer doesn't have call priority
                        if (m_network->m_callCollisionTimeout > 0U && !hasCallPriority) {
                            uint64_t lastPktDuration = hrc::diff(hrc::now(), status.lastPacket);
                            if ((lastPktDuration / 1000) > m_network->m_callCollisionTimeout) {
                                LogWarning((fromUpstream) ? LOG_PEER : LOG_MASTER, "Analog, Call Collision, lasted more then %us with no further updates, resetting call source", m_network->m_callCollisionTimeout);

                                m_status.lock(false);
                                m_status[dstId].streamId = streamId;
                                m_status[dstId].srcId = srcId;
                                m_status[dstId].ssrc = ssrc;
                                m_status.unlock();
                            }
                            else {
                                LogWarning((fromUpstream) ? LOG_PEER : LOG_MASTER, "Analog, Call Collision, peer = %u, ssrc = %u, srcId = %u, dstId = %u, streamId = %u, rxPeer = %u, rxSrcId = %u, rxDstId = %u, rxStreamId = %u, fromUpstream = %u",
                                    peerId, ssrc, srcId, dstId, streamId, status.peerId, status.srcId, status.dstId, status.streamId, fromUpstream);
                                return false;
                            }
                        } else {
                            if (hasCallPriority && !m_network->m_disallowInCallCtrl) {
                                LogInfoEx((fromUpstream) ? LOG_PEER : LOG_MASTER, "Analog, Call Source Switched (Priority), peer = %u, ssrc = %u, srcId = %u, dstId = %u, streamId = %u, rxPeer = %u, rxSrcId = %u, rxDstId = %u, rxStreamId = %u, fromUpstream = %u",
                                    peerId, ssrc, srcId, dstId, streamId, status.peerId, status.srcId, status.dstId, status.streamId, fromUpstream);

                                // since we're gonna switch over the stream and interrupt the current call inprogress lets try to ICC the transmitting peer
                                if (m_network->isPeerLocal(m_status[dstId].ssrc))
                                    m_network->writePeerICC(m_status[dstId].peerId, m_status[dstId].streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG, NET_ICC::REJECT_TRAFFIC, dstId, 0U, true, false,
                                        m_status[dstId].ssrc);
                                else
                                    m_network->writePeerICC(m_status[dstId].peerId, m_status[dstId].streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG, NET_ICC::REJECT_TRAFFIC, dstId, 0U, true, true,
                                        m_status[dstId].ssrc);
                            }

                            m_status.lock(false);
                            m_status[dstId].streamId = streamId;
                            m_status[dstId].srcId = srcId;
                            m_status[dstId].ssrc = ssrc;
                            m_status.unlock();
                        }
                    }
                }
            }
            else {
                // is this a parrot talkgroup? if so, clear any remaining frames from the buffer
                lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(dstId);
                if (tg.config().parrot() && !m_parrotPlayback) {
                    m_parrotFramesReady = false;
                    if (m_parrotFrames.size() > 0) {
                        m_parrotFrames.lock(false);
                        for (auto& pkt : m_parrotFrames) {
                            if (pkt.buffer != nullptr) {
                                delete[] pkt.buffer;
                            }
                        }
                        m_parrotFrames.unlock();
                        m_parrotFrames.clear();
                    }
                }

                // this is a new call stream
                m_status.lock(false);
                m_status[dstId].callStartTime = pktTime;
                m_status[dstId].srcId = srcId;
                m_status[dstId].dstId = dstId;
                m_status[dstId].streamId = streamId;
                m_status[dstId].peerId = peerId;
                m_status[dstId].ssrc = ssrc;
                m_status[dstId].activeCall = true;
                m_status.unlock();

                #define CALL_START_LOG "Analog, Call Start, peer = %u, ssrc = %u, srcId = %u, dstId = %u, streamId = %u, fromUpstream = %u", peerId, ssrc, srcId, dstId, streamId, fromUpstream
                if (m_network->m_logUpstreamCallStartEnd && fromUpstream)
                    LogInfoEx(LOG_PEER, CALL_START_LOG);
                else if (!fromUpstream)
                    LogInfoEx(LOG_MASTER, CALL_START_LOG);
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

        m_status.lock(false);
        m_status[dstId].lastPacket = hrc::now();
        m_status.unlock();

        /*
        ** MASTER TRAFFIC
        */

        // repeat traffic to nodes peered to us as master
        if (m_network->m_peers.size() > 0U) {
            uint32_t i = 0U;
            udp::BufferQueue queue = udp::BufferQueue();

            m_network->m_peers.shared_lock();
            for (auto peer : m_network->m_peers) {
                if (peer.second == nullptr)
                    continue;
                if (peerId != peer.first) {
                    if (ssrc == peer.first) {
                        // skip the peer if it is the source peer
                        continue;
                    }

                    // is this peer ignored?
                    if (!isPeerPermitted(peer.first, analogData, streamId)) {
                        continue;
                    }

                    // every MAX_QUEUED_PEER_MSGS peers flush the queue
                    if (i % MAX_QUEUED_PEER_MSGS == 0U) {
                        m_network->m_frameQueue->flushQueue(&queue);
                    }

                    DECLARE_UINT8_ARRAY(outboundPeerBuffer, len);
                    ::memcpy(outboundPeerBuffer, buffer, len);

                    // perform TGID route rewrites if configured
                    routeRewrite(outboundPeerBuffer, peer.first, dstId);

                    m_network->writePeerQueue(&queue, peer.first, ssrc, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG }, outboundPeerBuffer, len, pktSeq, streamId);
                    if (m_network->m_debug) {
                        LogDebugEx(LOG_ANALOG, "TagAnalogData::processFrame()", "Master, ssrc = %u, srcPeer = %u, dstPeer = %u, seqNo = %u, srcId = %u, dstId = %u, len = %u, pktSeq = %u, stream = %u, fromUpstream = %u", 
                            ssrc, peerId, peer.first, seqNo, srcId, dstId, len, pktSeq, streamId, fromUpstream);
                    }

                    i++;
                }
            }
            m_network->m_frameQueue->flushQueue(&queue);
            m_network->m_peers.shared_unlock();
        }

        /*
        ** PEER TRAFFIC (e.g. upstream networks this FNE is peered to)
        */

        // repeat traffic to master nodes we have connected to as a peer
        if (m_network->m_host->m_peerNetworks.size() > 0U && !tg.config().parrot()) {
            for (auto peer : m_network->m_host->m_peerNetworks) {
                uint32_t dstPeerId = peer.second->getPeerId();

                // don't try to repeat traffic to the source peer...if this traffic
                // is coming from a neighbor FNE peer
                if (dstPeerId != peerId) {
                    if (ssrc == dstPeerId) {
                        // skip the peer if it is the source peer
                        continue;
                    }

                    // skip peer if it isn't enabled
                    if (!peer.second->isEnabled()) {
                        continue;
                    }

                    // is this peer ignored?
                    if (!isPeerPermitted(dstPeerId, analogData, streamId, true)) {
                        continue;
                    }

                    DECLARE_UINT8_ARRAY(outboundPeerBuffer, len);
                    ::memcpy(outboundPeerBuffer, buffer, len);

                    // perform TGID route rewrites if configured
                    routeRewrite(outboundPeerBuffer, dstPeerId, dstId);

                    // are we a replica peer?
                    if (peer.second->isReplica())
                        peer.second->writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG }, outboundPeerBuffer, len, pktSeq, streamId, false, 0U, ssrc);
                    else
                        peer.second->writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG }, outboundPeerBuffer, len, pktSeq, streamId);
                    if (m_network->m_debug) {
                        LogDebugEx(LOG_ANALOG, "TagAnalogData::processFrame()", "Peers, ssrc = %u, srcPeer = %u, dstPeer = %u, seqNo = %u, srcId = %u, dstId = %u, len = %u, pktSeq = %u, stream = %u, fromUpstream = %u", 
                            ssrc, peerId, dstPeerId, seqNo, srcId, dstId, len, pktSeq, streamId, fromUpstream);
                    }
                }
            }
        }

        return true;
    }

    return false;
}

/* Helper to trigger a call takeover from a In-Call control event. */

void TagAnalogData::triggerCallTakeover(uint32_t dstId)
{
    auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair& x) {
        if (x.second.dstId == dstId) {
            if (x.second.activeCall)
                return true;
        }
        return false;
    });
    if (it != m_status.end()) {
        m_status.lock(false);
        m_status[dstId].callTakeover = true;
        m_status.unlock();
    }
}

/* Helper to playback a parrot frame to the network. */

void TagAnalogData::playbackParrot()
{
    if (m_parrotFrames.size() == 0) {
        m_parrotFramesReady = false;
        m_parrotPlayback = false;
        return;
    }

    m_parrotPlayback = true;

    auto& pkt = m_parrotFrames[0];
    m_parrotFrames.lock();
    if (pkt.buffer != nullptr) {
        m_lastParrotPeerId = pkt.peerId;
        m_lastParrotSrcId = pkt.srcId;
        m_lastParrotDstId = pkt.dstId;

        if (m_network->m_parrotOnlyOriginating) {
            m_network->writePeer(pkt.peerId, pkt.peerId, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG }, pkt.buffer, pkt.bufferLen, pkt.pktSeq, pkt.streamId);
            if (m_network->m_debug) {
                LogDebugEx(LOG_ANALOG, "TagAnalogData::playbackParrot()", "Parrot, dstPeer = %u, len = %u, pktSeq = %u, streamId = %u", 
                    pkt.peerId, pkt.bufferLen, pkt.pktSeq, pkt.streamId);
            }
        }
        else {
            // repeat traffic to the connected peers
            uint32_t i = 0U;
            udp::BufferQueue queue = udp::BufferQueue();

            m_network->m_peers.shared_lock();
            for (auto peer : m_network->m_peers) {
                // every MAX_QUEUED_PEER_MSGS peers flush the queue
                if (i % MAX_QUEUED_PEER_MSGS == 0U) {
                    m_network->m_frameQueue->flushQueue(&queue);
                }

                m_network->writePeerQueue(&queue, peer.first, pkt.peerId, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG }, pkt.buffer, pkt.bufferLen, pkt.pktSeq, pkt.streamId);
                if (m_network->m_debug) {
                    LogDebugEx(LOG_ANALOG, "TagAnalogData::playbackParrot()", "Parrot, dstPeer = %u, len = %u, pktSeq = %u, streamId = %u", 
                        peer.first, pkt.bufferLen, pkt.pktSeq, pkt.streamId);
                }

                i++;
            }
            m_network->m_frameQueue->flushQueue(&queue);
            m_network->m_peers.shared_unlock();
        }

        delete[] pkt.buffer;
    }
    Thread::sleep(60);
    m_parrotFrames.unlock();
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

bool TagAnalogData::isPeerPermitted(uint32_t peerId, data::NetData& data, uint32_t streamId, bool fromUpstream)
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

    // is this peer a replica peer?
    if (connection != nullptr) {
        if (connection->isReplica()) {
            return true; // replica peers are *always* allowed to receive traffic and no other rules may filter
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
                    fromUpstream = true; // we'll just set the fromUpstream flag to disable the affiliation check
                                         // for conventional peers
                }
            }
        }

        // is this peer a SysView peer?
        if (connection != nullptr) {
            if (connection->isSysView()) {
                fromUpstream = true; // we'll just set the fromUpstream flag to disable the affiliation check
                                     // for SysView peers
            }
        }

        // is this a TG that requires affiliations to repeat?
        // NOTE: neighbor FNE peers *always* repeat traffic regardless of affiliation
        if (tg.config().affiliated() && !fromUpstream) {
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

            if (m_network->m_logDenials)
                LogError(LOG_ANALOG, INFLUXDB_ERRSTR_DISABLED_SRC_RID ", peer = %u, srcId = %u, dstId = %u", peerId, data.getSrcId(), data.getDstId());

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

                if (m_network->m_logDenials)
                    LogError(LOG_ANALOG, INFLUXDB_ERRSTR_DISABLED_DST_RID ", peer = %u, srcId = %u, dstId = %u", peerId, data.getSrcId(), data.getDstId());

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
                                .field("message", std::string(INFLUXDB_ERRSTR_ILLEGAL_RID_ACCESS))
                            .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                        .requestAsync(m_network->m_influxServer);
                }

                if (m_network->m_logDenials)
                    LogWarning(LOG_ANALOG, INFLUXDB_ERRSTR_ILLEGAL_RID_ACCESS ", srcId = %u, dstId = %u", data.getSrcId(), data.getDstId());

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

            if (m_network->m_logDenials)
                LogError(LOG_ANALOG, INFLUXDB_ERRSTR_INV_TALKGROUP ", peer = %u, srcId = %u, dstId = %u", peerId, data.getSrcId(), data.getDstId());

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
                            .field("message", std::string(INFLUXDB_ERRSTR_ILLEGAL_RID_ACCESS))
                        .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                    .requestAsync(m_network->m_influxServer);
            }

            if (m_network->m_logDenials)
                LogWarning(LOG_ANALOG, INFLUXDB_ERRSTR_ILLEGAL_RID_ACCESS ", srcId = %u, dstId = %u", data.getSrcId(), data.getDstId());

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

            if (m_network->m_logDenials)
                LogError(LOG_ANALOG, INFLUXDB_ERRSTR_DISABLED_TALKGROUP ", peer = %u, srcId = %u, dstId = %u", peerId, data.getSrcId(), data.getDstId());

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

                    if (m_network->m_logDenials)
                        LogError(LOG_ANALOG, INFLUXDB_ERRSTR_RID_NOT_PERMITTED ", peer = %u, srcId = %u, dstId = %u", peerId, data.getSrcId(), data.getDstId());

                    // report In-Call Control to the peer sending traffic
                    m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG, NET_ICC::REJECT_TRAFFIC, data.getDstId());
                    return false;
                }
            }
        }
    }

    return true;
}
