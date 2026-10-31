// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Charles Bricker, K3NOP
 *
 */
#include "Defines.h"
#include "p25/lc/tsbk/IOSP_TELE_INT_ANS.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the IOSP_TELE_INT_ANS class. */

IOSP_TELE_INT_ANS::IOSP_TELE_INT_ANS() : TSBK(),
    m_digits()
{
    m_lco = TSBKO::IOSP_TELE_INT_ANS;
}

/* Decode a trunking signalling block. */

bool IOSP_TELE_INT_ANS::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES + 1U];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    bool ret = TSBK::decode(data, tsbk, rawTSBK);
    if (!ret)
        return false;

    ulong64_t tsbkValue = TSBK::toValue(tsbk);

    // the ISP and OSP share an opcode but not a layout, so both are decoded; the trailing
    // subscriber unit address is the source (ISP) or target (OSP) and is stored as both
    m_emergency = (((tsbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;                   // Emergency Flag (ISP)
    m_encrypted = (((tsbkValue >> 56) & 0xFFU) & 0x40U) == 0x40U;                   // Encryption Flag (ISP)
    m_priority = (((tsbkValue >> 56) & 0xFFU) & 0x07U);                             // Priority (ISP)
    m_response = (uint8_t)((tsbkValue >> 48) & 0xFFU);                              // Answer Response (ISP)

    // a $F digit escapes the next digit ($0 - $3 = A - D, $F = null filler)
    m_digits = "";
    for (uint32_t i = 0U; i < MAX_DIGITS; i++) {
        uint8_t digit = (uint8_t)((tsbkValue >> (60U - (i * 4U))) & 0x0FU);         // Digits 1 - 10 (OSP)
        if (digit <= 0x09U) {
            m_digits += (char)('0' + digit);
        }
        else if (digit == 0x0AU) {
            m_digits += '*';
        }
        else if (digit == 0x0BU) {
            m_digits += '#';
        }
        else if (digit == 0x0FU) {
            if (i + 1U >= MAX_DIGITS)
                break;

            uint8_t escaped = (uint8_t)((tsbkValue >> (60U - ((i + 1U) * 4U))) & 0x0FU);
            if (escaped > 0x03U)
                break;

            m_digits += (char)('A' + escaped);
            i++;
        }
    }

    m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                    // Source (ISP) / Target (OSP) Radio Address
    m_dstId = m_srcId;

    return true;
}

/* Encode a trunking signalling block. */

void IOSP_TELE_INT_ANS::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    uint8_t digits[MAX_DIGITS];
    ::memset(digits, 0x0FU, MAX_DIGITS);                                            // Null Filler

    uint32_t count = 0U;
    for (char c : m_digits) {
        if (c >= '0' && c <= '9' && count < MAX_DIGITS) {
            digits[count++] = (uint8_t)(c - '0');
        }
        else if (c == '*' && count < MAX_DIGITS) {
            digits[count++] = 0x0AU;
        }
        else if (c == '#' && count < MAX_DIGITS) {
            digits[count++] = 0x0BU;
        }
        else if (c >= 'A' && c <= 'D' && count + 1U < MAX_DIGITS) {
            digits[count++] = 0x0FU;
            digits[count++] = (uint8_t)(c - 'A');
        }
    }

    ulong64_t tsbkValue = 0U;
    for (uint32_t i = 0U; i < MAX_DIGITS; i++)
        tsbkValue = (tsbkValue << 4) + digits[i];                                   // Digits 1 - 10
    tsbkValue = (tsbkValue << 24) + m_dstId;                                        // Target Radio Address

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/* Returns a string that represents the current TSBK. */

std::string IOSP_TELE_INT_ANS::toString(bool isp)
{
    return (isp) ? std::string("TSBKO, IOSP_TELE_INT_ANS (Telephone Interconnect Answer Response)") :
        std::string("TSBKO, IOSP_TELE_INT_ANS (Telephone Interconnect Answer Request)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void IOSP_TELE_INT_ANS::copy(const IOSP_TELE_INT_ANS& data)
{
    TSBK::copy(data);

    m_digits = data.m_digits;
}
