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
*   Copyright (C) 2015-2020 by Jonathan Naylor G4KLX
*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
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
#include "nxdn/NXDNDefines.h"
#include "nxdn/channel/UDCH.h"
#include "nxdn/packet/Data.h"
#include "nxdn/acl/AccessControl.h"
#include "nxdn/Sync.h"
#include "nxdn/NXDNUtils.h"
#include "edac/CRC.h"
#include "HostMain.h"
#include "Log.h"
#include "Utils.h"

using namespace nxdn;
using namespace nxdn::packet;

#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

// Don't process RF frames if the network isn't in a idle state and the RF destination
// is the network destination and stop network frames from processing -- RF wants to
// transmit on a different talkgroup
#define CHECK_TRAFFIC_COLLISION(_SRC_ID, _DST_ID)                                       \
    if (m_nxdn->m_netState != RS_NET_IDLE && _DST_ID == m_nxdn->m_netLastDstId) {       \
        LogWarning(LOG_RF, "Traffic collision detect, preempting new RF traffic to existing network traffic!"); \
        resetRF();                                                                      \
        return false;                                                                   \
    }                                                                                   \
                                                                                        \
    if (m_nxdn->m_netState != RS_NET_IDLE) {                                            \
        if (m_nxdn->m_netLC.getSrcId() == _SRC_ID && m_nxdn->m_netLastDstId == _DST_ID) { \
            LogWarning(LOG_RF, "Traffic collision detect, preempting new RF traffic to existing RF traffic (Are we in a voting condition?), rfSrcId = %u, rfDstId = %u, netSrcId = %u, netDstId = %u", srcId, dstId, \
                m_nxdn->m_netLC.getSrcId(), m_nxdn->m_netLastDstId);                    \
            resetRF();                                                                  \
            return false;                                                               \
        }                                                                               \
        else {                                                                          \
            LogWarning(LOG_RF, "Traffic collision detect, preempting existing network traffic to new RF traffic, rfDstId = %u, netDstId = %u", dstId, \
                m_nxdn->m_netLastDstId);                                                \
            resetNet();                                                                 \
        }                                                                               \
    }

// Don't process network frames if the destination ID's don't match and the network TG hang
// timer is running, and don't process network frames if the RF modem isn't in a listening state
#define CHECK_NET_TRAFFIC_COLLISION(_LAYER3, _SRC_ID, _DST_ID)                          \
    if (m_nxdn->m_rfLastDstId != 0U) {                                                  \
        if (m_nxdn->m_rfLastDstId != dstId && (m_nxdn->m_rfTGHang.isRunning() && !m_nxdn->m_rfTGHang.hasExpired())) { \
            resetNet();                                                                 \
            return false;                                                               \
        }                                                                               \
                                                                                        \
        if (m_nxdn->m_rfLastDstId == dstId && (m_nxdn->m_rfTGHang.isRunning() && !m_nxdn->m_rfTGHang.hasExpired())) { \
            m_nxdn->m_rfTGHang.start();                                                 \
        }                                                                               \
    }                                                                                   \
                                                                                        \
    if (m_nxdn->m_rfState != RS_RF_LISTENING) {                                         \
        if (_LAYER3.getSrcId() == srcId && _LAYER3.getDstId() == dstId) { \
            LogWarning(LOG_RF, "Traffic collision detect, preempting new network traffic to existing RF traffic (Are we in a voting condition?), rfSrcId = %u, rfDstId = %u, netSrcId = %u, netDstId = %u", _LAYER3.getSrcId(), _LAYER3.getDstId(), \
                srcId, dstId);                                                          \
            resetNet();                                                                 \
            return false;                                                               \
        }                                                                               \
        else {                                                                          \
            LogWarning(LOG_RF, "Traffic collision detect, preempting new network traffic to existing RF traffic, rfDstId = %u, netDstId = %u", _LAYER3.getDstId(), \
                dstId);                                                                 \
            resetNet();                                                                 \
            return false;                                                               \
        }                                                                               \
    }

// Validate the source RID
#define VALID_SRCID(_SRC_ID, _DST_ID, _GROUP)                                           \
    if (!acl::AccessControl::validateSrcId(_SRC_ID)) {                                  \
        if (m_lastRejectId == 0U || m_lastRejectId != _SRC_ID) {                        \
            LogWarning(LOG_RF, "NXDN, " NXDN_RTCH_MSG_TYPE_DCALL_HDR " denial, RID rejection, srcId = %u", _SRC_ID); \
            ::ActivityLog("NXDN", true, "RF voice rejection from %u to %s%u ", _SRC_ID, _GROUP ? "TG " : "", _DST_ID); \
            m_lastRejectId = _SRC_ID;                                                   \
        }                                                                               \
                                                                                        \
        m_nxdn->m_rfLastDstId = 0U;                                                     \
        m_nxdn->m_rfTGHang.stop();                                                      \
        m_nxdn->m_rfState = RS_RF_REJECTED;                                             \
        return false;                                                                   \
    }

// Validate the destination ID
#define VALID_DSTID(_SRC_ID, _DST_ID, _GROUP)                                           \
    if (!_GROUP) {                                                                      \
        if (!acl::AccessControl::validateSrcId(_DST_ID)) {                              \
            if (m_lastRejectId == 0 || m_lastRejectId != _DST_ID) {                     \
                LogWarning(LOG_RF, "NXDN, " NXDN_RTCH_MSG_TYPE_DCALL_HDR " denial, RID rejection, dstId = %u", _DST_ID); \
                ::ActivityLog("NXDN", true, "RF voice rejection from %u to %s%u ", _SRC_ID, _GROUP ? "TG " : "", _DST_ID); \
                m_lastRejectId = _DST_ID;                                               \
            }                                                                           \
                                                                                        \
            m_nxdn->m_rfLastDstId = 0U;                                                 \
            m_nxdn->m_rfTGHang.stop();                                                  \
            m_nxdn->m_rfState = RS_RF_REJECTED;                                         \
            return false;                                                               \
        }                                                                               \
    }                                                                                   \
    else {                                                                              \
        if (!acl::AccessControl::validateTGId(_DST_ID)) {                               \
            if (m_lastRejectId == 0 || m_lastRejectId != _DST_ID) {                     \
                LogWarning(LOG_RF, "NXDN, " NXDN_RTCH_MSG_TYPE_DCALL_HDR " denial, TGID rejection, dstId = %u", _DST_ID); \
                ::ActivityLog("NXDN", true, "RF voice rejection from %u to %s%u ", _SRC_ID, _GROUP ? "TG " : "", _DST_ID); \
                m_lastRejectId = _DST_ID;                                               \
            }                                                                           \
                                                                                        \
            m_nxdn->m_rfLastDstId = 0U;                                                 \
            m_nxdn->m_rfTGHang.stop();                                                  \
            m_nxdn->m_rfState = RS_RF_REJECTED;                                         \
            return false;                                                               \
        }                                                                               \
    }

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Resets the data states for the RF interface.
/// </summary>
void Data::resetRF()
{
    /* stub */
}

/// <summary>
/// Resets the data states for the network.
/// </summary>
void Data::resetNet()
{
    /* stub */
}

/// <summary>
/// Process a data frame from the RF interface.
/// </summary>
/// <param name="option">Channel options.</param>
/// <param name="data">Buffer containing data frame.</param>
/// <param name="len">Length of data frame.</param>
/// <returns></returns>
bool Data::process(uint8_t option, uint8_t* data, uint32_t len)
{
    assert(data != nullptr);

    channel::UDCH udch;
    bool validUDCH = udch.decode(data + 2U);
    if (m_nxdn->m_rfState == RS_RF_LISTENING && !validUDCH)
        return false;

    if (validUDCH) {
        uint8_t ran = udch.getRAN();
        if (ran != m_nxdn->m_ran && ran != 0U)
            return false;
    }

    // the layer 3 LC data will only be correct if valid is true
    uint8_t buffer[NXDN_RTCH_LC_LENGTH_BYTES];
    udch.getData(buffer);

    lc::RTCH lc;
    lc.setVerbose(m_verbose);
    lc.decode(buffer, NXDN_UDCH_LENGTH_BITS);
    uint16_t dstId = lc.getDstId();
    uint16_t srcId = lc.getSrcId();
    bool group = lc.getGroup();

    if (m_nxdn->m_rfState == RS_RF_LISTENING) {
        uint8_t type = lc.getMessageType();
        if (type != RTCH_MESSAGE_TYPE_DCALL_HDR)
            return false;

        CHECK_TRAFFIC_COLLISION(srcId, dstId);

        // validate source RID
        VALID_SRCID(srcId, dstId, group);

        // validate destination ID
        VALID_DSTID(srcId, dstId, group);

        if (m_verbose) {
            LogMessage(LOG_RF, "NXDN, " NXDN_RTCH_MSG_TYPE_DCALL_HDR ", srcId = %u, dstId = %u, ack = %u, blocksToFollow = %u, padCount = %u, firstFragment = %u, fragmentCount = %u",
                srcId, dstId, lc.getPacketInfo().getDelivery(), lc.getPacketInfo().getBlockCount(), lc.getPacketInfo().getPadCount(), lc.getPacketInfo().getStart(), lc.getPacketInfo().getFragmentCount());
        }

        ::ActivityLog("NXDN", true, "RF data transmission from %u to %s%u", srcId, group ? "TG " : "", dstId);

        m_nxdn->m_rfLC = lc;
        m_nxdn->m_voice->m_rfFrames = 0U;

        m_nxdn->m_rfState = RS_RF_DATA;
    }

    if (m_nxdn->m_rfState != RS_RF_DATA)
        return false;

    Sync::addNXDNSync(data + 2U);

    channel::LICH lich;
    lich.setRFCT(NXDN_LICH_RFCT_RDCH);
    lich.setFCT(NXDN_LICH_USC_UDCH);
    lich.setOption(option);
    lich.setOutbound(!m_nxdn->m_duplex ? false : true);
    lich.encode(data + 2U);

    lich.setOutbound(false);

    uint8_t type = RTCH_MESSAGE_TYPE_DCALL_DATA;
    if (validUDCH) {
        type = lc.getMessageType();
        data[0U] = type == RTCH_MESSAGE_TYPE_TX_REL ? modem::TAG_EOT : modem::TAG_DATA;

        udch.setRAN(m_nxdn->m_ran);
        udch.encode(data + 2U);
    } else {
        data[0U] = modem::TAG_DATA;
        data[1U] = 0x00U;
    }

    NXDNUtils::scrambler(data + 2U);

    writeNetwork(data, NXDN_FRAME_LENGTH_BYTES + 2U);

    if (m_nxdn->m_duplex) {
        m_nxdn->addFrame(data, NXDN_FRAME_LENGTH_BYTES + 2U);
    }

	m_nxdn->m_voice->m_rfFrames++;

    if (data[0U] == modem::TAG_EOT) {
        ::ActivityLog("NXDN", true, "RF ended RF data transmission");

        LogMessage(LOG_RF, "NXDN, " NXDN_RTCH_MSG_TYPE_TX_REL ", total frames: %d",
            m_nxdn->m_voice->m_rfFrames);

        m_nxdn->writeEndRF();
    }

    return true;
}

/// <summary>
/// Process a data frame from the RF interface.
/// </summary>
/// <param name="option">Channel options.</param>
/// <param name="netLC"></param>
/// <param name="data">Buffer containing data frame.</param>
/// <param name="len">Length of data frame.</param>
/// <returns></returns>
bool Data::processNetwork(uint8_t option, lc::RTCH& netLC, uint8_t* data, uint32_t len)
{
    assert(data != nullptr);

    if (m_nxdn->m_netState == RS_NET_IDLE) {
        m_nxdn->m_queue.clear();

        resetRF();
        resetNet();
    }

    channel::UDCH udch;
    bool validUDCH = udch.decode(data + 2U);
    if (m_nxdn->m_netState == RS_NET_IDLE && !validUDCH)
        return false;

    // The layer3 data will only be correct if valid is true
    uint8_t buffer[NXDN_RTCH_LC_LENGTH_BYTES];
    udch.getData(buffer);

    lc::RTCH lc;
    lc.setVerbose(m_verbose);
    lc.decode(buffer, NXDN_UDCH_LENGTH_BITS);
    uint16_t dstId = lc.getDstId();
    uint16_t srcId = lc.getSrcId();
    bool group = lc.getGroup();

    if (m_nxdn->m_netState == RS_NET_IDLE) {
        uint8_t type = lc.getMessageType();
        if (type != RTCH_MESSAGE_TYPE_DCALL_HDR)
            return false;

        CHECK_NET_TRAFFIC_COLLISION(lc, srcId, dstId);

        // validate source RID
        VALID_SRCID(srcId, dstId, group);

        // validate destination ID
        VALID_DSTID(srcId, dstId, group);

        if (m_verbose) {
            LogMessage(LOG_NET, "NXDN, " NXDN_RTCH_MSG_TYPE_DCALL_HDR ", srcId = %u, dstId = %u, ack = %u, blocksToFollow = %u, padCount = %u, firstFragment = %u, fragmentCount = %u",
                srcId, dstId, lc.getPacketInfo().getDelivery(), lc.getPacketInfo().getBlockCount(), lc.getPacketInfo().getPadCount(), lc.getPacketInfo().getStart(), lc.getPacketInfo().getFragmentCount());
        }

        ::ActivityLog("NXDN", false, "network data transmission from %u to %s%u", srcId, group ? "TG " : "", dstId);

        m_nxdn->m_netLC = lc;
        m_nxdn->m_voice->m_netFrames = 0U;

        m_nxdn->m_netState = RS_NET_DATA;
    }

    if (m_nxdn->m_netState != RS_NET_DATA)
        return false;

    Sync::addNXDNSync(data + 2U);

    channel::LICH lich;
    lich.setRFCT(NXDN_LICH_RFCT_RDCH);
    lich.setFCT(NXDN_LICH_USC_UDCH);
    lich.setOption(option);
    lich.setOutbound(true);
    lich.encode(data + 2U);

    uint8_t type = RTCH_MESSAGE_TYPE_DCALL_DATA;
    if (validUDCH) {
        type = lc.getMessageType();
        data[0U] = type == RTCH_MESSAGE_TYPE_TX_REL ? modem::TAG_EOT : modem::TAG_DATA;

        udch.setRAN(m_nxdn->m_ran);
        udch.encode(data + 2U);
    } else {
        data[0U] = modem::TAG_DATA;
        data[1U] = 0x00U;
    }

    NXDNUtils::scrambler(data + 2U);

    if (m_nxdn->m_duplex) {
        m_nxdn->addFrame(data, NXDN_FRAME_LENGTH_BYTES + 2U);
    }

	m_nxdn->m_voice->m_netFrames++;

    if (data[0U] == modem::TAG_EOT) {
        ::ActivityLog("NXDN", true, "network ended RF data transmission");

        LogMessage(LOG_NET, "NXDN, " NXDN_RTCH_MSG_TYPE_TX_REL ", total frames: %d",
            m_nxdn->m_voice->m_netFrames);

        m_nxdn->writeEndNet();
    }

    return true;
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the Data class.
/// </summary>
/// <param name="nxdn">Instance of the Control class.</param>
/// <param name="network">Instance of the BaseNetwork class.</param>
/// <param name="debug">Flag indicating whether NXDN debug is enabled.</param>
/// <param name="verbose">Flag indicating whether NXDN verbose logging is enabled.</param>
Data::Data(Control* nxdn, network::BaseNetwork* network, bool debug, bool verbose) :
    m_nxdn(nxdn),
    m_network(network),
    m_lastRejectId(0U),
    m_verbose(verbose),
    m_debug(debug)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the Data class.
/// </summary>
Data::~Data()
{
    /* stub */
}

/// <summary>
/// Write data processed from RF to the network.
/// </summary>
/// <param name="data"></param>
/// <param name="len"></param>
void Data::writeNetwork(const uint8_t *data, uint32_t len)
{
    assert(data != nullptr);

    if (m_network == nullptr)
        return;

    if (m_nxdn->m_rfTimeout.isRunning() && m_nxdn->m_rfTimeout.hasExpired())
        return;

    m_network->writeNXDN(m_nxdn->m_rfLC, data, len);
}
