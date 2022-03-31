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

#define CHECK_TG_HANG(_DST_ID)                                                          \
    if (m_slot->m_rfLastDstId != 0U) {                                                  \
        if (m_slot->m_rfLastDstId != _DST_ID && (m_slot->m_rfTGHang.isRunning() && !m_slot->m_rfTGHang.hasExpired())) { \
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

    if (dataType == DT_TERMINATOR_WITH_LC) {
        if (m_slot->m_rfState != RS_RF_AUDIO)
            return false;

        // Regenerate the LC data
        lc::FullLC fullLC;
        fullLC.encode(*m_slot->m_rfLC, data + 2U, DT_TERMINATOR_WITH_LC);

        // Regenerate the Slot Type
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        if (!m_slot->m_rfTimeout) {
            data[0U] = modem::TAG_EOT;
            data[1U] = 0x00U;

            m_slot->writeNetworkRF(data, DT_TERMINATOR_WITH_LC);

            if (m_slot->m_duplex) {
                for (uint32_t i = 0U; i < m_slot->m_hangCount; i++)
                    m_slot->writeQueueRF(data);
            }
        }

        if (m_verbose) {
            LogMessage(LOG_RF, DMR_DT_TERMINATOR_WITH_LC ", slot = %u, dstId = %u", m_slot->m_slotNo, m_slot->m_rfLC->getDstId());
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

        if (m_slot->m_rfTimeout) {
            m_slot->writeEndRF();
            return false;
        }
        else {
            m_slot->writeEndRF();
            return true;
        }
    }
    else if (dataType == DT_DATA_HEADER) {
        if (m_slot->m_rfState == RS_RF_DATA)
            return true;

        data::DataHeader* dataHeader = new data::DataHeader();
        bool valid = dataHeader->decode(data + 2U);
        if (!valid)
            return false;

        m_slot->m_rfDataHeader = dataHeader;

        bool gi = dataHeader->getGI();
        uint32_t srcId = dataHeader->getSrcId();
        uint32_t dstId = dataHeader->getDstId();

        CHECK_TRAFFIC_COLLISION(dstId);

        // validate the source RID
        if (!acl::AccessControl::validateSrcId(srcId)) {
            LogWarning(LOG_RF, "DMR Slot %u, DT_DATA_HEADER denial, RID rejection, srcId = %u", m_slot->m_slotNo, srcId);
            m_slot->m_rfState = RS_RF_REJECTED;
            return false;
        }

        // validate the target ID
        if (gi) {
            if (!acl::AccessControl::validateTGId(m_slot->m_slotNo, dstId)) {
                LogWarning(LOG_RF, "DMR Slot %u, DT_DATA_HEADER denial, TGID rejection, srcId = %u, dstId = %u", m_slot->m_slotNo, srcId, dstId);
                m_slot->m_rfState = RS_RF_REJECTED;
                return false;
            }
        }

        m_slot->m_rfFrames = dataHeader->getBlocks();
        m_slot->m_rfSeqNo = 0U;
        m_slot->m_rfLC = new lc::LC(gi ? FLCO_GROUP : FLCO_PRIVATE, srcId, dstId);

        // Regenerate the data header
        dataHeader->encode(data + 2U);

        // Regenerate the Slot Type
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        data[0U] = m_slot->m_rfFrames == 0U ? modem::TAG_EOT : modem::TAG_DATA;
        data[1U] = 0x00U;

        if (m_slot->m_duplex && m_repeatDataPacket)
            m_slot->writeQueueRF(data);

        m_slot->writeNetworkRF(data, DT_DATA_HEADER);

        m_slot->m_rfState = RS_RF_DATA;
        m_slot->m_rfLastDstId = dstId;

        if (m_slot->m_netState == RS_NET_IDLE) {
            m_slot->setShortLC(m_slot->m_slotNo, dstId, gi ? FLCO_GROUP : FLCO_PRIVATE, false);
        }

        if (m_verbose) {
            LogMessage(LOG_RF, DMR_DT_DATA_HEADER ", slot = %u, dpf = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padCount = %u, seqNo = %u, dstId = %u, srcId = %u, group = %u",
                m_slot->m_slotNo, dataHeader->getDPF(), dataHeader->getSAP(), dataHeader->getFullMesage(), dataHeader->getBlocks(), dataHeader->getPadCount(), dataHeader->getFSN(),
                dstId, srcId, gi);
        }

        ::ActivityLog("DMR", true, "Slot %u RF data header from %u to %s%u, %u blocks", m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId, m_slot->m_rfFrames);

        ::memset(m_pduUserData, 0x00U, DMR_MAX_PDU_COUNT * DMR_MAX_PDU_LENGTH + 2U);
        m_pduDataOffset = 0U;

        if (m_slot->m_rfFrames == 0U) {
            ::ActivityLog("DMR", true, "Slot %u ended RF data transmission", m_slot->m_slotNo);
            m_slot->writeEndRF();
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
                Utils::dump(1U, "Unfixable PDU Data", data + 2U, DMR_FRAME_LENGTH_BYTES);
            }

            m_pduDataOffset += 18U;
        }

        m_slot->m_rfFrames--;

        data[0U] = m_slot->m_rfFrames == 0U ? modem::TAG_EOT : modem::TAG_DATA;
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
            m_slot->writeEndRF();
        }

        if (m_verbose) {
            if (dataType == DT_RATE_12_DATA) {
                LogMessage(LOG_RF, DMR_DT_RATE_12_DATA ", block = %u", m_slot->m_rfFrames + 1);
            }
            else if (dataType == DT_RATE_34_DATA) {
                LogMessage(LOG_RF, DMR_DT_RATE_34_DATA ", block = %u", m_slot->m_rfFrames + 1);
            }
            else {
                LogMessage(LOG_RF, DMR_DT_RATE_1_DATA ", block = %u", m_slot->m_rfFrames + 1);
            }
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

    if (dataType == DT_TERMINATOR_WITH_LC) {
        if (m_slot->m_netState != RS_NET_AUDIO)
            return;

        // Regenerate the LC data
        lc::FullLC fullLC;
        fullLC.encode(*m_slot->m_netLC, data + 2U, DT_TERMINATOR_WITH_LC);

        // Regenerate the Slot Type
        SlotType slotType;
        slotType.setColorCode(m_slot->m_colorCode);
        slotType.setDataType(DT_TERMINATOR_WITH_LC);
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        if (!m_slot->m_netTimeout) {
            data[0U] = modem::TAG_EOT;
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

        if (m_verbose) {
            LogMessage(LOG_RF, DMR_DT_TERMINATOR_WITH_LC ", slot = %u, dstId = %u", m_slot->m_slotNo, m_slot->m_netLC->getDstId());
        }

        // We've received the voice header and terminator haven't we?
        m_slot->m_netFrames += 2U;
        ::ActivityLog("DMR", false, "Slot %u network end of voice transmission, %.1f seconds, %u%% packet loss, BER: %.1f%%",
            m_slot->m_slotNo, float(m_slot->m_netFrames) / 16.667F, (m_slot->m_netLost * 100U) / m_slot->m_netFrames, float(m_slot->m_netErrs * 100U) / float(m_slot->m_netBits));

        m_slot->writeEndNet();
    }
    else if (dataType == DT_DATA_HEADER) {
        if (m_slot->m_netState == RS_NET_DATA)
            return;

        data::DataHeader* dataHeader = new data::DataHeader();
        bool valid = dataHeader->decode(data + 2U);
        if (!valid) {
            LogError(LOG_NET, "DMR Slot %u, DT_DATA_HEADER, unable to decode the network data header", m_slot->m_slotNo);
            return;
        }

        m_slot->m_netDataHeader = dataHeader;

        bool gi = dataHeader->getGI();
        uint32_t srcId = dataHeader->getSrcId();
        uint32_t dstId = dataHeader->getDstId();

        CHECK_TG_HANG(dstId);

        m_slot->m_netFrames = dataHeader->getBlocks();
        m_slot->m_netLC = new lc::LC(gi ? FLCO_GROUP : FLCO_PRIVATE, srcId, dstId);

        // Regenerate the data header
        dataHeader->encode(data + 2U);

        // Regenerate the Slot Type
        SlotType slotType;
        slotType.setColorCode(m_slot->m_colorCode);
        slotType.setDataType(DT_DATA_HEADER);
        slotType.encode(data + 2U);

        // Convert the Data Sync to be from the BS or MS as needed
        Sync::addDMRDataSync(data + 2U, m_slot->m_duplex);

        data[0U] = m_slot->m_netFrames == 0U ? modem::TAG_EOT : modem::TAG_DATA;
        data[1U] = 0x00U;

        // Put a small delay into starting transmission
        m_slot->writeQueueNet(m_slot->m_idle);
        m_slot->writeQueueNet(m_slot->m_idle);

        m_slot->writeQueueNet(data);

        m_slot->m_netState = RS_NET_DATA;
        m_slot->m_netLastDstId = dstId;

        m_slot->setShortLC(m_slot->m_slotNo, dstId, gi ? FLCO_GROUP : FLCO_PRIVATE, false);

        if (m_verbose) {
            LogMessage(LOG_NET, DMR_DT_DATA_HEADER ", slot = %u, dpf = $%02X, sap = $%02X, fullMessage = %u, blocksToFollow = %u, padCount = %u, seqNo = %u, dstId = %u, srcId = %u, group = %u",
                m_slot->m_slotNo, dataHeader->getDPF(), dataHeader->getSAP(), dataHeader->getFullMesage(), dataHeader->getBlocks(), dataHeader->getPadCount(), dataHeader->getFSN(),
                dstId, srcId, gi);
        }

        ::ActivityLog("DMR", false, "Slot %u network data header from %u to %s%u, %u blocks",
            m_slot->m_slotNo, srcId, gi ? "TG " : "", dstId, m_slot->m_netFrames);

        ::memset(m_pduUserData, 0x00U, DMR_MAX_PDU_COUNT * DMR_MAX_PDU_LENGTH + 2U);
        m_pduDataOffset = 0U;

        if (m_slot->m_netFrames == 0U) {
            ::ActivityLog("DMR", false, "Slot %u ended network data transmission", m_slot->m_slotNo);
            m_slot->writeEndNet();
        }
    }
    else if (dataType == DT_RATE_12_DATA || dataType == DT_RATE_34_DATA || dataType == DT_RATE_1_DATA) {
        if (m_slot->m_netState != RS_NET_DATA || m_slot->m_netFrames == 0U) {
            m_slot->writeEndNet();
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

            data[0U] = m_slot->m_netFrames == 0U ? modem::TAG_EOT : modem::TAG_DATA;
            data[1U] = 0x00U;

            m_slot->writeQueueNet(data);

            if (m_verbose) {
                if (dataType == DT_RATE_12_DATA) {
                    LogMessage(LOG_RF, DMR_DT_RATE_12_DATA ", block = %u", m_slot->m_netFrames + 1);
                }
                else if (dataType == DT_RATE_34_DATA) {
                    LogMessage(LOG_RF, DMR_DT_RATE_34_DATA ", block = %u", m_slot->m_netFrames + 1);
                }
                else {
                    LogMessage(LOG_RF, DMR_DT_RATE_1_DATA ", block = %u", m_slot->m_netFrames + 1);
                }
            }
        }

        if (m_slot->m_netFrames == 0U) {
            if (m_dumpDataPacket) {
                Utils::dump(1U, "PDU Packet", m_pduUserData, m_pduDataOffset);
            }

            LogMessage(LOG_NET, "DMR Slot %u, DT_RATE_12/34_DATA, ended data transmission", m_slot->m_slotNo);
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
/// <summary>
/// Initializes a new instance of the DataPacket class.
/// </summary>
/// <param name="slot">DMR slot.</param>
/// <param name="network">Instance of the BaseNetwork class.</param>
/// <param name="dumpDataPacket"></param>
/// <param name="repeatDataPacket"></param>
/// <param name="debug">Flag indicating whether DMR debug is enabled.</param>
/// <param name="verbose">Flag indicating whether DMR verbose logging is enabled.</param>
DataPacket::DataPacket(Slot* slot, network::BaseNetwork* network, bool dumpDataPacket, bool repeatDataPacket, bool debug, bool verbose) :
    m_slot(slot),
    m_pduUserData(NULL),
    m_pduDataOffset(0U),
    m_lastRejectId(0U),
    m_dumpDataPacket(dumpDataPacket),
    m_repeatDataPacket(repeatDataPacket),
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
