// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017,2018 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/dmr/acl/AccessControl.h"
#include "common/dmr/data/EMB.h"
#include "common/dmr/lc/CSBK.h"
#include "common/dmr/lc/FullLC.h"
#include "common/dmr/SlotType.h"
#include "common/dmr/Sync.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "dmr/packet/Voice.h"
#include "dmr/Slot.h"
#include "ActivityLog.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::packet;

#include <cassert>
#include <algorithm>

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

#define CHECK_AUTHORITATIVE(_DST_ID)                                                    \
    if (!m_slot->m_authoritative && m_slot->m_permittedDstId != _DST_ID) {              \
        LogWarning(LOG_RF, "[NON-AUTHORITATIVE] Ignoring RF traffic, destination not permitted, dstId = %u", _DST_ID); \
        m_slot->m_rfState = RS_RF_LISTENING;                                            \
        return false;                                                                   \
    }

#define CHECK_NET_AUTHORITATIVE(_DST_ID)                                                \
    if (!m_slot->m_authoritative && m_slot->m_permittedDstId != _DST_ID) {              \
        return;                                                                         \
    }

#define CHECK_TRAFFIC_COLLISION(_DST_ID)                                                \
    if (m_slot->m_netState != RS_NET_IDLE && _DST_ID == m_slot->m_netLastDstId) {       \
        LogWarning(LOG_RF, "DMR Slot %u, Traffic collision detect, preempting new RF traffic to existing network traffic!", m_slot->m_slotNo); \
        m_slot->m_rfState = RS_RF_LISTENING;                                            \
        return false;                                                                   \
    }                                                                                   \
    if (m_slot->m_enableTSCC && _DST_ID == m_slot->m_netLastDstId) {                    \
        if (m_slot->m_affiliations->isNetGranted(_DST_ID)) {                            \
            LogWarning(LOG_RF, "DMR Slot %u, Traffic collision detect, preempting new RF traffic to existing granted network traffic (Are we in a voting condition?)", m_slot->m_slotNo); \
            m_slot->m_rfState = RS_RF_LISTENING;                                        \
            return false;                                                               \
        }                                                                               \
    }

// Don't process network frames if the destination ID's don't match and the network TG hang
// timer is running, and don't process network frames if the RF modem isn't in a listening state
#define CHECK_NET_TRAFFIC_COLLISION(_DST_ID)                                            \
    if (m_slot->m_rfLastDstId != 0U) {                                                  \
        if (m_slot->m_rfLastDstId != _DST_ID && (m_slot->m_rfTGHang.isRunning() && !m_slot->m_rfTGHang.hasExpired())) { \
            return;                                                                     \
        }                                                                               \
    }                                                                                   \
                                                                                        \
    if (m_slot->m_netLastDstId != 0U) {                                                 \
        if (m_slot->m_netLastDstId != _DST_ID && (m_slot->m_netTGHang.isRunning() && !m_slot->m_netTGHang.hasExpired())) { \
            return;                                                                     \
        }                                                                               \
    }                                                                                   \

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Process DMR voice frame from the RF interface. */

bool Voice::process(uint8_t* data, uint32_t len)
{
    assert(data != nullptr);

    bool dataSync = (data[1U] & SYNC_DATA) == SYNC_DATA;
    bool voiceSync = (data[1U] & SYNC_VOICE) == SYNC_VOICE;

    if (dataSync) {
        DataType::E dataType = (DataType::E)(data[1U] & 0x0FU);

        SlotType slotType;
        slotType.setColorCode(m_slot->m_colorCode);
        slotType.setDataType(dataType);

        if (dataType == DataType::VOICE_LC_HEADER) {
            if (m_slot->m_rfState == RS_RF_AUDIO)
                return true;

            lc::FullLC fullLC;
            std::unique_ptr<lc::LC> lc = fullLC.decode(data + 2U, DataType::VOICE_LC_HEADER);
            if (lc == nullptr)
                return false;

            uint32_t srcId = lc->getSrcId();
            uint32_t dstId = lc->getDstId();
            FLCO::E flco = lc->getFLCO();

            CHECK_AUTHORITATIVE(dstId);
            CHECK_TRAFFIC_COLLISION(dstId);

            if (m_slot->m_tsccPayloadDstId != 0U && m_slot->m_tsccPayloadActRetry.isRunning()) {
                m_slot->m_tsccPayloadActRetry.stop();
            }

            // validate source RID
            if (!acl::AccessControl::validateSrcId(srcId)) {
                if (m_slot->m_data->m_lastRejectId == 0U || m_slot->m_data->m_lastRejectId == srcId) {
                    LogWarning(LOG_RF, "DMR Slot %u, VOICE_LC_HEADER denial, RID rejection, srcId = %u", m_slot->m_slotNo, srcId);
                    ::ActivityLog("DMR", true, "Slot %u RF voice rejection from %u to %s%u ", m_slot->m_slotNo, srcId, flco == FLCO::GROUP ? "TG " : "", dstId);
                }

                m_slot->m_rfLastDstId = 0U;
                m_slot->m_rfLastSrcId = 0U;
                m_slot->m_rfTGHang.stop();

                m_slot->m_rfState = RS_RF_REJECTED;
                return false;
            }

            // validate target TID, if the target is a talkgroup
            if (flco == FLCO::GROUP) {
                if (!acl::AccessControl::validateTGId(m_slot->m_slotNo, dstId)) {
                    if (m_slot->m_data->m_lastRejectId == 0U || m_slot->m_data->m_lastRejectId == dstId) {
                        LogWarning(LOG_RF, "DMR Slot %u, VOICE_LC_HEADER denial, TGID rejection, srcId = %u, dstId = %u", m_slot->m_slotNo, srcId, dstId);
                        ::ActivityLog("DMR", true, "Slot %u RF voice rejection from %u to TG %u ", m_slot->m_slotNo, srcId, dstId);
                    }

                    m_slot->m_rfLastDstId = 0U;
                    m_slot->m_rfLastSrcId = 0U;
                    m_slot->m_rfTGHang.stop();

                    m_slot->m_rfState = RS_RF_REJECTED;
                    return false;
                }
            }

            m_slot->m_data->m_lastRejectId = 0U;

            uint8_t fid = lc->getFID();

            // NOTE: this is fiddly -- on Motorola a FID of 0x10 indicates a SU has transmitted with Enhanced Privacy enabled -- this might change
            // and is not exact science!
            bool encrypted = (fid & 0x10U) == 0x10U;

            m_slot->m_rfLC = std::move(lc);

            // The standby LC data
            m_slot->m_voice->m_rfEmbeddedLC.setLC(*m_slot->m_rfLC);
            m_slot->m_voice->m_rfEmbeddedData[0U].setLC(*m_slot->m_rfLC);
            m_slot->m_voice->m_rfEmbeddedData[1U].setLC(*m_slot->m_rfLC);

            // Regenerate the LC data
            fullLC.encode(*m_slot->m_rfLC, data + 2U, DataType::VOICE_LC_HEADER);

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
            m_slot->m_voice->m_rfTalkerId = TalkerID::NONE;

            m_slot->m_minRSSI = m_slot->m_rssi;
            m_slot->m_maxRSSI = m_slot->m_rssi;
            m_slot->m_aveRSSI = m_slot->m_rssi;
            m_slot->m_rssiCount = 1U;

            if (m_slot->m_duplex) {
                m_slot->m_txQueue.clear();
                m_slot->m_modem->writeDMRAbort(m_slot->m_slotNo);

                for (uint32_t i = 0U; i < NO_HEADERS_DUPLEX; i++)
                    m_slot->addFrame(data);
            }

            m_slot->writeNetwork(data, DataType::VOICE_LC_HEADER);

            m_slot->m_rfState = RS_RF_AUDIO;
            m_slot->m_rfLastDstId = dstId;
            m_slot->m_rfLastSrcId = srcId;

            if (m_slot->m_netState == RS_NET_IDLE) {
                m_slot->setShortLC(m_slot->m_slotNo, dstId, flco, Slot::SLCO_ACT_TYPE::VOICE);
            }

            if (m_verbose) {
                LogMessage(LOG_RF, DMR_DT_VOICE_LC_HEADER ", slot = %u, srcId = %u, dstId = %u, FLCO = $%02X, FID = $%02X, PF = %u", m_slot->m_slotNo,
                    m_slot->m_rfLC->getSrcId(), m_slot->m_rfLC->getDstId(), m_slot->m_rfLC->getFLCO(), m_slot->m_rfLC->getFID(), m_slot->m_rfLC->getPF());
            }

            ::ActivityLog("DMR", true, "Slot %u RF %svoice header from %u to %s%u", m_slot->m_slotNo, encrypted ? "encrypted " : "", srcId, flco == FLCO::GROUP ? "TG " : "", dstId);
            return true;
        }
        else if (dataType == DataType::VOICE_PI_HEADER) {
            if (m_slot->m_rfState != RS_RF_AUDIO)
                return false;

            lc::FullLC fullLC;
            std::unique_ptr<lc::PrivacyLC> lc = fullLC.decodePI(data + 2U);
            if (lc == nullptr) {
                LogWarning(LOG_RF, "DMR Slot %u, VOICE_PI_HEADER, bad LC received, replacing", m_slot->m_slotNo);
                lc = std::make_unique<lc::PrivacyLC>();
                lc->setDstId(m_slot->m_rfLC->getDstId());
            }

            m_slot->m_rfPrivacyLC = std::move(lc);

            // Regenerate the LC data
            fullLC.encodePI(*m_slot->m_rfPrivacyLC, data + 2U);

            // Regenerate the Slot Type
            slotType.encode(data + 2U);

            // Convert the Data Sync to be from the BS or MS as needed
            Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

            data[0U] = modem::TAG_DATA;
            data[1U] = 0x00U;

            if (m_slot->m_duplex)
                m_slot->addFrame(data);

            m_slot->writeNetwork(data, DataType::VOICE_PI_HEADER);

            if (m_verbose) {
                LogMessage(LOG_RF, DMR_DT_VOICE_PI_HEADER ", slot = %u, algId = %u, kId = %u, dstId = %u", m_slot->m_slotNo,
                    m_slot->m_rfPrivacyLC->getAlgId(), m_slot->m_rfPrivacyLC->getKId(), m_slot->m_rfPrivacyLC->getDstId());
            }

            return true;
        }

        return false;
    }

    if (voiceSync) {
        if (m_slot->m_rfState == RS_RF_AUDIO) {
            m_lastRfN = 0U;

            // convert the Audio Sync to be from the BS or MS as needed
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
            m_slot->m_netTGHang.stop();
            m_slot->m_rfLastDstId = m_slot->m_rfLC->getDstId();
            m_slot->m_rfLastSrcId = m_slot->m_rfLC->getSrcId();

            m_rfEmbeddedReadN = (m_rfEmbeddedReadN + 1U) % 2U;
            m_rfEmbeddedWriteN = (m_rfEmbeddedWriteN + 1U) % 2U;
            m_rfEmbeddedData[m_rfEmbeddedWriteN].reset();

            if (!m_slot->m_rfTimeout) {
                data[0U] = modem::TAG_DATA;
                data[1U] = 0x00U;

                if (m_slot->m_duplex)
                    m_slot->addFrame(data);

                m_slot->writeNetwork(data, DataType::VOICE_SYNC, errors);
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
                    // get the LCSS from the EMB
                    data::EMB emb;
                    emb.decode(data + 2U);

                    insertNullAudio(data + 2U);
                    m_fec.regenerateDMR(data + 2U);

                    // regenerate EMB
                    emb.encode(data + 2U);

                    LogWarning(LOG_RF, DMR_DT_VOICE ", exceeded lost audio threshold, filling in");
                }

                m_slot->m_rfErrs += errors;
            }

            m_slot->m_rfBits += 141U;
            m_slot->m_rfFrames++;

            m_slot->m_rfTGHang.start();
            m_slot->m_netTGHang.stop();
            m_slot->m_rfLastDstId = m_slot->m_rfLC->getDstId();
            m_slot->m_rfLastSrcId = m_slot->m_rfLC->getSrcId();

            // get the LCSS from the EMB
            data::EMB emb;
            emb.decode(data + 2U);
            uint8_t lcss = emb.getLCSS();

            // dump any interesting Embedded Data
            bool ret = m_rfEmbeddedData[m_rfEmbeddedWriteN].addData(data + 2U, lcss);
            if (ret) {
                uint8_t flco = m_rfEmbeddedData[m_rfEmbeddedWriteN].getFLCO();

                uint8_t data[9U];
                m_rfEmbeddedData[m_rfEmbeddedWriteN].getRawData(data);

                char text[80U];
                switch (flco) {
                case FLCO::GROUP:
                case FLCO::PRIVATE:
                    break;

                case FLCO::GPS_INFO:
                    if (m_dumpTAData) {
                        ::sprintf(text, "DMR Slot %u, GPS_INFO (Embedded GPS Info)", m_slot->m_slotNo);
                        Utils::dump(2U, text, data, 9U);
                    }

                    if (m_verbose) {
                        logGPSPosition(m_slot->m_rfLC->getSrcId(), data);
                    }
                    break;

                case FLCO::TALKER_ALIAS_HEADER:
                    if (!(m_rfTalkerId & TalkerID::HEADER)) {
                        if (m_dumpTAData) {
                            ::sprintf(text, "DMR Slot %u, TALKER_ALIAS_HEADER (Embedded Talker Alias Header)", m_slot->m_slotNo);
                            Utils::dump(2U, text, data, 9U);
                        }

                        m_rfTalkerId |= TalkerID::HEADER;
                    }
                    break;

                case FLCO::TALKER_ALIAS_BLOCK1:
                    if (!(m_rfTalkerId & TalkerID::BLOCK1)) {
                        if (m_dumpTAData) {
                            ::sprintf(text, "DMR Slot %u, TALKER_ALIAS_BLOCK1 (Embedded Talker Alias Block 1)", m_slot->m_slotNo);
                            Utils::dump(2U, text, data, 9U);
                        }

                        m_rfTalkerId |= TalkerID::BLOCK1;
                    }
                    break;

                case FLCO::TALKER_ALIAS_BLOCK2:
                    if (!(m_rfTalkerId & TalkerID::BLOCK2)) {
                        if (m_dumpTAData) {
                            ::sprintf(text, "DMR Slot %u, TALKER_ALIAS_BLOCK2 (Embedded Talker Alias Block 2)", m_slot->m_slotNo);
                            Utils::dump(2U, text, data, 9U);
                        }

                        m_rfTalkerId |= TalkerID::BLOCK2;
                    }
                    break;

                case FLCO::TALKER_ALIAS_BLOCK3:
                    if (!(m_rfTalkerId & TalkerID::BLOCK3)) {
                        if (m_dumpTAData) {
                            ::sprintf(text, "DMR Slot %u, TALKER_ALIAS_BLOCK3 (Embedded Talker Alias Block 3)", m_slot->m_slotNo);
                            Utils::dump(2U, text, data, 9U);
                        }

                        m_rfTalkerId |= TalkerID::BLOCK3;
                    }
                    break;

                default:
                    ::sprintf(text, "DMR Slot %u, Unknown Embedded Data", m_slot->m_slotNo);
                    Utils::dump(1U, text, data, 9U);
                    break;
                }
            }

            // Regenerate the previous super blocks Embedded Data or substitude the LC for it
            if (m_rfEmbeddedData[m_rfEmbeddedReadN].valid())
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

                m_slot->writeNetwork(data, DataType::VOICE, errors);

                if (m_embeddedLCOnly) {
                    // Only send the previously received LC
                    lcss = m_rfEmbeddedLC.getData(data + 2U, m_rfN);

                    // Regenerate the EMB
                    emb.setColorCode(m_slot->m_colorCode);
                    emb.setLCSS(lcss);
                    emb.encode(data + 2U);
                }

                if (m_slot->m_duplex)
                    m_slot->addFrame(data);

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
            std::unique_ptr<lc::LC> lc = m_rfEmbeddedLC.getLC();
            if (lc != nullptr) {
                uint32_t srcId = lc->getSrcId();
                uint32_t dstId = lc->getDstId();
                FLCO::E flco = lc->getFLCO();

                CHECK_AUTHORITATIVE(dstId);
                CHECK_TRAFFIC_COLLISION(dstId);

                // validate the source RID
                if (!acl::AccessControl::validateSrcId(srcId)) {
                    LogWarning(LOG_RF, "DMR Slot %u, VOICE denial, RID rejection, srcId = %u", m_slot->m_slotNo, srcId);
                    m_slot->m_rfState = RS_RF_REJECTED;
                    return false;
                }

                // validate the target ID, if the target is a talkgroup
                if (flco == FLCO::GROUP) {
                    if (!acl::AccessControl::validateTGId(m_slot->m_slotNo, dstId)) {
                        LogWarning(LOG_RF, "DMR Slot %u, VOICE denial, TGID rejection, srcId = %u, dstId = %u", m_slot->m_slotNo, srcId, dstId);
                        m_slot->m_rfState = RS_RF_REJECTED;
                        return false;
                    }
                }

                m_slot->m_rfLC = std::move(lc);

                // The standby LC data
                m_rfEmbeddedLC.setLC(*m_slot->m_rfLC);
                m_rfEmbeddedData[0U].setLC(*m_slot->m_rfLC);
                m_rfEmbeddedData[1U].setLC(*m_slot->m_rfLC);

                // Create a dummy start frame to replace the received frame
                uint8_t start[DMR_FRAME_LENGTH_BYTES + 2U];

                Sync::addDMRDataSync(start + 2U, m_slot->m_duplex);

                lc::FullLC fullLC;
                fullLC.encode(*m_slot->m_rfLC, start + 2U, DataType::VOICE_LC_HEADER);

                SlotType slotType;
                slotType.setColorCode(m_slot->m_colorCode);
                slotType.setDataType(DataType::VOICE_LC_HEADER);
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
                m_rfTalkerId = TalkerID::NONE;

                m_slot->m_minRSSI = m_slot->m_rssi;
                m_slot->m_maxRSSI = m_slot->m_rssi;
                m_slot->m_aveRSSI = m_slot->m_rssi;
                m_slot->m_rssiCount = 1U;

                if (m_slot->m_duplex) {
                    m_slot->m_txQueue.clear();
                    m_slot->m_modem->writeDMRAbort(m_slot->m_slotNo);

                    for (uint32_t i = 0U; i < NO_HEADERS_DUPLEX; i++)
                        m_slot->addFrame(start);
                }

                m_slot->writeNetwork(start, DataType::VOICE_LC_HEADER);

                m_rfN = data[1U] & 0x0FU;

                if (m_rfN > 5U)
                    return false;
                if (m_rfN == m_lastRfN)
                    return false;
                m_lastRfN = m_rfN;

                // regenerate the EMB
                emb.encode(data + 2U);

                // send the original audio frame out
                uint32_t errors = 0U;
                uint8_t fid = m_slot->m_rfLC->getFID();
                if (fid == FID_ETSI || fid == FID_DMRA) {
                    errors = m_fec.regenerateDMR(data + 2U);
                    if (m_verbose) {
                        LogMessage(LOG_RF, DMR_DT_VOICE ", audio, slot = %u, sequence no = %u, errs = %u/141 (%.1f%%)",
                            m_slot->m_slotNo, m_rfN, errors, float(errors) / 1.41F);
                    }

                    if (errors > m_slot->m_silenceThreshold) {
                        // get the LCSS from the EMB
                        data::EMB emb;
                        emb.decode(data + 2U);

                        insertNullAudio(data + 2U);
                        m_fec.regenerateDMR(data + 2U);

                        // regenerate EMB
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
                    m_slot->addFrame(data);

                m_slot->writeNetwork(data, DataType::VOICE, errors);

                m_slot->m_rfState = RS_RF_AUDIO;

                m_slot->m_rfTGHang.start();
                m_slot->m_netTGHang.stop();
                m_slot->m_rfLastDstId = dstId;
                m_slot->m_rfLastSrcId = srcId;

                if (m_slot->m_netState == RS_NET_IDLE) {
                    m_slot->setShortLC(m_slot->m_slotNo, dstId, flco, Slot::SLCO_ACT_TYPE::VOICE);
                }

                ::ActivityLog("DMR", true, "Slot %u RF late entry from %u to %s%u", m_slot->m_slotNo, srcId, flco == FLCO::GROUP ? "TG " : "", dstId);
                return true;
            }
        }
    }

    return false;
}

/* Process a voice frame from the network. */

void Voice::processNetwork(const data::NetData& dmrData)
{
    uint8_t dataType = dmrData.getDataType();

    uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];
    dmrData.getData(data + 2U);

    if (dataType == DataType::VOICE_LC_HEADER) {
        if (m_slot->m_netState == RS_NET_AUDIO)
            return;

        lc::FullLC fullLC;
        std::unique_ptr<lc::LC> lc = fullLC.decode(data + 2U, DataType::VOICE_LC_HEADER);
        if (lc == nullptr) {
            LogWarning(LOG_NET, "DMR Slot %u, VOICE_LC_HEADER, bad LC received from the network, replacing", m_slot->m_slotNo);
            lc = std::make_unique<lc::LC>(dmrData.getFLCO(), dmrData.getSrcId(), dmrData.getDstId());
        }

        uint32_t srcId = lc->getSrcId();
        uint32_t dstId = lc->getDstId();
        FLCO::E flco = lc->getFLCO();

        CHECK_NET_AUTHORITATIVE(dstId);
        CHECK_NET_TRAFFIC_COLLISION(dstId);

        if (m_slot->m_tsccPayloadDstId != 0U && m_slot->m_tsccPayloadActRetry.isRunning()) {
            m_slot->m_tsccPayloadActRetry.stop();
        }

        if (dstId != dmrData.getDstId() || srcId != dmrData.getSrcId() || flco != dmrData.getFLCO())
            LogWarning(LOG_NET, "DMR Slot %u, VOICE_LC_HEADER, header doesn't match the DMR RF header: %u->%s%u %u->%s%u", m_slot->m_slotNo,
                dmrData.getSrcId(), dmrData.getFLCO() == FLCO::GROUP ? "TG" : "", dmrData.getDstId(),
                srcId, flco == FLCO::GROUP ? "TG" : "", dstId);

        if (m_verbose) {
            LogMessage(LOG_NET, "DMR Slot %u, VOICE_LC_HEADER, srcId = %u, dstId = %u, FLCO = $%02X, FID = $%02X, PF = %u", m_slot->m_slotNo, lc->getSrcId(), lc->getDstId(), lc->getFLCO(), lc->getFID(), lc->getPF());
        }

        m_slot->m_netLC = std::move(lc);

        // The standby LC data
        m_slot->m_voice->m_netEmbeddedLC.setLC(*m_slot->m_netLC);
        m_slot->m_voice->m_netEmbeddedData[0U].setLC(*m_slot->m_netLC);
        m_slot->m_voice->m_netEmbeddedData[1U].setLC(*m_slot->m_netLC);

        // Regenerate the LC data
        fullLC.encode(*m_slot->m_netLC, data + 2U, DataType::VOICE_LC_HEADER);

        // Regenerate the Slot Type
        SlotType slotType;
        slotType.setColorCode(m_slot->m_colorCode);
        slotType.setDataType(DataType::VOICE_LC_HEADER);
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
        m_slot->m_voice->m_netTalkerId = TalkerID::NONE;

        if (m_slot->m_duplex) {
            m_slot->m_txQueue.clear();
            m_slot->m_modem->writeDMRAbort(m_slot->m_slotNo);
        }

        for (uint32_t i = 0U; i < m_slot->m_jitterSlots; i++)
            m_slot->addFrame(m_slot->m_idle, true);

        if (m_slot->m_duplex) {
            for (uint32_t i = 0U; i < NO_HEADERS_DUPLEX; i++)
                m_slot->addFrame(data, true);
        }
        else {
            for (uint32_t i = 0U; i < NO_HEADERS_SIMPLEX; i++)
                m_slot->addFrame(data, true);
        }

        m_slot->m_netState = RS_NET_AUDIO;
        m_slot->m_netLastDstId = dstId;
        m_slot->m_netLastSrcId = srcId;
        m_slot->m_netTGHang.start();

        m_slot->setShortLC(m_slot->m_slotNo, dstId, flco, Slot::SLCO_ACT_TYPE::VOICE);

        if (m_verbose) {
            LogMessage(LOG_NET, DMR_DT_VOICE_LC_HEADER ", slot = %u, srcId = %u, dstId = %u, FLCO = $%02X, FID = $%02X, PF = %u", m_slot->m_slotNo,
                m_slot->m_netLC->getSrcId(), m_slot->m_netLC->getDstId(), m_slot->m_netLC->getFLCO(), m_slot->m_netLC->getFID(), m_slot->m_netLC->getPF());
        }

        ::ActivityLog("DMR", false, "Slot %u network voice header from %u to %s%u", m_slot->m_slotNo, srcId, flco == FLCO::GROUP ? "TG " : "", dstId);
    }
    else if (dataType == DataType::VOICE_PI_HEADER) {
        if (m_slot->m_netState != RS_NET_AUDIO) {
            std::unique_ptr<lc::LC> lc = std::make_unique<lc::LC>(dmrData.getFLCO(), dmrData.getSrcId(), dmrData.getDstId());

            uint32_t srcId = lc->getSrcId();
            uint32_t dstId = lc->getDstId();

            CHECK_NET_AUTHORITATIVE(dstId);
            CHECK_NET_TRAFFIC_COLLISION(dstId);

            m_slot->m_netLC = std::move(lc);

            m_slot->m_voice->m_lastFrameValid = false;

            m_slot->m_netTimeoutTimer.start();
            m_slot->m_netTimeout = false;

            if (m_slot->m_duplex) {
                m_slot->m_txQueue.clear();
                m_slot->m_modem->writeDMRAbort(m_slot->m_slotNo);
            }

            for (uint32_t i = 0U; i < m_slot->m_jitterSlots; i++)
                m_slot->addFrame(m_slot->m_idle, true);

            // Create a dummy start frame
            uint8_t start[DMR_FRAME_LENGTH_BYTES + 2U];

            Sync::addDMRDataSync(start + 2U, m_slot->m_duplex);

            lc::FullLC fullLC;
            fullLC.encode(*m_slot->m_netLC, start + 2U, DataType::VOICE_LC_HEADER);

            SlotType slotType;
            slotType.setColorCode(m_slot->m_colorCode);
            slotType.setDataType(DataType::VOICE_LC_HEADER);
            slotType.encode(start + 2U);

            start[0U] = modem::TAG_DATA;
            start[1U] = 0x00U;

            if (m_slot->m_duplex) {
                for (uint32_t i = 0U; i < NO_HEADERS_DUPLEX; i++)
                    m_slot->addFrame(start);
            }
            else {
                for (uint32_t i = 0U; i < NO_HEADERS_SIMPLEX; i++)
                    m_slot->addFrame(start);
            }

            m_slot->m_netFrames = 0U;
            m_slot->m_netLost = 0U;
            m_slot->m_netBits = 1U;
            m_slot->m_netErrs = 0U;

            m_slot->m_netState = RS_NET_AUDIO;
            m_slot->m_netLastDstId = dstId;
            m_slot->m_netLastSrcId = srcId;
            m_slot->m_netTGHang.start();

            m_slot->setShortLC(m_slot->m_slotNo, dstId, m_slot->m_netLC->getFLCO(), Slot::SLCO_ACT_TYPE::VOICE);

            ::ActivityLog("DMR", false, "Slot %u network late entry from %u to %s%u",
                m_slot->m_slotNo, srcId, m_slot->m_netLC->getFLCO() == FLCO::GROUP ? "TG " : "", dstId);
        }

        lc::FullLC fullLC;
        std::unique_ptr<lc::PrivacyLC> lc = fullLC.decodePI(data + 2U);
        if (lc == nullptr) {
            LogWarning(LOG_NET, "DMR Slot %u, VOICE_PI_HEADER, bad LC received, replacing", m_slot->m_slotNo);
            lc = std::make_unique<lc::PrivacyLC>();
            lc->setDstId(dmrData.getDstId());
        }

        m_slot->m_netPrivacyLC = std::move(lc);

        // Regenerate the LC data
        fullLC.encodePI(*m_slot->m_netPrivacyLC, data + 2U);

        // Regenerate the Slot Type
        SlotType slotType;
        slotType.setColorCode(m_slot->m_colorCode);
        slotType.setDataType(DataType::VOICE_PI_HEADER);
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        data[0U] = modem::TAG_DATA;
        data[1U] = 0x00U;

        m_slot->addFrame(data, true);

        if (m_verbose) {
            LogMessage(LOG_NET, DMR_DT_VOICE_PI_HEADER ", slot = %u, algId = %u, kId = %u, dstId = %u", m_slot->m_slotNo,
                m_slot->m_netPrivacyLC->getAlgId(), m_slot->m_netPrivacyLC->getKId(), m_slot->m_netPrivacyLC->getDstId());
        }
    }
    else if (dataType == DataType::VOICE_SYNC) {
        if (m_slot->m_netState == RS_NET_IDLE) {
            std::unique_ptr<lc::LC> lc = std::make_unique<lc::LC>(dmrData.getFLCO(), dmrData.getSrcId(), dmrData.getDstId());

            uint32_t dstId = lc->getDstId();
            uint32_t srcId = lc->getSrcId();

            CHECK_NET_AUTHORITATIVE(dstId);

            m_slot->m_netLC = std::move(lc);

            // The standby LC data
            m_netEmbeddedLC.setLC(*m_slot->m_netLC);
            m_netEmbeddedData[0U].setLC(*m_slot->m_netLC);
            m_netEmbeddedData[1U].setLC(*m_slot->m_netLC);

            m_lastFrameValid = false;

            m_slot->m_netTimeoutTimer.start();
            m_slot->m_netTimeout = false;

            if (m_slot->m_duplex) {
                m_slot->m_txQueue.clear();
                m_slot->m_modem->writeDMRAbort(m_slot->m_slotNo);
            }

            for (uint32_t i = 0U; i < m_slot->m_jitterSlots; i++)
                m_slot->addFrame(m_slot->m_idle, true);

            // Create a dummy start frame
            uint8_t start[DMR_FRAME_LENGTH_BYTES + 2U];

            Sync::addDMRDataSync(start + 2U, m_slot->m_duplex);

            lc::FullLC fullLC;
            fullLC.encode(*m_slot->m_netLC, start + 2U, DataType::VOICE_LC_HEADER);

            SlotType slotType;
            slotType.setColorCode(m_slot->m_colorCode);
            slotType.setDataType(DataType::VOICE_LC_HEADER);
            slotType.encode(start + 2U);

            start[0U] = modem::TAG_DATA;
            start[1U] = 0x00U;

            if (m_slot->m_duplex) {
                for (uint32_t i = 0U; i < NO_HEADERS_DUPLEX; i++)
                    m_slot->addFrame(start);
            }
            else {
                for (uint32_t i = 0U; i < NO_HEADERS_SIMPLEX; i++)
                    m_slot->addFrame(start);
            }

            m_slot->m_netFrames = 0U;
            m_slot->m_netLost = 0U;
            m_slot->m_netBits = 1U;
            m_slot->m_netErrs = 0U;

            m_netEmbeddedReadN = 0U;
            m_netEmbeddedWriteN = 1U;
            m_netTalkerId = TalkerID::NONE;

            m_slot->m_netState = RS_NET_AUDIO;
            m_slot->m_netLastDstId = dstId;
            m_slot->m_netLastSrcId = srcId;
            m_slot->m_netTGHang.start();

            m_slot->setShortLC(m_slot->m_slotNo, dstId, m_slot->m_netLC->getFLCO(), Slot::SLCO_ACT_TYPE::VOICE);

            ::ActivityLog("DMR", false, "Slot %u network late entry from %u to %s%u",
                m_slot->m_slotNo, srcId, m_slot->m_netLC->getFLCO() == FLCO::GROUP ? "TG " : "", dstId);
        }

        if (m_slot->m_netState == RS_NET_AUDIO) {
            uint8_t fid = m_slot->m_netLC->getFID();
            if (fid == FID_ETSI || fid == FID_DMRA) {
                m_slot->m_netErrs += m_fec.regenerateDMR(data + 2U);
                if (m_verbose) {
                    LogMessage(LOG_NET, "DMR Slot %u, VOICE_SYNC audio, sequence no = %u, errs = %u/141 (%.1f%%)",
                        m_slot->m_slotNo, m_netN, m_slot->m_netErrs, float(m_slot->m_netErrs) / 1.41F);
                }
            }

            if (m_netN >= 5U) {
                m_slot->m_netErrs = 0U;
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
                m_slot->addFrame(data, true);

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
    else if (dataType == DataType::VOICE) {
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
        m_slot->m_netTGHang.start();

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
            case FLCO::GROUP:
            case FLCO::PRIVATE:
                break;
            case FLCO::GPS_INFO:
                if (m_dumpTAData) {
                    ::sprintf(text, "DMR Slot %u, GPS_INFO (Embedded GPS Info)", m_slot->m_slotNo);
                    Utils::dump(2U, text, data, 9U);
                }

                logGPSPosition(m_slot->m_netLC->getSrcId(), data);
                break;
            case FLCO::TALKER_ALIAS_HEADER:
                if (!(m_netTalkerId & TalkerID::HEADER)) {
                    if (m_dumpTAData) {
                        ::sprintf(text, "DMR Slot %u, TALKER_ALIAS_HEADER (Embedded Talker Alias Header)", m_slot->m_slotNo);
                        Utils::dump(2U, text, data, 9U);
                    }

                    m_netTalkerId |= TalkerID::HEADER;
                }
                break;
            case FLCO::TALKER_ALIAS_BLOCK1:
                if (!(m_netTalkerId & TalkerID::BLOCK1)) {
                    if (m_dumpTAData) {
                        ::sprintf(text, "DMR Slot %u, TALKER_ALIAS_BLOCK1 (Embedded Talker Alias Block 1)", m_slot->m_slotNo);
                        Utils::dump(2U, text, data, 9U);
                    }

                    m_netTalkerId |= TalkerID::BLOCK1;
                }
                break;
            case FLCO::TALKER_ALIAS_BLOCK2:
                if (!(m_netTalkerId & TalkerID::BLOCK2)) {
                    if (m_dumpTAData) {
                        ::sprintf(text, "DMR Slot %u, TALKER_ALIAS_BLOCK2 (Embedded Talker Alias Block 2)", m_slot->m_slotNo);
                        Utils::dump(2U, text, data, 9U);
                    }

                    m_netTalkerId |= TalkerID::BLOCK2;
                }
                break;
            case FLCO::TALKER_ALIAS_BLOCK3:
                if (!(m_netTalkerId & TalkerID::BLOCK3)) {
                    if (m_dumpTAData) {
                        ::sprintf(text, "DMR Slot %u, TALKER_ALIAS_BLOCK3 (Embedded Talker Alias Block 3)", m_slot->m_slotNo);
                        Utils::dump(2U, text, data, 9U);
                    }

                    m_netTalkerId |= TalkerID::BLOCK3;
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
            if (m_netEmbeddedData[m_netEmbeddedReadN].valid())
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
                m_slot->addFrame(data, true);
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

/* Initializes a new instance of the Voice class. */

Voice::Voice(Slot* slot, network::BaseNetwork* network, bool embeddedLCOnly, bool dumpTAData, bool debug, bool verbose) :
    m_slot(slot),
    m_lastFrame(nullptr),
    m_lastFrameValid(false),
    m_rfN(0U),
    m_lastRfN(0U),
    m_netN(0U),
    m_rfEmbeddedLC(),
    m_rfEmbeddedData(nullptr),
    m_rfEmbeddedReadN(0U),
    m_rfEmbeddedWriteN(1U),
    m_netEmbeddedLC(),
    m_netEmbeddedData(nullptr),
    m_netEmbeddedReadN(0U),
    m_netEmbeddedWriteN(1U),
    m_rfTalkerId(TalkerID::NONE),
    m_netTalkerId(TalkerID::NONE),
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

/* Finalizes a instance of the Voice class. */

Voice::~Voice()
{
    delete[] m_lastFrame;

    delete[] m_rfEmbeddedData;
    delete[] m_netEmbeddedData;
}

/* */

void Voice::logGPSPosition(const uint32_t srcId, const uint8_t* data)
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

/* Helper to insert AMBE null frames for missing audio. */

void Voice::insertNullAudio(uint8_t* data)
{
    uint8_t* ambeBuffer = new uint8_t[DMR_AMBE_LENGTH_BYTES];
    ::memset(ambeBuffer, 0x00U, DMR_AMBE_LENGTH_BYTES);

    for (uint32_t i = 0; i < 3U; i++) {
        ::memcpy(ambeBuffer + (i * 9U), NULL_AMBE, 9U);
    }

    ::memcpy(data, ambeBuffer, 13U);
    data[13U] = ambeBuffer[13U] & 0xF0U;
    data[19U] = ambeBuffer[13U] & 0x0FU;
    ::memcpy(data + 20U, ambeBuffer + 14U, 13U);

    delete[] ambeBuffer;
}

/* Helper to insert DMR AMBE silence frames. */

bool Voice::insertSilence(const uint8_t* data, uint8_t seqNo)
{
    assert(data != nullptr);

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

/* Helper to insert DMR AMBE silence frames. */

void Voice::insertSilence(uint32_t count)
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
        ::memcpy(data, SILENCE_DATA, DMR_FRAME_LENGTH_BYTES + 2U);
    }

    uint8_t n = (m_netN + 1U) % 6U;

    uint8_t fid = m_slot->m_netLC->getFID();

    data::EMB emb;
    emb.setColorCode(m_slot->m_colorCode);

    for (uint32_t i = 0U; i < count; i++) {
        // only use our silence frame if its AMBE audio data
        if (fid == FID_ETSI || fid == FID_DMRA) {
            if (i > 0U) {
                ::memcpy(data, SILENCE_DATA, DMR_FRAME_LENGTH_BYTES + 2U);
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

        m_slot->addFrame(data, true);

        m_netN = n;

        m_slot->m_netFrames++;
        m_slot->m_netLost++;

        n = (n + 1U) % 6U;
    }
}
