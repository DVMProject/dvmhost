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
#include "nxdn/NXDNDefines.h"
#include "nxdn/lc/RCCH.h"
#include "Log.h"
#include "Utils.h"

using namespace nxdn;
using namespace nxdn::lc;

#include <cstdio>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

bool RCCH::m_verbose = false;

uint8_t *RCCH::m_siteCallsign = nullptr;
SiteData RCCH::m_siteData = SiteData();

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a copy instance of the RCCH class.
/// </summary>
/// <param name="data"></param>
RCCH::RCCH(const RCCH& data) : RCCH()
{
    copy(data);
}

/// <summary>
/// Initializes a new instance of the RCCH class.
/// </summary>
RCCH::RCCH() :
    m_messageType(MESSAGE_TYPE_IDLE),
    m_srcId(0U),
    m_dstId(0U),
    m_locId(0U),
    m_regOption(0U),
    m_version(0U),
    m_causeRsp(NXDN_CAUSE_MM_REG_ACCEPTED),
    m_grpVchNo(0U),
    m_callType(CALL_TYPE_UNSPECIFIED),
    m_emergency(false),
    m_encrypted(false),
    m_priority(false),
    m_group(true),
    m_duplex(false),
    m_transmissionMode(TRANSMISSION_MODE_4800),
    m_siteIdenEntry()
{
    if (m_siteCallsign == nullptr) {
        m_siteCallsign = new uint8_t[NXDN_CALLSIGN_LENGTH_BYTES];
        ::memset(m_siteCallsign, 0x00U, NXDN_CALLSIGN_LENGTH_BYTES);
    }
}

/// <summary>
/// Finalizes a instance of RCCH class.
/// </summary>
RCCH::~RCCH()
{
    /* stub */
}

/// <summary>
/// Sets the callsign.
/// </summary>
/// <param name="callsign">Callsign.</param>
void RCCH::setCallsign(std::string callsign)
{
    if (m_siteCallsign == nullptr) {
        m_siteCallsign = new uint8_t[NXDN_CALLSIGN_LENGTH_BYTES];
        ::memset(m_siteCallsign, 0x00U, NXDN_CALLSIGN_LENGTH_BYTES);
    }

    uint32_t idLength = callsign.length();
    if (idLength > 0) {
        ::memset(m_siteCallsign, 0x20U, NXDN_CALLSIGN_LENGTH_BYTES);

        if (idLength > NXDN_CALLSIGN_LENGTH_BYTES)
            idLength = NXDN_CALLSIGN_LENGTH_BYTES;
        for (uint32_t i = 0; i < idLength; i++)
            m_siteCallsign[i] = callsign[i];
    }
}

/// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to decode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rcch"></param>
/// <param name="length"></param>
/// <param name="offset"></param>
/// <returns>True, if RCCH was decoded, otherwise false.</returns>
void RCCH::decode(const uint8_t* data, uint8_t* rcch, uint32_t length, uint32_t offset)
{
    assert(data != nullptr);
    assert(rcch != nullptr);

    for (uint32_t i = 0U; i < length; i++, offset++) {
        bool b = READ_BIT(data, offset);
        WRITE_BIT(rcch, i, b);
    }

    if (m_verbose) {
        Utils::dump(2U, "Decoded RCCH Data", rcch, NXDN_RCCH_LC_LENGTH_BYTES);
    }

    m_messageType = data[0U] & 0x3FU;                                               // Message Type
}

/// <summary>
/// Internal helper to encode a RCCH.
/// </summary>
/// <param name="data"></param>
/// <param name="rcch"></param>
/// <param name="length"></param>
/// <param name="offset"></param>
void RCCH::encode(uint8_t* data, const uint8_t* rcch, uint32_t length, uint32_t offset)
{
    assert(data != nullptr);
    assert(rcch != nullptr);

    for (uint32_t i = 0U; i < length; i++, offset++) {
        bool b = READ_BIT(rcch, i);
        WRITE_BIT(data, offset, b);
    }

    if (data[0U] == 0x00U) {
        data[0U] = m_messageType & 0x3FU;                                           // Message Type
    }

    if (m_verbose) {
        Utils::dump(2U, "Encoded RCCH Data", data, NXDN_RCCH_LC_LENGTH_BYTES);
    }
}

// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void RCCH::copy(const RCCH& data)
{
    m_messageType = data.m_messageType;

    m_srcId = data.m_srcId;
    m_dstId = data.m_dstId;

    m_locId = data.m_locId;
    m_regOption = data.m_regOption;

    m_version = data.m_version;

    m_causeRsp = data.m_causeRsp;

    m_grpVchNo = data.m_grpVchNo;

    m_callType = data.m_callType;

    m_emergency = data.m_emergency;
    m_encrypted = data.m_encrypted;
    m_priority = data.m_priority;
    m_group = data.m_group;
    m_duplex = data.m_duplex;

    m_transmissionMode = data.m_transmissionMode;

    m_siteIdenEntry = data.m_siteIdenEntry;
}
