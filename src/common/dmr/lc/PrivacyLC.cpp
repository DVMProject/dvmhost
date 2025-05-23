// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * @package DVM / Common Library
 * @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
 *
 *  Copyright (C) 2021,2024,2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "dmr/DMRDefines.h"
#include "dmr/lc/PrivacyLC.h"
#include "Utils.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::lc;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the PrivacyLC class. */

PrivacyLC::PrivacyLC(const uint8_t* data) :
    m_FID(FID_ETSI),
    m_dstId(0U),
    m_group(false),
    m_algId(0U),
    m_kId(0U),
    m_mi(nullptr)
{
    assert(data != nullptr);

    m_mi = new uint8_t[MI_LENGTH_BYTES];

    m_group = (data[0U] & 0x20U) == 0x20U;
    m_algId = data[0U] & 7;                                                     // Algorithm ID

    m_FID = data[1U];
    m_kId = data[2U];

    m_mi[0U] = data[3U];
    m_mi[1U] = data[4U];
    m_mi[2U] = data[5U];
    m_mi[3U] = data[6U];

    m_dstId = data[7U] << 16 | data[8U] << 8 | data[9U];                        // Destination Address
}

/* Initializes a new instance of the PrivacyLC class. */

PrivacyLC::PrivacyLC(const bool* bits) :
    m_FID(FID_ETSI),
    m_dstId(0U),
    m_group(false),
    m_algId(0U),
    m_kId(0U),
    m_mi(nullptr)
{
    assert(bits != nullptr);

    m_mi = new uint8_t[MI_LENGTH_BYTES];

    uint8_t temp1, temp2, temp3;
    Utils::bitsToByteBE(bits + 0U, temp1);

    m_group = (temp1 & 0x20U) == 0x20U;
    m_algId = temp1 & 7;                                                        // Algorithm ID

    Utils::bitsToByteBE(bits + 8U, temp2);
    m_FID = temp2;

    Utils::bitsToByteBE(bits + 16U, temp3);
    m_kId = temp3;

    uint8_t mi1, mi2, mi3, mi4;
    Utils::bitsToByteBE(bits + 24U, mi1);
    Utils::bitsToByteBE(bits + 32U, mi2);
    Utils::bitsToByteBE(bits + 40U, mi3);
    Utils::bitsToByteBE(bits + 48U, mi4);

    m_mi[0U] = mi1;
    m_mi[1U] = mi2;
    m_mi[2U] = mi3;
    m_mi[3U] = mi4;

    uint8_t d1, d2, d3;
    Utils::bitsToByteBE(bits + 56U, d1);
    Utils::bitsToByteBE(bits + 64U, d2);
    Utils::bitsToByteBE(bits + 72U, d3);

    m_dstId = d1 << 16 | d2 << 8 | d3;                                          // Destination Address
}

/* Initializes a new instance of the PrivacyLC class. */

PrivacyLC::PrivacyLC() :
    m_FID(FID_ETSI),
    m_dstId(0U),
    m_group(false),
    m_algId(0U),
    m_kId(0U),
    m_mi(nullptr)
{
    m_mi = new uint8_t[MI_LENGTH_BYTES];
}

/* Finalizes a instance of the PrivacyLC class. */

PrivacyLC::~PrivacyLC()
{
    delete m_mi;
}

/* Gets LC data as bytes. */

void PrivacyLC::getData(uint8_t* data) const
{
    assert(data != nullptr);

    data[0U] = (m_group ? 0x20U : 0x00U) +
        (m_algId & 0x07U);                                                      // Algorithm ID

    data[1U] = m_FID;
    data[2U] = m_kId;

    data[3U] = m_mi[0U];
    data[4U] = m_mi[1U];
    data[5U] = m_mi[2U];
    data[6U] = m_mi[3U];

    data[7U] = m_dstId >> 16;                                                   // Destination Address
    data[8U] = m_dstId >> 8;                                                    // ..
    data[9U] = m_dstId >> 0;                                                    // ..
}

/* Gets LC data as bits. */

void PrivacyLC::getData(bool* bits) const
{
    assert(bits != nullptr);

    uint8_t bytes[10U];
    getData(bytes);

    Utils::byteToBitsBE(bytes[0U], bits + 0U);
    Utils::byteToBitsBE(bytes[1U], bits + 8U);
    Utils::byteToBitsBE(bytes[2U], bits + 16U);
    Utils::byteToBitsBE(bytes[3U], bits + 24U);
    Utils::byteToBitsBE(bytes[4U], bits + 32U);
    Utils::byteToBitsBE(bytes[5U], bits + 40U);
    Utils::byteToBitsBE(bytes[6U], bits + 48U);
    Utils::byteToBitsBE(bytes[7U], bits + 56U);
    Utils::byteToBitsBE(bytes[8U], bits + 64U);
    Utils::byteToBitsBE(bytes[9U], bits + 72U);
}

/*
** Encryption data 
*/

/* Sets the encryption message indicator. */

void PrivacyLC::setMI(const uint8_t* mi)
{
    assert(mi != nullptr);

    ::memcpy(m_mi, mi, MI_LENGTH_BYTES);
}

/* Gets the encryption message indicator. */

void PrivacyLC::getMI(uint8_t* mi) const
{
    assert(mi != nullptr);

    ::memcpy(mi, m_mi, MI_LENGTH_BYTES);
}
