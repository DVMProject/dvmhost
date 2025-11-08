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
#include "common/dmr/SlotType.h"
#include "common/dmr/Sync.h"
#include "common/edac/CRC.h"
#include "common/Clock.h"
#include "common/Log.h"
#include "common/Thread.h"
#include "common/Utils.h"
#include "network/FNENetwork.h"
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

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint8_t DATA_CALL_COLL_TIMEOUT = 60U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the DMRPacketData class. */

DMRPacketData::DMRPacketData(FNENetwork* network, TagDMRData* tag, bool debug) :
    m_network(network),
    m_tag(tag),
    m_status(),
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
