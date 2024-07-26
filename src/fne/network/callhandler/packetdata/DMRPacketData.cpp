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

bool DMRPacketData::processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint16_t pktSeq, uint32_t streamId, bool external)
{
    hrc::hrc_t pktTime = hrc::now();

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

    if (dataSync) {
        dmrData.setData(data + 20U);
        dmrData.setDataType(dataType);
        dmrData.setN(0U);
    }

    uint8_t frame[DMR_FRAME_LENGTH_BYTES];
    dmrData.getData(frame);

    // is the stream valid?
    if (m_tag->validate(peerId, dmrData, streamId)) {
        // is this peer ignored?
        if (!m_tag->isPeerPermitted(peerId, dmrData, streamId)) {
            return false;
        }

        auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) { return x.second->peerId == peerId; });
        if (it != m_status.end()) {
            RxStatus* status = m_status[peerId];
            if (streamId != status->streamId) {
                LogWarning(LOG_NET, "DMR, Data Call Collision, peer = %u, streamId = %u, rxPeer = %u, rxLlId = %u, rxSlotNo = %u, rxStreamId = %u, external = %u",
                    peerId, streamId, status->peerId, status->srcId, status->slotNo, status->streamId, external);

                uint64_t duration = hrc::diff(pktTime, status->callStartTime);

                if ((duration / 1000) > DATA_CALL_COLL_TIMEOUT) {
                    LogWarning(LOG_NET, "DMR, force clearing stuck data call, timeout, peer = %u, streamId = %u, rxPeer = %u, rxLlId = %u, rxStreamId = %u, external = %u",
                        peerId, streamId, status->peerId, status->srcId, status->slotNo, status->streamId, external);

                    delete status;
                    m_status.erase(peerId);
                }

                return false;
            }
        } else {
            if (dataSync && (dataType == DataType::DATA_HEADER)) {
                // this is a new call stream
                RxStatus* status = new RxStatus();
                status->callStartTime = pktTime;
                status->srcId = srcId;
                status->dstId = dstId;
                status->slotNo = slotNo;
                status->streamId = streamId;
                status->peerId = peerId;

                bool ret = status->header.decode(frame);
                if (!ret) {
                    LogError(LOG_NET, "DMR Slot %u, DataType::DATA_HEADER, unable to decode the network data header", status->slotNo);
                    Utils::dump(1U, "Unfixable PDU Data", frame, DMR_FRAME_LENGTH_BYTES);

                    delete status;
                    m_status.erase(peerId);
                    return false;
                }

                status->frames = status->header.getBlocksToFollow();
                status->dataBlockCnt = 0U;

                bool gi = status->header.getGI();
                uint32_t srcId = status->header.getSrcId();
                uint32_t dstId = status->header.getDstId();

                LogMessage(LOG_NET, DMR_DT_DATA_HEADER ", peerId = %u, slot = %u, dpf = $%02X, ack = %u, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, packetLength = %u, seqNo = %u, dstId = %u, srcId = %u, group = %u",
                    peerId, status->slotNo, status->header.getDPF(), status->header.getA(), status->header.getSAP(), status->header.getFullMesage(), status->header.getBlocksToFollow(), status->header.getPadLength(), status->header.getPacketLength(),
                    status->header.getFSN(), dstId, srcId, gi);

                // make sure we don't get a PDU with more blocks then we support
                if (status->header.getBlocksToFollow() >= MAX_PDU_COUNT) {
                    LogError(LOG_NET, P25_PDU_STR ", too many PDU blocks to process, %u > %u", status->header.getBlocksToFollow(), MAX_PDU_COUNT);
                    return false;
                }

                m_status[peerId] = status;
                
                LogMessage(LOG_NET, "DMR, Data Call Start, peer = %u, slot = %u, srcId = %u, dstId = %u, group = %u, streamId = %u, external = %u", peerId, status->slotNo, status->srcId, status->dstId, gi, streamId, external);
                dispatchToFNE(peerId, dmrData, data, len, seqNo, pktSeq, streamId);

                return true;
            } else {
                return false;
            }
        }

        RxStatus* status = m_status[peerId];

        // a PDU header only with no blocks to follow is usually a response header
        if (status->header.getBlocksToFollow() == 0U) {
            LogMessage(LOG_NET, "DMR, Data Call End, peer = %u, slot = %u, srcId = %u, dstId = %u, streamId = %u, external = %u",
                peerId, status->slotNo, status->srcId, status->dstId, streamId, external);

            delete status;
            m_status.erase(peerId);
            return true;
        }

        data::DataBlock dataBlock;
        dataBlock.setDataType(dataType);

        bool ret = dataBlock.decode(frame, status->header);
        if (ret) {
            uint32_t blockLen = dataBlock.getData(status->pduUserData + status->pduDataOffset);
            status->pduDataOffset += blockLen;

            status->frames--;
            if (status->frames == 0U)
                dataBlock.setLastBlock(true);

            if (dataType == DataType::RATE_34_DATA) {
                LogMessage(LOG_NET, DMR_DT_RATE_34_DATA ", ISP, block %u, peer = %u, dataType = $%02X, dpf = $%02X", status->dataBlockCnt, peerId, dataBlock.getDataType(), dataBlock.getFormat());
            } else if (dataType == DataType::RATE_12_DATA) {
                LogMessage(LOG_NET, DMR_DT_RATE_12_DATA ", ISP, block %u, peer = %u, dataType = $%02X, dpf = $%02X", status->dataBlockCnt, peerId, dataBlock.getDataType(), dataBlock.getFormat());
            }
            else {
                LogMessage(LOG_NET, DMR_DT_RATE_1_DATA ", ISP, block %u, peer = %u, dataType = $%02X, dpf = $%02X", status->dataBlockCnt, peerId, dataBlock.getDataType(), dataBlock.getFormat());
            }

            dispatchToFNE(peerId, dmrData, data, len, seqNo, pktSeq, streamId);
            status->dataBlockCnt++;
        }

        // dispatch the PDU data
        if (status->dataBlockCnt > 0U && status->frames == 0U) {
            dispatch(peerId, dmrData, data, len);

            uint64_t duration = hrc::diff(pktTime, status->callStartTime);
            bool gi = status->header.getGI();
            uint32_t srcId = status->header.getSrcId();
            uint32_t dstId = status->header.getDstId();
            LogMessage(LOG_NET, "P25, Data Call End, peer = %u, slot = %u, srcId = %u, dstId = %u, group = %u, blocks = %u, duration = %u, streamId = %u, external = %u",
                peerId, srcId, dstId, gi, status->header.getBlocksToFollow(), duration / 1000, streamId, external);

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

            delete status;
            m_status.erase(peerId);
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Helper to dispatch PDU user data. */

void DMRPacketData::dispatch(uint32_t peerId, dmr::data::NetData& dmrData, const uint8_t* data, uint32_t len)
{
    RxStatus *status = m_status[peerId];

    if (status->header.getBlocksToFollow() > 0U && status->frames == 0U) {
        bool crcRet = edac::CRC::checkCRC32(status->pduUserData, status->pduDataOffset);
        if (!crcRet) {
            LogWarning(LOG_NET, P25_PDU_STR ", failed CRC-32 check, blocks %u, len %u", status->header.getBlocksToFollow(), status->pduDataOffset);
        }

        if (m_network->m_dumpDataPacket) {
            Utils::dump(1U, "PDU Packet", status->pduUserData, status->pduDataOffset);
        }
    }
}

/* Helper to dispatch PDU user data back to the FNE network. */

void DMRPacketData::dispatchToFNE(uint32_t peerId, dmr::data::NetData& dmrData, const uint8_t* data, uint32_t len, uint8_t seqNo, uint16_t pktSeq, uint32_t streamId)
{
    RxStatus* status = m_status[peerId];

    uint32_t srcId = status->header.getSrcId();
    uint32_t dstId = status->header.getDstId();

    // repeat traffic to the connected peers
    if (m_network->m_peers.size() > 0U) {
        uint32_t i = 0U;
        for (auto peer : m_network->m_peers) {
            if (peerId != peer.first) {
                // is this peer ignored?
                if (!m_tag->isPeerPermitted(peer.first, dmrData, streamId)) {
                    continue;
                }

                // every 5 peers flush the queue
                if (i % 5U == 0U) {
                    m_network->m_frameQueue->flushQueue();
                }

                m_network->writePeer(peer.first, { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR }, data, len, pktSeq, streamId, true);
                if (m_network->m_debug) {
                    LogDebug(LOG_NET, "DMR, srcPeer = %u, dstPeer = %u, seqNo = %u, srcId = %u, dstId = %u, slotNo = %u, len = %u, pktSeq = %u, stream = %u", 
                        peerId, peer.first, seqNo, srcId, dstId, status->slotNo, len, pktSeq, streamId);
                }

                if (!m_network->m_callInProgress)
                    m_network->m_callInProgress = true;
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
                // is this peer ignored?
                if (!m_tag->isPeerPermitted(dstPeerId, dmrData, streamId, true)) {
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

                peer.second->writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR }, data, len, pktSeq, streamId);
                if (m_network->m_debug) {
                    LogDebug(LOG_NET, "DMR, srcPeer = %u, dstPeer = %u, seqNo = %u, srcId = %u, dstId = %u, slotNo = %u, len = %u, pktSeq = %u, stream = %u", 
                        peerId, dstPeerId, seqNo, srcId, dstId, status->slotNo, len, pktSeq, streamId);
                }

                if (!m_network->m_callInProgress)
                    m_network->m_callInProgress = true;
            }
        }
    }
}
