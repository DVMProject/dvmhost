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
*   Copyright (C) 2017-2023 by Bryan Biedenkapp N2PLL
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
#include "dmr/Slot.h"
#include "dmr/acl/AccessControl.h"
#include "dmr/lc/FullLC.h"
#include "dmr/lc/ShortLC.h"
#include "dmr/lc/CSBK.h"
#include "dmr/SlotType.h"
#include "dmr/Sync.h"
#include "edac/BPTC19696.h"
#include "edac/CRC.h"
#include "remote/RESTClient.h"
#include "Log.h"
#include "Utils.h"

using namespace dmr;
using namespace dmr::packet;

#include <cassert>
#include <ctime>
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
network::BaseNetwork* Slot::m_network = nullptr;

bool Slot::m_duplex = true;

::lookups::IdenTableLookup* Slot::m_idenTable = nullptr;
::lookups::RadioIdLookup* Slot::m_ridLookup = nullptr;
::lookups::TalkgroupIdLookup* Slot::m_tidLookup = nullptr;
dmr::lookups::DMRAffiliationLookup *Slot::m_affiliations = nullptr;

::lookups::IdenTable Slot::m_idenEntry = ::lookups::IdenTable();

uint32_t Slot::m_hangCount = 3U * 17U;

::lookups::RSSIInterpolator* Slot::m_rssiMapper = nullptr;

uint32_t Slot::m_jitterTime = 360U;
uint32_t Slot::m_jitterSlots = 6U;

uint8_t* Slot::m_idle = nullptr;

uint8_t Slot::m_flco1;
uint8_t Slot::m_id1 = 0U;
bool Slot::m_voice1 = true;
uint8_t Slot::m_flco2;
uint8_t Slot::m_id2 = 0U;
bool Slot::m_voice2 = true;

bool Slot::m_verifyReg = false;

uint8_t Slot::m_alohaNRandWait = DEFAULT_NRAND_WAIT;
uint8_t Slot::m_alohaBackOff = 1U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the Slot class.
/// </summary>
/// <param name="slotNo">DMR slot number.</param>
/// <param name="queueSize">Modem frame buffer queue size (bytes).</param>
/// <param name="timeout">Transmit timeout.</param>
/// <param name="tgHang">Amount of time to hang on the last talkgroup mode from RF.</param>
/// <param name="dumpDataPacket"></param>
/// <param name="repeatDataPacket"></param>
/// <param name="dumpCSBKData"></param>
/// <param name="debug">Flag indicating whether DMR debug is enabled.</param>
/// <param name="verbose">Flag indicating whether DMR verbose logging is enabled.</param>
Slot::Slot(uint32_t slotNo, uint32_t timeout, uint32_t tgHang, uint32_t queueSize, bool dumpDataPacket, bool repeatDataPacket,
    bool dumpCSBKData, bool debug, bool verbose) :
    m_slotNo(slotNo),
    m_queue(queueSize, "DMR Slot Frame"),
    m_rfState(RS_RF_LISTENING),
    m_rfLastDstId(0U),
    m_netState(RS_NET_IDLE),
    m_netLastDstId(0U),
    m_permittedDstId(0U),
    m_rfLC(nullptr),
    m_rfPrivacyLC(nullptr),
    m_rfDataHeader(nullptr),
    m_rfSeqNo(0U),
    m_netLC(nullptr),
    m_netPrivacyLC(nullptr),
    m_netDataHeader(nullptr),
    m_networkWatchdog(1000U, 0U, 1500U),
    m_rfTimeoutTimer(1000U, timeout),
    m_rfTGHang(1000U, tgHang),
    m_netTimeoutTimer(1000U, timeout),
    m_packetTimer(1000U, 0U, 50U),
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
    m_ccSeq(0U),
    m_ccRunning(false),
    m_ccPrevRunning(false),
    m_ccHalted(false),
    m_enableTSCC(false),
    m_dedicatedTSCC(false),
    m_tsccPayloadDstId(0U),
    m_tsccPayloadGroup(false),
    m_tsccPayloadVoice(true),
    m_lastLateEntry(0U),
    m_supervisor(false),
    m_verbose(verbose),
    m_debug(debug)
{
    m_interval.start();

    m_voice = new Voice(this, m_network, m_embeddedLCOnly, m_dumpTAData, debug, verbose);
    m_data = new Data(this, m_network, dumpDataPacket, repeatDataPacket, debug, verbose);
    m_control = new ControlSignaling(this, m_network, dumpCSBKData, debug, verbose);
}

/// <summary>
/// Finalizes a instance of the Slot class.
/// </summary>
Slot::~Slot()
{
    delete m_voice;
    delete m_data;
    delete m_control;
}

/// <summary>
/// Process DMR data frame from the RF interface.
/// </summary>
/// <param name="data">Buffer containing data frame.</param>
/// <param name="len">Length of data frame.</param>
/// <returns></returns>
bool Slot::processFrame(uint8_t *data, uint32_t len)
{
    assert(data != nullptr);

    if (data[0U] == modem::TAG_LOST && m_rfState == RS_RF_AUDIO) {
        if (m_rssi != 0U) {
            ::ActivityLog("DMR", true, "Slot %u RF voice transmission lost, %.1f seconds, BER: %.1f%%, RSSI: -%u/-%u/-%u dBm",
                m_slotNo, float(m_rfFrames) / 16.667F, float(m_rfErrs * 100U) / float(m_rfBits), m_minRSSI, m_maxRSSI, m_aveRSSI / m_rssiCount);
        }
        else {
            ::ActivityLog("DMR", true, "Slot %u RF voice transmission lost, %.1f seconds, BER: %.1f%%",
                m_slotNo, float(m_rfFrames) / 16.667F, float(m_rfErrs * 100U) / float(m_rfBits));
        }

        LogMessage(LOG_RF, "DMR Slot %u, total frames: %d, total bits: %d, errors: %d, BER: %.4f%%",
            m_slotNo, m_rfFrames, m_rfBits, m_rfErrs, float(m_rfErrs * 100U) / float(m_rfBits));

        // release trunked grant (if necessary)
        Slot *m_tscc = m_dmr->getTSCCSlot();
        if (m_tscc != nullptr) {
            if (m_tscc->m_enableTSCC && m_rfLC != nullptr) {
                m_tscc->m_affiliations->releaseGrant(m_rfLC->getDstId(), false);
            }
        }

        if (m_rfTimeout) {
            writeEndRF();
            return false;
        }
        else {
            writeEndRF(true);
            return true;
        }
    }

    if (data[0U] == modem::TAG_LOST && m_rfState == RS_RF_DATA) {
        ::ActivityLog("DMR", true, "Slot %u, RF data transmission lost", m_slotNo);
        writeEndRF();
        return false;
    }

    if (data[0U] == modem::TAG_LOST) {
        m_rfState = RS_RF_LISTENING;
        m_rfLastDstId = 0U;
        m_rfTGHang.stop();
        return false;
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

    bool dataSync = (data[1U] & DMR_SYNC_DATA) == DMR_SYNC_DATA;
    bool voiceSync = (data[1U] & DMR_SYNC_VOICE) == DMR_SYNC_VOICE;

    if (!(dataSync || voiceSync) && m_rfState == RS_RF_LISTENING) {
        uint8_t sync[DMR_SYNC_LENGTH_BYTES];
        ::memcpy(sync, data + 2U, DMR_SYNC_LENGTH_BYTES);

        // count data sync errors
        uint8_t dataErrs = 0U;
        for (uint8_t i = 0U; i < DMR_SYNC_LENGTH_BYTES; i++)
            dataErrs += Utils::countBits8(sync[i] ^ DMR_MS_DATA_SYNC_BYTES[i]);

        // count voice sync errors
        uint8_t voiceErrs = 0U;
        for (uint8_t i = 0U; i < DMR_SYNC_LENGTH_BYTES; i++)
            voiceErrs += Utils::countBits8(sync[i] ^ DMR_MS_VOICE_SYNC_BYTES[i]);

        LogWarning(LOG_RF, "DMR, possible sync word rejected, dataErrs = %u, voiceErrs = %u, sync word = %02X %02X %02X %02X %02X %02X", dataErrs, voiceErrs,
            sync[0U], sync[1U], sync[2U], sync[3U], sync[4U], sync[5U]);
    }

    if ((dataSync || voiceSync) && m_debug) {
        Utils::symbols("!!! *Rx DMR", data + 2U, len - 2U);
    }

    if ((dataSync || voiceSync) && m_rfState != RS_RF_LISTENING)
        m_rfTGHang.start();

    if (dataSync) {
        uint8_t dataType = data[1U] & 0x0FU;

        if (dataType == DT_CSBK) {
            return m_control->process(data, len);
        }

        if (m_enableTSCC && m_dedicatedTSCC)
            return false;

        switch (dataType)
        {
        case DT_VOICE_LC_HEADER:
        case DT_VOICE_PI_HEADER:
            return m_voice->process(data, len);
        case DT_TERMINATOR_WITH_LC:
        case DT_DATA_HEADER:
        case DT_RATE_12_DATA:
        case DT_RATE_34_DATA:
        case DT_RATE_1_DATA:
        default:
            return m_data->process(data, len);
        }
    }

    return m_voice->process(data, len);
}

/// <summary>
/// Get frame data from data ring buffer.
/// </summary>
/// <param name="data">Buffer to store frame data.</param>
/// <returns>Length of frame data retreived.</returns>
uint32_t Slot::getFrame(uint8_t* data)
{
    assert(data != nullptr);

    if (m_queue.isEmpty())
        return 0U;

    uint8_t len = 0U;
    m_queue.getData(&len, 1U);
    m_queue.getData(data, len);

    return len;
}

/// <summary>
/// Process a data frame from the network.
/// </summary>
/// <param name="dmrData"></param>
void Slot::processNetwork(const data::Data& dmrData)
{
    // don't process network frames if the RF modem isn't in a listening state
    if (m_rfState != RS_RF_LISTENING) {
        LogWarning(LOG_NET, "Traffic collision detect, preempting new network traffic to existing RF traffic!");
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

    m_networkWatchdog.start();

    uint8_t dataType = dmrData.getDataType();

    // ignore non-CSBK data destined for the TSCC slot
    if (m_enableTSCC && m_dedicatedTSCC && m_slotNo == m_dmr->m_tsccSlotNo &&
        dataType != DT_CSBK) {
        return;
    }

    switch (dataType)
    {
    case DT_CSBK:
        m_control->processNetwork(dmrData);
        break;
    case DT_VOICE_LC_HEADER:
    case DT_VOICE_PI_HEADER:
    case DT_VOICE_SYNC:
    case DT_VOICE:
        m_voice->processNetwork(dmrData);
        break;
    case DT_TERMINATOR_WITH_LC:
    case DT_DATA_HEADER:
    case DT_RATE_12_DATA:
    case DT_RATE_34_DATA:
    case DT_RATE_1_DATA:
    default:
        m_data->processNetwork(dmrData);
        break;
    }
}

/// <summary>
/// Updates the DMR slot processor.
/// </summary>
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
    }

    // if we have control enabled; do clocking to generate a CC data stream
    if (m_enableTSCC) {
        m_modem->setDMRIgnoreCACH_AT(m_slotNo);

        // clock all the grant timers
        m_affiliations->clock(ms);

        if (m_ccRunning && !m_ccPacketInterval.isRunning()) {
            m_ccPacketInterval.start();
        }

        if (m_ccHalted) {
            if (!m_ccRunning) {
                m_ccHalted = false;
                m_ccPrevRunning = m_ccRunning;
                m_queue.clear(); // clear the frame buffer
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
            m_queue.clear(); // clear the frame buffer
            m_ccPrevRunning = m_ccRunning;
        }
    }

    // activate payload channel if requested from the TSCC
    if (m_dmr->m_tsccPayloadActive) {
        if (m_rfState == RS_RF_LISTENING && m_netState == RS_NET_IDLE) {
            if (m_tsccPayloadDstId > 0U) {
                if ((m_dmr->m_tsccCnt % 2) > 0) {
                    setShortLC(m_slotNo, m_tsccPayloadDstId, m_tsccPayloadGroup ? FLCO_GROUP : FLCO_PRIVATE, m_tsccPayloadVoice);
                }
            }
        }
        else {
            clearTSCCActivated();
        }
    }

    m_rfTimeoutTimer.clock(ms);
    if (m_rfTimeoutTimer.isRunning() && m_rfTimeoutTimer.hasExpired()) {
        if (!m_rfTimeout) {
            LogMessage(LOG_RF, "DMR Slot %u, user has timed out", m_slotNo);
            m_rfTimeout = true;
        }
    }

    m_netTimeoutTimer.clock(ms);
    if (m_netTimeoutTimer.isRunning() && m_netTimeoutTimer.hasExpired()) {
        if (!m_netTimeout) {
            LogMessage(LOG_NET, "DMR Slot %u, user has timed out", m_slotNo);
            m_netTimeout = true;
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

            // reset permitted ID and clear permission state
            if (!m_authoritative && m_permittedDstId != 0U) {
                m_permittedDstId = 0U;
            }
        }
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

    if (m_rfState == RS_RF_REJECTED) {
        if (!m_enableTSCC) {
            m_queue.clear();
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

/// <summary>
/// Permits a TGID on a non-authoritative host.
/// </summary>
/// <param name="dstId"></param>
void Slot::permittedTG(uint32_t dstId)
{
    if (m_authoritative) {
        return;
    }

    m_permittedDstId = dstId;
}

/// <summary>
/// Helper to change the debug and verbose state.
/// </summary>
/// <param name="debug">Flag indicating whether DMR debug is enabled.</param>
/// <param name="verbose">Flag indicating whether DMR verbose logging is enabled.</param>
void Slot::setDebugVerbose(bool debug, bool verbose)
{
    m_debug = m_voice->m_debug = m_data->m_debug = debug = m_control->m_debug;
    m_verbose = m_voice->m_verbose = m_data->m_verbose = verbose = m_control->m_verbose;
}

/// <summary>
/// Helper to enable and configure TSCC support for this slot.
/// </summary>
/// <param name="enable">Flag indicating whether DMR TSCC is enabled on this slot.</param>
/// <param name="enable">Flag indicating whether DMR TSCC is dedicated on this slot.</param>
void Slot::setTSCC(bool enable, bool dedicated)
{
    m_enableTSCC = enable;
    m_dedicatedTSCC = dedicated;
    if (m_enableTSCC) {
        m_modem->setDMRIgnoreCACH_AT(m_slotNo);
        m_affiliations->setSlotForChannelTSCC(m_channelNo, m_slotNo);
    }
}

/// <summary>
/// Helper to activate a TSCC payload slot.
/// </summary>
/// <param name="dstId"></param>
/// <param name="srcId"></param>
/// <param name="group"></param>
/// <param name="voice"></param>
void Slot::setTSCCActivated(uint32_t dstId, uint32_t srcId, bool group, bool voice)
{
    m_tsccPayloadDstId = dstId;
    m_tsccPayloadGroup = group;
    m_tsccPayloadVoice = voice;
}

/// <summary>
/// Helper to set the voice error silence threshold.
/// </summary>
/// <param name="threshold"></param>
void Slot::setSilenceThreshold(uint32_t threshold)
{
    m_silenceThreshold = threshold;
}

/// <summary>
/// Helper to initialize the DMR slot processor.
/// </summary>
/// <param name="dmr">Instance of the Control class.</param>
/// <param name="authoritative">Flag indicating whether or not the DVM is grant authoritative.</param>
/// <param name="colorCode">DMR access color code.</param>
/// <param name="siteData">DMR site data.</param>
/// <param name="embeddedLCOnly"></param>
/// <param name="dumpTAData"></param>
/// <param name="callHang">Amount of hangtime for a DMR call.</param>
/// <param name="modem">Instance of the Modem class.</param>
/// <param name="network">Instance of the BaseNetwork class.</param>
/// <param name="duplex">Flag indicating full-duplex operation.</param>
/// <param name="ridLookup">Instance of the RadioIdLookup class.</param>
/// <param name="tidLookup">Instance of the TalkgroupIdLookup class.</param>
/// <param name="idenTable">Instance of the IdenTableLookup class.</param>
/// <param name="rssi">Instance of the RSSIInterpolator class.</param>
/// <param name="jitter"></param>
/// <param name="verbose"></param>
void Slot::init(Control* dmr, bool authoritative, uint32_t colorCode, SiteData siteData, bool embeddedLCOnly, bool dumpTAData, uint32_t callHang, modem::Modem* modem,
    network::BaseNetwork* network, bool duplex, ::lookups::RadioIdLookup* ridLookup, ::lookups::TalkgroupIdLookup* tidLookup,
    ::lookups::IdenTableLookup* idenTable, ::lookups::RSSIInterpolator* rssiMapper, uint32_t jitter, bool verbose)
{
    assert(dmr != nullptr);
    assert(modem != nullptr);
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
    m_affiliations = new dmr::lookups::DMRAffiliationLookup(verbose);

    // set the grant release callback
    m_affiliations->setReleaseGrantCallback([=](uint32_t chNo, uint32_t dstId, uint8_t slot) {
        Slot* tscc = m_dmr->getTSCCSlot();
        if (tscc != nullptr) {
            if (chNo == tscc->m_channelNo) {
                m_dmr->tsccClearActivatedSlot(slot);
                return;
            }

            ::lookups::VoiceChData voiceChData = tscc->m_affiliations->getRFChData(chNo);
            if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0) {
                json::object req = json::object();
                req["slot"].set<uint8_t>(slot);
                bool clear = true;
                req["clear"].set<bool>(clear);

                RESTClient::send(voiceChData.address(), voiceChData.port(), voiceChData.password(),
                    HTTP_PUT, PUT_DMR_TSCC_PAYLOAD_ACT, req, tscc->m_debug);
            }
            else {
                ::LogError(LOG_DMR, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), failed to clear payload channel, chNo = %u, slot = %u", tscc->m_slotNo, chNo, slot);
            }

            // callback REST API to clear TG permit for the granted TG on the specified voice channel
            if (m_authoritative && m_dmr->m_supervisor) {
                if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0) {
                    json::object req = json::object();
                    int state = modem::DVM_STATE::STATE_DMR;
                    req["state"].set<int>(state);
                    dstId = 0U; // clear TG value
                    req["dstId"].set<uint32_t>(dstId);
                    req["slot"].set<uint8_t>(slot);

                    RESTClient::send(voiceChData.address(), voiceChData.port(), voiceChData.password(),
                        HTTP_PUT, PUT_PERMIT_TG, req, m_dmr->m_debug);
                }
                else {
                    ::LogError(LOG_DMR, "DMR Slot %u, DT_CSBK, CSBKO_RAND (Random Access), failed to clear TG permit, chNo = %u, slot = %u", tscc->m_slotNo, chNo, slot);
                }
            }
        }
    });

    m_hangCount = callHang * 17U;

    m_rssiMapper = rssiMapper;

    m_jitterTime = jitter;

    float jitter_tmp = float(jitter) / 360.0F;
    m_jitterSlots = (uint32_t)(std::ceil(jitter_tmp) * 6.0F);

    m_idle = new uint8_t[DMR_FRAME_LENGTH_BYTES + 2U];
    ::memcpy(m_idle, DMR_IDLE_DATA, DMR_FRAME_LENGTH_BYTES + 2U);

    // Generate the Slot Type for the Idle frame
    SlotType slotType;
    slotType.setColorCode(colorCode);
    slotType.setDataType(DT_IDLE);
    slotType.encode(m_idle + 2U);
}

/// <summary>
/// Sets local configured site data.
/// </summary>
/// <param name="voiceChNo">Voice Channel Number list.</param>
/// <param name="voiceChData">Voice Channel data map.</param>
/// <param name="netId">DMR Network ID.</param>
/// <param name="siteId">DMR Site ID.</param>
/// <param name="channelId">Channel ID.</param>
/// <param name="channelNo">Channel Number.</param>
/// <param name="requireReg"></param>
void Slot::setSiteData(const std::vector<uint32_t> voiceChNo, const std::unordered_map<uint32_t, ::lookups::VoiceChData> voiceChData,
    uint32_t netId, uint8_t siteId, uint8_t channelId, uint32_t channelNo, bool requireReg)
{
    m_siteData = SiteData(SITE_MODEL_SMALL, netId, siteId, 3U, requireReg);
    m_channelNo = channelNo;

    std::vector<::lookups::IdenTable> entries = m_idenTable->list();
    for (auto entry : entries) {
        if (entry.channelId() == channelId) {
            m_idenEntry = entry;
            break;
        }
    }

    for (uint32_t chNo : voiceChNo) {
        m_affiliations->addRFCh(chNo);
    }

    std::unordered_map<uint32_t, ::lookups::VoiceChData> chData = std::unordered_map<uint32_t, ::lookups::VoiceChData>(voiceChData);
    m_affiliations->setRFChData(chData);

    lc::CSBK::setSiteData(m_siteData);
}

/// <summary>
/// Sets TSCC Aloha configuration.
/// </summary>
/// <param name="nRandWait"></param>
/// <param name="backOff"></param>
void Slot::setAlohaConfig(uint8_t nRandWait, uint8_t backOff)
{
    m_alohaNRandWait = nRandWait;
    m_alohaBackOff = backOff;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Add data frame to the data ring buffer.
/// </summary>
/// <param name="data"></param>
/// <param name="net"></param>
void Slot::addFrame(const uint8_t *data, bool net)
{
    assert(data != nullptr);

    if (!net) {
        if (m_netState != RS_NET_IDLE)
            return;
    }

    uint8_t len = DMR_FRAME_LENGTH_BYTES + 2U;
    uint32_t space = m_queue.freeSpace();
    if (space < (len + 1U)) {
        if (!net) {
            uint32_t queueLen = m_queue.length();
            m_queue.resize(queueLen + (DMR_FRAME_LENGTH_BYTES + 2U));
            LogError(LOG_DMR, "Slot %u, overflow in the DMR slot queue; queue free is %u, needed %u; resized was %u is %u", m_slotNo, space, len, queueLen, m_queue.length());
            return;
        }
        else {
            LogError(LOG_DMR, "Slot %u, overflow in the DMR slot queue while writing network data; queue free is %u, needed %u", m_slotNo, space, len);
            return;
        }
    }

    if (m_debug) {
        Utils::symbols("!!! *Tx DMR", data + 2U, len - 2U);
    }

    m_queue.addData(&len, 1U);
    m_queue.addData(data, len);
}

/// <summary>
/// Write data frame to the network.
/// </summary>
/// <param name="data"></param>
/// <param name="dataType"></param>
/// <param name="errors"></param>
void Slot::writeNetwork(const uint8_t* data, uint8_t dataType, uint8_t errors)
{
    assert(data != nullptr);
    assert(m_rfLC != nullptr);

    writeNetwork(data, dataType, m_rfLC->getFLCO(), m_rfLC->getSrcId(), m_rfLC->getDstId(), errors);
}

/// <summary>
/// Write data frame to the network.
/// </summary>
/// <param name="data"></param>
/// <param name="dataType"></param>
/// <param name="flco"></param>
/// <param name="srcId"></param>
/// <param name="dstId"></param>
/// <param name="errors"></param>
void Slot::writeNetwork(const uint8_t* data, uint8_t dataType, uint8_t flco, uint32_t srcId,
    uint32_t dstId, uint8_t errors)
{
    assert(data != nullptr);

    if (m_netState != RS_NET_IDLE)
        return;

    if (m_network == nullptr)
        return;

    data::Data dmrData;
    dmrData.setSlotNo(m_slotNo);
    dmrData.setDataType(dataType);
    dmrData.setSrcId(srcId);
    dmrData.setDstId(dstId);
    dmrData.setFLCO(flco);
    dmrData.setN(m_voice->m_rfN);
    dmrData.setSeqNo(m_rfSeqNo);
    dmrData.setBER(errors);
    dmrData.setRSSI(m_rssi);

    m_rfSeqNo++;

    dmrData.setData(data + 2U);

    m_network->writeDMR(dmrData);
}

/// <summary>
/// Helper to write RF end of frame data.
/// </summary>
/// <param name="writeEnd"></param>
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
            fullLC.encode(*m_rfLC, data + 2U, DT_TERMINATOR_WITH_LC);

            SlotType slotType;
            slotType.setColorCode(m_colorCode);
            slotType.setDataType(DT_TERMINATOR_WITH_LC);
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
    m_rfDataHeader = nullptr;
}

/// <summary>
/// Helper to write network end of frame data.
/// </summary>
/// <param name="writeEnd"></param>
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
        fullLC.encode(*m_netLC, data + 2U, DT_TERMINATOR_WITH_LC);

        SlotType slotType;
        slotType.setColorCode(m_colorCode);
        slotType.setDataType(DT_TERMINATOR_WITH_LC);
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
    m_netDataHeader = nullptr;
}

/// <summary>
/// Helper to write control channel packet data.
/// </summary>
/// <param name="frameCnt"></param>
/// <param name="n"></param>
void Slot::writeRF_ControlData(uint16_t frameCnt, uint8_t n)
{
    uint8_t i = 0U, seqCnt = 0U;

    if (!m_enableTSCC)
        return;

    // don't add any frames if the queue is full
    uint8_t len = DMR_FRAME_LENGTH_BYTES + 2U;
    uint32_t space = m_queue.freeSpace();
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

                        if (m_debug) {
                            LogDebug(LOG_DMR, "writeRF_ControlData, frameCnt = %u, seq = %u, late entry, dstId = %u, srcId = %u", frameCnt, n, dstId, srcId);
                        }

                        m_control->writeRF_CSBK_Grant_LateEntry(dstId, srcId);
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
        case 2:
            m_control->writeRF_TSCC_Bcast_Ann_Wd(m_channelNo, true);
            break;
        case 1:
            m_control->writeRF_TSCC_Aloha();
            break;
        case 0:
        default:
            m_control->writeRF_TSCC_Bcast_Sys_Parm();
            break;
        }

        if (seqCnt > 0U)
            n++;
        i++;
    } while (i <= seqCnt);
}

/// <summary>
///
/// </summary>
/// <param name="slotNo"></param>
/// <param name="id"></param>
/// <param name="flco"></param>
/// <param name="voice"></param>
void Slot::setShortLC(uint32_t slotNo, uint32_t id, uint8_t flco, bool voice)
{
    assert(m_modem != nullptr);

    switch (slotNo) {
        case 1U:
            m_id1 = 0U;
            m_flco1 = flco;
            m_voice1 = voice;
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
            m_voice2 = voice;
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
    lc[0U] = SLCO_ACT;
    lc[1U] = 0x00U;
    lc[2U] = 0x00U;
    lc[3U] = 0x00U;

    if (m_id1 != 0U) {
        lc[2U] = m_id1;
        if (m_voice1) {
            if (m_flco1 == FLCO_GROUP)
                lc[1U] |= 0x80U;
            else
                lc[1U] |= 0x90U;
        }
        else {
            if (m_flco1 == FLCO_GROUP)
                lc[1U] |= 0xB0U;
            else
                lc[1U] |= 0xA0U;
        }
    }

    if (m_id2 != 0U) {
        lc[3U] = m_id2;
        if (m_voice2) {
            if (m_flco2 == FLCO_GROUP)
                lc[1U] |= 0x08U;
            else
                lc[1U] |= 0x09U;
        }
        else {
            if (m_flco2 == FLCO_GROUP)
                lc[1U] |= 0x0BU;
            else
                lc[1U] |= 0x0AU;
        }
    }

    lc[4U] = edac::CRC::crc8(lc, 4U);

    uint8_t sLC[9U];

    lc::ShortLC shortLC;
    shortLC.encode(lc, sLC);

    m_modem->writeDMRShortLC(sLC);
}

/// <summary>
///
/// </summary>
/// <param name="siteData"></param>
/// <param name="counter"></param>
void Slot::setShortLC_TSCC(SiteData siteData, uint16_t counter)
{
    assert(m_modem != nullptr);

    uint8_t lc[5U];
    uint32_t lcValue = 0U;
    lcValue = SLCO_TSCC;
    lcValue = (lcValue << 2) + siteData.siteModel();

    switch (siteData.siteModel())
    {
    case SITE_MODEL_TINY:
    {
        lcValue = (lcValue << 9) + siteData.netId();
        lcValue = (lcValue << 3) + siteData.siteId();
    }
    break;
    case SITE_MODEL_SMALL:
    {
        lcValue = (lcValue << 7) + siteData.netId();
        lcValue = (lcValue << 5) + siteData.siteId();
    }
    break;
    case SITE_MODEL_LARGE:
    {
        lcValue = (lcValue << 5) + siteData.netId();
        lcValue = (lcValue << 7) + siteData.siteId();
    }
    break;
    case SITE_MODEL_HUGE:
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

    //LogDebug(LOG_DMR, "setShortLC_TSCC, netId = %02X, siteId = %02X", siteData.netId(), siteData.siteId());
    //Utils::dump(1U, "shortLC_TSCC", lc, 5U);

    uint8_t sLC[9U];

    lc::ShortLC shortLC;
    shortLC.encode(lc, sLC);

    m_modem->writeDMRShortLC(sLC);
}

/// <summary>
///
/// </summary>
/// <param name="siteData"></param>
/// <param name="counter"></param>
void Slot::setShortLC_Payload(SiteData siteData, uint16_t counter)
{
    assert(m_modem != nullptr);

    uint8_t lc[5U];
    uint32_t lcValue = 0U;
    lcValue = SLCO_PAYLOAD;
    lcValue = (lcValue << 2) + siteData.siteModel();

    switch (siteData.siteModel())
    {
    case SITE_MODEL_TINY:
    {
        lcValue = (lcValue << 9) + siteData.netId();
        lcValue = (lcValue << 3) + siteData.siteId();
    }
    break;
    case SITE_MODEL_SMALL:
    {
        lcValue = (lcValue << 7) + siteData.netId();
        lcValue = (lcValue << 5) + siteData.siteId();
    }
    break;
    case SITE_MODEL_LARGE:
    {
        lcValue = (lcValue << 5) + siteData.netId();
        lcValue = (lcValue << 7) + siteData.siteId();
    }
    break;
    case SITE_MODEL_HUGE:
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

    //LogDebug(LOG_DMR, "setShortLC_Payload, netId = %02X, siteId = %02X", siteData.netId(), siteData.siteId());
    //Utils::dump(1U, "setShortLC_Payload", lc, 5U);

    uint8_t sLC[9U];

    lc::ShortLC shortLC;
    shortLC.encode(lc, sLC);

    m_modem->writeDMRShortLC(sLC);
}
