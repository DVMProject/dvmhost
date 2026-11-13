// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */

 #include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/kmm/KMMWarmStartCommand.h"

using namespace p25::defines;
using namespace p25::kmm;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the KMMWarmStartCommand class. */

KMMWarmStartCommand::KMMWarmStartCommand() : KMMFrame(),
    m_decryptInfoFmt(KMM_DECRYPT_INSTRUCT_NONE),
    m_kekAlgId(ALGO_UNENCRYPT),
    m_kekKId(0U),
    m_keyLength(0U),
    m_tekAlgId(ALGO_UNENCRYPT),
    m_key(),
    m_mi()
{
    m_messageId = KMM_MessageType::WARM_START_CMD;
    m_respKind = KMM_ResponseKind::IMMEDIATE;
    ::memset(m_mi, 0x00U, sizeof(m_mi));
}

/* Gets the byte length of this KMMWarmStartCommand. */

uint32_t KMMWarmStartCommand::length() const
{
    return KMMFrame::length() + 11U + (m_decryptInfoFmt == KMM_DECRYPT_INSTRUCT_MI ? P25DEF::MI_LENGTH_BYTES : 0U) +
        m_keyLength;
}

/* Decodes a KMM Warm-Start Command from the given data buffer. */

bool KMMWarmStartCommand::decode(const uint8_t* data)
{

    assert(data != nullptr);

    KMMFrame::decodeHeader(data);

    uint32_t offset = 10U + m_bodyOffset;
    m_decryptInfoFmt = data[offset++];                      // Decryption information format
    m_kekAlgId = data[offset++];                            // KEK algorithm identifier
    m_kekKId = GET_UINT16(data, offset); offset += 2U;      // KEK key identifier

    if (m_decryptInfoFmt == KMM_DECRYPT_INSTRUCT_MI) {
        ::memcpy(m_mi, data + offset, sizeof(m_mi));
        offset += sizeof(m_mi);
    }

    m_keyLength = data[offset++];
    m_tekAlgId = data[offset++];
    m_key.keyFormat(data[offset++] & 0xE0U);

    uint16_t sln = GET_UINT16(data, offset); offset += 2U;
    m_key.sln(sln);

    uint16_t kid = GET_UINT16(data, offset); offset += 2U;
    m_key.kId(kid);

    m_key.setKey(data + offset, m_keyLength);

    return true;
}

/* Encodes this KMM Warm-Start Command into the given data buffer. */

void KMMWarmStartCommand::encode(uint8_t* data)
{
    assert(data != nullptr);
    m_messageLength = length();

    KMMFrame::encodeHeader(data);

    uint32_t offset = 10U + m_bodyOffset;
    data[offset++] = m_decryptInfoFmt;                      // Decryption information format
    data[offset++] = m_kekAlgId;                            // KEK algorithm identifier
    SET_UINT16(m_kekKId, data, offset); offset += 2U;      // KEK key identifier

    if (m_decryptInfoFmt == KMM_DECRYPT_INSTRUCT_MI) {
        ::memcpy(data + offset, m_mi, sizeof(m_mi));
        offset += sizeof(m_mi);
    }

    data[offset++] = m_keyLength;                           // TEK key length in bytes
    data[offset++] = m_tekAlgId;                            // TEK algorithm identifier
    data[offset++] = m_key.keyFormat();                     // TEK key format

    uint16_t sln = m_key.sln();
    SET_UINT16(sln, data, offset);
    offset += 2U;

    uint16_t kid = m_key.kId();
    SET_UINT16(kid, data, offset);
    offset += 2U;

    m_key.getKey(data + offset);
}

/* Returns a string representation of this KMM Warm-Start Command. */

std::string KMMWarmStartCommand::toString()
{
    return std::string("KMM, WARM_START_CMD (Warm-Start Command)");
}

/*
** Encryption data 
*/

/* Sets the encryption message indicator. */

void KMMWarmStartCommand::setMI(const uint8_t* mi)
{
    assert(mi != nullptr);
    ::memcpy(m_mi, mi, sizeof(m_mi));
}

/* Gets the encryption message indicator. */

void KMMWarmStartCommand::getMI(uint8_t* mi) const
{
    assert(mi != nullptr);
    ::memcpy(mi, m_mi, sizeof(m_mi));
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void KMMWarmStartCommand::copy(const KMMWarmStartCommand& data)
{
    KMMFrame::copy(data);

    m_decryptInfoFmt = data.m_decryptInfoFmt;

    m_kekAlgId = data.m_kekAlgId;
    m_kekKId = data.m_kekKId;

    m_keyLength = data.m_keyLength;

    m_tekAlgId = data.m_tekAlgId;

    m_key = data.m_key;

    if (data.m_mi != nullptr) {
        ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);
        ::memcpy(m_mi, data.m_mi, MI_LENGTH_BYTES);
    }
}
