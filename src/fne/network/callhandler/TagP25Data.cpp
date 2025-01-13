// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023-2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/Defines.h"
#include "common/p25/lc/tsbk/TSBKFactory.h"
#include "common/p25/Sync.h"
#include "common/Clock.h"
#include "common/Log.h"
#include "common/Thread.h"
#include "common/Utils.h"
#include "network/FNENetwork.h"
#include "network/callhandler/TagP25Data.h"
#include "HostFNE.h"

using namespace system_clock;
using namespace network;
using namespace network::callhandler;
using namespace network::callhandler::packetdata;
using namespace p25;
using namespace p25::defines;

#include <cassert>
#include <chrono>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t GRANT_TIMER_TIMEOUT = 15U;
const uint32_t CALL_COLL_TIMEOUT = 10U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the TagP25Data class. */

TagP25Data::TagP25Data(FNENetwork* network, bool debug) :
    m_network(network),
    m_parrotFrames(),
    m_parrotFramesReady(false),
    m_parrotFirstFrame(true),
    m_status(),
    m_packetData(nullptr),
    m_debug(debug)
{
    assert(network != nullptr);

    m_packetData = new P25PacketData(network, this, debug);
}

/* Finalizes a instance of the TagP25Data class. */

TagP25Data::~TagP25Data()
{
    delete m_packetData;
}

/* Process a data frame from the network. */

bool TagP25Data::processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint16_t pktSeq, uint32_t streamId, bool external)
{
    hrc::hrc_t pktTime = hrc::now();

    UInt8Array __buffer = std::make_unique<uint8_t[]>(len);
    uint8_t* buffer = __buffer.get();
    ::memset(buffer, 0x00U, len);
    ::memcpy(buffer, data, len);

    uint8_t lco = data[4U];

    uint32_t srcId = __GET_UINT16(data, 5U);
    uint32_t dstId = __GET_UINT16(data, 8U);

    uint8_t MFId = data[15U];

    uint8_t lsd1 = data[20U];
    uint8_t lsd2 = data[21U];

    DUID::E duid = (DUID::E)data[22U];
    FrameType::E frameType = FrameType::DATA_UNIT;

    if (duid == DUID::PDU) {
        if (m_network->m_disablePacketData)
            return false;
        return m_packetData->processFrame(data, len, peerId, pktSeq, streamId, external);
    }

    // perform TGID route rewrites if configured
    routeRewrite(buffer, peerId, duid, dstId, false);
    dstId = __GET_UINT16(buffer, 8U);

    lc::LC control;
    data::LowSpeedData lsd;

    // is this a LDU1, is this the first of a call?
    if (duid == DUID::LDU1) {
        frameType = (FrameType::E)data[180U];

        if (m_debug) {
            LogDebug(LOG_NET, "P25, frameType = $%02X", frameType);
        }

        if (frameType == FrameType::HDU_VALID) {
            uint8_t algId = data[181U];
            uint32_t kid = (data[182U] << 8) | (data[183U] << 0);

            // copy MI data
            uint8_t mi[MI_LENGTH_BYTES];
            ::memset(mi, 0x00U, MI_LENGTH_BYTES);

            for (uint8_t i = 0; i < MI_LENGTH_BYTES; i++) {
                mi[i] = data[184U + i];
            }

            if (m_debug) {
                LogDebug(LOG_NET, "P25, HDU algId = $%02X, kId = $%02X", algId, kid);
                Utils::dump(1U, "P25 HDU Network MI", mi, MI_LENGTH_BYTES);
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

    uint32_t frameLength = buffer[23U];

    // process a TSBK out into a class literal if possible
    std::unique_ptr<lc::TSBK> tsbk;
    if (duid == DUID::TSDU) {
        UInt8Array data = std::unique_ptr<uint8_t[]>(new uint8_t[frameLength]);
        ::memset(data.get(), 0x00U, frameLength);
        ::memcpy(data.get(), buffer + 24U, frameLength);

        tsbk = lc::tsbk::TSBKFactory::createTSBK(data.get());
    }

    // is the stream valid?
    if (validate(peerId, control, duid, tsbk.get(), streamId)) {
        // is this peer ignored?
        if (!isPeerPermitted(peerId, control, duid, streamId)) {
            return false;
        }

        // specifically only check the following logic for end of call or voice frames
        if (duid != DUID::TSDU && duid != DUID::PDU) {
            // is this the end of the call stream?
            if ((duid == DUID::TDU) || (duid == DUID::TDULC)) {
                if (srcId == 0U && dstId == 0U) {
                    LogWarning(LOG_NET, "P25, invalid TDU, peer = %u, srcId = %u, dstId = %u, streamId = %u, external = %u", peerId, srcId, dstId, streamId, external);
                    return false;
                }

                RxStatus status = m_status[dstId];
                uint64_t duration = hrc::diff(pktTime, status.callStartTime);

                // perform a test for grant demands, and if the TG isn't valid ignore the demand
                bool grantDemand = (data[14U] & 0x80U) == 0x80U;
                if (grantDemand) {
                    lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(control.getDstId());
                    if (!tg.config().active()) {
                        return false;
                    }
                }

                if (std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) { return x.second.dstId == dstId; }) != m_status.end()) {
                    if (grantDemand) {
                        LogWarning(LOG_NET, "P25, Call Collision, peer = %u, srcId = %u, dstId = %u, streamId = %u, rxPeer = %u, rxSrcId = %u, rxDstId = %u, rxStreamId = %u, external = %u",
                            peerId, srcId, dstId, streamId, status.peerId, status.srcId, status.dstId, status.streamId, external);
                        return false;
                    }
                    else {
                        m_status.erase(dstId);

                        // is this a parrot talkgroup? if so, clear any remaining frames from the buffer
                        lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(dstId);
                        if (tg.config().parrot()) {
                            if (m_parrotFrames.size() > 0) {
                                m_parrotFramesReady = true;
                                m_parrotFirstFrame = true;
                                LogMessage(LOG_NET, "P25, Parrot Playback will Start, peer = %u, srcId = %u", peerId, srcId);
                                m_network->m_parrotDelayTimer.start();
                            }
                        }

                        LogMessage(LOG_NET, "P25, Call End, peer = %u, srcId = %u, dstId = %u, duration = %u, streamId = %u, external = %u",
                            peerId, srcId, dstId, duration / 1000, streamId, external);

                        // report call event to InfluxDB
                        if (m_network->m_enableInfluxDB) {
                            influxdb::QueryBuilder()
                                .meas("call_event")
                                    .tag("peerId", std::to_string(peerId))
                                    .tag("mode", "P25")
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
            }

            // is this a new call stream?
            if ((duid != DUID::TDU) && (duid != DUID::TDULC)) {
                if (srcId == 0U && dstId == 0U) {
                    LogWarning(LOG_NET, "P25, invalid call, peer = %u, srcId = %u, dstId = %u, streamId = %u, external = %u", peerId, srcId, dstId, streamId, external);
                    return false;
                }

                auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) { return x.second.dstId == dstId; });
                if (it != m_status.end()) {
                    RxStatus status = m_status[dstId];
                    if (streamId != status.streamId && ((duid != DUID::TDU) && (duid != DUID::TDULC))) {
                        if (status.srcId != 0U && status.srcId != srcId) {
                            uint64_t lastPktDuration = hrc::diff(hrc::now(), status.lastPacket);
                            if ((lastPktDuration / 1000) > CALL_COLL_TIMEOUT) {
                                LogWarning(LOG_NET, "P25, Call Collision, lasted more then %us with no further updates, forcibly ending call");
                                m_status.erase(dstId);
                                m_network->m_callInProgress = false;
                            }

                            LogWarning(LOG_NET, "P25, Call Collision, peer = %u, srcId = %u, dstId = %u, streamId = %u, rxPeer = %u, rxSrcId = %u, rxDstId = %u, rxStreamId = %u, external = %u",
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
                    RxStatus status = RxStatus();
                    status.callStartTime = pktTime;
                    status.srcId = srcId;
                    status.dstId = dstId;
                    status.streamId = streamId;
                    status.peerId = peerId;
                    m_status[dstId] = status;

                    LogMessage(LOG_NET, "P25, Call Start, peer = %u, srcId = %u, dstId = %u, streamId = %u, external = %u", peerId, srcId, dstId, streamId, external);

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

        // process TSDU from peer
        if (!processTSDUFrom(buffer, peerId, duid)) {
            return false;
        }

        m_status[dstId].lastPacket = hrc::now();

        // repeat traffic to the connected peers
        if (m_network->m_peers.size() > 0U) {
            uint32_t i = 0U;
            for (auto peer : m_network->m_peers) {
                if (peerId != peer.first) {
                    // is this peer ignored?
                    if (!isPeerPermitted(peer.first, control, duid, streamId)) {
                        continue;
                    }

                    // process TSDU to peer
                    if (!processTSDUTo(buffer, peer.first, duid)) {
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
                    routeRewrite(outboundPeerBuffer, peer.first, duid, dstId);

                    m_network->writePeer(peer.first, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, outboundPeerBuffer, len, pktSeq, streamId, true);
                    if (m_network->m_debug) {
                        LogDebug(LOG_NET, "P25, srcPeer = %u, dstPeer = %u, duid = $%02X, lco = $%02X, MFId = $%02X, srcId = %u, dstId = %u, len = %u, pktSeq = %u, streamId = %u, external = %u", 
                            peerId, peer.first, duid, lco, MFId, srcId, dstId, len, pktSeq, streamId, external);
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
                    if (!isPeerPermitted(dstPeerId, control, duid, streamId, true)) {
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
                    routeRewrite(outboundPeerBuffer, dstPeerId, duid, dstId);

                    // process TSDUs going to external peers
                    if (processTSDUToExternal(outboundPeerBuffer, peerId, dstPeerId, duid)) {
                        peer.second->writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, outboundPeerBuffer, len, pktSeq, streamId);
                        if (m_network->m_debug) {
                            LogDebug(LOG_NET, "P25, srcPeer = %u, dstPeer = %u, duid = $%02X, lco = $%02X, MFId = $%02X, srcId = %u, dstId = %u, len = %u, pktSeq = %u, streamId = %u, external = %u", 
                                peerId, dstPeerId, duid, lco, MFId, srcId, dstId, len, pktSeq, streamId, external);
                        }
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

bool TagP25Data::processGrantReq(uint32_t srcId, uint32_t dstId, bool unitToUnit, uint32_t peerId, uint16_t pktSeq, uint32_t streamId)
{
    // if we have an Rx status for the destination deny the grant
    if (std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) { return x.second.dstId == dstId; }) != m_status.end()) {
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
                write_TSDU_Grant(peer.first, srcId, dstId, 4U, !unitToUnit);
            }
        }
    }

    return true;
}

/* Helper to playback a parrot frame to the network. */

void TagP25Data::playbackParrot()
{
    if (m_parrotFrames.size() == 0) {
        m_parrotFramesReady = false;
        m_parrotFirstFrame = true;
        return;
    }

    auto& pkt = m_parrotFrames[0];
    if (pkt.buffer != nullptr) {
        if (m_parrotFirstFrame) {
            if (m_network->m_parrotGrantDemand) {
                uint32_t srcId = pkt.srcId;
                uint32_t dstId = pkt.dstId;

                // create control data
                lc::LC control = lc::LC();
                control.setSrcId(srcId);
                control.setDstId(dstId);

                // create empty LSD
                data::LowSpeedData lsd = data::LowSpeedData();

                uint8_t controlByte = 0x80U;

                // send grant demand
                uint32_t messageLength = 0U;
                UInt8Array message = m_network->createP25_TDUMessage(messageLength, control, lsd, controlByte);
                if (message != nullptr) {
                    if (m_network->m_parrotOnlyOriginating) {
                        LogMessage(LOG_NET, "P25, Parrot Grant Demand, peer = %u, srcId = %u, dstId = %u", pkt.peerId, srcId, dstId);
                        m_network->writePeer(pkt.peerId, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, 0U, false);
                    } else {
                        // repeat traffic to the connected peers
                        for (auto peer : m_network->m_peers) {
                            LogMessage(LOG_NET, "P25, Parrot Grant Demand, peer = %u, srcId = %u, dstId = %u", peer.first, srcId, dstId);
                            m_network->writePeer(peer.first, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, 0U, false);
                        }
                    }
                }
            }

            m_parrotFirstFrame = false;
        }

        if (m_network->m_parrotOnlyOriginating) {
            m_network->writePeer(pkt.peerId, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, pkt.buffer, pkt.bufferLen, pkt.pktSeq, pkt.streamId, false);
            if (m_network->m_debug) {
                LogDebug(LOG_NET, "P25, parrot, dstPeer = %u, len = %u, pktSeq = %u, streamId = %u", 
                    pkt.peerId, pkt.bufferLen, pkt.pktSeq, pkt.streamId);
            }
        } else {
            // repeat traffic to the connected peers
            for (auto peer : m_network->m_peers) {
                m_network->writePeer(peer.first, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, pkt.buffer, pkt.bufferLen, pkt.pktSeq, pkt.streamId, false);
                if (m_network->m_debug) {
                    LogDebug(LOG_NET, "P25, parrot, dstPeer = %u, len = %u, pktSeq = %u, streamId = %u", 
                        peer.first, pkt.bufferLen, pkt.pktSeq, pkt.streamId);
                }
            }
        }

        delete[] pkt.buffer;
    }
    Thread::sleep(180);
    m_parrotFrames.pop_front();
}

/* Helper to write a call alert packet. */

void TagP25Data::write_TSDU_Call_Alrt(uint32_t peerId, uint32_t srcId, uint32_t dstId)
{
    std::unique_ptr<lc::tsbk::IOSP_CALL_ALRT> iosp = std::make_unique<lc::tsbk::IOSP_CALL_ALRT>();
    iosp->setSrcId(srcId);
    iosp->setDstId(dstId);

    LogMessage(LOG_NET, P25_TSDU_STR ", %s, srcId = %u, dstId = %u, txMult = %u", iosp->toString().c_str(), srcId, dstId);

    write_TSDU(peerId, iosp.get());
}

/* Helper to write a radio monitor packet. */

void TagP25Data::write_TSDU_Radio_Mon(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t txMult)
{
    std::unique_ptr<lc::tsbk::IOSP_RAD_MON> iosp = std::make_unique<lc::tsbk::IOSP_RAD_MON>();
    iosp->setSrcId(srcId);
    iosp->setDstId(dstId);
    iosp->setTxMult(txMult);

    LogMessage(LOG_NET, P25_TSDU_STR ", %s, srcId = %u, dstId = %u, txMult = %u", iosp->toString().c_str(), srcId, dstId, txMult);

    write_TSDU(peerId, iosp.get());
}

/* Helper to write a extended function packet. */

void TagP25Data::write_TSDU_Ext_Func(uint32_t peerId, uint32_t func, uint32_t arg, uint32_t dstId)
{
    std::unique_ptr<lc::tsbk::IOSP_EXT_FNCT> iosp = std::make_unique<lc::tsbk::IOSP_EXT_FNCT>();
    iosp->setExtendedFunction(func);
    iosp->setSrcId(arg);
    iosp->setDstId(dstId);

    // class $02 is Motorola -- set the MFID properly
    if ((func >> 8) == 0x02U) {
        iosp->setMFId(MFG_MOT);
    }

    LogMessage(LOG_NET, P25_TSDU_STR ", %s, mfId = $%02X, op = $%02X, arg = %u, tgt = %u",
        iosp->toString().c_str(), iosp->getMFId(), iosp->getExtendedFunction(), iosp->getSrcId(), iosp->getDstId());

    write_TSDU(peerId, iosp.get());
}

/* Helper to write a group affiliation query packet. */

void TagP25Data::write_TSDU_Grp_Aff_Q(uint32_t peerId, uint32_t dstId)
{
    std::unique_ptr<lc::tsbk::OSP_GRP_AFF_Q> osp = std::make_unique<lc::tsbk::OSP_GRP_AFF_Q>();
    osp->setSrcId(WUID_FNE);
    osp->setDstId(dstId);

    LogMessage(LOG_NET, P25_TSDU_STR ", %s, dstId = %u", osp->toString().c_str(), dstId);

    write_TSDU(peerId, osp.get());
}

/* Helper to write a unit registration command packet. */

void TagP25Data::write_TSDU_U_Reg_Cmd(uint32_t peerId, uint32_t dstId)
{
    std::unique_ptr<lc::tsbk::OSP_U_REG_CMD> osp = std::make_unique<lc::tsbk::OSP_U_REG_CMD>();
    osp->setSrcId(WUID_FNE);
    osp->setDstId(dstId);

    LogMessage(LOG_NET, P25_TSDU_STR ", %s, dstId = %u", osp->toString().c_str(), dstId);

    write_TSDU(peerId, osp.get());
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Helper to route rewrite the network data buffer. */

void TagP25Data::routeRewrite(uint8_t* buffer, uint32_t peerId, uint8_t duid, uint32_t dstId, bool outbound)
{
    uint32_t srcId = __GET_UINT16(buffer, 5U);
    uint32_t frameLength = buffer[23U];

    uint32_t rewriteDstId = dstId;

    // does the data require route writing?
    if (peerRewrite(peerId, rewriteDstId, outbound)) {
        // rewrite destination TGID in the frame
        __SET_UINT16(rewriteDstId, buffer, 8U);

        // are we receiving a TSDU?
        if (duid == DUID::TSDU) {
            UInt8Array data = std::unique_ptr<uint8_t[]>(new uint8_t[frameLength]);
            ::memset(data.get(), 0x00U, frameLength);
            ::memcpy(data.get(), buffer + 24U, frameLength);

            std::unique_ptr<lc::TSBK> tsbk = lc::tsbk::TSBKFactory::createTSBK(data.get());
            if (tsbk != nullptr) {
                // handle standard P25 reference opcodes
                switch (tsbk->getLCO()) {
                    case TSBKO::IOSP_GRP_VCH:
                    {
                        LogMessage(LOG_NET, P25_TSDU_STR ", %s, emerg = %u, encrypt = %u, prio = %u, chNo = %u, srcId = %u, dstId = %u",
                            tsbk->toString(true).c_str(), tsbk->getEmergency(), tsbk->getEncrypted(), tsbk->getPriority(), tsbk->getGrpVchNo(), srcId, rewriteDstId);

                        tsbk->setDstId(rewriteDstId);
                    }
                    break;
                }

                // regenerate TSDU
                uint8_t data[P25_TSDU_FRAME_LENGTH_BYTES + 2U];
                ::memset(data + 2U, 0x00U, P25_TSDU_FRAME_LENGTH_BYTES);

                // Generate Sync
                Sync::addP25Sync(data + 2U);

                // Generate TSBK block
                tsbk->setLastBlock(true); // always set last block -- this a Single Block TSDU
                tsbk->encode(data + 2U);

                if (m_debug) {
                    LogDebug(LOG_RF, P25_TSDU_STR ", lco = $%02X, mfId = $%02X, lastBlock = %u, AIV = %u, EX = %u, srcId = %u, dstId = %u, sysId = $%03X, netId = $%05X",
                        tsbk->getLCO(), tsbk->getMFId(), tsbk->getLastBlock(), tsbk->getAIV(), tsbk->getEX(), tsbk->getSrcId(), tsbk->getDstId(),
                        tsbk->getSysId(), tsbk->getNetId());

                    Utils::dump(1U, "!!! *TSDU (SBF) TSBK Block Data", data + P25_PREAMBLE_LENGTH_BYTES + 2U, P25_TSBK_FEC_LENGTH_BYTES);
                }

                ::memcpy(buffer + 24U, data + 2U, P25_TSDU_FRAME_LENGTH_BYTES);
            }
        }
    }
}

/* Helper to route rewrite destination ID. */

bool TagP25Data::peerRewrite(uint32_t peerId, uint32_t& dstId, bool outbound)
{
    lookups::TalkgroupRuleGroupVoice tg;
    if (outbound) {
        tg = m_network->m_tidLookup->find(dstId);
    }
    else {
        tg = m_network->m_tidLookup->findByRewrite(peerId, dstId);
    }

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
                return true;
            }
        }
    }

    return false;
}

/* Helper to process TSDUs being passed from a peer. */

bool TagP25Data::processTSDUFrom(uint8_t* buffer, uint32_t peerId, uint8_t duid)
{
    // are we receiving a TSDU?
    if (duid == DUID::TSDU) {
        uint32_t frameLength = buffer[23U];

        UInt8Array data = std::unique_ptr<uint8_t[]>(new uint8_t[frameLength]);
        ::memset(data.get(), 0x00U, frameLength);
        ::memcpy(data.get(), buffer + 24U, frameLength);

        std::unique_ptr<lc::TSBK> tsbk = lc::tsbk::TSBKFactory::createTSBK(data.get());
        if (tsbk != nullptr) {
            // report tsbk event to InfluxDB
            if (m_network->m_enableInfluxDB && m_network->m_influxLogRawData) {
                const uint8_t* raw = tsbk->getDecodedRaw();
                if (raw != nullptr) {
                    std::stringstream ss;
                    ss << std::hex <<
                        (int)raw[0] << (int)raw[1]  << (int)raw[2] << (int)raw[4] <<
                        (int)raw[5] << (int)raw[6]  << (int)raw[7] << (int)raw[8] <<
                        (int)raw[9] << (int)raw[10] << (int)raw[11];

                    influxdb::QueryBuilder()
                        .meas("tsbk_event")
                            .tag("peerId", std::to_string(peerId))
                            .tag("lco", __INT_HEX_STR(tsbk->getLCO()))
                            .tag("tsbk", tsbk->toString())
                                .field("raw", ss.str())
                            .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                        .request(m_network->m_influxServer);
                }
            }

            // handle standard P25 reference opcodes
            switch (tsbk->getLCO()) {
            case TSBKO::IOSP_UU_VCH:
            case TSBKO::IOSP_UU_ANS:
                {
                    if (m_network->checkU2UDroppedPeer(peerId))
                        return false;
                }
                break;
            case TSBKO::OSP_ADJ_STS_BCAST:
                {
                    if (m_network->m_disallowAdjStsBcast) {
                        // LogWarning(LOG_NET, "PEER %u, passing ADJ_STS_BCAST to internal peers is prohibited, dropping", peerId);
                        return false;
                    } else {
                        lc::tsbk::OSP_ADJ_STS_BCAST* osp = static_cast<lc::tsbk::OSP_ADJ_STS_BCAST*>(tsbk.get());

                        if (m_network->m_verbose) {
                            LogMessage(LOG_NET, P25_TSDU_STR ", %s, sysId = $%03X, rfss = $%02X, site = $%02X, chId = %u, chNo = %u, svcClass = $%02X, peerId = %u", tsbk->toString().c_str(),
                                osp->getAdjSiteSysId(), osp->getAdjSiteRFSSId(), osp->getAdjSiteId(), osp->getAdjSiteChnId(), osp->getAdjSiteChnNo(), osp->getAdjSiteSvcClass(), peerId);
                        }
                    }
                }
                break;
            default:
                break;
            }
        } else {
            std::string peerIdentity = m_network->resolvePeerIdentity(peerId);
            LogWarning(LOG_NET, "PEER %u (%s), passing TSBK that failed to decode? tsbk == nullptr", peerId, peerIdentity.c_str());
        }
    }

    return true;
}

/* Helper to process TSDUs being passed to a peer. */

bool TagP25Data::processTSDUTo(uint8_t* buffer, uint32_t peerId, uint8_t duid)
{
    // are we receiving a TSDU?
    if (duid == DUID::TSDU) {
        uint32_t frameLength = buffer[23U];

        UInt8Array data = std::unique_ptr<uint8_t[]>(new uint8_t[frameLength]);
        ::memset(data.get(), 0x00U, frameLength);
        ::memcpy(data.get(), buffer + 24U, frameLength);

        std::unique_ptr<lc::TSBK> tsbk = lc::tsbk::TSBKFactory::createTSBK(data.get());
        if (tsbk != nullptr) {
            //uint32_t srcId = tsbk->getSrcId();
            uint32_t dstId = tsbk->getDstId();

            FNEPeerConnection* connection = nullptr;
            if (peerId > 0 && (m_network->m_peers.find(peerId) != m_network->m_peers.end())) {
                connection = m_network->m_peers[peerId];
            }

            // handle standard P25 reference opcodes
            switch (tsbk->getLCO()) {
            case TSBKO::IOSP_GRP_VCH:
                {
                    if (m_network->m_restrictGrantToAffOnly) {
                        lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(dstId);
                        if (tg.config().affiliated()) {
                            uint32_t lookupPeerId = peerId;
                            if (connection != nullptr) {
                                if (connection->ccPeerId() > 0U)
                                    lookupPeerId = connection->ccPeerId();
                            }

                            // check the affiliations for this peer to see if we can repeat the TSDU
                            lookups::AffiliationLookup* aff = m_network->m_peerAffiliations[lookupPeerId];
                            if (aff == nullptr) {
                                std::string peerIdentity = m_network->resolvePeerIdentity(lookupPeerId);
                                LogError(LOG_NET, "PEER %u (%s) has an invalid affiliations lookup? This shouldn't happen BUGBUG.", lookupPeerId, peerIdentity.c_str());
                                return false; // this will cause no TSDU to pass for this peer now...I'm not sure this is good behavior
                            }
                            else {
                                if (!aff->hasGroupAff(dstId)) {
                                    if (m_debug) {
                                        std::string peerIdentity = m_network->resolvePeerIdentity(lookupPeerId);
                                        LogDebug(LOG_NET, "PEER %u (%s) can fuck off there's no affiliations.", lookupPeerId, peerIdentity.c_str()); // just so Faulty can see more "salty" log messages
                                    }
                                    return false;
                                }
                            }
                        }
                    }
                }
                break;
            default:
                break;
            }
        } else {
            std::string peerIdentity = m_network->resolvePeerIdentity(peerId);
            LogWarning(LOG_NET, "PEER %u (%s), passing TSBK that failed to decode? tsbk == nullptr", peerId, peerIdentity.c_str());
        }
    }

    return true;
}

/* Helper to process TSDUs being passed to an external peer. */

bool TagP25Data::processTSDUToExternal(uint8_t* buffer, uint32_t srcPeerId, uint32_t dstPeerId, uint8_t duid)
{
    // are we receiving a TSDU?
    if (duid == DUID::TSDU) {
        uint32_t frameLength = buffer[23U];

        UInt8Array data = std::unique_ptr<uint8_t[]>(new uint8_t[frameLength]);
        ::memset(data.get(), 0x00U, frameLength);
        ::memcpy(data.get(), buffer + 24U, frameLength);

        std::unique_ptr<lc::TSBK> tsbk = lc::tsbk::TSBKFactory::createTSBK(data.get());
        if (tsbk != nullptr) {
            // handle standard P25 reference opcodes
            switch (tsbk->getLCO()) {
            case TSBKO::OSP_ADJ_STS_BCAST:
                {
                    if (m_network->m_disallowExtAdjStsBcast) {
                        // LogWarning(LOG_NET, "PEER %u, passing ADJ_STS_BCAST to external peers is prohibited, dropping", dstPeerId);
                        return false;
                    } else {
                        lc::tsbk::OSP_ADJ_STS_BCAST* osp = static_cast<lc::tsbk::OSP_ADJ_STS_BCAST*>(tsbk.get());

                        if (m_network->m_verbose) {
                            LogMessage(LOG_NET, P25_TSDU_STR ", %s, sysId = $%03X, rfss = $%02X, site = $%02X, chId = %u, chNo = %u, svcClass = $%02X, peerId = %u", tsbk->toString().c_str(),
                                osp->getAdjSiteSysId(), osp->getAdjSiteRFSSId(), osp->getAdjSiteId(), osp->getAdjSiteChnId(), osp->getAdjSiteChnNo(), osp->getAdjSiteSvcClass(), srcPeerId);
                        }
                    }
                }
                break;
            default:
                break;
            }
        } else {
            std::string peerIdentity = m_network->resolvePeerIdentity(srcPeerId);
            LogWarning(LOG_NET, "PEER %u (%s), passing TSBK that failed to decode? tsbk == nullptr", srcPeerId, peerIdentity.c_str());
        }
    }

    return true;
}

/* Helper to determine if the peer is permitted for traffic. */

bool TagP25Data::isPeerPermitted(uint32_t peerId, lc::LC& control, DUID::E duid, uint32_t streamId, bool external)
{
    if (control.getLCO() == LCO::PRIVATE) {
        if (!m_network->checkU2UDroppedPeer(peerId))
            return true;
        return false;
    }

    // always permit a TSDU or PDU
    if (duid == DUID::TSDU || duid == DUID::PDU)
        return true;

    if (duid == DUID::HDU) {
        if (m_network->m_filterHeaders) {
            if (control.getSrcId() != 0U && control.getDstId() != 0U) {
                // is this a group call?
                lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(control.getDstId());
                if (!tg.isInvalid()) {
                    return true;
                }

                tg = m_network->m_tidLookup->findByRewrite(peerId, control.getDstId());
                if (!tg.isInvalid()) {
                    return true;
                }

                // is this a U2U call?
                lookups::RadioId rid = m_network->m_ridLookup->find(control.getDstId());
                if (!rid.radioDefault() && rid.radioEnabled()) {
                    return true;
                }

                return false;
            }
        }

        // always permit a headers
        return true;
    }

    if (duid == DUID::TDULC) {
        // always permit a terminator
        return true;
    }

    if (duid == DUID::TDU) {
        if (m_network->m_filterTerminators) {
            if (/*control.getSrcId() != 0U &&*/control.getDstId() != 0U) {
                // is this a group call?
                lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(control.getDstId());
                if (!tg.isInvalid()) {
                    return true;
                }

                tg = m_network->m_tidLookup->findByRewrite(peerId, control.getDstId());
                if (!tg.isInvalid()) {
                    return true;
                }

                // is this a U2U call?
                lookups::RadioId rid = m_network->m_ridLookup->find(control.getDstId());
                if (!rid.radioDefault() && rid.radioEnabled()) {
                    return true;
                }

                return false;
            }
        }

        // always permit a terminator
        return true;
    }

    // is this a group call?
    lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(control.getDstId());

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
            if (!aff->hasGroupAff(control.getDstId())) {
                return false;
            }
        }
    }

    return true;
}

/* Helper to validate the P25 call stream. */

bool TagP25Data::validate(uint32_t peerId, lc::LC& control, DUID::E duid, const p25::lc::TSBK* tsbk, uint32_t streamId)
{
    // is the source ID a blacklisted ID?
    lookups::RadioId rid = m_network->m_ridLookup->find(control.getSrcId());
    if (!rid.radioDefault()) {
        if (!rid.radioEnabled()) {
            // report error event to InfluxDB
            if (m_network->m_enableInfluxDB) {
                influxdb::QueryBuilder()
                    .meas("call_error_event")
                        .tag("peerId", std::to_string(peerId))
                        .tag("streamId", std::to_string(streamId))
                        .tag("srcId", std::to_string(control.getSrcId()))
                        .tag("dstId", std::to_string(control.getDstId()))
                            .field("message", INFLUXDB_ERRSTR_DISABLED_SRC_RID)
                        .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                    .request(m_network->m_influxServer);
            }

            return false;
        }
    }

    // always validate a PDU if the source is valid
    if (duid == DUID::PDU)
        return true;

    // always validate a terminator if the source is valid
    if (duid == DUID::TDU || duid == DUID::TDULC)
        return true;

    // is this a private call?
    if (control.getLCO() == LCO::PRIVATE) {
        // is the destination ID a blacklisted ID?
        lookups::RadioId rid = m_network->m_ridLookup->find(control.getDstId());
        if (!rid.radioDefault()) {
            if (!rid.radioEnabled()) {
                // report error event to InfluxDB
                if (m_network->m_enableInfluxDB) {
                    influxdb::QueryBuilder()
                        .meas("call_error_event")
                            .tag("peerId", std::to_string(peerId))
                            .tag("streamId", std::to_string(streamId))
                            .tag("srcId", std::to_string(control.getSrcId()))
                            .tag("dstId", std::to_string(control.getDstId()))
                                .field("message", INFLUXDB_ERRSTR_DISABLED_DST_RID)
                            .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                        .request(m_network->m_influxServer);
                }

                return false;
            }
        }

        return true;
    }

    // always validate a TSDU or PDU if the source is valid
    if (duid == DUID::TSDU) {
        if (tsbk != nullptr) {
            // handle standard P25 reference opcodes
            switch (tsbk->getLCO()) {
                case TSBKO::IOSP_GRP_VCH:
                {
                    lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(tsbk->getDstId());

                    // check TGID validity
                    if (tg.isInvalid()) {
                        return false;
                    }

                    if (!tg.config().active()) {
                        return false;
                    }
                }
                break;
            }
        }

        return true;
    }

    // check TGID validity
    lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(control.getDstId());
    if (tg.isInvalid()) {
        // report error event to InfluxDB
        if (m_network->m_enableInfluxDB) {
            influxdb::QueryBuilder()
                .meas("call_error_event")
                    .tag("peerId", std::to_string(peerId))
                    .tag("streamId", std::to_string(streamId))
                    .tag("srcId", std::to_string(control.getSrcId()))
                    .tag("dstId", std::to_string(control.getDstId()))
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
                    .tag("srcId", std::to_string(control.getSrcId()))
                    .tag("dstId", std::to_string(control.getDstId()))
                        .field("message", INFLUXDB_ERRSTR_DISABLED_TALKGROUP)
                    .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                .request(m_network->m_influxServer);
        }

        return false;
    }

    return true;
}


/* Helper to write a grant packet. */

bool TagP25Data::write_TSDU_Grant(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t serviceOptions, bool grp)
{
    bool emergency = ((serviceOptions & 0xFFU) & 0x80U) == 0x80U;           // Emergency Flag
    bool encryption = ((serviceOptions & 0xFFU) & 0x40U) == 0x40U;          // Encryption Flag
    uint8_t priority = ((serviceOptions & 0xFFU) & 0x07U);                  // Priority

    if (dstId == TGID_ALL) {
        return true; // do not generate grant packets for $FFFF (All Call) TGID
    }

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

    if (grp) {
        std::unique_ptr<lc::tsbk::IOSP_GRP_VCH> iosp = std::make_unique<lc::tsbk::IOSP_GRP_VCH>();
        iosp->setSrcId(srcId);
        iosp->setDstId(dstId);
        iosp->setGrpVchId(0U);
        iosp->setGrpVchNo(0U);
        iosp->setEmergency(emergency);
        iosp->setEncrypted(encryption);
        iosp->setPriority(priority);

        if (m_network->m_verbose) {
            LogMessage(LOG_NET, P25_TSDU_STR ", %s, emerg = %u, encrypt = %u, prio = %u, chNo = %u, srcId = %u, dstId = %u, peerId = %u",
                iosp->toString().c_str(), iosp->getEmergency(), iosp->getEncrypted(), iosp->getPriority(), iosp->getGrpVchNo(), iosp->getSrcId(), iosp->getDstId(), peerId);
        }

        write_TSDU(peerId, iosp.get());
    }
    else {
        std::unique_ptr<lc::tsbk::IOSP_UU_VCH> iosp = std::make_unique<lc::tsbk::IOSP_UU_VCH>();
        iosp->setSrcId(srcId);
        iosp->setDstId(dstId);
        iosp->setGrpVchId(0U);
        iosp->setGrpVchNo(0U);
        iosp->setEmergency(emergency);
        iosp->setEncrypted(encryption);
        iosp->setPriority(priority);

        if (m_network->m_verbose) {
            LogMessage(LOG_NET, P25_TSDU_STR ", %s, emerg = %u, encrypt = %u, prio = %u, chNo = %u, srcId = %u, dstId = %u, peerId = %u",
                iosp->toString().c_str(), iosp->getEmergency(), iosp->getEncrypted(), iosp->getPriority(), iosp->getGrpVchNo(), iosp->getSrcId(), iosp->getDstId(), peerId);
        }

        write_TSDU(peerId, iosp.get());
    }

    return true;
}

/* Helper to write a deny packet. */

void TagP25Data::write_TSDU_Deny(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t reason, uint8_t service, bool grp, bool aiv)
{
    std::unique_ptr<lc::tsbk::OSP_DENY_RSP> osp = std::make_unique<lc::tsbk::OSP_DENY_RSP>();
    osp->setAIV(aiv);
    osp->setSrcId(srcId);
    osp->setDstId(dstId);
    osp->setService(service);
    osp->setResponse(reason);
    osp->setGroup(grp);

    if (m_network->m_verbose) {
        LogMessage(LOG_RF, P25_TSDU_STR ", %s, AIV = %u, reason = $%02X, srcId = %u, dstId = %u",
            osp->toString().c_str(), osp->getAIV(), reason, osp->getSrcId(), osp->getDstId());
    }

    write_TSDU(peerId, osp.get());
}

/* Helper to write a queue packet. */

void TagP25Data::write_TSDU_Queue(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t reason, uint8_t service, bool grp, bool aiv)
{
    std::unique_ptr<lc::tsbk::OSP_QUE_RSP> osp = std::make_unique<lc::tsbk::OSP_QUE_RSP>();
    osp->setAIV(aiv);
    osp->setSrcId(srcId);
    osp->setDstId(dstId);
    osp->setService(service);
    osp->setResponse(reason);
    osp->setGroup(grp);

    if (m_network->m_verbose) {
        LogMessage(LOG_RF, P25_TSDU_STR ", %s, AIV = %u, reason = $%02X, srcId = %u, dstId = %u",
            osp->toString().c_str(), osp->getAIV(), reason, osp->getSrcId(), osp->getDstId());
    }

    write_TSDU(peerId, osp.get());
}

/* Helper to write a network TSDU. */

void TagP25Data::write_TSDU(uint32_t peerId, lc::TSBK* tsbk)
{
    uint8_t data[P25_TSDU_FRAME_LENGTH_BYTES];
    ::memset(data, 0x00U, P25_TSDU_FRAME_LENGTH_BYTES);

    // generate Sync
    Sync::addP25Sync(data);

    // network bursts have no NID

    // generate TSBK block
    tsbk->setLastBlock(true); // always set last block -- this a Single Block TSDU
    tsbk->encode(data);

    // add status bits
    P25Utils::addStatusBits(data, P25_TSDU_FRAME_LENGTH_BYTES, false, true);
    P25Utils::setStatusBitsStartIdle(data);

    if (m_debug) {
        LogDebug(LOG_RF, P25_TSDU_STR ", lco = $%02X, mfId = $%02X, lastBlock = %u, AIV = %u, EX = %u, srcId = %u, dstId = %u, sysId = $%03X, netId = $%05X",
            tsbk->getLCO(), tsbk->getMFId(), tsbk->getLastBlock(), tsbk->getAIV(), tsbk->getEX(), tsbk->getSrcId(), tsbk->getDstId(),
            tsbk->getSysId(), tsbk->getNetId());

        Utils::dump(1U, "!!! *TSDU (SBF) TSBK Block Data", data + P25_PREAMBLE_LENGTH_BYTES, P25_TSBK_FEC_LENGTH_BYTES);
    }

    lc::LC lc = lc::LC();
    lc.setLCO(tsbk->getLCO());
    lc.setMFId(tsbk->getMFId());
    lc.setSrcId(tsbk->getSrcId());
    lc.setDstId(tsbk->getDstId());

    uint32_t messageLength = 0U;
    UInt8Array message = m_network->createP25_TSDUMessage(messageLength, lc, data);
    if (message == nullptr) {
        return;
    }

    uint32_t streamId = m_network->createStreamId();
    if (peerId > 0U) {
        m_network->writePeer(peerId, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, RTP_END_OF_CALL_SEQ, streamId, false, true);
    } else {
        // repeat traffic to the connected peers
        if (m_network->m_peers.size() > 0U) {
            uint32_t i = 0U;
            for (auto peer : m_network->m_peers) {
                // every 5 peers flush the queue
                if (i % 5U == 0U) {
                    m_network->m_frameQueue->flushQueue();
                }

                m_network->writePeer(peer.first, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, RTP_END_OF_CALL_SEQ, streamId, true);
                if (m_network->m_debug) {
                    LogDebug(LOG_NET, "P25, peer = %u, len = %u, streamId = %u", 
                        peer.first, messageLength, streamId);
                }
                i++;
            }
            m_network->m_frameQueue->flushQueue();
        }

        // repeat traffic to external peers
        if (m_network->m_host->m_peerNetworks.size() > 0U) {
            for (auto peer : m_network->m_host->m_peerNetworks) {
                uint32_t dstPeerId = peer.second->getPeerId();
                peer.second->writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, RTP_END_OF_CALL_SEQ, streamId);
                if (m_network->m_debug) {
                    LogDebug(LOG_NET, "P25, peer = %u, len = %u, streamId = %u", 
                        dstPeerId, messageLength, streamId);
                }
            }
        }
    }
}
