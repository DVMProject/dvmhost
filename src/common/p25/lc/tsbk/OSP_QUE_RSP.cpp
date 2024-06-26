// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/lc/tsbk/OSP_QUE_RSP.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the OSP_QUE_RSP class. */

OSP_QUE_RSP::OSP_QUE_RSP() : TSBK()
{
    m_lco = TSBKO::OSP_QUE_RSP;
}

/* Decode a trunking signalling block. */

bool OSP_QUE_RSP::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES + 1U];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    bool ret = TSBK::decode(data, tsbk, rawTSBK);
    if (!ret)
        return false;

    ulong64_t tsbkValue = TSBK::toValue(tsbk);

    m_aivFlag = (((tsbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;                     // Additional Info. Flag
    m_service = (uint8_t)((tsbkValue >> 56) & 0x3FU);                               // Service Type
    m_response = (uint8_t)((tsbkValue >> 48) & 0xFFU);                              // Reason
    m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFFFU);                            // Target Radio Address
    m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/* Encode a trunking signalling block. */

void OSP_QUE_RSP::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    if (m_response == 0U) {
        LogError(LOG_P25, "OSP_QUE_RSP::encode(), invalid values for TSBKO::OSP_QUE_RSP, reason = %u", m_response);
        return; // blatantly ignore creating this TSBK
    }

    tsbkValue = (m_service & 0x3FU);                                                // Service Type
    tsbkValue |= (m_aivFlag) ? 0x80U : 0x00U;                                       // Additional Info. Valid Flag
    tsbkValue = (tsbkValue << 8) + m_response;                                      // Deny/Queue Reason
    if (m_aivFlag) {
        if (m_group) {
            // group deny/queue
            tsbkValue = (tsbkValue << 8) + 0U;                                      // Call Options
            tsbkValue = (tsbkValue << 16) + m_dstId;                                // Talkgroup Address
        }
        else {
            // private/individual deny/queue
            tsbkValue = (tsbkValue << 24) + m_dstId;                                // Target Radio Address
        }
    } else {
        tsbkValue = (tsbkValue << 24) + 0U;
    }
    tsbkValue = (tsbkValue << 24) + m_srcId;                                        // Source Radio Address

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/* Returns a string that represents the current TSBK. */

std::string OSP_QUE_RSP::toString(bool isp)
{
    return std::string("TSBKO, OSP_QUE_RSP (Queued Response)");
}
