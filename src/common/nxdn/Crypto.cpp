// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 C. Lovell, Dev_Ranger
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "nxdn/NXDNDefines.h"
#include "nxdn/Crypto.h"
#include "AESCrypto.h"
#include "DESCrypto.h"
#include "Log.h"

using namespace ::crypto;
using namespace nxdn::defines;
using namespace nxdn::crypto;

#include <algorithm>
#include <cassert>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define KEYSTREAM_LENGTH_BYTES 196U

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the NXDNCrypto class. */

NXDNCrypto::NXDNCrypto() :
    m_tekCipherType(CIPHER_TYPE_NONE),
    m_tekKeyId(0U),
    m_tekLength(0U),
    m_keystream(nullptr),
    m_mi(new uint8_t[MI_LENGTH_BYTES]),
    m_tek(nullptr),
    m_random()
{
    ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);
    std::random_device rd;
    m_random.seed(rd());
}

/* Finalizes a instance of the NXDNCrypto class. */

NXDNCrypto::~NXDNCrypto()
{
    if (m_keystream != nullptr) {
        ::memset(m_keystream, 0x00U, KEYSTREAM_LENGTH_BYTES);
        delete[] m_keystream;
    }

    ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);
    delete[] m_mi;
}

/* Helper to generate a new initial seed MI. */

void NXDNCrypto::generateMI()
{
    do {
        std::uniform_int_distribution<uint32_t> dist(0U, 255U);
        for (uint8_t i = 0U; i < MI_LENGTH_BYTES; i++)
            m_mi[i] = (uint8_t)dist(m_random);
    } while (!hasValidMI());
}

/* Helper given the last MI, generate the next MI using LFSR. */

void NXDNCrypto::generateNextMI()
{
    if (!hasValidMI())
        return;

    uint8_t nextMI[MI_LENGTH_BYTES];
    ::memcpy(nextMI, m_mi, MI_LENGTH_BYTES);

    for (uint8_t cycle = 0U; cycle < 64U; cycle++) {
        // Calculate bit 0 for the next cycle.
        uint8_t carry = ((nextMI[0U] >> 7U) ^ (nextMI[0U] >> 5U) ^
                         (nextMI[2U] >> 5U) ^ (nextMI[3U] >> 5U) ^
                         (nextMI[4U] >> 2U) ^ (nextMI[6U] >> 6U)) & 0x01U;

        // Shift each byte and carry in the high bit from the next byte.
        uint8_t i;
        for (i = 0U; i < MI_LENGTH_BYTES - 1U; i++)
            nextMI[i] = ((nextMI[i] & 0x7FU) << 1U) | (nextMI[i + 1U] >> 7U);

        nextMI[i] = ((nextMI[i] & 0x7FU) << 1U) | carry;
    }

    ::memcpy(m_mi, nextMI, MI_LENGTH_BYTES);
}

/* Helper to check if there is a valid encryption keystream. */

bool NXDNCrypto::hasValidKeystream() const
{
    return m_tek != nullptr && m_tekLength > 0U && m_keystream != nullptr;
}

/* Helper to generate the encryption keystream. */

void NXDNCrypto::generateKeystream()
{
    if (m_tek == nullptr || m_tekLength == 0U)
        return;
    if (m_tekCipherType != CIPHER_TYPE_EHR && !hasValidMI())
        return;
    if (m_keystream == nullptr)
        m_keystream = new uint8_t[KEYSTREAM_LENGTH_BYTES];

    ::memset(m_keystream, 0x00U, KEYSTREAM_LENGTH_BYTES);
    switch (m_tekCipherType) {
    case CIPHER_TYPE_EHR:
        {
            uint16_t lfsr = (uint16_t(m_tek[0U]) << 8U) | m_tek[1U];
            for (uint32_t bit = 0U; bit < uint32_t(EHR_WORDS_PER_SESSION) * AMBE_LENGTH_BITS; bit++) {
                m_keystream[bit >> 3U] |= (lfsr & 1U) << (7U - (bit & 7U));
                lfsr = (lfsr >> 1U) | (((lfsr ^ (lfsr >> 1U)) & 1U) << 14U);
            }
        }
        break;
    case CIPHER_TYPE_DES:
        {
            DES des;
            uint8_t input[DES_KEY_LENGTH_BYTES];
            ::memcpy(input, m_mi, sizeof(input));

            uint8_t* out = des.encryptBlock(input, m_tek.get());
            ::memcpy(input, out, sizeof(input));
            delete[] out;

            for (uint32_t off = 0U; off < KEYSTREAM_LENGTH_BYTES; off += sizeof(input)) {
                out = des.encryptBlock(input, m_tek.get());
                ::memcpy(input, out, sizeof(input));
                ::memcpy(m_keystream + off, out, std::min<uint32_t>(sizeof(input), KEYSTREAM_LENGTH_BYTES - off));
                delete[] out;
            }
        }
        break;
    case CIPHER_TYPE_AES:
        {
            AES aes(AESKeyLength::AES_256);
            uint8_t input[16U];
            ::memcpy(input, m_mi, MI_LENGTH_BYTES);

            uint64_t next = 0U;
            for (uint8_t i = 0U; i < MI_LENGTH_BYTES; i++)
                next = (next << 8U) | m_mi[i];

            for (uint8_t i = 0U; i < 64U; i++)
                stepLFSR(next);

            for (int8_t i = 15; i >= int8_t(MI_LENGTH_BYTES); i--) {
                input[i] = (uint8_t)(next & 0xFFU);
                next >>= 8U;
            }

            uint8_t* out = aes.encryptECB(input, sizeof(input), m_tek.get());
            ::memcpy(input, out, sizeof(input));
            delete[] out;
            
            for (uint32_t off = 0U; off < KEYSTREAM_LENGTH_BYTES; off += sizeof(input)) {
                out = aes.encryptECB(input, sizeof(input), m_tek.get());
                ::memcpy(input, out, sizeof(input));
                ::memcpy(m_keystream + off, out, std::min<uint32_t>(sizeof(input), KEYSTREAM_LENGTH_BYTES - off));
                delete[] out;
            }
        }
        break;
    default:
        LogError(LOG_NXDN, "unsupported crypto algorithm, cipherType = $%02X", m_tekCipherType);
        if (m_keystream != nullptr) {
            delete[] m_keystream;
            m_keystream = nullptr;
        }
        break;
    }
}

/* Helper to reset the encryption keystream. */

void NXDNCrypto::resetKeystream()
{
    clearMI();

    if (m_keystream != nullptr) {
        ::memset(m_keystream, 0x00U, KEYSTREAM_LENGTH_BYTES);
        delete[] m_keystream;
        m_keystream = nullptr;
    }
}

/* Helper to crypt an AMBE word using the EHR algorithm. */

void NXDNCrypto::cryptEHR_AMBE(uint8_t* ambe, uint8_t n) const
{
    if (m_tekCipherType == CIPHER_TYPE_EHR)
        cryptAMBE(ambe, n, EHR_WORDS_PER_SESSION);
}

/* Helper to crypt an AMBE word using the DES algorithm. */

void NXDNCrypto::cryptDES_AMBE(uint8_t* ambe, uint8_t n) const
{
    if (m_tekCipherType == CIPHER_TYPE_DES)
        cryptAMBE(ambe, n, BLOCK_WORDS_PER_SESSION);
}

/* Helper to crypt an AMBE word using the AES algorithm. */

void NXDNCrypto::cryptAES_AMBE(uint8_t* ambe, uint8_t n) const
{
    if (m_tekCipherType == CIPHER_TYPE_AES)
        cryptAMBE(ambe, n, BLOCK_WORDS_PER_SESSION);
}

/* Helper to check if there is a valid encryption message indicator. */

bool NXDNCrypto::hasValidMI() const
{
    for (uint8_t i = 0U; i < MI_LENGTH_BYTES; i++) {
        if (m_mi[i] != 0x00U)
            return true;
    }

    return false;
}

/* Helper to set the encryption message indicator. */

void NXDNCrypto::setMI(const uint8_t* mi)
{
    assert(mi != nullptr);
    ::memcpy(m_mi, mi, MI_LENGTH_BYTES);
}

/* Helper to get the encryption message indicator. */

void NXDNCrypto::getMI(uint8_t* mi) const
{
    assert(mi != nullptr);
    ::memcpy(mi, m_mi, MI_LENGTH_BYTES);
}

/* Helper to clear the encryption message indicator. */

void NXDNCrypto::clearMI()
{
    ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);
}

/* Helper to set the encryption key. */

void NXDNCrypto::setKey(const uint8_t* key, uint8_t len)
{
    assert(key != nullptr);

    uint8_t expected = m_tekCipherType == CIPHER_TYPE_EHR ? EHR_KEY_LENGTH_BYTES :
        m_tekCipherType == CIPHER_TYPE_DES ? DES_KEY_LENGTH_BYTES :
        m_tekCipherType == CIPHER_TYPE_AES ? AES_KEY_LENGTH_BYTES : 0U;

    bool valid = expected > 0U && len == expected && m_tekKeyId > 0U && m_tekKeyId <= 63U;
    if (valid && m_tekCipherType == CIPHER_TYPE_EHR) {
        uint16_t value = (uint16_t(key[0U]) << 8U) | key[1U];
        valid = value > 0U && value <= 0x7FFFU;
    }

    if (valid && m_tekCipherType == CIPHER_TYPE_DES)
        valid = !isWeakDESKey(key);

    if (!valid) {
        LogError(LOG_NXDN, "invalid crypto key, cipherType = $%02X, keyId = $%02X, len = %u", m_tekCipherType, m_tekKeyId, len);
        clearKey();
        return;
    }

    clearKey();
    m_tek = std::make_unique<uint8_t[]>(len);
    ::memcpy(m_tek.get(), key, len);

    m_tekLength = len;
}

/* Helper to get the encryption key. */

void NXDNCrypto::getKey(uint8_t* key) const
{
    assert(key != nullptr);

    if (m_tek != nullptr)
        ::memcpy(key, m_tek.get(), m_tekLength);
}

/* Helper to clear the stored encryption key. */

void NXDNCrypto::clearKey()
{
    if (m_tek != nullptr) {
        ::memset(m_tek.get(), 0U, m_tekLength);
        m_tek.reset();
    }
    
    m_tekLength = 0U;
    if (m_keystream != nullptr) {
        ::memset(m_keystream, 0U, KEYSTREAM_LENGTH_BYTES);
        delete[] m_keystream;
        m_keystream = nullptr;
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Crypt AMBE data using the keystream. */

void NXDNCrypto::cryptAMBE(uint8_t* ambe, uint8_t word, uint8_t words) const
{
    assert(ambe != nullptr);
    if (m_keystream == nullptr || word >= words)
        return;

    for (uint32_t bit = 0U; bit < AMBE_LENGTH_BITS; bit++) {
        uint32_t pos = uint32_t(word) * AMBE_LENGTH_BITS + bit;
        ambe[bit] ^= (m_keystream[pos >> 3U] >> (7U - (pos & 7U))) & 1U;
    }
}

/* Helper to step the linear feedback shift register (LFSR). */

uint64_t NXDNCrypto::stepLFSR(uint64_t& lfsr)
{
    uint64_t ovBit = (lfsr >> 63U) & 0x01U;

    // compute feedback bit using polynomial: x^64 + x^62 + x^46 + x^38 + x^27 + x^15 + 1
    uint64_t fbBit = ((lfsr >> 63U) ^ (lfsr >> 61U) ^ (lfsr >> 45U) ^ (lfsr >> 37U) ^
                      (lfsr >> 26U) ^ (lfsr >> 14U)) & 0x01U;

    // shift LFSR left and insert feedback bit
    lfsr = (lfsr << 1) | fbBit;
    return ovBit;
}

/* Helper to check for weak DES keys. */

bool NXDNCrypto::isWeakDESKey(const uint8_t* key)
{
    static const uint8_t WEAK_KEYS[][DES_KEY_LENGTH_BYTES] = {
        { 0x01U, 0x01U, 0x01U, 0x01U, 0x01U, 0x01U, 0x01U, 0x01U },
        { 0xFEU, 0xFEU, 0xFEU, 0xFEU, 0xFEU, 0xFEU, 0xFEU, 0xFEU },
        { 0xE0U, 0xE0U, 0xE0U, 0xE0U, 0xF1U, 0xF1U, 0xF1U, 0xF1U },
        { 0x1FU, 0x1FU, 0x1FU, 0x1FU, 0x0EU, 0x0EU, 0x0EU, 0x0EU },
        { 0x01U, 0xFEU, 0x01U, 0xFEU, 0x01U, 0xFEU, 0x01U, 0xFEU },
        { 0xFEU, 0x01U, 0xFEU, 0x01U, 0xFEU, 0x01U, 0xFEU, 0x01U },
        { 0x1FU, 0xE0U, 0x1FU, 0xE0U, 0x0EU, 0xF1U, 0x0EU, 0xF1U },
        { 0xE0U, 0x1FU, 0xE0U, 0x1FU, 0xF1U, 0x0EU, 0xF1U, 0x0EU },
        { 0x01U, 0xE0U, 0x01U, 0xE0U, 0x01U, 0xF1U, 0x01U, 0xF1U },
        { 0xE0U, 0x01U, 0xE0U, 0x01U, 0xF1U, 0x01U, 0xF1U, 0x01U },
        { 0x1FU, 0xFEU, 0x1FU, 0xFEU, 0x0EU, 0xFEU, 0x0EU, 0xFEU },
        { 0xFEU, 0x1FU, 0xFEU, 0x1FU, 0xFEU, 0x0EU, 0xFEU, 0x0EU },
        { 0x01U, 0x1FU, 0x01U, 0x1FU, 0x01U, 0x0EU, 0x01U, 0x0EU },
        { 0x1FU, 0x01U, 0x1FU, 0x01U, 0x0EU, 0x01U, 0x0EU, 0x01U },
        { 0xE0U, 0xFEU, 0xE0U, 0xFEU, 0xF1U, 0xFEU, 0xF1U, 0xFEU },
        { 0xFEU, 0xE0U, 0xFEU, 0xE0U, 0xFEU, 0xF1U, 0xFEU, 0xF1U }
    };

    for (const auto& weakKey : WEAK_KEYS) {
        bool match = true;
        for (uint8_t i = 0U; i < DES_KEY_LENGTH_BYTES; i++) {
            // DES parity bits do not contribute to the effective 56-bit key
            if ((key[i] & 0xFEU) != (weakKey[i] & 0xFEU)) {
                match = false;
                break;
            }
        }

        if (match)
            return true;
    }

    return false;
}
