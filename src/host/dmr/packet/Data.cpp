// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017,2018 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/dmr/acl/AccessControl.h"
#include "common/dmr/lc/FullLC.h"
#include "common/dmr/lc/CSBK.h"
#include "common/dmr/SlotType.h"
#include "common/dmr/Sync.h"
#include "common/edac/BPTC19696.h"
#include "common/edac/CRC.h"
#include "common/edac/Trellis.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "dmr/packet/Data.h"
#include "dmr/Slot.h"
#include "ActivityLog.h"
#include "HostMain.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::packet;

#include <cassert>
#include <algorithm>

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

// Helper macro to check if the host is authoritative and the destination ID is permitted.
#define CHECK_AUTHORITATIVE(_DST_ID)                                                    \
    if (!m_slot->m_authoritative && m_slot->m_permittedDstId != dstId) {                \
        if (!g_disableNonAuthoritativeLogging)                                          \
            LogWarning(LOG_RF, "[NON-AUTHORITATIVE] Ignoring RF traffic, destination not permitted!"); \
        m_slot->m_rfState = RS_RF_LISTENING;                                            \
        return false;                                                                   \
    }

// Helper macro to check if the host is authoritative and the destination ID is permitted.
#define CHECK_NET_AUTHORITATIVE(_DST_ID)                                                \
    if (!m_slot->m_authoritative && m_slot->m_permittedDstId != dstId) {                \
        return;                                                                         \
    }

// Helper macro to perform RF traffic collision checking.
#define CHECK_TRAFFIC_COLLISION(_DST_ID)                                                \
    /* don't process RF frames if the network isn't in a idle state and the RF destination is the network destination */ \
    if (m_slot->m_netState != RS_NET_IDLE && _DST_ID == m_slot->m_netLastDstId) {       \
        LogWarning(LOG_RF, "DMR Slot %u, Traffic collision detect, preempting new RF traffic to existing network traffic!", m_slot->m_slotNo); \
        m_slot->m_rfState = RS_RF_LISTENING;                                            \
        return false;                                                                   \
    }                                                                                   \
                                                                                        \
    if (m_slot->m_enableTSCC && _DST_ID == m_slot->m_netLastDstId) {                    \
        if (m_slot->m_affiliations->isNetGranted(_DST_ID)) {                            \
            LogWarning(LOG_RF, "DMR Slot %u, Traffic collision detect, preempting new RF traffic to existing granted network traffic (Are we in a voting condition?)", m_slot->m_slotNo); \
            m_slot->m_rfState = RS_RF_LISTENING;                                        \
            return false;                                                               \
        }                                                                               \
    }

// Helper macro to check if the RF talkgroup hang timer is running and the destination ID matches.
#define CHECK_TG_HANG(_DST_ID)                                                          \
    if (m_slot->m_rfLastDstId != 0U) {                                                  \
        if (m_slot->m_rfLastDstId != _DST_ID && (m_slot->m_rfTGHang.isRunning() && !m_slot->m_rfTGHang.hasExpired())) { \
            return;                                                                     \
        }                                                                               \
    }

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Process DMR data frame from the RF interface. */

bool Data::process(uint8_t* data, uint32_t len)
{
    assert(data != nullptr);

    // get the type from the packet metadata
    DataType::E dataType = (DataType::E)(data[1U] & 0x0FU);

    SlotType slotType;
    slotType.setColorCode(m_slot->m_colorCode);
    slotType.setDataType(dataType);

    if (dataType == DataType::TERMINATOR_WITH_LC) {
        if (m_slot->m_rfState != RS_RF_AUDIO)
            return false;

        // Regenerate the LC data
        lc::FullLC fullLC;
        fullLC.encode(*m_slot->m_rfLC, data + 2U, DataType::TERMINATOR_WITH_LC);

        // Regenerate the Slot Type
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        if (!m_slot->m_rfTimeout) {
            data[0U] = modem::TAG_EOT;
            data[1U] = 0x00U;

            m_slot->writeNetwork(data, DataType::TERMINATOR_WITH_LC, 0U);

            if (m_slot->m_duplex) {
                for (uint32_t i = 0U; i < m_slot->m_hangCount; i++)
                    m_slot->addFrame(data);
            }
        }

        if (m_verbose) {
            LogMessage(LOG_RF, DMR_DT_TERMINATOR_WITH_LC ", slot = %u, dstId = %u", m_slot->m_slotNo, m_slot->m_rfLC->getDstId());
        }

        // release trunked grant (if necessary)
        Slot *m_tscc = m_slot->m_dmr->getTSCCSlot();
        if (m_tscc != nullptr) {
            if (m_tscc->m_enableTSCC) {
                m_tscc->m_affiliations->releaseGrant(m_slot->m_rfLC->getDstId(), false);
                m_slot->clearTSCCActivated();
            }
        }

        if (m_slot->m_rssi != 0U) {
            ::ActivityLog("DMR", true, "Slot %u RF end of voice transmission, %.1f seconds, BER: %.1f%%, RSSI: -%u/-%u/-%u dBm",
                m_slot->m_slotNo, float(m_slot->m_rfFrames) / 16.667F, float(m_slot->m_rfErrs * 100U) / float(m_slot->m_rfBits),
                m_slot->m_minRSSI, m_slot->m_maxRSSI, m_slot->m_aveRSSI / m_slot->m_rssiCount);
        }
        else {
            ::ActivityLog("DMR", true, "Slot %u RF end of voice transmission, %.1f seconds, BER: %.1f%%",
                m_slot->m_slotNo, float(m_slot->m_rfFrames) / 16.667F, float(m_slot->m_rfErrs * 100U) / float(m_slot->m_rfBits));
        }

        LogMessage(LOG_RF, "DMR Slot %u, total frames: %d, total bits: %d, errors: %d, BER: %.4f%%",
            m_slot->m_slotNo, m_slot->m_rfFrames, m_slot->m_rfBits, m_slot->m_rfErrs, float(m_slot->m_rfErrs * 100U) / float(m_slot->m_rfBits));

        m_slot->m_dmr->tsccClearActivatedSlot(m_slot->m_slotNo);

        if (m_slot->m_rfTimeout) {
            m_slot->writeEndRF();
            return false;
        }
        else {
            m_slot->writeEndRF();
            return true;
        }
    }
    else if (dataType == DataType::DATA_HEADER) {
        if (m_slot->m_rfState == RS_RF_DATA)
            return true;

        m_rfDataHeader.reset();
        bool valid = m_rfDataHeader.decode(data + 2U);
        if (!valid)
            return false;

        bool gi = m_rfDataHeader.getGI();
        uint32_t srcId = m_rfDataHeader.getSrcId();
        uint32_t dstId = m_rfDataHeader.getDstId();

        CHECK_AUTHORITATIVE(dstId);
        CHECK_TRAFFIC_COLLISION(dstId);

        if (m_slot->m_tsccPayloadDstId != 0U && m_slot->m_tsccPayloadActRetry.isRunning()) {
            m_slot->m_tsccPayloadActRetry.stop();
        }

        // validate the source RID
        if (!acl::AccessControl::validateSrcId(srcId)) {
            LogWarning(LOG_RF, "DMR Slot %u, DATA_HEADER denial, RID rejection, srcId = %u", m_slot->m_slotNo, srcId);
            m_slot->m_rfState = RS_RF_REJECTED;
            return false;
        }

        // validate the target ID
        if (gi) {
            if (!acl::AccessControl::validateTGId(m_slot->m_slotNo, dstId)) {
                LogWarning(LOG_RF, "DMR Slot %u, DATA_HEADER denial, TGID rejection, srcId = %u, dstId = %u", m_slot->m_slotNo, srcId, dstId);
                m_slot->m_rfState = RS_RF_REJECTED;
                return false;
            }
        }

        m_rfDataBlockCnt = 0U;

        m_slot->m_rfFrames = m_rfDataHeader.getBlocksToFollow();
        m_slot->m_rfSeqNo = 0U;
        m_slot->m_rfLC = std::make_unique<lc::LC>(gi ? FLCO::GROUP : FLCO::PRIVATE, srcId, dstId);

        if (m_verbose) {
            LogMessage(LOG_RF, DMR_DT_DATA_HEADER ", slot = %u, dpf = $%02X, ack = %u, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, packetLength = %u, seqNo = %u, dstId = %u, srcId = %u, group = %u",
                m_slot->m_slotNo, m_rfDataHeader.getDPF(), m_rfDataHeader.getA(), m_rfDataHeader.getSAP(), m_rfDataHeader.getFullMesage(), m_rfDataHeader.getBlocksToFollow(), m_rfDataHeader.getPadLength(), m_rfDataHeader.getPacketLength(),
                m_rfDataHeader.getFSN(), dstId, srcId, gi);
        }

        // did we receive a response header?
        if (m_rfDataHeader.getDPF() == DPF::RESPONSE) {
            if (m_verbose) {
                LogMessage(LOG_RF, DMR_DT_DATA_HEADER " ISP, response, slot = %u, sap = $%02X, rspClass = $%02X, rspType = $%02X, rspStatus = $%02X, dstId = %u, srcId = %u, group = %u",
                        m_slot->m_slotNo, m_rfDataHeader.getSAP(), m_rfDataHeader.getResponseClass(), m_rfDataHeader.getResponseType(), m_rfDataHeader.getResponseStatus(),
                        dstId, srcId, gi);

                if (m_rfDataHeader.getResponseClass() == PDUResponseClass::ACK && m_rfDataHeader.getResponseType() == PDUResponseType::ACK) {
                    LogMessage(LOG_RF, DMR_DT_DATA_HEADER " ISP, response, OSP ACK, slot = %u, dstId = %u, srcId = %u, group = %u",
                        m_slot->m_slotNo, dstId, srcId, gi);
                } else {
                    if (m_rfDataHeader.getResponseClass() == PDUResponseClass::NACK) {
                        switch (m_rfDataHeader.getResponseType()) {
                            case PDUResponseType::NACK_ILLEGAL:
                                LogMessage(LOG_RF, DMR_DT_DATA_HEADER " ISP, response, OSP NACK, illegal format, slot = %u, dstId = %u, srcId = %u, group = %u",
                                    m_slot->m_slotNo, dstId, srcId, gi);
                                break;
                            case PDUResponseType::NACK_PACKET_CRC:
                                LogMessage(LOG_RF, DMR_DT_DATA_HEADER " ISP, response, OSP NACK, packet CRC error, slot = %u, dstId = %u, srcId = %u, group = %u",
                                    m_slot->m_slotNo, dstId, srcId, gi);
                                break;
                            case PDUResponseType::NACK_UNDELIVERABLE:
                                LogMessage(LOG_RF, DMR_DT_DATA_HEADER " ISP, response, OSP NACK, packet undeliverable, slot = %u, dstId = %u, srcId = %u, group = %u",
                                    m_slot->m_slotNo, dstId, srcId, gi);
                                break;

                            default:
                                break;
                            }
                    } else if (m_rfDataHeader.getResponseClass() == PDUResponseClass::ACK_RETRY) {
                        LogMessage(LOG_RF, DMR_DT_DATA_HEADER " ISP, response, OSP ACK RETRY, slot = %u, dstId = %u, srcId = %u, group = %u",
                            m_slot->m_slotNo, dstId, srcId, gi);
                    }
                }
            }
        }

        // Regenerate the data header
        m_rfDataHeader.encode(data + 2U);

        // Regenerate the Slot Type
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        data[0U] = m_slot->m_rfFrames == 0U ? modem::TAG_EOT : modem::TAG_DATA;
        data[1U] = 0x00U;

        if (m_slot->m_duplex && m_repeatDataPacket)
            m_slot->addFrame(data);

        uint8_t controlByte = 0U;
        if (m_slot->m_convNetGrantDemand)
            controlByte |= network::NET_CTRL_GRANT_DEMAND;                          // Grant Demand Flag

        m_slot->writeNetwork(data, DataType::DATA_HEADER, controlByte);

        m_slot->m_rfState = RS_RF_DATA;
        m_slot->m_rfLastDstId = dstId;
        m_slot->m_rfLastSrcId = srcId;

        if (m_slot->m_netState == RS_NET_IDLE) {
            m_slot->setShortLC(m_slot->m_slotNo, dstId, gi ? FLCO::GROUP : FLCO::PRIVATE, Slot::SLCO_ACT_TYPE::DATA);
        }

        ::ActivityLog("DMR", true, "Slot %u RF data header from %u to %s%u, %u blocks", m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId, m_slot->m_rfFrames);

        ::memset(m_pduUserData, 0x00U, MAX_PDU_COUNT * DMR_PDU_UNCODED_LENGTH_BYTES + 2U);
        m_pduDataOffset = 0U;

        if (m_slot->m_rfFrames == 0U) {
            ::ActivityLog("DMR", true, "Slot %u ended RF data transmission", m_slot->m_slotNo);
            m_slot->writeEndRF();
        }

        return true;
    }
    else if (dataType == DataType::RATE_12_DATA || dataType == DataType::RATE_34_DATA || dataType == DataType::RATE_1_DATA) {
        if (m_slot->m_rfState != RS_RF_DATA || m_slot->m_rfFrames == 0U)
            return false;

        data::DataBlock dataBlock;
        dataBlock.setDataType(dataType);

        bool ret = dataBlock.decode(data + 2U, m_rfDataHeader);
        if (ret) {
            uint32_t len = dataBlock.getData(m_pduUserData + m_pduDataOffset);
            m_pduDataOffset += len;

            m_slot->m_rfFrames--;
            if (m_slot->m_rfFrames == 0U)
                dataBlock.setLastBlock(true);

            if (m_verbose) {
                if (dataType == DataType::RATE_34_DATA) {
                    LogMessage(LOG_RF, DMR_DT_RATE_34_DATA ", ISP, block %u, dataType = $%02X, dpf = $%02X", m_rfDataBlockCnt, dataBlock.getDataType(), dataBlock.getFormat());
                } else if (dataType == DataType::RATE_12_DATA) {
                    LogMessage(LOG_RF, DMR_DT_RATE_12_DATA ", ISP, block %u, dataType = $%02X, dpf = $%02X", m_rfDataBlockCnt, dataBlock.getDataType(), dataBlock.getFormat());
                }
                else {
                    LogMessage(LOG_RF, DMR_DT_RATE_1_DATA ", ISP, block %u, dataType = $%02X, dpf = $%02X", m_rfDataBlockCnt, dataBlock.getDataType(), dataBlock.getFormat());
                }
            }

            dataBlock.encode(data + 2U);
            m_rfDataBlockCnt++;
        }

        if (m_rfDataHeader.getBlocksToFollow() > 0U && m_slot->m_rfFrames == 0U) {
            bool crcRet = edac::CRC::checkCRC32(m_pduUserData, m_pduDataOffset);
            if (!crcRet) {
                LogWarning(LOG_RF, P25_PDU_STR ", failed CRC-32 check, blocks %u, len %u", m_rfDataHeader.getBlocksToFollow(), m_pduDataOffset);
            }

            if (m_dumpDataPacket) {
                Utils::dump(1U, "DMR, PDU Packet", m_pduUserData, m_pduDataOffset);
            }
        }

        data[0U] = m_slot->m_rfFrames == 0U ? modem::TAG_EOT : modem::TAG_DATA;
        data[1U] = 0x00U;

        // regenerate the Slot Type
        slotType.encode(data + 2U);

        // convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        m_slot->writeNetwork(data, dataType, 0U);

        if (m_slot->m_duplex && m_repeatDataPacket) {
            m_slot->addFrame(data);
        }

        if (m_slot->m_rfFrames == 0U) {
            LogMessage(LOG_RF, "DMR Slot %u, RATE_12/34_DATA, ended data transmission", m_slot->m_slotNo);
            m_slot->writeEndRF();
        }

        return true;
    }

    return false;
}

/* Process a data frame from the network. */

void Data::processNetwork(const data::NetData& dmrData)
{
    DataType::E dataType = dmrData.getDataType();

    uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];
    dmrData.getData(data + 2U);

    if (dataType == DataType::TERMINATOR_WITH_LC) {
        if (m_slot->m_netState != RS_NET_AUDIO)
            return;

        // Regenerate the LC data
        lc::FullLC fullLC;
        fullLC.encode(*m_slot->m_netLC, data + 2U, DataType::TERMINATOR_WITH_LC);

        // Regenerate the Slot Type
        SlotType slotType;
        slotType.setColorCode(m_slot->m_colorCode);
        slotType.setDataType(DataType::TERMINATOR_WITH_LC);
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        if (!m_slot->m_netTimeout) {
            data[0U] = modem::TAG_EOT;
            data[1U] = 0x00U;

            if (m_slot->m_duplex) {
                for (uint32_t i = 0U; i < m_slot->m_hangCount; i++)
                    m_slot->addFrame(data, true);
            }
            else {
                for (uint32_t i = 0U; i < 3U; i++)
                    m_slot->addFrame(data, true);
            }
        }

        if (m_verbose) {
            LogMessage(LOG_RF, DMR_DT_TERMINATOR_WITH_LC ", slot = %u, dstId = %u", m_slot->m_slotNo, m_slot->m_netLC->getDstId());
        }

        // release trunked grant (if necessary)
        Slot *m_tscc = m_slot->m_dmr->getTSCCSlot();
        if (m_tscc != nullptr) {
            if (m_tscc->m_enableTSCC) {
                m_tscc->m_affiliations->releaseGrant(m_slot->m_netLC->getDstId(), false);
                m_slot->clearTSCCActivated();
            }
        }

        // We've received the voice header and terminator haven't we?
        m_slot->m_netFrames += 2U;
        ::ActivityLog("DMR", false, "Slot %u network end of voice transmission, %.1f seconds, %u%% packet loss, BER: %.1f%%",
            m_slot->m_slotNo, float(m_slot->m_netFrames) / 16.667F, (m_slot->m_netLost * 100U) / m_slot->m_netFrames, float(m_slot->m_netErrs * 100U) / float(m_slot->m_netBits));

        m_slot->m_dmr->tsccClearActivatedSlot(m_slot->m_slotNo);

        m_slot->writeEndNet();
    }
    else if (dataType == DataType::DATA_HEADER) {
        if (m_slot->m_netState == RS_NET_DATA)
            return;

        m_netDataHeader.reset();
        bool valid = m_netDataHeader.decode(data + 2U);
        if (!valid) {
            LogError(LOG_NET, "DMR Slot %u, DataType::DATA_HEADER, unable to decode the network data header", m_slot->m_slotNo);
            return;
        }

        bool gi = m_netDataHeader.getGI();
        uint32_t srcId = m_netDataHeader.getSrcId();
        uint32_t dstId = m_netDataHeader.getDstId();

        CHECK_NET_AUTHORITATIVE(dstId);
        CHECK_TG_HANG(dstId);

        if (m_slot->m_tsccPayloadDstId != 0U && m_slot->m_tsccPayloadActRetry.isRunning()) {
            m_slot->m_tsccPayloadActRetry.stop();
        }

        m_netDataBlockCnt = 0U;

        m_slot->m_netFrames = m_netDataHeader.getBlocksToFollow();
        m_slot->m_netLC = std::make_unique<lc::LC>(gi ? FLCO::GROUP : FLCO::PRIVATE, srcId, dstId);

        // did we receive a response header?
        if (m_netDataHeader.getDPF() == DPF::RESPONSE) {
            if (m_verbose) {
                LogMessage(LOG_NET, DMR_DT_DATA_HEADER " ISP, response, slot = %u, sap = $%02X, rspClass = $%02X, rspType = $%02X, rspStatus = $%02X, dstId = %u, srcId = %u, group = %u",
                        m_slot->m_slotNo, m_netDataHeader.getSAP(), m_netDataHeader.getResponseClass(), m_netDataHeader.getResponseType(), m_netDataHeader.getResponseStatus(),
                        dstId, srcId, gi);

                if (m_netDataHeader.getResponseClass() == PDUResponseClass::ACK && m_netDataHeader.getResponseType() == PDUResponseType::ACK) {
                    LogMessage(LOG_NET, DMR_DT_DATA_HEADER " ISP, response, OSP ACK, slot = %u, dstId = %u, srcId = %u, group = %u",
                        m_slot->m_slotNo, dstId, srcId, gi);
                } else {
                    if (m_netDataHeader.getResponseClass() == PDUResponseClass::NACK) {
                        switch (m_netDataHeader.getResponseType()) {
                            case PDUResponseType::NACK_ILLEGAL:
                                LogMessage(LOG_NET, DMR_DT_DATA_HEADER " ISP, response, OSP NACK, illegal format, slot = %u, dstId = %u, srcId = %u, group = %u",
                                    m_slot->m_slotNo, dstId, srcId, gi);
                                break;
                            case PDUResponseType::NACK_PACKET_CRC:
                                LogMessage(LOG_NET, DMR_DT_DATA_HEADER " ISP, response, OSP NACK, packet CRC error, slot = %u, dstId = %u, srcId = %u, group = %u",
                                    m_slot->m_slotNo, dstId, srcId, gi);
                                break;
                            case PDUResponseType::NACK_UNDELIVERABLE:
                                LogMessage(LOG_NET, DMR_DT_DATA_HEADER " ISP, response, OSP NACK, packet undeliverable, slot = %u, dstId = %u, srcId = %u, group = %u",
                                    m_slot->m_slotNo, dstId, srcId, gi);
                                break;

                            default:
                                break;
                            }
                    } else if (m_netDataHeader.getResponseClass() == PDUResponseClass::ACK_RETRY) {
                        LogMessage(LOG_NET, DMR_DT_DATA_HEADER " ISP, response, OSP ACK RETRY, slot = %u, dstId = %u, srcId = %u, group = %u",
                            m_slot->m_slotNo, dstId, srcId, gi);
                    }
                }
            }
        }

        // Regenerate the data header
        m_netDataHeader.encode(data + 2U);

        // Regenerate the Slot Type
        SlotType slotType;
        slotType.setColorCode(m_slot->m_colorCode);
        slotType.setDataType(DataType::DATA_HEADER);
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        data[0U] = m_slot->m_netFrames == 0U ? modem::TAG_EOT : modem::TAG_DATA;
        data[1U] = 0x00U;

        // Put a small delay into starting transmission
        m_slot->addFrame(m_slot->m_idle, true);
        m_slot->addFrame(m_slot->m_idle, true);

        m_slot->addFrame(data, true);

        m_slot->m_netState = RS_NET_DATA;
        m_slot->m_netLastDstId = dstId;
        m_slot->m_netLastSrcId = srcId;

        m_slot->setShortLC(m_slot->m_slotNo, dstId, gi ? FLCO::GROUP : FLCO::PRIVATE, Slot::SLCO_ACT_TYPE::DATA);

        if (m_verbose) {
            LogMessage(LOG_NET, DMR_DT_DATA_HEADER ", slot = %u, dpf = $%02X, ack = %u, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padLength = %u, packetLength = %u, seqNo = %u, dstId = %u, srcId = %u, group = %u",
                m_slot->m_slotNo, m_netDataHeader.getDPF(), m_netDataHeader.getA(), m_netDataHeader.getSAP(), m_netDataHeader.getFullMesage(), m_netDataHeader.getBlocksToFollow(), m_netDataHeader.getPadLength(), m_netDataHeader.getPacketLength(),
                m_netDataHeader.getFSN(), dstId, srcId, gi);
        }

        ::ActivityLog("DMR", false, "Slot %u network data header from %u to %s%u, %u blocks",
            m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId, m_slot->m_netFrames);

        ::memset(m_pduUserData, 0x00U, MAX_PDU_COUNT * DMR_PDU_UNCODED_LENGTH_BYTES + 2U);
        m_pduDataOffset = 0U;

        if (m_slot->m_netFrames == 0U) {
            ::ActivityLog("DMR", false, "Slot %u ended network data transmission", m_slot->m_slotNo);
            m_slot->writeEndNet();
        }
    }
    else if (dataType == DataType::RATE_12_DATA || dataType == DataType::RATE_34_DATA || dataType == DataType::RATE_1_DATA) {
        if (m_slot->m_netState != RS_NET_DATA || m_slot->m_netFrames == 0U) {
            m_slot->writeEndNet();
            return;
        }

        data::DataBlock dataBlock;
        dataBlock.setDataType(dataType);

        bool ret = dataBlock.decode(data + 2U, m_netDataHeader);
        if (ret) {
            uint32_t len = dataBlock.getData(m_pduUserData + m_pduDataOffset);
            m_pduDataOffset += len;

            m_slot->m_netFrames--;
            if (m_slot->m_netFrames == 0U)
                dataBlock.setLastBlock(true);

            if (m_verbose) {
                if (dataType == DataType::RATE_34_DATA) {
                    LogMessage(LOG_NET, DMR_DT_RATE_34_DATA ", ISP, block %u, dataType = $%02X, dpf = $%02X", m_netDataBlockCnt, dataBlock.getDataType(), dataBlock.getFormat());
                } else if (dataType == DataType::RATE_12_DATA) {
                    LogMessage(LOG_NET, DMR_DT_RATE_12_DATA ", ISP, block %u, dataType = $%02X, dpf = $%02X", m_netDataBlockCnt, dataBlock.getDataType(), dataBlock.getFormat());
                }
                else {
                    LogMessage(LOG_NET, DMR_DT_RATE_1_DATA ", ISP, block %u, dataType = $%02X, dpf = $%02X", m_netDataBlockCnt, dataBlock.getDataType(), dataBlock.getFormat());
                }
            }

            dataBlock.encode(data + 2U);
            m_netDataBlockCnt++;
        }

        if (m_netDataHeader.getBlocksToFollow() > 0U && m_slot->m_netFrames == 0U) {
            bool crcRet = edac::CRC::checkCRC32(m_pduUserData, m_pduDataOffset);
            if (!crcRet) {
                LogWarning(LOG_NET, P25_PDU_STR ", failed CRC-32 check, blocks %u, len %u", m_netDataHeader.getBlocksToFollow(), m_pduDataOffset);
            }

            if (m_dumpDataPacket) {
                Utils::dump(1U, "DMR, PDU Packet", m_pduUserData, m_pduDataOffset);
            }
        }

        if (m_repeatDataPacket) {
            // regenerate the Slot Type
            SlotType slotType;
            slotType.decode(data + 2U);
            slotType.setColorCode(m_slot->m_colorCode);
            slotType.encode(data + 2U);

            // convert the Data Sync to be from the BS or MS as needed
            Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

            data[0U] = m_slot->m_netFrames == 0U ? modem::TAG_EOT : modem::TAG_DATA;
            data[1U] = 0x00U;

            m_slot->addFrame(data, true);
        }

        if (m_slot->m_netFrames == 0U) {
            LogMessage(LOG_NET, "DMR Slot %u, RATE_12/34_DATA, ended data transmission", m_slot->m_slotNo);
            m_slot->writeEndNet();
        }
    }
    else {
        // Unhandled data type
        LogWarning(LOG_NET, "DMR Slot %u, unhandled network data, type = $%02X", m_slot->m_slotNo, dataType);
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the Data class. */

Data::Data(Slot* slot, network::BaseNetwork* network, bool dumpDataPacket, bool repeatDataPacket, bool debug, bool verbose) :
    m_slot(slot),
    m_rfDataHeader(),
    m_rfDataBlockCnt(0U),
    m_netDataHeader(),
    m_netDataBlockCnt(0U),
    m_pduUserData(nullptr),
    m_pduDataOffset(0U),
    m_lastRejectId(0U),
    m_dumpDataPacket(dumpDataPacket),
    m_repeatDataPacket(repeatDataPacket),
    m_verbose(verbose),
    m_debug(debug)
{
    m_pduUserData = new uint8_t[MAX_PDU_COUNT * DMR_PDU_UNCODED_LENGTH_BYTES + 2U];
    ::memset(m_pduUserData, 0x00U, MAX_PDU_COUNT * DMR_PDU_UNCODED_LENGTH_BYTES + 2U);
}

/* Finalizes a instance of the Data class. */

Data::~Data()
{
    delete[] m_pduUserData;
}

/* Helper to write a DMR PDU packet. */

void Data::writeRF_PDU(DataType::E dataType, const uint8_t* pdu)
{
    assert(pdu != nullptr);

    if ((dataType != DataType::DATA_HEADER) &&
        (dataType != DataType::RATE_12_DATA) &&
        (dataType != DataType::RATE_34_DATA) &&
        (dataType != DataType::RATE_1_DATA))
        return;

    // don't add any frames if the queue is full
    uint8_t len = DMR_FRAME_LENGTH_BYTES + 2U;
    uint32_t space = m_slot->m_txQueue.freeSpace();
    if (space < (len + 1U)) {
        return;
    }

    uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, DMR_FRAME_LENGTH_BYTES);
    ::memcpy(data, pdu, DMR_FRAME_LENGTH_BYTES);

    SlotType slotType;
    slotType.setColorCode(m_slot->m_colorCode);
    slotType.setDataType(dataType);

    // Regenerate the Slot Type
    slotType.encode(data + 2U);

    // Convert the Data Sync to be from the BS or MS as needed
    Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

    m_slot->m_rfSeqNo = 0U;

    data[0U] = modem::TAG_DATA;
    data[1U] = 0x00U;

    if (m_slot->m_duplex)
        m_slot->addFrame(data);
}

/* Helper to write a PDU acknowledge response. */

void Data::writeRF_PDU_Ack_Response(uint8_t rspClass, uint8_t rspType, uint8_t rspStatus, uint8_t sap, bool gi, uint32_t srcId, uint32_t dstId)
{
    if (rspClass == PDUResponseClass::ACK && rspType != PDUResponseType::ACK)
        return;

    edac::BPTC19696 bptc;

    uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, DMR_FRAME_LENGTH_BYTES);

    data::DataHeader rspHeader = data::DataHeader();
    rspHeader.setDPF(DPF::RESPONSE);
    rspHeader.setSAP(sap);
    rspHeader.setGI(gi);
    rspHeader.setSrcId(srcId);
    rspHeader.setDstId(dstId);
    rspHeader.setResponseClass(rspClass);
    rspHeader.setResponseType(rspType);
    rspHeader.setResponseStatus(rspStatus);
    rspHeader.setBlocksToFollow(1U);

    rspHeader.encode(data + 2U);
    writeRF_PDU(DataType::DATA_HEADER, data);

    ::memset(data + 2U, 0x00U, DMR_FRAME_LENGTH_BYTES);

    // decode the BPTC (196,96) FEC
    uint8_t payload[DMR_PDU_UNCONFIRMED_LENGTH_BYTES];
    ::memset(payload, 0x00U, DMR_PDU_UNCONFIRMED_LENGTH_BYTES);

    // encode the BPTC (196,96) FEC
    bptc.encode(payload, data + 2U);
    writeRF_PDU(DataType::RATE_12_DATA, data);
}
