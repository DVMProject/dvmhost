// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 */
#include "fne/Defines.h"
#include "common/Clock.h"
#include "common/Log.h"
#include "common/Thread.h"
#include "common/Utils.h"
#include "lookups/AffiliationLookup.h"
#include "network/TrafficNetwork.h"
#include "network/callhandler/TagP25P2Data.h"
#include "HostFNE.h"
#include "FNEMain.h"

#include <algorithm>
#include <cassert>
#include <cstring>

using namespace network;
using namespace network::callhandler;
using namespace system_clock;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the TagP25P2Data class. */

TagP25P2Data::TagP25P2Data(TrafficNetwork* network, bool debug) :
    m_network(network),
    m_parrotFrames(),
    m_parrotFramesReady(false),
    m_parrotPlayback(false),
    m_lastParrotPeerId(0U),
    m_lastParrotSrcId(0U),
    m_lastParrotDstId(0U),
    m_status(),
    m_statusPVCall(),
    m_rejectedCallStreams(),
    m_debug(debug)
{
    assert(network != nullptr);
}

/* Process a data frame from the network. */

bool TagP25P2Data::processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint32_t ssrc, uint16_t pktSeq, uint32_t streamId, bool fromUpstream)
{
    using namespace p25::defines;

    hrc::hrc_t pktTime = hrc::now();

    // Header (24), TDMA burst (40), and packet pad (2).
    if (data == nullptr || len < P25_P2_PACKET_LENGTH || data[23U] != 64U) {
        LogError(LOG_NET, "malformed P25 Phase 2 packet, peer = %u, len = %u", peerId, len);
        return false;
    }

    DECLARE_UINT8_ARRAY(buffer, len);
    ::memcpy(buffer, data, len);

    uint32_t srcId = GET_UINT24(buffer, 5U);
    uint32_t dstId = GET_UINT24(buffer, 8U);
    uint8_t slotNo = (buffer[19U] & 0x80U) != 0U ? 2U : 1U;
    const P2_DUID::E duid = (P2_DUID::E)(buffer[19U] & 0x0FU);
    const bool unitToUnit = (buffer[14U] & NET_CTRL_U2U) != 0U;
    const uint8_t wireMacOpcode = buffer[4U];
    const uint16_t scramblerOffset = GET_UINT16(buffer, 20U);

    const bool facch = duid == P2_DUID::FACCH_SCRAMBLED || duid == P2_DUID::FACCH_UNSCRAMBLED;
    const bool sacch = duid == P2_DUID::SACCH_SCRAMBLED || duid == P2_DUID::SACCH_UNSCRAMBLED;

    // network MAC bursts are normalized to outbound/OEMI form by the originating
    // host -- END_PTT and MAC_RELEASE are authoritative call-end indications
    bool macDecoded = false;
    uint8_t macOpcode = wireMacOpcode;
    uint8_t macMCO = P2_MAC_MCO::PDU_NULL;
    if (facch || sacch) {
        p25::lc::LC macControl;
        macControl.setP2ScrambleOffset(scramblerOffset);
        macDecoded = macControl.decodeVCH_MACPDU_OEMI(buffer + 24U, facch);
        if (macDecoded) {
            macOpcode = macControl.getMACPDUOpcode();
            macMCO = macControl.getLCO();

            if (macOpcode != wireMacOpcode) {
                LogWarning(LOG_P25, "P25 Phase 2 Slot %u, MAC opcode mismatch, header = $%02X, decoded = $%02X, streamId = %u",
                    slotNo, wireMacOpcode, macOpcode, streamId);
            }
        }
    }

    routeRewrite(buffer, peerId, dstId, slotNo, false);
    dstId = GET_UINT24(buffer, 8U);
    slotNo = (buffer[19U] & 0x80U) != 0U ? 2U : 1U;

    // is the stream valid?
    if (validate(peerId, srcId, dstId, slotNo, unitToUnit, streamId)) {
        // is this peer ignored?
        if (!isPeerPermitted(peerId, dstId, slotNo, unitToUnit, streamId, fromUpstream)) {
            return false;
        }

        // is this the end of the call stream?
        if (pktSeq == RTP_END_OF_CALL_SEQ || macOpcode == P2_MAC_HEADER_OPCODE::END_PTT ||
            (macDecoded && macMCO == P2_MAC_MCO::MAC_RELEASE)) {
            bool illegalTerm = false;
            if (srcId == 0U && dstId == 0U) {
                LogWarning(LOG_NET, "P25 Phase 2, invalid TERMINATOR, peer = %u, ssrc = %u, srcId = %u, dstId = %u, slot = %u, streamId = %u, fromUpstream = %u", peerId, ssrc, srcId, dstId, slotNo, streamId, fromUpstream);
                illegalTerm = true;
            }

            // if the terminator is illegal, we need to clean up the call state for this stream, to do this
            // we match against the peer ID, SSRC and stream ID
            if (illegalTerm) {
                auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair& x) {
                    if (x.second.peerId == peerId && x.second.ssrc == ssrc && x.second.streamId == streamId &&
                        x.second.slotNo == slotNo) {
                        if (x.second.activeCall)
                            return true;
                    }
                    return false;
                });
                if (it == m_status.end()) {
                    return false;
                }

                // determine the destination ID for the call, if it is not set use the key from 
                // the status map
                uint32_t dstId = it->second.dstId;
                if (dstId == 0U) {
                    dstId = it->first;
                }

                lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(dstId);

                m_status.lock(false);
                m_status[dstId].reset();
                m_status.unlock();

                // is this a private call?
                auto pvIt = std::find_if(m_statusPVCall.begin(), m_statusPVCall.end(), [&](StatusMapPair& x) {
                    if (x.second.peerId == peerId && x.second.ssrc == ssrc && x.second.streamId == streamId &&
                        x.second.slotNo == slotNo) {
                        if (x.second.activeCall)
                            return true;
                    }
                    return false;
                });
                if (pvIt != m_statusPVCall.end()) {
                    m_statusPVCall.lock(false);
                    m_statusPVCall[dstId].reset();
                    m_statusPVCall.unlock();
                }

                // clear any rejected call streams for this TG
                m_rejectedCallStreams.lock(false);
                m_rejectedCallStreams[dstId].clear();
                m_rejectedCallStreams.unlock();

                if (!tg.config().parrot()) {
                    TrafficNetwork::MetricsLogging::decrementActiveCalls(m_network);
                }

                m_network->eraseStreamPktSeq(peerId, streamId);
                return false;
            }

            RxStatus status;
            {
                auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair& x) {
                    if (x.second.dstId == dstId && x.second.slotNo == slotNo) {
                        return true;
                    }
                    return false;
                });
                if (it == m_status.end()) {
                    LogError(LOG_NET, "P25 Phase 2, tried to end call for non-existent call in progress?, peer = %u, ssrc = %u, srcId = %u, dstId = %u, slot = %u, streamId = %u, fromUpstream = %u",
                        peerId, ssrc, srcId, dstId, slotNo, streamId, fromUpstream);
                }
                else {
                    status = it->second;
                }
            }

            uint64_t duration = hrc::diff(pktTime, status.callStartTime);

            auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair& x) {
                if (x.second.dstId == dstId && x.second.slotNo == slotNo) {
                    if (x.second.activeCall)
                        return true;
                }
                return false;
            });
            if (it != m_status.end()) {
                m_status.lock(false);
                m_status[dstId].reset();
                m_status.unlock();

                // is this a parrot talkgroup? if so, clear any remaining frames from the buffer
                lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(dstId);
                if (tg.config().parrot() && !m_parrotPlayback) {
                    if (m_parrotFrames.size() > 0) {
                        m_parrotFramesReady = true;
                        LogInfoEx(LOG_NET, "P25 Phase 2, Parrot Playback will Start, peer = %u, ssrc = %u, srcId = %u", peerId, ssrc, srcId);
                        m_network->m_parrotDelayTimer.start();
                    }
                }

                // is this a private call?
                auto it = std::find_if(m_statusPVCall.begin(), m_statusPVCall.end(), [&](StatusMapPair& x) {
                    if (x.second.dstId == dstId) {
                        if (x.second.activeCall)
                            return true;
                    }
                    return false;
                });
                if (it != m_statusPVCall.end()) {
                    m_statusPVCall.lock(false);
                    m_statusPVCall[dstId].reset();
                    m_statusPVCall.unlock();

                    m_network->m_globalAff->releaseGrant(dstId);

                    #define PRV_CALL_END_LOG "P25 Phase 2, Private Call End, peer = %u, ssrc = %u, srcId = %u, dstId = %u, slot = %u, duration = %u, streamId = %u, fromUpstream = %u", peerId, ssrc, srcId, dstId, slotNo, duration / 1000, streamId, fromUpstream
                    if (m_network->m_logUpstreamCallStartEnd && fromUpstream)
                        LogInfoEx(LOG_PEER, PRV_CALL_END_LOG);
                    else if (!fromUpstream)
                        LogInfoEx(LOG_MASTER, PRV_CALL_END_LOG);
                }
                else {
                    m_network->m_globalAff->releaseGrant(dstId);

                    #define CALL_END_LOG "P25 Phase 2, Call End, peer = %u, ssrc = %u, srcId = %u, dstId = %u, slot = %u, duration = %u, streamId = %u, fromUpstream = %u", peerId, ssrc, srcId, dstId, slotNo, duration / 1000, streamId, fromUpstream
                    if (m_network->m_logUpstreamCallStartEnd && fromUpstream)
                        LogInfoEx(LOG_PEER, CALL_END_LOG);
                    else if (!fromUpstream)
                        LogInfoEx(LOG_MASTER, CALL_END_LOG);
                }

                // clear any rejected call streams for this TG
                m_rejectedCallStreams.lock(false);
                m_rejectedCallStreams[dstId].clear();
                m_rejectedCallStreams.unlock();

                if (!tg.config().parrot()) {
                    TrafficNetwork::MetricsLogging::decrementActiveCalls(m_network);
                }

                TrafficNetwork::MetricsLogging::incrementCallsProcessed(m_network);

                // report call event to metrics
                TrafficNetwork::MetricsLogging::logCallEvent(m_network, "P25 Phase 2", peerId, streamId, srcId, dstId, duration, slotNo);

                m_network->eraseStreamPktSeq(peerId, streamId);
            } else {
                #define NONCALL_END_LOG "P25 Phase 2, Non-Call Terminator, peer = %u, ssrc = %u, srcId = %u, dstId = %u, slot = %u, streamId = %u, fromUpstream = %u", peerId, ssrc, srcId, dstId, slotNo, streamId, fromUpstream
                if (m_network->m_logUpstreamCallStartEnd && fromUpstream)
                    LogInfoEx(LOG_PEER, NONCALL_END_LOG);
                else if (!fromUpstream)
                    LogInfoEx(LOG_MASTER, NONCALL_END_LOG);

                m_status.lock(false);
                m_status[dstId].callStartTime = pktTime; // because Non-Call Terminators can just happen lets reset the callStartTime to pktTime to prevent insane durations
                m_status.unlock();
            }
        }

        auto it = std::find_if(m_status.begin(), m_status.end(),
            [&](StatusMapPair& entry) {
                return entry.second.activeCall &&
                    entry.second.dstId == dstId &&
                    entry.second.slotNo == slotNo;
            });

        const bool noActiveCall = it == m_status.end();
        const bool pttStart = macOpcode == P2_MAC_HEADER_OPCODE::PTT;
        const bool activeLateEntry = noActiveCall && macDecoded &&
            macOpcode == P2_MAC_HEADER_OPCODE::ACTIVE && (macMCO == P2_MAC_MCO::GROUP || macMCO == P2_MAC_MCO::PRIVATE || macMCO == P2_MAC_MCO::TEL_INT_VCH_USER);
        const bool voiceLateEntry = noActiveCall && (duid == P2_DUID::VTCH_4V || duid == P2_DUID::VTCH_2V);

        // is this a new call stream?
        if (pttStart || activeLateEntry || voiceLateEntry) {
            if (srcId == 0U && dstId == 0U) {
                LogWarning(LOG_NET, "P25 Phase 2, invalid call, peer = %u, ssrc = %u, srcId = %u, dstId = %u, streamId = %u, fromUpstream = %u", peerId, ssrc, srcId, dstId, streamId, fromUpstream);
                return false;
            }

            bool switchOver = (data[14U] & network::NET_CTRL_SWITCH_OVER) == network::NET_CTRL_SWITCH_OVER;

            auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair& x) {
                if (x.second.dstId == dstId && x.second.slotNo == slotNo) {
                    if (x.second.activeCall)
                        return true;
                }
                return false;
            });
            if (it != m_status.end()) {
                RxStatus status = it->second;

                // is the call being taken over?
                if (status.callTakeover) {
                    LogInfoEx((fromUpstream) ? LOG_PEER : LOG_MASTER, "P25 Phase 2, Call Source Switched (Takeover), peer = %u, ssrc = %u, srcId = %u, dstId = %u, slotNo = %u, streamId = %u, rxPeer = %u, rxSrcId = %u, rxDstId = %u, rxSlotNo = %u, rxStreamId = %u, fromUpstream = %u",
                        peerId, ssrc, srcId, dstId, slotNo, streamId, status.peerId, status.srcId, status.dstId, status.slotNo, status.streamId, fromUpstream);

                    m_status.lock(false);
                    m_status[dstId].streamId = streamId;
                    m_status[dstId].srcId = srcId;
                    m_status[dstId].ssrc = ssrc;
                    m_status[dstId].callTakeover = false; // reset takeover flag
                    m_status.unlock();

                    TrafficNetwork::MetricsLogging::incrementCallSwitches(m_network);

                    status = m_status[dstId];
                }

                if (streamId != status.streamId) {
                    // perform TG switch over -- this can happen in special conditions where a TG may rapidly switch
                    // from one source to another (primarily from bridge resources)
                    if (switchOver && status.slotNo == slotNo) {
                        m_status.lock(false);
                        m_status[dstId].streamId = streamId;
                        m_status[dstId].ssrc = ssrc;
                        if (status.srcId == 0U)
                            m_status[dstId].srcId = srcId;
                        if (status.srcId != srcId) {
                            LogInfoEx((fromUpstream) ? LOG_PEER : LOG_MASTER, "P25 Phase 2, Call Source Switched, peer = %u, ssrc = %u, srcId = %u, dstId = %u, slotNo = %u, streamId = %u, rxPeer = %u, rxSrcId = %u, rxDstId = %u, rxSlotNo = %u, rxStreamId = %u, fromUpstream = %u",
                                peerId, ssrc, srcId, dstId, slotNo, streamId, status.peerId, status.srcId, status.dstId, status.slotNo, status.streamId, fromUpstream);
                            m_status[dstId].srcId = srcId;
                        }
                        TrafficNetwork::MetricsLogging::incrementCallSwitches(m_network);
                        m_status.unlock();
                    }
                    else {
                        if (status.srcId != 0U && status.srcId != srcId) {
                            bool hasCallPriority = false;

                            // determine if the peer trying to transmit has call priority
                            if (m_network->m_callCollisionTimeout > 0U) {
                                for (const auto& peer : m_network->peersSnapshot()) {
                                    if (peerId == peer.first) {
                                        TrafficNetwork::PeerConnectionPtr conn = peer.second;
                                        if (conn != nullptr) {
                                            hasCallPriority = conn->hasCallPriority();
                                            break;
                                        }
                                    }
                                }
                            }

                            // perform standard call collision if the call collision timeout is set *and*
                            //  the peer doesn't have call priority
                            if (m_network->m_callCollisionTimeout > 0U && !hasCallPriority) {
                                uint64_t lastPktDuration = hrc::diff(hrc::now(), status.lastPacket);
                                if ((lastPktDuration / 1000) > m_network->m_callCollisionTimeout) {
                                    LogWarning((fromUpstream) ? LOG_PEER : LOG_MASTER, "P25 Phase 2, Call Collision, lasted more then %us with no further updates, resetting call source", m_network->m_callCollisionTimeout);

                                    m_status.lock(false);
                                    m_status[dstId].streamId = streamId;
                                    m_status[dstId].srcId = srcId;
                                    m_status[dstId].ssrc = ssrc;
                                    m_status.unlock();

                                    // because the call stream source has reset, clear any rejected call streams for 
                                    // this TG to allow the new source to transmit
                                    m_rejectedCallStreams.lock(false);
                                    m_rejectedCallStreams[dstId].clear();
                                    m_rejectedCallStreams.unlock();
                                } else {
                                    LogWarning((fromUpstream) ? LOG_PEER : LOG_MASTER, "P25 Phase 2, Call Collision, peer = %u, ssrc = %u, srcId = %u, dstId = %u, slotNo = %u, streamId = %u, rxPeer = %u, rxSrcId = %u, rxDstId = %u, rxSlotNo = %u, rxStreamId = %u, fromUpstream = %u",
                                        peerId, ssrc, srcId, dstId, slotNo, streamId, status.peerId, status.srcId, status.dstId, status.slotNo, status.streamId, fromUpstream);

                                    m_rejectedCallStreams.lock(false);
                                    m_rejectedCallStreams[dstId].push_back(streamId);
                                    m_rejectedCallStreams.unlock();

                                    TrafficNetwork::MetricsLogging::incrementCallCollisions(m_network);
                                    TrafficNetwork::MetricsLogging::logCallCollisionEvent(m_network, peerId, streamId, srcId, dstId, slotNo, status.peerId, status.streamId, status.srcId, status.dstId, status.slotNo);

                                    return false;
                                }
                            } else {
                                if (hasCallPriority && !m_network->m_disallowInCallCtrl) {
                                    LogInfoEx((fromUpstream) ? LOG_PEER : LOG_MASTER, "P25 Phase 2, Call Source Switched (Priority), peer = %u, ssrc = %u, srcId = %u, dstId = %u, slotNo = %u, streamId = %u, rxPeer = %u, rxSrcId = %u, rxDstId = %u, rxSlotNo = %u, rxStreamId = %u, fromUpstream = %u",
                                        peerId, ssrc, srcId, dstId, slotNo, streamId, status.peerId, status.srcId, status.dstId, status.slotNo, status.streamId, fromUpstream);

                                    // since we're gonna switch over the stream and interrupt the current call inprogress lets try to ICC the transmitting peer
                                    if (m_network->isPeerLocal(m_status[dstId].ssrc))
                                        m_network->writePeerICC(m_status[dstId].peerId, m_status[dstId].streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR, NET_ICC::REJECT_TRAFFIC, dstId, 0U, true, false,
                                            m_status[dstId].ssrc);
                                    else
                                        m_network->writePeerICC(m_status[dstId].peerId, m_status[dstId].streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR, NET_ICC::REJECT_TRAFFIC, dstId, 0U, true, true,
                                            m_status[dstId].ssrc);
                                }

                                m_status.lock(false);
                                m_status[dstId].streamId = streamId;
                                m_status[dstId].srcId = srcId;
                                m_status[dstId].ssrc = ssrc;
                                m_status.unlock();

                                TrafficNetwork::MetricsLogging::incrementCallSwitches(m_network);
                            }
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
                // bryanb: this could be problematic and is naive, if a dstId appears on both slots (which shouldn't happen)
                m_status.lock(false);
                m_status[dstId].callStartTime = pktTime;
                m_status[dstId].srcId = srcId;
                m_status[dstId].dstId = dstId;
                m_status[dstId].slotNo = slotNo;
                m_status[dstId].streamId = streamId;
                m_status[dstId].peerId = peerId;
                m_status[dstId].ssrc = ssrc;
                m_status[dstId].activeCall = true;
                m_status.unlock();

                // clear any rejected call streams for this TG
                m_rejectedCallStreams.lock(false);
                m_rejectedCallStreams[dstId].clear();
                m_rejectedCallStreams.unlock();

                if (!tg.config().parrot()) {
                    TrafficNetwork::MetricsLogging::incrementActiveCalls(m_network);
                }

                // is this a private call?
                if (unitToUnit) {
                    m_statusPVCall.lock(false);
                    m_statusPVCall[dstId].callStartTime = pktTime;
                    m_statusPVCall[dstId].srcId = srcId;
                    m_statusPVCall[dstId].dstId = dstId;
                    m_statusPVCall[dstId].slotNo = slotNo;
                    m_statusPVCall[dstId].streamId = streamId;
                    m_statusPVCall[dstId].peerId = peerId;
                    m_statusPVCall[dstId].ssrc = ssrc;
                    m_statusPVCall[dstId].activeCall = true;

                    // find the SSRC of the peer that registered this unit
                    uint32_t regSSRC = m_network->findPeerUnitReg(srcId);
                    m_statusPVCall[dstId].dstPeerId = regSSRC;
                    m_statusPVCall.unlock();

                    if (!m_network->m_globalAff->isGranted(dstId))
                        m_network->m_globalAff->grantCh(dstId, srcId, ssrc, fne_lookups::GRANT_TIMER_TIMEOUT, false);

                    #define PRV_CALL_START_LOG "P25 Phase 2, Private Call Start, peer = %u, ssrc = %u, srcId = %u, dstId = %u, streamId = %u, fromUpstream = %u", peerId, ssrc, srcId, dstId, streamId, fromUpstream
                    if (m_network->m_logUpstreamCallStartEnd && fromUpstream)
                        LogInfoEx(LOG_PEER, PRV_CALL_START_LOG);
                    else if (!fromUpstream)
                        LogInfoEx(LOG_MASTER, PRV_CALL_START_LOG);
                }
                else {
                    if (!m_network->m_globalAff->isGranted(dstId))
                        m_network->m_globalAff->grantCh(dstId, srcId, ssrc, fne_lookups::GRANT_TIMER_TIMEOUT, true);

                    #define CALL_START_LOG "P25 Phase 2, Call Start, peer = %u, ssrc = %u, srcId = %u, dstId = %u, streamId = %u, fromUpstream = %u", peerId, ssrc, srcId, dstId, streamId, fromUpstream
                    if (m_network->m_logUpstreamCallStartEnd && fromUpstream)
                        LogInfoEx(LOG_PEER, CALL_START_LOG);
                    else if (!fromUpstream)
                        LogInfoEx(LOG_MASTER, CALL_START_LOG);
                }
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

            parrotFrame.slotNo = slotNo;

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

        bool noConnectedPeerRepeat = false;
        bool privateCallInProgress = false;

        // is this a private call in-progress?
        if (m_network->m_restrictPVCallToRegOnly) {
            if (unitToUnit) {
                privateCallInProgress = true;
            }

            if (privateCallInProgress) {
                // if we've not determined the destination peer, we have to repeat it everywhere
                if (m_statusPVCall[dstId].dstPeerId == 0U) {
                    noConnectedPeerRepeat = false;
                    privateCallInProgress = false; // trick the system to repeat everywhere
                } else {
                    // if this is a private call, check if the destination peer is one directly connected to us, if not
                    // flag the call so it only repeats to upstream neighbor peers
                    if (m_network->peerCount() > 0U && !noConnectedPeerRepeat) {
                        noConnectedPeerRepeat = true;
                        for (const auto& peer : m_network->peersSnapshot()) {
                            if (peerId != peer.first) {
                                TrafficNetwork::PeerConnectionPtr conn = peer.second;
                                if (conn != nullptr) {
                                    if (conn->peerClass() == PEER_CONN_CLASS_NEIGHBOR) {
                                        continue;
                                    }
                                }

                                if (m_statusPVCall[dstId].dstPeerId == peer.first) {
                                    noConnectedPeerRepeat = false;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }

        /*
        ** MASTER TRAFFIC
        */

        // repeat traffic to nodes peered to us as master
        if (m_network->peerCount() > 0U && !noConnectedPeerRepeat) {
            uint32_t i = 0U;
            udp::BufferQueue queue = udp::BufferQueue();

            for (const auto& peer : m_network->peersSnapshot()) {
                if (peer.second == nullptr)
                    continue;
                if (peerId != peer.first) {
                    TrafficNetwork::PeerConnectionPtr conn = peer.second;
                    if (ssrc == peer.first) {
                        // skip the peer if it is the source peer
                        continue;
                    }

                    if (m_network->m_restrictPVCallToRegOnly) {
                        // is this peer an upstream neighbor peer?
                        bool neighbor = false;
                        if (conn != nullptr) {
                            neighbor = conn->peerClass() == PEER_CONN_CLASS_NEIGHBOR;
                        }

                        // is this a private call?
                        if ((unitToUnit) && !neighbor) {
                            // is this a private call? if so only repeat to the peer that registered the unit
                            auto it = std::find_if(m_statusPVCall.begin(), m_statusPVCall.end(), [&](StatusMapPair& x) {
                                if (x.second.dstId == dstId) {
                                    if (x.second.activeCall)
                                        return true;
                                }
                                return false;
                            });
                            if (it != m_statusPVCall.end()) {
                                if (peer.first != m_statusPVCall[dstId].dstPeerId) {
                                    continue;
                                }
                            }
                        }
                    }

                    // is this peer ignored?
                    if (!isPeerPermitted(peer.first, dstId, slotNo, unitToUnit, streamId)) {
                        continue;
                    }

                    // every MAX_QUEUED_PEER_MSGS peers flush the queue
                    if (i % MAX_QUEUED_PEER_MSGS == 0U) {
                        m_network->m_frameQueue->flushQueue(&queue);
                    }

                    DECLARE_UINT8_ARRAY(outboundPeerBuffer, len);
                    ::memcpy(outboundPeerBuffer, buffer, len);

                    // perform TGID route rewrites if configured
                    routeRewrite(outboundPeerBuffer, peer.first, dstId, slotNo);

                    m_network->writePeerQueue(&queue, peer.first, ssrc, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25_P2 }, outboundPeerBuffer, len, pktSeq, streamId);
                    if (m_network->m_debug) {
                        LogDebugEx(LOG_DMR, "TagDMRData::processFrame()", "Master, ssrc = %u, srcPeer = %u, dstPeer = %u, srcId = %u, dstId = %u, duid = $%02X, macMCO = $%02X, slotNo = %u, len = %u, pktSeq = %u, stream = %u, fromUpstream = %u", 
                            ssrc, peerId, peer.first, srcId, dstId, duid, macMCO, slotNo, len, pktSeq, streamId, fromUpstream);
                    }

                    i++;
                }
            }
            m_network->m_frameQueue->flushQueue(&queue);
        }

        // if this is a private call, and we have already repeated to the connected peer that registered
        // the unit, don't repeat to any neighbor FNE peers
        if (privateCallInProgress && !noConnectedPeerRepeat) {
            return true;
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
                    if (!isPeerPermitted(dstPeerId, dstId, slotNo, unitToUnit, streamId, true)) {
                        continue;
                    }

                    DECLARE_UINT8_ARRAY(outboundPeerBuffer, len);
                    ::memcpy(outboundPeerBuffer, buffer, len);

                    // perform TGID route rewrites if configured
                    routeRewrite(outboundPeerBuffer, dstPeerId, dstId, slotNo);

                    // are we a replica peer?
                    if (peer.second->isReplica())
                        peer.second->writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25_P2 }, outboundPeerBuffer, len, pktSeq, streamId, false, 0U, ssrc);
                    else
                        peer.second->writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25_P2 }, outboundPeerBuffer, len, pktSeq, streamId);
                    if (m_network->m_debug) {
                        LogDebugEx(LOG_DMR, "TagP25P2Data::processFrame()", "Peers, ssrc = %u, srcPeer = %u, dstPeer = %u, srcId = %u, dstId = %u, duid = $%02X, macMCO = $%02X, slotNo = %u, len = %u, pktSeq = %u, stream = %u, fromUpstream = %u", 
                            ssrc, peerId, dstPeerId, srcId, dstId, duid, macMCO, slotNo, len, pktSeq, streamId, fromUpstream);
                    }
                }
            }
        }

        return true;
    }

    return false;
}

/* Helper to trigger a call takeover from a In-Call control event. */

void TagP25P2Data::triggerCallTakeover(uint32_t dstId)
{
    m_status.lock(false);
    for (auto& entry : m_status) {
        if (entry.second.dstId == dstId && entry.second.activeCall)
            entry.second.callTakeover = true;
    }
    m_status.unlock();
}

/* Helper to playback a parrot frame to the network. */

void TagP25P2Data::playbackParrot()
{
    if (m_parrotFrames.size() == 0) {
        m_parrotFramesReady = false;
        return;
    }

    m_parrotPlayback = true;

    auto& pkt = m_parrotFrames[0];
    m_parrotFrames.lock();
    if (pkt.buffer != nullptr) {
        // has the override source ID been set?
        if (m_network->m_parrotOverrideSrcId > 0U) {
            pkt.srcId = m_network->m_parrotOverrideSrcId;

            // override source ID
            SET_UINT24(m_network->m_parrotOverrideSrcId, pkt.buffer, 5U);
        }

        m_lastParrotPeerId = pkt.peerId;
        m_lastParrotSrcId = pkt.srcId;
        m_lastParrotDstId = pkt.dstId;

        lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(pkt.dstId);
        if (tg.isInvalid()) {
            LogWarning(LOG_P25, "TagP25P2Data::playbackParrot(), invalid talkgroup, dstId = %u", pkt.dstId);
            delete[] pkt.buffer;
            m_parrotFrames.unlock();
            m_parrotFrames.pop_front();
            return;
        }

        if (m_network->m_parrotOnlyOriginating) {
            m_network->writePeer(pkt.peerId, pkt.peerId, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25_P2 }, pkt.buffer, pkt.bufferLen, pkt.pktSeq, pkt.streamId);
            if (m_network->m_debug) {
                LogDebugEx(LOG_P25, "TagP25P2Data::playbackParrot()", "Parrot, dstPeer = %u, len = %u, pktSeq = %u, streamId = %u", 
                    pkt.peerId, pkt.bufferLen, pkt.pktSeq, pkt.streamId);
            }
        }
        else {
            // repeat traffic to the connected peers
            uint32_t i = 0U;
            udp::BufferQueue queue = udp::BufferQueue();

            for (const auto& peer : m_network->peersSnapshot()) {
                // every MAX_QUEUED_PEER_MSGS peers flush the queue
                if (i % MAX_QUEUED_PEER_MSGS == 0U) {
                    m_network->m_frameQueue->flushQueue(&queue);
                }

                // skip peers that are not affiliated with the target group if parrot TG affiliation is enabled
                if (tg.config().affiliated()) {
                    bool skipChecks = false;

                    // check if the peer is in the exclusion list
                    std::vector<uint32_t> exclusion = tg.config().exclusion();
                    if (exclusion.size() > 0) {
                        auto it = std::find(exclusion.begin(), exclusion.end(), peer.first);
                        if (it != exclusion.end()) {
                            continue;
                        }
                    }

                    // peer always send list takes priority over any other rules
                    std::vector<uint32_t> alwaysSend = tg.config().alwaysSend();
                    if (alwaysSend.size() > 0) {
                        auto it = std::find(alwaysSend.begin(), alwaysSend.end(), peer.first);
                        if (it != alwaysSend.end()) {
                            skipChecks = true; // skip any following checks and always send traffic
                        }
                    }

                    TrafficNetwork::PeerConnectionPtr connection = peer.first > 0U ? m_network->findPeer(peer.first) : nullptr;
                    if (connection != nullptr) {
                        if (connection->peerClass() == PEER_CONN_CLASS_CONSOLE) {
                            skipChecks = true; // skip any following checks for console peers
                        }
                    }

                    if (!skipChecks) {
                        TrafficNetwork::PeerConnectionPtr connection = peer.first > 0U ? m_network->findPeer(peer.first) : nullptr;
                        uint32_t lookupPeerId = peer.first;
                        if (connection != nullptr) {
                            if (connection->ccPeerId() > 0U)
                                lookupPeerId = connection->ccPeerId();
                        }

                        std::shared_ptr<fne_lookups::AffiliationLookup> aff = m_network->getPeerAffiliations(lookupPeerId);
                        if (aff == nullptr) {
                            continue;
                        }
                        else {
                            if (!aff->hasGroupAff(pkt.dstId)) {
                                continue;
                            }
                        }
                    }
                }

                m_network->writePeerQueue(&queue, peer.first, pkt.peerId, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25_P2 }, pkt.buffer, pkt.bufferLen, pkt.pktSeq, pkt.streamId);
                if (m_network->m_debug) {
                    LogDebugEx(LOG_P25, "TagP25P2Data::playbackParrot()", "Parrot, dstPeer = %u, len = %u, pktSeq = %u, streamId = %u", 
                        peer.first, pkt.bufferLen, pkt.pktSeq, pkt.streamId);
                }

                i++;
            }
            m_network->m_frameQueue->flushQueue(&queue);
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

void TagP25P2Data::routeRewrite(uint8_t* buffer, uint32_t peerId, uint32_t dstId, uint8_t slotNo, bool outbound)
{
    if (!peerRewrite(peerId, dstId, slotNo, outbound))
        return;

    SET_UINT24(dstId, buffer, 8U);
    buffer[19U] = (buffer[19U] & 0x7FU) | (slotNo == 2U ? 0x80U : 0x00U);
}

/* Helper to route rewrite destination ID and slot. */

bool TagP25P2Data::peerRewrite(uint32_t peerId, uint32_t& dstId, uint8_t& slotNo, bool outbound)
{
    lookups::TalkgroupRuleGroupVoice tg;
    if (outbound) {
        tg = m_network->m_tidLookup->find(dstId, slotNo);
    }
    else {
        tg = m_network->m_tidLookup->findByRewrite(peerId, dstId, slotNo);
    }

    bool rewrote = false;
    if (tg.config().rewriteSize() > 0) {
        std::vector<lookups::TalkgroupRuleRewrite> rewrites = tg.config().rewrite();
        for (auto entry : rewrites) {
            if (entry.peerId() == peerId) {
                if (outbound) {
                    dstId = entry.tgId();
                    slotNo = entry.tgSlot();
                }
                else {
                    dstId = tg.source().tgId();
                    slotNo = tg.source().tgSlot();
                }
                rewrote = true;
                break;
            }
        }
    }

    return rewrote;
}

/* Helper to determine if the peer is permitted for traffic. */

bool TagP25P2Data::isPeerPermitted(uint32_t peerId, uint32_t dstId, uint8_t slotNo, bool unitToUnit, uint32_t streamId, bool fromUpstream)
{
    // promiscuous hub mode performs no ACL checking and will pass all traffic
    if (g_promiscuousHub)
        return true;

    if (unitToUnit)
        return !m_network->m_disallowU2U && !m_network->checkU2UDroppedPeer(peerId);

    TrafficNetwork::PeerConnectionPtr connection = peerId > 0U ? m_network->findPeer(peerId) : nullptr;

    if (connection != nullptr) {
        // is this peer a replica peer?
        if (connection->isReplica()) {
            return true; // replica peers are *always* allowed to receive traffic and no other rules may filter
                         // these peers
        }

        // is this peer a SysView peer?
        if (connection->peerClass() == PEER_CONN_CLASS_SYSVIEW) {
            return true; // SysView peers are *always* allowed to receive traffic and no other rules may filter
                         // these peers
        }
    }

    lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(dstId, slotNo);

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
            if (connection->peerClass() == PEER_CONN_CLASS_STANDARD && connection->isConventional()) {
                fromUpstream = true; // we'll just set the fromUpstream flag to disable the affiliation check
                                     // for conventional peers
            }
        }
    }

    // is this peer a console peer?
    if (connection != nullptr) {
        if (connection->peerClass() == PEER_CONN_CLASS_CONSOLE) {
            fromUpstream = true; // we'll just set the fromUpstream flag to disable the affiliation check
                                 // for console peers
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
        std::shared_ptr<fne_lookups::AffiliationLookup> aff = m_network->getPeerAffiliations(lookupPeerId);
        if (aff == nullptr) {
            std::string peerIdentity = m_network->resolvePeerIdentity(lookupPeerId);
            //LogError(LOG_NET, "PEER %u (%s) has an invalid affiliations lookup? This shouldn't happen BUGBUG.", lookupPeerId, peerIdentity.c_str());
            return false; // this will cause no traffic to pass for this peer now...I'm not sure this is good behavior
        }
        else {
            if (!aff->hasGroupAff(dstId)) {
                return false;
            }
        }
    }

    return true;
}

/* Helper to validate the P25 Phase 2 call stream. */

bool TagP25P2Data::validate(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t slotNo, bool unitToUnit, uint32_t streamId)
{
    // promiscuous hub mode performs no ACL checking and will pass all traffic
    if (g_promiscuousHub)
        return true;

    // is the source ID a blacklisted ID?
    bool rejectUnknownBadCall = false;
    lookups::RadioId rid = m_network->m_ridLookup->find(srcId);
    if (!rid.radioDefault()) {
        if (!rid.radioEnabled()) {
            // report error event to metrics
            TrafficNetwork::MetricsLogging::logCallErrorEvent(m_network, peerId, streamId, srcId, dstId, std::string(DB_ERRSTR_DISABLED_SRC_RID), slotNo);

            if (m_network->m_logDenials)
                LogError(LOG_P25, "P25 Phase 2 Slot %u, " DB_ERRSTR_DISABLED_SRC_RID ", peer = %u, srcId = %u, dstId = %u", slotNo, peerId, srcId, dstId);

            // report In-Call Control to the peer sending traffic
            m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25_P2, NET_ICC::REJECT_TRAFFIC, dstId, slotNo);
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

    // is the call stream rejected?
    m_rejectedCallStreams.lock(false);
    std::vector<uint32_t> rejectedStreams = m_rejectedCallStreams[dstId];
    if (std::find(rejectedStreams.begin(), rejectedStreams.end(), streamId) != rejectedStreams.end()) {
        // report error event to metrics
        TrafficNetwork::MetricsLogging::logCallErrorEvent(m_network, peerId, streamId, srcId, dstId, std::string(DB_ERRSTR_CALL_NOT_PERMITTED), slotNo);

        if (m_network->m_logDenials)
            LogError(LOG_P25, "P25 Phase 2 Slot %u, " DB_ERRSTR_CALL_NOT_PERMITTED ", peer = %u, srcId = %u, dstId = %u", slotNo, peerId, srcId, dstId);

        m_rejectedCallStreams.unlock();

        // report In-Call Control to the peer sending traffic
        m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25_P2, NET_ICC::REJECT_TRAFFIC, dstId, slotNo);
        return false;
    }
    m_rejectedCallStreams.unlock();

    // is this a private call?
    if (unitToUnit) {
        // is the destination ID a blacklisted ID?
        lookups::RadioId rid = m_network->m_ridLookup->find(dstId);
        if (!rid.radioDefault()) {
            if (!rid.radioEnabled()) {
                // report error event to metrics
                TrafficNetwork::MetricsLogging::logCallErrorEvent(m_network, peerId, streamId, srcId, dstId, std::string(DB_ERRSTR_DISABLED_DST_RID), slotNo);

                if (m_network->m_logDenials)
                    LogError(LOG_P25, "P25 Phase 2 Slot %u, " DB_ERRSTR_DISABLED_DST_RID ", peer = %u, srcId = %u, dstId = %u", slotNo, peerId, srcId, dstId);

                return false;
            }
        }
        else {
            // if this is a default radio -- and we are rejecting undefined radios
            // report call error
            if (m_network->m_rejectUnknownRID) {
                // report error event to metrics
                TrafficNetwork::MetricsLogging::logCallErrorEvent(m_network, peerId, streamId, srcId, dstId, std::string(DB_ERRSTR_ILLEGAL_RID_ACCESS), slotNo);

                if (m_network->m_logDenials)
                    LogWarning(LOG_P25, "P25 Phase 2 slot %u, " DB_ERRSTR_ILLEGAL_RID_ACCESS ", srcId = %u, dstId = %u", slotNo, srcId, dstId);

                // report In-Call Control to the peer sending traffic
                m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25_P2, NET_ICC::REJECT_TRAFFIC, dstId, slotNo);
                return false;
            }
        }
    } else {
        lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(dstId, slotNo);
        if (tg.isInvalid()) {
            // report error event to metrics
            TrafficNetwork::MetricsLogging::logCallErrorEvent(m_network, peerId, streamId, srcId, dstId, std::string(DB_ERRSTR_INV_TALKGROUP), slotNo);

            if (m_network->m_logDenials)
                LogError(LOG_P25, "P25 Phase 2 Slot %u, " DB_ERRSTR_INV_TALKGROUP ", peer = %u, srcId = %u, dstId = %u", slotNo, peerId, srcId, dstId);

            // report In-Call Control to the peer sending traffic
            m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25_P2, NET_ICC::REJECT_TRAFFIC, dstId,  slotNo);
            return false;
        }

        /*
        ** bryanb: yes the encryption else condition is missing here
        */

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
            // report error event to metrics
            TrafficNetwork::MetricsLogging::logCallErrorEvent(m_network, peerId, streamId, srcId, dstId, std::string(DB_ERRSTR_ILLEGAL_RID_ACCESS), slotNo);

            if (m_network->m_logDenials)
                LogWarning(LOG_P25, "P25 Phase 2 slot %u, " DB_ERRSTR_ILLEGAL_RID_ACCESS ", srcId = %u, dstId = %u", slotNo, srcId, dstId);

            // report In-Call Control to the peer sending traffic
            m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25_P2, NET_ICC::REJECT_TRAFFIC, dstId, slotNo);
            return false;
        }

        // check the P25 Phase 2 slot number
        if (tg.source().tgSlot() != slotNo) {
            // report error event to metrics
            TrafficNetwork::MetricsLogging::logCallErrorEvent(m_network, peerId, streamId, srcId, dstId, std::string(DB_ERRSTR_INV_SLOT), slotNo);

            if (m_network->m_logDenials)
                LogError(LOG_P25, "P25 Phase 2 Slot %u, " DB_ERRSTR_INV_SLOT ", peer = %u, srcId = %u, dstId = %u", slotNo, peerId, srcId, dstId);

            // report In-Call Control to the peer sending traffic
            m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25_P2, NET_ICC::REJECT_TRAFFIC, dstId, slotNo);
            return false;
        }

        // is the TGID active?
        if (!tg.config().active()) {
            // report error event to metrics
            TrafficNetwork::MetricsLogging::logCallErrorEvent(m_network, peerId, streamId, srcId, dstId, std::string(DB_ERRSTR_DISABLED_TALKGROUP), slotNo);

            if (m_network->m_logDenials)
                LogError(LOG_P25, "P25 Phase 2 Slot %u, " DB_ERRSTR_DISABLED_TALKGROUP ", peer = %u, srcId = %u, dstId = %u", slotNo, peerId, srcId, dstId);

            // report In-Call Control to the peer sending traffic
            m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25_P2, NET_ICC::REJECT_TRAFFIC, dstId, slotNo);
            return false;
        }

        // always peers can violate the rules...hurray
        if (!isAlwaysPeer) {
            // does the TGID have a permitted RID list?
            if (tg.config().permittedRIDs().size() > 0) {
                // does the transmitting RID have permission?
                std::vector<uint32_t> permittedRIDs = tg.config().permittedRIDs();
                if (std::find(permittedRIDs.begin(), permittedRIDs.end(), srcId) == permittedRIDs.end()) {
                    // report error event to metrics
                    TrafficNetwork::MetricsLogging::logCallErrorEvent(m_network, peerId, streamId, srcId, dstId, std::string(DB_ERRSTR_RID_NOT_PERMITTED));

                    if (m_network->m_logDenials)
                        LogError(LOG_P25, "P25 Phase 2 Slot %u, " DB_ERRSTR_RID_NOT_PERMITTED ", peer = %u, srcId = %u, dstId = %u", slotNo, peerId, srcId, dstId);

                    // report In-Call Control to the peer sending traffic
                    m_network->writePeerICC(peerId, streamId, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25_P2, NET_ICC::REJECT_TRAFFIC, dstId, slotNo);
                    return false;
                }
            }
        }
    }

    return true;
}
