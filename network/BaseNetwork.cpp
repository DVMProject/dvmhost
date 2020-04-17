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
*   Copyright (C) 2015,2016,2017 by Jonathan Naylor G4KLX
*   Copyright (C) 2020 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include "Defines.h"
#include "edac/SHA256.h"
#include "network/BaseNetwork.h"
#include "Log.h"
#include "StopWatch.h"
#include "Utils.h"

using namespace network;

#include <cstdio>
#include <cassert>
#include <cstdlib>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the BaseNetwork class.
/// </summary>
/// <param name="localPort">Local port used to listen for incoming data.</param>
/// <param name="id">Unique ID of this modem on the network.</param>
/// <param name="duplex">Flag indicating full-duplex operation.</param>
/// <param name="debug"></param>
/// <param name="slot1">Flag indicating whether DMR slot 1 is enabled for network traffic.</param>
/// <param name="slot2">Flag indicating whether DMR slot 2 is enabled for network traffic.</param>
/// <param name="transferActivityLog">Flag indicating that the system activity log will be sent to the network.</param>
BaseNetwork::BaseNetwork(uint32_t localPort, uint32_t id, bool duplex, bool debug, bool slot1, bool slot2, bool transferActivityLog) :
    m_id(id),
    m_slot1(slot1),
    m_slot2(slot2),
    m_transferActivityLog(transferActivityLog),
    m_duplex(duplex),
    m_debug(debug),
    m_socket(localPort),
    m_status(NET_STAT_INVALID),
    m_retryTimer(1000U, 10U),
    m_timeoutTimer(1000U, 60U),
    m_buffer(NULL),
    m_salt(NULL),
    m_streamId(NULL),
    m_p25StreamId(0U),
    m_rxDMRData(4000U, "DMR Net Buffer"),
    m_rxP25Data(4000U, "P25 Net Buffer"),
    m_audio()
{
    assert(id > 1000U);

    m_buffer = new uint8_t[DATA_PACKET_LENGTH];
    m_salt = new uint8_t[sizeof(uint32_t)];
    m_streamId = new uint32_t[2U];

    m_p25StreamId = 0U;
    m_streamId[0U] = 0x00U;
    m_streamId[1U] = 0x00U;

    StopWatch stopWatch;
    ::srand((uint32_t)stopWatch.start());
}

/// <summary>
/// Finalizes a instance of the BaseNetwork class.
/// </summary>
BaseNetwork::~BaseNetwork()
{
    delete[] m_buffer;
    delete[] m_salt;
    delete[] m_streamId;
}

/// <summary>
/// Reads DMR frame data from the DMR ring buffer.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
bool BaseNetwork::readDMR(dmr::data::Data& data)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    if (m_rxDMRData.isEmpty())
        return false;

    uint8_t length = 0U;
    m_rxDMRData.getData(&length, 1U);
    m_rxDMRData.getData(m_buffer, length);

    uint8_t seqNo = m_buffer[4U];

    uint32_t srcId = (m_buffer[5U] << 16) | (m_buffer[6U] << 8) | (m_buffer[7U] << 0);

    uint32_t dstId = (m_buffer[8U] << 16) | (m_buffer[9U] << 8) | (m_buffer[10U] << 0);

    uint8_t flco = (m_buffer[15U] & 0x40U) == 0x40U ? dmr::FLCO_PRIVATE : dmr::FLCO_GROUP;

    uint32_t slotNo = (m_buffer[15U] & 0x80U) == 0x80U ? 2U : 1U;

    // DMO mode slot disabling
    if (slotNo == 1U && !m_duplex)
        return false;

    // Individual slot disabling
    if (slotNo == 1U && !m_slot1)
        return false;
    if (slotNo == 2U && !m_slot2)
        return false;

    data.setSeqNo(seqNo);
    data.setSlotNo(slotNo);
    data.setSrcId(srcId);
    data.setDstId(dstId);
    data.setFLCO(flco);

    bool dataSync = (m_buffer[15U] & 0x20U) == 0x20U;
    bool voiceSync = (m_buffer[15U] & 0x10U) == 0x10U;

    if (m_debug) {
        LogDebug(LOG_NET, "DMR, seqNo = %u, srcId = %u, dstId = %u, flco = $%02X, slotNo = %u, len = %u", seqNo, srcId, dstId, flco, slotNo, length);
    }

    if (dataSync) {
        uint8_t dataType = m_buffer[15U] & 0x0FU;
        data.setData(m_buffer + 20U);
        data.setDataType(dataType);
        data.setN(0U);
    }
    else if (voiceSync) {
        data.setData(m_buffer + 20U);
        data.setDataType(dmr::DT_VOICE_SYNC);
        data.setN(0U);
    }
    else {
        uint8_t n = m_buffer[15U] & 0x0FU;
        data.setData(m_buffer + 20U);
        data.setDataType(dmr::DT_VOICE);
        data.setN(n);
    }

    return true;
}

/// <summary>
/// Reads P25 frame data from the P25 ring buffer.
/// </summary>
/// <param name="ret"></param>
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <param name="duid"></param>
/// <param name="len"></param>
/// <returns></returns>
uint8_t* BaseNetwork::readP25(bool& ret, p25::lc::LC& control, p25::data::LowSpeedData& lsd, uint8_t& duid, uint32_t& len)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING) {
        ret = false;
        return NULL;
    }

    if (m_rxP25Data.isEmpty()) {
        ret = false;
        return NULL;
    }

    uint8_t length = 0U;
    m_rxP25Data.getData(&length, 1U);
    m_rxP25Data.getData(m_buffer, length);

    uint8_t lco = m_buffer[4U];

    uint32_t srcId = (m_buffer[5U] << 16) | (m_buffer[6U] << 8) | (m_buffer[7U] << 0);

    uint32_t dstId = (m_buffer[8U] << 16) | (m_buffer[9U] << 8) | (m_buffer[10U] << 0);

    uint8_t MFId = m_buffer[15U];

    uint8_t lsd1 = m_buffer[20U];
    uint8_t lsd2 = m_buffer[21U];

    duid = m_buffer[22U];

    if (m_debug) {
        LogDebug(LOG_NET, "P25, lco = $%02X, MFId = $%02X, srcId = %u, dstId = %u, len = %u", lco, MFId, srcId, dstId, length);
    }

    control.setLCO(lco);
    control.setSrcId(srcId);
    control.setDstId(dstId);
    control.setMFId(MFId);

    lsd.setLSD1(lsd1);
    lsd.setLSD2(lsd2);

    uint8_t* data = NULL;
    len = m_buffer[23U];
    if (len <= 24) {
        data = new uint8_t[len];
        ::memset(data, 0x00U, len);
    }
    else {
        data = new uint8_t[len];
        ::memset(data, 0x00U, len);
        ::memcpy(data, m_buffer + 24U, len);
    }

    ret = true;
    return data;
}

/// <summary>
/// Writes DMR frame data to the network.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
bool BaseNetwork::writeDMR(const dmr::data::Data& data)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    uint32_t slotNo = data.getSlotNo();

    // individual slot disabling
    if (slotNo == 1U && !m_slot1)
        return false;
    if (slotNo == 2U && !m_slot2)
        return false;

    uint8_t dataType = data.getDataType();

    uint32_t slotIndex = slotNo - 1U;

    if (dataType == dmr::DT_VOICE_LC_HEADER) {
        m_streamId[slotIndex] = ::rand() + 1U;
    }

    if (dataType == dmr::DT_CSBK || dataType == dmr::DT_DATA_HEADER) {
        m_streamId[slotIndex] = ::rand() + 1U;
    }

    return writeDMR(m_id, m_streamId[slotIndex], data);
}

/// <summary>
/// Writes P25 LDU1 frame data to the network.
/// </summary>
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <param name="data"></param>
/// <returns></returns>
bool BaseNetwork::writeP25LDU1(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t* data)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    if (m_p25StreamId == 0U)
        m_p25StreamId = ::rand() + 1U;

    m_streamId[0] = m_p25StreamId;

    return writeP25LDU1(m_id, m_p25StreamId, control, lsd, data);
}

/// <summary>
/// Writes P25 LDU2 frame data to the network.
/// </summary>
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <param name="data"></param>
/// <returns></returns>
bool BaseNetwork::writeP25LDU2(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t* data)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    if (m_p25StreamId == 0U)
        m_p25StreamId = ::rand() + 1U;

    m_streamId[0] = m_p25StreamId;

    return writeP25LDU2(m_id, m_p25StreamId, control, lsd, data);
}

/// <summary>
/// Writes P25 TDU frame data to the network.
/// </summary>
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <returns></returns>
bool BaseNetwork::writeP25TDU(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    if (m_p25StreamId == 0U)
        m_p25StreamId = ::rand() + 1U;

    m_streamId[0] = m_p25StreamId;

    return writeP25TDU(m_id, m_p25StreamId, control, lsd);
}

/// <summary>
/// Writes P25 TSDU frame data to the network.
/// </summary>
/// <param name="tsbk"></param>
/// <param name="data"></param>
/// <returns></returns>
bool BaseNetwork::writeP25TSDU(const p25::lc::TSBK& tsbk, const uint8_t* data)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    if (m_p25StreamId == 0U)
        m_p25StreamId = ::rand() + 1U;

    m_streamId[0] = m_p25StreamId;

    return writeP25TSDU(m_id, m_p25StreamId, tsbk, data);
}

/// <summary>
/// Writes P25 PDU frame data to the network.
/// </summary>
/// <param name="llId"></param>
/// <param name="dataType"></param>
/// <param name="data"></param>
/// <param name="len"></param>
/// <returns></returns>
bool BaseNetwork::writeP25PDU(const uint32_t llId, const uint8_t dataType, const uint8_t* data, const uint32_t len)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    if (m_p25StreamId == 0U)
        m_p25StreamId = ::rand() + 1U;

    m_streamId[0] = m_p25StreamId;

    return writeP25PDU(m_id, m_p25StreamId, llId, dataType, data, len);
}

/// <summary>
/// Writes the local activity log to the network.
/// </summary>
/// <param name="message"></param>
/// <returns></returns>
bool BaseNetwork::writeActLog(const char* message)
{
    if (!m_transferActivityLog)
        return false;
    if (m_status != NET_STAT_RUNNING)
        return false;

    assert(message != NULL);

    char buffer[DATA_PACKET_LENGTH];
    uint32_t len = ::strlen(message);

    ::memcpy(buffer + 0U, TAG_REPEATER_LOG, 7U);
    __SET_UINT32(m_id, buffer, 7U);
    ::strcpy(buffer + 11U, message);

    return write((uint8_t*)buffer, (uint32_t)len + 12U);
}

/// <summary>
/// Resets the DMR ring buffer for the given slot.
/// </summary>
/// <param name="slotNo">DMR slot ring buffer to reset.</param>
void BaseNetwork::resetDMR(uint32_t slotNo)
{
    assert(slotNo == 1U || slotNo == 2U);

    if (slotNo == 1U) {
        m_streamId[0U] = ::rand() + 1U;
    }
    else {
        m_streamId[1U] = ::rand() + 1U;
    }

    m_rxDMRData.clear();
}

/// <summary>
/// Resets the P25 ring buffer.
/// </summary>
void BaseNetwork::resetP25()
{
    m_p25StreamId = ::rand() + 1U;
    m_streamId[0] = m_p25StreamId;

    m_rxP25Data.clear();
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Writes DMR frame data to the network.
/// </summary>
/// <param name="id"></param>
/// <param name="streamId"></param>
/// <param name="data"></param>
/// <returns></returns>
bool BaseNetwork::writeDMR(const uint32_t id, const uint32_t streamId, const dmr::data::Data& data)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    ::memcpy(buffer + 0U, TAG_DMR_DATA, 4U);

    uint32_t srcId = data.getSrcId();                                               // Source Address
    __SET_UINT16(srcId, buffer, 5U);

    uint32_t dstId = data.getDstId();                                               // Target Address
    __SET_UINT16(dstId, buffer, 8U);

    __SET_UINT32(id, buffer, 11U);

    uint32_t slotNo = data.getSlotNo();

    // Individual slot disabling
    if (slotNo == 1U && !m_slot1)
        return false;
    if (slotNo == 2U && !m_slot2)
        return false;

    buffer[15U] = slotNo == 1U ? 0x00U : 0x80U;                                     // Slot Number

    uint8_t flco = data.getFLCO();
    buffer[15U] |= flco == dmr::FLCO_GROUP ? 0x00U : 0x40U;                         // Group

    uint32_t count = 1U;

    uint8_t dataType = data.getDataType();
    if (dataType == dmr::DT_VOICE_SYNC) {
        buffer[15U] |= 0x10U;
    }
    else if (dataType == dmr::DT_VOICE) {
        buffer[15U] |= data.getN();
    }
    else {
        if (dataType == dmr::DT_VOICE_LC_HEADER) {
            count = 2U;
        }

        if (dataType == dmr::DT_CSBK || dataType == dmr::DT_DATA_HEADER) {
            count = 1U;
        }

        buffer[15U] |= (0x20U | dataType);
    }

    buffer[4U] = data.getSeqNo();                                                   // Sequence Number

    __SET_UINT32(streamId, buffer, 16U);

    data.getData(buffer + 20U);

    buffer[53U] = data.getBER();                                                    // Bit Error Rate
    buffer[54U] = data.getRSSI();                                                   // RSSI

    if (m_debug)
        Utils::dump(1U, "Network Transmitted, DMR", buffer, (DMR_PACKET_SIZE + PACKET_PAD));

    for (uint32_t i = 0U; i < count; i++)
        write(buffer, (DMR_PACKET_SIZE + PACKET_PAD));

    return true;
}

/// <summary>
/// Writes P25 LDU1 frame data to the network.
/// </summary>
/// <param name="id"></param>
/// <param name="streamId"></param>
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <param name="data"></param>
/// <returns></returns>
bool BaseNetwork::writeP25LDU1(const uint32_t id, const uint32_t streamId, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd,
    const uint8_t* data)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    assert(data != NULL);

    uint8_t serviceOptions =
        (control.getEmergency() ? 0x80U : 0x00U) +
        (control.getEncrypted() ? 0x40U : 0x00U) +
        (control.getPriority() & 0x07U);

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    ::memcpy(buffer + 0U, TAG_P25_DATA, 4U);

    buffer[4U] = control.getLCO();                                                  // LCO

    uint32_t srcId = control.getSrcId();                                            // Source Address
    __SET_UINT16(srcId, buffer, 5U);

    uint32_t dstId = control.getDstId();                                            // Target Address
    __SET_UINT16(dstId, buffer, 8U);

    __SET_UINT32(id, buffer, 11U);

    buffer[15U] = control.getMFId();                                                // MFId

    __SET_UINT32(streamId, buffer, 16U);

    buffer[20U] = lsd.getLSD1();                                                    // LSD 1
    buffer[21U] = lsd.getLSD2();                                                    // LSD 2

    buffer[22U] = p25::P25_DUID_LDU1;                                               // DUID

    uint32_t count = 24U;
    uint8_t tempBuf[22U];

    // The '62' record
    ::memcpy(tempBuf, LDU1_REC62, 22U);
    m_audio.decode(data, tempBuf + 10U, 0U);
    ::memcpy(buffer + 24U, tempBuf, 22U);
    count += 22U;

    // The '63' record
    ::memcpy(tempBuf, LDU1_REC63, 14U);
    m_audio.decode(data, tempBuf + 1U, 1U);
    ::memcpy(buffer + 46U, tempBuf, 14U);
    count += 14U;

    // The '64' record
    ::memcpy(tempBuf, LDU1_REC64, 17U);
    tempBuf[1U] = control.getLCO();
    tempBuf[2U] = control.getMFId();
    m_audio.decode(data, tempBuf + 5U, 2U);
    ::memcpy(buffer + 60U, tempBuf, 17U);
    count += 17U;

    // The '65' record
    ::memcpy(tempBuf, LDU1_REC65, 17U);
    tempBuf[1U] = (dstId >> 16) & 0xFFU;
    tempBuf[2U] = (dstId >> 8) & 0xFFU;
    tempBuf[3U] = (dstId >> 0) & 0xFFU;
    m_audio.decode(data, tempBuf + 5U, 3U);
    ::memcpy(buffer + 77U, tempBuf, 17U);
    count += 17U;

    // The '66' record
    ::memcpy(tempBuf, LDU1_REC66, 17U);
    tempBuf[1U] = (srcId >> 16) & 0xFFU;
    tempBuf[2U] = (srcId >> 8) & 0xFFU;
    tempBuf[3U] = (srcId >> 0) & 0xFFU;
    m_audio.decode(data, tempBuf + 5U, 4U);
    ::memcpy(buffer + 94U, tempBuf, 17U);
    count += 17U;

    // The '67' record
    ::memcpy(tempBuf, LDU1_REC67, 17U);
    m_audio.decode(data, tempBuf + 5U, 5U);
    ::memcpy(buffer + 111U, tempBuf, 17U);
    count += 17U;

    // The '68' record
    ::memcpy(tempBuf, LDU1_REC68, 17U);
    m_audio.decode(data, tempBuf + 5U, 6U);
    ::memcpy(buffer + 128U, tempBuf, 17U);
    count += 17U;

    // The '69' record
    ::memcpy(tempBuf, LDU1_REC69, 17U);
    m_audio.decode(data, tempBuf + 5U, 7U);
    ::memcpy(buffer + 145U, tempBuf, 17U);
    count += 17U;

    // The '6A' record
    ::memcpy(tempBuf, LDU1_REC6A, 16U);
    tempBuf[1U] = serviceOptions;
    m_audio.decode(data, tempBuf + 4U, 8U);
    ::memcpy(buffer + 162U, tempBuf, 16U);
    count += 16U;

    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "Network Transmitted, P25 LDU1", buffer, (count + PACKET_PAD));

    write(buffer, (count + PACKET_PAD));

    return true;
}

/// <summary>
/// Writes P25 LDU2 frame data to the network.
/// </summary>
/// <param name="id"></param>
/// <param name="streamId"></param>
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <param name="data"></param>
/// <returns></returns>
bool BaseNetwork::writeP25LDU2(const uint32_t id, const uint32_t streamId, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd,
    const uint8_t* data)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    assert(data != NULL);

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    ::memcpy(buffer + 0U, TAG_P25_DATA, 4U);

    buffer[4U] = control.getLCO();                                                  // LCO

    uint32_t srcId = control.getSrcId();                                            // Source Address
    __SET_UINT16(srcId, buffer, 5U);

    uint32_t dstId = control.getDstId();                                            // Target Address
    __SET_UINT16(dstId, buffer, 8U);

    __SET_UINT32(id, buffer, 11U);

    buffer[15U] = control.getMFId();                                                // MFId

    __SET_UINT32(streamId, buffer, 16U);

    buffer[20U] = lsd.getLSD1();                                                    // LSD 1
    buffer[21U] = lsd.getLSD2();                                                    // LSD 2

    buffer[22U] = p25::P25_DUID_LDU2;                                               // DUID

    uint32_t count = 24U;
    uint8_t tempBuf[22U];

    // The '6B' record
    ::memcpy(tempBuf, LDU2_REC6B, 22U);
    m_audio.decode(data, tempBuf + 10U, 0U);
    ::memcpy(buffer + 24U, tempBuf, 22U);
    count += 22U;

    // The '6C' record
    ::memcpy(tempBuf, LDU2_REC6C, 14U);
    m_audio.decode(data, tempBuf + 1U, 1U);
    ::memcpy(buffer + 46U, tempBuf, 14U);
    count += 14U;

    // generate MI data
    uint8_t mi[p25::P25_MI_LENGTH_BYTES];
    control.getMI(mi);

    // The '6D' record
    ::memcpy(tempBuf, LDU2_REC6D, 17U);
    tempBuf[1U] = mi[0U];
    tempBuf[2U] = mi[1U];
    tempBuf[3U] = mi[2U];
    m_audio.decode(data, tempBuf + 5U, 2U);
    ::memcpy(buffer + 60U, tempBuf, 17U);
    count += 17U;

    // The '6E' record
    ::memcpy(tempBuf, LDU2_REC6E, 17U);
    tempBuf[1U] = mi[3U];
    tempBuf[2U] = mi[4U];
    tempBuf[3U] = mi[5U];
    m_audio.decode(data, tempBuf + 5U, 3U);
    ::memcpy(buffer + 77U, tempBuf, 17U);
    count += 17U;

    // The '6F' record
    ::memcpy(tempBuf, LDU2_REC6F, 17U);
    tempBuf[1U] = mi[6U];
    tempBuf[2U] = mi[7U];
    tempBuf[3U] = mi[8U];
    m_audio.decode(data, tempBuf + 5U, 4U);
    ::memcpy(buffer + 94U, tempBuf, 17U);
    count += 17U;

    // The '70' record
    ::memcpy(tempBuf, LDU2_REC70, 17U);
    tempBuf[1U] = control.getAlgId();
    uint32_t kid = control.getKId();
    tempBuf[2U] = (kid >> 8) & 0xFFU;
    tempBuf[3U] = (kid >> 0) & 0xFFU;
    m_audio.decode(data, tempBuf + 5U, 5U);
    ::memcpy(buffer + 111U, tempBuf, 17U);
    count += 17U;

    // The '71' record
    ::memcpy(tempBuf, LDU2_REC71, 17U);
    m_audio.decode(data, tempBuf + 5U, 6U);
    ::memcpy(buffer + 128U, tempBuf, 17U);
    count += 17U;

    // The '72' record
    ::memcpy(tempBuf, LDU2_REC72, 17U);
    m_audio.decode(data, tempBuf + 5U, 7U);
    ::memcpy(buffer + 145U, tempBuf, 17U);
    count += 17U;

    // The '73' record
    ::memcpy(tempBuf, LDU2_REC73, 16U);
    m_audio.decode(data, tempBuf + 4U, 8U);
    ::memcpy(buffer + 162U, tempBuf, 16U);

    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "Network Transmitted, P25 LDU2", buffer, (count + PACKET_PAD));

    write(buffer, (count + PACKET_PAD));

    return true;
}

/// <summary>
/// Writes P25 TDU frame data to the network.
/// </summary>
/// <param name="id"></param>
/// <param name="streamId"></param>
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <returns></returns>
bool BaseNetwork::writeP25TDU(const uint32_t id, const uint32_t streamId, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    ::memcpy(buffer + 0U, TAG_P25_DATA, 4U);

    buffer[4U] = control.getLCO();                                                  // LCO

    uint32_t srcId = control.getSrcId();                                            // Source Address
    __SET_UINT16(srcId, buffer, 5U);

    uint32_t dstId = control.getDstId();                                            // Target Address
    __SET_UINT16(dstId, buffer, 8U);

    __SET_UINT32(id, buffer, 11U);

    buffer[15U] = control.getMFId();                                                // MFId

    __SET_UINT32(streamId, buffer, 16U);

    buffer[20U] = lsd.getLSD1();                                                    // LSD 1
    buffer[21U] = lsd.getLSD2();                                                    // LSD 2

    buffer[22U] = p25::P25_DUID_TDU;                                                // DUID

    uint32_t count = 24U;
    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "Network Transmitted, P25 TDU", buffer, (count + PACKET_PAD));

    write(buffer, (count + PACKET_PAD));

    return true;
}

/// <summary>
/// Writes P25 TSDU frame data to the network.
/// </summary>
/// <param name="id"></param>
/// <param name="streamId"></param>
/// <param name="tsbk"></param>
/// <param name="data"></param>
/// <returns></returns>
bool BaseNetwork::writeP25TSDU(const uint32_t id, const uint32_t streamId, const p25::lc::TSBK& tsbk, const uint8_t* data)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    ::memcpy(buffer + 0U, TAG_P25_DATA, 4U);

    buffer[4U] = tsbk.getLCO();                                                     // LCO

    uint32_t srcId = tsbk.getSrcId();                                               // Source Address
    __SET_UINT16(srcId, buffer, 5U);

    uint32_t dstId = tsbk.getDstId();                                               // Target Address
    __SET_UINT16(dstId, buffer, 8U);

    __SET_UINT32(id, buffer, 11U);

    buffer[15U] = tsbk.getMFId();                                                   // MFId

    __SET_UINT32(streamId, buffer, 16U);

    buffer[20U] = 0U;                                                               // Reserved (LSD 1)
    buffer[21U] = 0U;                                                               // Reserved (LSD 2)

    buffer[22U] = p25::P25_DUID_TSDU;                                               // DUID

    uint32_t count = 24U;

    ::memcpy(buffer + 24U, data, p25::P25_TSDU_FRAME_LENGTH_BYTES);
    count += p25::P25_TSDU_FRAME_LENGTH_BYTES;

    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "Network Transmitted, P25 TDSU", buffer, (count + PACKET_PAD));

    write(buffer, (count + PACKET_PAD));

    return true;
}

/// <summary>
/// Writes P25 PDU frame data to the network.
/// </summary>
/// <param name="id"></param>
/// <param name="streamId"></param>
/// <param name="llId"></param>
/// <param name="dataType"></param>
/// <param name="data"></param>
/// <param name="len"></param>
/// <returns></returns>
bool BaseNetwork::writeP25PDU(const uint32_t id, const uint32_t streamId, const uint32_t llId, const uint8_t dataType, const uint8_t* data,
    const uint32_t len)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    assert(data != NULL);

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    ::memcpy(buffer + 0U, TAG_P25_DATA, 4U);

    buffer[4U] = dataType;                                                          // Data Type (LCO)

    __SET_UINT16(llId, buffer, 5U);                                                 // Logical Link Address (Source Address)

    __SET_UINT16(len, buffer, 8U);                                                  // PDU Length [bytes] (Target Address)

    __SET_UINT32(id, buffer, 11U);

    buffer[15U] = p25::P25_MFG_STANDARD;                                            // MFId

    __SET_UINT32(streamId, buffer, 16U);

    buffer[20U] = 0U;                                                               // Reserved (LSD 1)
    buffer[21U] = 0U;                                                               // Reserved (LSD 2)

    buffer[22U] = p25::P25_DUID_PDU;                                                // DUID

    uint32_t count = 24U;

    ::memcpy(buffer + 24U, data, len);
    count += len;

    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "Network Transmitted, P25 PDU", buffer, (count + PACKET_PAD));

    write(buffer, (count + PACKET_PAD));

    return true;
}

/// <summary>
/// Writes data to the network.
/// </summary>
/// <param name="buffer">Buffer to write to the network.</param>
/// <param name="length">Length of buffer to write.</param>
/// <param name="address">IP address to write data to.</param>
/// <param name="port">Port number for remote UDP socket.</param>
/// <returns>True, if buffer is written to the network, otherwise false.</returns>
bool BaseNetwork::write(const uint8_t* data, uint32_t length, const in_addr& address, uint32_t port)
{
    assert(data != NULL);
    assert(length > 0U);
    assert(port > 0U);

    // if (m_debug)
    //    Utils::dump(1U, "Network Transmitted", data, length);

    bool ret = m_socket.write(data, length, address, port);
    if (!ret) {
        LogError(LOG_NET, "Socket has failed when writing data to the peer, retrying connection");
        return false;
    }

    return true;
}
