// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *   Copyright (C) 2022,204 Bryan Biedenkapp, N2PLL
 *
 */
#include "nxdn/NXDNDefines.h"
#include "nxdn/lc/RCCH.h"
#include "Utils.h"

using namespace nxdn;
using namespace nxdn::defines;
using namespace nxdn::lc;

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

bool RCCH::m_verbose = false;

uint8_t* RCCH::m_siteCallsign = nullptr;
SiteData RCCH::m_siteData = SiteData();

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the RCCH class. */
RCCH::RCCH() :
    m_messageType(MessageType::IDLE),
    m_srcId(0U),
    m_dstId(0U),
    m_locId(0U),
    m_regOption(0U),
    m_version(0U),
    m_causeRsp(CauseResponse::MM_REG_ACCEPTED),
    m_grpVchNo(0U),
    m_callType(CallType::UNSPECIFIED),
    m_emergency(false),
    m_encrypted(false),
    m_priority(false),
    m_group(true),
    m_duplex(false),
    m_transmissionMode(TransmissionMode::MODE_4800),
    m_siteIdenEntry()
{
    if (m_siteCallsign == nullptr) {
        m_siteCallsign = new uint8_t[CALLSIGN_LENGTH_BYTES];
        ::memset(m_siteCallsign, 0x00U, CALLSIGN_LENGTH_BYTES);
    }
}

/* Initializes a copy instance of the RCCH class. */
RCCH::RCCH(const RCCH& data) : RCCH()
{
    copy(data);
}

/* Finalizes a instance of RCCH class. */
RCCH::~RCCH()
{
    /* stub */
}

/* Returns a string that represents the current RCCH. */
std::string RCCH::toString(bool isp)
{
    return std::string("MESSAGE_TYPE_UNKWN (Unknown RCCH)");
}

/* Sets the callsign. */
void RCCH::setCallsign(std::string callsign)
{
    if (m_siteCallsign == nullptr) {
        m_siteCallsign = new uint8_t[CALLSIGN_LENGTH_BYTES];
        ::memset(m_siteCallsign, 0x00U, CALLSIGN_LENGTH_BYTES);
    }

    uint32_t idLength = callsign.length();
    if (idLength > 0) {
        ::memset(m_siteCallsign, 0x20U, CALLSIGN_LENGTH_BYTES);

        if (idLength > CALLSIGN_LENGTH_BYTES)
            idLength = CALLSIGN_LENGTH_BYTES;
        for (uint32_t i = 0; i < idLength; i++)
            m_siteCallsign[i] = callsign[i];
    }
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to decode a RCCH link control message. */
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

/* Internal helper to encode a RCCH link control message. */
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

/* Internal helper to copy the the class. */
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
