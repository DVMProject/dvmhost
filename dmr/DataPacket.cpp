/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2015,2016,2017,2018 Jonathan Naylor, G4KLX
*   Copyright (C) 2017-2020 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; version 2 of the License.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*/
#include "Defines.h"
#include "dmr/DataPacket.h"
#include "dmr/acl/AccessControl.h"
#include "dmr/data/DataHeader.h"
#include "dmr/data/EMB.h"
#include "dmr/edac/Trellis.h"
#include "dmr/lc/ShortLC.h"
#include "dmr/lc/FullLC.h"
#include "dmr/lc/CSBK.h"
#include "dmr/SlotType.h"
#include "dmr/Sync.h"
#include "edac/BPTC19696.h"
#include "edac/CRC.h"
#include "Log.h"
#include "Utils.h"

using namespace dmr;

#include <cassert>
#include <ctime>
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------
// Don't process RF frames if the network isn't in a idle state.
#define CHECK_TRAFFIC_COLLISION(_DST_ID)                                                \
    if (m_slot->m_netState != RS_NET_IDLE && _DST_ID == m_slot->m_netLastDstId) {       \
        LogWarning(LOG_RF, "DMR Slot %u, Traffic collision detect, preempting new RF traffic to existing network traffic!", m_slot->m_slotNo); \
        return false;                                                                   \
    }

#define CHECK_TRAFFIC_COLLISION_DELLC(_DST_ID)                                          \
    if (m_slot->m_netState != RS_NET_IDLE && _DST_ID == m_slot->m_netLastDstId) {       \
        LogWarning(LOG_RF, "DMR Slot %u, Traffic collision detect, preempting new RF traffic to existing network traffic!", m_slot->m_slotNo); \
        delete lc;                                                                      \
        return false;                                                                   \
    }

#define CHECK_TG_HANG(_DST_ID)                                                          \
    if (m_slot->m_rfLastDstId != 0U) {                                                  \
        if (m_slot->m_rfLastDstId != _DST_ID && (m_slot->m_rfTGHang.isRunning() && !m_slot->m_rfTGHang.hasExpired())) { \
            return;                                                                     \
        }                                                                               \
    }

#define CHECK_TG_HANG_DELLC(_DST_ID)                                                    \
    if (m_slot->m_rfLastDstId != 0U) {                                                  \
        if (m_slot->m_rfLastDstId != _DST_ID && (m_slot->m_rfTGHang.isRunning() && !m_slot->m_rfTGHang.hasExpired())) { \
            delete lc;                                                                  \
            return;                                                                     \
        }                                                                               \
    }

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Process DMR data frame from the RF interface.
/// </summary>
/// <param name="data">Buffer containing data frame.</param>
/// <param name="len">Length of data frame.</param>
/// <returns></returns>
bool DataPacket::process(uint8_t* data, uint32_t len)
{
    assert(data != NULL);

    // Get the type from the packet metadata
    uint8_t dataType = data[1U] & 0x0FU;

    SlotType slotType;
    slotType.setColorCode(m_slot->m_colorCode);
    slotType.setDataType(dataType);

    if (dataType == DT_VOICE_LC_HEADER) {
        if (m_slot->m_rfState == RS_RF_AUDIO)
            return true;

        lc::FullLC fullLC;
        lc::LC* lc = fullLC.decode(data + 2U, DT_VOICE_LC_HEADER);
        if (lc == NULL)
            return false;

        uint32_t srcId = lc->getSrcId();
        uint32_t dstId = lc->getDstId();
        uint8_t flco = lc->getFLCO();

        CHECK_TRAFFIC_COLLISION_DELLC(dstId);

        // validate source RID
        if (!acl::AccessControl::validateSrcId(srcId)) {
            LogWarning(LOG_RF, "DMR Slot %u, DT_VOICE_LC_HEADER denial, RID rejection, srcId = %u", m_slot->m_slotNo, srcId);
            delete lc;
            return false;
        }

        // validate target TID, if the target is a talkgroup
        if (flco == FLCO_GROUP) {
            if (!acl::AccessControl::validateTGId(m_slot->m_slotNo, dstId)) {
                LogWarning(LOG_RF, "DMR Slot %u, DT_VOICE_LC_HEADER denial, TGID rejection, srcId = %u, dstId = %u", m_slot->m_slotNo, srcId, dstId);
                delete lc;
                return false;
            }
        }

        if (m_verbose) {
            LogMessage(LOG_RF, "DMR Slot %u, DT_VOICE_LC_HEADER, srcId = %u, dstId = %u, FLCO = $%02X, FID = $%02X, PF = %u", m_slot->m_slotNo, lc->getSrcId(), lc->getDstId(), lc->getFLCO(), lc->getFID(), lc->getPF());
        }

        uint8_t fid = lc->getFID();

        // NOTE: this is fiddly -- on Motorola a FID of 0x10 indicates a SU has transmitted with Enhanced Privacy enabled -- this might change
        // and is not exact science!
        bool encrypted = (fid & 0x10U) == 0x10U;

        m_rfLC = lc;

        // The standby LC data
        m_slot->m_voice->m_rfEmbeddedLC.setLC(*m_rfLC);
        m_slot->m_voice->m_rfEmbeddedData[0U].setLC(*m_rfLC);
        m_slot->m_voice->m_rfEmbeddedData[1U].setLC(*m_rfLC);

        // Regenerate the LC data
        fullLC.encode(*m_rfLC, data + 2U, DT_VOICE_LC_HEADER);

        // Regenerate the Slot Type
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        data[0U] = TAG_DATA;
        data[1U] = 0x00U;

        m_slot->m_rfTimeoutTimer.start();
        m_slot->m_rfTimeout = false;

        m_slot->m_rfFrames = 0U;
        m_slot->m_rfSeqNo = 0U;
        m_slot->m_rfBits = 1U;
        m_slot->m_rfErrs = 0U;

        m_slot->m_voice->m_rfEmbeddedReadN = 0U;
        m_slot->m_voice->m_rfEmbeddedWriteN = 1U;
        m_slot->m_voice->m_rfTalkerId = TALKER_ID_NONE;

        m_slot->m_minRSSI = m_slot->m_rssi;
        m_slot->m_maxRSSI = m_slot->m_rssi;
        m_slot->m_aveRSSI = m_slot->m_rssi;
        m_slot->m_rssiCount = 1U;

        if (m_slot->m_duplex) {
            m_slot->m_queue.clear();
            m_slot->m_modem->writeDMRAbort(m_slot->m_slotNo);

            for (uint32_t i = 0U; i < NO_HEADERS_DUPLEX; i++)
                m_slot->writeQueueRF(data);
        }

        m_slot->writeNetworkRF(data, DT_VOICE_LC_HEADER);

        m_slot->m_rfState = RS_RF_AUDIO;
        m_slot->m_rfLastDstId = dstId;

        if (m_slot->m_netState == RS_NET_IDLE) {
            m_slot->setShortLC(m_slot->m_slotNo, dstId, flco, true);
        }

        if (m_debug) {
            Utils::dump(2U, "!!! *TX DMR Frame - DT_VOICE_LC_HEADER", data + 2U, DMR_FRAME_LENGTH_BYTES);
        }

        ::ActivityLog("DMR", true, "Slot %u, received RF %svoice header from %u to %s%u", m_slot->m_slotNo, encrypted ? "encrypted " : "", srcId, flco == FLCO_GROUP ? "TG " : "", dstId);
        return true;
    }
    else if (dataType == DT_VOICE_PI_HEADER) {
        if (m_slot->m_rfState != RS_RF_AUDIO)
            return false;

        // Regenerate the Slot Type
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        // Regenerate the payload and the BPTC (196,96) FEC
        edac::BPTC19696 bptc;
        uint8_t payload[12U];
        bptc.decode(data + 2U, payload);
        bptc.encode(payload, data + 2U);

        data[0U] = TAG_DATA;
        data[1U] = 0x00U;

        if (m_slot->m_duplex)
            m_slot->writeQueueRF(data);

        m_slot->writeNetworkRF(data, DT_VOICE_PI_HEADER);

        if (m_debug) {
            Utils::dump(2U, "!!! *TX DMR Frame - DT_VOICE_PI_HEADER", data + 2U, DMR_FRAME_LENGTH_BYTES);
        }

        return true;
    }
    else if (dataType == DT_TERMINATOR_WITH_LC) {
        if (m_slot->m_rfState != RS_RF_AUDIO)
            return false;

        // Regenerate the LC data
        lc::FullLC fullLC;
        fullLC.encode(*m_rfLC, data + 2U, DT_TERMINATOR_WITH_LC);

        // Regenerate the Slot Type
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        if (!m_slot->m_rfTimeout) {
            data[0U] = TAG_EOT;
            data[1U] = 0x00U;

            m_slot->writeNetworkRF(data, DT_TERMINATOR_WITH_LC);

            if (m_slot->m_duplex) {
                for (uint32_t i = 0U; i < m_slot->m_hangCount; i++)
                    m_slot->writeQueueRF(data);
            }
        }

        if (m_debug) {
            Utils::dump(2U, "!!! *TX DMR Frame - DT_TERMINATOR_WITH_LC", data + 2U, DMR_FRAME_LENGTH_BYTES);
        }

        if (m_slot->m_rssi != 0U) {
            ::ActivityLog("DMR", true, "Slot %u, received RF end of voice transmission, %.1f seconds, BER: %.1f%%, RSSI: -%u/-%u/-%u dBm",
                m_slot->m_slotNo, float(m_slot->m_rfFrames) / 16.667F, float(m_slot->m_rfErrs * 100U) / float(m_slot->m_rfBits),
                m_slot->m_minRSSI, m_slot->m_maxRSSI, m_slot->m_aveRSSI / m_slot->m_rssiCount);
        }
        else {
            ::ActivityLog("DMR", true, "Slot %u, received RF end of voice transmission, %.1f seconds, BER: %.1f%%",
                m_slot->m_slotNo, float(m_slot->m_rfFrames) / 16.667F, float(m_slot->m_rfErrs * 100U) / float(m_slot->m_rfBits));
        }

        LogMessage(LOG_RF, "DMR Slot %u, total frames: %d, total bits: %d, errors: %d, BER: %.4f%%",
            m_slot->m_slotNo, m_slot->m_rfFrames, m_slot->m_rfBits, m_slot->m_rfErrs, float(m_slot->m_rfErrs * 100U) / float(m_slot->m_rfBits));

        if (m_slot->m_rfTimeout) {
            writeEndRF();
            return false;
        }
        else {
            writeEndRF();
            return true;
        }
    }
    else if (dataType == DT_CSBK) {
        // generate a new CSBK and check validity
        lc::CSBK csbk = lc::CSBK();
        csbk.setVerbose(m_dumpCSBKData);

        bool valid = csbk.decode(data + 2U);
        if (!valid)
            return false;

        uint8_t csbko = csbk.getCSBKO();
        if (csbko == CSBKO_BSDWNACT)
            return false;

        bool gi = csbk.getGI();
        uint32_t srcId = csbk.getSrcId();
        uint32_t dstId = csbk.getDstId();

        if (srcId != 0U || dstId != 0U) {
            CHECK_TRAFFIC_COLLISION(dstId);

            // validate the source RID
            if (!acl::AccessControl::validateSrcId(srcId)) {
                LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK denial, RID rejection, srcId = %u", m_slot->m_slotNo, srcId);
                return false;
            }

            // validate the target ID
            if (gi) {
                if (!acl::AccessControl::validateTGId(m_slot->m_slotNo, dstId)) {
                    LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK denial, TGID rejection, srcId = %u, dstId = %u", m_slot->m_slotNo, srcId, dstId);
                    return false;
                }
            }
        }

        // Regenerate the CSBK data
        csbk.encode(data + 2U);

        // Regenerate the Slot Type
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        m_slot->m_rfSeqNo = 0U;

        data[0U] = TAG_DATA;
        data[1U] = 0x00U;

        if (m_slot->m_duplex)
            m_slot->writeQueueRF(data);

        m_slot->writeNetworkRF(data, DT_CSBK, gi ? FLCO_GROUP : FLCO_PRIVATE, srcId, dstId);

        if (m_verbose) {
            switch (csbko) {
            case CSBKO_UU_V_REQ:
                if (m_verbose) {
                    LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_UU_V_REQ (Unit to Unit Voice Service Request), src = %u, dst = %s%u",
                        m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId);
                }
                break;
            case CSBKO_UU_ANS_RSP:
                if (m_verbose) {
                    LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_UU_ANS_RSP (Unit to Unit Voice Service Answer Response), src = %u, dst = %s%u",
                        m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId);
                }
                break;
            case CSBKO_NACK_RSP:
                if (m_verbose) {
                    LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_NACK_RSP (Negative Acknowledgment Response), src = %u, dst = %s%u",
                        m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId);
                }
                break;
            case CSBKO_CALL_ALRT:
                if (m_verbose) {
                    LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_CALL_ALRT (Call Alert), src = %u, dst = %s%u",
                        m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId);
                }

                ::ActivityLog("DMR", true, "Slot %u received call alert request from %u to %u", m_slot->m_slotNo, srcId, dstId);
                break;
            case CSBKO_ACK_RSP:
                if (m_verbose) {
                    LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_ACK_RSP (Acknowledge Response), src = %u, dst = %s%u",
                        m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId);
                }

                ::ActivityLog("DMR", true, "Slot %u received ack response from %u to %u", m_slot->m_slotNo, srcId, dstId);
                break;
            case CSBKO_EXT_FNCT:
                if (m_verbose) {
                    LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_EXT_FNCT (Extended Function), op = $%02X, arg = %u, tgt = %u",
                        m_slot->m_slotNo, csbk.getCBF(), dstId, srcId);
                }

                // generate activity log entry
                if (csbk.getCBF() == DMR_EXT_FNCT_CHECK) {
                    ::ActivityLog("DMR", true, "Slot %u received radio check request from %u to %u", m_slot->m_slotNo, dstId, srcId);
                }
                else if (csbk.getCBF() == DMR_EXT_FNCT_INHIBIT) {
                    ::ActivityLog("DMR", true, "Slot %u received radio inhibit request from %u to %u", m_slot->m_slotNo, dstId, srcId);
                }
                else if (csbk.getCBF() == DMR_EXT_FNCT_UNINHIBIT) {
                    ::ActivityLog("DMR", true, "Slot %u received radio uninhibit request from %u to %u", m_slot->m_slotNo, dstId, srcId);
                }
                else if (csbk.getCBF() == DMR_EXT_FNCT_CHECK_ACK) {
                    ::ActivityLog("DMR", true, "Slot %u received radio check response from %u to %u", m_slot->m_slotNo, dstId, srcId);
                }
                else if (csbk.getCBF() == DMR_EXT_FNCT_INHIBIT_ACK) {
                    ::ActivityLog("DMR", true, "Slot %u received radio inhibit response from %u to %u", m_slot->m_slotNo, dstId, srcId);
                }
                else if (csbk.getCBF() == DMR_EXT_FNCT_UNINHIBIT_ACK) {
                    ::ActivityLog("DMR", true, "Slot %u received radio uninhibit response from %u to %u", m_slot->m_slotNo, dstId, srcId);
                }
                break;
            case CSBKO_PRECCSBK:
                if (m_verbose) {
                    LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_PRECCSBK (%s Preamble CSBK), toFollow = %u, src = %u, dst = %s%u",
                        m_slot->m_slotNo, csbk.getDataContent() ? "Data" : "CSBK", csbk.getCBF(), srcId, gi ? "TG " : "", dstId);
                }
                break;
            default:
                LogWarning(LOG_RF, "DMR Slot %u, DT_CSBK, unhandled CSBK, csbko = $%02X, fid = $%02X", m_slot->m_slotNo, csbko, csbk.getFID());
                break;
            }
        }

        if (m_debug) {
            Utils::dump(2U, "!!! *TX DMR Frame - DT_CSBK", data + 2U, DMR_FRAME_LENGTH_BYTES);
        }

        return true;
    }
    else if (dataType == DT_DATA_HEADER) {
        if (m_slot->m_rfState == RS_RF_DATA)
            return true;

        data::DataHeader dataHeader;
        bool valid = dataHeader.decode(data + 2U);
        if (!valid)
            return false;

        bool gi = dataHeader.getGI();
        uint32_t srcId = dataHeader.getSrcId();
        uint32_t dstId = dataHeader.getDstId();

        CHECK_TRAFFIC_COLLISION(dstId);

        // validate the source RID
        if (!acl::AccessControl::validateSrcId(srcId)) {
            LogWarning(LOG_RF, "DMR Slot %u, DT_DATA_HEADER denial, RID rejection, srcId = %u", m_slot->m_slotNo, srcId);
            return false;
        }

        // validate the target ID
        if (gi) {
            if (!acl::AccessControl::validateTGId(m_slot->m_slotNo, dstId)) {
                LogWarning(LOG_RF, "DMR Slot %u, DT_DATA_HEADER denial, TGID rejection, srcId = %u, dstId = %u", m_slot->m_slotNo, srcId, dstId);
                return false;
            }
        }

        m_slot->m_rfFrames = dataHeader.getBlocks();
        m_slot->m_rfSeqNo = 0U;
        m_rfLC = new lc::LC(gi ? FLCO_GROUP : FLCO_PRIVATE, srcId, dstId);

        // Regenerate the data header
        dataHeader.encode(data + 2U);

        // Regenerate the Slot Type
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        data[0U] = m_slot->m_rfFrames == 0U ? TAG_EOT : TAG_DATA;
        data[1U] = 0x00U;

        if (m_slot->m_duplex && m_repeatDataPacket)
            m_slot->writeQueueRF(data);

        m_slot->writeNetworkRF(data, DT_DATA_HEADER);

        m_slot->m_rfState = RS_RF_DATA;
        m_slot->m_rfLastDstId = dstId;

        if (m_slot->m_netState == RS_NET_IDLE) {
            m_slot->setShortLC(m_slot->m_slotNo, dstId, gi ? FLCO_GROUP : FLCO_PRIVATE, false);
        }

        if (m_debug) {
            Utils::dump(2U, "!!! *TX DMR Frame - DT_DATA_HEADER", data + 2U, DMR_FRAME_LENGTH_BYTES);
        }

        ::ActivityLog("DMR", true, "Slot %u, received RF data header from %u to %s%u, %u blocks", m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId, m_slot->m_rfFrames);

        ::memset(m_pduUserData, 0x00U, DMR_MAX_PDU_COUNT * DMR_MAX_PDU_LENGTH + 2U);
        m_pduDataOffset = 0U;

        if (m_slot->m_rfFrames == 0U) {
            ::ActivityLog("DMR", true, "Slot %u, ended RF data transmission", m_slot->m_slotNo);
            writeEndRF();
        }

        return true;
    }
    else if (dataType == DT_RATE_12_DATA || dataType == DT_RATE_34_DATA || dataType == DT_RATE_1_DATA) {
        if (m_slot->m_rfState != RS_RF_DATA || m_slot->m_rfFrames == 0U)
            return false;

        edac::BPTC19696 bptc;
        edac::Trellis trellis;

        // decode the rate 1/2 payload
        if (dataType == DT_RATE_12_DATA) {
            // decode the BPTC (196,96) FEC
            uint8_t payload[12U];
            bptc.decode(data + 2U, payload);

            // store payload
            ::memcpy(m_pduUserData, payload, 12U);
            m_pduDataOffset += 12U;

            // encode the BPTC (196,96) FEC
            bptc.encode(payload, data + 2U);
        }
        else if (dataType == DT_RATE_34_DATA) {
            // decode the Trellis 3/4 rate FEC
            uint8_t payload[18U];
            bool ret = trellis.decode(data + 2U, payload);
            if (ret) {
                // store payload
                ::memcpy(m_pduUserData, payload, 18U);

                // encode the Trellis 3/4 rate FEC
                trellis.encode(payload, data + 2U);
            }
            else {
                LogWarning(LOG_RF, "DMR Slot %u, DT_RATE_34_DATA, unfixable RF rate 3/4 data", m_slot->m_slotNo);
                Utils::dump(1U, "Data", data + 2U, DMR_FRAME_LENGTH_BYTES);
            }

            m_pduDataOffset += 18U;
        }

        m_slot->m_rfFrames--;

        data[0U] = m_slot->m_rfFrames == 0U ? TAG_EOT : TAG_DATA;
        data[1U] = 0x00U;

        // regenerate the Slot Type
        slotType.encode(data + 2U);

        // convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        m_slot->writeNetworkRF(data, dataType);

        if (m_slot->m_duplex && m_repeatDataPacket) {
            m_slot->writeQueueRF(data);
        }

        if (m_slot->m_rfFrames == 0U) {
            if (m_dumpDataPacket) {
                Utils::dump(1U, "PDU Packet", m_pduUserData, m_pduDataOffset);
            }

            LogMessage(LOG_RF, "DMR Slot %u, DT_RATE_12/34_DATA, ended data transmission", m_slot->m_slotNo);
            writeEndRF();
        }

        if (m_debug) {
            Utils::dump(2U, "!!! *TX DMR Frame - DT_RATE_12/34_DATA", data + 2U, DMR_FRAME_LENGTH_BYTES);
        }

        return true;
    }

    return false;
}

/// <summary>
/// Process a data frame from the network.
/// </summary>
/// <param name="dmrData"></param>
void DataPacket::processNetwork(const data::Data& dmrData)
{
    uint8_t dataType = dmrData.getDataType();

    uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];
    dmrData.getData(data + 2U);

    if (dataType == DT_VOICE_LC_HEADER) {
        if (m_slot->m_netState == RS_NET_AUDIO)
            return;

        lc::FullLC fullLC;
        lc::LC * lc = fullLC.decode(data + 2U, DT_VOICE_LC_HEADER);
        if (lc == NULL) {
            LogWarning(LOG_NET, "DMR Slot %u, DT_VOICE_LC_HEADER, bad LC received from the network, replacing", m_slot->m_slotNo);
            lc = new lc::LC(dmrData.getFLCO(), dmrData.getSrcId(), dmrData.getDstId());
        }

        uint32_t srcId = lc->getSrcId();
        uint32_t dstId = lc->getDstId();
        uint8_t flco = lc->getFLCO();

        CHECK_TG_HANG_DELLC(dstId);

        if (dstId != dmrData.getDstId() || srcId != dmrData.getSrcId() || flco != dmrData.getFLCO())
            LogWarning(LOG_NET, "DMR Slot %u, DT_VOICE_LC_HEADER, header doesn't match the DMR RF header: %u->%s%u %u->%s%u", m_slot->m_slotNo,
                dmrData.getSrcId(), dmrData.getFLCO() == FLCO_GROUP ? "TG" : "", dmrData.getDstId(),
                srcId, flco == FLCO_GROUP ? "TG" : "", dstId);

        if (m_verbose) {
            LogMessage(LOG_NET, "DMR Slot %u, DT_VOICE_LC_HEADER, srcId = %u, dstId = %u, FLCO = $%02X, FID = $%02X, PF = %u", m_slot->m_slotNo, lc->getSrcId(), lc->getDstId(), lc->getFLCO(), lc->getFID(), lc->getPF());
        }

        m_netLC = lc;

        // The standby LC data
        m_slot->m_voice->m_netEmbeddedLC.setLC(*m_netLC);
        m_slot->m_voice->m_netEmbeddedData[0U].setLC(*m_netLC);
        m_slot->m_voice->m_netEmbeddedData[1U].setLC(*m_netLC);

        // Regenerate the LC data
        fullLC.encode(*m_netLC, data + 2U, DT_VOICE_LC_HEADER);

        // Regenerate the Slot Type
        SlotType slotType;
        slotType.setColorCode(m_slot->m_colorCode);
        slotType.setDataType(DT_VOICE_LC_HEADER);
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        data[0U] = TAG_DATA;
        data[1U] = 0x00U;

        m_slot->m_voice->m_lastFrameValid = false;

        m_slot->m_netTimeoutTimer.start();
        m_slot->m_netTimeout = false;

        m_slot->m_netFrames = 0U;
        m_slot->m_netLost = 0U;
        m_slot->m_netBits = 1U;
        m_slot->m_netErrs = 0U;

        m_slot->m_voice->m_netEmbeddedReadN = 0U;
        m_slot->m_voice->m_netEmbeddedWriteN = 1U;
        m_slot->m_voice->m_netTalkerId = TALKER_ID_NONE;

        if (m_slot->m_duplex) {
            m_slot->m_queue.clear();
            m_slot->m_modem->writeDMRAbort(m_slot->m_slotNo);
        }

        for (uint32_t i = 0U; i < m_slot->m_jitterSlots; i++)
            m_slot->writeQueueNet(m_slot->m_idle);

        if (m_slot->m_duplex) {
            for (uint32_t i = 0U; i < NO_HEADERS_DUPLEX; i++)
                m_slot->writeQueueNet(data);
        }
        else {
            for (uint32_t i = 0U; i < NO_HEADERS_SIMPLEX; i++)
                m_slot->writeQueueNet(data);
        }

        m_slot->m_netState = RS_NET_AUDIO;
        m_slot->m_netLastDstId = dstId;

        m_slot->setShortLC(m_slot->m_slotNo, dstId, flco, true);

        if (m_debug) {
            Utils::dump(2U, "!!! *TX DMR Network Frame - DT_VOICE_LC_HEADER", data + 2U, DMR_FRAME_LENGTH_BYTES);
        }

        ::ActivityLog("DMR", false, "Slot %u, received network voice header from %u to %s%u", m_slot->m_slotNo, srcId, flco == FLCO_GROUP ? "TG " : "", dstId);
    }
    else if (dataType == DT_VOICE_PI_HEADER) {
        if (m_slot->m_netState != RS_NET_AUDIO) {
            lc::LC* lc = new lc::LC(dmrData.getFLCO(), dmrData.getSrcId(), dmrData.getDstId());

            uint32_t srcId = lc->getSrcId();
            uint32_t dstId = lc->getDstId();

            CHECK_TG_HANG_DELLC(dstId);

            m_netLC = lc;

            m_slot->m_voice->m_lastFrameValid = false;

            m_slot->m_netTimeoutTimer.start();
            m_slot->m_netTimeout = false;

            if (m_slot->m_duplex) {
                m_slot->m_queue.clear();
                m_slot->m_modem->writeDMRAbort(m_slot->m_slotNo);
            }

            for (uint32_t i = 0U; i < m_slot->m_jitterSlots; i++)
                m_slot->writeQueueNet(m_slot->m_idle);

            // Create a dummy start frame
            uint8_t start[DMR_FRAME_LENGTH_BYTES + 2U];

            Sync::addDMRDataSync(start + 2U, m_slot->m_duplex);

            lc::FullLC fullLC;
            fullLC.encode(*m_netLC, start + 2U, DT_VOICE_LC_HEADER);

            SlotType slotType;
            slotType.setColorCode(m_slot->m_colorCode);
            slotType.setDataType(DT_VOICE_LC_HEADER);
            slotType.encode(start + 2U);

            start[0U] = TAG_DATA;
            start[1U] = 0x00U;

            if (m_slot->m_duplex) {
                for (uint32_t i = 0U; i < NO_HEADERS_DUPLEX; i++)
                    m_slot->writeQueueRF(start);
            }
            else {
                for (uint32_t i = 0U; i < NO_HEADERS_SIMPLEX; i++)
                    m_slot->writeQueueRF(start);
            }

            m_slot->m_netFrames = 0U;
            m_slot->m_netLost = 0U;
            m_slot->m_netBits = 1U;
            m_slot->m_netErrs = 0U;

            m_slot->m_netState = RS_NET_AUDIO;
            m_slot->m_netLastDstId = dstId;

            m_slot->setShortLC(m_slot->m_slotNo, dstId, m_netLC->getFLCO(), true);

            ::ActivityLog("DMR", false, "Slot %u, received network late entry from %u to %s%u",
                m_slot->m_slotNo, srcId, m_netLC->getFLCO() == FLCO_GROUP ? "TG " : "", dstId);
        }

        // Regenerate the Slot Type
        SlotType slotType;
        slotType.setColorCode(m_slot->m_colorCode);
        slotType.setDataType(DT_VOICE_PI_HEADER);
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        // Regenerate the payload and the BPTC (196,96) FEC
        edac::BPTC19696 bptc;
        uint8_t payload[12U];
        bptc.decode(data + 2U, payload);
        bptc.encode(payload, data + 2U);

        data[0U] = TAG_DATA;
        data[1U] = 0x00U;

        m_slot->writeQueueNet(data);

        if (m_debug) {
            Utils::dump(2U, "!!! *TX DMR Network Frame - DT_VOICE_PI_HEADER", data + 2U, DMR_FRAME_LENGTH_BYTES);
        }
    }
    else if (dataType == DT_TERMINATOR_WITH_LC) {
        if (m_slot->m_netState != RS_NET_AUDIO)
            return;

        // Regenerate the LC data
        lc::FullLC fullLC;
        fullLC.encode(*m_netLC, data + 2U, DT_TERMINATOR_WITH_LC);

        // Regenerate the Slot Type
        SlotType slotType;
        slotType.setColorCode(m_slot->m_colorCode);
        slotType.setDataType(DT_TERMINATOR_WITH_LC);
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        if (!m_slot->m_netTimeout) {
            data[0U] = TAG_EOT;
            data[1U] = 0x00U;

            if (m_slot->m_duplex) {
                for (uint32_t i = 0U; i < m_slot->m_hangCount; i++)
                    m_slot->writeQueueNet(data);
            }
            else {
                for (uint32_t i = 0U; i < 3U; i++)
                    m_slot->writeQueueNet(data);
            }
        }

        if (m_debug) {
            Utils::dump(2U, "!!! *TX DMR Network Frame - DT_TERMINATOR_WITH_LC", data + 2U, DMR_FRAME_LENGTH_BYTES);
        }

        // We've received the voice header and terminator haven't we?
        m_slot->m_netFrames += 2U;
        ::ActivityLog("DMR", false, "Slot %u, received network end of voice transmission, %.1f seconds, %u%% packet loss, BER: %.1f%%",
            m_slot->m_slotNo, float(m_slot->m_netFrames) / 16.667F, (m_slot->m_netLost * 100U) / m_slot->m_netFrames, float(m_slot->m_netErrs * 100U) / float(m_slot->m_netBits));

        writeEndNet();
    }
    else if (dataType == DT_CSBK) {
        lc::CSBK csbk = lc::CSBK();
        csbk.setVerbose(m_dumpCSBKData);

        bool valid = csbk.decode(data + 2U);
        if (!valid) {
            LogError(LOG_NET, "DMR Slot %u, DT_CSBK, unable to decode the network CSBK", m_slot->m_slotNo);
            return;
        }

        uint8_t csbko = csbk.getCSBKO();
        if (csbko == CSBKO_BSDWNACT)
            return;

        bool gi = csbk.getGI();
        uint32_t srcId = csbk.getSrcId();
        uint32_t dstId = csbk.getDstId();

        CHECK_TG_HANG(dstId);

        // Regenerate the CSBK data
        csbk.encode(data + 2U);

        // Regenerate the Slot Type
        SlotType slotType;
        slotType.decode(data + 2U);
        slotType.setColorCode(m_slot->m_colorCode);
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        data[0U] = TAG_DATA;
        data[1U] = 0x00U;

        if (csbko == CSBKO_PRECCSBK && csbk.getDataContent()) {
            uint32_t cbf = NO_PREAMBLE_CSBK + csbk.getCBF() - 1U;
            for (uint32_t i = 0U; i < NO_PREAMBLE_CSBK; i++, cbf--) {
                // Change blocks to follow
                csbk.setCBF(cbf);

                // Regenerate the CSBK data
                csbk.encode(data + 2U);

                // Regenerate the Slot Type
                SlotType slotType;
                slotType.decode(data + 2U);
                slotType.setColorCode(m_slot->m_colorCode);
                slotType.encode(data + 2U);

                // Convert the Data Sync to be from the BS or MS as needed
                Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

                m_slot->writeQueueNet(data);
            }
        }
        else
            m_slot->writeQueueNet(data);

        if (m_verbose) {
            switch (csbko) {
            case CSBKO_UU_V_REQ:
                if (m_verbose) {
                    LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, CSBKO_UU_V_REQ (Unit to Unit Voice Service Request), src = %u, dst = %s%u",
                        m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId);
                }
                break;
            case CSBKO_UU_ANS_RSP:
                if (m_verbose) {
                    LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, CSBKO_UU_ANS_RSP (Unit to Unit Voice Service Answer Response), src = %u, dst = %s%u",
                        m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId);
                }
                break;
            case CSBKO_NACK_RSP:
                if (m_verbose) {
                    LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, CSBKO_NACK_RSP (Negative Acknowledgment Response), src = %u, dst = %s%u",
                        m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId);
                }
                break;
            case CSBKO_CALL_ALRT:
                if (m_verbose) {
                    LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, CSBKO_CALL_ALRT (Call Alert), src = %u, dst = %s%u",
                        m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId);
                }

                ::ActivityLog("DMR", false, "Slot %u received call alert request from %u to %u", m_slot->m_slotNo, srcId, dstId);
                break;
            case CSBKO_ACK_RSP:
                if (m_verbose) {
                    LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, CSBKO_ACK_RSP (Acknowledge Response), src = %u, dst = %s%u",
                        m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId);
                }

                ::ActivityLog("DMR", false, "Slot %u received ack response from %u to %u", m_slot->m_slotNo, srcId, dstId);
                break;
            case CSBKO_EXT_FNCT:
                if (m_verbose) {
                    LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, CSBKO_EXT_FNCT (Extended Function), op = $%02X, arg = %u, tgt = %u",
                        m_slot->m_slotNo, csbk.getCBF(), dstId, srcId);
                }

                // generate activity log entry
                if (csbk.getCBF() == DMR_EXT_FNCT_CHECK) {
                    ::ActivityLog("DMR", false, "Slot %u received radio check request from %u to %u", m_slot->m_slotNo, dstId, srcId);
                }
                else if (csbk.getCBF() == DMR_EXT_FNCT_INHIBIT) {
                    ::ActivityLog("DMR", false, "Slot %u received radio inhibit request from %u to %u", m_slot->m_slotNo, dstId, srcId);
                }
                else if (csbk.getCBF() == DMR_EXT_FNCT_UNINHIBIT) {
                    ::ActivityLog("DMR", false, "Slot %u received radio uninhibit request from %u to %u", m_slot->m_slotNo, dstId, srcId);
                }
                else if (csbk.getCBF() == DMR_EXT_FNCT_CHECK_ACK) {
                    ::ActivityLog("DMR", false, "Slot %u received radio check response from %u to %u", m_slot->m_slotNo, dstId, srcId);
                }
                else if (csbk.getCBF() == DMR_EXT_FNCT_INHIBIT_ACK) {
                    ::ActivityLog("DMR", false, "Slot %u received radio inhibit response from %u to %u", m_slot->m_slotNo, dstId, srcId);
                }
                else if (csbk.getCBF() == DMR_EXT_FNCT_UNINHIBIT_ACK) {
                    ::ActivityLog("DMR", false, "Slot %u received radio uninhibit response from %u to %u", m_slot->m_slotNo, dstId, srcId);
                }
                break;
            case CSBKO_PRECCSBK:
                if (m_verbose) {
                    LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, CSBKO_PRECCSBK (%s Preamble CSBK), toFollow = %u, src = %u, dst = %s%u",
                        m_slot->m_slotNo, csbk.getDataContent() ? "Data" : "CSBK", csbk.getCBF(), srcId, gi ? "TG " : "", dstId);
                }
                break;
            default:
                LogWarning(LOG_NET, "DMR Slot %u, DT_CSBK, unhandled network CSBK, csbko = $%02X, fid = $%02X", m_slot->m_slotNo, csbko, csbk.getFID());
                break;
            }
        }
    }
    else if (dataType == DT_DATA_HEADER) {
        if (m_slot->m_netState == RS_NET_DATA)
            return;

        data::DataHeader dataHeader;
        bool valid = dataHeader.decode(data + 2U);
        if (!valid) {
            LogError(LOG_NET, "DMR Slot %u, DT_DATA_HEADER, unable to decode the network data header", m_slot->m_slotNo);
            return;
        }

        bool gi = dataHeader.getGI();
        uint32_t srcId = dataHeader.getSrcId();
        uint32_t dstId = dataHeader.getDstId();

        CHECK_TG_HANG(dstId);

        m_slot->m_netFrames = dataHeader.getBlocks();

        // Regenerate the data header
        dataHeader.encode(data + 2U);

        // Regenerate the Slot Type
        SlotType slotType;
        slotType.setColorCode(m_slot->m_colorCode);
        slotType.setDataType(DT_DATA_HEADER);
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        data[0U] = m_slot->m_netFrames == 0U ? TAG_EOT : TAG_DATA;
        data[1U] = 0x00U;

        // Put a small delay into starting transmission
        m_slot->writeQueueNet(m_slot->m_idle);
        m_slot->writeQueueNet(m_slot->m_idle);

        m_slot->writeQueueNet(data);

        m_slot->m_netState = RS_NET_DATA;
        m_slot->m_netLastDstId = dstId;

        m_slot->setShortLC(m_slot->m_slotNo, dstId, gi ? FLCO_GROUP : FLCO_PRIVATE, false);

        if (m_debug) {
            Utils::dump(2U, "!!! *TX DMR Network Frame - DT_DATA_HEADER", data + 2U, DMR_FRAME_LENGTH_BYTES);
        }

        ::ActivityLog("DMR", false, "Slot %u, received network data header from %u to %s%u, %u blocks",
            m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId, m_slot->m_netFrames);

        ::memset(m_pduUserData, 0x00U, DMR_MAX_PDU_COUNT * DMR_MAX_PDU_LENGTH + 2U);
        m_pduDataOffset = 0U;

        if (m_slot->m_netFrames == 0U) {
            ::ActivityLog("DMR", false, "Slot %u, ended network data transmission", m_slot->m_slotNo);
            writeEndNet();
        }
    }
    else if (dataType == DT_RATE_12_DATA || dataType == DT_RATE_34_DATA || dataType == DT_RATE_1_DATA) {
        if (m_slot->m_netState != RS_NET_DATA || m_slot->m_netFrames == 0U) {
            writeEndNet();
            return;
        }

        // regenerate the rate 1/2 payload
        if (dataType == DT_RATE_12_DATA) {
            // decode the BPTC (196,96) FEC
            edac::BPTC19696 bptc;
            uint8_t payload[12U];
            bptc.decode(data + 2U, payload);

            // store payload
            ::memcpy(m_pduUserData, payload, 12U);
            m_pduDataOffset += 12U;

            // encode the BPTC (196,96) FEC
            bptc.encode(payload, data + 2U);
        }
        else if (dataType == DT_RATE_34_DATA) {
            // decode the Trellis 3/4 rate FEC
            edac::Trellis trellis;
            uint8_t payload[18U];
            bool ret = trellis.decode(data + 2U, payload);
            if (ret) {
                // store payload
                ::memcpy(m_pduUserData, payload, 18U);

                // encode the Trellis 3/4 rate FEC
                trellis.encode(payload, data + 2U);
            }
            else {
                LogWarning(LOG_NET, "DMR Slot %u, DT_RATE_34_DATA, unfixable network rate 3/4 data", m_slot->m_slotNo);
                Utils::dump(1U, "Data", data + 2U, DMR_FRAME_LENGTH_BYTES);
            }

            m_pduDataOffset += 18U;
        }

        m_slot->m_netFrames--;

        if (m_repeatDataPacket) {
            // regenerate the Slot Type
            SlotType slotType;
            slotType.decode(data + 2U);
            slotType.setColorCode(m_slot->m_colorCode);
            slotType.encode(data + 2U);

            // convert the Data Sync to be from the BS or MS as needed
            Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

            data[0U] = m_slot->m_netFrames == 0U ? TAG_EOT : TAG_DATA;
            data[1U] = 0x00U;

            m_slot->writeQueueNet(data);

            if (m_debug) {
                Utils::dump(2U, "!!! *TX DMR Network Frame - DT_RATE_12/34_DATA", data + 2U, DMR_FRAME_LENGTH_BYTES);
            }
        }

        if (m_slot->m_netFrames == 0U) {
            if (m_dumpDataPacket) {
                Utils::dump(1U, "PDU Packet", m_pduUserData, m_pduDataOffset);
            }

            LogMessage(LOG_NET, "DMR Slot %u, DT_RATE_12/34_DATA, ended data transmission", m_slot->m_slotNo);
            writeEndNet();
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
/// <summary>
/// Initializes a new instance of the DataPacket class.
/// </summary>
/// <param name="slot">DMR slot.</param>
/// <param name="network">Instance of the BaseNetwork class.</param>
/// <param name="dumpDataPacket"></param>
/// <param name="repeatDataPacket"></param>
/// <param name="dumpCSBKData"></param>
/// <param name="debug">Flag indicating whether DMR debug is enabled.</param>
/// <param name="verbose">Flag indicating whether DMR verbose logging is enabled.</param>
DataPacket::DataPacket(Slot* slot, network::BaseNetwork* network, bool dumpDataPacket, bool repeatDataPacket, bool dumpCSBKData, bool debug, bool verbose) :
    m_slot(slot),
    m_rfLC(NULL),
    m_netLC(NULL),
    m_pduUserData(NULL),
    m_pduDataOffset(0U),
    m_dumpDataPacket(dumpDataPacket),
    m_repeatDataPacket(repeatDataPacket),
    m_dumpCSBKData(dumpCSBKData),
    m_verbose(verbose),
    m_debug(debug)
{
    m_pduUserData = new uint8_t[DMR_MAX_PDU_COUNT * DMR_MAX_PDU_LENGTH + 2U];
    ::memset(m_pduUserData, 0x00U, DMR_MAX_PDU_COUNT * DMR_MAX_PDU_LENGTH + 2U);
}

/// <summary>
/// Finalizes a instance of the DataPacket class.
/// </summary>
DataPacket::~DataPacket()
{
    delete[] m_pduUserData;
}

/// <summary>
/// Helper to write RF end of frame data.
/// </summary>
/// <param name="writeEnd"></param>
void DataPacket::writeEndRF(bool writeEnd)
{
    m_slot->m_rfState = RS_RF_LISTENING;

    if (m_slot->m_netState == RS_NET_IDLE) {
        m_slot->setShortLC(m_slot->m_slotNo, 0U);
    }

    if (writeEnd) {
        if (m_slot->m_netState == RS_NET_IDLE && m_slot->m_duplex && !m_slot->m_rfTimeout) {
            // Create a dummy start end frame
            uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];

            Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

            lc::FullLC fullLC;
            fullLC.encode(*m_rfLC, data + 2U, DT_TERMINATOR_WITH_LC);

            SlotType slotType;
            slotType.setColorCode(m_slot->m_colorCode);
            slotType.setDataType(DT_TERMINATOR_WITH_LC);
            slotType.encode(data + 2U);

            data[0U] = TAG_EOT;
            data[1U] = 0x00U;

            for (uint32_t i = 0U; i < m_slot->m_hangCount; i++)
                m_slot->writeQueueRF(data);
        }
    }

    m_pduDataOffset = 0U;

    if (m_slot->m_network != NULL)
        m_slot->m_network->resetDMR(m_slot->m_slotNo);

    m_slot->m_rfTimeoutTimer.stop();
    m_slot->m_rfTimeout = false;

    m_slot->m_rfFrames = 0U;
    m_slot->m_rfErrs = 0U;
    m_slot->m_rfBits = 1U;

    delete m_rfLC;
    m_rfLC = NULL;
}

/// <summary>
/// Helper to write network end of frame data.
/// </summary>
/// <param name="writeEnd"></param>
void DataPacket::writeEndNet(bool writeEnd)
{
    m_slot->m_netState = RS_NET_IDLE;

    m_slot->setShortLC(m_slot->m_slotNo, 0U);

    m_slot->m_voice->m_lastFrameValid = false;

    if (writeEnd && !m_slot->m_netTimeout) {
        // Create a dummy start end frame
        uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];

        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        lc::FullLC fullLC;
        fullLC.encode(*m_netLC, data + 2U, DT_TERMINATOR_WITH_LC);

        SlotType slotType;
        slotType.setColorCode(m_slot->m_colorCode);
        slotType.setDataType(DT_TERMINATOR_WITH_LC);
        slotType.encode(data + 2U);

        data[0U] = TAG_EOT;
        data[1U] = 0x00U;

        if (m_slot->m_duplex) {
            for (uint32_t i = 0U; i < m_slot->m_hangCount; i++)
                m_slot->writeQueueNet(data);
        }
        else {
            for (uint32_t i = 0U; i < 3U; i++)
                m_slot->writeQueueNet(data);
        }
    }

    m_pduDataOffset = 0U;

    if (m_slot->m_network != NULL)
        m_slot->m_network->resetDMR(m_slot->m_slotNo);

    m_slot->m_networkWatchdog.stop();
    m_slot->m_netTimeoutTimer.stop();
    m_slot->m_packetTimer.stop();
    m_slot->m_netTimeout = false;

    m_slot->m_netFrames = 0U;
    m_slot->m_netLost = 0U;

    m_slot->m_netErrs = 0U;
    m_slot->m_netBits = 1U;

    delete m_netLC;
    m_netLC = NULL;
}
