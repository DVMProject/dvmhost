// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024-2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/Defines.h"
#include "common/p25/kmm/KMMFactory.h"
#include "common/p25/data/ConventionalRegistration.h"
#include "common/p25/data/IPv4Packet.h"
#include "common/p25/sndcp/SNDCPFactory.h"
#include "common/p25/Sync.h"
#include "common/edac/CRC.h"
#include "common/Clock.h"
#include "common/Log.h"
#include "common/Thread.h"
#include "common/Utils.h"
#include "network/TrafficNetwork.h"
#include "network/P25OTARService.h"
#include "network/callhandler/packetdata/P25PacketData.h"
#include "HostFNE.h"

using namespace system_clock;
using namespace network;
using namespace network::callhandler;
using namespace network::callhandler::packetdata;
using namespace network::callhandler::packetdata::p25data;
using namespace p25;
using namespace p25::defines;
using namespace p25::data;
using namespace p25::kmm;
using namespace p25::sndcp;

#include <algorithm>
#include <cassert>
#include <chrono>
#include <utility>

#if !defined(_WIN32)
#include <netinet/ip.h>
#endif // !defined(_WIN32)

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint8_t DATA_CALL_COLL_TIMEOUT = 60U;
const uint8_t MAX_PKT_RETRY_CNT = 2U;

const uint32_t INTERPACKET_DELAY = 100U; // milliseconds
const uint32_t ARP_RETRY_MS = 5000U; // milliseconds
const uint32_t SUBSCRIBER_READY_RETRY_MS = 1000U; // milliseconds
const uint32_t CONVENTIONAL_LOCATION_MAX_AGE_MS = 300000U; // 5 minutes

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the P25PacketData class. */

P25PacketData::P25PacketData(TrafficNetwork* network, TagP25Data* tag, bool debug) :
    m_network(network),
    m_tag(tag),
    m_assembler(nullptr),
    m_packetScheduler(network->m_vtunQueueMaxFrames, network->m_vtunQueueMaxBytes),
    m_status(),
    m_neighborCache(),
    m_bindingRegistry(),
    m_locationRegistry(CONVENTIONAL_LOCATION_MAX_AGE_MS),
    m_dataLinkManager(),
    m_conventionalDataService(m_bindingRegistry, m_dataLinkManager),
    m_scepService(m_bindingRegistry),
    m_debug(debug)
{
    assert(network != nullptr);
    assert(tag != nullptr);

    data::Assembler::setVerbose(network->m_verbose);
    data::Assembler::setDumpPDUData(network->m_dumpPacketData);

    m_assembler = new data::Assembler();
    m_assembler->setBlockWriter([](const void* userContext, const uint8_t currentBlock, const uint8_t *data, uint32_t len, bool lastBlock) {
        UserContext* context = const_cast<UserContext*>(static_cast<const UserContext*>(userContext));
        if (context == nullptr) {
            return;
        }

        P25PacketData* packetData = static_cast<P25PacketData*>(context->obj);
        if (packetData == nullptr) {
            return;
        }

        if (!packetData->writeNetwork(context->peerId, context->srcPeerId, context->peerNet, *(context->header),
            currentBlock, data, len, context->pktSeq, context->streamId))
            context->success = false;
    });
}

/* Finalizes a instance of the P25PacketData class. */

P25PacketData::~P25PacketData()
{
    if (m_assembler != nullptr)
        delete m_assembler;
}

/* Process a data frame from the network. */

bool P25PacketData::processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint16_t pktSeq, uint32_t streamId, bool fromUpstream)
{
    // validate the incoming network data frame
    if (data == nullptr || len < 24U) {
        LogError(LOG_P25, P25_PDU_STR ", truncated network PDU header, len = %u", len);
        return false;
    }

    hrc::hrc_t pktTime = hrc::now();

    uint8_t totalBlocks = data[20U] + 1U;
    uint32_t blockLength = GET_UINT24(data, 8U);
    uint8_t currentBlock = data[21U];

    if (totalBlocks == 0U)
        return false;
    if (blockLength == 0U)
        return false;

    auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) { return x.second->peerId == peerId; });
    if (it == m_status.end()) {
        // create a new status entry
        m_status.lock(true);
        RxStatus* status = new RxStatus();
        status->callStartTime = pktTime;
        status->streamId = streamId;
        status->peerId = peerId;
        status->totalBlocks = totalBlocks;
        m_status.unlock();

        m_status.insert(peerId, status);
    }

    RxStatus* status = m_status[peerId];
    if ((status->streamId != 0U && streamId != status->streamId) || status->callBusy) {
        LogDebugEx(LOG_NET, "P25PacketData::processFrame()", "streamId = %u, status->streamId = %u, status->callBusy = %u", streamId, status->streamId, status->callBusy);
        if (m_network->m_callCollisionTimeout > 0U) {
            uint64_t lastPktDuration = hrc::diff(hrc::now(), status->lastPacket);
            if ((lastPktDuration / 1000) > m_network->m_callCollisionTimeout) {
                LogWarning((fromUpstream) ? LOG_PEER : LOG_MASTER, "P25, Data Call Collision, lasted more then %us with no further updates, resetting call source", m_network->m_callCollisionTimeout);

                m_status.lock(false);
                status->streamId = streamId;
                status->callBusy = false;
                m_status.unlock();
            }
            else {
                LogWarning((fromUpstream) ? LOG_PEER : LOG_MASTER, "P25, Data Call Collision, peer = %u, streamId = %u, rxPeer = %u, rxStreamId = %u, fromUpstream = %u",
                    peerId, streamId, status->peerId, status->streamId, fromUpstream);
                return false;
            }
        } else {
            m_status.lock(false);
            status->streamId = streamId;
            m_status.unlock();
        }
    }

    if (status->callBusy) {
        LogWarning((fromUpstream) ? LOG_PEER : LOG_MASTER, "P25, Data Call Lockout, cannot process data packets while data call in progress, peer = %u, streamId = %u, fromUpstream = %u",
            peerId, streamId, fromUpstream);
        return false;
    }

    m_status.lock(false);
    status->lastPacket = hrc::now();
    m_status.unlock();

    // network PDU messages always carry one 25-byte encoded CAI block after
    // their 24-byte metadata header -- reject inconsistent framing before copy
    if (blockLength != P25_PDU_FEC_LENGTH_BYTES || len < (24U + blockLength) ||
        totalBlocks == 0U || currentBlock >= totalBlocks ||
        currentBlock >= P25_MAX_PDU_BLOCKS || status->totalBlocks > P25_MAX_PDU_BLOCKS) {
        LogError(LOG_P25, P25_PDU_STR ", too many PDU blocks to process, %u > %u", currentBlock, P25_MAX_PDU_BLOCKS);
        return false;
    }

    LogInfoEx(LOG_NET, P25_PDU_STR ", received block %u, peerId = %u, len = %u",
        currentBlock, peerId, blockLength);

    // store the received block
    auto existing = status->receivedBlocks.find(currentBlock);
    if (existing != status->receivedBlocks.end()) {
        if (::memcmp(existing->second, data + 24U, blockLength) != 0) {
            LogWarning(LOG_P25, P25_PDU_STR ", conflicting duplicate block %u", currentBlock);
            return false;
        } else {
            LogWarning(LOG_P25, P25_PDU_STR ", ignoring identical duplicate block %u", currentBlock);
        }
    }

    uint8_t* blockData = new uint8_t[blockLength];
    ::memcpy(blockData, data + 24U, blockLength);
    status->receivedBlocks.emplace(currentBlock, blockData);
    status->dataBlockCnt = (uint16_t)status->receivedBlocks.size();

    totalBlocks = status->totalBlocks;
    if (status->dataBlockCnt == totalBlocks) {
        for (uint16_t i = 0U; i < totalBlocks; i++) {
            if (status->receivedBlocks.find(i) != status->receivedBlocks.end()) {
                // block 0 is always the PDU header block
                if (i == 0U) {
                    bool ret = status->assembler.disassemble(status->receivedBlocks[i], P25_PDU_FEC_LENGTH_BYTES, true);
                    if (!ret) {
                        status->streamId = 0U;
                        status->clearReceivedBlocks();
                        return false;
                    }

                    LogInfoEx(LOG_P25, P25_PDU_STR ", peerId = %u, ack = %u, outbound = %u, fmt = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, packetLength = %u, S = %u, n = %u, seqNo = %u, hdrOffset = %u, llId = %u",
                        peerId, status->assembler.dataHeader.getAckNeeded(), status->assembler.dataHeader.getOutbound(), status->assembler.dataHeader.getFormat(), status->assembler.dataHeader.getSAP(), status->assembler.dataHeader.getFullMessage(),
                        status->assembler.dataHeader.getBlocksToFollow(), status->assembler.dataHeader.getPadLength(), status->assembler.dataHeader.getPacketLength(), status->assembler.dataHeader.getSynchronize(), status->assembler.dataHeader.getNs(), 
                        status->assembler.dataHeader.getFSN(), status->assembler.dataHeader.getHeaderOffset(), status->assembler.dataHeader.getLLId());

                    // make sure we don't get a PDU with more blocks then we support
                    if (status->assembler.dataHeader.getBlocksToFollow() >= P25_MAX_PDU_BLOCKS) {
                        LogError(LOG_P25, P25_PDU_STR ", too many PDU blocks to process, %u > %u", status->assembler.dataHeader.getBlocksToFollow(), P25_MAX_PDU_BLOCKS);
                        status->streamId = 0U;
                        status->clearReceivedBlocks();
                        return false;
                    }

                    status->hasRxHeader = true;
                    status->llId = status->assembler.dataHeader.getLLId();

                    m_dataLinkManager.setReady(status->llId, true);

                    // is this a response header?
                    if (status->assembler.dataHeader.getFormat() == PDUFormatType::RSP) {
                        dispatch(peerId);

                        // A response PDU is a complete one-block transaction.
                        // Do not retain an RxStatus with totalBlocks reset to
                        // zero: the next PDU from this peer would reuse it and
                        // could never satisfy dataBlockCnt == totalBlocks.
                        m_status.erase(peerId);
                        delete status;
                        status = nullptr;
                        return true;
                    }

                    LogInfoEx((fromUpstream) ? LOG_PEER : LOG_MASTER, "P25, Data Call Start, peer = %u, llId = %u, streamId = %u, fromUpstream = %u", peerId, status->llId, streamId, fromUpstream);
                    continue;
                }

                status->callBusy = true;
                bool ret = status->assembler.disassemble(status->receivedBlocks[i], blockLength);
                if (!ret) {
                    status->callBusy = false;
                    status->clearReceivedBlocks();
                    return false;
                }
                else {
                    if (status->hasRxHeader && status->assembler.getComplete()) {
                        // is the source ID a blacklisted ID?
                        lookups::RadioId rid = m_network->m_ridLookup->find(status->assembler.dataHeader.getLLId());
                        if (!rid.radioDefault()) {
                            if (!rid.radioEnabled()) {
                                // report error event to metrics
                                TrafficNetwork::MetricsLogging::logCallErrorEvent(m_network, peerId, streamId, status->assembler.dataHeader.getLLId(), status->assembler.dataHeader.getLLId(), std::string(DB_ERRSTR_DISABLED_SRC_RID));

                                m_status.erase(peerId);
                                delete status;
                                status = nullptr;
                                return false;
                            }
                        }

                        status->callBusy = true;

                        // process all blocks in the data stream
                        status->pduUserDataLength = status->assembler.getUserDataLength();
                        status->pduUserData = new uint8_t[P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U];
                        ::memset(status->pduUserData, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U);

                        // dispatch the PDU data
                        if (status->assembler.getUserData(status->pduUserData) > 0U) {
                            if (m_network->m_dumpPacketData) {
                                Utils::dump(1U, "P25, PDU Packet", status->pduUserData, status->pduUserDataLength);
                            }
                            dispatch(peerId);
                        }

                        uint64_t duration = hrc::diff(pktTime, status->callStartTime);
                        uint32_t srcId = (status->assembler.getExtendedAddress()) ? status->assembler.dataHeader.getSrcLLId() : status->assembler.dataHeader.getLLId();
                        uint32_t dstId = status->assembler.dataHeader.getLLId();
                        LogInfoEx((fromUpstream) ? LOG_PEER : LOG_MASTER, "P25, Data Call End, peer = %u, srcId = %u, dstId = %u, blocks = %u, duration = %u, streamId = %u, fromUpstream = %u",
                            peerId, srcId, dstId, status->assembler.dataHeader.getBlocksToFollow(), duration / 1000, streamId, fromUpstream);

                        TrafficNetwork::MetricsLogging::incrementCallsProcessed(m_network);

                        // report call event to metrics
                        TrafficNetwork::MetricsLogging::logCallEvent(m_network, "P25", peerId, streamId, srcId, dstId, duration);

                        m_status.erase(peerId);
                        delete status;
                        status = nullptr;
                        break;
                    } else {
                        status->callBusy = false;
                    }
                }
            }
        }
    }

    return true;
}

/* Process a data frame from the virtual IP network. */

void P25PacketData::processPacketFrame(const uint8_t* data, uint32_t len, bool alreadyQueued)
{
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

#if !defined(_WIN32)
    IPDecodeResult decoded = m_scepService.decode(data, len);
    if (decoded.result != ConvergenceResult::OK) {
        LogError(LOG_P25, "VTUN SCEP packet rejected, length = %u, reason = %u",
            len, uint8_t(decoded.result));
        return;
    }
    const IPv4Packet& ipPacket = decoded.packet;

    uint32_t networkSrcAddr = htonl(ipPacket.sourceAddress);
    uint32_t networkDstAddr = htonl(ipPacket.destinationAddress);

    char srcIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &networkSrcAddr, srcIp, INET_ADDRSTRLEN);

    char dstIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &networkDstAddr, dstIp, INET_ADDRSTRLEN);

    uint8_t proto = ipPacket.protocol;
    uint16_t pktLen = ipPacket.totalLength;

#if DEBUG_P25_PDU_DATA
    Utils::dump(1U, "P25, P25PacketData::processPacketFrame() packet", data, pktLen);
#endif

    // lazily materialize provisioned static SCEP bindings before routing
    getLLIdAddress(ipPacket.destinationAddress);
    IPEncodeResult route = m_scepService.encode(data, len);
    if (route.result != ConvergenceResult::OK) {
        LogWarning(LOG_P25, "VTUN SCEP route rejected, dstIp = %s, reason = %u",
            __IP_FROM_UINT(ipPacket.destinationAddress).c_str(), uint8_t(route.result));
        return;
    }
    uint32_t llId = route.llId;

    uint32_t srcProtoAddr = ipPacket.sourceAddress;
    uint32_t tgtProtoAddr = ipPacket.destinationAddress;

    std::string srcIpStr = __IP_FROM_UINT(srcProtoAddr);
    std::string tgtIpStr = __IP_FROM_UINT(tgtProtoAddr);

    LogInfoEx(LOG_P25, "VTUN -> PDU IP Data, srcIp = %s (%u), dstIp = %s (%u), pktLen = %u, proto = %02X%s, llId = %u%s", 
        srcIpStr.c_str(), WUID_FNE, tgtIpStr.c_str(), llId, pktLen, proto, (proto == 0x01) ? " (ICMP)" : "",
        llId, (llId == 0U) ? " (UNRESOLVED - will retry with ARP)" : "");

    // assemble a P25 PDU frame header for transport...
    data::DataHeader pktHeader;
    bool confirmed = route.delivery == DataDeliveryMode::CONFIRMED;
    pktHeader.setFormat(confirmed ? PDUFormatType::CONFIRMED : PDUFormatType::UNCONFIRMED);
    pktHeader.setMFId(MFG_STANDARD);
    pktHeader.setAckNeeded(confirmed);
    pktHeader.setOutbound(true);
    pktHeader.setSAP(PDUSAP::PACKET_DATA);
    pktHeader.setLLId(llId);
    pktHeader.setBlocksToFollow(1U);

    pktHeader.calculateLength(pktLen);
    uint32_t pduLength = pktHeader.getPDULength();
    if (pduLength < pktLen) {
        LogWarning(LOG_P25, "VTUN, data truncated!");
        pktLen = pduLength; // don't overflow the buffer
    }

    DECLARE_UINT8_ARRAY(pduUserData, pduLength);
    ::memcpy(pduUserData, data, pktLen);
//#if DEBUG_P25_PDU_DATA
    Utils::dump(1U, "P25, P25PacketData::processPacketFrame(), pduUserData", pduUserData, pduLength);
//#endif

    ScheduledP25DataPacket packet;
    packet.header = pktHeader;
    packet.llId = llId;
    packet.targetIPAddress = tgtProtoAddr;
    packet.dueAt = now + INTERPACKET_DELAY;
    packet.userData.assign(pduUserData, pduUserData + pduLength);

    uint32_t droppedFrames = m_packetScheduler.enqueue(std::move(packet));
    if (droppedFrames > 0U) {
        LogWarning(LOG_P25, "VTUN queue cap reached, dropped %u frame(s), queuedFrames = %u, queuedBytes = %u",
            droppedFrames, uint32_t(m_packetScheduler.size()), m_packetScheduler.byteCount());
    }
#endif // !defined(_WIN32)
}

/* Helper to write a PDU acknowledge response. */

void P25PacketData::write_PDU_Ack_Response(uint8_t ackClass, uint8_t ackType, uint8_t ackStatus, uint32_t llId, bool extendedAddress, uint32_t srcLlId)
{
    if (ackClass == PDUAckClass::ACK && ackType != PDUAckType::ACK)
        return;

    data::DataHeader rspHeader = data::DataHeader();
    rspHeader.setFormat(PDUFormatType::RSP);
    rspHeader.setMFId(MFG_STANDARD);
    rspHeader.setOutbound(true);
    rspHeader.setResponseClass(ackClass);
    rspHeader.setResponseType(ackType);
    rspHeader.setResponseStatus(ackStatus);
    rspHeader.setLLId(llId);
    if (srcLlId > 0U) {
        rspHeader.setSrcLLId(srcLlId);
    }

    if (!extendedAddress)
        rspHeader.setFullMessage(true);
    else
        rspHeader.setFullMessage(false);

    rspHeader.setBlocksToFollow(0U);

    dispatchUserFrameToFNE(rspHeader, srcLlId > 0U, false, nullptr);
}

/* Helper used to return a KMM to the calling SU. */

bool P25PacketData::write_PDU_KMM(const uint8_t* data, uint32_t len, uint32_t llId, bool encrypted, uint8_t algId, uint16_t kId, const uint8_t* mi)
{
    // assemble a P25 PDU frame header for transport...
    data::DataHeader dataHeader = data::DataHeader();
    dataHeader.setFormat(PDUFormatType::CONFIRMED);
    dataHeader.setMFId(MFG_STANDARD);
    dataHeader.setAckNeeded(true);
    dataHeader.setOutbound(true);
    dataHeader.setSAP((encrypted) ? PDUSAP::ENC_USER_DATA : PDUSAP::UNENC_KMM);
    dataHeader.setLLId(llId);
    dataHeader.setBlocksToFollow(1U);

    bool auxiliaryES = false;
    if (encrypted) {
        if (mi == nullptr) {
            LogError(LOG_P25, P25_PDU_STR ", missing MI for encrypted KMM, llId = %u", llId);
            return false;
        }

        dataHeader.setEXSAP(PDUSAP::UNENC_KMM);
        dataHeader.setAlgId(algId);
        dataHeader.setKId(kId);
        dataHeader.setMI(mi);
        auxiliaryES = true;
    }

    dataHeader.calculateLength(len);
    uint32_t pduLength = dataHeader.getPDULength();

    DECLARE_UINT8_ARRAY(pduUserData, pduLength);
    ::memcpy(pduUserData, data, len);

    return dispatchUserFrameToFNE(dataHeader, false, auxiliaryES, pduUserData);
}

/* Updates the timer by the passed number of milliseconds. */

void P25PacketData::clock(uint32_t ms)
{
#if !defined(_WIN32)
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    m_conventionalDataService.expire(now);
    m_locationRegistry.expire(now);

    if (m_packetScheduler.empty()) {
        return;
    }

    ScheduledP25DataPacket* packet = m_packetScheduler.front();
    if (packet == nullptr || now <= packet->dueAt)
        return;

    std::string targetIP = __IP_FROM_UINT(packet->targetIPAddress);
    uint8_t protocol = packet->userData.size() >= 20U ? packet->userData[9U] : 0U;
    if (packet->retryCount >= (packet->extendedRetry ? MAX_PKT_RETRY_CNT * 2U : MAX_PKT_RETRY_CNT)) {
        LogWarning(LOG_P25, P25_PDU_STR ", max packet retry count exceeded, dropping packet, dstIp = %s",
            targetIP.c_str());
        m_dataLinkManager.setReady(packet->llId, true);
        m_packetScheduler.pop();
        return;
    }

    if (packet->llId != WUID_ALL) {
        DataRoute route = m_locationRegistry.resolve(packet->llId, AccessMode::CONVENTIONAL, now);
        if (!route.valid) {
            LogWarning(LOG_P25, P25_PDU_STR ", subscriber location unknown, dstIp = %s (%u)",
                targetIP.c_str(), packet->llId);

            UnknownLocationPolicy policy = m_locationRegistry.unknownLocationPolicy();
            if (policy == UnknownLocationPolicy::DROP) {
                m_packetScheduler.pop();
                return;
            }
            if (policy == UnknownLocationPolicy::ARP)
                write_PDU_ARP(packet->targetIPAddress);

            packet->dueAt = now + ARP_RETRY_MS;
            packet->retryCount++;
            m_packetScheduler.rotate();
            return;
        }
    }

    if (m_dataLinkManager.hasState(packet->llId) &&
        !m_dataLinkManager.isReady(packet->llId)) {
        LogWarning(LOG_P25, P25_PDU_STR ", subscriber not ready, dstIp = %s (%u), proto = %02X%s, will retry in %ums",
            targetIP.c_str(), packet->llId, protocol, (protocol == 0x01U) ? " (ICMP)" : "",
            SUBSCRIBER_READY_RETRY_MS);
        packet->dueAt = now + SUBSCRIBER_READY_RETRY_MS;
        packet->extendedRetry = true;
        packet->retryCount++;
        m_packetScheduler.rotate();
        return;
    }

    LogInfoEx(LOG_P25, "VTUN -> PDU IP Data (queued), dstIp = %s (%u), userDataLen = %u, proto = %02X%s, retries = %u",
        targetIP.c_str(), packet->llId, uint32_t(packet->userData.size()), protocol,
        (protocol == 0x01U) ? " (ICMP)" : "", packet->retryCount);

    const bool confirmed = packet->header.getFormat() == PDUFormatType::CONFIRMED;
    if (confirmed)
        m_dataLinkManager.setReady(packet->llId, false);

    if (dispatchUserFrameToFNE(packet->header, false, false, packet->userData.data())) {
        m_packetScheduler.pop();
    }
    else {
        if (confirmed)
            m_dataLinkManager.setReady(packet->llId, true);

        packet->dueAt = now + SUBSCRIBER_READY_RETRY_MS;
        packet->retryCount++;
        m_packetScheduler.rotate();

        LogWarning(LOG_P25, P25_PDU_STR ", downlink dispatch rejected, dstIp = %s (%u), retry = %u",
            targetIP.c_str(), packet->llId, packet->retryCount);
    }
#endif // !defined(_WIN32)
}

/* Helper to cleanup any call's left in a dangling state without any further updates. */

void P25PacketData::cleanupStale()
{
    // check to see if any peers have been quiet (no ping) longer than allowed
    std::vector<uint32_t> peersToRemove = std::vector<uint32_t>();
    m_status.lock(false);
    for (auto peerStatus : m_status) {
        uint32_t id = peerStatus.first;
        RxStatus* status = peerStatus.second;
        if (status != nullptr) {
            uint64_t lastPktDuration = hrc::diff(hrc::now(), status->lastPacket);
            if ((lastPktDuration / 1000) > 10U) {
                LogWarning(LOG_P25, "P25, Data Call Timeout, lasted more then %us with no further updates", 10U);
                status->callBusy = true; // force flag the call busy
                peersToRemove.push_back(id);
            }
        }
    }
    m_status.unlock();

    // remove any peers
    for (uint32_t peerId : peersToRemove) {
        RxStatus* status = m_status[peerId];
        if (status != nullptr) {
            m_status.erase(peerId);
            delete status;
            status = nullptr;
        }
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Helper to dispatch PDU user data. */

void P25PacketData::dispatch(uint32_t peerId)
{
    RxStatus* status = m_status[peerId];

    if (status == nullptr) {
        LogError(LOG_P25, P25_PDU_STR ", illegal PDU packet state, status shouldn't be null");
        return;
    }

    if (status->assembler.dataHeader.getFormat() == PDUFormatType::RSP) {
        LogInfoEx(LOG_P25, P25_PDU_STR ", ISP, response, peer = %u, fmt = $%02X, rspClass = $%02X, rspType = $%02X, rspStatus = $%02X, llId = %u, srcLlId = %u",
                peerId, status->assembler.dataHeader.getFormat(), status->assembler.dataHeader.getResponseClass(), status->assembler.dataHeader.getResponseType(), status->assembler.dataHeader.getResponseStatus(),
                status->assembler.dataHeader.getLLId(), status->assembler.dataHeader.getSrcLLId());

        const uint32_t responseLlId = status->assembler.getExtendedAddress() ? status->assembler.dataHeader.getSrcLLId() : status->assembler.dataHeader.getLLId();
        if (responseLlId != 0U)
            m_dataLinkManager.setReady(responseLlId, true);

        if (status->assembler.dataHeader.getResponseClass() == PDUAckClass::ACK && status->assembler.dataHeader.getResponseType() == PDUAckType::ACK) {
            LogInfoEx(LOG_P25, P25_PDU_STR ", ISP, response, OSP ACK, peer = %u, llId = %u, all blocks received OK, n = %u",
                peerId, status->assembler.dataHeader.getLLId(), status->assembler.dataHeader.getResponseStatus());
        } else {
            if (status->assembler.dataHeader.getResponseClass() == PDUAckClass::NACK) {
                switch (status->assembler.dataHeader.getResponseType()) {
                    case PDUAckType::NACK_ILLEGAL:
                        LogInfoEx(LOG_P25, P25_PDU_STR ", ISP, response, OSP NACK, illegal format, peer = %u, llId = %u",
                            peerId, status->assembler.dataHeader.getLLId());
                        break;
                    case PDUAckType::NACK_PACKET_CRC:
                        LogInfoEx(LOG_P25, P25_PDU_STR ", ISP, response, OSP NACK, packet CRC error, peer = %u, llId = %u, n = %u",
                            peerId, status->assembler.dataHeader.getLLId(), status->assembler.dataHeader.getResponseStatus());
                        break;
                    case PDUAckType::NACK_SEQ:
                    case PDUAckType::NACK_OUT_OF_SEQ:
                        LogInfoEx(LOG_P25, P25_PDU_STR ", ISP, response, OSP NACK, packet out of sequence, peer = %u, llId = %u, seqNo = %u",
                            peerId, status->assembler.dataHeader.getLLId(), status->assembler.dataHeader.getResponseStatus());
                        break;
                    case PDUAckType::NACK_UNDELIVERABLE:
                        LogInfoEx(LOG_P25, P25_PDU_STR ", ISP, response, OSP NACK, packet undeliverable, peer = %u, llId = %u, n = %u",
                            peerId, status->assembler.dataHeader.getLLId(), status->assembler.dataHeader.getResponseStatus());
                        break;

                    default:
                        break;
                    }
            }
        }

        return;
    }

    // handle unconfirmed PDU format and set the data link manager ready state if necessary
    if (status->assembler.dataHeader.getFormat() == PDUFormatType::UNCONFIRMED) {
        const uint32_t responseLlId = status->assembler.getExtendedAddress() ?
            status->assembler.dataHeader.getSrcLLId() : status->assembler.dataHeader.getLLId();

        if (responseLlId != 0U)
            m_dataLinkManager.setReady(responseLlId, true);
    }

    // do not route or inject incomplete/invalid confirmed payloads -- the empty
    // ACK_RETRY is the interoperable whole-PDU fallback until bitmap-selective
    // retry is implemented; a complete packet with a bad CRC is explicitly NACKed
    if (status->assembler.dataHeader.getFormat() == PDUFormatType::CONFIRMED &&
        (status->assembler.getUndecodableBlockCount() > 0U ||
         status->assembler.getPacketCRCFailed())) {
        const uint32_t srcLlId = status->assembler.getExtendedAddress() ? status->assembler.dataHeader.getSrcLLId() : status->assembler.dataHeader.getLLId();
        const uint32_t dstLlId = status->assembler.dataHeader.getLLId();
        const uint8_t responseClass = status->assembler.getUndecodableBlockCount() > 0U ? PDUAckClass::ACK_RETRY : PDUAckClass::NACK;
        const uint8_t responseType = status->assembler.getUndecodableBlockCount() > 0U ? PDUAckType::ACK : PDUAckType::NACK_PACKET_CRC;

        write_PDU_Ack_Response(responseClass, responseType, status->assembler.dataHeader.getNs(), srcLlId, 
            status->assembler.getExtendedAddress(), dstLlId);
        return;
    }

    uint8_t sap = (status->assembler.getExtendedAddress()) ? status->assembler.dataHeader.getEXSAP() : status->assembler.dataHeader.getSAP();
    if (status->assembler.getAuxiliaryES())
        sap = status->assembler.dataHeader.getEXSAP();

    // the peer is the only channel identity carried at this boundary -- preserve
    // unknown channel fields as zero until peer metadata supplies them, a
    // registration establishes location only after the binding is accepted
    if (sap != PDUSAP::CONV_DATA_REG && m_locationRegistry.learnFromInbound()) {
        uint32_t inboundLlId = status->assembler.getExtendedAddress() ?
            status->assembler.dataHeader.getSrcLLId() : status->assembler.dataHeader.getLLId();
        if (inboundLlId != 0U && inboundLlId != WUID_ALL) {
            ConventionalLocation location;
            location.peerId = peerId;
            location.lastSeen = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            m_locationRegistry.updateConventional(inboundLlId, location);
        }
    }

    // handle standard P25 service access points
    switch (sap) {
    case PDUSAP::ARP:
    {
#if !defined(_WIN32)
        // is the host virtual tunneling enabled?
        if (!m_network->m_host->m_vtunEnabled)
            break;

        uint32_t fneIPv4 = __IP_FROM_STR(m_network->m_host->m_tun->getIPv4());

        if (status->pduUserDataLength < P25_PDU_ARP_PCKT_LENGTH) {
            LogError(LOG_P25, P25_PDU_STR ", ARP packet too small, %u bytes (need %u)",
                status->pduUserDataLength, P25_PDU_ARP_PCKT_LENGTH);
            break;
        }

        uint8_t arpPacket[P25_PDU_ARP_PCKT_LENGTH];
        ::memset(arpPacket, 0x00U, P25_PDU_ARP_PCKT_LENGTH);
        ::memcpy(arpPacket, status->pduUserData, P25_PDU_ARP_PCKT_LENGTH);

        uint16_t opcode = GET_UINT16(arpPacket, 6U);
        uint32_t srcHWAddr = GET_UINT24(arpPacket, 8U);
        uint32_t srcProtoAddr = GET_UINT32(arpPacket, 11U);
        //uint32_t tgtHWAddr = GET_UINT24(arpPacket, 15U);
        uint32_t tgtProtoAddr = GET_UINT32(arpPacket, 18U);

        if (opcode == P25_PDU_ARP_REQUEST) {
            LogInfoEx(LOG_P25, P25_PDU_STR ", ARP request, who has %s? tell %s (%u)", __IP_FROM_UINT(tgtProtoAddr).c_str(), __IP_FROM_UINT(srcProtoAddr).c_str(), srcHWAddr);
            if (fneIPv4 == tgtProtoAddr) {
                write_PDU_ARP_Reply(fneIPv4, srcHWAddr, srcProtoAddr, WUID_FNE);
            } else {
                write_PDU_ARP_Reply(tgtProtoAddr, srcHWAddr, srcProtoAddr);
            }
        } else if (opcode == P25_PDU_ARP_REPLY) {
            LogInfoEx(LOG_P25, P25_PDU_STR ", ARP reply, %s is at %u", __IP_FROM_UINT(srcProtoAddr).c_str(), srcHWAddr);
            if (fneIPv4 == srcProtoAddr) {
                LogWarning(LOG_P25, P25_PDU_STR ", ARP reply, %u is trying to masquerade as us...", srcHWAddr);
            } else {
                m_neighborCache.observe(srcHWAddr, srcProtoAddr);

                // is the SU ready for the next packet?
                m_dataLinkManager.setReady(srcHWAddr, true);
            }
        }
#else
        break;
#endif // !defined(_WIN32)
    }
    break;
    case PDUSAP::PACKET_DATA:
    {
#if !defined(_WIN32)
        // is the host virtual tunneling enabled?
        if (!m_network->m_host->m_vtunEnabled)
            break;

        uint32_t srcLlId = status->assembler.dataHeader.getSrcLLId();
        if (!status->assembler.getExtendedAddress())
            srcLlId = status->assembler.dataHeader.getLLId();
        uint32_t dstLlId = status->assembler.dataHeader.getLLId();
        if (!status->assembler.getExtendedAddress())
            dstLlId = WUID_FNE;

        IPDecodeResult decoded = m_scepService.decode(status->pduUserData,
            status->pduUserDataLength);
        if (decoded.result != ConvergenceResult::OK) {
            LogError(LOG_P25, P25_PDU_STR ", SCEP uplink decode rejected, length = %u, reason = %u",
                status->pduUserDataLength, uint8_t(decoded.result));
            break;
        }
        const IPv4Packet& ipPacket = decoded.packet;

        // provisioned static entries become typed bindings on first use -- ARP
        // observations are intentionally not consulted for authorization
        getIPAddress(srcLlId);
        ConvergenceResult authorization =
            m_scepService.authorizeUplink(srcLlId, ipPacket);
        if (authorization != ConvergenceResult::OK) {
            LogWarning(LOG_P25, P25_PDU_STR ", SCEP uplink unauthorized, llId = %u, srcIp = %s, reason = %u",
                srcLlId, __IP_FROM_UINT(ipPacket.sourceAddress).c_str(), uint8_t(authorization));
            write_PDU_Ack_Response(PDUAckClass::NACK, PDUAckType::NACK_INVL_USER,
                status->assembler.dataHeader.getNs(), srcLlId,
                status->assembler.getExtendedAddress(), dstLlId);
            break;
        }

        uint32_t networkSrcAddr = htonl(ipPacket.sourceAddress);
        uint32_t networkDstAddr = htonl(ipPacket.destinationAddress);
        uint16_t pktLen = ipPacket.totalLength;

        char srcIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &networkSrcAddr, srcIp, INET_ADDRSTRLEN);

        char dstIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &networkDstAddr, dstIp, INET_ADDRSTRLEN);

        uint8_t proto = ipPacket.protocol;

        // decide whether this datagram also needs CAI delivery, but do not
        // reflect it until sequence/duplicate validation has succeeded
        bool reflectBroadcast = status->assembler.dataHeader.getLLId() == WUID_ALL;
        uint32_t routedLlId = m_scepService.routeDownlink(ipPacket.destinationAddress);
        bool reflectRouted = routedLlId != 0U;
        bool handled = reflectBroadcast || reflectRouted;

        // sequence validation - check N(S) against V(R)
        uint8_t receivedNs = status->assembler.dataHeader.getNs();
        bool synchronize = status->assembler.dataHeader.getSynchronize();

        uint8_t expectedNs = 0U;
        uint32_t packetFingerprint = DataLinkManager::fingerprint(
            status->pduUserData, pktLen);
        ReceiveSequenceResult sequenceResult =
            m_dataLinkManager.acceptReceiveSequence(srcLlId, receivedNs, synchronize,
                packetFingerprint, expectedNs);
        if (sequenceResult == ReceiveSequenceResult::DUPLICATE) {
            // re-ACK a duplicate, but never deliver the IP datagram twice
            if (status->assembler.getExtendedAddress()) {
                write_PDU_Ack_Response(PDUAckClass::ACK, PDUAckType::ACK, receivedNs,
                    srcLlId, true, dstLlId);
            } else {
                write_PDU_Ack_Response(PDUAckClass::ACK, PDUAckType::ACK, receivedNs,
                    srcLlId, false);
            }
            break;
        }
        if (sequenceResult == ReceiveSequenceResult::OUT_OF_SEQUENCE) {
            // out of sequence - send NACK_OUT_OF_SEQ
            LogWarning(LOG_P25, P25_PDU_STR ", NACK_OUT_OF_SEQ, llId %u, expected N(S) %u or %u, received N(S) = %u", 
                srcLlId, expectedNs, (expectedNs + 1) % 8, receivedNs);
            if (status->assembler.getExtendedAddress()) {
                write_PDU_Ack_Response(PDUAckClass::NACK, PDUAckType::NACK_OUT_OF_SEQ, expectedNs, srcLlId, true, dstLlId);
            } else {
                write_PDU_Ack_Response(PDUAckClass::NACK, PDUAckType::NACK_OUT_OF_SEQ, expectedNs, srcLlId, false);
            }
            break; // don't process out-of-sequence packet
        }

        if (handled) {
            LogInfoEx(LOG_P25, "PDU -> VTUN, IP Data, repeated to CAI, dstIp = %s (%u)",
                dstIp, reflectBroadcast ? WUID_ALL : routedLlId);
            dispatchUserFrameToFNE(status->assembler.dataHeader,
                status->assembler.getExtendedAddress(), status->assembler.getAuxiliaryES(),
                status->pduUserData);
        }

        // transmit packet to IP network
        LogInfoEx(LOG_P25, "PDU -> VTUN, IP Data, srcIp = %s (%u), dstIp = %s (%u), pktLen = %u, proto = %02X%s", 
            srcIp, srcLlId, dstIp, dstLlId, pktLen, proto, (proto == 0x01) ? " (ICMP)" : "");

        DECLARE_UINT8_ARRAY(ipFrame, pktLen);
        ::memcpy(ipFrame, status->pduUserData, pktLen);
#if DEBUG_P25_PDU_DATA
        Utils::dump(1U, "P25, P25PacketData::dispatch(), ipFrame", ipFrame, pktLen);
#endif
        if (!m_network->m_host->m_tun->write(ipFrame, pktLen)) {
            LogError(LOG_P25, P25_PDU_STR ", failed to write IP frame to virtual tunnel, len %u", pktLen);
        }

        // if the packet is unhandled and sent off to VTUN; ack the packet so the sender knows we received it
        if (!handled) {
            //LogDebugEx(LOG_P25, "P25PacketData::dispatch()", "marking llId %u ready for next packet (proto = %02X)", srcLlId, proto);
            if (status->assembler.getExtendedAddress()) {
                m_dataLinkManager.setReady(srcLlId, true);
                write_PDU_Ack_Response(PDUAckClass::ACK, PDUAckType::ACK, receivedNs, srcLlId, true, dstLlId);
            } else {
                m_dataLinkManager.setReady(srcLlId, true);
                write_PDU_Ack_Response(PDUAckClass::ACK, PDUAckType::ACK, receivedNs, srcLlId, false);
            }
        }
#endif // !defined(_WIN32)
    }
    break;
    case PDUSAP::CONV_DATA_REG:
    {
        LogInfoEx(LOG_P25, P25_PDU_STR ", CONV_DATA_REG (Conventional Data Registration), peer = %u, blocksToFollow = %u",
            peerId, status->assembler.dataHeader.getBlocksToFollow());

        processConvDataReg(status);
    }
    break;
    case PDUSAP::SNDCP_CTRL_DATA:
    {
        LogInfoEx(LOG_P25, P25_PDU_STR ", SNDCP_CTRL_DATA (SNDCP Control Data), peer = %u, blocksToFollow = %u",
            peerId, status->assembler.dataHeader.getBlocksToFollow());

        processSNDCPControl(status);
    }
    break;
    case PDUSAP::UNENC_KMM:
    case PDUSAP::ENC_KMM:
    {
        LogInfoEx(LOG_P25, P25_PDU_STR ", KMM (Key Management Message), peer = %u, blocksToFollow = %u",
            peerId, status->assembler.dataHeader.getBlocksToFollow());

        bool encrypted = (status->assembler.dataHeader.getSAP() == PDUSAP::ENC_KMM ||
            status->assembler.dataHeader.getSAP() == PDUSAP::ENC_USER_DATA);
        uint8_t algId = P25DEF::ALGO_UNENCRYPT;
        uint16_t kId = 0U;
        uint8_t mi[MI_LENGTH_BYTES];
        ::memset(mi, 0x00U, MI_LENGTH_BYTES);

        // if this was transported as encrypted user data with auxiliary ES,
        // the KMM appears as UNENC_KMM via EXSAP but is still encrypted
        if (status->assembler.getAuxiliaryES() && status->assembler.dataHeader.getSAP() == PDUSAP::ENC_USER_DATA) {
            algId = status->assembler.dataHeader.getAlgId();
            kId = status->assembler.dataHeader.getKId();
            status->assembler.dataHeader.getMI(mi);
        }

        m_network->m_p25OTARService->processDLD(status->pduUserData, status->pduUserDataLength, status->llId, 
            status->assembler.dataHeader.getNs(), encrypted, algId, kId, encrypted ? mi : nullptr);
    }
    break;
    default:
        dispatchToFNE(peerId);
        break;
    }
}

/* Helper to dispatch PDU user data back to the FNE network. */

void P25PacketData::dispatchToFNE(uint32_t peerId)
{
    RxStatus* status = m_status[peerId];

    uint32_t srcId = (status->assembler.getExtendedAddress()) ? status->assembler.dataHeader.getSrcLLId() : status->assembler.dataHeader.getLLId();
    uint32_t dstId = status->assembler.dataHeader.getLLId();

    std::vector<DataRoute> routes;
    bool groupDelivery = dstId == WUID_ALL;
    if (groupDelivery) {
        std::vector<uint32_t> availablePeers;
        for (const auto& peer : m_network->m_peers)
            availablePeers.push_back(peer.first);

        for (const auto& peer : m_network->m_host->m_peerNetworks) {
            if (peer.second->isEnabled())
                availablePeers.push_back(peer.second->getPeerId());
        }

        routes = m_locationRegistry.resolveGroup(availablePeers);
    }
    else {
        uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        DataRoute route = m_locationRegistry.resolve(dstId, AccessMode::CONVENTIONAL, now);
        if (!route.valid) {
            LogWarning(LOG_P25, P25_PDU_STR ", no RF route for relayed packet, llId = %u", dstId);
            return;
        }

        routes.push_back(route);
    }

    auto selectedPeer = [&routes](uint32_t candidatePeerId) {
        return std::any_of(routes.cbegin(), routes.cend(), [candidatePeerId](const DataRoute& route) { return route.peerId == candidatePeerId; });
    };

    /*
    ** MASTER TRAFFIC
    */

    // repeat traffic to the connected peers
    if (m_network->m_peers.size() > 0U) {
        for (auto peer : m_network->m_peers) {
            if (peerId != peer.first && selectedPeer(peer.first)) {
                write_PDU_User(peer.first, peerId, nullptr, status->assembler.dataHeader, status->assembler.getExtendedAddress(),
                    status->assembler.getAuxiliaryES(), status->pduUserData);
                if (m_network->m_debug) {
                    LogDebug(LOG_P25, "srcPeer = %u, dstPeer = %u, duid = $%02X, srcId = %u, dstId = %u", 
                        peerId, peer.first, DUID::PDU, srcId, dstId);
                }
            }
        }
    }

    /*
    ** PEER TRAFFIC (e.g. upstream networks this FNE is peered to)
    */

    // repeat traffic to neighbor FNE peers
    if (m_network->m_host->m_peerNetworks.size() > 0U) {
        for (auto peer : m_network->m_host->m_peerNetworks) {
            uint32_t dstPeerId = peer.second->getPeerId();

            // don't try to repeat traffic to the source peer...if this traffic
            // is coming from a neighbor FNE peer
            if (dstPeerId != peerId && selectedPeer(dstPeerId)) {
                // skip peer if it isn't enabled
                if (!peer.second->isEnabled()) {
                    continue;
                }

                write_PDU_User(dstPeerId, peerId, peer.second, status->assembler.dataHeader, status->assembler.getExtendedAddress(),
                    status->assembler.getAuxiliaryES(), status->pduUserData);
                if (m_network->m_debug) {
                    LogDebug(LOG_P25, "srcPeer = %u, dstPeer = %u, duid = $%02X, srcId = %u, dstId = %u", 
                        peerId, dstPeerId, DUID::PDU, srcId, dstId);
                }
            }
        }
    }
}

/* Helper to dispatch PDU user data back to the local FNE network. (Will not transmit to neighbor FNE peers.) */

bool P25PacketData::dispatchUserFrameToFNE(DataHeader& dataHeader, bool extendedAddress, bool auxiliaryES, uint8_t* pduUserData)
{
    uint32_t srcId = (extendedAddress) ? dataHeader.getSrcLLId() : dataHeader.getLLId();
    uint32_t dstId = dataHeader.getLLId();

    std::vector<DataRoute> routes;
    if (dstId == WUID_ALL) {
        std::vector<uint32_t> availablePeers;
        for (const auto& peer : m_network->m_peers)
            availablePeers.push_back(peer.first);

        routes = m_locationRegistry.resolveGroup(availablePeers);
    }
    else {
        uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        DataRoute route = m_locationRegistry.resolve(dstId, AccessMode::CONVENTIONAL, now);
        if (route.valid)
            routes.push_back(route);
    }

    // if no routes are available, log a warning and return false
    if (routes.empty()) {
        LogWarning(LOG_P25, P25_PDU_STR ", no RF route for downlink, llId = %u", dstId);
        return false;
    }

    // update V(S) only after a usable RF route has been selected
    bool synchronize = false;
    uint8_t sendSequence = m_dataLinkManager.nextSendSequence(srcId, synchronize);
    if (synchronize)
        dataHeader.setSynchronize(true);
    dataHeader.setNs(sendSequence);

    // dispatch the user frame to each resolved RF route
    bool dispatched = false;
    for (const DataRoute& route : routes) {
        auto localPeer = m_network->m_peers.find(route.peerId);
        if (localPeer != m_network->m_peers.end()) {
            dispatched = write_PDU_User(route.peerId, m_network->m_peerId, nullptr, dataHeader,
                extendedAddress, auxiliaryES, pduUserData) || dispatched;
        }
        else {
            for (const auto& peer : m_network->m_host->m_peerNetworks) {
                if (peer.second->getPeerId() == route.peerId && peer.second->isEnabled()) {
                    dispatched = write_PDU_User(route.peerId, m_network->m_peerId, peer.second,
                        dataHeader, extendedAddress, auxiliaryES, pduUserData) || dispatched;
                    break;
                }
            }
        }

        if (m_network->m_debug) {
            LogDebug(LOG_P25, "dstPeer = %u, duid = $%02X, srcId = %u, dstId = %u",
                route.peerId, DUID::PDU, srcId, dstId);
        }
    }
    return dispatched;
}

/* Helper used to process conventional data registration from PDU data. */

bool P25PacketData::processConvDataReg(RxStatus* status)
{
    if (status == nullptr || status->pduUserData == nullptr) {
        LogError(LOG_P25, P25_PDU_STR ", truncated conventional registration payload");
        return false;
    }

    ConventionalRegistration registration;
    if (!ConventionalRegistration::decode(status->pduUserData,
        status->pduUserDataLength, registration)) {
        LogError(LOG_P25, P25_PDU_STR ", invalid conventional registration payload");
        return false;
    }

    ConventionalRegistrationProvisioning provisioning;
    lookups::RadioId rid = m_network->m_ridLookup->find(registration.llId);
    provisioning.known = !rid.radioDefault();
    provisioning.enabled = provisioning.known && rid.radioEnabled();
    provisioning.allowDynamicAddress = provisioning.enabled && rid.radioIPAddress().empty();
    if (provisioning.enabled && !rid.radioIPAddress().empty())
        provisioning.staticIPAddress = __IP_FROM_STR(rid.radioIPAddress());

    uint32_t headerLlId = status->assembler.getExtendedAddress() ?
        status->assembler.dataHeader.getSrcLLId() : status->assembler.dataHeader.getLLId();
    ConventionalRegistrationResult result = m_conventionalDataService.process(
        status->pduUserData, status->pduUserDataLength, headerLlId, provisioning);

    if (result.decision == RegistrationDecision::DISCONNECTED) {
        m_neighborCache.erase(registration.llId);
        m_locationRegistry.erase(registration.llId, AccessMode::CONVENTIONAL);
        LogInfoEx(LOG_P25, P25_PDU_STR ", DISCONNECT, llId = %u", registration.llId);
        return true;
    }

    if (result.decision == RegistrationDecision::ACCEPT) {
        ConventionalLocation location;
        location.peerId = status->peerId;
        location.lastSeen = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        m_locationRegistry.updateConventional(registration.llId, location);
    }

    // only the FNE emits the registration decision -- the host merely validates
    // and forwards the request, so an ACCEPT always reflects installed state
    uint8_t responseData[ConventionalRegistration::LENGTH];
    if (!result.response.encode(responseData, sizeof(responseData)))
        return false;

    data::DataHeader responseHeader;
    responseHeader.setFormat(PDUFormatType::CONFIRMED);
    responseHeader.setMFId(MFG_STANDARD);
    responseHeader.setAckNeeded(true);
    responseHeader.setOutbound(true);
    responseHeader.setSAP(PDUSAP::CONV_DATA_REG);
    responseHeader.setLLId(registration.llId);
    responseHeader.calculateLength(sizeof(responseData));
    dispatchUserFrameToFNE(responseHeader, false, false, responseData);

    if (result.decision == RegistrationDecision::ACCEPT) {
        LogInfoEx(LOG_P25, P25_PDU_STR ", registration ACCEPT, llId = %u, ipAddr = %s",
            registration.llId, __IP_FROM_UINT(result.response.ipAddress).c_str());
        return true;
    }

    LogWarning(LOG_P25, P25_PDU_STR ", registration DENY, llId = %u, reason = %u (%s)",
        registration.llId, uint8_t(result.denyReason), ConventionalDataService::denyReasonToString(result.denyReason).c_str());
    return false;
}

/* Helper used to process SNDCP control data from PDU data. */

bool P25PacketData::processSNDCPControl(RxStatus* status)
{
    std::unique_ptr<sndcp::SNDCPPacket> packet = SNDCPFactory::create(status->pduUserData);
    if (packet == nullptr) {
        LogWarning(LOG_P25, P25_PDU_STR ", undecodable SNDCP packet");
        return false;
    }

    uint32_t llId = status->assembler.dataHeader.getLLId();

    switch (packet->getPDUType()) {
        case SNDCP_PDUType::ACT_TDS_CTX:
        {
            SNDCPCtxActRequest* isp = static_cast<SNDCPCtxActRequest*>(packet.get());
            LogInfoEx(LOG_P25, P25_PDU_STR ", SNDCP context activation request, llId = %u, nsapi = %u, ipAddr = %s, nat = $%02X, dsut = $%02X, mdpco = $%02X", llId,
                isp->getNSAPI(), __IP_FROM_UINT(isp->getIPAddress()).c_str(), isp->getNAT(), isp->getDSUT(), isp->getMDPCO());

            // check if subscriber is provisioned (from RID table)
            lookups::RadioId rid = m_network->m_ridLookup->find(llId);
            if (rid.radioDefault() || !rid.radioEnabled()) {
                uint8_t txPduUserData[P25_MAX_PDU_BLOCKS * P25_PDU_UNCONFIRMED_LENGTH_BYTES];
                ::memset(txPduUserData, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_UNCONFIRMED_LENGTH_BYTES);

                std::unique_ptr<SNDCPCtxActReject> osp = std::make_unique<SNDCPCtxActReject>();
                osp->setNSAPI(isp->getNSAPI());
                osp->setRejectCode(SNDCPRejectReason::SU_NOT_PROVISIONED);
                osp->encode(txPduUserData);

                // Build response header
                data::DataHeader rspHeader;
                rspHeader.setFormat(PDUFormatType::CONFIRMED);
                rspHeader.setMFId(MFG_STANDARD);
                rspHeader.setAckNeeded(true);
                rspHeader.setOutbound(true);
                rspHeader.setSAP(PDUSAP::SNDCP_CTRL_DATA);
                rspHeader.setLLId(llId);
                rspHeader.setBlocksToFollow(1U);
                rspHeader.calculateLength(2U);

                dispatchUserFrameToFNE(rspHeader, false, false, txPduUserData);

                LogWarning(LOG_P25, P25_PDU_STR ", SNDCP context activation reject, llId = %u, reason = SU_NOT_PROVISIONED", llId);
                return true;
            }

            // handle different network address types
            switch (isp->getNAT()) {
                case SNDCPNAT::IPV4_STATIC_ADDR:
                {
                    // get static IP from RID table
                    uint32_t staticIP = 0U;
                    if (!rid.radioDefault()) {
                        std::string addr = rid.radioIPAddress();
                        staticIP = __IP_FROM_STR(addr);
                    }

                    if (staticIP == 0U) {
                        // no static IP configured - reject
                        uint8_t txPduUserData[P25_MAX_PDU_BLOCKS * P25_PDU_UNCONFIRMED_LENGTH_BYTES];
                        ::memset(txPduUserData, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_UNCONFIRMED_LENGTH_BYTES);

                        std::unique_ptr<SNDCPCtxActReject> osp = std::make_unique<SNDCPCtxActReject>();
                        osp->setNSAPI(isp->getNSAPI());
                        osp->setRejectCode(SNDCPRejectReason::STATIC_IP_ALLOCATION_UNSUPPORTED);
                        osp->encode(txPduUserData);

                        data::DataHeader rspHeader;
                        rspHeader.setFormat(PDUFormatType::CONFIRMED);
                        rspHeader.setMFId(MFG_STANDARD);
                        rspHeader.setAckNeeded(true);
                        rspHeader.setOutbound(true);
                        rspHeader.setSAP(PDUSAP::SNDCP_CTRL_DATA);
                        rspHeader.setLLId(llId);
                        rspHeader.setBlocksToFollow(1U);
                        rspHeader.calculateLength(2U);

                        dispatchUserFrameToFNE(rspHeader, false, false, txPduUserData);

                        LogWarning(LOG_P25, P25_PDU_STR ", SNDCP context activation reject, llId = %u, reason = STATIC_IP_ALLOCATION_UNSUPPORTED", llId);
                        return true;
                    }

                    // Accept with static IP
                    uint8_t txPduUserData[P25_MAX_PDU_BLOCKS * P25_PDU_UNCONFIRMED_LENGTH_BYTES];
                    ::memset(txPduUserData, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_UNCONFIRMED_LENGTH_BYTES);

                    std::unique_ptr<SNDCPCtxActAccept> osp = std::make_unique<SNDCPCtxActAccept>();
                    osp->setNSAPI(isp->getNSAPI());
                    osp->setPriority(4U);
                    osp->setReadyTimer(SNDCPReadyTimer::TEN_SECONDS);
                    osp->setStandbyTimer(SNDCPStandbyTimer::ONE_MINUTE);
                    osp->setNAT(SNDCPNAT::IPV4_STATIC_ADDR);
                    osp->setIPAddress(staticIP);
                    osp->setMTU(SNDCP_MTU_510);
                    osp->setMDPCO(isp->getMDPCO());
                    osp->encode(txPduUserData);

                    data::DataHeader rspHeader;
                    rspHeader.setFormat(PDUFormatType::CONFIRMED);
                    rspHeader.setMFId(MFG_STANDARD);
                    rspHeader.setAckNeeded(true);
                    rspHeader.setOutbound(true);
                    rspHeader.setSAP(PDUSAP::SNDCP_CTRL_DATA);
                    rspHeader.setLLId(llId);
                    rspHeader.setBlocksToFollow(1U);
                    rspHeader.calculateLength(13U);

                    m_neighborCache.observe(llId, staticIP);
                    m_dataLinkManager.setReady(llId, true);

                    dispatchUserFrameToFNE(rspHeader, false, false, txPduUserData);

                    LogInfoEx(LOG_P25, P25_PDU_STR ", SNDCP context activation accept, llId = %u, ipAddr = %s (static)", 
                        llId, __IP_FROM_UINT(staticIP).c_str());
                }
                break;

                case SNDCPNAT::IPV4_DYN_ADDR:
                {
                    // allocate dynamic IP
                    uint32_t dynamicIP = allocateIPAddress(llId);
                    if (dynamicIP == 0U) {
                        // IP pool exhausted - reject
                        uint8_t txPduUserData[P25_MAX_PDU_BLOCKS * P25_PDU_UNCONFIRMED_LENGTH_BYTES];
                        ::memset(txPduUserData, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_UNCONFIRMED_LENGTH_BYTES);

                        std::unique_ptr<SNDCPCtxActReject> osp = std::make_unique<SNDCPCtxActReject>();
                        osp->setNSAPI(isp->getNSAPI());
                        osp->setRejectCode(SNDCPRejectReason::DYN_IP_POOL_EMPTY);
                        osp->encode(txPduUserData);

                        data::DataHeader rspHeader;
                        rspHeader.setFormat(PDUFormatType::CONFIRMED);
                        rspHeader.setMFId(MFG_STANDARD);
                        rspHeader.setAckNeeded(true);
                        rspHeader.setOutbound(true);
                        rspHeader.setSAP(PDUSAP::SNDCP_CTRL_DATA);
                        rspHeader.setLLId(llId);
                        rspHeader.setBlocksToFollow(1U);
                        rspHeader.calculateLength(2U);

                        dispatchUserFrameToFNE(rspHeader, false, false, txPduUserData);

                        LogWarning(LOG_P25, P25_PDU_STR ", SNDCP context activation reject, llId = %u, reason = DYN_IP_POOL_EMPTY", llId);
                        return true;
                    }

                    // accept with dynamic IP
                    uint8_t txPduUserData[P25_MAX_PDU_BLOCKS * P25_PDU_UNCONFIRMED_LENGTH_BYTES];
                    ::memset(txPduUserData, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_UNCONFIRMED_LENGTH_BYTES);

                    std::unique_ptr<SNDCPCtxActAccept> osp = std::make_unique<SNDCPCtxActAccept>();
                    osp->setNSAPI(isp->getNSAPI());
                    osp->setPriority(4U);
                    osp->setReadyTimer(SNDCPReadyTimer::TEN_SECONDS);
                    osp->setStandbyTimer(SNDCPStandbyTimer::ONE_MINUTE);
                    osp->setNAT(SNDCPNAT::IPV4_DYN_ADDR);
                    osp->setIPAddress(dynamicIP);
                    osp->setMTU(SNDCP_MTU_510);
                    osp->setMDPCO(isp->getMDPCO());
                    osp->encode(txPduUserData);

                    data::DataHeader rspHeader;
                    rspHeader.setFormat(PDUFormatType::CONFIRMED);
                    rspHeader.setMFId(MFG_STANDARD);
                    rspHeader.setAckNeeded(true);
                    rspHeader.setOutbound(true);
                    rspHeader.setSAP(PDUSAP::SNDCP_CTRL_DATA);
                    rspHeader.setLLId(llId);
                    rspHeader.setBlocksToFollow(1U);
                    rspHeader.calculateLength(13U);

                    m_neighborCache.observe(llId, dynamicIP);
                    m_dataLinkManager.setReady(llId, true);

                    dispatchUserFrameToFNE(rspHeader, false, false, txPduUserData);

                    LogInfoEx(LOG_P25, P25_PDU_STR ", SNDCP context activation accept, llId = %u, ipAddr = %s (dynamic)", 
                        llId, __IP_FROM_UINT(dynamicIP).c_str());
                }
                break;

                default:
                {
                    // unsupported NAT type - reject
                    uint8_t txPduUserData[P25_MAX_PDU_BLOCKS * P25_PDU_UNCONFIRMED_LENGTH_BYTES];
                    ::memset(txPduUserData, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_UNCONFIRMED_LENGTH_BYTES);
                    
                    std::unique_ptr<SNDCPCtxActReject> osp = std::make_unique<SNDCPCtxActReject>();
                    osp->setNSAPI(isp->getNSAPI());
                    osp->setRejectCode(SNDCPRejectReason::ANY_REASON);
                    osp->encode(txPduUserData);

                    data::DataHeader rspHeader;
                    rspHeader.setFormat(PDUFormatType::CONFIRMED);
                    rspHeader.setMFId(MFG_STANDARD);
                    rspHeader.setAckNeeded(true);
                    rspHeader.setOutbound(true);
                    rspHeader.setSAP(PDUSAP::SNDCP_CTRL_DATA);
                    rspHeader.setLLId(llId);
                    rspHeader.setBlocksToFollow(1U);
                    rspHeader.calculateLength(2U);

                    dispatchUserFrameToFNE(rspHeader, false, false, txPduUserData);

                    LogWarning(LOG_P25, P25_PDU_STR ", SNDCP context activation reject, llId = %u, reason = UNSUPPORTED_NAT", llId);
                }
                break;
            }
        }
        break;

        case SNDCP_PDUType::DEACT_TDS_CTX_REQ:
        {
            SNDCPCtxDeactivation* isp = static_cast<SNDCPCtxDeactivation*>(packet.get());
            LogInfoEx(LOG_P25, P25_PDU_STR ", SNDCP context deactivation request, llId = %u, deactType = %02X", llId,
                isp->getDeactType());

            m_neighborCache.erase(llId);
            m_dataLinkManager.erase(llId);

            // send ACK response
            write_PDU_Ack_Response(PDUAckClass::ACK, PDUAckType::ACK, 
                status->assembler.dataHeader.getNs(), llId, false);
        }
        break;

        default:
        break;
    } // switch (packet->getPDUType())

    return true;
}

/* Helper write ARP request to the network. */

void P25PacketData::write_PDU_ARP(uint32_t addr)
{
#if !defined(_WIN32)
    if (!m_network->m_host->m_vtunEnabled)
        return;

    uint8_t arpPacket[P25_PDU_ARP_PCKT_LENGTH];
    ::memset(arpPacket, 0x00U, P25_PDU_ARP_PCKT_LENGTH);

    SET_UINT16(P25_PDU_ARP_CAI_TYPE, arpPacket, 0U);            // Hardware Address Type
    SET_UINT16(PDUSAP::PACKET_DATA, arpPacket, 2U);             // Protocol Address Type
    arpPacket[4U] = P25_PDU_ARP_HW_ADDR_LENGTH;                 // Hardware Address Length
    arpPacket[5U] = P25_PDU_ARP_PROTO_ADDR_LENGTH;              // Protocol Address Length
    SET_UINT16(P25_PDU_ARP_REQUEST, arpPacket, 6U);             // Opcode

    SET_UINT24(WUID_FNE, arpPacket, 8U);                        // Sender Hardware Address

    std::string fneIPv4 = m_network->m_host->m_tun->getIPv4();
    SET_UINT32(__IP_FROM_STR(fneIPv4), arpPacket, 11U);         // Sender Protocol Address

    SET_UINT32(addr, arpPacket, 18U);                           // Target Protocol Address
#if DEBUG_P25_PDU_DATA
    Utils::dump(1U, "P25, P25PacketData::write_PDU_ARP(), arpPacket", arpPacket, P25_PDU_ARP_PCKT_LENGTH);
#endif
    LogInfoEx(LOG_P25, P25_PDU_STR ", ARP request, who has %s? tell %s (%u)", __IP_FROM_UINT(addr).c_str(), fneIPv4.c_str(), WUID_FNE);

    // assemble a P25 PDU frame header for transport...
    data::DataHeader rspHeader = data::DataHeader();
    rspHeader.setFormat(PDUFormatType::UNCONFIRMED);
    rspHeader.setMFId(MFG_STANDARD);
    rspHeader.setAckNeeded(false);
    rspHeader.setOutbound(true);
    rspHeader.setSAP(PDUSAP::EXT_ADDR);
    rspHeader.setLLId(WUID_ALL);
    rspHeader.setBlocksToFollow(1U);

    rspHeader.setEXSAP(PDUSAP::ARP);
    rspHeader.setSrcLLId(WUID_FNE);

    rspHeader.calculateLength(P25_PDU_ARP_PCKT_LENGTH);
    uint32_t pduLength = rspHeader.getPDULength();

    DECLARE_UINT8_ARRAY(pduUserData, pduLength);
    ::memcpy(pduUserData + P25_PDU_HEADER_LENGTH_BYTES, arpPacket, P25_PDU_ARP_PCKT_LENGTH);

    dispatchUserFrameToFNE(rspHeader, true, false, pduUserData);
#endif // !defined(_WIN32)
}

/* Helper write ARP reply to the network. */

void P25PacketData::write_PDU_ARP_Reply(uint32_t targetAddr, uint32_t requestorLlid, uint32_t requestorAddr, uint32_t targetLlid)
{
    if (!m_network->m_host->m_vtunEnabled)
        return;

    uint32_t tgtLlid = getLLIdAddress(targetAddr);
    if (targetLlid != 0U) {
        tgtLlid = targetLlid; // forcibly override
    }
    if (tgtLlid == 0U)
        return;

    uint8_t arpPacket[P25_PDU_ARP_PCKT_LENGTH];
    ::memset(arpPacket, 0x00U, P25_PDU_ARP_PCKT_LENGTH);

    SET_UINT16(P25_PDU_ARP_CAI_TYPE, arpPacket, 0U);            // Hardware Address Type
    SET_UINT16(PDUSAP::PACKET_DATA, arpPacket, 2U);             // Protocol Address Type
    arpPacket[4U] = P25_PDU_ARP_HW_ADDR_LENGTH;                 // Hardware Address Length
    arpPacket[5U] = P25_PDU_ARP_PROTO_ADDR_LENGTH;              // Protocol Address Length
    SET_UINT16(P25_PDU_ARP_REPLY, arpPacket, 6U);               // Opcode

    SET_UINT24(tgtLlid, arpPacket, 8U);                         // Sender Hardware Address
    SET_UINT32(targetAddr, arpPacket, 11U);                     // Sender Protocol Address

    SET_UINT24(requestorLlid, arpPacket, 15U);                  // Requestor Hardware Address
    SET_UINT32(requestorAddr, arpPacket, 18U);                  // Requestor Protocol Address
#if DEBUG_P25_PDU_DATA
    Utils::dump(1U, "P25, P25PacketData::write_PDU_ARP_Reply(), arpPacket", arpPacket, P25_PDU_ARP_PCKT_LENGTH);
#endif
    LogInfoEx(LOG_P25, P25_PDU_STR ", ARP reply, %s is at %u", __IP_FROM_UINT(targetAddr).c_str(), tgtLlid);

    // assemble a P25 PDU frame header for transport...
    data::DataHeader rspHeader = data::DataHeader();
    rspHeader.setFormat(PDUFormatType::UNCONFIRMED);
    rspHeader.setMFId(MFG_STANDARD);
    rspHeader.setAckNeeded(false);
    rspHeader.setOutbound(true);
    rspHeader.setSAP(PDUSAP::EXT_ADDR);
    rspHeader.setLLId(WUID_ALL);
    rspHeader.setBlocksToFollow(1U);

    rspHeader.setEXSAP(PDUSAP::ARP);
    rspHeader.setSrcLLId(WUID_FNE);

    rspHeader.calculateLength(P25_PDU_ARP_PCKT_LENGTH);
    uint32_t pduLength = rspHeader.getPDULength();

    DECLARE_UINT8_ARRAY(pduUserData, pduLength);
    ::memcpy(pduUserData + P25_PDU_HEADER_LENGTH_BYTES, arpPacket, P25_PDU_ARP_PCKT_LENGTH);

    dispatchUserFrameToFNE(rspHeader, true, false, pduUserData);
}

/* Helper to write user data as a P25 PDU packet. */

bool P25PacketData::write_PDU_User(uint32_t peerId, uint32_t srcPeerId, network::PeerNetwork* peerNet, data::DataHeader& dataHeader,
    bool extendedAddress, bool auxiliaryES, uint8_t* pduUserData)
{
    uint32_t streamId = m_network->createStreamId();
    uint16_t pktSeq = 0U;

    if (pduUserData == nullptr)
        pktSeq = RTP_END_OF_CALL_SEQ;

    UserContext* context = new UserContext();
    context->obj = this;
    context->peerId = peerId;
    context->srcPeerId = srcPeerId;
    context->peerNet = peerNet;
    context->header = new data::DataHeader(dataHeader);
    context->pktSeq = pktSeq;
    context->streamId = streamId;
    context->success = true;

    m_assembler->assemble(dataHeader, extendedAddress, auxiliaryES, pduUserData, nullptr, context);
    const bool success = context->success;
    delete context->header;
    delete context;
    return success;
}

/* Write data processed to the network. */

bool P25PacketData::writeNetwork(uint32_t peerId, uint32_t srcPeerId, network::PeerNetwork* peerNet, const DataHeader& dataHeader, const uint8_t currentBlock, 
    const uint8_t *data, uint32_t len, uint16_t pktSeq, uint32_t streamId)
{
    assert(data != nullptr);

    uint32_t messageLength = 0U;
    UInt8Array message = m_network->createP25_PDUMessage(messageLength, dataHeader, currentBlock, data, len);
    if (message == nullptr) {
        return false;
    }

    if (peerNet != nullptr) {
        return peerNet->writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, pktSeq, streamId);
    } else {
        return m_network->writePeer(peerId, srcPeerId, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, pktSeq, streamId);
    }
}

/* Helper to determine if the logical link ID has an ARP entry. */

bool P25PacketData::hasARPEntry(uint32_t llId) const
{
    if (llId == 0U) {
        return false;
    }

    const RouteNeighbor* neighbor = m_neighborCache.findBySubscriberId(llId);
    return neighbor != nullptr && neighbor->ipAddress != 0U;
}

/* Helper to get the IP address for the given logical link ID. */

uint32_t P25PacketData::getIPAddress(uint32_t llId)
{
    if (llId == 0U) {
        return 0U;
    }

    const IPBinding* binding = m_bindingRegistry.findByLLId(llId);
    if (binding != nullptr && binding->authorized) {
        return binding->ipAddress;
    }

    // provisioned RID addresses are authoritative; ARP is discovery only and
    // must never create packet-data authorization
    lookups::RadioId rid = m_network->m_ridLookup->find(llId);
    if (!rid.radioDefault() && rid.radioEnabled() && !rid.radioIPAddress().empty()) {
        uint32_t ipAddr = __IP_FROM_STR(rid.radioIPAddress());
        if (ipAddr != 0U && m_conventionalDataService.installStatic(llId, ipAddr))
            return ipAddr;
    }

    return 0U;
}

/* Helper to get the logical link ID for the given IP address. */

uint32_t P25PacketData::getLLIdAddress(uint32_t addr)
{
    if (addr == 0U) {
        return 0U;
    }

    const IPBinding* binding = m_bindingRegistry.findByIPAddress(addr);
    if (binding != nullptr && binding->authorized)
        return binding->link.llId;

    // lookup IP from static RID table
    std::string ipAddr = __IP_FROM_UINT(addr);
    std::unordered_map<uint32_t, lookups::RadioId> ridTable = m_network->m_ridLookup->table();
    auto it = std::find_if(ridTable.begin(), ridTable.end(), [&](std::pair<const uint32_t, lookups::RadioId> x) {
        if (x.second.radioIPAddress() == ipAddr) {
            if (x.second.radioEnabled() && !x.second.radioDefault())
                return true;
        }
        return false; 
    });
    if (it != ridTable.end()) {
        if (m_conventionalDataService.installStatic(it->first, addr))
            return it->first;
    }

    return 0U;
}

/* Helper to allocate a dynamic IP address for SNDCP. */

uint32_t P25PacketData::allocateIPAddress(uint32_t llId)
{
    uint32_t existingIP = getIPAddress(llId);
    if (existingIP != 0U) {
        return existingIP;
    }

    // sequential allocation from configurable pool with uniqueness check
    static uint32_t nextIP = 0U;

    // initialize nextIP on first call
    if (nextIP == 0U) {
        nextIP = m_network->m_sndcpStartAddr;
    }

    // find next available IP not already in use
    uint32_t candidateIP = nextIP;
    const uint32_t poolSize = m_network->m_sndcpEndAddr - m_network->m_sndcpStartAddr + 1U;
    uint32_t attempts = 0U;

    while ((m_neighborCache.findByIPAddress(candidateIP) != nullptr ||
        m_bindingRegistry.findByIPAddress(candidateIP) != nullptr) && attempts < poolSize) {
        candidateIP++;

        // wrap around if we exceed the end address
        if (candidateIP > m_network->m_sndcpEndAddr) {
            candidateIP = m_network->m_sndcpStartAddr;
        }

        attempts++;
    }

    if (attempts >= poolSize) {
        LogError(LOG_P25, P25_PDU_STR ", SNDCP dynamic IP pool exhausted for llId = %u (pool: %s - %s)", 
            llId, __IP_FROM_UINT(m_network->m_sndcpStartAddr).c_str(), __IP_FROM_UINT(m_network->m_sndcpEndAddr).c_str());
        return 0U; // Pool exhausted
    }

    // allocate the unique IP
    uint32_t allocatedIP = candidateIP;
    nextIP = candidateIP + 1U;

    // wrap around for next allocation if needed
    if (nextIP > m_network->m_sndcpEndAddr) {
        nextIP = m_network->m_sndcpStartAddr;
    }

    m_neighborCache.observe(llId, allocatedIP);
    LogInfoEx(LOG_P25, P25_PDU_STR ", SNDCP allocated dynamic IP %s to llId = %u (pool: %s - %s)", 
        __IP_FROM_UINT(allocatedIP).c_str(), llId, __IP_FROM_UINT(m_network->m_sndcpStartAddr).c_str(), __IP_FROM_UINT(m_network->m_sndcpEndAddr).c_str());

    return allocatedIP;
}
