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
#include "p25/kmm/KeysetItem.h"
#include "p25/P25Defines.h"
#include "p25/Crypto.h"
#include "AESCrypto.h"
#include "RC4Crypto.h"
#include "Log.h"
#include "Utils.h"

using namespace ::crypto;
using namespace p25;
using namespace p25::defines;
using namespace p25::crypto;

#include <cassert>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define MAX_ENC_KEY_LENGTH_BYTES 32U

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the P25Crypto class. */

P25Crypto::P25Crypto() :
    m_tekAlgoId(ALGO_UNENCRYPT),
    m_tekKeyId(0U),
    m_tekLength(0U),
    m_keystream(nullptr),
    m_keystreamPos(0U),
    m_mi(nullptr),
    m_tek(nullptr),
    m_random()
{
    m_mi = new uint8_t[MI_LENGTH_BYTES];
    ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);

    std::random_device rd;
    std::mt19937 mt(rd());
    m_random = mt;
}

/* Finalizes a instance of the P25Crypto class. */

P25Crypto::~P25Crypto()
{
    if (m_keystream != nullptr)
        delete[] m_keystream;
    delete[] m_mi;
}

/* Helper given to generate a new initial seed MI. */

void P25Crypto::generateMI()
{
    for (uint8_t i = 0; i < MI_LENGTH_BYTES; i++) {
        std::uniform_int_distribution<uint32_t> dist(0x00U, 0xFFU);
        m_mi[i] = (uint8_t)dist(m_random);
    }
}

/* Given the last MI, generate the next MI using LFSR. */

void P25Crypto::generateNextMI()
{
    uint8_t carry, i;
    
    uint8_t nextMI[9U];
    ::memcpy(nextMI, m_mi, MI_LENGTH_BYTES);

    for (uint8_t cycle = 0; cycle < 64; cycle++) {
        // calculate bit 0 for the next cycle
        carry = ((nextMI[0] >> 7) ^ (nextMI[0] >> 5) ^ (nextMI[2] >> 5) ^
                 (nextMI[3] >> 5) ^ (nextMI[4] >> 2) ^ (nextMI[6] >> 6)) &
                0x01;

        // shift all the list elements, except the last one
        for (i = 0; i < 7; i++) {
            // grab high bit from the next element and use it as our low bit
            nextMI[i] = ((nextMI[i] & 0x7F) << 1) | (nextMI[i + 1] >> 7);
        }

        // shift last element, then copy the bit 0 we calculated in
        nextMI[7] = ((nextMI[i] & 0x7F) << 1) | carry;
    }

    ::memcpy(m_mi, nextMI, MI_LENGTH_BYTES);
}

/* Helper to check if there is a valid encryption keystream. */

bool P25Crypto::hasValidKeystream()
{
    if (m_tek == nullptr)
        return false;
    if (m_tekLength == 0U)
        return false;
    if (m_keystream == nullptr)
        return false;

    return true;
}

/* Helper to generate the encryption keystream. */

void P25Crypto::generateKeystream()
{
    if (m_tek == nullptr)
        return;
    if (m_tekLength == 0U)
        return;
    if (m_mi == nullptr)
        return;

    m_keystreamPos = 0U;

    // generate keystream
    switch (m_tekAlgoId) {
    case ALGO_AES_256:
        {
            if (m_keystream == nullptr)
                m_keystream = new uint8_t[240U];
            ::memset(m_keystream, 0x00U, 240U);

            uint8_t* iv = expandMIToIV();

            AES aes = AES(AESKeyLength::AES_256);

            uint8_t input[16U];
            ::memset(input, 0x00U, 16U);
            ::memcpy(input, iv, 16U);

            for (uint32_t i = 0U; i < (240U / 16U); i++) {
                uint8_t* output = aes.encryptECB(input, 16U, m_tek.get());
                ::memcpy(m_keystream + (i * 16U), output, 16U);
                ::memcpy(input, output, 16U);
            }

            delete[] iv;
        }
        break;
    case ALGO_ARC4:
        {
            if (m_keystream == nullptr)
                m_keystream = new uint8_t[469U];
            ::memset(m_keystream, 0x00U, 469U);

            uint8_t padding = (uint8_t)::fmax(5U - m_tekLength, 0U);
            uint8_t adpKey[13U];
            ::memset(adpKey, 0x00U, 13U);

            uint8_t i = 0U;
            for (i = 0U; i < padding; i++)
                adpKey[i] = 0x00U;

            for (; i < 5U; i++)
                adpKey[i] = (m_tekLength > 0U) ? m_tek[i - padding] : 0x00U;

            for (i = 5U; i < 13U; i++)
                adpKey[i] = m_mi[i - 5U];

            // generate ARC4 keystream
            RC4 rc4 = RC4();
            m_keystream = rc4.keystream(469U, adpKey, 13U);
        }
        break;
    default:
        LogError(LOG_P25, "unsupported crypto algorithm, algId = $%02X", m_tekAlgoId);
        break;
    }
}

/* Helper to reset the encryption keystream. */

void P25Crypto::resetKeystream()
{
    ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);
    if (m_keystream != nullptr) {
        delete[] m_keystream;
        m_keystream = nullptr;
        m_keystreamPos = 0U;
    }
}

/* Helper to crypt IMBE audio using AES-256. */

void P25Crypto::cryptAES_IMBE(uint8_t* imbe, DUID::E duid)
{
    if (m_keystream == nullptr)
        return;

    uint32_t offset = 16U;
    if (duid == DUID::LDU2) {
        offset += 101U;
    }

    offset += (m_keystreamPos * RAW_IMBE_LENGTH_BYTES) + RAW_IMBE_LENGTH_BYTES + ((m_keystreamPos < 8U) ? 0U : 2U);
    m_keystreamPos = (m_keystreamPos + 1U) % 9U;

    for (uint8_t i = 0U; i < RAW_IMBE_LENGTH_BYTES; i++) {
        imbe[i] ^= m_keystream[offset + i];
    }
}

/* Helper to crypt IMBE audio using ARC4. */

void P25Crypto::cryptARC4_IMBE(uint8_t* imbe, DUID::E duid)
{
    if (m_keystream == nullptr)
        return;

    uint32_t offset = 0U;
    if (duid == DUID::LDU2) {
        offset += 101U;
    }

    offset += (m_keystreamPos * RAW_IMBE_LENGTH_BYTES) + 267U + ((m_keystreamPos < 8U) ? 0U : 2U);
    m_keystreamPos = (m_keystreamPos + 1U) % 9U;

    for (uint8_t i = 0U; i < RAW_IMBE_LENGTH_BYTES; i++) {
        imbe[i] ^= m_keystream[offset + i];
    }
}

/* Helper to check if there is a valid encryption message indicator. */

bool P25Crypto::hasValidMI()
{
    bool hasMI = false;
    for (uint8_t i = 0; i < MI_LENGTH_BYTES; i++) {
        if (m_mi[i] != 0x00U)
            hasMI = true;
    }

    return hasMI;
}

/* Sets the encryption message indicator. */

void P25Crypto::setMI(const uint8_t* mi)
{
    assert(mi != nullptr);

    ::memcpy(m_mi, mi, MI_LENGTH_BYTES);
}

/* Gets the encryption message indicator. */

void P25Crypto::getMI(uint8_t* mi) const
{
    assert(mi != nullptr);

    ::memcpy(mi, m_mi, MI_LENGTH_BYTES);
}

/* Clears the stored encryption message indicator. */

void P25Crypto::clearMI()
{
    ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);
}

/* Sets the encryption key. */

void P25Crypto::setKey(const uint8_t* key, uint8_t len)
{
    assert(key != nullptr);

    m_tekLength = len;
    if (m_tek != nullptr)
        m_tek.reset();

    m_tek = std::make_unique<uint8_t[]>(len);
    ::memset(m_tek.get(), 0x00U, MAX_ENC_KEY_LENGTH_BYTES);
    ::memset(m_tek.get(), 0x00U, m_tekLength);
    ::memcpy(m_tek.get(), key, len);
}

/* Gets the encryption key. */

void P25Crypto::getKey(uint8_t* key) const
{
    assert(key != nullptr);

    ::memcpy(key, m_tek.get(), m_tekLength);
}

/* Clears the stored encryption key. */

void P25Crypto::clearKey()
{
    m_tekLength = 0U;
    if (m_tek != nullptr)
        m_tek.reset();

    m_tek = std::make_unique<uint8_t[]>(MAX_ENC_KEY_LENGTH_BYTES);
    ::memset(m_tek.get(), 0x00U, MAX_ENC_KEY_LENGTH_BYTES);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* */

uint64_t P25Crypto::stepLFSR(uint64_t& lfsr)
{
    uint64_t ovBit = (lfsr >> 63U) & 0x01U;

    // compute feedback bit using polynomial: x^64 + x^62 + x^46 + x^38 + x^27 + x^15 + 1
    uint64_t fbBit = ((lfsr >> 63U) ^ (lfsr >> 61U) ^ (lfsr >> 45U) ^ (lfsr >> 37U) ^
                      (lfsr >> 26U) ^ (lfsr >> 14U)) & 0x01U;

    // shift LFSR left and insert feedback bit
    lfsr = (lfsr << 1) | fbBit;
    return ovBit;
}

/* Expands the 9-byte MI into a proper 16-byte IV. */

uint8_t* P25Crypto::expandMIToIV()
{
    // this should never happen...
    if (m_mi == nullptr)
        return nullptr;

    uint8_t* iv = new uint8_t[16U];
    ::memset(iv, 0x00U, 16U);

    // copy first 64-bits of the MI info LFSR
    uint64_t lfsr = 0U;
    for (uint8_t i = 0U; i < 8U; i++) {
        lfsr = (lfsr << 8U) | m_mi[i];
    }

    uint64_t overflow = 0U;
    for (uint8_t i = 0U; i < 64U; i++) {
        overflow = (overflow << 1U) | stepLFSR(lfsr);
    }

    // copy expansion and LFSR into IV
    for (int i = 7; i >= 0; i--) {
        iv[i] = (uint8_t)(overflow & 0xFFU);
        overflow >>= 8U;
    }

    for (int i = 15; i >= 8; i--) {
        iv[i] = (uint8_t)(lfsr & 0xFFU);
        lfsr >>= 8U;
    }

    return iv;
}
