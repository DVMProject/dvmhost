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
#include "p25/kmm/KMMRekeyCommand.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::kmm;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the KMMRekeyCommand class. */

KMMRekeyCommand::KMMRekeyCommand() : KMMFrame(),
    m_decryptInfoFmt(KMM_DECRYPT_INSTRUCT_NONE),
    m_algId(ALGO_UNENCRYPT),
    m_kId(0U),
    m_keysets(),
    m_miSet(false),
    m_mi(nullptr)
{
    m_messageId = KMM_MessageType::REKEY_CMD;

    m_mi = new uint8_t[MI_LENGTH_BYTES];
    ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);
}

/* Finalizes a instance of the KMMRekeyCommand class. */

KMMRekeyCommand::~KMMRekeyCommand()
{
    if (m_mi != nullptr) {
        delete[] m_mi;
        m_mi = nullptr;
    }
}

/* Gets the byte length of this KMMRekeyCommand. */

uint32_t KMMRekeyCommand::length() const
{
    uint32_t len = KMM_REKEY_CMD_LENGTH;
    if (m_miSet)
        len += MI_LENGTH_BYTES;

    for (auto keysetItem : m_keysets)
        len += keysetItem.length();

    return len;
}

/* Decode a KMM modify key. */

bool KMMRekeyCommand::decode(const uint8_t* data)
{
    assert(data != nullptr);

    KMMFrame::decodeHeader(data);

    m_decryptInfoFmt = data[10U];                               // Decryption Instruction Format
    m_algId = data[11U];                                        // Algorithm ID
    m_kId = GET_UINT16(data, 12U);                              // Key ID

    uint16_t offset = 0U;
    if (m_decryptInfoFmt == KMM_DECRYPT_INSTRUCT_MI) {
        ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);
        ::memcpy(m_mi, data + 14U, MI_LENGTH_BYTES);
        offset += 9U;
    }

    uint8_t keysetCount = data[14U + offset];
    for (uint8_t n = 0U; n < keysetCount; n++) {
        KeysetItem keysetItem = KeysetItem();

        keysetItem.keysetId(data[16U + offset]);
        keysetItem.algId(data[17U + offset]);
        keysetItem.keyLength(data[18U + offset]);

        uint8_t keyCount = data[19U + offset];
        for (uint8_t i = 0U; i < keyCount; i++) {
            KeyItem key = KeyItem();

            DECLARE_UINT8_ARRAY(keyPayload, keysetItem.keyLength());

            uint8_t keyFormat = data[20U + offset];
            uint8_t keyNameLen = keyFormat & 0x1FU;

            key.keyFormat(keyFormat & 0xE0U);

            uint16_t sln = GET_UINT16(data, 21U + offset);
            key.sln(sln);

            uint16_t kId = GET_UINT16(data, 23U + offset);
            key.kId(kId);

            ::memcpy(keyPayload, data + (25U + offset), keysetItem.keyLength());
            key.setKey(keyPayload, keysetItem.keyLength());

            keysetItem.push_back(key);

            offset += 5U + keyNameLen + keysetItem.keyLength();
        }

        m_keysets.push_back(keysetItem);
        offset += 4U;
    }

    return true;
}

/* Encode a KMM modify key. */

void KMMRekeyCommand::encode(uint8_t* data)
{
    assert(data != nullptr);
    m_messageLength = length();

    KMMFrame::encodeHeader(data);

    if (!m_miSet && m_decryptInfoFmt == KMM_DECRYPT_INSTRUCT_MI) {
        m_decryptInfoFmt = KMM_DECRYPT_INSTRUCT_NONE;
    }

    data[10U] = m_decryptInfoFmt;                               // Decryption Instruction Format
    data[11U] = m_algId;                                        // Algorithm ID
    SET_UINT16(m_kId, data, 12U);                               // Key ID

    uint16_t offset = 0U;
    if (m_decryptInfoFmt == KMM_DECRYPT_INSTRUCT_MI) {
        ::memcpy(data + 14U, m_mi, MI_LENGTH_BYTES);
        offset += 9U;
    }

    uint8_t keysetCount = m_keysets.size();
    data[14U + offset] = keysetCount;
    for (auto keysetItem : m_keysets) {
        data[16U + offset] = keysetItem.keysetId();
        data[17U + offset] = keysetItem.algId();
        data[18U + offset] = keysetItem.keyLength();

        uint8_t keyCount = keysetItem.keys().size();
        data[19U + offset] = keyCount;
        for (auto key : keysetItem.keys()) {
            uint8_t keyNameLen = key.keyFormat() & 0x1FU;
            data[18U + offset] = key.keyFormat();
            SET_UINT16(key.sln(), data, 19U + offset);
            SET_UINT16(key.kId(), data, 21U + offset);

            DECLARE_UINT8_ARRAY(keyPayload, keysetItem.keyLength());
            key.getKey(keyPayload);

            ::memcpy(data + (23U + offset), keyPayload, keysetItem.keyLength());

            offset += 5U + keyNameLen + keysetItem.keyLength();
        }

        offset += 4U;
    }
}

/* Returns a string that represents the current KMM frame. */

std::string KMMRekeyCommand::toString()
{
    return std::string("KMM, REKEY_CMD (Rekey Command)");
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void KMMRekeyCommand::copy(const KMMRekeyCommand& data)
{
    KMMFrame::copy(data);

    m_decryptInfoFmt = data.m_decryptInfoFmt;
    m_algId = data.m_algId;
    m_kId = data.m_kId;

    if (data.m_mi != nullptr) {
        ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);
        ::memcpy(m_mi, data.m_mi, MI_LENGTH_BYTES);
    }

    m_keysets = data.m_keysets;
}
