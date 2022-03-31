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
*   Copyright (C) 2017-2019 by Bryan Biedenkapp N2PLL
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
#include "dmr/VoicePacket.h"
#include "dmr/acl/AccessControl.h"
#include "dmr/data/DataHeader.h"
#include "dmr/data/EMB.h"
#include "dmr/edac/Trellis.h"
#include "dmr/lc/CSBK.h"
#include "dmr/lc/ShortLC.h"
#include "dmr/lc/FullLC.h"
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
#define CHECK_TRAFFIC_COLLISION_DELLC(_DST_ID)                                          \
    if (m_slot->m_netState != RS_NET_IDLE && _DST_ID == m_slot->m_netLastDstId) {       \
        LogWarning(LOG_RF, "DMR Slot %u, Traffic collision detect, preempting new RF traffic to existing network traffic!", m_slot->m_slotNo); \
        delete lc;                                                                      \
        return false;                                                                   \
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
/// Process DMR voice frame from the RF interface.
/// </summary>
/// <param name="data">Buffer containing data frame.</param>
/// <param name="len">Length of data frame.</param>
/// <returns></returns>
bool VoicePacket::process(uint8_t* data, uint32_t len)
{
    assert(data != NULL);

    bool dataSync = (data[1U] & DMR_SYNC_DATA) == DMR_SYNC_DATA;
    bool voiceSync = (data[1U] & DMR_SYNC_VOICE) == DMR_SYNC_VOICE;

    if (dataSync) {
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
                if (m_slot->m_data->m_lastRejectId == 0U || m_slot->m_data->m_lastRejectId == srcId) {
                    LogWarning(LOG_RF, "DMR Slot %u, DT_VOICE_LC_HEADER denial, RID rejection, srcId = %u", m_slot->m_slotNo, srcId);
                    ::ActivityLog("DMR", true, "Slot %u RF voice rejection from %u to %s%u ", m_slot->m_slotNo, srcId, flco == FLCO_GROUP ? "TG " : "", dstId);
                }

                m_slot->m_rfLastDstId = 0U;
                m_slot->m_rfTGHang.stop();

                delete lc;
                m_slot->m_rfState = RS_RF_REJECTED;
                return false;
            }

            // validate target TID, if the target is a talkgroup
            if (flco == FLCO_GROUP) {
                if (!acl::AccessControl::validateTGId(m_slot->m_slotNo, dstId)) {
                    if (m_slot->m_data->m_lastRejectId == 0U || m_slot->m_data->m_lastRejectId == dstId) {
                        LogWarning(LOG_RF, "DMR Slot %u, DT_VOICE_LC_HEADER denial, TGID rejection, srcId = %u, dstId = %u", m_slot->m_slotNo, srcId, dstId);
                        ::ActivityLog("DMR", true, "Slot %u RF voice rejection from %u to TG %u ", m_slot->m_slotNo, srcId, dstId);
                    }

                    m_slot->m_rfLastDstId = 0U;
                    m_slot->m_rfTGHang.stop();

                    delete lc;
                    m_slot->m_rfState = RS_RF_REJECTED;
                    return false;
                }
            }

            m_slot->m_data->m_lastRejectId = 0U;

            uint8_t fid = lc->getFID();

            // NOTE: this is fiddly -- on Motorola a FID of 0x10 indicates a SU has transmitted with Enhanced Privacy enabled -- this might change
            // and is not exact science!
            bool encrypted = (fid & 0x10U) == 0x10U;

            m_slot->m_rfLC = lc;

            // The standby LC data
            m_slot->m_voice->m_rfEmbeddedLC.setLC(*m_slot->m_rfLC);
            m_slot->m_voice->m_rfEmbeddedData[0U].setLC(*m_slot->m_rfLC);
            m_slot->m_voice->m_rfEmbeddedData[1U].setLC(*m_slot->m_rfLC);

            // Regenerate the LC data
            fullLC.encode(*m_slot->m_rfLC, data + 2U, DT_VOICE_LC_HEADER);

            // Regenerate the Slot Type
            slotType.encode(data + 2U);

            // Convert the Data Sync to be from the BS or MS as needed
            Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

            data[0U] = modem::TAG_DATA;
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

            if (m_verbose) {
                LogMessage(LOG_RF, DMR_DT_VOICE_LC_HEADER ", slot = %u, srcId = %u, dstId = %u, FLCO = $%02X, FID = $%02X, PF = %u", m_slot->m_slotNo, lc->getSrcId(), lc->getDstId(), lc->getFLCO(), lc->getFID(), lc->getPF());
            }

            ::ActivityLog("DMR", true, "Slot %u RF %svoice header from %u to %s%u", m_slot->m_slotNo, encrypted ? "encrypted " : "", srcId, flco == FLCO_GROUP ? "TG " : "", dstId);
            return true;
        }
        else if (dataType == DT_VOICE_PI_HEADER) {
            if (m_slot->m_rfState != RS_RF_AUDIO)
                return false;

            lc::FullLC fullLC;
            lc::PrivacyLC* lc = fullLC.decodePI(data + 2U);
            if (lc == NULL) {
                LogWarning(LOG_RF, "DMR Slot %u, DT_VOICE_PI_HEADER, bad LC received, replacing", m_slot->m_slotNo);
                lc = new lc::PrivacyLC();
                lc->setDstId(m_slot->m_rfLC->getDstId());
            }

            m_slot->m_rfPrivacyLC = lc;

            // Regenerate the LC data
            fullLC.encodePI(*m_slot->m_rfPrivacyLC, data + 2U);

            // Regenerate the Slot Type
            slotType.encode(data + 2U);

            // Convert the Data Sync to be from the BS or MS as needed
            Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

            data[0U] = modem::TAG_DATA;
            data[1U] = 0x00U;

            if (m_slot->m_duplex)
                m_slot->writeQueueRF(data);

            m_slot->writeNetworkRF(data, DT_VOICE_PI_HEADER);

            if (m_verbose) {
                LogMessage(LOG_RF, DMR_DT_VOICE_PI_HEADER ", slot = %u, algId = %u, kId = %u, dstId = %u", m_slot->m_slotNo, lc->getAlgId(), lc->getKId(), lc->getDstId());
            }

            return true;
        }

        return false;
    }

    if (voiceSync) {
        if (m_slot->m_rfState == RS_RF_AUDIO) {
            m_lastRfN = 0U;

            // Convert the Audio Sync to be from the BS or MS as needed
            Sync::addDMRAudioSync(data + 2U, m_slot->m_duplex);

            uint32_t errors = 0U;
            uint8_t fid = m_slot->m_rfLC->getFID();
            if (fid == FID_ETSI || fid == FID_DMRA) {
                errors = m_fec.regenerateDMR(data + 2U);
                if (m_verbose) {
                    LogMessage(LOG_RF, DMR_DT_VOICE_SYNC ", audio, slot = %u, srcId = %u, dstId = %u, seqNo = 0, errs = %u/141 (%.1f%%)", m_slot->m_slotNo, m_slot->m_rfLC->getSrcId(), m_slot->m_rfLC->getDstId(),
                        errors, float(errors) / 1.41F);
                }

                if (errors > m_slot->m_silenceThreshold) {
                    insertNullAudio(data + 2U);
                    m_fec.regenerateDMR(data + 2U);

                    LogWarning(LOG_RF, DMR_DT_VOICE_SYNC ", exceeded lost audio threshold, filling in");
                }

                m_slot->m_rfErrs += errors;
            }

            m_slot->m_rfBits += 141U;
            m_slot->m_rfFrames++;

            m_slot->m_rfTGHang.start();
            m_slot->m_rfLastDstId = m_slot->m_rfLC->getDstId();

            m_rfEmbeddedReadN = (m_rfEmbeddedReadN + 1U) % 2U;
            m_rfEmbeddedWriteN = (m_rfEmbeddedWriteN + 1U) % 2U;
            m_rfEmbeddedData[m_rfEmbeddedWriteN].reset();

            if (!m_slot->m_rfTimeout) {
                data[0U] = modem::TAG_DATA;
                data[1U] = 0x00U;

                if (m_slot->m_duplex)
                    m_slot->writeQueueRF(data);

                m_slot->writeNetworkRF(data, DT_VOICE_SYNC, errors);
                return true;
            }

            return false;
        }
        else if (m_slot->m_rfState == RS_RF_LISTENING) {
            m_rfEmbeddedLC.reset();
            m_slot->m_rfState = RS_RF_LATE_ENTRY;
            return false;
        }
    }
    else {
        if (m_slot->m_rfState == RS_RF_AUDIO) {
            m_rfN = data[1U] & 0x0FU;

            if (m_rfN > 5U)
                return false;
            if (m_rfN == m_lastRfN)
                return false;
            if (m_rfN != (m_lastRfN + 1U))
                return false;
            m_lastRfN = m_rfN;

            uint32_t errors = 0U;
            uint8_t fid = m_slot->m_rfLC->getFID();
            if (fid == FID_ETSI || fid == FID_DMRA) {
                errors = m_fec.regenerateDMR(data + 2U);
                if (m_verbose) {
                    LogMessage(LOG_RF, DMR_DT_VOICE ", audio, slot = %u, srcId = %u, dstId = %u, seqNo = %u, errs = %u/141 (%.1f%%)", m_slot->m_slotNo, m_slot->m_rfLC->getSrcId(), m_slot->m_rfLC->getDstId(),
                        m_rfN, errors, float(errors) / 1.41F);
                }

                if (errors > m_slot->m_silenceThreshold) {
                    // Get the LCSS from the EMB
                    data::EMB emb;
                    emb.decode(data + 2U);

                    insertNullAudio(data + 2U);
                    m_fec.regenerateDMR(data + 2U);

                    // Regenerate EMB
                    emb.encode(data + 2U);

                    LogWarning(LOG_RF, DMR_DT_VOICE ", exceeded lost audio threshold, filling in");
                }

                m_slot->m_rfErrs += errors;
            }

            m_slot->m_rfBits += 141U;
            m_slot->m_rfFrames++;

            m_slot->m_rfTGHang.start();
            m_slot->m_rfLastDstId = m_slot->m_rfLC->getDstId();

            // Get the LCSS from the EMB
            data::EMB emb;
            emb.decode(data + 2U);
            uint8_t lcss = emb.getLCSS();

            // Dump any interesting Embedded Data
            bool ret = m_rfEmbeddedData[m_rfEmbeddedWriteN].addData(data + 2U, lcss);
            if (ret) {
                uint8_t flco = m_rfEmbeddedData[m_rfEmbeddedWriteN].getFLCO();

                uint8_t data[9U];
                m_rfEmbeddedData[m_rfEmbeddedWriteN].getRawData(data);

                char text[80U];
                switch (flco) {
                case FLCO_GROUP:
                case FLCO_PRIVATE:
                    break;

                case FLCO_GPS_INFO:
                    if (m_dumpTAData) {
                        ::sprintf(text, "DMR Slot %u, FLCO_GPS_INFO (Embedded GPS Info)", m_slot->m_slotNo);
                        Utils::dump(2U, text, data, 9U);
                    }

                    if (m_verbose) {
                        logGPSPosition(m_slot->m_rfLC->getSrcId(), data);
                    }
                    break;

                case FLCO_TALKER_ALIAS_HEADER:
                    if (!(m_rfTalkerId & TALKER_ID_HEADER)) {
                        if (m_dumpTAData) {
                            ::sprintf(text, "DMR Slot %u, FLCO_TALKER_ALIAS_HEADER (Embedded Talker Alias Header)", m_slot->m_slotNo);
                            Utils::dump(2U, text, data, 9U);
                        }

                        m_rfTalkerId |= TALKER_ID_HEADER;
                    }
                    break;

                case FLCO_TALKER_ALIAS_BLOCK1:
                    if (!(m_rfTalkerId & TALKER_ID_BLOCK1)) {
                        if (m_dumpTAData) {
                            ::sprintf(text, "DMR Slot %u, FLCO_TALKER_ALIAS_BLOCK1 (Embedded Talker Alias Block 1)", m_slot->m_slotNo);
                            Utils::dump(2U, text, data, 9U);
                        }

                        m_rfTalkerId |= TALKER_ID_BLOCK1;
                    }
                    break;

                case FLCO_TALKER_ALIAS_BLOCK2:
                    if (!(m_rfTalkerId & TALKER_ID_BLOCK2)) {
                        if (m_dumpTAData) {
                            ::sprintf(text, "DMR Slot %u, FLCO_TALKER_ALIAS_BLOCK2 (Embedded Talker Alias Block 2)", m_slot->m_slotNo);
                            Utils::dump(2U, text, data, 9U);
                        }

                        m_rfTalkerId |= TALKER_ID_BLOCK2;
                    }
                    break;

                case FLCO_TALKER_ALIAS_BLOCK3:
                    if (!(m_rfTalkerId & TALKER_ID_BLOCK3)) {
                        if (m_dumpTAData) {
                            ::sprintf(text, "DMR Slot %u, FLCO_TALKER_ALIAS_BLOCK3 (Embedded Talker Alias Block 3)", m_slot->m_slotNo);
                            Utils::dump(2U, text, data, 9U);
                        }

                        m_rfTalkerId |= TALKER_ID_BLOCK3;
                    }
                    break;

                default:
                    ::sprintf(text, "DMR Slot %u, Unknown Embedded Data", m_slot->m_slotNo);
                    Utils::dump(1U, text, data, 9U);
                    break;
                }
            }

            // Regenerate the previous super blocks Embedded Data or substitude the LC for it
            if (m_rfEmbeddedData[m_rfEmbeddedReadN].isValid())
                lcss = m_rfEmbeddedData[m_rfEmbeddedReadN].getData(data + 2U, m_rfN);
            else
                lcss = m_rfEmbeddedLC.getData(data + 2U, m_rfN);

            // Regenerate the EMB
            emb.setColorCode(m_slot->m_colorCode);
            emb.setLCSS(lcss);
            emb.encode(data + 2U);

            if (!m_slot->m_rfTimeout) {
                data[0U] = modem::TAG_DATA;
                data[1U] = 0x00U;

                m_slot->writeNetworkRF(data, DT_VOICE, errors);

                if (m_embeddedLCOnly) {
                    // Only send the previously received LC
                    lcss = m_rfEmbeddedLC.getData(data + 2U, m_rfN);

                    // Regenerate the EMB
                    emb.setColorCode(m_slot->m_colorCode);
                    emb.setLCSS(lcss);
                    emb.encode(data + 2U);
                }

                if (m_slot->m_duplex)
                    m_slot->writeQueueRF(data);

                return true;
            }

            return false;
        }
        else if (m_slot->m_rfState == RS_RF_LATE_ENTRY) {
            data::EMB emb;
            emb.decode(data + 2U);

            // If we haven't received an LC yet, then be strict on the color code
            uint8_t colorCode = emb.getColorCode();
            if (colorCode != m_slot->m_colorCode)
                return false;

            m_rfEmbeddedLC.addData(data + 2U, emb.getLCSS());
            lc::LC* lc = m_rfEmbeddedLC.getLC();
            if (lc != NULL) {
                uint32_t srcId = lc->getSrcId();
                uint32_t dstId = lc->getDstId();
                uint8_t flco = lc->getFLCO();

                CHECK_TRAFFIC_COLLISION_DELLC(dstId);

                // validate the source RID
                if (!acl::AccessControl::validateSrcId(srcId)) {
                    LogWarning(LOG_RF, "DMR Slot %u, DT_VOICE denial, RID rejection, srcId = %u", m_slot->m_slotNo, srcId);
                    delete lc;
                    m_slot->m_rfState = RS_RF_REJECTED;
                    return false;
                }

                // validate the target ID, if the target is a talkgroup
                if (flco == FLCO_GROUP) {
                    if (!acl::AccessControl::validateTGId(m_slot->m_slotNo, dstId)) {
                        LogWarning(LOG_RF, "DMR Slot %u, DT_VOICE denial, TGID rejection, srcId = %u, dstId = %u", m_slot->m_slotNo, srcId, dstId);
                        delete lc;
                        m_slot->m_rfState = RS_RF_REJECTED;
                        return false;
                    }
                }

                m_slot->m_rfLC = lc;

                // The standby LC data
                m_rfEmbeddedLC.setLC(*m_slot->m_rfLC);
                m_rfEmbeddedData[0U].setLC(*m_slot->m_rfLC);
                m_rfEmbeddedData[1U].setLC(*m_slot->m_rfLC);

                // Create a dummy start frame to replace the received frame
                uint8_t start[DMR_FRAME_LENGTH_BYTES + 2U];

                Sync::addDMRDataSync(start + 2U, m_slot->m_duplex);

                lc::FullLC fullLC;
                fullLC.encode(*m_slot->m_rfLC, start + 2U, DT_VOICE_LC_HEADER);

                SlotType slotType;
                slotType.setColorCode(m_slot->m_colorCode);
                slotType.setDataType(DT_VOICE_LC_HEADER);
                slotType.encode(start + 2U);

                start[0U] = modem::TAG_DATA;
                start[1U] = 0x00U;

                m_slot->m_rfTimeoutTimer.start();
                m_slot->m_rfTimeout = false;

                m_slot->m_rfFrames = 0U;
                m_slot->m_rfSeqNo = 0U;
                m_slot->m_rfBits = 1U;
                m_slot->m_rfErrs = 0U;

                m_rfEmbeddedReadN = 0U;
                m_rfEmbeddedWriteN = 1U;
                m_rfTalkerId = TALKER_ID_NONE;

                m_slot->m_minRSSI = m_slot->m_rssi;
                m_slot->m_maxRSSI = m_slot->m_rssi;
                m_slot->m_aveRSSI = m_slot->m_rssi;
                m_slot->m_rssiCount = 1U;

                if (m_slot->m_duplex) {
                    m_slot->m_queue.clear();
                    m_slot->m_modem->writeDMRAbort(m_slot->m_slotNo);

                    for (uint32_t i = 0U; i < NO_HEADERS_DUPLEX; i++)
                        m_slot->writeQueueRF(start);
                }

                m_slot->writeNetworkRF(start, DT_VOICE_LC_HEADER);

                m_rfN = data[1U] & 0x0FU;

                if (m_rfN > 5U)
                    return false;
                if (m_rfN == m_lastRfN)
                    return false;
                m_lastRfN = m_rfN;

                // Regenerate the EMB
                emb.encode(data + 2U);

                // Send the original audio frame out
                uint32_t errors = 0U;
                uint8_t fid = m_slot->m_rfLC->getFID();
                if (fid == FID_ETSI || fid == FID_DMRA) {
                    errors = m_fec.regenerateDMR(data + 2U);
                    if (m_verbose) {
                        LogMessage(LOG_RF, DMR_DT_VOICE ", audio, slot = %u, sequence no = %u, errs = %u/141 (%.1f%%)",
                            m_slot->m_slotNo, m_rfN, errors, float(errors) / 1.41F);
                    }

                    if (errors > m_slot->m_silenceThreshold) {
                        // Get the LCSS from the EMB
                        data::EMB emb;
                        emb.decode(data + 2U);

                        insertNullAudio(data + 2U);
                        m_fec.regenerateDMR(data + 2U);

                        // Regenerate EMB
                        emb.encode(data + 2U);

                        LogWarning(LOG_RF, DMR_DT_VOICE ", exceeded lost audio threshold, filling in");
                    }

                    m_slot->m_rfErrs += errors;
                }

                m_slot->m_rfBits += 141U;
                m_slot->m_rfFrames++;

                data[0U] = modem::TAG_DATA;
                data[1U] = 0x00U;

                if (m_slot->m_duplex)
                    m_slot->writeQueueRF(data);

                m_slot->writeNetworkRF(data, DT_VOICE, errors);

                m_slot->m_rfState = RS_RF_AUDIO;

                m_slot->m_rfTGHang.start();
                m_slot->m_rfLastDstId = dstId;

                if (m_slot->m_netState == RS_NET_IDLE) {
                    m_slot->setShortLC(m_slot->m_slotNo, dstId, flco, true);
                }

                ::ActivityLog("DMR", true, "Slot %u RF late entry from %u to %s%u", m_slot->m_slotNo, srcId, flco == FLCO_GROUP ? "TG " : "", dstId);
                return true;
            }
        }
    }

    return false;
}

/// <summary>
/// Process a voice frame from the network.
/// </summary>
/// <param name="dmrData"></param>
void VoicePacket::processNetwork(const data::Data& dmrData)
{
    uint8_t dataType = dmrData.getDataType();

    uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];
    dmrData.getData(data + 2U);
    
    if (dataType == DT_VOICE_LC_HEADER) {
        if (m_slot->m_netState == RS_NET_AUDIO)
            return;

        lc::FullLC fullLC;
        lc::LC* lc = fullLC.decode(data + 2U, DT_VOICE_LC_HEADER);
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

        m_slot->m_netLC = lc;

        // The standby LC data
        m_slot->m_voice->m_netEmbeddedLC.setLC(*m_slot->m_netLC);
        m_slot->m_voice->m_netEmbeddedData[0U].setLC(*m_slot->m_netLC);
        m_slot->m_voice->m_netEmbeddedData[1U].setLC(*m_slot->m_netLC);

        // Regenerate the LC data
        fullLC.encode(*m_slot->m_netLC, data + 2U, DT_VOICE_LC_HEADER);

        // Regenerate the Slot Type
        SlotType slotType;
        slotType.setColorCode(m_slot->m_colorCode);
        slotType.setDataType(DT_VOICE_LC_HEADER);
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        data[0U] = modem::TAG_DATA;
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

        if (m_verbose) {
            LogMessage(LOG_NET, DMR_DT_VOICE_LC_HEADER ", slot = %u, srcId = %u, dstId = %u, FLCO = $%02X, FID = $%02X, PF = %u", m_slot->m_slotNo, lc->getSrcId(), lc->getDstId(), lc->getFLCO(), lc->getFID(), lc->getPF());
        }

        ::ActivityLog("DMR", false, "Slot %u network voice header from %u to %s%u", m_slot->m_slotNo, srcId, flco == FLCO_GROUP ? "TG " : "", dstId);
    }
    else if (dataType == DT_VOICE_PI_HEADER) {
        if (m_slot->m_netState != RS_NET_AUDIO) {
            lc::LC* lc = new lc::LC(dmrData.getFLCO(), dmrData.getSrcId(), dmrData.getDstId());

            uint32_t srcId = lc->getSrcId();
            uint32_t dstId = lc->getDstId();

            CHECK_TG_HANG_DELLC(dstId);

            m_slot->m_netLC = lc;

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
            fullLC.encode(*m_slot->m_netLC, start + 2U, DT_VOICE_LC_HEADER);

            SlotType slotType;
            slotType.setColorCode(m_slot->m_colorCode);
            slotType.setDataType(DT_VOICE_LC_HEADER);
            slotType.encode(start + 2U);

            start[0U] = modem::TAG_DATA;
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

            m_slot->setShortLC(m_slot->m_slotNo, dstId, m_slot->m_netLC->getFLCO(), true);

            ::ActivityLog("DMR", false, "Slot %u network late entry from %u to %s%u",
                m_slot->m_slotNo, srcId, m_slot->m_netLC->getFLCO() == FLCO_GROUP ? "TG " : "", dstId);
        }

        lc::FullLC fullLC;
        lc::PrivacyLC* lc = fullLC.decodePI(data + 2U);
        if (lc == NULL) {
            LogWarning(LOG_NET, "DMR Slot %u, DT_VOICE_PI_HEADER, bad LC received, replacing", m_slot->m_slotNo);
            lc = new lc::PrivacyLC();
            lc->setDstId(dmrData.getDstId());
        }

        m_slot->m_netPrivacyLC = lc;

        // Regenerate the LC data
        fullLC.encodePI(*m_slot->m_netPrivacyLC, data + 2U);

        // Regenerate the Slot Type
        SlotType slotType;
        slotType.setColorCode(m_slot->m_colorCode);
        slotType.setDataType(DT_VOICE_PI_HEADER);
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        data[0U] = modem::TAG_DATA;
        data[1U] = 0x00U;

        m_slot->writeQueueNet(data);

        if (m_verbose) {
            LogMessage(LOG_NET, DMR_DT_VOICE_PI_HEADER ", slot = %u, algId = %u, kId = %u, dstId = %u", m_slot->m_slotNo, lc->getAlgId(), lc->getKId(), lc->getDstId());
        }
    }
    else if (dataType == DT_VOICE_SYNC) {
        if (m_slot->m_netState == RS_NET_IDLE) {
            lc::LC* lc = new lc::LC(dmrData.getFLCO(), dmrData.getSrcId(), dmrData.getDstId());

            uint32_t dstId = lc->getDstId();
            uint32_t srcId = lc->getSrcId();

            m_slot->m_netLC = lc;

            // The standby LC data
            m_netEmbeddedLC.setLC(*m_slot->m_netLC);
            m_netEmbeddedData[0U].setLC(*m_slot->m_netLC);
            m_netEmbeddedData[1U].setLC(*m_slot->m_netLC);

            m_lastFrameValid = false;

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
            fullLC.encode(*m_slot->m_netLC, start + 2U, DT_VOICE_LC_HEADER);

            SlotType slotType;
            slotType.setColorCode(m_slot->m_colorCode);
            slotType.setDataType(DT_VOICE_LC_HEADER);
            slotType.encode(start + 2U);

            start[0U] = modem::TAG_DATA;
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

            m_netEmbeddedReadN = 0U;
            m_netEmbeddedWriteN = 1U;
            m_netTalkerId = TALKER_ID_NONE;

            m_slot->m_netState = RS_NET_AUDIO;
            m_slot->m_netLastDstId = dstId;

            m_slot->setShortLC(m_slot->m_slotNo, dstId, m_slot->m_netLC->getFLCO(), true);

            ::ActivityLog("DMR", false, "Slot %u network late entry from %u to %s%u",
                m_slot->m_slotNo, srcId, m_slot->m_netLC->getFLCO() == FLCO_GROUP ? "TG " : "", dstId);
        }

        if (m_slot->m_netState == RS_NET_AUDIO) {
            uint8_t fid = m_slot->m_netLC->getFID();
            if (fid == FID_ETSI || fid == FID_DMRA) {
                m_slot->m_netErrs += m_fec.regenerateDMR(data + 2U);
                if (m_verbose) {
                    LogMessage(LOG_NET, "DMR Slot %u, DT_VOICE_SYNC audio, sequence no = %u, errs = %u/141 (%.1f%%)",
                        m_slot->m_slotNo, m_netN, m_slot->m_netErrs, float(m_slot->m_netErrs) / 1.41F);
                }
            }
            m_slot->m_netBits += 141U;

            data[0U] = modem::TAG_DATA;
            data[1U] = 0x00U;

            // Convert the Audio Sync to be from the BS or MS as needed
            Sync::addDMRAudioSync(data + 2U, m_slot->m_duplex);

            // Initialise the lost packet data
            if (m_slot->m_netFrames == 0U) {
                ::memcpy(m_lastFrame, data, DMR_FRAME_LENGTH_BYTES + 2U);
                m_lastFrameValid = true;
                m_netN = 5U;
                m_slot->m_netLost = 0U;
            }

            if (!m_slot->m_netTimeout)
                m_slot->writeQueueNet(data);

            m_netEmbeddedReadN = (m_netEmbeddedReadN + 1U) % 2U;
            m_netEmbeddedWriteN = (m_netEmbeddedWriteN + 1U) % 2U;

            m_netEmbeddedData[m_netEmbeddedWriteN].reset();

            m_slot->m_packetTimer.start();
            m_slot->m_elapsed.start();

            m_slot->m_netFrames++;

            // Save details in case we need to infill data
            m_netN = dmrData.getN();
        }
    }
    else if (dataType == DT_VOICE) {
        if (m_slot->m_netState != RS_NET_AUDIO)
            return;

        uint8_t fid = m_slot->m_netLC->getFID();
        if (fid == FID_ETSI || fid == FID_DMRA) {
            m_slot->m_netErrs += m_fec.regenerateDMR(data + 2U);
            if (m_verbose) {
                LogMessage(LOG_NET, DMR_DT_VOICE ", audio, slot = %u, srcId = %u, dstId = %u, seqNo = %u, errs = %u/141 (%.1f%%)", m_slot->m_slotNo, m_slot->m_netLC->getSrcId(), m_slot->m_netLC->getDstId(),
                    m_netN, m_slot->m_netErrs, float(m_slot->m_netErrs) / 1.41F);
            }
        }
        m_slot->m_netBits += 141U;

        // Get the LCSS from the EMB
        data::EMB emb;
        emb.decode(data + 2U);
        uint8_t lcss = emb.getLCSS();

        // Dump any interesting Embedded Data
        bool ret = m_netEmbeddedData[m_netEmbeddedWriteN].addData(data + 2U, lcss);
        if (ret) {
            uint8_t flco = m_netEmbeddedData[m_netEmbeddedWriteN].getFLCO();

            uint8_t data[9U];
            m_netEmbeddedData[m_netEmbeddedWriteN].getRawData(data);

            char text[80U];
            switch (flco) {
            case FLCO_GROUP:
            case FLCO_PRIVATE:
                break;
            case FLCO_GPS_INFO:
                if (m_dumpTAData) {
                    ::sprintf(text, "DMR Slot %u, FLCO_GPS_INFO (Embedded GPS Info)", m_slot->m_slotNo);
                    Utils::dump(2U, text, data, 9U);
                }

                logGPSPosition(m_slot->m_netLC->getSrcId(), data);
                break;
            case FLCO_TALKER_ALIAS_HEADER:
                if (!(m_netTalkerId & TALKER_ID_HEADER)) {
                    if (m_dumpTAData) {
                        ::sprintf(text, "DMR Slot %u, FLCO_TALKER_ALIAS_HEADER (Embedded Talker Alias Header)", m_slot->m_slotNo);
                        Utils::dump(2U, text, data, 9U);
                    }

                    m_netTalkerId |= TALKER_ID_HEADER;
                }
                break;
            case FLCO_TALKER_ALIAS_BLOCK1:
                if (!(m_netTalkerId & TALKER_ID_BLOCK1)) {
                    if (m_dumpTAData) {
                        ::sprintf(text, "DMR Slot %u, FLCO_TALKER_ALIAS_BLOCK1 (Embedded Talker Alias Block 1)", m_slot->m_slotNo);
                        Utils::dump(2U, text, data, 9U);
                    }

                    m_netTalkerId |= TALKER_ID_BLOCK1;
                }
                break;
            case FLCO_TALKER_ALIAS_BLOCK2:
                if (!(m_netTalkerId & TALKER_ID_BLOCK2)) {
                    if (m_dumpTAData) {
                        ::sprintf(text, "DMR Slot %u, FLCO_TALKER_ALIAS_BLOCK2 (Embedded Talker Alias Block 2)", m_slot->m_slotNo);
                        Utils::dump(2U, text, data, 9U);
                    }

                    m_netTalkerId |= TALKER_ID_BLOCK2;
                }
                break;
            case FLCO_TALKER_ALIAS_BLOCK3:
                if (!(m_netTalkerId & TALKER_ID_BLOCK3)) {
                    if (m_dumpTAData) {
                        ::sprintf(text, "DMR Slot %u, FLCO_TALKER_ALIAS_BLOCK3 (Embedded Talker Alias Block 3)", m_slot->m_slotNo);
                        Utils::dump(2U, text, data, 9U);
                    }

                    m_netTalkerId |= TALKER_ID_BLOCK3;
                }
                break;
            default:
                ::sprintf(text, "DMR Slot %u, Unknown Embedded Data", m_slot->m_slotNo);
                Utils::dump(1U, text, data, 9U);
                break;
            }
        }

        if (m_embeddedLCOnly) {
            // Only send the previously received LC
            lcss = m_netEmbeddedLC.getData(data + 2U, dmrData.getN());
        }
        else {
            // Regenerate the previous super blocks Embedded Data or substitude the LC for it
            if (m_netEmbeddedData[m_netEmbeddedReadN].isValid())
                lcss = m_netEmbeddedData[m_netEmbeddedReadN].getData(data + 2U, dmrData.getN());
            else
                lcss = m_netEmbeddedLC.getData(data + 2U, dmrData.getN());
        }

        // Regenerate the EMB
        emb.setColorCode(m_slot->m_colorCode);
        emb.setLCSS(lcss);
        emb.encode(data + 2U);

        data[0U] = modem::TAG_DATA;
        data[1U] = 0x00U;

        // Initialise the lost packet data
        if (m_slot->m_netFrames == 0U) {
            ::memcpy(m_lastFrame, data, DMR_FRAME_LENGTH_BYTES + 2U);
            m_lastFrameValid = true;
            m_netN = 5U;
            m_slot->m_netLost = 0U;
        }

        if (insertSilence(data, dmrData.getN())) {
            if (!m_slot->m_netTimeout)
                m_slot->writeQueueNet(data);
        }

        m_slot->m_packetTimer.start();
        m_slot->m_elapsed.start();

        m_slot->m_netFrames++;

        // Save details in case we need to infill data
        m_netN = dmrData.getN();
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
/// Initializes a new instance of the VoicePacket class.
/// </summary>
/// <param name="slot">DMR slot.</param>
/// <param name="network">Instance of the BaseNetwork class.</param>
/// <param name="embeddedLCOnly"></param>
/// <param name="dumpTAData"></param>
/// <param name="debug">Flag indicating whether DMR debug is enabled.</param>
/// <param name="verbose">Flag indicating whether DMR verbose logging is enabled.</param>
VoicePacket::VoicePacket(Slot* slot, network::BaseNetwork* network, bool embeddedLCOnly, bool dumpTAData, bool debug, bool verbose) :
    m_slot(slot),
    m_lastFrame(NULL),
    m_lastFrameValid(false),
    m_rfN(0U),
    m_lastRfN(0U),
    m_netN(0U),
    m_rfEmbeddedLC(),
    m_rfEmbeddedData(NULL),
    m_rfEmbeddedReadN(0U),
    m_rfEmbeddedWriteN(1U),
    m_netEmbeddedLC(),
    m_netEmbeddedData(NULL),
    m_netEmbeddedReadN(0U),
    m_netEmbeddedWriteN(1U),
    m_rfTalkerId(TALKER_ID_NONE),
    m_netTalkerId(TALKER_ID_NONE),
    m_fec(),
    m_embeddedLCOnly(embeddedLCOnly),
    m_dumpTAData(dumpTAData),
    m_verbose(verbose),
    m_debug(debug)
{
    m_lastFrame = new uint8_t[DMR_FRAME_LENGTH_BYTES + 2U];

    m_rfEmbeddedData = new data::EmbeddedData[2U];
    m_netEmbeddedData = new data::EmbeddedData[2U];
}

/// <summary>
/// Finalizes a instance of the VoicePacket class.
/// </summary>
VoicePacket::~VoicePacket()
{
    delete[] m_lastFrame;

    delete[] m_rfEmbeddedData;
    delete[] m_netEmbeddedData;
}

/// <summary>
///
/// </summary>
/// <param name="srcId">Source radio ID.</param>
/// <param name="data"></param>
void VoicePacket::logGPSPosition(const uint32_t srcId, const uint8_t* data)
{
    uint32_t errorVal = (data[2U] & 0x0E) >> 1U;

    const char* error;
    switch (errorVal) {
    case 0U:
        error = "< 2m";
        break;
    case 1U:
        error = "< 20m";
        break;
    case 2U:
        error = "< 200m";
        break;
    case 3U:
        error = "< 2km";
        break;
    case 4U:
        error = "< 20km";
        break;
    case 5U:
        error = "< 200km";
        break;
    case 6U:
        error = "> 200km";
        break;
    default:
        error = "not known";
        break;
    }

    int longitudeVal = ((data[2U] & 0x01U) << 31) | (data[3U] << 23) | (data[4U] << 15) | (data[5U] << 7);
    longitudeVal >>= 7;

    int latitudeVal = (data[6U] << 24) | (data[7U] << 16) | (data[8U] << 8);
    latitudeVal >>= 8;

    float longitude = 360.0F / 33554432.0F;    // 360/2^25 steps
    float latitude = 180.0F / 16777216.0F;    // 180/2^24 steps

    longitude *= float(longitudeVal);
    latitude *= float(latitudeVal);

    LogMessage(LOG_DMR, "GPS position for %u [lat %f, long %f] (Position error %s)", srcId, latitude, longitude, error);
}

/// <summary>
/// Helper to insert AMBE null frames for missing audio.
/// </summary>
/// <param name="data"></param>
void VoicePacket::insertNullAudio(uint8_t* data)
{
    uint8_t* ambeBuffer = new uint8_t[dmr::DMR_AMBE_LENGTH_BYTES];
    ::memset(ambeBuffer, 0x00U, dmr::DMR_AMBE_LENGTH_BYTES);

    for (uint32_t i = 0; i < 3U; i++) {
        ::memcpy(ambeBuffer + (i * 9U), DMR_NULL_AMBE, 9U);
    }

    ::memcpy(data, ambeBuffer, 13U);
    data[13U] = ambeBuffer[13U] & 0xF0U;
    data[19U] = ambeBuffer[13U] & 0x0FU;
    ::memcpy(data + 20U, ambeBuffer + 14U, 13U);

    delete[] ambeBuffer;
}

/// <summary>
/// Helper to insert DMR AMBE silence frames.
/// </summary>
/// <param name="data"></param>
/// <param name="seqNo"></param>
/// <returns></returns>
bool VoicePacket::insertSilence(const uint8_t* data, uint8_t seqNo)
{
    assert(data != NULL);

    // Do not send duplicate
    if (seqNo == m_netN)
        return false;

    // Check to see if we have any spaces to fill?
    uint8_t seq = (m_netN + 1U) % 6U;
    if (seq == seqNo) {
        // Just copy the data, nothing else to do here
        ::memcpy(m_lastFrame, data, DMR_FRAME_LENGTH_BYTES + 2U);
        m_lastFrameValid = true;
        return true;
    }

    uint32_t count = (seqNo - seq + 6U) % 6U;

    insertSilence(count);

    ::memcpy(m_lastFrame, data, DMR_FRAME_LENGTH_BYTES + 2U);
    m_lastFrameValid = true;

    return true;
}

/// <summary>
/// Helper to insert DMR AMBE silence frames.
/// </summary>
/// <param name="count"></param>
void VoicePacket::insertSilence(uint32_t count)
{
    uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];

    if (m_lastFrameValid) {
        ::memcpy(data, m_lastFrame, 2U);                        // The control data
        ::memcpy(data + 2U, m_lastFrame + 24U + 2U, 9U);        // Copy the last audio block to the first
        ::memcpy(data + 24U + 2U, data + 2U, 9U);               // Copy the last audio block to the last
        ::memcpy(data + 9U + 2U, data + 2U, 5U);                // Copy the last audio block to the middle (1/2)
        ::memcpy(data + 19U + 2U, data + 4U + 2U, 5U);          // Copy the last audio block to the middle (2/2)
    }
    else {
        // Not sure what to do if this isn't AMBE audio
        ::memcpy(data, DMR_SILENCE_DATA, DMR_FRAME_LENGTH_BYTES + 2U);
    }

    uint8_t n = (m_netN + 1U) % 6U;

    uint8_t fid = m_slot->m_netLC->getFID();

    data::EMB emb;
    emb.setColorCode(m_slot->m_colorCode);

    for (uint32_t i = 0U; i < count; i++) {
        // Only use our silence frame if its AMBE audio data
        if (fid == FID_ETSI || fid == FID_DMRA) {
            if (i > 0U) {
                ::memcpy(data, DMR_SILENCE_DATA, DMR_FRAME_LENGTH_BYTES + 2U);
                m_lastFrameValid = false;
            }
        }

        if (n == 0U) {
            Sync::addDMRAudioSync(data + 2U, m_slot->m_duplex);
        }
        else {
            uint8_t lcss = m_netEmbeddedLC.getData(data + 2U, n);
            emb.setLCSS(lcss);
            emb.encode(data + 2U);
        }

        m_slot->writeQueueNet(data);

        m_netN = n;

        m_slot->m_netFrames++;
        m_slot->m_netLost++;

        n = (n + 1U) % 6U;
    }
}
