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
#include "common/dmr/lc/FullLC.h"
#include "common/dmr/lc/ShortLC.h"
#include "common/dmr/lc/CSBK.h"
#include "common/dmr/SlotType.h"
#include "common/dmr/Sync.h"
#include "common/edac/CRC.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "dmr/Slot.h"
#include "common/dmr/acl/AccessControl.h"
#include "ActivityLog.h"
#include "HostMain.h"
#include "Host.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::packet;

#include <cassert>
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

Control* Slot::m_dmr = nullptr;

bool Slot::m_authoritative = true;

uint32_t Slot::m_colorCode = 0U;

SiteData Slot::m_siteData = SiteData();
uint32_t Slot::m_channelNo = 0U;

bool Slot::m_embeddedLCOnly = false;
bool Slot::m_dumpTAData = true;

modem::Modem* Slot::m_modem = nullptr;
network::Network* Slot::m_network = nullptr;

bool Slot::m_duplex = true;

::lookups::IdenTableLookup* Slot::m_idenTable = nullptr;
::lookups::RadioIdLookup* Slot::m_ridLookup = nullptr;
::lookups::TalkgroupRulesLookup* Slot::m_tidLookup = nullptr;
dmr::lookups::DMRAffiliationLookup *Slot::m_affiliations = nullptr;
::lookups::VoiceChData Slot::m_controlChData = ::lookups::VoiceChData();

::lookups::IdenTable Slot::m_idenEntry = ::lookups::IdenTable();

uint32_t Slot::m_hangCount = 3U * 17U;

::lookups::RSSIInterpolator* Slot::m_rssiMapper = nullptr;

uint32_t Slot::m_jitterTime = 360U;
uint32_t Slot::m_jitterSlots = 6U;

uint8_t* Slot::m_idle = nullptr;

FLCO::E Slot::m_flco1;
uint8_t Slot::m_id1 = 0U;
Slot::SLCO_ACT_TYPE Slot::m_actType1 = Slot::SLCO_ACT_TYPE::VOICE;
FLCO::E Slot::m_flco2;
uint8_t Slot::m_id2 = 0U;
Slot::SLCO_ACT_TYPE Slot::m_actType2 = Slot::SLCO_ACT_TYPE::VOICE;

bool Slot::m_verifyReg = false;

uint8_t Slot::m_alohaNRandWait = DEFAULT_NRAND_WAIT;
uint8_t Slot::m_alohaBackOff = 1U;

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t ADJ_SITE_TIMER_TIMEOUT = 60U;
const uint32_t ADJ_SITE_UPDATE_CNT = 5U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the Slot class. */

Slot::Slot(uint32_t slotNo, uint32_t timeout, uint32_t tgHang, uint32_t queueSize, bool dumpDataPacket, bool repeatDataPacket,
    bool dumpCSBKData, bool debug, bool verbose) :
    m_slotNo(slotNo),
    m_txImmQueue(queueSize, "DMR Imm Slot Frame"),
    m_txQueue(queueSize, "DMR Slot Frame"),
    m_queueLock(),
    m_rfState(RS_RF_LISTENING),
    m_rfLastDstId(0U),
    m_rfLastSrcId(0U),
    m_netState(RS_NET_IDLE),
    m_netLastDstId(0U),
    m_netLastSrcId(0U),
    m_permittedDstId(0U),
    m_rfLC(nullptr),
    m_rfPrivacyLC(nullptr),
    m_rfSeqNo(0U),
    m_netLC(nullptr),
    m_netPrivacyLC(nullptr),
    m_networkWatchdog(1000U, 0U, 1500U),
    m_rfTimeoutTimer(1000U, timeout),
    m_rfTGHang(1000U, tgHang),
    m_rfLossWatchdog(1000U, 0U, 1500U),
    m_netTimeoutTimer(1000U, timeout),
    m_netTGHang(1000U, 2U),
    m_packetTimer(1000U, 0U, 50U),
    m_adjSiteTable(),
    m_adjSiteUpdateCnt(),
    m_adjSiteUpdateTimer(1000U),
    m_adjSiteUpdateInterval(ADJ_SITE_TIMER_TIMEOUT),
    m_adjSiteUpdate(1000U, 75U),
    m_ccPacketInterval(1000U, 0U, DMR_SLOT_TIME),
    m_interval(),
    m_elapsed(),
    m_rfFrames(0U),
    m_netFrames(0U),
    m_netLost(0U),
    m_netMissed(0U),
    m_rfBits(1U),
    m_netBits(1U),
    m_rfErrs(0U),
    m_netErrs(0U),
    m_rfTimeout(false),
    m_netTimeout(false),
    m_rssi(0U),
    m_maxRSSI(0U),
    m_minRSSI(0U),
    m_aveRSSI(0U),
    m_rssiCount(0U),
    m_silenceThreshold(DEFAULT_SILENCE_THRESHOLD),
    m_frameLossCnt(0U),
    m_frameLossThreshold(DEFAULT_FRAME_LOSS_THRESHOLD),
    m_ccSeq(0U),
    m_ccRunning(false),
    m_ccPrevRunning(false),
    m_ccHalted(false),
    m_enableTSCC(false),
    m_dedicatedTSCC(false),
    m_ignoreAffiliationCheck(false),
    m_disableNetworkGrant(false),
    m_convNetGrantDemand(false),
    m_legacyGroupReg(false),
    m_defaultNetIdleTalkgroup(0U),
    m_tsccPayloadDstId(0U),
    m_tsccPayloadSrcId(0U),
    m_tsccPayloadGroup(false),
    m_tsccPayloadVoice(true),
    m_tsccPayloadActRetry(1000U, 0U, 250U),
    m_tsccAdjSSCnt(0U),
    m_disableGrantSrcIdCheck(false),
    m_lastLateEntry(0U),
    m_supervisor(false),
    m_notifyCC(true),
    m_ccDebug(debug),
    m_verbose(verbose),
    m_debug(debug)
{
    m_interval.start();

    m_adjSiteTable.clear();
    m_adjSiteUpdateCnt.clear();

    m_adjSiteUpdateInterval = ADJ_SITE_TIMER_TIMEOUT;
    m_adjSiteUpdateTimer.setTimeout(m_adjSiteUpdateInterval);
    m_adjSiteUpdateTimer.start();

    m_voice = new Voice(this, m_network, m_embeddedLCOnly, m_dumpTAData, debug, verbose);
    m_data = new Data(this, m_network, dumpDataPacket, repeatDataPacket, debug, verbose);
    m_control = new ControlSignaling(this, m_network, dumpCSBKData, debug, verbose);
}

/* Finalizes a instance of the Slot class. */

Slot::~Slot()
{
    delete m_voice;
    delete m_data;
    delete m_control;
}

/* Process DMR data frame from the RF interface. */

bool Slot::processFrame(uint8_t *data, uint32_t len)
{
    assert(data != nullptr);

    if (data[0U] == modem::TAG_LOST) {
        if (m_frameLossCnt > m_frameLossThreshold) {
            m_frameLossCnt = 0U;

            processFrameLoss();

            return false;
        }
        else {
            // increment the frame loss count by one for audio or data; otherwise drop
            // packets
            if (m_rfState == RS_RF_AUDIO || m_rfState == RS_RF_DATA) {
                m_rfLossWatchdog.start();
                ++m_frameLossCnt;
            }
            else {
                // drop normally
                m_frameLossCnt = 0U;
                m_rfState = RS_RF_LISTENING;

                m_rfLastDstId = 0U;
                m_rfLastSrcId = 0U;
                m_rfTGHang.stop();

                return false;
            }
        }
    }

    if (m_rfState == RS_RF_AUDIO || m_rfState == RS_RF_DATA) {
        if (m_rfLossWatchdog.isRunning()) {
            m_rfLossWatchdog.start();
        }
    }

    // Have we got RSSI bytes on the end?
    if (len == (DMR_FRAME_LENGTH_BYTES + 4U)) {
        uint16_t raw = 0U;
        raw |= (data[35U] << 8) & 0xFF00U;
        raw |= (data[36U] << 0) & 0x00FFU;

        // Convert the raw RSSI to dBm
        int rssi = m_rssiMapper->interpolate(raw);
        if (m_verbose) {
            LogMessage(LOG_RF, "DMR Slot %u, raw RSSI = %u, reported RSSI = %d dBm", m_slotNo, raw, rssi);
        }

        // RSSI is always reported as positive
        m_rssi = (rssi >= 0) ? rssi : -rssi;

        if (m_rssi > m_minRSSI)
            m_minRSSI = m_rssi;
        if (m_rssi < m_maxRSSI)
            m_maxRSSI = m_rssi;

        m_aveRSSI += m_rssi;
        m_rssiCount++;
    }

    bool dataSync = (data[1U] & SYNC_DATA) == SYNC_DATA;
    bool voiceSync = (data[1U] & SYNC_VOICE) == SYNC_VOICE;

    if (!(dataSync || voiceSync) && m_rfState == RS_RF_LISTENING) {
        uint8_t sync[DMR_SYNC_LENGTH_BYTES];
        ::memcpy(sync, data + 2U, DMR_SYNC_LENGTH_BYTES);

        // count data sync errors
        uint8_t dataErrs = 0U;
        for (uint8_t i = 0U; i < DMR_SYNC_LENGTH_BYTES; i++)
            dataErrs += Utils::countBits8(sync[i] ^ MS_DATA_SYNC_BYTES[i]);

        // count voice sync errors
        uint8_t voiceErrs = 0U;
        for (uint8_t i = 0U; i < DMR_SYNC_LENGTH_BYTES; i++)
            voiceErrs += Utils::countBits8(sync[i] ^ MS_VOICE_SYNC_BYTES[i]);

        LogWarning(LOG_RF, "DMR, possible sync word rejected, dataErrs = %u, voiceErrs = %u, sync word = %02X %02X %02X %02X %02X %02X", dataErrs, voiceErrs,
            sync[0U], sync[1U], sync[2U], sync[3U], sync[4U], sync[5U]);
    }

    if ((dataSync || voiceSync) && m_debug) {
        Utils::symbols("!!! *Rx DMR", data + 2U, len - 2U);
    }

    if ((dataSync || voiceSync) && m_rfState != RS_RF_LISTENING)
        m_rfTGHang.start();

    if (dataSync) {
        DataType::E dataType = (DataType::E)(data[1U] & 0x0FU);

        if (dataType == DataType::CSBK) {
            return m_control->process(data, len);
        }

        if (m_enableTSCC && m_dedicatedTSCC)
            return false;

        switch (dataType)
        {
        case DataType::VOICE_LC_HEADER:
        case DataType::VOICE_PI_HEADER:
            return m_voice->process(data, len);
        case DataType::TERMINATOR_WITH_LC:
            m_frameLossCnt = 0U;
        case DataType::DATA_HEADER:
        case DataType::RATE_12_DATA:
        case DataType::RATE_34_DATA:
        case DataType::RATE_1_DATA:
        default:
            return m_data->process(data, len);
        }
    }

    return m_voice->process(data, len);
}

/* Get the frame data length for the next frame in the data ring buffer. */

uint32_t Slot::peekFrameLength()
{
    std::lock_guard<std::mutex> lock(m_queueLock);

    if (m_txQueue.isEmpty() && m_txImmQueue.isEmpty())
        return 0U;

    uint8_t len = 0U;

    // tx immediate queue takes priority
    if (!m_txImmQueue.isEmpty()) {
        m_txImmQueue.peek(&len, 1U);
    }
    else {
        m_txQueue.peek(&len, 1U);
    }

    return len;
}

/* Helper to determine whether or not the internal frame queue is full. */

bool Slot::isQueueFull()
{
    if (m_txQueue.isEmpty() && m_txImmQueue.isEmpty())
        return false;

    // tx immediate queue takes priority
    if (!m_txImmQueue.isEmpty()) {
        uint32_t space = m_txImmQueue.freeSpace();
        if (space < (DMR_FRAME_LENGTH_BYTES + 1U))
            return true;
    }
    else {
        uint32_t space = m_txQueue.freeSpace();
        if (space < (DMR_FRAME_LENGTH_BYTES + 1U))
            return true;
    }

    return false;
}

/* Get frame data from data ring buffer. */

uint32_t Slot::getFrame(uint8_t* data)
{
    assert(data != nullptr);

    std::lock_guard<std::mutex> lock(m_queueLock);

    if (m_txQueue.isEmpty() && m_txImmQueue.isEmpty())
        return 0U;

    uint8_t len = 0U;

    // tx immediate queue takes priority
    if (!m_txImmQueue.isEmpty()) {
        m_txImmQueue.get(&len, 1U);
        m_txImmQueue.get(data, len);
    }
    else {
        m_txQueue.get(&len, 1U);
        m_txQueue.get(data, len);
    }

    return len;
}

/* Process a data frame from the network. */

void Slot::processNetwork(const data::NetData& dmrData)
{
    // don't process network frames if the RF modem isn't in a listening state
    if (m_rfState != RS_RF_LISTENING) {
        m_network->resetDMR(m_slotNo);
        return;
    }

    // don't process network frames if the destination ID's don't match and the network TG hang timer is running
    if (m_rfLastDstId != 0U) {
        if (m_rfLastDstId != dmrData.getDstId() && (m_rfTGHang.isRunning() && !m_rfTGHang.hasExpired())) {
            return;
        }

        if (m_rfLastDstId == dmrData.getDstId() && (m_rfTGHang.isRunning() && !m_rfTGHang.hasExpired())) {
            m_rfTGHang.start();
        }
    }

    if (m_authoritative) {
        // don't process network frames if the destination ID's don't match and the network TG hang timer is running
        if (m_netLastDstId != 0U && dmrData.getDstId() != 0U && m_netState != RS_NET_IDLE) {
            if (m_netLastDstId != dmrData.getDstId() && (m_netTGHang.isRunning() && !m_netTGHang.hasExpired())) {
                return;
            }

            if (m_netLastDstId == dmrData.getDstId() && (m_netTGHang.isRunning() && !m_netTGHang.hasExpired())) {
                m_netTGHang.start();
            }
        }
    }

    // don't process network frames if this modem isn't authoritative
    if (!m_authoritative && m_permittedDstId != dmrData.getDstId()) {
        if (!g_disableNonAuthoritativeLogging)
            LogWarning(LOG_NET, "DMR Slot %u, [NON-AUTHORITATIVE] Ignoring network traffic, destination not permitted!", m_slotNo);
        return;
    }

    m_networkWatchdog.start();

    DataType::E dataType = dmrData.getDataType();

    Slot* tscc = m_dmr->getTSCCSlot();

    bool enableTSCC = false;
    if (tscc != nullptr)
        enableTSCC = tscc->m_enableTSCC;
    bool dedicatedTSCC = false;
    if (tscc != nullptr)
        dedicatedTSCC = tscc->m_dedicatedTSCC;

    // check if this host instance is TSCC enabled or not -- if it is, handle processing network grant demands
    if (enableTSCC && dedicatedTSCC) {
        switch (dataType)
        {
        case DataType::VOICE_LC_HEADER:
        case DataType::DATA_HEADER:
            {
                bool grantDemand = (dmrData.getControl() & network::NET_CTRL_GRANT_DEMAND) == network::NET_CTRL_GRANT_DEMAND;
                bool unitToUnit = (dmrData.getControl() & network::NET_CTRL_U2U) == network::NET_CTRL_U2U;
                
                if (grantDemand) {
                    if (m_disableNetworkGrant) {
                        break;
                    }

                    // if we're non-dedicated control, and if we're not in a listening or idle state, ignore any grant
                    // demands
                    if (!dedicatedTSCC && (m_rfState != RS_RF_LISTENING || m_netState != RS_NET_IDLE)) {
                        break;
                    }

                    // validate source RID
                    if (!acl::AccessControl::validateSrcId(dmrData.getSrcId())) {
                        break;
                    }

                    // validate the target ID, if the target is a talkgroup
                    if (!acl::AccessControl::validateTGId(dmrData.getSlotNo(), dmrData.getDstId())) {
                        break;
                    }

                    if (m_verbose) {
                        LogMessage(LOG_NET, "DMR Slot %u, remote grant demand, srcId = %u, dstId = %u, unitToUnit = %u",
                                   m_slotNo, dmrData.getSrcId(), dmrData.getDstId(), unitToUnit);
                    }

                    // perform grant response logic
                    if (dataType == DataType::VOICE_LC_HEADER)
                        tscc->m_control->writeRF_CSBK_Grant(dmrData.getSrcId(), dmrData.getDstId(), 4U, !unitToUnit, true);
                    if (dataType == DataType::DATA_HEADER)
                        tscc->m_control->writeRF_CSBK_Data_Grant(dmrData.getSrcId(), dmrData.getDstId(), 4U, !unitToUnit, true);
                }
            }
            break;
        default:
            break;
        }

        // if *this slot* is the TSCC slot, stop processing after this point
        if (m_enableTSCC && m_dedicatedTSCC)
        {
            if (dataType != DataType::CSBK)
                return;
            else {
                if (m_slotNo != m_dmr->m_tsccSlotNo)
                    return;
            }
        }
    }

    switch (dataType)
    {
    case DataType::CSBK:
        m_control->processNetwork(dmrData);
        break;
    case DataType::VOICE_LC_HEADER:
    case DataType::VOICE_PI_HEADER:
    case DataType::VOICE_SYNC:
    case DataType::VOICE:
        m_voice->processNetwork(dmrData);
        break;
    case DataType::TERMINATOR_WITH_LC:
    case DataType::DATA_HEADER:
    case DataType::RATE_12_DATA:
    case DataType::RATE_34_DATA:
    case DataType::RATE_1_DATA:
    default:
        m_data->processNetwork(dmrData);
        break;
    }
}

/* Helper to process an In-Call Control message. */

void Slot::processInCallCtrl(network::NET_ICC::ENUM command, uint32_t dstId)
{
    switch (command) {
    case network::NET_ICC::REJECT_TRAFFIC:
        {
            if (m_rfState == RS_RF_AUDIO && m_rfLC->getDstId() == dstId) {
                LogWarning(LOG_DMR, "Slot %u, network requested in-call traffic reject, dstId = %u", m_slotNo, dstId);
                if (m_affiliations->isGranted(dstId)) {
                    m_affiliations->releaseGrant(dstId, false);
                    if (!m_enableTSCC) {
                        notifyCC_ReleaseGrant(dstId);
                    }
                }

                processFrameLoss();

                m_rfLastDstId = 0U;
                m_rfLastSrcId = 0U;
                m_rfTGHang.stop();
                m_rfState = RS_RF_REJECTED;
            }
        }
        break;

    default:
        break;
    }
}

/* Updates the DMR slot processor. */

void Slot::clock()
{
    uint32_t ms = m_interval.elapsed();
    m_interval.start();

    if (m_network != nullptr) {
        if (m_network->getStatus() == network::NET_STAT_RUNNING) {
            m_siteData.setNetActive(true);
        }
        else {
            m_siteData.setNetActive(false);
        }

        lc::CSBK::setSiteData(m_siteData);
    }

    // if we have control enabled; do clocking to generate a CC data stream
    if (m_enableTSCC) {
        m_dmr->m_tsccCntInterval.clock(ms);
        if (m_dmr->m_tsccCntInterval.isRunning() && m_dmr->m_tsccCntInterval.hasExpired()) {
            m_dmr->m_tsccCnt++;
            if (m_dmr->m_tsccCnt == TSCC_MAX_CSC_CNT) {
                m_dmr->m_tsccCnt = 0U;
            }

            m_dmr->m_tsccCntInterval.start();
        }

        m_modem->setDMRIgnoreCACH_AT(m_slotNo);

        if (m_ccRunning && !m_ccPacketInterval.isRunning()) {
            m_ccPacketInterval.start();
        }

        if (m_ccHalted) {
            if (!m_ccRunning) {
                m_ccHalted = false;
                m_ccPrevRunning = m_ccRunning;
                m_txQueue.clear(); // clear the frame buffer
            }
        }
        else {
            m_ccPacketInterval.clock(ms);
            if (!m_ccPacketInterval.isRunning()) {
                m_ccPacketInterval.start();
            }

            if (m_ccPacketInterval.isRunning() && m_ccPacketInterval.hasExpired()) {
                if (m_ccRunning) {
                    if (m_ccSeq == 4U) {
                        m_ccSeq = 0U;
                    }

                    if (m_dmr->m_tsccPayloadActive) {
                        if ((m_dmr->m_tsccCnt % 2) == 0) {
                            setShortLC_Payload(m_siteData, m_dmr->m_tsccCnt);
                        }
                    }
                    else {
                        setShortLC_TSCC(m_siteData, m_dmr->m_tsccCnt);
                    }

                    writeRF_ControlData(m_dmr->m_tsccCnt, m_ccSeq);

                    m_ccSeq++;
                }

                m_ccPacketInterval.start();
            }
        }

        if (m_ccPrevRunning && !m_ccRunning) {
            m_txQueue.clear(); // clear the frame buffer
            m_ccPrevRunning = m_ccRunning;
        }
    }

    // activate payload channel if requested from the TSCC
    if (m_dmr->m_tsccPayloadActive) {
        if (m_rfState == RS_RF_LISTENING && m_netState == RS_NET_IDLE) {
            if (m_tsccPayloadDstId > 0U) {
                if (m_tsccPayloadActRetry.isRunning()) {
                    m_tsccPayloadActRetry.clock(ms);

                    if (m_tsccPayloadActRetry.hasExpired()) {
                        m_control->writeRF_CSBK_Payload_Activate(m_tsccPayloadDstId, m_tsccPayloadSrcId, m_tsccPayloadGroup, m_tsccPayloadVoice, true);
                        m_tsccPayloadActRetry.start(0U, 500U);
                    }
                }

                if ((m_dmr->m_tsccCnt % 2) > 0) {
                    if (m_tsccPayloadVoice)
                        setShortLC(m_slotNo, m_tsccPayloadDstId, m_tsccPayloadGroup ? FLCO::GROUP : FLCO::PRIVATE, SLCO_ACT_TYPE::VOICE);
                    else                        
                        setShortLC(m_slotNo, m_tsccPayloadDstId, m_tsccPayloadGroup ? FLCO::GROUP : FLCO::PRIVATE, SLCO_ACT_TYPE::DATA);
                }
            }
        }
    }

    // handle timeouts and hang timers
    m_rfTimeoutTimer.clock(ms);
    m_netTimeoutTimer.clock(ms);

    if (m_rfTimeoutTimer.isRunning() && m_rfTimeoutTimer.hasExpired()) {
        if (!m_rfTimeout) {
            LogMessage(LOG_RF, "DMR Slot %u, user has timed out", m_slotNo);
            m_rfTimeout = true;
        }
    }

    if (m_rfState == RS_RF_AUDIO || m_rfState == RS_RF_DATA) {
        if (m_rfLossWatchdog.isRunning()) {
            m_rfLossWatchdog.clock(ms);

            if (m_rfLossWatchdog.hasExpired()) {
                m_rfLossWatchdog.stop();

                processFrameLoss();
            }
        }
    }

    if (m_rfTGHang.isRunning()) {
        m_rfTGHang.clock(ms);

        if (m_rfTGHang.hasExpired()) {
            m_rfTGHang.stop();
            if (m_verbose) {
                LogMessage(LOG_RF, "Slot %u, talkgroup hang has expired, lastDstId = %u", m_slotNo, m_rfLastDstId);
            }
            m_rfLastDstId = 0U;
            m_rfLastSrcId = 0U;

            // reset permitted ID and clear permission state
            if (!m_authoritative && m_permittedDstId != 0U) {
                m_permittedDstId = 0U;
            }
        }
    }

    if (m_netTimeoutTimer.isRunning() && m_netTimeoutTimer.hasExpired()) {
        if (!m_netTimeout) {
            LogMessage(LOG_NET, "DMR Slot %u, user has timed out", m_slotNo);
            m_netTimeout = true;
        }
    }

    if (m_authoritative) {
        if (m_netTGHang.isRunning()) {
            m_netTGHang.clock(ms);

            if (m_netTGHang.hasExpired()) {
                m_netTGHang.stop();
                if (m_verbose) {
                    LogMessage(LOG_NET, "Slot %u, talkgroup hang has expired, lastDstId = %u", m_slotNo, m_netLastDstId);
                }
                m_netLastDstId = 0U;
                m_netLastSrcId = 0U;
            }
        }
    }
    else {
        m_netTGHang.stop();
    }

    if (m_netState == RS_NET_AUDIO || m_netState == RS_NET_DATA) {
        m_networkWatchdog.clock(ms);

        if (m_networkWatchdog.hasExpired()) {
            if (m_netState == RS_NET_AUDIO) {
                // We've received the voice header haven't we?
                m_netFrames += 1U;
                ::ActivityLog("DMR", false, "Slot %u network watchdog has expired, %.1f seconds, %u%% packet loss, BER: %.1f%%",
                    m_slotNo, float(m_netFrames) / 16.667F, (m_netLost * 100U) / m_netFrames, float(m_netErrs * 100U) / float(m_netBits));
                writeEndNet(true);
            }
            else {
                ::ActivityLog("DMR", false, "Slot %u network watchdog has expired", m_slotNo);
                writeEndNet();
            }
        }
    }

    if (m_netState == RS_NET_AUDIO) {
        m_packetTimer.clock(ms);

        if (m_packetTimer.isRunning() && m_packetTimer.hasExpired()) {
            uint32_t elapsed = m_elapsed.elapsed();
            if (elapsed >= m_jitterTime) {
                LogWarning(LOG_NET, "DMR Slot %u, lost audio for %ums filling in", m_slotNo, elapsed);
                m_voice->insertSilence(m_jitterSlots);
                m_elapsed.start();
            }

            m_packetTimer.start();
        }
    }

    // reset states if we're in a rejected state and we're a control channel
    if (m_rfState == RS_RF_REJECTED && m_enableTSCC) {
        clearRFReject();
    }

    if (m_frameLossCnt > 0U && m_rfState == RS_RF_LISTENING)
        m_frameLossCnt = 0U;
    if (m_frameLossCnt >= m_frameLossThreshold && (m_rfState == RS_RF_AUDIO || m_rfState == RS_RF_DATA)) {
        processFrameLoss();
    }
}

/* Updates the adj. site tables. */

void Slot::clockSiteData(uint32_t ms)
{
    if (m_enableTSCC) {
        // clock all the grant timers
        m_affiliations->clock(ms);

        // do we need to network announce ourselves?
        if (!m_adjSiteUpdateTimer.isRunning()) {
            m_control->writeAdjSSNetwork();
            m_adjSiteUpdateTimer.start();
        }

        m_adjSiteUpdateTimer.clock(ms);
        if (m_adjSiteUpdateTimer.isRunning() && m_adjSiteUpdateTimer.hasExpired()) {
            if (m_rfState == RS_RF_LISTENING && m_netState == RS_NET_IDLE) {
                m_control->writeAdjSSNetwork();
                if (m_network != nullptr) {
                    if (m_affiliations->grpAffSize() > 0) {
                        auto affs = m_affiliations->grpAffTable();
                        m_network->announceAffiliationUpdate(affs);
                    }
                }
                m_adjSiteUpdateTimer.start();
            }
        }

        // clock adjacent site update timers
        m_adjSiteUpdateTimer.clock(ms);
        if (m_adjSiteUpdateTimer.isRunning() && m_adjSiteUpdateTimer.hasExpired()) {
            // update adjacent site data
            for (auto& entry : m_adjSiteUpdateCnt) {
                uint8_t siteId = entry.first;
                uint8_t updateCnt = entry.second;
                if (updateCnt > 0U) {
                    updateCnt--;
                }

                if (updateCnt == 0U) {
                    AdjSiteData siteData = m_adjSiteTable[siteId];
                    LogWarning(LOG_NET, "DMR, Adjacent Site Status Expired, no data [FAILED], sysId = $%03X, chNo = %u",
                        siteData.systemIdentity, siteData.channelNo);
                }

                entry.second = updateCnt;
            }

            m_adjSiteUpdateTimer.setTimeout(m_adjSiteUpdateInterval);
            m_adjSiteUpdateTimer.start();
        }
    }
}

/* Permits a TGID on a non-authoritative host. */

void Slot::permittedTG(uint32_t dstId)
{
    if (m_authoritative) {
        return;
    }

    if (m_verbose) {
        if (dstId == 0U)
            LogMessage(LOG_DMR, "DMR Slot %u, non-authoritative TG unpermit", m_slotNo);
        else
            LogMessage(LOG_DMR, "DMR Slot %u, non-authoritative TG permit, dstId = %u", m_slotNo, dstId);
    }

    m_permittedDstId = dstId;
}

/* Grants a TGID on a non-authoritative host. */

void Slot::grantTG(uint32_t srcId, uint32_t dstId, bool grp)
{
    if (!m_control) {
        return;
    }

    if (m_verbose) {
        LogMessage(LOG_DMR, "DMR Slot %u, network TG grant demand, srcId = %u, dstId = %u", m_slotNo, srcId, dstId);
    }

    m_control->writeRF_CSBK_Grant(srcId, dstId, 4U, grp);
}

/* Releases a granted TG. */

void Slot::releaseGrantTG(uint32_t dstId)
{
    if (!m_control) {
        return;
    }

    if (m_verbose) {
        LogMessage(LOG_DMR, "DMR Slot %u, VC request, release TG grant, dstId = %u", m_slotNo, dstId);
    }

    if (m_affiliations->isGranted(dstId)) {
        uint32_t chNo = m_affiliations->getGrantedCh(dstId);
        uint32_t srcId = m_affiliations->getGrantedSrcId(dstId);
        ::lookups::VoiceChData voiceCh = m_affiliations->rfCh()->getRFChData(chNo);

        if (m_verbose) {
            LogMessage(LOG_DMR, "DMR Slot %u, VC %s:%u, TG grant released, srcId = %u, dstId = %u, chNo = %u-%u", m_slotNo, voiceCh.address().c_str(), voiceCh.port(), srcId, dstId, voiceCh.chId(), chNo);
        }
    
        m_affiliations->releaseGrant(dstId, false);
    }
}

/* Touches a granted TG to keep a channel grant alive. */

void Slot::touchGrantTG(uint32_t dstId)
{
    if (!m_control) {
        return;
    }

    if (m_affiliations->isGranted(dstId)) {
        uint32_t chNo = m_affiliations->getGrantedCh(dstId);
        uint32_t srcId = m_affiliations->getGrantedSrcId(dstId);
        ::lookups::VoiceChData voiceCh = m_affiliations->rfCh()->getRFChData(chNo);

        if (m_verbose) {
            LogMessage(LOG_DMR, "DMR Slot %u, VC %s:%u, call in progress, srcId = %u, dstId = %u, chNo = %u-%u", m_slotNo, voiceCh.address().c_str(), voiceCh.port(), srcId, dstId, voiceCh.chId(), chNo);
        }

        m_affiliations->touchGrant(dstId);
    }
}

/* Clears the current operating RF state back to idle. */

void Slot::clearRFReject()
{
    if (m_rfState == RS_RF_REJECTED) {
        if (!m_enableTSCC) {
            m_txQueue.clear();
        }

        m_rfFrames = 0U;
        m_rfErrs = 0U;
        m_rfBits = 1U;

        m_netFrames = 0U;
        m_netLost = 0U;

        if (m_network != nullptr)
            m_network->resetDMR(m_slotNo);

        m_rfState = RS_RF_LISTENING;
    }
}

/* Helper to change the debug and verbose state. */

void Slot::setDebugVerbose(bool debug, bool verbose)
{
    m_debug = m_voice->m_debug = m_data->m_debug = debug = m_control->m_debug;
    m_verbose = m_voice->m_verbose = m_data->m_verbose = verbose = m_control->m_verbose;
}

/* Helper to enable and configure TSCC support for this slot. */

void Slot::setTSCC(bool enable, bool dedicated)
{
    m_enableTSCC = enable;
    m_dedicatedTSCC = dedicated;
    if (m_enableTSCC) {
        m_modem->setDMRIgnoreCACH_AT(m_slotNo);
        m_affiliations->setSlotForChannelTSCC(m_channelNo, m_slotNo);
    }
}

/* Helper to activate a TSCC payload slot. */

void Slot::setTSCCActivated(uint32_t dstId, uint32_t srcId, bool group, bool voice)
{
    m_tsccPayloadDstId = dstId;
    m_tsccPayloadSrcId = srcId;
    m_tsccPayloadGroup = group;
    m_tsccPayloadVoice = voice;

    // start payload channel transmit
    if (!m_modem->hasTX()) {
        m_modem->writeDMRStart(true);
    }

    if (m_tsccPayloadDstId != 0U && !m_tsccPayloadActRetry.isRunning()) {
        m_tsccPayloadActRetry.start();
    }
}

/* Helper to get the last transmitted destination ID. */

uint32_t Slot::getLastDstId() const
{
    if (m_rfLastDstId != 0U) {
        return m_rfLastDstId;
    }

    if (m_netLastDstId != 0U) {
        return m_netLastDstId;
    }

    return 0U;
}

/* Helper to get the last transmitted source ID. */

uint32_t Slot::getLastSrcId() const
{
    if (m_rfLastSrcId != 0U) {
        return m_rfLastSrcId;
    }

    if (m_netLastSrcId != 0U) {
        return m_netLastSrcId;
    }

    return 0U;
}

/* Helper to initialize the DMR slot processor. */

void Slot::init(Control* dmr, bool authoritative, uint32_t colorCode, SiteData siteData, bool embeddedLCOnly, bool dumpTAData, uint32_t callHang, modem::Modem* modem,
    network::Network* network, bool duplex, ::lookups::ChannelLookup* chLookup, ::lookups::RadioIdLookup* ridLookup, ::lookups::TalkgroupRulesLookup* tidLookup,
    ::lookups::IdenTableLookup* idenTable, ::lookups::RSSIInterpolator* rssiMapper, uint32_t jitter, bool verbose)
{
    assert(dmr != nullptr);
    assert(modem != nullptr);
    assert(chLookup != nullptr);
    assert(ridLookup != nullptr);
    assert(tidLookup != nullptr);
    assert(idenTable != nullptr);
    assert(rssiMapper != nullptr);

    m_dmr = dmr;

    m_authoritative = authoritative;

    m_colorCode = colorCode;

    m_siteData = siteData;

    m_embeddedLCOnly = embeddedLCOnly;
    m_dumpTAData = dumpTAData;

    m_modem = modem;
    m_network = network;

    m_duplex = duplex;

    m_idenTable = idenTable;
    m_ridLookup = ridLookup;
    m_tidLookup = tidLookup;
    m_affiliations = new dmr::lookups::DMRAffiliationLookup(chLookup, verbose);

    // set the grant release callback
    m_affiliations->setReleaseGrantCallback([=](uint32_t chNo, uint32_t dstId, uint8_t slot) {
        Slot* tscc = m_dmr->getTSCCSlot();
        if (tscc != nullptr) {
            if (chNo == tscc->m_channelNo) {
                m_dmr->tsccClearActivatedSlot(slot);
                return;
            }

            ::lookups::VoiceChData voiceChData = tscc->m_affiliations->rfCh()->getRFChData(chNo);
            if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0) {
                json::object req = json::object();
                req["slot"].set<uint8_t>(slot);
                bool clear = true;
                req["clear"].set<bool>(clear);

                g_RPC->req(RPC_DMR_TSCC_PAYLOAD_ACT, req, nullptr, voiceChData.address(), voiceChData.port());
            }
            else {
                ::LogError(LOG_DMR, "DMR Slot %u, CSBK, RAND (Random Access), failed to clear payload channel, chNo = %u, slot = %u", tscc->m_slotNo, chNo, slot);
            }

            // callback REST API to clear TG permit for the granted TG on the specified voice channel
            if (m_authoritative && m_dmr->m_supervisor) {
                if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0) {
                    json::object req = json::object();
                    dstId = 0U; // clear TG value
                    req["dstId"].set<uint32_t>(dstId);
                    req["slot"].set<uint8_t>(slot);

                    g_RPC->req(RPC_PERMIT_DMR_TG, req, nullptr, voiceChData.address(), voiceChData.port());
                }
                else {
                    ::LogError(LOG_DMR, "DMR Slot %u, CSBK, RAND (Random Access), failed to clear TG permit, chNo = %u, slot = %u", tscc->m_slotNo, chNo, slot);
                }
            }
        }
    });

    // set the unit deregistration callback
    m_affiliations->setUnitDeregCallback([=](uint32_t srcId, bool automatic) {
        if (m_network != nullptr)
            m_network->announceUnitDeregistration(srcId);
    });

    m_hangCount = callHang * 17U;

    m_rssiMapper = rssiMapper;

    m_jitterTime = jitter;

    float jitter_tmp = float(jitter) / 360.0F;
    m_jitterSlots = (uint32_t)(std::ceil(jitter_tmp) * 6.0F);

    m_idle = new uint8_t[DMR_FRAME_LENGTH_BYTES + 2U];
    ::memcpy(m_idle, IDLE_DATA, DMR_FRAME_LENGTH_BYTES + 2U);

    // Generate the Slot Type for the Idle frame
    SlotType slotType;
    slotType.setColorCode(colorCode);
    slotType.setDataType(DataType::IDLE);
    slotType.encode(m_idle + 2U);
}

/* Sets local configured site data. */

void Slot::setSiteData(::lookups::VoiceChData controlChData, uint32_t netId, uint8_t siteId, uint8_t channelId, uint32_t channelNo, bool requireReg)
{
    m_siteData = SiteData(SiteModel::SM_SMALL, netId, siteId, 3U, requireReg);
    m_channelNo = channelNo;

    std::vector<::lookups::IdenTable> entries = m_idenTable->list();
    for (auto entry : entries) {
        if (entry.channelId() == channelId) {
            m_idenEntry = entry;
            break;
        }
    }

    m_controlChData = controlChData;

    lc::CSBK::setSiteData(m_siteData);
}

/* Sets TSCC Aloha configuration. */

void Slot::setAlohaConfig(uint8_t nRandWait, uint8_t backOff)
{
    m_alohaNRandWait = nRandWait;
    m_alohaBackOff = backOff;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Add data frame to the data ring buffer. */

void Slot::addFrame(const uint8_t *data, bool net, bool imm)
{
    assert(data != nullptr);

    std::lock_guard<std::mutex> lock(m_queueLock);

    if (!net) {
        if (m_netState != RS_NET_IDLE)
            return;
    }

    uint8_t len = DMR_FRAME_LENGTH_BYTES + 2U;
    if (m_debug) {
        Utils::symbols("!!! *Tx DMR", data + 2U, len - 2U);
    }

    uint32_t fifoSpace = 0U;
    if (m_slotNo == 1U) {
        fifoSpace = m_modem->getDMRSpace1();
    } else {
        fifoSpace = m_modem->getDMRSpace2();
    }

    //LogDebugEx(LOG_DMR, "Slot::addFrame()", "Slot %u, fifoSpace = %u", m_slotNo, fifoSpace);

    // is this immediate data?
    if (imm) {
        // resize immediate queue if necessary (this shouldn't really ever happen)
        uint32_t space = m_txImmQueue.freeSpace();
        if (space < (len + 1U)) {
            if (!net) {
                uint32_t queueLen = m_txImmQueue.length();
                m_txImmQueue.resize(queueLen + len);
                LogError(LOG_DMR, "Slot %u, overflow in the imm DMR slot queue; queue free is %u, needed %u; resized was %u is %u, fifoSpace = %u", m_slotNo, space, len, queueLen, m_txQueue.length(), fifoSpace);
                return;
            }
            else {
                LogError(LOG_DMR, "Slot %u, overflow in the imm DMR slot queue while writing network data; queue free is %u, needed %u, fifoSpace = %u", m_slotNo, space, len, fifoSpace);
                return;
            }
        }

        m_txImmQueue.addData(&len, 1U);
        m_txImmQueue.addData(data, len);
        return;
    }

    uint32_t space = m_txQueue.freeSpace();
    if (space < (len + 1U)) {
        if (!net) {
            uint32_t queueLen = m_txQueue.length();
            m_txQueue.resize(queueLen + (DMR_FRAME_LENGTH_BYTES + 2U));
            LogError(LOG_DMR, "Slot %u, overflow in the DMR slot queue; queue free is %u, needed %u; resized was %u is %u, fifoSpace = %u", m_slotNo, space, len, queueLen, m_txQueue.length(), fifoSpace);
            return;
        }
        else {
            LogError(LOG_DMR, "Slot %u, overflow in the DMR slot queue while writing network data; queue free is %u, needed %u, fifoSpace = %u", m_slotNo, space, len, fifoSpace);
            return;
        }
    }

    m_txQueue.addData(&len, 1U);
    m_txQueue.addData(data, len);
}

/* Helper to process loss of frame stream from modem. */

void Slot::processFrameLoss()
{
    if (m_rfState == RS_RF_AUDIO) {
        if (m_rssi != 0U) {
            ::ActivityLog("DMR", true, "Slot %u RF voice transmission lost, %.1f seconds, BER: %.1f%%, RSSI: -%u/-%u/-%u dBm, loss count: %u",
                m_slotNo, float(m_rfFrames) / 16.667F, float(m_rfErrs * 100U) / float(m_rfBits), m_minRSSI, m_maxRSSI, m_aveRSSI / m_rssiCount, m_frameLossCnt);
        }
        else {
            ::ActivityLog("DMR", true, "Slot %u RF voice transmission lost, %.1f seconds, BER: %.1f%%, loss count: %u",
                m_slotNo, float(m_rfFrames) / 16.667F, float(m_rfErrs * 100U) / float(m_rfBits), m_frameLossCnt);
        }

        LogMessage(LOG_RF, "DMR Slot %u, total frames: %d, total bits: %d, errors: %d, BER: %.4f%%",
            m_slotNo, m_rfFrames, m_rfBits, m_rfErrs, float(m_rfErrs * 100U) / float(m_rfBits));

        // release trunked grant (if necessary)
        Slot* tscc = m_dmr->getTSCCSlot();
        if (tscc != nullptr) {
            if (tscc->m_enableTSCC && m_rfLC != nullptr) {
                tscc->m_affiliations->releaseGrant(m_rfLC->getDstId(), false);
            }
            
            clearTSCCActivated();

            if (!tscc->m_enableTSCC) {
                notifyCC_ReleaseGrant(m_rfLC->getDstId());
            }
        }

        if (m_rfTimeout) {
            writeEndRF();
        }
        else {
            writeEndRF(true);
        }
    }

    if (m_rfState == RS_RF_DATA) {
        ::ActivityLog("DMR", true, "Slot %u, RF data transmission lost", m_slotNo);
        writeEndRF();
    }

    m_rfState = RS_RF_LISTENING;

    m_rfLastDstId = 0U;
    m_rfTGHang.stop();
}

/* Helper to send a REST API request to the CC to release a channel grant at the end of a call. */

void Slot::notifyCC_ReleaseGrant(uint32_t dstId)
{
    if (m_controlChData.address().empty()) {
        return;
    }

    if (m_controlChData.port() == 0) {
        return;
    }

    if (!m_notifyCC) {
        return;
    }

    if (m_verbose) {
        LogMessage(LOG_DMR, "DMR Slot %u, CC %s:%u, notifying CC of call termination, dstId = %u", m_slotNo, m_controlChData.address().c_str(), m_controlChData.port(), dstId);
    }

    // callback REST API to release the granted TG on the specified control channel
    json::object req = json::object();
    req["dstId"].set<uint32_t>(dstId);
    uint8_t slot = m_slotNo;
    req["slot"].set<uint8_t>(slot);

    g_RPC->req(RPC_RELEASE_DMR_TG, req, [=](json::object& req, json::object& reply) {
        if (!req["status"].is<int>()) {
            ::LogError(LOG_DMR, "DMR Slot %u, failed to notify the CC %s:%u of the release of, dstId = %u, invalid RPC response", m_slotNo, m_controlChData.address().c_str(), m_controlChData.port(), dstId);
            return;
        }

        int status = req["status"].get<int>();
        if (status != network::NetRPC::OK) {
            ::LogError(LOG_DMR, "DMR Slot %u, failed to notify the CC %s:%u of the release of, dstId = %u", m_slotNo, m_controlChData.address().c_str(), m_controlChData.port(), dstId);
            if (req["message"].is<std::string>()) {
                std::string retMsg = req["message"].get<std::string>();
                ::LogError(LOG_DMR, "DMR Slot %u, RPC failed, %s", m_slotNo, retMsg.c_str());
            }
        }
        else
            ::LogMessage(LOG_DMR, "DMR Slot %u, CC %s:%u, released grant, dstId = %u", m_slotNo, m_controlChData.address().c_str(), m_controlChData.port(), dstId);
    }, m_controlChData.address(), m_controlChData.port());

    m_rfLastDstId = 0U;
    m_rfLastSrcId = 0U;
    m_netLastDstId = 0U;
    m_netLastSrcId = 0U;
}

/* Helper to send a REST API request to the CC to "touch" a channel grant to refresh grant timers. */

void Slot::notifyCC_TouchGrant(uint32_t dstId)
{
    if (m_controlChData.address().empty()) {
        return;
    }

    if (m_controlChData.port() == 0) {
        return;
    }

    if (!m_notifyCC) {
        return;
    }

    // callback REST API to touch the granted TG on the specified control channel
    json::object req = json::object();
    req["dstId"].set<uint32_t>(dstId);
    uint8_t slot = m_slotNo;
    req["slot"].set<uint8_t>(slot);

    g_RPC->req(RPC_TOUCH_DMR_TG, req, [=](json::object& req, json::object& reply) {
        // validate channelNo is a string within the JSON blob
        if (!req["status"].is<int>()) {
            ::LogError(LOG_DMR, "DMR Slot %u, failed to notify the CC %s:%u of the touch of, dstId = %u, invalid RPC response", m_slotNo, m_controlChData.address().c_str(), m_controlChData.port(), dstId);
            return;
        }

        int status = req["status"].get<int>();
        if (status != network::NetRPC::OK) {
            ::LogError(LOG_DMR, "DMR Slot %u, failed to notify the CC %s:%u of the touch of, dstId = %u", m_slotNo, m_controlChData.address().c_str(), m_controlChData.port(), dstId);
            if (req["message"].is<std::string>()) {
                std::string retMsg = req["message"].get<std::string>();
                ::LogError(LOG_DMR, "DMR Slot %u, RPC failed, %s", m_slotNo, retMsg.c_str());
            }
        }
        else
            ::LogMessage(LOG_DMR, "DMR Slot %u, CC %s:%u, touched grant, dstId = %u", m_slotNo, m_controlChData.address().c_str(), m_controlChData.port(), dstId);
    }, m_controlChData.address(), m_controlChData.port());
}

/* Write data frame to the network. */

void Slot::writeNetwork(const uint8_t* data, DataType::E dataType, uint8_t control, uint8_t errors, bool noSequence)
{
    assert(data != nullptr);
    assert(m_rfLC != nullptr);

    writeNetwork(data, dataType, m_rfLC->getFLCO(), m_rfLC->getSrcId(), m_rfLC->getDstId(), control, errors);
}

/* Write data frame to the network. */

void Slot::writeNetwork(const uint8_t* data, DataType::E dataType, FLCO::E flco, uint32_t srcId,
    uint32_t dstId, uint8_t control, uint8_t errors, bool noSequence)
{
    assert(data != nullptr);

    if (m_netState != RS_NET_IDLE)
        return;

    if (m_network == nullptr)
        return;

    data::NetData dmrData;
    dmrData.setSlotNo(m_slotNo);
    dmrData.setDataType(dataType);
    dmrData.setSrcId(srcId);
    dmrData.setDstId(dstId);
    dmrData.setFLCO(flco);
    dmrData.setControl(control);
    dmrData.setN(m_voice->m_rfN);
    dmrData.setSeqNo(m_rfSeqNo);
    dmrData.setBER(errors);
    dmrData.setRSSI(m_rssi);

    m_rfSeqNo++;

    dmrData.setData(data + 2U);

    m_network->writeDMR(dmrData, noSequence);
}

/* Helper to write RF end of frame data. */

void Slot::writeEndRF(bool writeEnd)
{
    m_rfState = RS_RF_LISTENING;

    if (m_netState == RS_NET_IDLE) {
        if (m_enableTSCC)
            setShortLC_Payload(m_siteData, m_dmr->m_tsccCnt);
        else
            setShortLC(m_slotNo, 0U);
    }

    if (writeEnd) {
        if (m_netState == RS_NET_IDLE && m_duplex && !m_rfTimeout) {
            // Create a dummy start end frame
            uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];

            Sync::addDMRDataSync(data + 2U, m_duplex);

            lc::FullLC fullLC;
            fullLC.encode(*m_rfLC, data + 2U, DataType::TERMINATOR_WITH_LC);

            SlotType slotType;
            slotType.setColorCode(m_colorCode);
            slotType.setDataType(DataType::TERMINATOR_WITH_LC);
            slotType.encode(data + 2U);

            data[0U] = modem::TAG_EOT;
            data[1U] = 0x00U;

            for (uint32_t i = 0U; i < m_hangCount; i++)
                addFrame(data);
        }
    }

    m_data->m_pduDataOffset = 0U;

    if (m_network != nullptr)
        m_network->resetDMR(m_slotNo);

    m_rfTimeoutTimer.stop();
    m_rfTimeout = false;

    m_rfFrames = 0U;
    m_rfErrs = 0U;
    m_rfBits = 1U;

    m_rfLC = nullptr;
    m_rfPrivacyLC = nullptr;
}

/* Helper to write network end of frame data. */

void Slot::writeEndNet(bool writeEnd)
{
    m_netState = RS_NET_IDLE;

    setShortLC(m_slotNo, 0U);

    m_voice->m_lastFrameValid = false;

    if (writeEnd && !m_netTimeout) {
        // Create a dummy start end frame
        uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];

        Sync::addDMRDataSync(data + 2U, m_duplex);

        lc::FullLC fullLC;
        fullLC.encode(*m_netLC, data + 2U, DataType::TERMINATOR_WITH_LC);

        SlotType slotType;
        slotType.setColorCode(m_colorCode);
        slotType.setDataType(DataType::TERMINATOR_WITH_LC);
        slotType.encode(data + 2U);

        data[0U] = modem::TAG_EOT;
        data[1U] = 0x00U;

        if (m_duplex) {
            for (uint32_t i = 0U; i < m_hangCount; i++)
                addFrame(data, true);
        }
        else {
            for (uint32_t i = 0U; i < 3U; i++)
                addFrame(data, true);
        }
    }

    // release trunked grant (if necessary)
    Slot* tscc = m_dmr->getTSCCSlot();
    if (tscc != nullptr) {
        if (tscc->m_enableTSCC && m_netLC != nullptr) {
            tscc->m_affiliations->releaseGrant(m_netLC->getDstId(), false);
        }

        clearTSCCActivated();

        if (!tscc->m_enableTSCC) {
            notifyCC_ReleaseGrant(m_netLC->getDstId());
        }
    }

    m_data->m_pduDataOffset = 0U;

    if (m_network != nullptr)
        m_network->resetDMR(m_slotNo);

    m_networkWatchdog.stop();
    m_netTimeoutTimer.stop();
    m_packetTimer.stop();
    m_netTimeout = false;

    m_netFrames = 0U;
    m_netLost = 0U;

    m_netErrs = 0U;
    m_netBits = 1U;

    m_netLC = nullptr;
    m_netPrivacyLC = nullptr;
}

/* Helper to write control channel packet data. */

void Slot::writeRF_ControlData(uint16_t frameCnt, uint8_t n)
{
    uint8_t i = 0U, seqCnt = 0U;

    if (!m_enableTSCC)
        return;

    // disable verbose CSBK dumping during control data writes (if necessary)
    bool csbkVerbose = lc::CSBK::getVerbose();
    if (csbkVerbose)
        lc::CSBK::setVerbose(false);

    // disable debug logging during control data writes (if necessary)
    bool controlDebug = m_debug;
    if (!m_ccDebug)
        m_debug = false;

    // don't add any frames if the queue is full
    uint8_t len = DMR_FRAME_LENGTH_BYTES + 2U;
    uint32_t space = m_txQueue.freeSpace();
    if (space < (len + 1U)) {
        m_ccSeq--;
        if (m_ccSeq < 0U)
            m_ccSeq = 0U;
        return;
    }

    // loop to generate 4 control sequences
    if (frameCnt == 511U) {
        seqCnt = 4U;
    }

    // should we insert the Git Hash burst?
    bool hash = (frameCnt % 256U) == 0U;
    if (hash) {
        m_control->writeRF_TSCC_Git_Hash();

        if (seqCnt > 0U)
            n++;

        return;
    }

    do
    {
        if (m_debug) {
            LogDebug(LOG_DMR, "writeRF_ControlData, frameCnt = %u, seq = %u", frameCnt, n);
        }

        switch (n)
        {
        /** required data */
        case 0:
        default:
            m_control->writeRF_TSCC_Bcast_Sys_Parm();
            break;
        case 1:
            m_control->writeRF_TSCC_Aloha();
            break;
        case 2:
            m_control->writeRF_TSCC_Bcast_Ann_Wd(m_channelNo, true, m_siteData.systemIdentity(), m_siteData.requireReg());
            break;
        case 3:
            {
                std::unordered_map<uint32_t, uint32_t> grants = m_affiliations->grantTable();
                if (grants.size() > 0) {
                    uint32_t j = 0U;
                    if (m_lastLateEntry > grants.size()) {
                        m_lastLateEntry = 0U;
                    }

                    for (auto entry : grants) {
                        if (j == m_lastLateEntry) {
                            uint32_t dstId = entry.first;
                            uint32_t srcId = m_affiliations->getGrantedSrcId(dstId);
                            bool grp = m_affiliations->isGroup(dstId);

                            if (m_debug) {
                                LogDebugEx(LOG_DMR, "Slot::writeRF_ControlData()", "frameCnt = %u, seq = %u, late entry, dstId = %u, srcId = %u", frameCnt, n, dstId, srcId);
                            }

                            m_control->writeRF_CSBK_Grant_LateEntry(dstId, srcId, grp);
                            m_lastLateEntry = j++;
                            break;
                        }

                        j++;
                    }
                }
                else {
                    m_control->writeRF_TSCC_Bcast_Sys_Parm();
                }
            }
            break;
        /** extra data */
        case 4:
            // write ADJSS
            if (m_adjSiteTable.size() > 0) {
                if (m_tsccAdjSSCnt >= m_adjSiteTable.size())
                    m_tsccAdjSSCnt = 0U;

                uint8_t i = 0U;
                for (auto entry : m_adjSiteTable) {
                    // no good very bad way of skipping entries...
                    if (i != m_tsccAdjSSCnt) {
                        i++;
                        continue;
                    }
                    else {
                        AdjSiteData site = entry.second;
                        m_control->writeRF_TSCC_Bcast_Ann_Wd(site.channelNo, true, site.systemIdentity, site.requireReg);
                        m_tsccAdjSSCnt++;
                        break;
                    }
                }
                break;
            }
            break;
        }

        if (seqCnt > 0U)
            n++;
        i++;
    } while (i <= seqCnt);

    lc::CSBK::setVerbose(csbkVerbose);
    m_debug = controlDebug;
}

/* Clears the flag indicating whether the slot is a TSCC payload slot. */

void Slot::clearTSCCActivated() 
{
    if (m_tsccPayloadDstId != 0U && m_tsccPayloadSrcId != 0U) {
        m_control->writeRF_CSBK_Payload_Clear(m_tsccPayloadDstId, m_tsccPayloadSrcId, m_tsccPayloadGroup);
    }

    m_tsccPayloadDstId = 0U;
    m_tsccPayloadSrcId = 0U;
    m_tsccPayloadGroup = false;
    m_tsccPayloadVoice = true;

    m_tsccPayloadActRetry.stop();
}

/* Helper to set the DMR short LC. */

void Slot::setShortLC(uint32_t slotNo, uint32_t id, FLCO::E flco, SLCO_ACT_TYPE actType)
{
    assert(m_modem != nullptr);

    switch (slotNo) {
        case 1U:
            m_id1 = 0U;
            m_flco1 = flco;
            m_actType1 = actType;
            if (id != 0U) {
                uint8_t buffer[3U];
                buffer[0U] = (id << 16) & 0xFFU;
                buffer[1U] = (id << 8) & 0xFFU;
                buffer[2U] = (id << 0) & 0xFFU;
                m_id1 = edac::CRC::crc8(buffer, 3U);
            }
            break;
        case 2U:
            m_id2 = 0U;
            m_flco2 = flco;
            m_actType2 = actType;
            if (id != 0U) {
                uint8_t buffer[3U];
                buffer[0U] = (id << 16) & 0xFFU;
                buffer[1U] = (id << 8) & 0xFFU;
                buffer[2U] = (id << 0) & 0xFFU;
                m_id2 = edac::CRC::crc8(buffer, 3U);
            }
            break;
        default:
            LogError(LOG_DMR, "invalid slot number passed to setShortLC, slotNo = %u", slotNo);
            return;
    }

    // If we have no activity to report, let the modem send the null Short LC when it's ready
    if (m_id1 == 0U && m_id2 == 0U)
        return;

    uint8_t lc[5U];
    lc[0U] = SLCO::ACT;
    lc[1U] = 0x00U;
    lc[2U] = 0x00U;
    lc[3U] = 0x00U;

    if (m_id1 != 0U) {
        lc[2U] = m_id1;
        if (m_actType1 == SLCO_ACT_TYPE::VOICE && m_flco1 == FLCO::GROUP)
            lc[1U] |= 0x08U;
        else if (m_actType1 == SLCO_ACT_TYPE::VOICE && m_flco1 == FLCO::PRIVATE)
            lc[1U] |= 0x09U;
        else if (m_actType1 == SLCO_ACT_TYPE::DATA && m_flco1 == FLCO::GROUP)
            lc[1U] |= 0x0BU;
        else if (m_actType1 == SLCO_ACT_TYPE::DATA && m_flco1 == FLCO::PRIVATE)
            lc[1U] |= 0x0AU;
        else if (m_actType1 == SLCO_ACT_TYPE::CSBK && m_flco1 == FLCO::GROUP)
            lc[1U] |= 0x02U;
        else if (m_actType1 == SLCO_ACT_TYPE::CSBK && m_flco1 == FLCO::PRIVATE)
            lc[1U] |= 0x03U;
    }

    if (m_id2 != 0U) {
        lc[3U] = m_id2;
        if (m_actType2 == SLCO_ACT_TYPE::VOICE && m_flco2 == FLCO::GROUP)
            lc[1U] |= 0x08U;
        else if (m_actType2 == SLCO_ACT_TYPE::VOICE && m_flco2 == FLCO::PRIVATE)
            lc[1U] |= 0x09U;
        else if (m_actType2 == SLCO_ACT_TYPE::DATA && m_flco2 == FLCO::GROUP)
            lc[1U] |= 0x0BU;
        else if (m_actType2 == SLCO_ACT_TYPE::DATA && m_flco2 == FLCO::PRIVATE)
            lc[1U] |= 0x0AU;
        else if (m_actType2 == SLCO_ACT_TYPE::CSBK && m_flco2 == FLCO::GROUP)
            lc[1U] |= 0x02U;
        else if (m_actType2 == SLCO_ACT_TYPE::CSBK && m_flco2 == FLCO::PRIVATE)
            lc[1U] |= 0x03U;
    }

    lc[4U] = edac::CRC::crc8(lc, 4U);

    uint8_t sLC[9U];

    lc::ShortLC shortLC;
    shortLC.encode(lc, sLC);

    m_modem->writeDMRShortLC(sLC);
}

/* Helper to set the DMR short LC for TSCC. */

void Slot::setShortLC_TSCC(SiteData siteData, uint16_t counter)
{
    assert(m_modem != nullptr);

    uint8_t lc[5U];
    uint32_t lcValue = 0U;
    lcValue = SLCO::TSCC;
    lcValue = (lcValue << 2) + siteData.siteModel();

    switch (siteData.siteModel())
    {
    case SiteModel::SM_TINY:
    {
        lcValue = (lcValue << 9) + siteData.netId();
        lcValue = (lcValue << 3) + siteData.siteId();
    }
    break;
    case SiteModel::SM_SMALL:
    {
        lcValue = (lcValue << 7) + siteData.netId();
        lcValue = (lcValue << 5) + siteData.siteId();
    }
    break;
    case SiteModel::SM_LARGE:
    {
        lcValue = (lcValue << 5) + siteData.netId();
        lcValue = (lcValue << 7) + siteData.siteId();
    }
    break;
    case SiteModel::SM_HUGE:
    {
        lcValue = (lcValue << 2) + siteData.netId();
        lcValue = (lcValue << 10) + siteData.siteId();
    }
    break;
    }

    lcValue = (lcValue << 1) + ((siteData.requireReg()) ? 1U : 0U);
    lcValue = (lcValue << 9) + (counter & 0x1FFU);

    // split value into bytes
    lc[0U] = (uint8_t)((lcValue >> 24) & 0xFFU);
    lc[1U] = (uint8_t)((lcValue >> 16) & 0xFFU);
    lc[2U] = (uint8_t)((lcValue >> 8) & 0xFFU);
    lc[3U] = (uint8_t)((lcValue >> 0) & 0xFFU);
    lc[4U] = edac::CRC::crc8(lc, 4U);

    // LogDebugEx(LOG_DMR, "Slot::setShortLC_TSCC()", "netId = %02X, siteId = %02X", siteData.netId(), siteData.siteId());
    // Utils::dump(1U, "DMR, Slot::shortLC_TSCC(), LC", lc, 5U);

    uint8_t sLC[9U];

    lc::ShortLC shortLC;
    shortLC.encode(lc, sLC);

    m_modem->writeDMRShortLC(sLC);
}

/* Helper to set the DMR short LC for payload. */

void Slot::setShortLC_Payload(SiteData siteData, uint16_t counter)
{
    assert(m_modem != nullptr);

    uint8_t lc[5U];
    uint32_t lcValue = 0U;
    lcValue = SLCO::PAYLOAD;
    lcValue = (lcValue << 2) + siteData.siteModel();

    switch (siteData.siteModel())
    {
    case SiteModel::SM_TINY:
    {
        lcValue = (lcValue << 9) + siteData.netId();
        lcValue = (lcValue << 3) + siteData.siteId();
    }
    break;
    case SiteModel::SM_SMALL:
    {
        lcValue = (lcValue << 7) + siteData.netId();
        lcValue = (lcValue << 5) + siteData.siteId();
    }
    break;
    case SiteModel::SM_LARGE:
    {
        lcValue = (lcValue << 5) + siteData.netId();
        lcValue = (lcValue << 7) + siteData.siteId();
    }
    break;
    case SiteModel::SM_HUGE:
    {
        lcValue = (lcValue << 2) + siteData.netId();
        lcValue = (lcValue << 10) + siteData.siteId();
    }
    break;
    }

    lcValue = (lcValue << 1) + 0U;                          // Payload channel is Normal
    lcValue = (lcValue << 9) + (counter & 0x1FFU);

    // split value into bytes
    lc[0U] = (uint8_t)((lcValue >> 24) & 0xFFU);
    lc[1U] = (uint8_t)((lcValue >> 16) & 0xFFU);
    lc[2U] = (uint8_t)((lcValue >> 8) & 0xFFU);
    lc[3U] = (uint8_t)((lcValue >> 0) & 0xFFU);
    lc[4U] = edac::CRC::crc8(lc, 4U);

    // LogDebugEx(LOG_DMR, "Slot::setShortLC_Payload()", "netId = %02X, siteId = %02X", siteData.netId(), siteData.siteId());
    // Utils::dump(1U, "DMR, Slot::setShortLC_Payload(), LC", lc, 5U);

    uint8_t sLC[9U];

    lc::ShortLC shortLC;
    shortLC.encode(lc, sLC);

    m_modem->writeDMRShortLC(sLC);
}
