// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/P25Utils.h"
#include "Utils.h"

using namespace p25;
using namespace p25::defines;

#include <cassert>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

/* Helper to set the status bits on P25 frame data. */

void P25Utils::setStatusBits(uint8_t* data, uint32_t ssOffset, bool b1, bool b2)
{
    assert(data != nullptr);

    WRITE_BIT(data, ssOffset, b1);
    WRITE_BIT(data, ssOffset + 1U, b2);
}

/* Helper to set the starting status bits on P25 frame data to 1,1 for idle. */

void P25Utils::setStatusBitsStartIdle(uint8_t* data)
{
    assert(data != nullptr);

    // set "1,1" (Start of Inbound Slot/Idle) status bits [TIA-102.BAAA]
    P25Utils::setStatusBits(data, P25_SS0_START, true, true);
}

/* Helper to set all status bits on a P25 frame data to 1,1 for idle. */

void P25Utils::setStatusBitsAllIdle(uint8_t* data, uint32_t length)
{
    assert(data != nullptr);

    // set "1,1" (Idle) status bits [TIA-102.BAAA]
    for (uint32_t ss0Pos = P25_SS0_START; ss0Pos < length; ss0Pos += P25_SS_INCREMENT) {
        uint32_t ss1Pos = ss0Pos + 1U;
        WRITE_BIT(data, ss0Pos, true);              // 1
        WRITE_BIT(data, ss1Pos, true);              // 1
    }
}

/* Helper to add the status bits on P25 frame data. */

void P25Utils::addStatusBits(uint8_t* data, uint32_t length, bool busy, bool unknown)
{
    assert(data != nullptr);

    // set "1,0" (Unknown) status bits [TIA-102.BAAA]
    for (uint32_t ss0Pos = P25_SS0_START; ss0Pos < length; ss0Pos += P25_SS_INCREMENT) {
        uint32_t ss1Pos = ss0Pos + 1U;
        WRITE_BIT(data, ss0Pos, true);              // 1
        WRITE_BIT(data, ss1Pos, false);             // 0
    }

    // interleave the requested status bits (every other)
    for (uint32_t ss0Pos = P25_SS0_START; ss0Pos < length; ss0Pos += (P25_SS_INCREMENT * 2U)) {
        uint32_t ss1Pos = ss0Pos + 1U;
        if (busy) {
            // set "0,1" (Busy) status bits [TIA-102.BAAA]
            WRITE_BIT(data, ss0Pos, false);         // 0
            WRITE_BIT(data, ss1Pos, true);          // 1
        } else {
            if (unknown) {
                // set "1,0" (Unknown) status bits [TIA-102.BAAA]
                WRITE_BIT(data, ss0Pos, true);      // 1
                WRITE_BIT(data, ss1Pos, false);     // 0
            } else {
                // set "1,1" (Start of Inbound Slot/Idle) status bits [TIA-102.BAAA]
                WRITE_BIT(data, ss0Pos, true);      // 1
                WRITE_BIT(data, ss1Pos, true);      // 1
            }
        }
    }
}

/* Helper to add the unknown (1,0) status bits on P25 frame data. */

void P25Utils::addUnknownStatusBits(uint8_t* data, uint32_t length, uint8_t interval)
{
    assert(data != nullptr);

    if (interval == 0U)
        interval = 1U;

    for (uint32_t ss0Pos = P25_SS0_START; ss0Pos < length; ss0Pos += (P25_SS_INCREMENT * interval)) {
        uint32_t ss1Pos = ss0Pos + 1U;
        // set "1,0" (Unknown) status bits [TIA-102.BAAA]
        WRITE_BIT(data, ss0Pos, true);              // 1
        WRITE_BIT(data, ss1Pos, false);             // 0
    }
}

/* Helper to add the idle (1,1) status bits on P25 frame data. */

void P25Utils::addIdleStatusBits(uint8_t* data, uint32_t length, uint8_t interval)
{
    assert(data != nullptr);

    if (interval == 0U)
        interval = 1U;

    for (uint32_t ss0Pos = P25_SS0_START; ss0Pos < length; ss0Pos += (P25_SS_INCREMENT * interval)) {
        uint32_t ss1Pos = ss0Pos + 1U;
        // set "1,1" (Start of Inbound Slot/Idle) status bits [TIA-102.BAAA]
        WRITE_BIT(data, ss0Pos, true);              // 1
        WRITE_BIT(data, ss1Pos, true);              // 1
    }
}

/* Decode bit interleaving. */

uint32_t P25Utils::decode(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t stop)
{
    assert(in != nullptr);
    assert(out != nullptr);

    // Move the SSx positions to the range needed
    uint32_t ss0Pos = P25_SS0_START;
    uint32_t ss1Pos = P25_SS1_START;

    while (ss0Pos < start) {
        ss0Pos += P25_SS_INCREMENT;
        ss1Pos += P25_SS_INCREMENT;
    }

    uint32_t n = 0U;
    for (uint32_t i = start; i < stop; i++) {
        if (i == ss0Pos) {
            ss0Pos += P25_SS_INCREMENT;
        }
        else if (i == ss1Pos) {
            ss1Pos += P25_SS_INCREMENT;
        }
        else {
            bool b = READ_BIT(in, i);
            WRITE_BIT(out, n, b);
            n++;
        }
    }

    return n;
}

/* Encode bit interleaving. */

uint32_t P25Utils::encode(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t stop)
{
    assert(in != nullptr);
    assert(out != nullptr);

    // Move the SSx positions to the range needed
    uint32_t ss0Pos = P25_SS0_START;
    uint32_t ss1Pos = P25_SS1_START;

    while (ss0Pos < start) {
        ss0Pos += P25_SS_INCREMENT;
        ss1Pos += P25_SS_INCREMENT;
    }

    uint32_t n = 0U;
    for (uint32_t i = start; i < stop; i++) {
        if (i == ss0Pos) {
            ss0Pos += P25_SS_INCREMENT;
        }
        else if (i == ss1Pos) {
            ss1Pos += P25_SS_INCREMENT;
        }
        else {
            bool b = READ_BIT(in, n);
            WRITE_BIT(out, i, b);
            n++;
        }
    }

    return n;
}

/* Encode bit interleaving for a given length. */

uint32_t P25Utils::encodeByLength(const uint8_t* in, uint8_t* out, uint32_t length)
{
    assert(in != nullptr);
    assert(out != nullptr);

    // Move the SSx positions to the range needed
    uint32_t ss0Pos = P25_SS0_START;
    uint32_t ss1Pos = P25_SS1_START;

    uint32_t n = 0U;
    uint32_t pos = 0U;
    while (n < length) {
        if (pos == ss0Pos) {
            ss0Pos += P25_SS_INCREMENT;
        }
        else if (pos == ss1Pos) {
            ss1Pos += P25_SS_INCREMENT;
        }
        else {
            bool b = READ_BIT(in, n);
            WRITE_BIT(out, pos, b);
            n++;
        }
        pos++;

    }

    return pos;
}

/* Compare two datasets for the given length. */

uint32_t P25Utils::compare(const uint8_t* data1, const uint8_t* data2, uint32_t length)
{
    assert(data1 != nullptr);
    assert(data2 != nullptr);

    uint32_t errs = 0U;
    for (uint32_t i = 0U; i < length; i++) {
        uint8_t v = data1[i] ^ data2[i];
        while (v != 0U) {
            v &= v - 1U;
            errs++;
        }
    }

    return errs;
}

/* Helper to convert a denial reason code to a string. */

std::string P25Utils::denyRsnToString(uint8_t reason)
{
    switch (reason) {
    case ReasonCode::DENY_REQ_UNIT_NOT_VALID:
        return std::string("DENY_REQ_UNIT_NOT_VALID (Requesting Unit Not Valid)");
    case ReasonCode::DENY_REQ_UNIT_NOT_AUTH:
        return std::string("DENY_REQ_UNIT_NOT_AUTH (Requesting Unit Not Authenticated)");

    case ReasonCode::DENY_TGT_UNIT_NOT_VALID:
        return std::string("DENY_TGT_UNIT_NOT_VALID (Target Unit Not Valid)");
    case ReasonCode::DENY_TGT_UNIT_NOT_AUTH:
        return std::string("DENY_TGT_UNIT_NOT_AUTH (Target Unit Not Authenticated)");
    case ReasonCode::DENY_SU_FAILED_AUTH:
        return std::string("DENY_SU_FAILED_AUTH (Target Unit Failed Authentication)");
    case ReasonCode::DENY_TGT_UNIT_REFUSED:
        return std::string("DENY_TGT_UNIT_REFUSED (Target Unit Refused)");

    case ReasonCode::DENY_TGT_GROUP_NOT_VALID:
        return std::string("DENY_TGT_GROUP_NOT_VALID (Target Group Not Valid)");
    case ReasonCode::DENY_TGT_GROUP_NOT_AUTH:
        return std::string("DENY_TGT_GROUP_NOT_AUTH (Target Group Not Authenticated)");

    case ReasonCode::DENY_NO_NET_RSRC_AVAIL:
        return std::string("DENY_NO_NET_RSRC_AVAIL (Requested Network Resources Not Available)");
    case ReasonCode::DENY_NO_RF_RSRC_AVAIL:
        return std::string("DENY_NO_RF_RSRC_AVAIL (Requested RF Resources Not Available)");
    case ReasonCode::DENY_SVC_IN_USE:
        return std::string("DENY_SVC_IN_USE (Service In Use)");
    
    case ReasonCode::DENY_SITE_ACCESS_DENIAL:
        return std::string("DENY_SITE_ACCESS_DENIAL (Site Access Denial)");

    case ReasonCode::DENY_PTT_COLLIDE:
        return std::string("DENY_PTT_COLLIDE (Push-to-Talk Collision)");
    case ReasonCode::DENY_PTT_BONK:
        return std::string("DENY_PTT_BONK (Push-to-Talk Denial/Bonk)");

    case ReasonCode::DENY_SYS_UNSUPPORTED_SVC:
        return std::string("DENY_SYS_UNSUPPORTED_SVC (Service Unsupported)");

    default:
        return std::string();
    }
}

/* Helper to convert a queue reason code to a string. */

std::string P25Utils::queueRsnToString(uint8_t reason)
{
    switch (reason) {
    case ReasonCode::QUE_REQ_ACTIVE_SERVICE:
        return std::string("QUE_REQ_ACTIVE_SERVICE (Requested Service Active)");
    case ReasonCode::QUE_TGT_ACTIVE_SERVICE:
        return std::string("QUE_TGT_ACTIVE_SERVICE (Target Service Active)");

    case ReasonCode::QUE_TGT_UNIT_QUEUED:
        return std::string("QUE_TGT_UNIT_QUEUED (Target Unit Queued)");

    case ReasonCode::QUE_CHN_RESOURCE_NOT_AVAIL:
        return std::string("QUE_CHN_RESOURCE_NOT_AVAIL (Channel Resource Not Available)");

    default:
        return std::string();
    }
}
