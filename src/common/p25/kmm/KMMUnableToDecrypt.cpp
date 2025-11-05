// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/kmm/KMMUnableToDecrypt.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::kmm;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the KMMUnableToDecrypt class. */

KMMUnableToDecrypt::KMMUnableToDecrypt() : KMMFrame(),
    m_bodyFormat(0U),
    m_algId(ALGO_UNENCRYPT),
    m_kId(0U),
    m_status(0U),
    m_decryptInfoFmt(KMM_DECRYPT_INSTRUCT_NONE),
    m_decryptAlgId(ALGO_UNENCRYPT),
    m_decryptKId(0U),
    m_key(),
    m_miSet(false),
    m_mi(nullptr)
{
    m_messageId = KMM_MessageType::UNABLE_TO_DECRYPT;
    m_respKind = KMM_ResponseKind::NONE;

    m_mi = new uint8_t[MI_LENGTH_BYTES];
    ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);
}

/* Finalizes a instance of the KMMUnableToDecrypt class. */

KMMUnableToDecrypt::~KMMUnableToDecrypt()
{
    if (m_mi != nullptr) {
        delete[] m_mi;
        m_mi = nullptr;
    }
}

/* Gets the byte length of this KMMUnableToDecrypt. */

uint32_t KMMUnableToDecrypt::length() const
{
    uint32_t len = KMMFrame::length() + KMM_BODY_UNABLE_TO_DECRYPT_LENGTH;
    if ((m_bodyFormat & KEY_FORMAT_TEK) == KEY_FORMAT_TEK) {
        len += 3U;
    }

    if (m_miSet) {
        len += MI_LENGTH_BYTES;
    }

    len += 1U + m_key.getLength();

    return len;
}

/* Decode a KMM Unable-To-Decrypt. */

bool KMMUnableToDecrypt::decode(const uint8_t* data)
{
    assert(data != nullptr);

    KMMFrame::decodeHeader(data);

    m_bodyFormat = data[10U + m_bodyOffset];                    // Body Format
    m_algId = data[12U + m_bodyOffset];                         // Algorithm ID
    m_kId = GET_UINT16(data, 13U + m_bodyOffset);               // Key ID
    m_status = data[15U + m_bodyOffset];                        // Status

    uint8_t offset = 0U;
    if ((m_bodyFormat & KEY_FORMAT_TEK) == KEY_FORMAT_TEK) {
        m_decryptInfoFmt = data[16U + m_bodyOffset];            // Decrypt Info Format
        m_decryptAlgId = data[17U + m_bodyOffset];              // Decrypt Algorithm ID
        m_decryptKId = GET_UINT16(data, 18U + m_bodyOffset);    // Decrypt Key ID
        offset += 4U;

        if ((m_decryptInfoFmt & KMM_DECRYPT_INSTRUCT_MI) == KMM_DECRYPT_INSTRUCT_MI) {
            ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);
            ::memcpy(m_mi, data + (m_bodyOffset + 20U), MI_LENGTH_BYTES);
            offset += MI_LENGTH_BYTES;
        }
    }

    uint8_t keyLength = data[16U + (m_bodyOffset + offset)];
    m_key.keyFormat(data[18U + (m_bodyOffset + offset)]);

    uint16_t sln = GET_UINT16(data, 19U + (m_bodyOffset + offset));
    m_key.sln(sln);

    uint16_t kId = GET_UINT16(data, 21U + (m_bodyOffset + offset));
    m_key.kId(kId);

    DECLARE_UINT8_ARRAY(keyPayload, keyLength);

    ::memcpy(keyPayload, data + (23U + (m_bodyOffset + offset)), keyLength);
    m_key.setKey(keyPayload, keyLength);

    return true;
}

/* Encode a KMM Unable-To-Decrypt. */

void KMMUnableToDecrypt::encode(uint8_t* data)
{
    assert(data != nullptr);
    m_messageLength = length();

    KMMFrame::encodeHeader(data);

    if (!m_miSet && m_decryptInfoFmt == KMM_DECRYPT_INSTRUCT_MI) {
        m_decryptInfoFmt = KMM_DECRYPT_INSTRUCT_NONE;
    }

    data[10U + m_bodyOffset] = m_bodyFormat;                    // Body Format
    data[11U + m_bodyOffset] = m_algId;                         // Algorithm ID
    SET_UINT16(m_kId, data, 13U + m_bodyOffset);                // Key ID
    data[15U + m_bodyOffset] = m_status;                        // Status

    uint8_t offset = 0U;
    if ((m_bodyFormat & KEY_FORMAT_TEK) == KEY_FORMAT_TEK) {
        data[16U + m_bodyOffset] = m_decryptInfoFmt;            // Decrypt Info Format
        data[17U + m_bodyOffset] = m_decryptAlgId;              // Decrypt Algorithm ID
        SET_UINT16(m_decryptKId, data, 18U + m_bodyOffset);     // Decrypt Key ID

        if (m_miSet) {
            ::memcpy(data + (m_bodyOffset + 20U), m_mi, MI_LENGTH_BYTES);
            offset += MI_LENGTH_BYTES;
        }
    }

    data[16U + (m_bodyOffset + offset)] = m_key.getLength();

    uint16_t sln = m_key.sln();
    SET_UINT16(sln, data, 19U + (m_bodyOffset + offset));

    uint16_t kId = m_key.kId();
    SET_UINT16(kId, data, 21U + (m_bodyOffset + offset));

    DECLARE_UINT8_ARRAY(keyPayload, m_key.getLength());
    m_key.getKey(keyPayload);

    ::memcpy(data + (23U + (m_bodyOffset + offset)), keyPayload, m_key.getLength());
}

/* Returns a string that represents the current KMM frame. */

std::string KMMUnableToDecrypt::toString()
{
    return std::string("KMM, UNABLE_TO_DECRYPT (Unable to Decrypt)");
}

/*
** Encryption data 
*/

/* Sets the encryption message indicator. */

void KMMUnableToDecrypt::setMI(const uint8_t* mi)
{
    assert(mi != nullptr);

    m_miSet = true;
    ::memcpy(m_mi, mi, MI_LENGTH_BYTES);
}

/* Gets the encryption message indicator. */

void KMMUnableToDecrypt::getMI(uint8_t* mi) const
{
    assert(mi != nullptr);

    ::memcpy(mi, m_mi, MI_LENGTH_BYTES);
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void KMMUnableToDecrypt::copy(const KMMUnableToDecrypt& data)
{
    KMMFrame::copy(data);

    m_bodyFormat = data.m_bodyFormat;
    m_algId = data.m_algId;
    m_kId = data.m_kId;
    m_status = data.m_status;

    m_decryptInfoFmt = data.m_decryptInfoFmt;
    m_decryptAlgId = data.m_decryptAlgId;
    m_decryptKId = data.m_decryptKId;

    m_key = data.m_key;

    if (data.m_mi != nullptr) {
        ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);
        ::memcpy(m_mi, data.m_mi, MI_LENGTH_BYTES);
    }
}
