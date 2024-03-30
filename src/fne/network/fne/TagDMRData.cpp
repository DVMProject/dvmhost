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
#include "common/dmr/lc/csbk/CSBKFactory.h"
#include "common/dmr/lc/LC.h"
#include "common/dmr/lc/FullLC.h"
#include "common/dmr/SlotType.h"
#include "common/dmr/Sync.h"
#include "common/Clock.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "network/FNENetwork.h"
#include "network/fne/TagDMRData.h"
#include "HostFNE.h"

using namespace system_clock;
using namespace network;
using namespace network::fne;
using namespace dmr;

#include <cassert>
#include <chrono>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the TagDMRData class.
/// </summary>
/// <param name="network"></param>
/// <param name="debug"></param>
TagDMRData::TagDMRData(FNENetwork* network, bool debug) :
    m_network(network),
    m_parrotFrames(),
    m_parrotFramesReady(false),
    m_status(),
    m_debug(debug)
{
    assert(network != nullptr);
}

/// <summary>
/// Finalizes a instance of the TagDMRData class.
/// </summary>
TagDMRData::~TagDMRData() = default;

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
bool TagDMRData::processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint16_t pktSeq, uint32_t streamId, bool external)
{
    hrc::hrc_t pktTime = hrc::now();

    uint8_t buffer[len];
    ::memset(buffer, 0x00U, len);
    ::memcpy(buffer, data, len);

    uint8_t seqNo = data[4U];

    uint32_t srcId = __GET_UINT16(data, 5U);
    uint32_t dstId = __GET_UINT16(data, 8U);

    uint8_t flco = (data[15U] & 0x40U) == 0x40U ? FLCO_PRIVATE : FLCO_GROUP;

    uint32_t slotNo = (data[15U] & 0x80U) == 0x80U ? 2U : 1U;

    uint8_t dataType = data[15U] & 0x0FU;

    data::Data dmrData;
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
        dmrData.setDataType(DT_VOICE_SYNC);
        dmrData.setN(0U);
    }
    else {
        uint8_t n = data[15U] & 0x0FU;
        dmrData.setData(data + 20U);
        dmrData.setDataType(DT_VOICE);
        dmrData.setN(n);
    }

    // perform TGID route rewrites if configured
    routeRewrite(buffer, peerId, dmrData, dataType, dstId, slotNo, false);
    dstId = __GET_UINT16(buffer, 8U);

    // is the stream valid?
    if (validate(peerId, dmrData, streamId)) {
        // is this peer ignored?
        if (!isPeerPermitted(peerId, dmrData, streamId)) {
            return false;
        }

        // is this the end of the call stream?
        if (dataSync && (dataType == DT_TERMINATOR_WITH_LC)) {
            if (srcId == 0U && dstId == 0U) {
                LogWarning(LOG_NET, "DMR, invalid TERMINATOR, peer = %u, srcId = %u, dstId = %u, streamId = %u, external = %u", peerId, srcId, dstId, streamId, external);
                return false;
            }

            RxStatus status;
            auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) { return (x.second.dstId == dstId && x.second.slotNo == slotNo); });
            if (it == m_status.end()) {
                LogError(LOG_NET, "DMR, tried to end call for non-existent call in progress?, peer = %u, srcId = %u, dstId = %u, streamId = %u, external = %u",
                    peerId, srcId, dstId, streamId, external);
            }
            else {
                status = it->second;
            }

            uint64_t duration = hrc::diff(pktTime, status.callStartTime);

            if (std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) { return (x.second.dstId == dstId && x.second.slotNo == slotNo); }) != m_status.end()) {
                m_status.erase(dstId);

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
        if (dataSync && (dataType == DT_VOICE_LC_HEADER)) {
            if (srcId == 0U && dstId == 0U) {
                LogWarning(LOG_NET, "DMR, invalid call, peer = %u, srcId = %u, dstId = %u, streamId = %u, external = %u", peerId, srcId, dstId, streamId, external);
                return false;
            }

            auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) { return (x.second.dstId == dstId && x.second.slotNo == slotNo); });
            if (it != m_status.end()) {
                RxStatus status = it->second;
                if (streamId != status.streamId) {
                    if (status.srcId != 0U && status.srcId != srcId) {
                        LogWarning(LOG_NET, "DMR, Call Collision, peer = %u, srcId = %u, dstId = %u, streamId = %u, external = %u", peerId, srcId, dstId, streamId, external);
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
                status.slotNo = slotNo;
                status.streamId = streamId;
                m_status[dstId] = status; // this *could* be an issue if a dstId appears on both slots somehow...

                LogMessage(LOG_NET, "DMR, Call Start, peer = %u, srcId = %u, dstId = %u, streamId = %u, external = %u", peerId, srcId, dstId, streamId, external);

                m_network->m_callInProgress = true;
            }
        }

        // is this a parrot talkgroup?
        lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(dstId);
        if (tg.config().parrot()) {
            uint8_t* copy = new uint8_t[len];
            ::memcpy(copy, buffer, len);
            m_parrotFrames.push_back(std::make_tuple(copy, len, pktSeq, streamId));
        }

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

                    uint8_t outboundPeerBuffer[len];
                    ::memset(outboundPeerBuffer, 0x00U, len);
                    ::memcpy(outboundPeerBuffer, buffer, len);

                    // perform TGID route rewrites if configured
                    routeRewrite(outboundPeerBuffer, peer.first, dmrData, dataType, dstId, slotNo);

                    m_network->writePeer(peer.first, { NET_FUNC_PROTOCOL, NET_PROTOCOL_SUBFUNC_DMR }, outboundPeerBuffer, len, pktSeq, streamId, true);
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
                    if (!isPeerPermitted(dstPeerId, dmrData, streamId)) {
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
                    routeRewrite(outboundPeerBuffer, dstPeerId, dmrData, dataType, dstId, slotNo);

                    peer.second->writeMaster({ NET_FUNC_PROTOCOL, NET_PROTOCOL_SUBFUNC_DMR }, outboundPeerBuffer, len, pktSeq, streamId);
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

/// <summary>
/// Process a grant request frame from the network.
/// </summary>
/// <param name="srcId"></param>
/// <param name="dstId"></param>
/// <param name="slot"></param>
/// <param name="unitToUnit"></param>
/// <param name="peerId">Peer ID</param>
/// <param name="pktSeq"></param>
/// <param name="streamId">Stream ID</param>
/// <returns></returns>
bool TagDMRData::processGrantReq(uint32_t srcId, uint32_t dstId, uint8_t slot, bool unitToUnit, uint32_t peerId, uint16_t pktSeq, uint32_t streamId)
{
    // bryanb: TODO TODO TODO
    return false;
}

/// <summary>
/// Helper to playback a parrot frame to the network.
/// </summary>
void TagDMRData::playbackParrot()
{
    if (m_parrotFrames.size() == 0) {
        m_parrotFramesReady = false;
        return;
    }

    auto& pkt = m_parrotFrames[0];
    if (std::get<0>(pkt) != nullptr) {
        // repeat traffic to the connected peers
        for (auto peer : m_network->m_peers) {
            m_network->writePeer(peer.first, { NET_FUNC_PROTOCOL, NET_PROTOCOL_SUBFUNC_DMR }, std::get<0>(pkt), std::get<1>(pkt), std::get<2>(pkt), std::get<3>(pkt), false);
            if (m_network->m_debug) {
                LogDebug(LOG_NET, "DMR, parrot, dstPeer = %u, len = %u, pktSeq = %u, streamId = %u", 
                    peer.first, std::get<1>(pkt), std::get<2>(pkt), std::get<3>(pkt));
            }
        }

        delete std::get<0>(pkt);
    }
    Thread::sleep(60);
    m_parrotFrames.pop_front();
}

/// <summary>
/// Helper to write a extended function packet on the RF interface.
/// </summary>
/// <param name="peerId"></param>
/// <param name="slot"></param>
/// <param name="func">Extended function opcode.</param>
/// <param name="arg">Extended function argument.</param>
/// <param name="dstId">Destination radio ID.</param>
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

/// <summary>
/// Helper to write a call alert packet on the RF interface.
/// </summary>
/// <param name="peerId"></param>
/// <param name="slot"></param>
/// <param name="srcId">Source radio ID.</param>
/// <param name="dstId">Destination radio ID.</param>
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

/// <summary>
/// Helper to route rewrite the network data buffer.
/// </summary>
/// <param name="buffer"></param>
/// <param name="peerId">Peer ID</param>
/// <param name="dmrData"></param>
/// <param name="dataType"></param>
/// <param name="dstId"></param>
/// <param name="slotNo"></param>
/// <param name="outbound"></param>
void TagDMRData::routeRewrite(uint8_t* buffer, uint32_t peerId, dmr::data::Data& dmrData, uint8_t dataType, uint32_t dstId, uint32_t slotNo, bool outbound)
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

        if (dataType == DT_VOICE_LC_HEADER ||
            dataType == DT_TERMINATOR_WITH_LC) {
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
        else if (dataType == DT_VOICE_PI_HEADER) {
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

/// <summary>
/// Helper to route rewrite destination ID and slot.
/// </summary>
/// <param name="peerId">Peer ID</param>
/// <param name="dstId"></param>
/// <param name="slotNo"></param>
/// <param name="outbound"></param>
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

/// <summary>
/// Helper to determine if the peer is permitted for traffic.
/// </summary>
/// <param name="peerId">Peer ID</param>
/// <param name="data"></param>
/// <param name="streamId">Stream ID</param>
/// <returns></returns>
bool TagDMRData::isPeerPermitted(uint32_t peerId, data::Data& data, uint32_t streamId)
{
    // private calls are always permitted
    if (data.getDataType() == FLCO_PRIVATE) {
        return true;
    }

    // is this a group call?
    if (data.getDataType() == FLCO_GROUP) {
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

        // is this a TG that requires affiliations to repeat?
        if (tg.config().affiliated()) {
            // check the affiliations for this peer to see if we can repeat traffic
            lookups::AffiliationLookup* aff = m_network->m_peerAffiliations[peerId];
            if (!aff->hasGroupAff(data.getDstId())) {
                return false;
            }
        }
    }

    return true;
}

/// <summary>
/// Helper to validate the DMR call stream.
/// </summary>
/// <param name="peerId">Peer ID</param>
/// <param name="data"></param>
/// <param name="streamId">Stream ID</param>
/// <returns></returns>
bool TagDMRData::validate(uint32_t peerId, data::Data& data, uint32_t streamId)
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
                            .field("message", "disabled source RID")
                            .field("slot", data.getSlotNo())
                        .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                    .request(m_network->m_influxServer);
            }

            return false;
        }
    }

    // always validate a terminator if the source is valid
    if (data.getDataType() == DT_TERMINATOR_WITH_LC)
        return true;

    // is this a private call?
    if (data.getDataType() == FLCO_PRIVATE) {
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
                                .field("message", "disabled destination RID")
                                .field("slot", data.getSlotNo())
                            .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                        .request(m_network->m_influxServer);
                }

                return false;
            }
        }
    }

    // is this a group call?
    if (data.getDataType() == FLCO_GROUP) {
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
                            .field("message", "illegal/invalid talkgroup")
                            .field("slot", data.getSlotNo())
                        .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                    .request(m_network->m_influxServer);
            }

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
                            .field("message", "invalid slot for talkgroup")
                            .field("slot", data.getSlotNo())
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
                        .tag("srcId", std::to_string(data.getSrcId()))
                        .tag("dstId", std::to_string(data.getDstId()))
                            .field("message", "disabled talkgroup")
                            .field("slot", data.getSlotNo())
                        .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                    .request(m_network->m_influxServer);
            }

            return false;
        }
    }

    return true;
}

/// <summary>
/// Helper to write a NACK RSP packet.
/// </summary>
/// <param name="peerId"></param>
/// <param name="dstId"></param>
/// <param name="reason"></param>
/// <param name="service"></param>
void TagDMRData::write_CSBK_NACK_RSP(uint32_t peerId, uint32_t dstId, uint8_t reason, uint8_t service)
{
    std::unique_ptr<lc::csbk::CSBK_NACK_RSP> csbk = std::make_unique<lc::csbk::CSBK_NACK_RSP>();
    csbk->setServiceKind(service);
    csbk->setReason(reason);
    csbk->setSrcId(DMR_WUID_ALL); // hmmm...
    csbk->setDstId(dstId);

    write_CSBK(peerId, 1U, csbk.get());
}

/// <summary>
/// Helper to write a network CSBK.
/// </summary>
/// <param name="peerId"></param>
/// <param name="slot"></param>
/// <param name="csbk"></param>
void TagDMRData::write_CSBK(uint32_t peerId, uint8_t slot, lc::CSBK* csbk)
{
    uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, DMR_FRAME_LENGTH_BYTES);

    SlotType slotType;
    slotType.setColorCode(0U);
    slotType.setDataType(DT_CSBK);

    // Regenerate the CSBK data
    csbk->encode(data + 2U);

    // Regenerate the Slot Type
    slotType.encode(data + 2U);

    // Convert the Data Sync to be from the BS or MS as needed
    Sync::addDMRDataSync(data + 2U, true);

    data::Data dmrData;
    dmrData.setSlotNo(slot);
    dmrData.setDataType(DT_CSBK);
    dmrData.setSrcId(csbk->getSrcId());
    dmrData.setDstId(csbk->getDstId());
    dmrData.setFLCO(csbk->getGI() ? FLCO_GROUP : FLCO_PRIVATE);
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

    m_network->writePeer(peerId, { NET_FUNC_PROTOCOL, NET_PROTOCOL_SUBFUNC_DMR }, message.get(), messageLength, RTP_END_OF_CALL_SEQ, streamId, false, true);
}
