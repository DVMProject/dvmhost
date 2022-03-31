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
*   Copyright (C) 2017-2021 by Bryan Biedenkapp N2PLL
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
#include "Log.h"
#include "Utils.h"

using namespace dmr;

#include <cassert>
#include <ctime>
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint16_t TSCC_MAX_CNT = 511U;

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

uint32_t Slot::m_colorCode = 0U;

SiteData Slot::m_siteData = SiteData();
uint32_t Slot::m_channelNo = 0U;

bool Slot::m_embeddedLCOnly = false;
bool Slot::m_dumpTAData = true;

modem::Modem* Slot::m_modem = NULL;
network::BaseNetwork* Slot::m_network = NULL;

bool Slot::m_duplex = true;

lookups::IdenTableLookup* Slot::m_idenTable = NULL;
lookups::RadioIdLookup* Slot::m_ridLookup = NULL;
lookups::TalkgroupIdLookup* Slot::m_tidLookup = NULL;

lookups::IdenTable Slot::m_idenEntry = lookups::IdenTable();

uint32_t Slot::m_hangCount = 3U * 17U;

lookups::RSSIInterpolator* Slot::m_rssiMapper = NULL;

uint32_t Slot::m_jitterTime = 360U;
uint32_t Slot::m_jitterSlots = 6U;

uint8_t* Slot::m_idle = NULL;

uint8_t Slot::m_flco1;
uint8_t Slot::m_id1 = 0U;
bool Slot::m_voice1 = true;
uint8_t Slot::m_flco2;
uint8_t Slot::m_id2 = 0U;
bool Slot::m_voice2 = true;

uint16_t Slot::m_tsccCnt = 0U;

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
    m_queue(queueSize, "DMR Slot"),
    m_rfState(RS_RF_LISTENING),
    m_rfLastDstId(0U),
    m_netState(RS_NET_IDLE),
    m_netLastDstId(0U),
    m_rfLC(NULL),
    m_rfPrivacyLC(NULL),
    m_rfDataHeader(NULL),
    m_rfSeqNo(0U),
    m_netLC(NULL),
    m_netPrivacyLC(NULL),
    m_netDataHeader(NULL),
    m_networkWatchdog(1000U, 0U, 1500U),
    m_rfTimeoutTimer(1000U, timeout),
    m_rfTGHang(1000U, tgHang),
    m_netTimeoutTimer(1000U, timeout),
    m_packetTimer(1000U, 0U, 50U),
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
    m_enableTSCC(false),
    m_dumpCSBKData(dumpCSBKData),
    m_verbose(verbose),
    m_debug(debug)
{
    m_interval.start();

    m_voice = new VoicePacket(this, m_network, m_embeddedLCOnly, m_dumpTAData, debug, verbose);
    m_data = new DataPacket(this, m_network, dumpDataPacket, repeatDataPacket, debug, verbose);
    m_control = new ControlPacket(this, m_network, dumpCSBKData, debug, verbose);
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
/// Sets a flag indicating whether the DMR control channel is running.
/// </summary>
/// <param name="ccRunning"></param>
void Slot::setCCRunning(bool ccRunning)
{
    m_ccRunning = ccRunning;
}

/// <summary>
/// Process DMR data frame from the RF interface.
/// </summary>
/// <param name="data">Buffer containing data frame.</param>
/// <param name="len">Length of data frame.</param>
/// <returns></returns>
bool Slot::processFrame(uint8_t *data, uint32_t len)
{
    assert(data != NULL);

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

    // write and process TSCC CSBKs and short LC
    if (m_enableTSCC && m_dedicatedTSCC)
    {
        if (dataSync) {
            uint8_t dataType = data[1U] & 0x0FU;

            switch (dataType)
            {
            case DT_CSBK:
                return m_control->process(data, len);
            default:
                break;
            }
        }

        return false;
    }

    if (dataSync) {
        uint8_t dataType = data[1U] & 0x0FU;

        switch (dataType)
        {
        case DT_CSBK:
            return m_control->process(data, len);
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
    assert(data != NULL);

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

    if (m_network != NULL) {
        if (m_network->getStatus() == network::NET_STAT_RUNNING) {
            m_siteData.setNetActive(true);
        }
        else {
            m_siteData.setNetActive(false);
        }
    }

    if (m_enableTSCC) {
        // increment the TSCC counter on every slot 1 clock
        m_tsccCnt++;
        if (m_tsccCnt == TSCC_MAX_CNT) {
            m_tsccCnt = 0U;
        }

        if (m_ccSeq == 3U) {
            m_ccSeq = 0U;
        }

        if (m_dedicatedTSCC) {
            setShortLC_TSCC(m_siteData, m_tsccCnt);
            writeRF_ControlData(m_tsccCnt, m_ccSeq);
        }
        else {
            if (m_ccRunning) {
                setShortLC_TSCC(m_siteData, m_tsccCnt);
                writeRF_ControlData(m_tsccCnt, m_ccSeq);
            }
        }

        m_ccSeq++;
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
        m_queue.clear();

        m_rfFrames = 0U;
        m_rfErrs = 0U;
        m_rfBits = 1U;

        m_netFrames = 0U;
        m_netLost = 0U;

        if (m_network != NULL)
            m_network->resetDMR(m_slotNo);

        m_rfState = RS_RF_LISTENING;
    }
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
void Slot::init(uint32_t colorCode, SiteData siteData, bool embeddedLCOnly, bool dumpTAData, uint32_t callHang, modem::Modem* modem,
    network::BaseNetwork* network, bool duplex, lookups::RadioIdLookup* ridLookup, lookups::TalkgroupIdLookup* tidLookup,
    lookups::IdenTableLookup* idenTable, lookups::RSSIInterpolator* rssiMapper, uint32_t jitter)
{
    assert(modem != NULL);
    assert(ridLookup != NULL);
    assert(tidLookup != NULL);
    assert(idenTable != NULL);
    assert(rssiMapper != NULL);

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
/// <param name="netId">DMR Network ID.</param>
/// <param name="siteId">DMR Site ID.</param>
/// <param name="channelId">Channel ID.</param>
/// <param name="channelNo">Channel Number.</param>
void Slot::setSiteData(uint32_t netId, uint8_t siteId, uint8_t channelId, uint32_t channelNo)
{
    m_siteData = SiteData(SITE_MODEL_SMALL, netId, siteId, 3U, false);
    m_channelNo = channelNo;

    std::vector<lookups::IdenTable> entries = m_idenTable->list();
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        lookups::IdenTable entry = *it;
        if (entry.channelId() == channelId) {
            m_idenEntry = entry;
            break;
        }
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Write data processed from RF to the data ring buffer.
/// </summary>
/// <param name="data"></param>
void Slot::writeQueueRF(const uint8_t *data)
{
    assert(data != NULL);

    if (m_netState != RS_NET_IDLE)
        return;

    uint8_t len = DMR_FRAME_LENGTH_BYTES + 2U;

    uint32_t space = m_queue.freeSpace();
    if (space < (len + 1U)) {
        uint32_t queueLen = m_queue.length();
        m_queue.resize(queueLen + QUEUE_RESIZE_SIZE);

        LogError(LOG_DMR, "Slot %u, overflow in the DMR slot RF queue; queue resized was %u is %u", m_slotNo, queueLen, m_queue.length());
        return;
    }

    if (m_debug) {
        Utils::symbols("!!! *Tx DMR", data + 2U, len - 2U);
    }

    m_queue.addData(&len, 1U);
    m_queue.addData(data, len);
}

/// <summary>
/// Write data processed from the network to the data ring buffer.
/// </summary>
/// <param name="data"></param>
void Slot::writeQueueNet(const uint8_t *data)
{
    assert(data != NULL);

    uint8_t len = DMR_FRAME_LENGTH_BYTES + 2U;

    uint32_t space = m_queue.freeSpace();
    if (space < (len + 1U)) {
        LogError(LOG_DMR, "Slot %u, overflow in the DMR slot RF queue", m_slotNo);
        return;
    }

    if (m_debug) {
        Utils::symbols("!!! *Tx DMR", data + 2U, len - 2U);
    }

    m_queue.addData(&len, 1U);
    m_queue.addData(data, len);
}

/// <summary>
/// Write data processed from RF to the network.
/// </summary>
/// <param name="data"></param>
/// <param name="dataType"></param>
/// <param name="errors"></param>
void Slot::writeNetworkRF(const uint8_t* data, uint8_t dataType, uint8_t errors)
{
    assert(data != NULL);
    assert(m_rfLC != NULL);

    writeNetworkRF(data, dataType, m_rfLC->getFLCO(), m_rfLC->getSrcId(), m_rfLC->getDstId(), errors);
}

/// <summary>
/// Write data processed from RF to the network.
/// </summary>
/// <param name="data"></param>
/// <param name="dataType"></param>
/// <param name="flco"></param>
/// <param name="srcId"></param>
/// <param name="dstId"></param>
/// <param name="errors"></param>
void Slot::writeNetworkRF(const uint8_t* data, uint8_t dataType, uint8_t flco, uint32_t srcId,
    uint32_t dstId, uint8_t errors)
{
    assert(data != NULL);

    if (m_netState != RS_NET_IDLE)
        return;

    if (m_network == NULL)
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
            setShortLC_TSCC(m_siteData, m_tsccCnt);
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
                writeQueueRF(data);
        }
    }

    m_data->m_pduDataOffset = 0U;

    if (m_network != NULL)
        m_network->resetDMR(m_slotNo);

    m_rfTimeoutTimer.stop();
    m_rfTimeout = false;

    m_rfFrames = 0U;
    m_rfErrs = 0U;
    m_rfBits = 1U;

    delete m_rfLC;
    if (m_rfPrivacyLC != NULL)
        delete m_rfPrivacyLC;
    if (m_rfDataHeader != NULL)
        delete m_rfDataHeader;

    m_rfLC = NULL;
    m_rfPrivacyLC = NULL;
    m_rfDataHeader = NULL;
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
                writeQueueNet(data);
        }
        else {
            for (uint32_t i = 0U; i < 3U; i++)
                writeQueueNet(data);
        }
    }

    m_data->m_pduDataOffset = 0U;

    if (m_network != NULL)
        m_network->resetDMR(m_slotNo);

    m_networkWatchdog.stop();
    m_netTimeoutTimer.stop();
    m_packetTimer.stop();
    m_netTimeout = false;

    m_netFrames = 0U;
    m_netLost = 0U;

    m_netErrs = 0U;
    m_netBits = 1U;

    delete m_netLC;
    if (m_netPrivacyLC != NULL)
        delete m_netPrivacyLC;
    if (m_netDataHeader != NULL)
        delete m_netDataHeader;

    m_netLC = NULL;
    m_netPrivacyLC = NULL;
    m_netDataHeader = NULL;
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

    // loop to generate 2 control sequences
    if (frameCnt == 511U) {
        seqCnt = 3U;
    }

    do
    {
        if (m_debug) {
            LogDebug(LOG_DMR, "writeRF_ControlData, frameCnt = %u, seq = %u", frameCnt, n);
        }

        switch (n)
        {
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
    assert(m_modem != NULL);

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
/// <param name="slotNo"></param>
/// <param name="siteData"></param>
/// <param name="counter"></param>
void Slot::setShortLC_TSCC(SiteData siteData, uint16_t counter)
{
    assert(m_modem != NULL);

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

    uint8_t sLC[9U];

    lc::ShortLC shortLC;
    shortLC.encode(lc, sLC);

    m_modem->writeDMRShortLC(sLC);
}
