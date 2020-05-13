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
//  Static Class Members
// ---------------------------------------------------------------------------

uint32_t Slot::m_colorCode = 0U;

bool Slot::m_embeddedLCOnly = false;
bool Slot::m_dumpTAData = true;

modem::Modem* Slot::m_modem = NULL;
network::BaseNetwork* Slot::m_network = NULL;
bool Slot::m_duplex = true;
lookups::RadioIdLookup* Slot::m_ridLookup = NULL;
lookups::TalkgroupIdLookup* Slot::m_tidLookup = NULL;
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
/// <param name="debug">Flag indicating whether DMR debug is enabled.</param>
/// <param name="verbose">Flag indicating whether DMR verbose logging is enabled.</param>
Slot::Slot(uint32_t slotNo, uint32_t timeout, uint32_t tgHang, uint32_t queueSize, bool dumpDataPacket, bool repeatDataPacket,
    bool debug, bool verbose) :
    m_slotNo(slotNo),
    m_queue(queueSize, "DMR Slot"),
    m_rfState(RS_RF_LISTENING),
    m_rfLastDstId(0U),
    m_netState(RS_NET_IDLE),
    m_netLastDstId(0U),
    m_rfSeqNo(0U),
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
    m_verbose(verbose),
    m_debug(debug)
{
    m_interval.start();

    m_voice = new VoicePacket(this, m_network, m_embeddedLCOnly, m_dumpTAData, debug, verbose);
    m_data = new DataPacket(this, m_network, dumpDataPacket, repeatDataPacket, debug, verbose);
}

/// <summary>
/// Finalizes a instance of the Slot class.
/// </summary>
Slot::~Slot()
{
    delete m_voice;
    delete m_data;
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

    // Utils::dump(2U, "!!! *RX DMR Raw", data, len);

    if (data[0U] == TAG_LOST && m_rfState == RS_RF_AUDIO) {
        if (m_rssi != 0U) {
            ::ActivityLog("DMR", true, "Slot %u, RF voice transmission lost, %.1f seconds, BER: %.1f%%, RSSI: -%u/-%u/-%u dBm", 
                m_slotNo, float(m_rfFrames) / 16.667F, float(m_rfErrs * 100U) / float(m_rfBits), m_minRSSI, m_maxRSSI, m_aveRSSI / m_rssiCount);
        }
        else {
            ::ActivityLog("DMR", true, "Slot %u, RF voice transmission lost, %.1f seconds, BER: %.1f%%", 
                m_slotNo, float(m_rfFrames) / 16.667F, float(m_rfErrs * 100U) / float(m_rfBits));
        }

        LogMessage(LOG_RF, "DMR Slot %u, total frames: %d, total bits: %d, errors: %d, BER: %.4f%%", 
            m_slotNo, m_rfFrames, m_rfBits, m_rfErrs, float(m_rfErrs * 100U) / float(m_rfBits));

        if (m_rfTimeout) {
            m_data->writeEndRF();
            return false;
        }
        else {
            m_data->writeEndRF(true);
            return true;
        }
    }

    if (data[0U] == TAG_LOST && m_rfState == RS_RF_DATA) {
        ::ActivityLog("DMR", true, "Slot %u, RF data transmission lost", m_slotNo);
        m_data->writeEndRF();
        return false;
    }

    if (data[0U] == TAG_LOST) {
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
        if (m_verbose) {
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

            LogDebug(LOG_RF, "DMR, sync word rejected, dataErrs = %u, voiceErrs = %u", dataErrs, voiceErrs);
        }
    }

    if ((dataSync || voiceSync) && m_debug) {
        Utils::dump(1U, "!!! *RX DMR Modem Frame", data, len);
    }

    if ((dataSync || voiceSync) && m_rfState != RS_RF_LISTENING)
        m_rfTGHang.start();

    if (dataSync) {
        return m_data->process(data, len);
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
    case DT_VOICE_LC_HEADER:
    case DT_VOICE_PI_HEADER:
    case DT_TERMINATOR_WITH_LC:
    case DT_DATA_HEADER:
    case DT_CSBK:
    case DT_RATE_12_DATA:
    case DT_RATE_34_DATA:
    case DT_RATE_1_DATA:
        m_data->processNetwork(dmrData);
        break;
    case DT_VOICE_SYNC:
    case DT_VOICE:
    default:
        m_voice->processNetwork(dmrData);
    }
}

/// <summary>
/// Updates the DMR slot processor.
/// </summary>
void Slot::clock()
{
    uint32_t ms = m_interval.elapsed();
    m_interval.start();

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
                ::ActivityLog("DMR", false, "Slot %u, network watchdog has expired, %.1f seconds, %u%% packet loss, BER: %.1f%%", 
                    m_slotNo, float(m_netFrames) / 16.667F, (m_netLost * 100U) / m_netFrames, float(m_netErrs * 100U) / float(m_netBits));
                m_data->writeEndNet(true);
            }
            else {
                ::ActivityLog("DMR", false, "Slot %u, network watchdog has expired", m_slotNo);
                m_data->writeEndNet();
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
}

/// <summary>
/// Helper to initialize the DMR slot processor.
/// </summary>
/// <param name="colorCode">DMR access color code.</param>
/// <param name="embeddedLCOnly"></param>
/// <param name="dumpTAData"></param>
/// <param name="callHang">Amount of hangtime for a DMR call.</param>
/// <param name="modem">Instance of the Modem class.</param>
/// <param name="network">Instance of the BaseNetwork class.</param>
/// <param name="duplex">Flag indicating full-duplex operation.</param>
/// <param name="ridLookup">Instance of the RadioIdLookup class.</param>
/// <param name="tidLookup">Instance of the TalkgroupIdLookup class.</param>
/// <param name="rssi">Instance of the CRSSIInterpolator class.</param>
/// <param name="jitter"></param>
void Slot::init(uint32_t colorCode, bool embeddedLCOnly, bool dumpTAData, uint32_t callHang, modem::Modem* modem,
    network::BaseNetwork* network, bool duplex, lookups::RadioIdLookup* ridLookup, lookups::TalkgroupIdLookup* tidLookup,
    lookups::RSSIInterpolator* rssiMapper, uint32_t jitter)
{
    assert(modem != NULL);
    assert(ridLookup != NULL);
    assert(tidLookup != NULL);
    assert(rssiMapper != NULL);

    m_colorCode = colorCode;
    m_embeddedLCOnly = embeddedLCOnly;
    m_dumpTAData = dumpTAData;
    m_modem = modem;
    m_network = network;
    m_duplex = duplex;
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
        m_queue.resize(queueLen + 2500);

        LogError(LOG_DMR, "Slot %u, overflow in the DMR slot RF queue; queue resized was %u is %u", m_slotNo, queueLen, m_queue.length());
        return;
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
    assert(m_data->m_rfLC != NULL);

    writeNetworkRF(data, dataType, m_data->m_rfLC->getFLCO(), m_data->m_rfLC->getSrcId(), m_data->m_rfLC->getDstId(), errors);
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
/// Helper to write a extended function packet on the RF interface.
/// </summary>
/// <param name="func">Extended function opcode.</param>
/// <param name="arg">Extended function argument.</param>
/// <param name="dstId">Destination radio ID.</param>
void Slot::writeRF_Ext_Func(uint32_t func, uint32_t arg, uint32_t dstId)
{
    if (m_verbose) {
        LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_EXT_FNCT (Extended Function), op = $%02X, arg = %u, tgt = %u",
            m_slotNo, func, arg, dstId);
    }

    // generate activity log entry
    if (func == DMR_EXT_FNCT_CHECK) {
        ::ActivityLog("DMR", true, "Slot %u received radio check request from %u to %u", m_slotNo, arg, dstId);
    }
    else if (func == DMR_EXT_FNCT_INHIBIT) {
        ::ActivityLog("DMR", true, "Slot %u received radio inhibit request from %u to %u", m_slotNo, arg, dstId);
    }
    else if (func == DMR_EXT_FNCT_UNINHIBIT) {
        ::ActivityLog("DMR", true, "Slot %u received radio uninhibit request from %u to %u", m_slotNo, arg, dstId);
    }

    uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, DMR_FRAME_LENGTH_BYTES);

    SlotType slotType;
    slotType.setColorCode(m_colorCode);
    slotType.setDataType(DT_CSBK);

    lc::CSBK csbk = lc::CSBK(false);
    csbk.setCSBKO(CSBKO_EXT_FNCT);
    csbk.setFID(FID_DMRA);

    csbk.setGI(false);
    csbk.setCBF(func);
    csbk.setSrcId(arg);
    csbk.setDstId(dstId);

    // Regenerate the CSBK data
    csbk.encode(data + 2U);

    // Regenerate the Slot Type
    slotType.encode(data + 2U);

    // Convert the Data Sync to be from the BS or MS as needed
    Sync::addDMRDataSync(data + 2U, m_duplex);

    m_rfSeqNo = 0U;

    data[0U] = TAG_DATA;
    data[1U] = 0x00U;

    if (m_duplex)
        writeQueueRF(data);
}

/// <summary>
/// Helper to write a call alert packet on the RF interface.
/// </summary>
/// <param name="srcId">Source radio ID.</param>
/// <param name="dstId">Destination radio ID.</param>
void Slot::writeRF_Call_Alrt(uint32_t srcId, uint32_t dstId)
{
    if (m_verbose) {
        LogMessage(LOG_RF, "DMR Slot %u, DT_CSBK, CSBKO_CALL_ALRT (Call Alert), src = %u, dst = %u",
            m_slotNo, srcId, dstId);
    }

    ::ActivityLog("DMR", true, "Slot %u received call alert request from %u to %u", m_slotNo, srcId, dstId);

    uint8_t data[DMR_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, DMR_FRAME_LENGTH_BYTES);

    SlotType slotType;
    slotType.setColorCode(m_colorCode);
    slotType.setDataType(DT_CSBK);

    lc::CSBK csbk = lc::CSBK(false);
    csbk.setCSBKO(CSBKO_CALL_ALRT);
    csbk.setFID(FID_DMRA);

    csbk.setGI(false);
    csbk.setSrcId(srcId);
    csbk.setDstId(dstId);

    // Regenerate the CSBK data
    csbk.encode(data + 2U);

    // Regenerate the Slot Type
    slotType.encode(data + 2U);

    // Convert the Data Sync to be from the BS or MS as needed
    Sync::addDMRDataSync(data + 2U, m_duplex);

    m_rfSeqNo = 0U;

    data[0U] = TAG_DATA;
    data[1U] = 0x00U;

    if (m_duplex)
        writeQueueRF(data);
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
    lc[0U] = 0x01U;
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
