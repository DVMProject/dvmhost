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
#include "nxdn/channel/CAC.h"
#include "nxdn/packet/Trunk.h"
#include "nxdn/acl/AccessControl.h"
#include "nxdn/Sync.h"
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
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Resets the data states for the RF interface.
/// </summary>
void Trunk::resetRF()
{
    lc::RCCH lc = lc::RCCH(m_nxdn->m_siteData, m_nxdn->m_idenEntry, m_dumpRCCH);
    m_rfLC = lc;
}

/// <summary>
/// Resets the data states for the network.
/// </summary>
void Trunk::resetNet()
{
    lc::RCCH lc = lc::RCCH(m_nxdn->m_siteData, m_nxdn->m_idenEntry, m_dumpRCCH);
    m_netLC = lc;
}

/// <summary>
/// Process a data frame from the RF interface.
/// </summary>
/// <param name="fct">Functional channel type.</param>
/// <param name="option">Channel options.</param>
/// <param name="data">Buffer containing data frame.</param>
/// <param name="len">Length of data frame.</param>
/// <returns></returns>
bool Trunk::process(uint8_t fct, uint8_t option, uint8_t* data, uint32_t len)
{
    assert(data != NULL);

    channel::CAC cac;
    bool validCAC = cac.decode(data + 2U);
    if (m_nxdn->m_rfState == RS_RF_LISTENING && !validCAC)
        return false;

    if (validCAC) {
        uint8_t ran = cac.getRAN();
        if (ran != m_nxdn->m_ran && ran != 0U)
            return false;
    }

    // The layer3 data will only be correct if valid is true
    uint8_t buffer[NXDN_CAC_LENGTH_BYTES];
    cac.getData(buffer);

    m_rfLC.decode(buffer, NXDN_CAC_SHORT_IN_CRC_BITS);

    // TODO TODO -- process incoming data

    return true;
}

/// <summary>
/// Process a data frame from the RF interface.
/// </summary>
/// <param name="fct">Functional channel type.</param>
/// <param name="option">Channel options.</param>
/// <param name="netLC"></param>
/// <param name="data">Buffer containing data frame.</param>
/// <param name="len">Length of data frame.</param>
/// <returns></returns>
bool Trunk::processNetwork(uint8_t fct, uint8_t option, lc::RTCH& netLC, uint8_t* data, uint32_t len)
{
    assert(data != NULL);

    if (m_nxdn->m_netState == RS_NET_IDLE) {
        m_nxdn->m_queue.clear();
        
        resetRF();
        resetNet();
    }

    return true;
}

/// <summary>
/// Updates the processor by the passed number of milliseconds.
/// </summary>
/// <param name="ms"></param>
void Trunk::clock(uint32_t ms)
{
    return;
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the Trunk class.
/// </summary>
/// <param name="nxdn">Instance of the Control class.</param>
/// <param name="network">Instance of the BaseNetwork class.</param>
/// <param name="dumpRCCHData">Flag indicating whether RCCH data is dumped to the log.</param>
/// <param name="debug">Flag indicating whether NXDN debug is enabled.</param>
/// <param name="verbose">Flag indicating whether NXDN verbose logging is enabled.</param>
Trunk::Trunk(Control* nxdn, network::BaseNetwork* network, bool dumpRCCHData, bool debug, bool verbose) :
    m_nxdn(nxdn),
    m_network(network),
    m_rfLC(SiteData(), lookups::IdenTable()),
    m_netLC(SiteData(), lookups::IdenTable()),
    m_lastRejectId(0U),
    m_dumpRCCH(dumpRCCHData),
    m_verbose(verbose),
    m_debug(debug)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the Trunk class.
/// </summary>
Trunk::~Trunk()
{
    /* stub */
}

/// <summary>
/// Write data processed from RF to the network.
/// </summary>
/// <param name="data"></param>
/// <param name="len"></param>
void Trunk::writeNetwork(const uint8_t *data, uint32_t len)
{
    assert(data != NULL);

    if (m_network == NULL)
        return;

    if (m_nxdn->m_rfTimeout.isRunning() && m_nxdn->m_rfTimeout.hasExpired())
        return;

    m_network->writeNXDN(m_nxdn->m_rfLC, data, len);
}

/// <summary>
/// Helper to write control channel packet data.
/// </summary>
/// <param name="frameCnt"></param>
/// <param name="n"></param>
/// <param name="adjSS"></param>
void Trunk::writeRF_ControlData(uint8_t frameCnt, uint8_t n, bool adjSS)
{
    uint8_t i = 0U, seqCnt = 0U;

    if (!m_nxdn->m_control)
        return;

    // don't add any frames if the queue is full
    uint8_t len = NXDN_FRAME_LENGTH_BYTES + 2U;
    uint32_t space = m_nxdn->m_queue.freeSpace();
    if (space < (len + 1U)) {
        return;
    }

    do
    {
        if (m_debug) {
            LogDebug(LOG_DMR, "writeRF_ControlData, frameCnt = %u, seq = %u", frameCnt, n);
        }

        switch (n)
        {
        case 6:
            writeRF_CC_Site_Info();
            break;
        case 0:
        default:
            writeRF_CC_Service_Info();
            break;
        }

        if (seqCnt > 0U)
            n++;
        i++;
    } while (i <= seqCnt);
}

/// <summary>
/// Helper to write a CC SITE_INFO broadcast packet on the RF interface.
/// </summary>
void Trunk::writeRF_CC_Site_Info()
{
    if (m_debug) {
        LogMessage(LOG_RF, "NXDN, RCCH_MESSAGE_TYPE_SITE_INFO (Site Information)");
    }

    uint8_t data[NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, NXDN_FRAME_LENGTH_BYTES);

    Sync::addNXDNSync(data + 2U);

    channel::LICH lich;
    lich.setRFCT(NXDN_LICH_RFCT_RCCH);
    lich.setFCT(NXDN_LICH_CAC_OUTBOUND);
    lich.setOption(NXDN_LICH_DATA_NORMAL);
    lich.setDirection(NXDN_LICH_DIRECTION_OUTBOUND);
    lich.encode(data + 2U);

    uint8_t buffer[NXDN_RCCH_LC_LENGTH_BYTES];
    ::memset(buffer, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES);

    m_rfLC.setMessageType(RCCH_MESSAGE_TYPE_SITE_INFO);
    m_rfLC.encode(buffer, NXDN_CAC_OUT_CRC_BITS);

    channel::CAC cac;
    cac.setRAN(m_nxdn->m_ran);
    cac.setData(buffer);
    cac.encode(data + 2U);

    data[0U] = modem::TAG_DATA;
    data[1U] = 0x00U;

    m_nxdn->scrambler(data + 2U);

    if (m_nxdn->m_duplex) {
        m_nxdn->addFrame(data, NXDN_FRAME_LENGTH_BYTES + 2U);
    }
}

/// <summary>
/// Helper to write a CC SRV_INFO broadcast packet on the RF interface.
/// </summary>
void Trunk::writeRF_CC_Service_Info()
{
    if (m_debug) {
        LogMessage(LOG_RF, "NXDN, MESSAGE_TYPE_SRV_INFO (Service Information)");
    }

    uint8_t data[NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, NXDN_FRAME_LENGTH_BYTES);

    Sync::addNXDNSync(data + 2U);

    channel::LICH lich;
    lich.setRFCT(NXDN_LICH_RFCT_RCCH);
    lich.setFCT(NXDN_LICH_CAC_OUTBOUND);
    lich.setOption(NXDN_LICH_DATA_NORMAL);
    lich.setDirection(NXDN_LICH_DIRECTION_OUTBOUND);
    lich.encode(data + 2U);

    uint8_t buffer[NXDN_RCCH_LC_LENGTH_BYTES];
    ::memset(buffer, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES);

    m_rfLC.setMessageType(MESSAGE_TYPE_SRV_INFO);
    m_rfLC.encode(buffer, NXDN_CAC_OUT_CRC_BITS);

    channel::CAC cac;
    cac.setRAN(m_nxdn->m_ran);
    cac.setData(buffer);
    cac.encode(data + 2U);

    data[0U] = modem::TAG_DATA;
    data[1U] = 0x00U;

    m_nxdn->scrambler(data + 2U);

    if (m_nxdn->m_duplex) {
        m_nxdn->addFrame(data, NXDN_FRAME_LENGTH_BYTES + 2U);
    }
}
