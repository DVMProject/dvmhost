// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/Defines.h"
#include "common/p25/sndcp/SNDCPFactory.h"
#include "common/p25/Sync.h"
#include "common/edac/CRC.h"
#include "common/Clock.h"
#include "common/Log.h"
#include "common/Thread.h"
#include "common/Utils.h"
#include "network/FNENetwork.h"
#include "network/callhandler/packetdata/P25PacketData.h"
#include "HostFNE.h"

using namespace system_clock;
using namespace network;
using namespace network::callhandler;
using namespace network::callhandler::packetdata;
using namespace p25;
using namespace p25::defines;
using namespace p25::sndcp;

#include <cassert>
#include <chrono>

#if !defined(_WIN32)
#include <netinet/ip.h>
#endif // !defined(_WIN32)

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint8_t DATA_CALL_COLL_TIMEOUT = 60U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the P25PacketData class. */

P25PacketData::P25PacketData(FNENetwork* network, TagP25Data* tag, bool debug) :
    m_network(network),
    m_tag(tag),
    m_dataFrames(),
    m_status(),
    m_arpTable(),
    m_readyForPkt(),
    m_suNotReadyTimeout(),
    m_debug(debug)
{
    assert(network != nullptr);
    assert(tag != nullptr);
}

/* Finalizes a instance of the P25PacketData class. */

P25PacketData::~P25PacketData() = default;

/* Process a data frame from the network. */

bool P25PacketData::processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint16_t pktSeq, uint32_t streamId, bool external)
{
    hrc::hrc_t pktTime = hrc::now();

    uint32_t blockLength = __GET_UINT16(data, 8U);

    uint8_t currentBlock = data[21U];

    if (blockLength == 0U)
        return false;

    uint8_t buffer[P25_PDU_FEC_LENGTH_BYTES];
    ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
    ::memcpy(buffer, data + 24U, P25_PDU_FEC_LENGTH_BYTES);

    auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) { return x.second->peerId == peerId; });
    if (it != m_status.end()) {
        RxStatus* status = m_status[peerId];
        if (streamId != status->streamId) {
            LogWarning(LOG_NET, "P25, Data Call Collision, peer = %u, streamId = %u, rxPeer = %u, rxLlId = %u, rxStreamId = %u, external = %u",
                peerId, streamId, status->peerId, status->llId, status->streamId, external);

            uint64_t duration = hrc::diff(pktTime, status->callStartTime);

            if ((duration / 1000) > DATA_CALL_COLL_TIMEOUT) {
                LogWarning(LOG_NET, "P25, force clearing stuck data call, timeout, peer = %u, streamId = %u, rxPeer = %u, rxLlId = %u, rxStreamId = %u, external = %u",
                    peerId, streamId, status->peerId, status->llId, status->streamId, external);

                delete status;
                m_status.erase(peerId);
            }

            return false;
        }
    } else {
        if (currentBlock == 0U) {
            // this is a new call stream
            RxStatus* status = new RxStatus();
            status->callStartTime = pktTime;
            status->streamId = streamId;
            status->peerId = peerId;

            bool ret = status->header.decode(buffer);
            if (!ret) {
                LogWarning(LOG_NET, P25_PDU_STR ", unfixable RF 1/2 rate header data");
                Utils::dump(1U, "Unfixable PDU Data", buffer, P25_PDU_FEC_LENGTH_BYTES);
                return false;
            }

            LogMessage(LOG_NET, P25_PDU_STR ", peerId = %u, ack = %u, outbound = %u, fmt = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, packetLength = %u, S = %u, n = %u, seqNo = %u, hdrOffset = %u, llId = %u",
                peerId, status->header.getAckNeeded(), status->header.getOutbound(), status->header.getFormat(), status->header.getSAP(), status->header.getFullMessage(),
                status->header.getBlocksToFollow(), status->header.getPadLength(), status->header.getPacketLength(), status->header.getSynchronize(), status->header.getNs(), status->header.getFSN(),
                status->header.getHeaderOffset(), status->header.getLLId());

            // make sure we don't get a PDU with more blocks then we support
            if (status->header.getBlocksToFollow() >= P25_MAX_PDU_BLOCKS) {
                LogError(LOG_NET, P25_PDU_STR ", too many PDU blocks to process, %u > %u", status->header.getBlocksToFollow(), P25_MAX_PDU_BLOCKS);
                return false;
            }

            status->llId = status->header.getLLId();

            m_status[peerId] = status;

            // is this a response header?
            if (status->header.getFormat() == PDUFormatType::RSP) {
                dispatch(peerId);

                delete status;
                m_status.erase(peerId);
                return true;
            }

            if (status->header.getSAP() != PDUSAP::EXT_ADDR &&
                status->header.getFormat() != PDUFormatType::UNCONFIRMED) {
                m_readyForPkt[status->llId] = true;
                m_suSendSeq[status->llId] = 0U;
            }

            LogMessage(LOG_NET, "P25, Data Call Start, peer = %u, llId = %u, streamId = %u, external = %u", peerId, status->llId, streamId, external);
            return true;
        } else {
            LogError(LOG_NET, P25_PDU_STR ", illegal starting data block, peerId = %u", peerId);
            return false;
        }
    }

    RxStatus* status = m_status[peerId];

    // is the source ID a blacklisted ID?
    lookups::RadioId rid = m_network->m_ridLookup->find(status->header.getLLId());
    if (!rid.radioDefault()) {
        if (!rid.radioEnabled()) {
            // report error event to InfluxDB
            if (m_network->m_enableInfluxDB) {
                influxdb::QueryBuilder()
                    .meas("call_error_event")
                        .tag("peerId", std::to_string(peerId))
                        .tag("streamId", std::to_string(streamId))
                        .tag("srcId", std::to_string(status->header.getLLId()))
                        .tag("dstId", std::to_string(status->header.getLLId()))
                            .field("message", INFLUXDB_ERRSTR_DISABLED_SRC_RID)
                        .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                    .request(m_network->m_influxServer);
            }

            delete status;
            m_status.erase(peerId);
            return false;
        }
    }

    ::memcpy(status->netPDU + status->dataOffset, data + 24U, blockLength);
    status->dataOffset += blockLength;
    status->netPDUCount++;
    status->dataBlockCnt++;

    if (status->dataBlockCnt >= status->header.getBlocksToFollow()) {
        uint32_t blocksToFollow = status->header.getBlocksToFollow();
        uint32_t offset = 0U;
        uint32_t dataOffset = 0U;

        uint8_t buffer[P25_PDU_FEC_LENGTH_BYTES];

        status->dataBlockCnt = 0U;

        // process all blocks in the data stream
        status->pduUserData = new uint8_t[P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U];
        ::memset(status->pduUserData, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U);

        // process second header if we're using enhanced addressing
        if (status->header.getSAP() == PDUSAP::EXT_ADDR &&
            status->header.getFormat() == PDUFormatType::UNCONFIRMED) {
            ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
            ::memcpy(buffer, status->netPDU, P25_PDU_FEC_LENGTH_BYTES);

            bool ret = status->header.decodeExtAddr(buffer);
            if (!ret) {
                LogWarning(LOG_NET, P25_PDU_STR ", unfixable RF 1/2 rate second header data");
                Utils::dump(1U, "Unfixable PDU Data", buffer, P25_PDU_HEADER_LENGTH_BYTES);

                delete status;
                m_status.erase(peerId);

                return false;
            }

            LogMessage(LOG_NET, P25_PDU_STR ", ISP, extended address, sap = $%02X, srcLlId = %u",
                status->header.getEXSAP(), status->header.getSrcLLId());

            status->extendedAddress = true;
            status->llId = status->header.getSrcLLId();
            m_readyForPkt[status->llId] = true;
            m_suSendSeq[status->llId] = 0U;

            offset += P25_PDU_FEC_LENGTH_BYTES;
            blocksToFollow--;

            // if we are using a secondary header place it in the PDU user data buffer
            status->header.getExtAddrData(status->pduUserData + dataOffset);
            dataOffset += P25_PDU_HEADER_LENGTH_BYTES;
            status->pduUserDataLength += P25_PDU_HEADER_LENGTH_BYTES;
        }

        // decode data blocks
        for (uint32_t i = 0U; i < blocksToFollow; i++) {
            ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
            ::memcpy(buffer, status->netPDU + offset, P25_PDU_FEC_LENGTH_BYTES);

            bool ret = status->blockData[i].decode(buffer, status->header);
            if (ret) {
                // if we are getting unconfirmed or confirmed blocks, and if we've reached the total number of blocks
                // set this block as the last block for full packet CRC
                if ((status->header.getFormat() == PDUFormatType::CONFIRMED) || (status->header.getFormat() == PDUFormatType::UNCONFIRMED)) {
                    if ((status->dataBlockCnt + 1U) == blocksToFollow) {
                        status->blockData[i].setLastBlock(true);
                    }
                }

                // are we processing extended address data from the first block?
                if (status->header.getSAP() == PDUSAP::EXT_ADDR && status->header.getFormat() == PDUFormatType::CONFIRMED &&
                    status->blockData[i].getSerialNo() == 0U) {
                    uint8_t secondHeader[P25_PDU_HEADER_LENGTH_BYTES];
                    ::memset(secondHeader, 0x00U, P25_PDU_HEADER_LENGTH_BYTES);
                    status->blockData[i].getData(secondHeader);

                    status->header.decodeExtAddr(secondHeader);
                    LogMessage(LOG_NET, P25_PDU_STR ", ISP, block %u, fmt = $%02X, lastBlock = %u, sap = $%02X, srcLlId = %u",
                        status->blockData[i].getSerialNo(), status->blockData[i].getFormat(), status->blockData[i].getLastBlock(),
                        status->header.getEXSAP(), status->header.getSrcLLId());

                    status->extendedAddress = true;
                }
                else {
                    LogMessage(LOG_NET, P25_PDU_STR ", peerId = %u, block %u, fmt = $%02X, lastBlock = %u",
                        peerId, (status->header.getFormat() == PDUFormatType::CONFIRMED) ? status->blockData[i].getSerialNo() : status->dataBlockCnt, status->blockData[i].getFormat(),
                        status->blockData[i].getLastBlock());
                }

                status->blockData[i].getData(status->pduUserData + dataOffset);
                dataOffset += (status->header.getFormat() == PDUFormatType::CONFIRMED) ? P25_PDU_CONFIRMED_DATA_LENGTH_BYTES : P25_PDU_UNCONFIRMED_LENGTH_BYTES;
                status->pduUserDataLength = dataOffset;

                status->dataBlockCnt++;
            }
            else {
                if (status->blockData[i].getFormat() == PDUFormatType::CONFIRMED)
                    LogWarning(LOG_NET, P25_PDU_STR ", unfixable PDU data (3/4 rate or CRC), block %u", i);
                else
                    LogWarning(LOG_NET, P25_PDU_STR ", unfixable PDU data (1/2 rate or CRC), block %u", i);

                if (m_network->m_dumpDataPacket) {
                    Utils::dump(1U, "Unfixable PDU Data", buffer, P25_PDU_FEC_LENGTH_BYTES);
                }
            }

            offset += P25_PDU_FEC_LENGTH_BYTES;
        }

        if (status->dataBlockCnt < blocksToFollow) {
            LogWarning(LOG_NET, P25_PDU_STR ", incomplete PDU (%d / %d blocks), peerId = %u, llId = %u", status->dataBlockCnt, blocksToFollow, peerId, status->llId);
        }

        // dispatch the PDU data
        if (status->dataBlockCnt > 0U) {
            dispatch(peerId);
        }

        uint64_t duration = hrc::diff(pktTime, status->callStartTime);
        uint32_t srcId = (status->extendedAddress) ? status->header.getSrcLLId() : status->header.getLLId();
        uint32_t dstId = status->header.getLLId();
        LogMessage(LOG_NET, "P25, Data Call End, peer = %u, srcId = %u, dstId = %u, blocks = %u, duration = %u, streamId = %u, external = %u",
            peerId, srcId, dstId, status->header.getBlocksToFollow(), duration / 1000, streamId, external);

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

        delete status;
        m_status.erase(peerId);
    }

    return true;
}

/* Process a data frame from the virtual IP network. */

void P25PacketData::processPacketFrame(const uint8_t* data, uint32_t len, bool alreadyQueued)
{
#if !defined(_WIN32)
    struct ip* ipHeader = (struct ip*)data;

    char srcIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ipHeader->ip_src), srcIp, INET_ADDRSTRLEN);

    char dstIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ipHeader->ip_dst), dstIp, INET_ADDRSTRLEN);

    uint8_t proto = ipHeader->ip_p;
    uint16_t pktLen = Utils::reverseEndian(ipHeader->ip_len); // bryanb: this could be problematic on different endianness

    LogMessage(LOG_NET, "P25, VTUN -> PDU IP Data, srcIp = %s, dstIp = %s, pktLen = %u, proto = %02X", srcIp, dstIp, pktLen, proto);
#if DEBUG_P25_PDU_DATA
    Utils::dump(1U, "P25PacketData::processPacketFrame() packet", data, pktLen);
#endif

    VTUNDataFrame dataFrame;
    dataFrame.buffer = new uint8_t[len];
    ::memcpy(dataFrame.buffer, data, len);
    dataFrame.bufferLen = len;
    dataFrame.pktLen = pktLen;

    uint32_t dstLlId = getLLIdAddress(Utils::reverseEndian(ipHeader->ip_dst.s_addr));

    dataFrame.srcHWAddr = WUID_FNE;
    dataFrame.srcProtoAddr = Utils::reverseEndian(ipHeader->ip_src.s_addr);
    dataFrame.tgtHWAddr = dstLlId;
    dataFrame.tgtProtoAddr = Utils::reverseEndian(ipHeader->ip_dst.s_addr);

    if (dstLlId == 0U) {
        LogMessage(LOG_NET, "P25, no ARP entry for, dstIp = %s", dstIp);
        write_PDU_ARP(Utils::reverseEndian(ipHeader->ip_dst.s_addr));
    }

    m_dataFrames.push_back(dataFrame);
#endif // !defined(_WIN32)
}

/* Updates the timer by the passed number of milliseconds. */

void P25PacketData::clock(uint32_t ms)
{
    // transmit queued data frames
    if (m_dataFrames.size() > 0) {
        auto& dataFrame = m_dataFrames[0];

        if (dataFrame.tgtHWAddr == 0U) {
            uint32_t dstLlId = getLLIdAddress(dataFrame.tgtProtoAddr);
            if (dstLlId == 0U)
                return;

            dataFrame.tgtHWAddr = dstLlId;
        }

        // don't allow another packet to go out if we haven't acked the previous
        if (!m_readyForPkt[dataFrame.tgtHWAddr]) {
            m_suNotReadyTimeout[dataFrame.tgtHWAddr].clock(ms);
            if (m_suNotReadyTimeout[dataFrame.tgtHWAddr].isRunning() && m_suNotReadyTimeout[dataFrame.tgtHWAddr].hasExpired()) {
                m_suNotReadyTimeout[dataFrame.tgtHWAddr].stop();
                m_readyForPkt[dataFrame.tgtHWAddr] = true;
            }

            return;
        }

        m_readyForPkt[dataFrame.tgtHWAddr] = false;
        m_suNotReadyTimeout[dataFrame.tgtHWAddr] = Timer(1000U, 5U, 0U);
        m_suNotReadyTimeout[dataFrame.tgtHWAddr].start();

        // assemble a P25 PDU frame header for transport...
        data::DataHeader rspHeader = data::DataHeader();
        rspHeader.setFormat(PDUFormatType::CONFIRMED);
        rspHeader.setMFId(MFG_STANDARD);
        rspHeader.setAckNeeded(true);
        rspHeader.setOutbound(true);
        rspHeader.setSAP(PDUSAP::EXT_ADDR);
        rspHeader.setLLId(dataFrame.tgtHWAddr);
        rspHeader.setBlocksToFollow(1U);

        rspHeader.setEXSAP(PDUSAP::PACKET_DATA);
        rspHeader.setSrcLLId(WUID_FNE);

        rspHeader.calculateLength(dataFrame.pktLen);
        uint32_t pduLength = rspHeader.getPDULength();

        UInt8Array __pduUserData = std::make_unique<uint8_t[]>(pduLength);
        uint8_t* pduUserData = __pduUserData.get();
        ::memset(pduUserData, 0x00U, pduLength);
        ::memcpy(pduUserData + 4U, dataFrame.buffer, dataFrame.pktLen);
#if DEBUG_P25_PDU_DATA
        Utils::dump(1U, "P25PacketData::clock() pduUserData", pduUserData, pduLength);
#endif
        dispatchUserFrameToFNE(rspHeader, true, pduUserData);

        delete[] dataFrame.buffer;
        m_dataFrames.pop_front();
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Helper to dispatch PDU user data. */

void P25PacketData::dispatch(uint32_t peerId)
{
    RxStatus* status = m_status[peerId];

    bool crcValid = false;
    if (status->header.getBlocksToFollow() > 0U) {
        if (status->pduUserDataLength < 4U) {
            LogError(LOG_NET, P25_PDU_STR ", illegal PDU packet length, blocks %u, len %u", status->header.getBlocksToFollow(), status->pduUserDataLength);
            return;
        }

        crcValid = edac::CRC::checkCRC32(status->pduUserData, status->pduUserDataLength);
        if (!crcValid) {
            LogError(LOG_NET, P25_PDU_STR ", failed CRC-32 check, blocks %u, len %u", status->header.getBlocksToFollow(), status->pduUserDataLength);
            return;
        }
    }

    if (m_network->m_dumpDataPacket && status->dataBlockCnt > 0U) {
        Utils::dump(1U, "PDU Packet", status->pduUserData, status->pduUserDataLength);
    }    

    if (status->header.getFormat() == PDUFormatType::RSP) {
        LogMessage(LOG_NET, P25_PDU_STR ", ISP, response, fmt = $%02X, rspClass = $%02X, rspType = $%02X, rspStatus = $%02X, llId = %u, srcLlId = %u",
                status->header.getFormat(), status->header.getResponseClass(), status->header.getResponseType(), status->header.getResponseStatus(),
                status->header.getLLId(), status->header.getSrcLLId());
/*
        if (status->header.getResponseClass() == PDUAckClass::ACK && status->header.getResponseType() == PDUAckType::ACK) {
            m_readyForPkt[status->header.getSrcLLId()] = true;
        }
*/
        return;
    }

    uint8_t sap = (status->extendedAddress) ? status->header.getEXSAP() : status->header.getSAP();

    // don't dispatch SNDCP control, conventional data registration or ARP
    if (sap != PDUSAP::SNDCP_CTRL_DATA && sap != PDUSAP::CONV_DATA_REG &&
        sap != PDUSAP::ARP) {
        dispatchToFNE(peerId);
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

        uint8_t arpPacket[P25_PDU_ARP_PCKT_LENGTH];
        ::memset(arpPacket, 0x00U, P25_PDU_ARP_PCKT_LENGTH);
        ::memcpy(arpPacket, status->pduUserData + 12U, P25_PDU_ARP_PCKT_LENGTH);

        uint16_t opcode = __GET_UINT16B(arpPacket, 6U);
        uint32_t srcHWAddr = __GET_UINT16(arpPacket, 8U);
        uint32_t srcProtoAddr = __GET_UINT32(arpPacket, 11U);
        //uint32_t tgtHWAddr = __GET_UINT16(arpPacket, 15U);
        uint32_t tgtProtoAddr = __GET_UINT32(arpPacket, 18U);

        if (opcode == P25_PDU_ARP_REQUEST) {
            LogMessage(LOG_NET, P25_PDU_STR ", ARP request, who has %s? tell %s (%u)", __IP_FROM_UINT(tgtProtoAddr).c_str(), __IP_FROM_UINT(srcProtoAddr).c_str(), srcHWAddr);
            if (fneIPv4 == tgtProtoAddr) {
                write_PDU_ARP_Reply(fneIPv4, srcHWAddr, srcProtoAddr, WUID_FNE);
            } else {
                write_PDU_ARP_Reply(tgtProtoAddr, srcHWAddr, srcProtoAddr);
            }
        } else if (opcode == P25_PDU_ARP_REPLY) {
            LogMessage(LOG_NET, P25_PDU_STR ", ARP reply, %s is at %u", __IP_FROM_UINT(srcProtoAddr).c_str(), srcHWAddr);
            if (fneIPv4 == srcProtoAddr) {
                LogWarning(LOG_NET, P25_PDU_STR ", ARP reply, %u is trying to masquerade as us...", srcHWAddr);
            } else {
                m_arpTable[srcHWAddr] = srcProtoAddr;
            }

            m_readyForPkt[srcHWAddr] = true;
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

        int dataPktOffset = 0U;
        if (status->header.getFormat() == PDUFormatType::CONFIRMED && status->extendedAddress)
            dataPktOffset = 4U;
        if (status->header.getFormat() == PDUFormatType::UNCONFIRMED && status->extendedAddress)
            dataPktOffset = 12U;

        struct ip* ipHeader = (struct ip*)(status->pduUserData + dataPktOffset);

        char srcIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(ipHeader->ip_src), srcIp, INET_ADDRSTRLEN);

        char dstIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(ipHeader->ip_dst), dstIp, INET_ADDRSTRLEN);

        uint8_t proto = ipHeader->ip_p;
        uint16_t pktLen = Utils::reverseEndian(ipHeader->ip_len); // bryanb: this could be problematic on different endianness

        LogMessage(LOG_NET, "P25, PDU -> VTUN, IP Data, srcIp = %s, dstIp = %s, pktLen = %u, proto = %02X", srcIp, dstIp, pktLen, proto);

        UInt8Array __ipFrame = std::make_unique<uint8_t[]>(pktLen);
        uint8_t* ipFrame = __ipFrame.get();
        ::memset(ipFrame, 0x00U, pktLen);
        ::memcpy(ipFrame, status->pduUserData + dataPktOffset, pktLen);
#if DEBUG_P25_PDU_DATA
        Utils::dump(1U, "P25PacketData::dispatch() ipFrame", ipFrame, pktLen);
#endif
        if (!m_network->m_host->m_tun->write(ipFrame, pktLen)) {
            LogError(LOG_NET, P25_PDU_STR ", failed to write IP frame to virtual tunnel, len %u", pktLen);
        }

        write_PDU_Ack_Response(PDUAckClass::ACK, PDUAckType::ACK, status->header.getNs(), (status->extendedAddress) ? status->header.getSrcLLId() : status->header.getLLId());
        m_readyForPkt[status->header.getSrcLLId()] = true;
#endif // !defined(_WIN32)
    }
    break;
    case PDUSAP::SNDCP_CTRL_DATA:
    {
        LogMessage(LOG_NET, P25_PDU_STR ", SNDCP_CTRL_DATA (SNDCP Control Data), blocksToFollow = %u",
            status->header.getBlocksToFollow());

        processSNDCPControl(status);
    }
    break;
    default:
        break;
    }
}

/* Helper to dispatch PDU user data back to the FNE network. */

void P25PacketData::dispatchToFNE(uint32_t peerId)
{
    RxStatus* status = m_status[peerId];

    uint32_t srcId = (status->extendedAddress) ? status->header.getSrcLLId() : status->header.getLLId();
    uint32_t dstId = status->header.getLLId();

    // repeat traffic to the connected peers
    if (m_network->m_peers.size() > 0U) {
        uint32_t i = 0U;
        for (auto peer : m_network->m_peers) {
            if (peerId != peer.first) {
                // every 2 peers flush the queue
                if (i % 2U == 0U) {
                    m_network->m_frameQueue->flushQueue();
                }

                write_PDU_User(peer.first, nullptr, status->header, status->extendedAddress, status->pduUserData, true);
                if (m_network->m_debug) {
                    LogDebug(LOG_NET, "P25, srcPeer = %u, dstPeer = %u, duid = $%02X, srcId = %u, dstId = %u", 
                        peerId, peer.first, DUID::PDU, srcId, dstId);
                }

                i++;
            }
        }
        m_network->m_frameQueue->flushQueue();
    }

    // repeat traffic to external peers
    if (m_network->m_host->m_peerNetworks.size() > 0U) {
        for (auto peer : m_network->m_host->m_peerNetworks) {
            uint32_t dstPeerId = peer.second->getPeerId();

            // don't try to repeat traffic to the source peer...if this traffic
            // is coming from a external peer
            if (dstPeerId != peerId) {
                // check if the source peer is blocked from sending to this peer
                if (peer.second->checkBlockedPeer(peerId)) {
                    continue;
                }

                // skip peer if it isn't enabled
                if (!peer.second->isEnabled()) {
                    continue;
                }

                write_PDU_User(dstPeerId, peer.second, status->header, status->extendedAddress, status->pduUserData);
                if (m_network->m_debug) {
                    LogDebug(LOG_NET, "P25, srcPeer = %u, dstPeer = %u, duid = $%02X, srcId = %u, dstId = %u", 
                        peerId, dstPeerId, DUID::PDU, srcId, dstId);
                }
            }
        }
    }
}

/* Helper to dispatch PDU user data back to the FNE network. */

void P25PacketData::dispatchUserFrameToFNE(p25::data::DataHeader& dataHeader, bool extendedAddress, uint8_t* pduUserData)
{
    uint32_t srcId = (extendedAddress) ? dataHeader.getSrcLLId() : dataHeader.getLLId();
    uint32_t dstId = dataHeader.getLLId();

    uint8_t sendSeqNo = m_suSendSeq[srcId];
    if (sendSeqNo == 0U) {
        dataHeader.setSynchronize(true);
    }

    dataHeader.setNs(sendSeqNo);
    ++sendSeqNo;
    if (sendSeqNo > 7U)
        sendSeqNo = 0U;
    m_suSendSeq[srcId] = sendSeqNo;

    // repeat traffic to the connected peers
    if (m_network->m_peers.size() > 0U) {
        uint32_t i = 0U;
        for (auto peer : m_network->m_peers) {
            // every 2 peers flush the queue
            if (i % 2U == 0U) {
                m_network->m_frameQueue->flushQueue();
            }

            write_PDU_User(peer.first, nullptr, dataHeader, extendedAddress, pduUserData, true);
            if (m_network->m_debug) {
                LogDebug(LOG_NET, "P25, dstPeer = %u, duid = $%02X, srcId = %u, dstId = %u", 
                    peer.first, DUID::PDU, srcId, dstId);
            }

            i++;
        }
        m_network->m_frameQueue->flushQueue();
    }

    // repeat traffic to external peers
    if (m_network->m_host->m_peerNetworks.size() > 0U) {
        for (auto peer : m_network->m_host->m_peerNetworks) {
            uint32_t dstPeerId = peer.second->getPeerId();

            write_PDU_User(dstPeerId, peer.second, dataHeader, extendedAddress, pduUserData);
            if (m_network->m_debug) {
                LogDebug(LOG_NET, "P25, dstPeer = %u, duid = $%02X, srcId = %u, dstId = %u", 
                    dstPeerId, DUID::PDU, srcId, dstId);
            }
        }
    }
}

/* Helper used to process SNDCP control data from PDU data. */

bool P25PacketData::processSNDCPControl(RxStatus* status)
{
    std::unique_ptr<sndcp::SNDCPPacket> packet = SNDCPFactory::create(status->pduUserData);
    if (packet == nullptr) {
        LogWarning(LOG_NET, P25_PDU_STR ", undecodable SNDCP packet");
        return false;
    }

    uint32_t llId = status->header.getLLId();

    switch (packet->getPDUType()) {
        case SNDCP_PDUType::ACT_TDS_CTX:
        {
            SNDCPCtxActRequest* isp = static_cast<SNDCPCtxActRequest*>(packet.get());
            LogMessage(LOG_NET, P25_PDU_STR ", SNDCP context activation request, llId = %u, nsapi = %u, ipAddr = %s, nat = $%02X, dsut = $%02X, mdpco = $%02X", llId,
                isp->getNSAPI(), __IP_FROM_UINT(isp->getIPAddress()).c_str(), isp->getNAT(), isp->getDSUT(), isp->getMDPCO());

            m_arpTable[llId] = isp->getIPAddress();
        }
        break;

        case SNDCP_PDUType::DEACT_TDS_CTX_REQ:
        {
            SNDCPCtxDeactivation* isp = static_cast<SNDCPCtxDeactivation*>(packet.get());
            LogMessage(LOG_NET, P25_PDU_STR ", SNDCP context deactivation request, llId = %u, deactType = %02X", llId,
                isp->getDeactType());

            m_arpTable.erase(llId);
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

    __SET_UINT16B(P25_PDU_ARP_CAI_TYPE, arpPacket, 0U);         // Hardware Address Type
    __SET_UINT16B(PDUSAP::PACKET_DATA, arpPacket, 2U);          // Protocol Address Type
    arpPacket[4U] = P25_PDU_ARP_HW_ADDR_LENGTH;                 // Hardware Address Length
    arpPacket[5U] = P25_PDU_ARP_PROTO_ADDR_LENGTH;              // Protocol Address Length
    __SET_UINT16B(P25_PDU_ARP_REQUEST, arpPacket, 6U);          // Opcode

    __SET_UINT16(WUID_FNE, arpPacket, 8U);                      // Sender Hardware Address

    std::string fneIPv4 = m_network->m_host->m_tun->getIPv4();
    __SET_UINT32(__IP_FROM_STR(fneIPv4), arpPacket, 11U);       // Sender Protocol Address

    __SET_UINT32(addr, arpPacket, 18U);                         // Target Protocol Address
#if DEBUG_P25_PDU_DATA
    Utils::dump(1U, "P25PacketData::write_PDU_ARP() arpPacket", arpPacket, P25_PDU_ARP_PCKT_LENGTH);
#endif
    LogMessage(LOG_NET, P25_PDU_STR ", ARP request, who has %s? tell %s (%u)", __IP_FROM_UINT(addr).c_str(), fneIPv4.c_str(), WUID_FNE);

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

    UInt8Array __pduUserData = std::make_unique<uint8_t[]>(pduLength);
    uint8_t* pduUserData = __pduUserData.get();
    ::memset(pduUserData, 0x00U, pduLength);
    ::memcpy(pduUserData + P25_PDU_HEADER_LENGTH_BYTES, arpPacket, P25_PDU_ARP_PCKT_LENGTH);

    dispatchUserFrameToFNE(rspHeader, true, pduUserData);
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

    __SET_UINT16B(P25_PDU_ARP_CAI_TYPE, arpPacket, 0U);         // Hardware Address Type
    __SET_UINT16B(PDUSAP::PACKET_DATA, arpPacket, 2U);          // Protocol Address Type
    arpPacket[4U] = P25_PDU_ARP_HW_ADDR_LENGTH;                 // Hardware Address Length
    arpPacket[5U] = P25_PDU_ARP_PROTO_ADDR_LENGTH;              // Protocol Address Length
    __SET_UINT16B(P25_PDU_ARP_REPLY, arpPacket, 6U);            // Opcode

    __SET_UINT16(tgtLlid, arpPacket, 8U);                       // Sender Hardware Address
    __SET_UINT32(targetAddr, arpPacket, 11U);                   // Sender Protocol Address

    __SET_UINT16(requestorLlid, arpPacket, 15U);                // Requestor Hardware Address
    __SET_UINT32(requestorAddr, arpPacket, 18U);                // Requestor Protocol Address
#if DEBUG_P25_PDU_DATA
    Utils::dump(1U, "P25PacketData::write_PDU_ARP_Reply() arpPacket", arpPacket, P25_PDU_ARP_PCKT_LENGTH);
#endif
    LogMessage(LOG_NET, P25_PDU_STR ", ARP reply, %s is at %u", __IP_FROM_UINT(targetAddr).c_str(), tgtLlid);

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

    UInt8Array __pduUserData = std::make_unique<uint8_t[]>(pduLength);
    uint8_t* pduUserData = __pduUserData.get();
    ::memset(pduUserData, 0x00U, pduLength);
    ::memcpy(pduUserData + P25_PDU_HEADER_LENGTH_BYTES, arpPacket, P25_PDU_ARP_PCKT_LENGTH);

    dispatchUserFrameToFNE(rspHeader, true, pduUserData);
}

/* Helper to write a PDU acknowledge response. */

void P25PacketData::write_PDU_Ack_Response(uint8_t ackClass, uint8_t ackType, uint8_t ackStatus, uint32_t llId, uint32_t srcLlId)
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
        rspHeader.setFullMessage(false);
    }
    else {
        rspHeader.setFullMessage(true);
    }
    rspHeader.setBlocksToFollow(0U);

    dispatchUserFrameToFNE(rspHeader, srcLlId > 0U, nullptr);
}

/* Helper to write user data as a P25 PDU packet. */

void P25PacketData::write_PDU_User(uint32_t peerId, network::PeerNetwork* peerNet, data::DataHeader& dataHeader,
    bool extendedAddress, uint8_t* pduUserData, bool queueOnly)
{
    uint32_t streamId = m_network->createStreamId();
    uint16_t pktSeq = 0U;

    uint8_t buffer[P25_PDU_FEC_LENGTH_BYTES];
    ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

    uint32_t blocksToFollow = dataHeader.getBlocksToFollow();

    LogMessage(LOG_NET, P25_PDU_STR ", OSP, peerId = %u, ack = %u, outbound = %u, fmt = $%02X, mfId = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, packetLength = %u, S = %u, n = %u, seqNo = %u, lastFragment = %u, hdrOffset = %u, llId = %u",
        peerId, dataHeader.getAckNeeded(), dataHeader.getOutbound(), dataHeader.getFormat(), dataHeader.getMFId(), dataHeader.getSAP(), dataHeader.getFullMessage(),
        dataHeader.getBlocksToFollow(), dataHeader.getPadLength(), dataHeader.getPacketLength(), dataHeader.getSynchronize(), dataHeader.getNs(), dataHeader.getFSN(), dataHeader.getLastFragment(),
        dataHeader.getHeaderOffset(), dataHeader.getLLId());

    // generate the PDU header and 1/2 rate Trellis
    dataHeader.encode(buffer);
    writeNetwork(peerId, peerNet, dataHeader, 0U, buffer, P25_PDU_FEC_LENGTH_BYTES, pktSeq, streamId, queueOnly);

    if (pduUserData == nullptr)
        return;

    ++pktSeq;
    uint32_t packetLength = dataHeader.getPDULength();

    if (blocksToFollow > 0U) {
        uint32_t dataOffset = 0U;
        uint32_t networkBlock = 1U;

        // generate the second PDU header
        if ((dataHeader.getFormat() == PDUFormatType::UNCONFIRMED) && (dataHeader.getSAP() == PDUSAP::EXT_ADDR) && extendedAddress) {
            dataHeader.encodeExtAddr(pduUserData, true);

            ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
            dataHeader.encodeExtAddr(buffer);
            writeNetwork(peerId, peerNet, dataHeader, 1U, buffer, P25_PDU_FEC_LENGTH_BYTES, pktSeq, streamId, queueOnly);
            ++pktSeq;

            dataOffset += P25_PDU_HEADER_LENGTH_BYTES;

            blocksToFollow--;
            networkBlock++;

            LogMessage(LOG_NET, P25_PDU_STR ", OSP, extended address, sap = $%02X, srcLlId = %u",
                dataHeader.getEXSAP(), dataHeader.getSrcLLId());
        }

        // are we processing extended address data from the first block?
        if ((dataHeader.getFormat() == PDUFormatType::CONFIRMED) && (dataHeader.getSAP() == PDUSAP::EXT_ADDR) && extendedAddress) {
            dataHeader.encodeExtAddr(pduUserData);

            LogMessage(LOG_NET, P25_PDU_STR ", OSP, sap = $%02X, srcLlId = %u",
                dataHeader.getEXSAP(), dataHeader.getSrcLLId());
        }

        if (dataHeader.getFormat() != PDUFormatType::AMBT) {
            edac::CRC::addCRC32(pduUserData, packetLength);
        }

        if (m_network->m_dumpDataPacket) {
            Utils::dump("OSP PDU User Data", pduUserData, packetLength);
        }

        // generate the PDU data
        for (uint32_t i = 0U; i < blocksToFollow; i++) {
            data::DataBlock dataBlock = data::DataBlock();
            dataBlock.setFormat(dataHeader);
            dataBlock.setSerialNo(i);
            dataBlock.setData(pduUserData + dataOffset);

            LogMessage(LOG_NET, P25_PDU_STR ", OSP, peerId = %u, block %u, fmt = $%02X, lastBlock = %u",
                peerId, (dataHeader.getFormat() == PDUFormatType::CONFIRMED) ? dataBlock.getSerialNo() : i, dataBlock.getFormat(),
                dataBlock.getLastBlock());

            ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
            dataBlock.encode(buffer);
            writeNetwork(peerId, peerNet, dataHeader, networkBlock, buffer, P25_PDU_FEC_LENGTH_BYTES, (dataBlock.getLastBlock()) ? RTP_END_OF_CALL_SEQ : pktSeq, streamId);
            ++pktSeq;

            dataOffset += (dataHeader.getFormat() == PDUFormatType::CONFIRMED) ? P25_PDU_CONFIRMED_DATA_LENGTH_BYTES : P25_PDU_UNCONFIRMED_LENGTH_BYTES;

            networkBlock++;
        }
    }
}

/* Write data processed to the network. */

bool P25PacketData::writeNetwork(uint32_t peerId, network::PeerNetwork* peerNet, const p25::data::DataHeader& dataHeader, const uint8_t currentBlock, 
    const uint8_t *data, uint32_t len, uint16_t pktSeq, uint32_t streamId, bool queueOnly)
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
        return m_network->writePeer(peerId, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, pktSeq, streamId, false, true);
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

    return 0U;
}
