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
#include "common/dmr/lc/csbk/CSBKFactory.h"
#include "common/dmr/lc/LC.h"
#include "common/dmr/lc/FullLC.h"
#include "common/dmr/SlotType.h"
#include "common/dmr/Sync.h"
#include "common/Clock.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "network/FNENetwork.h"
#include "network/callhandler/TagDMRData.h"
#include "HostFNE.h"

using namespace system_clock;
using namespace network;
using namespace network::callhandler;
using namespace network::callhandler::packetdata;
using namespace dmr;
using namespace dmr::defines;

#include <cassert>
#include <chrono>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t CALL_COLL_TIMEOUT = 10U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the TagDMRData class. */

TagDMRData::TagDMRData(FNENetwork* network, bool debug) :
    m_network(network),
    m_parrotFrames(),
    m_parrotFramesReady(false),
    m_status(),
    m_debug(debug)
{
    assert(network != nullptr);

    m_packetData = new DMRPacketData(network, this, debug);
}

/* Finalizes a instance of the TagDMRData class. */

TagDMRData::~TagDMRData()
{
    delete m_packetData;
}

/* Process a data frame from the network. */

bool TagDMRData::processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint16_t pktSeq, uint32_t streamId, bool external)
{
    hrc::hrc_t pktTime = hrc::now();

    UInt8Array __buffer = std::make_unique<uint8_t[]>(len);
    uint8_t* buffer = __buffer.get();
    ::memset(buffer, 0x00U, len);
    ::memcpy(buffer, data, len);

    uint8_t seqNo = data[4U];

    uint32_t srcId = __GET_UINT16(data, 5U);
    uint32_t dstId = __GET_UINT16(data, 8U);

    FLCO::E flco = (data[15U] & 0x40U) == 0x40U ? FLCO::PRIVATE : FLCO::GROUP;

    uint32_t slotNo = (data[15U] & 0x80U) == 0x80U ? 2U : 1U;

    DataType::E dataType = (DataType::E)(data[15U] & 0x0FU);

    data::NetData dmrData;
    dmrData.setSeqNo(seqNo);
    dmrData.setSlotNo(slotNo);
    dmrData.setSrcId(srcId);
    dmrData.setDstId(dstId);
    dmrData.setFLCO(flco);

    bool dataSync = (data[15U] & 0x20U) == 0x20U;
    bool voiceSync = (data[15U] & 0x10U) == 0x10U;

    if (dataSync) {
        dmrData.setData(data + 20U);
        dmrData.setDataType(dataType);
        dmrData.setN(0U);
    }
    else if (voiceSync) {
        dmrData.setData(data + 20U);
        dmrData.setDataType(DataType::VOICE_SYNC);
        dmrData.setN(0U);
    }
    else {
        uint8_t n = data[15U] & 0x0FU;
        dmrData.setData(data + 20U);
        dmrData.setDataType(DataType::VOICE);
        dmrData.setN(n);
    }

    if (dataSync && ((dataType == DataType::DATA_HEADER)    ||
                     (dataType == DataType::RATE_12_DATA)   ||
                     (dataType == DataType::RATE_34_DATA)   ||
                     (dataType == DataType::RATE_1_DATA))) {
        if (m_network->m_disablePacketData)
            return false;
        return m_packetData->processFrame(data, len, peerId, pktSeq, streamId, external);
    }

    uint8_t frame[DMR_FRAME_LENGTH_BYTES];
    dmrData.getData(frame);

    // perform TGID route rewrites if configured
    routeRewrite(buffer, peerId, dmrData, dataType, dstId, slotNo, false);
    dstId = __GET_UINT16(buffer, 8U);

    // is the stream valid?
    if (validate(peerId, dmrData, streamId)) {
        // is this peer ignored?
        if (!isPeerPermitted(peerId, dmrData, streamId, external)) {
            return false;
        }

        // is this the end of the call stream?
        if (dataSync && (dataType == DataType::TERMINATOR_WITH_LC)) {
            if (srcId == 0U && dstId == 0U) {
                LogWarning(LOG_NET, "DMR, invalid TERMINATOR, peer = %u, srcId = %u, dstId = %u, streamId = %u, external = %u", peerId, srcId, dstId, streamId, external);
                return false;
            }

            RxStatus status;
            {
                auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) {
                    if (x.second.dstId == dstId && x.second.slotNo == slotNo) {
                        return true;
                    }
                    return false;
                });
                if (it == m_status.end()) {
                    LogError(LOG_NET, "DMR, tried to end call for non-existent call in progress?, peer = %u, srcId = %u, dstId = %u, streamId = %u, external = %u",
                        peerId, srcId, dstId, streamId, external);
                }
                else {
                    status = it->second;
                }
            }

            uint64_t duration = hrc::diff(pktTime, status.callStartTime);

            auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) {
                if (x.second.dstId == dstId && x.second.slotNo == slotNo) {
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
                        LogMessage(LOG_NET, "DMR, Parrot Playback will Start, peer = %u, srcId = %u", peerId, srcId);
                        m_network->m_parrotDelayTimer.start();
                    }
                }

                LogMessage(LOG_NET, "DMR, Call End, peer = %u, srcId = %u, dstId = %u, duration = %u, streamId = %u, external = %u",
                            peerId, srcId, dstId, duration / 1000, streamId, external);

                // report call event to InfluxDB
                if (m_network->m_enableInfluxDB) {
                    influxdb::QueryBuilder()
                        .meas("call_event")
                            .tag("peerId", std::to_string(peerId))
                            .tag("mode", "DMR")
                            .tag("streamId", std::to_string(streamId))
                            .tag("srcId", std::to_string(srcId))
                            .tag("dstId", std::to_string(dstId))
                                .field("duration", duration)
                                .field("slot", slotNo)
                            .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                        .request(m_network->m_influxServer);
                }

                m_network->m_callInProgress = false;
            }
        }

        // is this a new call stream?
        if (dataSync && (dataType == DataType::VOICE_LC_HEADER)) {
            if (srcId == 0U && dstId == 0U) {
                LogWarning(LOG_NET, "DMR, invalid call, peer = %u, srcId = %u, dstId = %u, streamId = %u, external = %u", peerId, srcId, dstId, streamId, external);
                return false;
            }

            auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) {
                if (x.second.dstId == dstId && x.second.slotNo == slotNo) {
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
                            LogWarning(LOG_NET, "DMR, Call Collision, lasted more then %us with no further updates, forcibly ending call");
                            m_status[dstId].reset();
                            m_network->m_callInProgress = false;
                        }

                        LogWarning(LOG_NET, "DMR, Call Collision, peer = %u, srcId = %u, dstId = %u, slotNo = %u, streamId = %u, rxPeer = %u, rxSrcId = %u, rxDstId = %u, rxSlotNo = %u, rxStreamId = %u, external = %u",
                            peerId, srcId, dstId, slotNo, streamId, status.peerId, status.srcId, status.dstId, status.slotNo, status.streamId, external);
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
                m_status[dstId].slotNo = slotNo;
                m_status[dstId].streamId = streamId;
                m_status[dstId].peerId = peerId;
                m_status[dstId].activeCall = true;

                LogMessage(LOG_NET, "DMR, Call Start, peer = %u, srcId = %u, dstId = %u, streamId = %u, external = %u", peerId, srcId, dstId, streamId, external);

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

        // process CSBK from peer
        if (!processCSBK(buffer, peerId, dmrData)) {
            return false;
        }

        m_status[dstId].lastPacket = hrc::now();

        // repeat traffic to the connected peers
        if (m_network->m_peers.size() > 0U) {
            uint32_t i = 0U;
            for (auto peer : m_network->m_peers) {
                if (peerId != peer.first) {
                    // is this peer ignored?
                    if (!isPeerPermitted(peer.first, dmrData, streamId)) {
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
                    routeRewrite(outboundPeerBuffer, peer.first, dmrData, dataType, dstId, slotNo);

                    m_network->writePeer(peer.first, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR }, outboundPeerBuffer, len, pktSeq, streamId, true);
                    if (m_network->m_debug) {
                        LogDebug(LOG_NET, "DMR, srcPeer = %u, dstPeer = %u, seqNo = %u, srcId = %u, dstId = %u, flco = $%02X, slotNo = %u, len = %u, pktSeq = %u, stream = %u, external = %u", 
                            peerId, peer.first, seqNo, srcId, dstId, flco, slotNo, len, pktSeq, streamId, external);
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
                    if (!isPeerPermitted(dstPeerId, dmrData, streamId, true)) {
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
                    routeRewrite(outboundPeerBuffer, dstPeerId, dmrData, dataType, dstId, slotNo);

                    peer.second->writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR }, outboundPeerBuffer, len, pktSeq, streamId);
                    if (m_network->m_debug) {
                        LogDebug(LOG_NET, "DMR, srcPeer = %u, dstPeer = %u, seqNo = %u, srcId = %u, dstId = %u, flco = $%02X, slotNo = %u, len = %u, pktSeq = %u, stream = %u, external = %u", 
                            peerId, dstPeerId, seqNo, srcId, dstId, flco, slotNo, len, pktSeq, streamId, external);
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

bool TagDMRData::processGrantReq(uint32_t srcId, uint32_t dstId, uint8_t slot, bool unitToUnit, uint32_t peerId, uint16_t pktSeq, uint32_t streamId)
{
    // if we have an Rx status for the destination deny the grant
    auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) {
        if (x.second.dstId == dstId/* && x.second.slotNo == slot*/) {
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
                write_CSBK_Grant(peer.first, srcId, dstId, 4U, !unitToUnit);
            }
        }
    }

    return true;
}

/* Helper to playback a parrot frame to the network. */

void TagDMRData::playbackParrot()
{
    if (m_parrotFrames.size() == 0) {
        m_parrotFramesReady = false;
        return;
    }

    auto& pkt = m_parrotFrames[0];
    if (pkt.buffer != nullptr) {
        if (m_network->m_parrotOnlyOriginating) {
            m_network->writePeer(pkt.peerId, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR }, pkt.buffer, pkt.bufferLen, pkt.pktSeq, pkt.streamId, false);
            if (m_network->m_debug) {
                LogDebug(LOG_NET, "DMR, parrot, dstPeer = %u, len = %u, pktSeq = %u, streamId = %u", 
                    pkt.peerId, pkt.bufferLen, pkt.pktSeq, pkt.streamId);
            }
        }
        else {
            // repeat traffic to the connected peers
            for (auto peer : m_network->m_peers) {
                m_network->writePeer(peer.first, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR }, pkt.buffer, pkt.bufferLen, pkt.pktSeq, pkt.streamId, false);
                if (m_network->m_debug) {
                    LogDebug(LOG_NET, "DMR, parrot, dstPeer = %u, len = %u, pktSeq = %u, streamId = %u", 
                        peer.first, pkt.bufferLen, pkt.pktSeq, pkt.streamId);
                }
            }
        }

        delete[] pkt.buffer;
    }
    Thread::sleep(60);
    m_parrotFrames.pop_front();
}

/* Helper to write a extended function packet on the RF interface. */

void TagDMRData::write_Ext_Func(uint32_t peerId, uint8_t slot, uint32_t func, uint32_t arg, uint32_t dstId)
{
    std::unique_ptr<lc::csbk::CSBK_EXT_FNCT> csbk = std::make_unique<lc::csbk::CSBK_EXT_FNCT>();
    csbk->setGI(false);
    csbk->setExtendedFunction(func);
    csbk->setSrcId(arg);
    csbk->setDstId(dstId);

    LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, %s, op = $%02X, arg = %u, tgt = %u",
        slot, csbk->toString().c_str(), func, arg, dstId);

    write_CSBK(peerId, slot, csbk.get());
}

/* Helper to write a call alert packet on the RF interface. */

void TagDMRData::write_Call_Alrt(uint32_t peerId, uint8_t slot, uint32_t srcId, uint32_t dstId)
{
    std::unique_ptr<lc::csbk::CSBK_CALL_ALRT> csbk = std::make_unique<lc::csbk::CSBK_CALL_ALRT>();
    csbk->setGI(false);
    csbk->setSrcId(srcId);
    csbk->setDstId(dstId);

    LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, %s, srcId = %u, dstId = %u",
        slot, csbk->toString().c_str(), srcId, dstId);

    write_CSBK(peerId, slot, csbk.get());
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Helper to route rewrite the network data buffer. */

void TagDMRData::routeRewrite(uint8_t* buffer, uint32_t peerId, dmr::data::NetData& dmrData, DataType::E dataType, uint32_t dstId, uint32_t slotNo, bool outbound)
{
    uint32_t rewriteDstId = dstId;
    uint32_t rewriteSlotNo = slotNo;

    // does the data require route rewriting?
    if (peerRewrite(peerId, rewriteDstId, rewriteSlotNo, outbound)) {
        // rewrite destination TGID in the frame
        __SET_UINT16(rewriteDstId, buffer, 8U);

        // set or clear the e.Slot flag (if 0x80 is set Slot 2 otherwise Slot 1)
        if (rewriteSlotNo == 2 && (buffer[15U] & 0x80U) == 0x00U)
            buffer[15U] |= 0x80;
        if (rewriteSlotNo == 1 && (buffer[15U] & 0x80U) == 0x80U)
            buffer[15U] = buffer[15U] & ~0x80U;

        uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];
        dmrData.getData(data + 2U);

        if (dataType == DataType::VOICE_LC_HEADER ||
            dataType == DataType::TERMINATOR_WITH_LC) {
            // decode and reconstruct embedded DMR data
            lc::FullLC fullLC;
            std::unique_ptr<lc::LC> lc = fullLC.decode(data + 2U, dataType);
            if (lc == nullptr) {
                LogWarning(LOG_NET, "DMR Slot %u, bad LC received from the network, replacing", slotNo);
                lc = std::make_unique<lc::LC>(dmrData.getFLCO(), dmrData.getSrcId(), rewriteDstId);
            }

            lc->setDstId(rewriteDstId);

            // Regenerate the LC data
            fullLC.encode(*lc, data + 2U, dataType);
            dmrData.setData(data + 2U);
        }
        else if (dataType == DataType::VOICE_PI_HEADER) {
            // decode and reconstruct embedded DMR data
            lc::FullLC fullLC;
            std::unique_ptr<lc::PrivacyLC> lc = fullLC.decodePI(data + 2U);
            if (lc == nullptr) {
                LogWarning(LOG_NET, "DMR Slot %u, DT_VOICE_PI_HEADER, bad LC received, replacing", slotNo);
                lc = std::make_unique<lc::PrivacyLC>();
            }

            lc->setDstId(rewriteDstId);

            // Regenerate the LC data
            fullLC.encodePI(*lc, data + 2U);
            dmrData.setData(data + 2U);
        }

        dmrData.getData(buffer + 20U);
    }
}

/* Helper to route rewrite destination ID and slot. */

bool TagDMRData::peerRewrite(uint32_t peerId, uint32_t& dstId, uint32_t& slotNo, bool outbound)
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

/* Helper to process CSBKs being passed from a peer. */

bool TagDMRData::processCSBK(uint8_t* buffer, uint32_t peerId, dmr::data::NetData& dmrData)
{
    // are we receiving a CSBK?
    if (dmrData.getDataType() == DataType::CSBK) {
        uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];
        dmrData.getData(data + 2U);

        std::unique_ptr<lc::CSBK> csbk = lc::csbk::CSBKFactory::createCSBK(data + 2U, DataType::CSBK);
        if (csbk != nullptr) {
            // report csbk event to InfluxDB
            if (m_network->m_enableInfluxDB && m_network->m_influxLogRawData) {
                const uint8_t* raw = csbk->getDecodedRaw();
                if (raw != nullptr) {
                    std::stringstream ss;
                    ss << std::hex <<
                        (int)raw[0] << (int)raw[1]  << (int)raw[2] << (int)raw[4] <<
                        (int)raw[5] << (int)raw[6]  << (int)raw[7] << (int)raw[8] <<
                        (int)raw[9] << (int)raw[10] << (int)raw[11];

                    influxdb::QueryBuilder()
                        .meas("csbk_event")
                            .tag("peerId", std::to_string(peerId))
                            .tag("lco", __INT_HEX_STR(csbk->getCSBKO()))
                            .tag("csbk", csbk->toString())
                                .field("raw", ss.str())
                            .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                        .request(m_network->m_influxServer);
                }
            }

            switch (csbk->getCSBKO()) {
            case CSBKO::BROADCAST:
                {
                    lc::csbk::CSBK_BROADCAST* osp = static_cast<lc::csbk::CSBK_BROADCAST*>(csbk.get());
                    if (osp->getAnncType() == BroadcastAnncType::ANN_WD_TSCC) {
                        if (m_network->m_disallowAdjStsBcast) {
                            // LogWarning(LOG_NET, "PEER %u, passing BroadcastAnncType::ANN_WD_TSCC to internal peers is prohibited, dropping", peerId);
                            return false;
                        } else {
                            if (m_network->m_verbose) {
                                LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, %s, sysId = $%03X, chNo = %u, peerId = %u", dmrData.getSlotNo(), csbk->toString().c_str(),
                                    osp->getSystemId(), osp->getLogicalCh1(), peerId);
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
            LogWarning(LOG_NET, "PEER %u (%s), passing CSBK that failed to decode? csbk == nullptr", peerId, peerIdentity.c_str());
        }
    }

    return true;
}

/* Helper to determine if the peer is permitted for traffic. */

bool TagDMRData::isPeerPermitted(uint32_t peerId, data::NetData& data, uint32_t streamId, bool external)
{
    if (data.getFLCO() == FLCO::PRIVATE) {
        if (m_network->m_disallowU2U)
            return false;
        if (!m_network->checkU2UDroppedPeer(peerId))
            return true;
        return false;
    }

    // is this a group call?
    if (data.getFLCO() == FLCO::GROUP) {
        lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(data.getDstId(), data.getSlotNo());

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
                if (!aff->hasGroupAff(data.getDstId())) {
                    return false;
                }
            }
        }
    }

    return true;
}

/* Helper to validate the DMR call stream. */

bool TagDMRData::validate(uint32_t peerId, data::NetData& data, uint32_t streamId)
{
    // is the source ID a blacklisted ID?
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
                            .field("message", INFLUXDB_ERRSTR_DISABLED_SRC_RID)
                            .field("slot", data.getSlotNo())
                        .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                    .request(m_network->m_influxServer);
            }

            // report In-Call Control to the peer sending traffic
            m_network->writePeerICC(peerId, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR, NET_ICC::REJECT_TRAFFIC, data.getDstId(), data.getSlotNo());
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
                            .field("message", INFLUXDB_ERRSTR_DISABLED_SRC_RID)
                            .field("slot", data.getSlotNo())
                        .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                    .request(m_network->m_influxServer);
            }

            LogWarning(LOG_NET, "DMR slot %s, illegal/unknown RID attempted access, srcId = %u, dstId = %u", data.getSlotNo(), data.getSrcId(), data.getDstId());

            // report In-Call Control to the peer sending traffic
            m_network->writePeerICC(peerId, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR, NET_ICC::REJECT_TRAFFIC, data.getDstId(), data.getSlotNo());
            return false;
        }
    }

    // always validate a terminator if the source is valid
    if (data.getDataType() == DataType::TERMINATOR_WITH_LC)
        return true;

    // is this a private call?
    if (data.getFLCO() == FLCO::PRIVATE) {
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
                                .field("message", INFLUXDB_ERRSTR_DISABLED_DST_RID)
                                .field("slot", data.getSlotNo())
                            .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                        .request(m_network->m_influxServer);
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
                                .field("message", INFLUXDB_ERRSTR_DISABLED_SRC_RID)
                                .field("slot", data.getSlotNo())
                            .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                        .request(m_network->m_influxServer);
                }

                LogWarning(LOG_NET, "DMR slot %s, illegal/unknown RID attempted access, srcId = %u, dstId = %u", data.getSlotNo(), data.getSrcId(), data.getDstId());

                // report In-Call Control to the peer sending traffic
                m_network->writePeerICC(peerId, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR, NET_ICC::REJECT_TRAFFIC, data.getDstId(), data.getSlotNo());
                return false;
            }
        }
    }

    // is this a group call?
    if (data.getFLCO() == FLCO::GROUP) {
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
                            .field("message", INFLUXDB_ERRSTR_INV_TALKGROUP)
                            .field("slot", data.getSlotNo())
                        .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                    .request(m_network->m_influxServer);
            }

            // report In-Call Control to the peer sending traffic
            m_network->writePeerICC(peerId, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR, NET_ICC::REJECT_TRAFFIC, data.getDstId(),  data.getSlotNo());
            return false;
        }

        // check the DMR slot number
        if (tg.source().tgSlot() != data.getSlotNo()) {
            // report error event to InfluxDB
            if (m_network->m_enableInfluxDB) {
                influxdb::QueryBuilder()
                    .meas("call_error_event")
                        .tag("peerId", std::to_string(peerId))
                        .tag("streamId", std::to_string(streamId))
                        .tag("srcId", std::to_string(data.getSrcId()))
                        .tag("dstId", std::to_string(data.getDstId()))
                            .field("message", INFLUXDB_ERRSTR_INV_SLOT)
                            .field("slot", data.getSlotNo())
                        .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                    .request(m_network->m_influxServer);
            }

            // report In-Call Control to the peer sending traffic
            m_network->writePeerICC(peerId, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR, NET_ICC::REJECT_TRAFFIC, data.getDstId(), data.getSlotNo());
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
                            .field("message", INFLUXDB_ERRSTR_DISABLED_TALKGROUP)
                            .field("slot", data.getSlotNo())
                        .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                    .request(m_network->m_influxServer);
            }

            // report In-Call Control to the peer sending traffic
            m_network->writePeerICC(peerId, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR, NET_ICC::REJECT_TRAFFIC, data.getDstId(), data.getSlotNo());
            return false;
        }

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
                                .field("message", INFLUXDB_ERRSTR_RID_NOT_PERMITTED)
                            .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                        .request(m_network->m_influxServer);
                }

                // report In-Call Control to the peer sending traffic
                m_network->writePeerICC(peerId, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR, NET_ICC::REJECT_TRAFFIC, data.getDstId(), data.getSlotNo());
                return false;
            }
        }
    }

    return true;
}

/* Helper to write a grant packet. */

bool TagDMRData::write_CSBK_Grant(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t serviceOptions, bool grp)
{
    uint8_t slot = 0U;

    bool emergency = ((serviceOptions & 0xFFU) & 0x80U) == 0x80U;           // Emergency Flag
    bool privacy = ((serviceOptions & 0xFFU) & 0x40U) == 0x40U;             // Privacy Flag
    bool broadcast = ((serviceOptions & 0xFFU) & 0x10U) == 0x10U;           // Broadcast Flag
    uint8_t priority = ((serviceOptions & 0xFFU) & 0x03U);                  // Priority

    if (dstId == WUID_ALL || dstId == WUID_ALLZ || dstId == WUID_ALLL) {
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
        std::unique_ptr<lc::csbk::CSBK_TV_GRANT> csbk = std::make_unique<lc::csbk::CSBK_TV_GRANT>();
        if (broadcast)
            csbk->setCSBKO(CSBKO::BTV_GRANT);
        csbk->setLogicalCh1(0U);
        csbk->setSlotNo(slot);

        if (m_network->m_verbose) {
            LogMessage(LOG_NET, "DMR, CSBK, %s, emerg = %u, privacy = %u, broadcast = %u, prio = %u, chNo = %u, slot = %u, srcId = %u, dstId = %u, peerId = %u",
                csbk->toString().c_str(), emergency, privacy, broadcast, priority, csbk->getLogicalCh1(), csbk->getSlotNo(), srcId, dstId, peerId);
        }

        csbk->setEmergency(emergency);
        csbk->setSrcId(srcId);
        csbk->setDstId(dstId);

        write_CSBK(peerId, 1U, csbk.get());
    }
    else {
        std::unique_ptr<lc::csbk::CSBK_PV_GRANT> csbk = std::make_unique<lc::csbk::CSBK_PV_GRANT>();
        csbk->setLogicalCh1(0U);
        csbk->setSlotNo(slot);

        if (m_network->m_verbose) {
            LogMessage(LOG_NET, "DMR, CSBK, %s, emerg = %u, privacy = %u, broadcast = %u, prio = %u, chNo = %u, slot = %u, srcId = %u, dstId = %u, peerId = %u",
                csbk->toString().c_str(), emergency, privacy, broadcast, priority, csbk->getLogicalCh1(), csbk->getSlotNo(), srcId, dstId, peerId);
        }

        csbk->setEmergency(emergency);
        csbk->setSrcId(srcId);
        csbk->setDstId(dstId);

        write_CSBK(peerId, 1U, csbk.get());
    }

    return true;
}

/* Helper to write a NACK RSP packet. */

void TagDMRData::write_CSBK_NACK_RSP(uint32_t peerId, uint32_t dstId, uint8_t reason, uint8_t service)
{
    std::unique_ptr<lc::csbk::CSBK_NACK_RSP> csbk = std::make_unique<lc::csbk::CSBK_NACK_RSP>();
    csbk->setServiceKind(service);
    csbk->setReason(reason);
    csbk->setSrcId(WUID_ALL); // hmmm...
    csbk->setDstId(dstId);

    write_CSBK(peerId, 1U, csbk.get());
}

/* Helper to write a network CSBK. */

void TagDMRData::write_CSBK(uint32_t peerId, uint8_t slot, lc::CSBK* csbk)
{
    uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, DMR_FRAME_LENGTH_BYTES);

    SlotType slotType;
    slotType.setColorCode(0U);
    slotType.setDataType(DataType::CSBK);

    // Regenerate the CSBK data
    csbk->encode(data + 2U);

    // Regenerate the Slot Type
    slotType.encode(data + 2U);

    // Convert the Data Sync to be from the BS or MS as needed
    Sync::addDMRDataSync(data + 2U, true);

    data::NetData dmrData;
    dmrData.setSlotNo(slot);
    dmrData.setDataType(DataType::CSBK);
    dmrData.setSrcId(csbk->getSrcId());
    dmrData.setDstId(csbk->getDstId());
    dmrData.setFLCO(csbk->getGI() ? FLCO::GROUP : FLCO::PRIVATE);
    dmrData.setN(0U);
    dmrData.setSeqNo(0U);
    dmrData.setBER(0U);
    dmrData.setRSSI(0U);

    dmrData.setData(data + 2U);

    uint32_t streamId = m_network->createStreamId();
    uint32_t messageLength = 0U;
    UInt8Array message = m_network->createDMR_Message(messageLength, streamId, dmrData);
    if (message == nullptr) {
        return;
    }

    if (peerId > 0U) {
        m_network->writePeer(peerId, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR }, message.get(), messageLength, RTP_END_OF_CALL_SEQ, streamId, false, true);
    } else {
        // repeat traffic to the connected peers
        if (m_network->m_peers.size() > 0U) {
            uint32_t i = 0U;
            for (auto peer : m_network->m_peers) {
                // every 5 peers flush the queue
                if (i % 5U == 0U) {
                    m_network->m_frameQueue->flushQueue();
                }

                m_network->writePeer(peer.first, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR }, message.get(), messageLength, RTP_END_OF_CALL_SEQ, streamId, true);
                if (m_network->m_debug) {
                    LogDebug(LOG_NET, "DMR, peer = %u, slotNo = %u, len = %u, stream = %u", 
                        peer.first, slot, messageLength, streamId);
                }
                i++;
            }
            m_network->m_frameQueue->flushQueue();
        }

        // repeat traffic to external peers
        if (m_network->m_host->m_peerNetworks.size() > 0U) {
            for (auto peer : m_network->m_host->m_peerNetworks) {
                uint32_t dstPeerId = peer.second->getPeerId();

                peer.second->writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR }, message.get(), messageLength, RTP_END_OF_CALL_SEQ, streamId);
                if (m_network->m_debug) {
                    LogDebug(LOG_NET, "DMR, peer = %u, slotNo = %u, len = %u, stream = %u", 
                        dstPeerId, slot, messageLength, streamId);
                }
            }
        }
    }
}
