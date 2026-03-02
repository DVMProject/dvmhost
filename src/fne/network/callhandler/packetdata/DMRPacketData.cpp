// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/Defines.h"
#include "common/dmr/data/DataBlock.h"
#include "common/dmr/data/Assembler.h"
#include "common/dmr/SlotType.h"
#include "common/dmr/Sync.h"
#include "common/edac/CRC.h"
#include "common/Clock.h"
#include "common/Log.h"
#include "common/Thread.h"
#include "common/Utils.h"
#include "network/TrafficNetwork.h"
#include "network/callhandler/packetdata/DMRPacketData.h"
#include "HostFNE.h"

using namespace system_clock;
using namespace network;
using namespace network::callhandler;
using namespace network::callhandler::packetdata;
using namespace dmr;
using namespace dmr::defines;

#include <cassert>
#include <chrono>

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

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the DMRPacketData class. */

DMRPacketData::DMRPacketData(TrafficNetwork* network, TagDMRData* tag, bool debug) :
    m_network(network),
    m_tag(tag),
    m_queuedFrames(),
    m_status(),
    m_arpTable(),
    m_readyForNextPkt(),
    m_suSendSeq(),
    m_debug(debug)
{
    assert(network != nullptr);
    assert(tag != nullptr);
}

/* Finalizes a instance of the DMRPacketData class. */

DMRPacketData::~DMRPacketData() = default;

/* Process a data frame from the network. */

bool DMRPacketData::processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint16_t pktSeq, uint32_t streamId, bool fromUpstream)
{
    hrc::hrc_t pktTime = hrc::now();

    uint8_t seqNo = data[4U];

    uint32_t srcId = GET_UINT24(data, 5U);
    uint32_t dstId = GET_UINT24(data, 8U);

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

    if (dataSync) {
        dmrData.setData(data + 20U);
        dmrData.setDataType(dataType);
        dmrData.setN(0U);
    }

    uint8_t frame[DMR_FRAME_LENGTH_BYTES];
    dmrData.getData(frame);

    if (dataSync) {
        auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) { return x.second->peerId == peerId; });
        if (it == m_status.end()) {
            // this is a new call stream
            m_status.lock();
            RxStatus* status = new RxStatus();
            status->callStartTime = pktTime;
            status->srcId = srcId;
            status->dstId = dstId;
            status->slotNo = slotNo;
            status->streamId = streamId;
            status->peerId = peerId;
            m_status.unlock();

            m_status.insert(peerId, status);
        }

        RxStatus* status = m_status[peerId];
        if ((status->streamId != 0U && streamId != status->streamId) || status->callBusy) {
            if (m_network->m_callCollisionTimeout > 0U) {
                uint64_t lastPktDuration = hrc::diff(hrc::now(), status->lastPacket);
                if ((lastPktDuration / 1000) > m_network->m_callCollisionTimeout) {
                    LogWarning((fromUpstream) ? LOG_PEER : LOG_MASTER, "DMR, Data Call Collision, lasted more then %us with no further updates, resetting call source", m_network->m_callCollisionTimeout);

                    m_status.lock(false);
                    status->streamId = streamId;
                    status->callBusy = false;
                    m_status.unlock();
                }
                else {
                    LogWarning((fromUpstream) ? LOG_PEER : LOG_MASTER, "DMR, Data Call Collision, peer = %u, slot = %u, streamId = %u, rxPeer = %u, rxStreamId = %u, fromUpstream = %u",
                        peerId, slotNo, streamId, status->peerId, status->streamId, fromUpstream);
                    return false;
                }
            } else {
                m_status.lock(false);
                status->streamId = streamId;
                m_status.unlock();
            }
        }

        if (status->callBusy) {
            LogWarning((fromUpstream) ? LOG_PEER : LOG_MASTER, "DMR, Data Call Lockout, cannot process data packets while data call in progress, peer = %u, slot = %u, streamId = %u, fromUpstream = %u",
                peerId, slotNo, streamId, fromUpstream);
            return false;
        }

        m_status.lock(false);
        status->lastPacket = hrc::now();
        m_status.unlock();

        if (dataType == DataType::DATA_HEADER) {
            bool ret = status->header.decode(frame);
            if (!ret) {
                LogError(LOG_DMR, "DMR Slot %u, DataType::DATA_HEADER, unable to decode the network data header", status->slotNo);
                Utils::dump(1U, "DMR, Unfixable PDU Data", frame, DMR_FRAME_LENGTH_BYTES);

                status->streamId = 0U;
                return false;
            }

            status->frames = status->header.getBlocksToFollow();
            status->dataBlockCnt = 0U;
            status->hasRxHeader = true;

            bool gi = status->header.getGI();
            uint32_t srcId = status->header.getSrcId();
            uint32_t dstId = status->header.getDstId();

            LogInfoEx(LOG_DMR, DMR_DT_DATA_HEADER ", peerId = %u, slot = %u, dpf = $%02X, ack = %u, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, packetLength = %u, seqNo = %u, dstId = %u, srcId = %u, group = %u",
                peerId, status->slotNo, status->header.getDPF(), status->header.getA(), status->header.getSAP(), status->header.getFullMesage(), status->header.getBlocksToFollow(), status->header.getPadLength(), status->header.getPacketLength(dataType),
                status->header.getFSN(), dstId, srcId, gi);

            // make sure we don't get a PDU with more blocks then we support
            if (status->header.getBlocksToFollow() >= MAX_PDU_COUNT) {
                LogError(LOG_DMR, DMR_DT_DATA_HEADER ", too many PDU blocks to process, %u > %u", status->header.getBlocksToFollow(), MAX_PDU_COUNT);
                status->streamId = 0U;
                return false;
            }

            m_status[peerId] = status;

            dispatchToFNE(peerId, dmrData, data, len, seqNo, pktSeq, streamId);

            // a PDU header only with no blocks to follow is usually a response header
            if (status->header.getBlocksToFollow() == 0U) {
                status->streamId = 0U;
                return true;
            }

            LogInfoEx((fromUpstream) ? LOG_PEER : LOG_MASTER, "DMR, Data Call Start, peer = %u, slot = %u, srcId = %u, dstId = %u, group = %u, streamId = %u, fromUpstream = %u", peerId, status->slotNo, status->srcId, status->dstId, gi, streamId, fromUpstream);
            return true;
        }

        if ((dataType == DataType::RATE_34_DATA) ||
            (dataType == DataType::RATE_12_DATA) ||
            (dataType == DataType::RATE_1_DATA)) {
            dispatchToFNE(peerId, dmrData, data, len, seqNo, pktSeq, streamId);

            data::DataBlock dataBlock;
            dataBlock.setDataType(dataType);

            bool ret = dataBlock.decode(frame, status->header);
            if (ret) {
                uint32_t blockLen = dataBlock.getData(status->pduUserData + status->pduDataOffset);
                status->pduDataOffset += blockLen;

                status->frames--;
                if (status->frames == 0U)
                    dataBlock.setLastBlock(true);
                status->dataBlockCnt++;
            }
        }

        // dispatch the PDU data
        if (status->hasRxHeader && status->dataBlockCnt > 0U && status->frames == 0U) {
            // is the source ID a blacklisted ID?
            lookups::RadioId rid = m_network->m_ridLookup->find(status->header.getSrcId());
            if (!rid.radioDefault()) {
                if (!rid.radioEnabled()) {
                    // report error event to InfluxDB
                    if (m_network->m_enableInfluxDB) {
                        influxdb::QueryBuilder()
                            .meas("call_error_event")
                                .tag("peerId", std::to_string(peerId))
                                .tag("streamId", std::to_string(streamId))
                                .tag("srcId", std::to_string(status->header.getSrcId()))
                                .tag("dstId", std::to_string(status->header.getDstId()))
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

            dispatch(peerId, dmrData, data, len);

            uint64_t duration = hrc::diff(pktTime, status->callStartTime);
            bool gi = status->header.getGI();
            uint32_t srcId = status->header.getSrcId();
            uint32_t dstId = status->header.getDstId();
            LogInfoEx((fromUpstream) ? LOG_PEER : LOG_MASTER, "DMR, Data Call End, peer = %u, slot = %u, srcId = %u, dstId = %u, group = %u, blocks = %u, duration = %u, streamId = %u, fromUpstream = %u",
                peerId, srcId, dstId, gi, status->header.getBlocksToFollow(), duration / 1000, streamId, fromUpstream);

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
                    .requestAsync(m_network->m_influxServer);
            }

            m_status.erase(peerId);
            delete status;
            status = nullptr;
        } else {
            status->callBusy = false;
        }
    }

    return true;
}

/* Process a data frame from the virtual IP network. */

void DMRPacketData::processPacketFrame(const uint8_t* data, uint32_t len, bool alreadyQueued)
{
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

#if !defined(_WIN32)
    // validate minimum IPv4 header size
    if (len < sizeof(struct ip)) {
        LogError(LOG_DMR, "DMR, packet too short for IP header");
        return;
    }

    // check IP version (must be IPv4)
    if ((data[0] & 0xF0) != 0x40) {
        LogError(LOG_DMR, "DMR, non-IPv4 packet received");
        return;
    }

    // validate Internet Header Length
    uint8_t ihl = (data[0] & 0x0F) * 4;  // IHL in 32-bit words, convert to bytes
    if (len < ihl || ihl < 20U) {
        LogError(LOG_DMR, "DMR, invalid IP header length");
        return;
    }

    struct ip* ipHeader = (struct ip*)data;

    char srcIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ipHeader->ip_src), srcIp, INET_ADDRSTRLEN);

    char dstIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ipHeader->ip_dst), dstIp, INET_ADDRSTRLEN);

    uint8_t proto = ipHeader->ip_p;
    uint16_t pktLen = Utils::reverseEndian(ipHeader->ip_len);

    // validate IP total length field against actual received length
    if (pktLen > len) {
        LogError(LOG_DMR, "DMR, IP packet length mismatch");
        return;
    }

    if (pktLen < ihl) {
        LogError(LOG_DMR, "DMR, IP packet length less than header length");
        return;
    }

    uint32_t dstId = getRadioIdAddress(Utils::reverseEndian(ipHeader->ip_dst.s_addr));
    uint32_t srcProtoAddr = Utils::reverseEndian(ipHeader->ip_src.s_addr);
    uint32_t tgtProtoAddr = Utils::reverseEndian(ipHeader->ip_dst.s_addr);

    std::string srcIpStr = __IP_FROM_UINT(srcProtoAddr);
    std::string tgtIpStr = __IP_FROM_UINT(tgtProtoAddr);

    LogInfoEx(LOG_DMR, "VTUN -> PDU IP Data, srcIp = %s (%u), dstIp = %s (%u), pktLen = %u, proto = %02X",
        srcIpStr.c_str(), DMRDEF::WUID_IPI, tgtIpStr.c_str(), dstId, pktLen, proto);

    // assemble a DMR PDU frame header for transport...
    dmr::data::DataHeader* pktHeader = new dmr::data::DataHeader();
    pktHeader->setDPF(DPF::CONFIRMED_DATA);
    pktHeader->setA(true);
    pktHeader->setSAP(PDUSAP::PACKET_DATA);
    pktHeader->setSrcId(DMRDEF::WUID_IPI);
    pktHeader->setDstId(dstId);
    pktHeader->setGI(false);
    pktHeader->setFullMesage(true);

    // bryanb: we are always sending data as 3/4 rate?
    pktHeader->calculateLength(DataType::RATE_34_DATA, pktLen);

    uint32_t pduLength = pktLen;

    DECLARE_UINT8_ARRAY(pduUserData, pduLength);
    ::memcpy(pduUserData, data, pktLen);

    // queue frame for dispatch
    QueuedDataFrame* qf = new QueuedDataFrame();
    qf->retryCnt = 0U;
    qf->extendRetry = false;
    qf->timestamp = now + INTERPACKET_DELAY;

    qf->header = pktHeader;
    qf->dstId = dstId;
    qf->tgtProtoAddr = tgtProtoAddr;

    qf->userData = new uint8_t[pduLength];
    ::memcpy(qf->userData, pduUserData, pduLength);
    qf->userDataLen = pduLength;

    m_queuedFrames.push_back(qf);
#endif // !defined(_WIN32)
}

/* Helper to write a PDU acknowledge response. */

void DMRPacketData::write_PDU_Ack_Response(uint8_t ackClass, uint8_t ackType, uint8_t ackStatus, uint32_t srcId, uint32_t dstId)
{
    if (ackClass == PDUResponseClass::ACK && ackType != PDUResponseType::ACK)
        ackType = PDUResponseType::ACK;

    dmr::data::DataHeader rspHeader = dmr::data::DataHeader();
    rspHeader.setDPF(DPF::RESPONSE);
    rspHeader.setSAP(PDUSAP::PACKET_DATA);
    rspHeader.setResponseClass(ackClass);
    rspHeader.setResponseType(ackType);
    rspHeader.setResponseStatus(ackStatus);
    rspHeader.setSrcId(srcId);
    rspHeader.setDstId(dstId);
    rspHeader.setGI(false);
    rspHeader.setBlocksToFollow(0U);

    dispatchUserFrameToFNE(rspHeader, nullptr);
}

/* Updates the timer by the passed number of milliseconds. */

void DMRPacketData::clock(uint32_t ms)
{
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    if (m_queuedFrames.size() == 0U) {
        return;
    }

    // transmit queued data frames
    auto& frame = m_queuedFrames[0];
    if (frame != nullptr) {
        if (now >= frame->timestamp) {
            // check if we have an ARP entry for the destination
            if (!hasARPEntry(frame->dstId)) {
                if (frame->retryCnt < MAX_PKT_RETRY_CNT || frame->extendRetry) {
                    // send ARP request
                    write_PDU_ARP(frame->tgtProtoAddr);
                    frame->timestamp = now + ARP_RETRY_MS;
                    frame->retryCnt++;
                } else {
                    LogWarning(LOG_DMR, "DMR, failed to resolve ARP for dstId %u", frame->dstId);
                    delete frame->header;
                    delete[] frame->userData;
                    delete frame;
                    m_queuedFrames.pop_front();
                }
            } else {
                // transmit the PDU frame
                dispatchUserFrameToFNE(*frame->header, frame->userData);

                delete frame->header;
                delete[] frame->userData;
                delete frame;
                m_queuedFrames.pop_front();
            }
        }
    }
}

/* Helper to cleanup any call's left in a dangling state without any further updates. */

void DMRPacketData::cleanupStale()
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
                LogWarning(LOG_DMR, "DMR, Data Call Timeout, lasted more then %us with no further updates", 10U);
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

void DMRPacketData::dispatch(uint32_t peerId, dmr::data::NetData& dmrData, const uint8_t* data, uint32_t len)
{
    RxStatus *status = m_status[peerId];

    if (status->header.getBlocksToFollow() > 0U && status->frames == 0U) {
        // ooookay -- lets do the insane, and ridiculously stupid, ETSI Big-Endian reversed byte ordering bullshit for the CRC-32
        uint8_t crcBytes[MAX_PDU_COUNT * DMR_PDU_UNCODED_LENGTH_BYTES + 2U];
        ::memset(crcBytes, 0x00U, MAX_PDU_COUNT * DMR_PDU_UNCODED_LENGTH_BYTES + 2U);
        for (uint8_t i = 0U; i < status->pduDataOffset - 4U; i += 2U) {
            crcBytes[i + 1U] = status->pduUserData[i];
            crcBytes[i] = status->pduUserData[i + 1U];
        }

        crcBytes[status->pduDataOffset - 1U] = status->pduUserData[status->pduDataOffset - 4U];
        crcBytes[status->pduDataOffset - 2U] = status->pduUserData[status->pduDataOffset - 3U];
        crcBytes[status->pduDataOffset - 3U] = status->pduUserData[status->pduDataOffset - 2U];
        crcBytes[status->pduDataOffset - 4U] = status->pduUserData[status->pduDataOffset - 1U];

        bool crcRet = edac::CRC::checkInvertedCRC32(crcBytes, status->pduDataOffset);
        if (!crcRet) {
            LogWarning(LOG_DMR, "DMR Data, failed CRC-32 check, blocks %u, len %u", status->header.getBlocksToFollow(), status->pduDataOffset);
        }

        if (m_network->m_dumpPacketData) {
            Utils::dump(1U, "DMR, ISP PDU Packet", status->pduUserData, status->pduDataOffset);
        }

        // handle service access points
        uint8_t sap = status->header.getSAP();
        switch (sap) {
        case PDUSAP::ARP:
        {
#if !defined(_WIN32)
            // is the host virtual tunneling enabled?
            if (!m_network->m_host->m_vtunEnabled)
                return;

            uint32_t fneIPv4 = __IP_FROM_STR(m_network->m_host->m_tun->getIPv4());

            if (status->pduDataOffset < DMR_PDU_ARP_PCKT_LENGTH) {
                LogError(LOG_DMR, "DMR, ARP packet too short, %u < %u", status->pduDataOffset, DMR_PDU_ARP_PCKT_LENGTH);
                return;
            }

            uint8_t arpPacket[DMR_PDU_ARP_PCKT_LENGTH];
            ::memset(arpPacket, 0x00U, DMR_PDU_ARP_PCKT_LENGTH);
            ::memcpy(arpPacket, status->pduUserData, DMR_PDU_ARP_PCKT_LENGTH);

            uint16_t opcode = GET_UINT16(arpPacket, 6U);
            uint32_t srcHWAddr = GET_UINT24(arpPacket, 8U);
            uint32_t srcProtoAddr = GET_UINT32(arpPacket, 11U);
            uint32_t tgtProtoAddr = GET_UINT32(arpPacket, 18U);

            if (opcode == DMR_PDU_ARP_REQUEST) {
                LogInfoEx(LOG_DMR, "DMR, ARP request, who has %s? tell %s (%u)", __IP_FROM_UINT(tgtProtoAddr).c_str(), __IP_FROM_UINT(srcProtoAddr).c_str(), srcHWAddr);
                m_arpTable[srcHWAddr] = srcProtoAddr; // update ARP table
                if (tgtProtoAddr == fneIPv4) {
                    write_PDU_ARP_Reply(fneIPv4, srcHWAddr, srcProtoAddr, DMRDEF::WUID_ALLL);
                }
            } else if (opcode == DMR_PDU_ARP_REPLY) {
                LogInfoEx(LOG_DMR, "DMR, ARP reply, %s is at %u", __IP_FROM_UINT(srcProtoAddr).c_str(), srcHWAddr);
                m_arpTable[srcHWAddr] = srcProtoAddr; // update ARP table
            }
#endif // !defined(_WIN32)
        }
        break;

        case PDUSAP::PACKET_DATA:
        {
#if !defined(_WIN32)
            // is the host virtual tunneling enabled?
            if (!m_network->m_host->m_vtunEnabled)
                return;

            uint32_t srcId = status->header.getSrcId();
            uint32_t dstId = status->header.getDstId();

            // validate minimum IP header size
            if (status->pduDataOffset < sizeof(struct ip)) {
                LogError(LOG_DMR, "DMR, IP packet too short for IP header, len %u", status->pduDataOffset);
                return;
            }

            struct ip* ipHeader = (struct ip*)(status->pduUserData);

            // verify IPv4 version
            if ((status->pduUserData[0] & 0xF0) != 0x40) {
                LogError(LOG_DMR, "DMR, non-IPv4 packet received, version %u", (status->pduUserData[0] & 0xF0) >> 4);
                return;
            }

            // validate IP header length
            uint8_t ihl = (status->pduUserData[0] & 0x0F) * 4;
            if (ihl < sizeof(struct ip) || ihl > status->pduDataOffset) {
                LogError(LOG_DMR, "DMR, invalid IP header length, ihl %u, pduLen %u", ihl, status->pduDataOffset);
                return;
            }

            char srcIp[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(ipHeader->ip_src), srcIp, INET_ADDRSTRLEN);

            char dstIp[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(ipHeader->ip_dst), dstIp, INET_ADDRSTRLEN);

            uint8_t proto = ipHeader->ip_p;
            uint16_t pktLen = Utils::reverseEndian(ipHeader->ip_len);

            // reflect broadcast messages back to the CAI network
            bool handled = false;
            if (status->header.getDstId() == DMRDEF::WUID_ALL) {
                uint32_t tgtProtoAddr = Utils::reverseEndian(ipHeader->ip_dst.s_addr);
                uint32_t targetId = getRadioIdAddress(tgtProtoAddr);
                if (targetId != 0U) {
                    LogInfoEx(LOG_DMR, "DMR, reflecting broadcast IP packet, srcIp = %s (%u), dstIp = %s (%u)",
                        srcIp, srcId, dstIp, targetId);
                    // TODO: reflect packet back to CAI
                    handled = true;
                }
            }

            // transmit packet to IP network
            LogInfoEx(LOG_DMR, "PDU -> VTUN, IP Data, srcIp = %s (%u), dstIp = %s (%u), pktLen = %u, proto = %02X",
                srcIp, srcId, dstIp, dstId, pktLen, proto);

            DECLARE_UINT8_ARRAY(ipFrame, pktLen);
            ::memcpy(ipFrame, status->pduUserData, pktLen);
            if (!m_network->m_host->m_tun->write(ipFrame, pktLen)) {
                LogError(LOG_DMR, "DMR, failed to write IP frame to virtual tunnel, len %u", pktLen);
            }

            // if the packet is unhandled and sent off to VTUN; ack the packet so the sender knows we received it
            if (!handled) {
                if (status->header.getA()) {
                    write_PDU_Ack_Response(PDUResponseClass::ACK, PDUResponseType::ACK,
                        status->header.getNs(), srcId, dstId);
                }
            }
#endif // !defined(_WIN32)
        }
        break;

        default:
            // unknown SAP, just log the packet
            LogWarning(LOG_DMR, "DMR, unhandled SAP $%02X, blocks %u, len %u", sap, status->header.getBlocksToFollow(), status->pduDataOffset);
            break;
        }
    }
}

/* Helper to dispatch PDU user data back to the FNE network. */

void DMRPacketData::dispatchToFNE(uint32_t peerId, dmr::data::NetData& dmrData, const uint8_t* data, uint32_t len, uint8_t seqNo, uint16_t pktSeq, uint32_t streamId)
{
    RxStatus* status = m_status[peerId];

    uint32_t srcId = status->header.getSrcId();
    uint32_t dstId = status->header.getDstId();

    /*
    ** MASTER TRAFFIC
    */

    // repeat traffic to the connected peers
    if (m_network->m_peers.size() > 0U) {
        for (auto peer : m_network->m_peers) {
            if (peerId != peer.first) {
                // is this peer ignored?
                if (!m_tag->isPeerPermitted(peer.first, dmrData, streamId)) {
                    continue;
                }

                m_network->writePeer(peer.first, peerId, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR }, data, len, pktSeq, streamId);
                if (m_network->m_debug) {
                    LogDebugEx(LOG_DMR, "DMRPacketData::dispatchToFNE()", "Master, srcPeer = %u, dstPeer = %u, seqNo = %u, srcId = %u, dstId = %u, slotNo = %u, len = %u, pktSeq = %u, stream = %u", 
                        peerId, peer.first, seqNo, srcId, dstId, status->slotNo, len, pktSeq, streamId);
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

                // is this peer ignored?
                if (!m_tag->isPeerPermitted(dstPeerId, dmrData, streamId, true)) {
                    continue;
                }

                peer.second->writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR }, data, len, pktSeq, streamId);
                if (m_network->m_debug) {
                    LogDebugEx(LOG_DMR, "DMRPacketData::dispatchToFNE()", "Peers, srcPeer = %u, dstPeer = %u, seqNo = %u, srcId = %u, dstId = %u, slotNo = %u, len = %u, pktSeq = %u, stream = %u", 
                        peerId, dstPeerId, seqNo, srcId, dstId, status->slotNo, len, pktSeq, streamId);
                }
            }
        }
    }
}

/* Helper to dispatch PDU user data back to the local FNE network. */

void DMRPacketData::dispatchUserFrameToFNE(dmr::data::DataHeader& dataHeader, uint8_t* pduUserData)
{
    uint32_t srcId = dataHeader.getSrcId();
    uint32_t dstId = dataHeader.getDstId();

    // update the sequence number
    m_suSendSeq[srcId]++;
    if (m_suSendSeq[srcId] >= 8U) {
        m_suSendSeq[srcId] = 0U;
    }

    dataHeader.setNs(m_suSendSeq[srcId]);

    // encode and transmit DMR PDU frames to all connected peers
    uint32_t streamId = m_network->createStreamId();
    uint16_t pktSeq = 0U;

    if (pduUserData == nullptr)
        pktSeq = RTP_END_OF_CALL_SEQ;

    // use lambda to capture context and write network frames
    auto blockWriter = [&](const void* context, const uint8_t currentBlock, const uint8_t* data, uint32_t len, bool lastBlock) -> void {
        if (data == nullptr)
            return;

        /*
        ** bryanb: dev notes for myself...
        **  - this implementation makes some dangerous naive assumptions about which slot to use
        **  - we always send data as PRIVATE; PDUs are essentially never group operations
        **  - we always start sequence numbers at 0 for each PDU -- this is possibly wrong, the host may misinterpret this
        */

        dmr::data::NetData dmrData;
        dmrData.setSlotNo(1U);
        dmrData.setSrcId(srcId);
        dmrData.setDstId(dstId);
        dmrData.setFLCO(FLCO::PRIVATE);
        dmrData.setN(0U);
        dmrData.setSeqNo(0U);
        dmrData.setDataType((currentBlock == 0U) ? DataType::DATA_HEADER : DataType::RATE_34_DATA);

        // create DMR network message
        uint32_t messageLength = 0U;
        UInt8Array message = m_network->createDMR_Message(messageLength, streamId, dmrData);
        if (message != nullptr) {
            // copy the DMR frame data
            ::memcpy(message.get() + 20U, data, len);

            // repeat traffic to the connected peers
            if (m_network->m_peers.size() > 0U) {
                for (auto peer : m_network->m_peers) {
                    m_network->writePeer(peer.first, m_network->m_peerId, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR }, 
                        message.get(), messageLength, pktSeq, streamId);
                }
            }
        }

        pktSeq++;
    };

    // determine data type based on header
    DataType::E dataType = DataType::RATE_34_DATA;

    // assemble and transmit the PDU
    dmr::data::Assembler assembler;
    assembler.setBlockWriter(blockWriter);
    assembler.assemble(dataHeader, dataType, pduUserData, nullptr, nullptr);
}

/* Helper write ARP request to the network. */

void DMRPacketData::write_PDU_ARP(uint32_t addr)
{
#if !defined(_WIN32)
    if (!m_network->m_host->m_vtunEnabled)
        return;

    uint8_t arpPacket[DMR_PDU_ARP_PCKT_LENGTH];
    ::memset(arpPacket, 0x00U, DMR_PDU_ARP_PCKT_LENGTH);

    SET_UINT16(DMR_PDU_ARP_CAI_TYPE, arpPacket, 0U);            // Hardware Address Type
    SET_UINT16(PDUSAP::PACKET_DATA, arpPacket, 2U);             // Protocol Address Type
    arpPacket[4U] = DMR_PDU_ARP_HW_ADDR_LENGTH;                 // Hardware Address Length
    arpPacket[5U] = DMR_PDU_ARP_PROTO_ADDR_LENGTH;              // Protocol Address Length
    SET_UINT16(DMR_PDU_ARP_REQUEST, arpPacket, 6U);             // Opcode

    SET_UINT24(DMRDEF::WUID_ALLL, arpPacket, 8U);              // Sender Hardware Address

    std::string fneIPv4 = m_network->m_host->m_tun->getIPv4();
    SET_UINT32(__IP_FROM_STR(fneIPv4), arpPacket, 11U);         // Sender Protocol Address

    SET_UINT32(addr, arpPacket, 18U);                           // Target Protocol Address

    LogInfoEx(LOG_DMR, "DMR, ARP request, who has %s? tell %s (%u)", __IP_FROM_UINT(addr).c_str(), fneIPv4.c_str(), DMRDEF::WUID_ALLL);

    // assemble a DMR PDU frame header for transport...
    dmr::data::DataHeader rspHeader = dmr::data::DataHeader();
    rspHeader.setDPF(DPF::UNCONFIRMED_DATA);
    rspHeader.setA(false);
    rspHeader.setSAP(PDUSAP::ARP);
    rspHeader.setSrcId(DMRDEF::WUID_ALLL);
    rspHeader.setDstId(DMRDEF::WUID_ALL);
    rspHeader.setGI(true);
    rspHeader.setBlocksToFollow(1U);

    dispatchUserFrameToFNE(rspHeader, arpPacket);
#endif // !defined(_WIN32)
}

/* Helper write ARP reply to the network. */

void DMRPacketData::write_PDU_ARP_Reply(uint32_t targetAddr, uint32_t requestorId, uint32_t requestorAddr, uint32_t targetId)
{
#if !defined(_WIN32)
    if (!m_network->m_host->m_vtunEnabled)
        return;

    uint8_t arpPacket[DMR_PDU_ARP_PCKT_LENGTH];
    ::memset(arpPacket, 0x00U, DMR_PDU_ARP_PCKT_LENGTH);

    SET_UINT16(DMR_PDU_ARP_CAI_TYPE, arpPacket, 0U);            // Hardware Address Type
    SET_UINT16(PDUSAP::PACKET_DATA, arpPacket, 2U);             // Protocol Address Type
    arpPacket[4U] = DMR_PDU_ARP_HW_ADDR_LENGTH;                 // Hardware Address Length
    arpPacket[5U] = DMR_PDU_ARP_PROTO_ADDR_LENGTH;              // Protocol Address Length
    SET_UINT16(DMR_PDU_ARP_REPLY, arpPacket, 6U);               // Opcode

    SET_UINT24(targetId, arpPacket, 8U);                        // Sender Hardware Address
    SET_UINT32(targetAddr, arpPacket, 11U);                     // Sender Protocol Address

    SET_UINT24(requestorId, arpPacket, 15U);                    // Target Hardware Address
    SET_UINT32(requestorAddr, arpPacket, 18U);                  // Target Protocol Address

    LogInfoEx(LOG_DMR, "DMR, ARP reply, %s is at %u", __IP_FROM_UINT(targetAddr).c_str(), targetId);

    // assemble a DMR PDU frame header for transport...
    dmr::data::DataHeader rspHeader = dmr::data::DataHeader();
    rspHeader.setDPF(DPF::UNCONFIRMED_DATA);
    rspHeader.setA(false);
    rspHeader.setSAP(PDUSAP::ARP);
    rspHeader.setSrcId(targetId);
    rspHeader.setDstId(requestorId);
    rspHeader.setGI(false);
    rspHeader.setBlocksToFollow(1U);

    dispatchUserFrameToFNE(rspHeader, arpPacket);
#endif // !defined(_WIN32)
}

/* Helper to determine if the radio ID has an ARP entry. */

bool DMRPacketData::hasARPEntry(uint32_t id) const
{
    if (id == 0U) {
        return false;
    }

    // lookup ARP table entry
    try {
        uint32_t addr = m_arpTable.at(id);
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

/* Helper to get the IP address for the given radio ID. */

uint32_t DMRPacketData::getIPAddress(uint32_t id)
{
    if (id == 0U) {
        return 0U;
    }

    if (hasARPEntry(id)) {
        return m_arpTable[id];
    } else {
        // do we have a static entry for this ID?
        lookups::RadioId rid = m_network->m_ridLookup->find(id);
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

/* Helper to get the radio ID for the given IP address. */

uint32_t DMRPacketData::getRadioIdAddress(uint32_t addr)
{
    if (addr == 0U) {
        return 0U;
    }

    for (auto entry : m_arpTable) {
        if (entry.second == addr) {
            return entry.first;
        }
    }

    // check if we have an entry in the RID lookup
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
        m_arpTable[it->first] = addr;
        return it->first;
    }

    return 0U;
}
