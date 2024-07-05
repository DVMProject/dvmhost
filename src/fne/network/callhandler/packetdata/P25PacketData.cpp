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
#include "common/p25/lc/tsbk/TSBKFactory.h"
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
using namespace network::callhandler::packetdata;
using namespace p25;
using namespace p25::defines;

#include <cassert>
#include <chrono>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the P25PacketData class. */

P25PacketData::P25PacketData(FNENetwork* network, bool debug) :
    m_network(network),
    m_status(),
    m_debug(debug)
{
    assert(network != nullptr);
}

/* Finalizes a instance of the P25PacketData class. */

P25PacketData::~P25PacketData() = default;

/* Process a data frame from the network. */

bool P25PacketData::processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint16_t pktSeq, uint32_t streamId, bool external)
{
    hrc::hrc_t pktTime = hrc::now();

    uint32_t blockLength = __GET_UINT16(data, 8U);

    uint8_t blocksToFollow = data[20U];
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

            LogMessage(LOG_NET, P25_PDU_STR ", peerId = %u, ack = %u, outbound = %u, fmt = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, packetLength = %u, n = %u, seqNo = %u, hdrOffset = %u, llId = %u",
                peerId, status->header.getAckNeeded(), status->header.getOutbound(), status->header.getFormat(), status->header.getSAP(), status->header.getFullMessage(),
                status->header.getBlocksToFollow(), status->header.getPadLength(), status->header.getPacketLength(), status->header.getNs(), status->header.getFSN(),
                status->header.getHeaderOffset(), status->header.getLLId());

            // make sure we don't get a PDU with more blocks then we support
            if (status->header.getBlocksToFollow() >= P25_MAX_PDU_BLOCKS) {
                LogError(LOG_NET, P25_PDU_STR ", too many PDU blocks to process, %u > %u", status->header.getBlocksToFollow(), P25_MAX_PDU_BLOCKS);
                return false;
            }

            status->llId = status->header.getLLId();

            m_status[peerId] = status;

            LogMessage(LOG_NET, "P25, Data Call Start, peer = %u, llId = %u, streamId = %u, external = %u", peerId, status->llId, streamId, external);
            return true;
        } else {
            LogError(LOG_NET, P25_PDU_STR ", illegal starting data block, peerId = %u", peerId);
            return false;
        }
    }

    RxStatus* status = m_status[peerId];

    ::memcpy(status->netPDU + status->dataOffset, data + 24U, blockLength);
    status->dataOffset += blockLength;
    status->netPDUCount++;
    status->dataBlockCnt++;

    if (status->dataBlockCnt >= status->header.getBlocksToFollow()) {
        uint32_t blocksToFollow = status->header.getBlocksToFollow();
        uint32_t offset = 0U;

        uint8_t buffer[P25_PDU_FEC_LENGTH_BYTES];

        // process second header if we're using enhanced addressing
        if (status->header.getSAP() == PDUSAP::EXT_ADDR &&
            status->header.getFormat() == PDUFormatType::UNCONFIRMED) {
            ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
            ::memcpy(buffer, status->netPDU, P25_PDU_FEC_LENGTH_BYTES);

            bool ret = status->secondHeader.decode(buffer);
            if (!ret) {
                LogWarning(LOG_NET, P25_PDU_STR ", unfixable RF 1/2 rate second header data");
                Utils::dump(1U, "Unfixable PDU Data", buffer, P25_PDU_HEADER_LENGTH_BYTES);

                delete status;
                m_status.erase(peerId);

                return false;
            }

            LogMessage(LOG_NET, P25_PDU_STR ", peerId = %u, fmt = $%02X, mfId = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, n = %u, seqNo = %u, lastFragment = %u, hdrOffset = %u, llId = %u",
                peerId, status->secondHeader.getFormat(), status->secondHeader.getMFId(), status->secondHeader.getSAP(), status->secondHeader.getFullMessage(),
                status->secondHeader.getBlocksToFollow(), status->secondHeader.getPadLength(), status->secondHeader.getNs(), status->secondHeader.getFSN(), status->secondHeader.getLastFragment(),
                status->secondHeader.getHeaderOffset(), status->secondHeader.getLLId());

            status->useSecondHeader = true;

            offset += P25_PDU_FEC_LENGTH_BYTES;
            blocksToFollow--;
        }

        status->dataBlockCnt = 0U;

        // process all blocks in the data stream
        uint32_t dataOffset = 0U;

        status->pduUserData = new uint8_t[P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U];
        ::memset(status->pduUserData, 0x00U, P25_MAX_PDU_BLOCKS * P25_PDU_CONFIRMED_LENGTH_BYTES + 2U);

        // if we are using a secondary header place it in the PDU user data buffer
        if (status->useSecondHeader) {
            status->secondHeader.getData(status->pduUserData + dataOffset);
            dataOffset += P25_PDU_HEADER_LENGTH_BYTES;
            status->pduUserDataLength += P25_PDU_HEADER_LENGTH_BYTES;
        }

        // decode data blocks
        for (uint32_t i = 0U; i < blocksToFollow; i++) {
            ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
            ::memcpy(buffer, status->netPDU + offset, P25_PDU_FEC_LENGTH_BYTES);

            bool ret = status->blockData[i].decode(buffer, (status->useSecondHeader) ? status->secondHeader : status->header);
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
                    LogMessage(LOG_NET, P25_PDU_STR ", peerId = %u, block %u, fmt = $%02X, lastBlock = %u, sap = $%02X, llId = %u",
                        peerId, status->blockData[i].getSerialNo(), status->blockData[i].getFormat(), status->blockData[i].getLastBlock(), status->blockData[i].getSAP(), status->blockData[i].getLLId());
                    if (m_network->m_dumpDataPacket) {
                        uint8_t dataBlock[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES];
                        ::memset(dataBlock, 0xAAU, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                        status->blockData[i].getData(dataBlock);
                        Utils::dump(2U, "Data Block", dataBlock, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                    }

                    status->secondHeader.reset();
                    status->secondHeader.setAckNeeded(true);
                    status->secondHeader.setFormat(status->blockData[i].getFormat());
                    status->secondHeader.setLLId(status->blockData[i].getLLId());
                    status->secondHeader.setSAP(status->blockData[i].getSAP());
                    status->extendedAddress = true;
                }
                else {
                    LogMessage(LOG_NET, P25_PDU_STR ", peerId = %u, block %u, fmt = $%02X, lastBlock = %u",
                        peerId, (status->header.getFormat() == PDUFormatType::CONFIRMED) ? status->blockData[i].getSerialNo() : status->dataBlockCnt, status->blockData[i].getFormat(),
                        status->blockData[i].getLastBlock());

                    if (m_network->m_dumpDataPacket) {
                        uint8_t dataBlock[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES];
                        ::memset(dataBlock, 0xAAU, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                        status->blockData[i].getData(dataBlock);
                        Utils::dump(2U, "Data Block", dataBlock, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                    }
                }

                status->blockData[i].getData(status->pduUserData + dataOffset);
                dataOffset += (status->header.getFormat() == PDUFormatType::CONFIRMED) ? P25_PDU_CONFIRMED_DATA_LENGTH_BYTES : P25_PDU_UNCONFIRMED_LENGTH_BYTES;
                status->pduUserDataLength = dataOffset;

                status->dataBlockCnt++;

                // is this the last block?
                if (status->blockData[i].getLastBlock() && status->dataBlockCnt == blocksToFollow) {
                    bool crcRet = edac::CRC::checkCRC32(status->pduUserData, status->pduUserDataLength);
                    if (!crcRet) {
                        LogWarning(LOG_NET, P25_PDU_STR ", failed CRC-32 check, blocks %u, len %u", blocksToFollow, status->pduUserDataLength);
                    }
                }
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
        uint32_t srcId = status->header.getLLId();
        uint32_t dstId = (status->useSecondHeader || status->extendedAddress) ? status->secondHeader.getLLId() : status->header.getLLId();
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

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Helper to dispatch PDU user data. */

void P25PacketData::dispatch(uint32_t peerId)
{
    RxStatus* status = m_status[peerId];

    if (m_network->m_dumpDataPacket) {
        Utils::dump(1U, "PDU Packet", status->pduUserData, status->pduUserDataLength);
    }

    uint32_t srcId = status->header.getLLId();
    uint32_t dstId = (status->useSecondHeader || status->extendedAddress) ? status->secondHeader.getLLId() : status->header.getLLId();

    // repeat traffic to the connected peers
    if (m_network->m_peers.size() > 0U) {
        uint32_t i = 0U;
        for (auto peer : m_network->m_peers) {
            if (peerId != peer.first) {
                // every 2 peers flush the queue
                if (i % 2U == 0U) {
                    m_network->m_frameQueue->flushQueue();
                }

                write_PDU_User(peer.first, nullptr, status->header, status->secondHeader, status->useSecondHeader, status->pduUserData, true);
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

                write_PDU_User(dstPeerId, peer.second, status->header, status->secondHeader, status->useSecondHeader, status->pduUserData);
                if (m_network->m_debug) {
                    LogDebug(LOG_NET, "P25, srcPeer = %u, dstPeer = %u, duid = $%02X, srcId = %u, dstId = %u", 
                        peerId, dstPeerId, DUID::PDU, srcId, dstId);
                }
            }
        }
    }
}

/* Helper to write user data as a P25 PDU packet. */

void P25PacketData::write_PDU_User(uint32_t peerId, network::PeerNetwork* peerNet, data::DataHeader& dataHeader, data::DataHeader& secondHeader, 
    bool useSecondHeader, uint8_t* pduUserData, bool queueOnly)
{
    assert(pduUserData != nullptr);

    uint32_t streamId = m_network->createStreamId();
    uint16_t pktSeq = 0U;

    uint8_t buffer[P25_PDU_FEC_LENGTH_BYTES];
    ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);

    uint32_t blocksToFollow = dataHeader.getBlocksToFollow();

    LogMessage(LOG_NET, P25_PDU_STR ", OSP, peerId = %u, ack = %u, outbound = %u, fmt = $%02X, mfId = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, packetLength = %u, n = %u, seqNo = %u, lastFragment = %u, hdrOffset = %u, llId = %u",
        peerId, dataHeader.getAckNeeded(), dataHeader.getOutbound(), dataHeader.getFormat(), dataHeader.getMFId(), dataHeader.getSAP(), dataHeader.getFullMessage(),
        dataHeader.getBlocksToFollow(), dataHeader.getPadLength(), dataHeader.getPacketLength(), dataHeader.getNs(), dataHeader.getFSN(), dataHeader.getLastFragment(),
        dataHeader.getHeaderOffset(), dataHeader.getLLId());

    // generate the PDU header and 1/2 rate Trellis
    dataHeader.encode(buffer);
    writeNetwork(peerId, peerNet, dataHeader, 0U, buffer, P25_PDU_FEC_LENGTH_BYTES, pktSeq, streamId, queueOnly);
    ++pktSeq;

    uint32_t dataOffset = 0U;

    // generate the second PDU header
    if (useSecondHeader) {
        secondHeader.encode(pduUserData, true);

        ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
        secondHeader.encode(buffer);
        writeNetwork(peerId, peerNet, dataHeader, 1U, buffer, P25_PDU_FEC_LENGTH_BYTES, pktSeq, streamId, queueOnly);
        ++pktSeq;

        dataOffset += P25_PDU_HEADER_LENGTH_BYTES;
        blocksToFollow--;

        LogMessage(LOG_NET, P25_PDU_STR ", OSP, peerId = %u, fmt = $%02X, mfId = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, n = %u, seqNo = %u, lastFragment = %u, hdrOffset = %u, llId = %u",
            peerId, secondHeader.getFormat(), secondHeader.getMFId(), secondHeader.getSAP(), secondHeader.getFullMessage(),
            secondHeader.getBlocksToFollow(), secondHeader.getPadLength(), secondHeader.getNs(), secondHeader.getFSN(), secondHeader.getLastFragment(),
            secondHeader.getHeaderOffset(), secondHeader.getLLId());
    }

    if (dataHeader.getFormat() != PDUFormatType::AMBT) {
        edac::CRC::addCRC32(pduUserData, (dataHeader.getPacketLength() + dataHeader.getPadLength() + 4U));
    }

    uint32_t networkBlock = 1U;
    if (useSecondHeader)
        ++networkBlock;

    // generate the PDU data
    for (uint32_t i = 0U; i < blocksToFollow; i++) {
        data::DataBlock dataBlock = data::DataBlock();
        dataBlock.setFormat((useSecondHeader) ? secondHeader : dataHeader);
        dataBlock.setSerialNo(i);
        dataBlock.setData(pduUserData + dataOffset);

        // are we processing extended address data from the first block?
        if (dataHeader.getSAP() == PDUSAP::EXT_ADDR && dataHeader.getFormat() == PDUFormatType::CONFIRMED &&
            dataBlock.getSerialNo() == 0U) {
            LogMessage(LOG_NET, P25_PDU_STR ", OSP, peerId = %u, block %u, fmt = $%02X, lastBlock = %u, sap = $%02X, llId = %u",
                peerId, dataBlock.getSerialNo(), dataBlock.getFormat(), dataBlock.getLastBlock(), dataBlock.getSAP(), dataBlock.getLLId());

            if (m_network->m_dumpDataPacket) {
                uint8_t rawDataBlock[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES];
                ::memset(rawDataBlock, 0xAAU, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                dataBlock.getData(rawDataBlock);
                Utils::dump(2U, "Data Block", rawDataBlock, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
            }
        }
        else {
            LogMessage(LOG_NET, P25_PDU_STR ", OSP, peerId = %u, block %u, fmt = $%02X, lastBlock = %u",
                peerId, (dataHeader.getFormat() == PDUFormatType::CONFIRMED) ? dataBlock.getSerialNo() : i, dataBlock.getFormat(),
                dataBlock.getLastBlock());

            if (m_network->m_dumpDataPacket) {
                uint8_t rawDataBlock[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES];
                ::memset(rawDataBlock, 0xAAU, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
                dataBlock.getData(rawDataBlock);
                Utils::dump(2U, "Data Block", rawDataBlock, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
            }
        }

        ::memset(buffer, 0x00U, P25_PDU_FEC_LENGTH_BYTES);
        dataBlock.encode(buffer);
        writeNetwork(peerId, peerNet, dataHeader, networkBlock, buffer, P25_PDU_FEC_LENGTH_BYTES, (dataBlock.getLastBlock()) ? RTP_END_OF_CALL_SEQ : pktSeq, streamId);
        ++pktSeq;

        networkBlock++;

        dataOffset += (dataHeader.getFormat() == PDUFormatType::CONFIRMED) ? P25_PDU_CONFIRMED_DATA_LENGTH_BYTES : P25_PDU_UNCONFIRMED_LENGTH_BYTES;
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
