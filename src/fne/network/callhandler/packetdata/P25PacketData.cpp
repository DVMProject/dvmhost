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
using namespace p25;
using namespace p25::defines;
using namespace p25::kmm;
using namespace p25::sndcp;

#include <cassert>
#include <chrono>
#include <unordered_set>

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

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the P25PacketData class. */

P25PacketData::P25PacketData(TrafficNetwork* network, TagP25Data* tag, bool debug) :
    m_network(network),
    m_tag(tag),
    m_assembler(nullptr),
    m_queuedFrames(),
    m_status(),
    m_arpTable(),
    m_readyForNextPkt(),
    m_suSendSeq(),
    m_suRecvSeq(),
    m_debug(debug)
{
    assert(network != nullptr);
    assert(tag != nullptr);

    data::Assembler::setVerbose(network->m_verbose);
    data::Assembler::setDumpPDUData(network->m_dumpPacketData);

    m_assembler = new data::Assembler();
    m_assembler->setBlockWriter([](const void* userContext, const uint8_t currentBlock, const uint8_t *data, uint32_t len, bool lastBlock) {
        const UserContext* context = static_cast<const UserContext*>(userContext);
        if (context == nullptr) {
            return;
        }

        P25PacketData* packetData = static_cast<P25PacketData*>(context->obj);
        if (packetData == nullptr) {
            return;
        }

        packetData->writeNetwork(context->peerId, context->srcPeerId, context->peerNet, *(context->header), 
            currentBlock, data, len, context->pktSeq, context->streamId);
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

    // make sure we don't get a PDU with more blocks then we support
    if (currentBlock >= P25_MAX_PDU_BLOCKS || status->totalBlocks > P25_MAX_PDU_BLOCKS) {
        LogError(LOG_P25, P25_PDU_STR ", too many PDU blocks to process, %u > %u", currentBlock, P25_MAX_PDU_BLOCKS);
        return false;
    }

    LogInfoEx(LOG_NET, P25_PDU_STR ", received block %u, peerId = %u, len = %u",
        currentBlock, peerId, blockLength);

    // store the received block
    uint8_t* blockData = new uint8_t[blockLength];
    ::memcpy(blockData, data + 24U, blockLength);
    status->receivedBlocks[currentBlock] = blockData;
    status->dataBlockCnt++;

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

                    m_readyForNextPkt[status->llId] = true;

                    // is this a response header?
                    if (status->assembler.dataHeader.getFormat() == PDUFormatType::RSP) {
                        dispatch(peerId);
                        status->streamId = 0U;
                        status->clearReceivedBlocks();
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
                                // report error event to InfluxDB
                                if (m_network->m_enableInfluxDB) {
                                    influxdb::QueryBuilder()
                                        .meas("call_error_event")
                                            .tag("peerId", std::to_string(peerId))
                                            .tag("streamId", std::to_string(streamId))
                                            .tag("srcId", std::to_string(status->assembler.dataHeader.getLLId()))
                                            .tag("dstId", std::to_string(status->assembler.dataHeader.getLLId()))
                                                .field("message", INFLUXDB_ERRSTR_DISABLED_SRC_RID)
                                            .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                                        .requestAsync(m_network->m_influxServer);
                                }

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
                                .requestAsync(m_network->m_influxServer);
                        }

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
    // validate minimum IPv4 header size
    if (len < sizeof(struct ip)) {
        LogError(LOG_P25, "VTUN packet too small: %u bytes (need minimum %u for IPv4 header)", len, (uint32_t)sizeof(struct ip));
        return;
    }

    // check IP version (must be IPv4)
    if ((data[0] & 0xF0) != 0x40) {
        LogWarning(LOG_P25, "VTUN non-IPv4 packet received, version = %u", (data[0] >> 4));
        return;
    }

    // validate Internet Header Length
    uint8_t ihl = (data[0] & 0x0F) * 4;  // IHL in 32-bit words, convert to bytes
    if (len < ihl || ihl < 20U) {
        LogError(LOG_P25, "VTUN packet has invalid or truncated IP header: len=%u, IHL=%u", len, ihl);
        return;
    }

    struct ip* ipHeader = (struct ip*)data;

    char srcIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ipHeader->ip_src), srcIp, INET_ADDRSTRLEN);

    char dstIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ipHeader->ip_dst), dstIp, INET_ADDRSTRLEN);

    uint8_t proto = ipHeader->ip_p;
    uint16_t pktLen = Utils::reverseEndian(ipHeader->ip_len); // bryanb: this could be problematic on different endianness

    // validate IP total length field against actual received length
    if (pktLen > len) {
        LogError(LOG_P25, "VTUN IP total length field (%u) exceeds actual packet size (%u)", pktLen, len);
        return;
    }

    if (pktLen < ihl) {
        LogError(LOG_P25, "VTUN IP total length (%u) is less than header length (%u)", pktLen, ihl);
        return;
    }

#if DEBUG_P25_PDU_DATA
    Utils::dump(1U, "P25, P25PacketData::processPacketFrame() packet", data, pktLen);
#endif

    uint32_t llId = getLLIdAddress(Utils::reverseEndian(ipHeader->ip_dst.s_addr));

    uint32_t srcProtoAddr = Utils::reverseEndian(ipHeader->ip_src.s_addr);
    uint32_t tgtProtoAddr = Utils::reverseEndian(ipHeader->ip_dst.s_addr);

    std::string srcIpStr = __IP_FROM_UINT(srcProtoAddr);
    std::string tgtIpStr = __IP_FROM_UINT(tgtProtoAddr);

    LogInfoEx(LOG_P25, "VTUN -> PDU IP Data, srcIp = %s (%u), dstIp = %s (%u), pktLen = %u, proto = %02X%s, llId = %u%s", 
        srcIpStr.c_str(), WUID_FNE, tgtIpStr.c_str(), llId, pktLen, proto, (proto == 0x01) ? " (ICMP)" : "",
        llId, (llId == 0U) ? " (UNRESOLVED - will retry with ARP)" : "");

    // assemble a P25 PDU frame header for transport...
    data::DataHeader* pktHeader = new data::DataHeader();
    pktHeader->setFormat(PDUFormatType::CONFIRMED);
    pktHeader->setMFId(MFG_STANDARD);
    pktHeader->setAckNeeded(true);
    pktHeader->setOutbound(true);
    pktHeader->setSAP(PDUSAP::PACKET_DATA);
    pktHeader->setLLId(llId);
    pktHeader->setBlocksToFollow(1U);

    pktHeader->calculateLength(pktLen);
    uint32_t pduLength = pktHeader->getPDULength();
    if (pduLength < pktLen) {
        LogWarning(LOG_P25, "VTUN, data truncated!");
        pktLen = pduLength; // don't overflow the buffer
    }

    DECLARE_UINT8_ARRAY(pduUserData, pduLength);
    ::memcpy(pduUserData, data, pktLen);
//#if DEBUG_P25_PDU_DATA
    Utils::dump(1U, "P25, P25PacketData::processPacketFrame(), pduUserData", pduUserData, pduLength);
//#endif

    // queue frame for dispatch
    QueuedDataFrame* qf = new QueuedDataFrame();
    qf->retryCnt = 0U;
    qf->extendRetry = false;
    qf->timestamp = now + INTERPACKET_DELAY;

    qf->header = pktHeader;
    qf->llId = llId;
    qf->tgtProtoAddr = tgtProtoAddr;

    qf->userData = new uint8_t[pduLength];
    ::memcpy(qf->userData, pduUserData, pduLength);
    qf->userDataLen = pduLength;

    m_queuedFrames.push_back(qf);
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

void P25PacketData::write_PDU_KMM(const uint8_t* data, uint32_t len, uint32_t llId, bool encrypted)
{
    // assemble a P25 PDU frame header for transport...
    data::DataHeader dataHeader = data::DataHeader();
    dataHeader.setFormat(PDUFormatType::CONFIRMED);
    dataHeader.setMFId(MFG_STANDARD);
    dataHeader.setAckNeeded(true);
    dataHeader.setOutbound(true);
    dataHeader.setSAP((encrypted) ? PDUSAP::ENC_KMM : PDUSAP::UNENC_KMM);
    dataHeader.setLLId(llId);
    dataHeader.setBlocksToFollow(1U);

    dataHeader.calculateLength(len);
    uint32_t pduLength = dataHeader.getPDULength();

    DECLARE_UINT8_ARRAY(pduUserData, pduLength);
    ::memcpy(pduUserData, data, len);

    dispatchUserFrameToFNE(dataHeader, false, false, pduUserData);
}

/* Updates the timer by the passed number of milliseconds. */

void P25PacketData::clock(uint32_t ms)
{
#if !defined(_WIN32)
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    if (m_queuedFrames.size() == 0U) {
        return;
    }

    // transmit queued data frames
    bool processed = false;

    auto& frame = m_queuedFrames[0];
    if (frame != nullptr) {
        if (now > frame->timestamp) {
            processed = true;

            if (frame->retryCnt >= MAX_PKT_RETRY_CNT && !frame->extendRetry) {
                LogWarning(LOG_P25, P25_PDU_STR ", max packet retry count exceeded, dropping packet, dstIp = %s", __IP_FROM_UINT(frame->tgtProtoAddr).c_str());
                goto pkt_clock_abort;
            }

            if (frame->retryCnt >= (MAX_PKT_RETRY_CNT * 2U) && frame->extendRetry) {
                LogWarning(LOG_P25, P25_PDU_STR ", max packet retry count exceeded, dropping packet, dstIp = %s", __IP_FROM_UINT(frame->tgtProtoAddr).c_str());
                m_readyForNextPkt[frame->llId] = true; // force ready for next packet
                goto pkt_clock_abort;
            }

            std::string tgtIpStr = __IP_FROM_UINT(frame->tgtProtoAddr);

            // extract protocol for logging
            uint8_t proto = 0x00;
            if (frame->userDataLen >= 20) {
                struct ip* ipHeader = (struct ip*)(frame->userData);
                proto = ipHeader->ip_p;
            }

            LogInfoEx(LOG_P25, "VTUN -> PDU IP Data (queued), dstIp = %s (%u), userDataLen = %u, proto = %02X%s, retries = %u", 
                tgtIpStr.c_str(), frame->llId, frame->userDataLen, proto, (proto == 0x01) ? " (ICMP)" : "", frame->retryCnt);

            // do we have a valid target address?
            if (frame->llId == 0U) {
                frame->llId = getLLIdAddress(frame->tgtProtoAddr);
                if (frame->llId == 0U) {
                    LogWarning(LOG_P25, P25_PDU_STR ", no ARP entry for, dstIp = %s", tgtIpStr.c_str());
                    write_PDU_ARP(frame->tgtProtoAddr);

                    processed = false;
                    frame->timestamp = now + ARP_RETRY_MS;
                    frame->retryCnt++;
                    goto pkt_clock_abort;
                }
                else {
                    frame->header->setLLId(frame->llId);
                }
            }

            // is the SU ready for the next packet?
            auto ready = std::find_if(m_readyForNextPkt.begin(), m_readyForNextPkt.end(), [=](ReadyForNextPktPair x) { return x.first == frame->llId; });
            if (ready != m_readyForNextPkt.end()) {
                if (!ready->second) {
                    LogWarning(LOG_P25, P25_PDU_STR ", subscriber not ready, dstIp = %s (%u), proto = %02X%s, will retry in %ums", 
                        tgtIpStr.c_str(), frame->llId, proto, (proto == 0x01) ? " (ICMP)" : "", SUBSCRIBER_READY_RETRY_MS);
                    processed = false;
                    frame->timestamp = now + SUBSCRIBER_READY_RETRY_MS;
                    frame->extendRetry = true;
                    frame->retryCnt++;
                    goto pkt_clock_abort;
                }
            }

            m_readyForNextPkt[frame->llId] = false;
            //LogDebugEx(LOG_P25, "P25PacketData::clock()", "dispatching queued PDU to llId %u (proto = %02X)", frame->llId, proto);
            dispatchUserFrameToFNE(*frame->header, false, false, frame->userData);
        }
    }

pkt_clock_abort:
    m_queuedFrames.pop_front();
    if (processed) {
        if (frame->userData != nullptr)
            delete[] frame->userData;
        if (frame->header != nullptr)
            delete frame->header;
        delete frame;
    } else {
        // requeue packet
        m_queuedFrames.push_back(frame);
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

        // bryanb: this is naive and possibly error prone
        m_readyForNextPkt[status->assembler.dataHeader.getSrcLLId()] = true;

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

    if (status->assembler.dataHeader.getFormat() == PDUFormatType::UNCONFIRMED) {
        m_readyForNextPkt[status->assembler.dataHeader.getSrcLLId()] = true;
    }

    uint8_t sap = (status->assembler.getExtendedAddress()) ? status->assembler.dataHeader.getEXSAP() : status->assembler.dataHeader.getSAP();
    if (status->assembler.getAuxiliaryES())
        sap = status->assembler.dataHeader.getEXSAP();

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
                m_arpTable[srcHWAddr] = srcProtoAddr;

                // is the SU ready for the next packet?
                auto ready = std::find_if(m_readyForNextPkt.begin(), m_readyForNextPkt.end(), [=](ReadyForNextPktPair x) { return x.first == srcHWAddr; });
                if (ready != m_readyForNextPkt.end()) {
                    if (!ready->second) {
                        m_readyForNextPkt[srcHWAddr] = true;
                    }
                } else {
                    m_readyForNextPkt[srcHWAddr] = true;
                }
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

        // validate minimum IP header size
        if (status->pduUserDataLength < sizeof(struct ip)) {
            LogError(LOG_P25, P25_PDU_STR ", PACKET_DATA too small, %u bytes", 
                status->pduUserDataLength);
            break;
        }

        struct ip* ipHeader = (struct ip*)(status->pduUserData);

        // verify IPv4 version
        if ((status->pduUserData[0] & 0xF0) != 0x40) {
            LogWarning(LOG_P25, P25_PDU_STR ", PACKET_DATA non-IPv4 packet");
            break;
        }

        // validate IP header length
        uint8_t ihl = (status->pduUserData[0] & 0x0F) * 4;
        if (ihl < sizeof(struct ip) || ihl > status->pduUserDataLength) {
            LogError(LOG_P25, P25_PDU_STR ", PACKET_DATA invalid IHL, ihl = %u", ihl);
            break;
        }

        char srcIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(ipHeader->ip_src), srcIp, INET_ADDRSTRLEN);

        char dstIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(ipHeader->ip_dst), dstIp, INET_ADDRSTRLEN);

        uint8_t proto = ipHeader->ip_p;
        uint16_t pktLen = Utils::reverseEndian(ipHeader->ip_len); // bryanb: this could be problematic on different endianness

        // reflect broadcast messages back to the CAI network
        bool handled = false;
        if (status->assembler.dataHeader.getLLId() == WUID_ALL) {
            LogInfoEx(LOG_P25, "PDU -> VTUN, IP Data, repeated to CAI, broadcast packet, dstIp = %s (%u)", 
                dstIp, status->assembler.dataHeader.getLLId());

            dispatchUserFrameToFNE(status->assembler.dataHeader, status->assembler.getExtendedAddress(), 
                status->assembler.getAuxiliaryES(), status->pduUserData);
            handled = true;

            // is the source SU one we have proper ARP entries for?
            auto arpEntry = std::find_if(m_arpTable.begin(), m_arpTable.end(), [=](ArpTablePair x) { return x.first == status->assembler.dataHeader.getSrcLLId(); });
            if (arpEntry == m_arpTable.end()) {
                uint32_t srcProtoAddr = Utils::reverseEndian(ipHeader->ip_src.s_addr);
                LogInfoEx(LOG_P25, P25_PDU_STR ", adding ARP entry, %s is at %u", __IP_FROM_UINT(srcProtoAddr).c_str(), status->assembler.dataHeader.getSrcLLId());
                m_arpTable[status->assembler.dataHeader.getSrcLLId()] = Utils::reverseEndian(ipHeader->ip_src.s_addr);
            }
        }

        // is the target SU one we have proper ARP entries for?
        auto arpEntry = std::find_if(m_arpTable.begin(), m_arpTable.end(), [=](ArpTablePair x) { return x.first == status->assembler.dataHeader.getLLId(); });
        if (arpEntry != m_arpTable.end()) {
            LogInfoEx(LOG_P25, "PDU -> VTUN, IP Data, repeated to CAI, destination IP has a CAI ARP table entry, dstIp = %s (%u)", 
                dstIp, status->assembler.dataHeader.getLLId());

            dispatchUserFrameToFNE(status->assembler.dataHeader, status->assembler.getExtendedAddress(), status->assembler.getAuxiliaryES(),
                status->pduUserData);
            handled = true;

            // is the source SU one we have proper ARP entries for?
            auto arpEntry = std::find_if(m_arpTable.begin(), m_arpTable.end(), [=](ArpTablePair x) { return x.first == status->assembler.dataHeader.getSrcLLId(); });
            if (arpEntry == m_arpTable.end()) {
                uint32_t srcProtoAddr = Utils::reverseEndian(ipHeader->ip_src.s_addr);
                LogInfoEx(LOG_P25, P25_PDU_STR ", adding ARP entry, %s is at %u", __IP_FROM_UINT(srcProtoAddr).c_str(), status->assembler.dataHeader.getSrcLLId());
                m_arpTable[status->assembler.dataHeader.getSrcLLId()] = Utils::reverseEndian(ipHeader->ip_src.s_addr);
            }
        }

        // sequence validation - check N(S) against V(R)
        uint8_t receivedNs = status->assembler.dataHeader.getNs();
        bool synchronize = status->assembler.dataHeader.getSynchronize();

        // Initialize V(R) if first packet from this LLId
        auto recvSeqIt = m_suRecvSeq.find(srcLlId);
        if (recvSeqIt == m_suRecvSeq.end()) {
            m_suRecvSeq[srcLlId] = 0U;
        }

        uint8_t expectedNs = m_suRecvSeq[srcLlId];
        bool sequenceValid = false;

        // handle synchronize flag - resets receive window per TIA-102
        if (synchronize) {
            //LogDebugEx(LOG_P25, "P25PacketData::dispatch()", "synchronize flag set by llId %u, resetting V(R) from %u to %u", 
            //    srcLlId, expectedNs, (receivedNs + 1) % 8);
            m_suRecvSeq[srcLlId] = (receivedNs + 1) % 8;
            sequenceValid = true;
        }
        else if (receivedNs == expectedNs || receivedNs == (expectedNs + 1) % 8) {
            // accept if N(S) == V(R) or V(R)+1 (allows one-ahead windowing)
            m_suRecvSeq[srcLlId] = (receivedNs + 1) % 8;
            sequenceValid = true;
            //LogDebugEx(LOG_P25, "P25PacketData::dispatch()", "sequence valid, llId %u, N(S) = %u, V(R) now = %u", 
            //    srcLlId, receivedNs, m_suRecvSeq[srcLlId]);
        }
        else {
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

        if (!sequenceValid) {
            break; // should never reach here due to logic above, but safety check
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
                m_readyForNextPkt[srcLlId] = true;
                write_PDU_Ack_Response(PDUAckClass::ACK, PDUAckType::ACK, receivedNs, srcLlId, true, dstLlId);
            } else {
                m_readyForNextPkt[srcLlId] = true;
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

        bool encrypted = (sap == PDUSAP::ENC_KMM);
        m_network->m_p25OTARService->processDLD(status->pduUserData, status->pduUserDataLength, status->llId, 
            status->assembler.dataHeader.getNs(), encrypted);
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

    /*
    ** MASTER TRAFFIC
    */

    // repeat traffic to the connected peers
    if (m_network->m_peers.size() > 0U) {
        for (auto peer : m_network->m_peers) {
            if (peerId != peer.first) {
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
            if (dstPeerId != peerId) {
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

void P25PacketData::dispatchUserFrameToFNE(p25::data::DataHeader& dataHeader, bool extendedAddress, bool auxiliaryES, uint8_t* pduUserData)
{
    uint32_t srcId = (extendedAddress) ? dataHeader.getSrcLLId() : dataHeader.getLLId();
    uint32_t dstId = dataHeader.getLLId();

    // update the sequence number
    m_suSendSeq[srcId]++;
    if (m_suSendSeq[srcId] >= 8U)
    {
        m_suSendSeq[srcId] = 0U;
        dataHeader.setSynchronize(true);
    }

    dataHeader.setNs(m_suSendSeq[srcId]);

    /*
    ** MASTER TRAFFIC
    */

    // repeat traffic to the connected peers
    if (m_network->m_peers.size() > 0U) {
        for (auto peer : m_network->m_peers) {
            write_PDU_User(peer.first, m_network->m_peerId, nullptr, dataHeader, extendedAddress, auxiliaryES, pduUserData);
            if (m_network->m_debug) {
                LogDebug(LOG_P25, "dstPeer = %u, duid = $%02X, srcId = %u, dstId = %u", 
                    peer.first, DUID::PDU, srcId, dstId);
            }
        }
    }
}

/* Helper used to process conventional data registration from PDU data. */

bool P25PacketData::processConvDataReg(RxStatus* status)
{
    uint8_t regType = (status->pduUserData[0U] >> 4) & 0x0F;
    switch (regType) {
    case PDURegType::CONNECT:
    {
        uint32_t llId = (status->pduUserData[1U] << 16) + (status->pduUserData[2U] << 8) + status->pduUserData[3U];
        uint32_t ipAddr = (status->pduUserData[8U] << 24) + (status->pduUserData[9U] << 16) + (status->pduUserData[10U] << 8) + status->pduUserData[11U];

        if (ipAddr == 0U) {
            LogWarning(LOG_P25, P25_PDU_STR ", CONNECT (Registration Request Connect) with zero IP address, llId = %u", llId);
            ipAddr = getIPAddress(llId);
        }

        LogInfoEx(LOG_P25, P25_PDU_STR ", CONNECT (Registration Request Connect), llId = %u, ipAddr = %s", llId, __IP_FROM_UINT(ipAddr).c_str());
        m_arpTable[llId] = ipAddr; // update ARP table
    }
    break;
    case PDURegType::DISCONNECT:
    {
        uint32_t llId = (status->pduUserData[1U] << 16) + (status->pduUserData[2U] << 8) + status->pduUserData[3U];

        LogInfoEx(LOG_P25, P25_PDU_STR ", DISCONNECT (Registration Request Disconnect), llId = %u", llId);

        m_arpTable.erase(llId);
    }
    break;
    default:
        LogError(LOG_P25, "P25 unhandled PDU registration type, regType = $%02X", regType);
        break;
    }

    return true;
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

                    m_arpTable[llId] = staticIP;
                    m_readyForNextPkt[llId] = true;

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

                    m_arpTable[llId] = dynamicIP;
                    m_readyForNextPkt[llId] = true;

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

            m_arpTable.erase(llId);
            m_readyForNextPkt.erase(llId);

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

void P25PacketData::write_PDU_User(uint32_t peerId, uint32_t srcPeerId, network::PeerNetwork* peerNet, data::DataHeader& dataHeader,
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

    m_assembler->assemble(dataHeader, extendedAddress, auxiliaryES, pduUserData, nullptr, context);
    delete context;
}

/* Write data processed to the network. */

bool P25PacketData::writeNetwork(uint32_t peerId, uint32_t srcPeerId, network::PeerNetwork* peerNet, const p25::data::DataHeader& dataHeader, const uint8_t currentBlock, 
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

    // lookup ARP table entry
    try {
        uint32_t addr = m_arpTable.at(llId);
        if (addr != 0U) {
            return true;
        }
        else {
            return false;
        }
    } catch (...) {
        return false;
    }
}

/* Helper to get the IP address for the given logical link ID. */

uint32_t P25PacketData::getIPAddress(uint32_t llId)
{
    if (llId == 0U) {
        return 0U;
    }

    if (hasARPEntry(llId)) {
        return m_arpTable[llId];
    } else {
        // do we have a static entry for this LLID?
        lookups::RadioId rid = m_network->m_ridLookup->find(llId);
        if (!rid.radioDefault()) {
            if (rid.radioEnabled()) {
                std::string addr = rid.radioIPAddress();
                uint32_t ipAddr = __IP_FROM_STR(addr);
                return ipAddr;
            }
        }
    }

    return 0U;
}

/* Helper to get the logical link ID for the given IP address. */

uint32_t P25PacketData::getLLIdAddress(uint32_t addr)
{
    if (addr == 0U) {
        return 0U;
    }

    // lookup ARP table entry
    for (auto entry : m_arpTable) {
        if (entry.second == addr) {
            return entry.first;
        }
    }

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

    // build set of already-allocated IPs to ensure uniqueness
    std::unordered_set<uint32_t> allocatedIPs;
    for (const auto& entry : m_arpTable) {
        allocatedIPs.insert(entry.second);
    }

    // find next available IP not already in use
    uint32_t candidateIP = nextIP;
    const uint32_t poolSize = m_network->m_sndcpEndAddr - m_network->m_sndcpStartAddr + 1U;
    uint32_t attempts = 0U;

    while (allocatedIPs.find(candidateIP) != allocatedIPs.end() && attempts < poolSize) {
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

    m_arpTable[llId] = allocatedIP;
    LogInfoEx(LOG_P25, P25_PDU_STR ", SNDCP allocated dynamic IP %s to llId = %u (pool: %s - %s)", 
        __IP_FROM_UINT(allocatedIP).c_str(), llId, __IP_FROM_UINT(m_network->m_sndcpStartAddr).c_str(), __IP_FROM_UINT(m_network->m_sndcpEndAddr).c_str());

    return allocatedIP;
}
