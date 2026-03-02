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
#include "DESCrypto.h"
#include "RC4Crypto.h"
#include "Log.h"
#include "Utils.h"

#if defined(ENABLE_SSL)
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/err.h>
#include <openssl/core_names.h>
#endif // ENABLE_SSL

using namespace ::crypto;
using namespace p25;
using namespace p25::defines;
using namespace p25::crypto;

#include <cassert>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define TEMP_BUFFER_LEN 1024U
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
    case ALGO_DES:
        {
            if (m_keystream == nullptr)
                m_keystream = new uint8_t[224U];
            ::memset(m_keystream, 0x00U, 224U);

            uint8_t desKey[8U];
            ::memset(desKey, 0x00U, 8U);
            uint8_t padLen = (uint8_t)::fmax(8 - m_tekLength, 0);
            for (uint8_t i = 0U; i < padLen; i++)
                desKey[i] = 0U;
            for (uint8_t i = padLen; i < 8U; i++)
                desKey[i] = m_tek[i - padLen];

            DES des = DES();

            uint8_t input[8U];
            ::memset(input, 0x00U, 8U);
            ::memcpy(input, m_mi, 8U);

            for (uint32_t i = 0U; i < (224U / 8U); i++) {
                uint8_t* output = des.encryptBlock(input, desKey);
                ::memcpy(m_keystream + (i * 8U), output, 8U);
                ::memcpy(input, output, 8U);
                delete[] output;
            }
        }
        break;
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
                delete[] output;
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

/* Helper to crypt a P25 TEK with the given AES-256 KEK. */

UInt8Array P25Crypto::cryptAES_TEK(const uint8_t* kek, uint8_t* tek, uint8_t tekLen)
{
#if defined(ENABLE_SSL)
    // static IV with $A6 pattern defined in TIA-102.AACA-C-2023 13.3
    uint8_t iv[AES::BLOCK_BYTES_LEN / 2] = {
        0xA6U, 0xA6U, 0xA6U, 0xA6U, 0xA6U, 0xA6U, 0xA6U, 0xA6U
    };

    int len;
    uint8_t tempBuf[TEMP_BUFFER_LEN];
    ::memset(tempBuf, 0x00U, TEMP_BUFFER_LEN);

    ERR_load_crypto_strings();

    EVP_CIPHER_CTX* ctx;

    // create and initialize a cipher context
    if (!(ctx = EVP_CIPHER_CTX_new())) {
        LogError(LOG_P25, "EVP_CIPHER_CTX_new(), failed to initialize cipher context: %s", ERR_error_string(ERR_get_error(), NULL));
        return nullptr;
    }

    // initialize the wrapper context with AES-256-WRAP
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_wrap(), NULL, kek, iv) != 1) {
        LogError(LOG_P25, "EVP_EncryptInit_ex(), failed to initialize cipher wrapping context: %s", ERR_error_string(ERR_get_error(), NULL));
        EVP_CIPHER_CTX_free(ctx);
        return nullptr;
    }

    // perform the wrapping operation
    if (EVP_EncryptUpdate(ctx, tempBuf, &len, tek, tekLen) != 1) {
        LogError(LOG_P25, "EVP_EncryptUpdate(), failed to wrap TEK: %s", ERR_error_string(ERR_get_error(), NULL));
        EVP_CIPHER_CTX_free(ctx);
        return nullptr;
    }

    // finalize the wrapping (no output, just padding)
    int tempLen;
    if (EVP_EncryptFinal_ex(ctx, tempBuf + len, &tempLen) != 1) {
        LogError(LOG_P25, "EVP_EncryptFinal_ex(), failed to finalize wrapping TEK: %s", ERR_error_string(ERR_get_error(), NULL));
        EVP_CIPHER_CTX_free(ctx);
        return nullptr;
    }
    len += tempLen;

    EVP_CIPHER_CTX_free(ctx);

    UInt8Array wrappedKey = std::unique_ptr<uint8_t[]>(new uint8_t[len]);
    ::memset(wrappedKey.get(), 0x00U, len);
    ::memcpy(wrappedKey.get(), tempBuf, len);

    return wrappedKey;
#else
    LogError(LOG_P25, "No OpenSSL, TEK encryption is not supported!");
    return nullptr;
#endif // ENABLE_SSL
}

/* Helper to decrypt a P25 TEK with the given AES-256 KEK. */

UInt8Array P25Crypto::decryptAES_TEK(const uint8_t* kek, uint8_t* tek, uint8_t tekLen)
{
#if defined(ENABLE_SSL)
    // static IV with $A6 pattern defined in TIA-102.AACA-C-2023 13.3
    uint8_t iv[AES::BLOCK_BYTES_LEN / 2] = {
        0xA6U, 0xA6U, 0xA6U, 0xA6U, 0xA6U, 0xA6U, 0xA6U, 0xA6U
    };

    int len;
    uint8_t tempBuf[TEMP_BUFFER_LEN];
    ::memset(tempBuf, 0x00U, TEMP_BUFFER_LEN);

    ERR_load_crypto_strings();

    EVP_CIPHER_CTX* ctx;

    // create and initialize a cipher context
    if (!(ctx = EVP_CIPHER_CTX_new())) {
        LogError(LOG_P25, "EVP_CIPHER_CTX_new(), failed to initialize cipher context: %s", ERR_error_string(ERR_get_error(), NULL));
        return nullptr;
    }

    // initialize the wrapper context with AES-256-WRAP
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_wrap(), NULL, kek, iv) != 1) {
        LogError(LOG_P25, "EVP_DecryptInit_ex(), failed to initialize cipher wrapping context: %s", ERR_error_string(ERR_get_error(), NULL));
        EVP_CIPHER_CTX_free(ctx);
        return nullptr;
    }

    // perform the wrapping operation
    if (EVP_DecryptUpdate(ctx, tempBuf, &len, tek, tekLen) != 1) {
        LogError(LOG_P25, "EVP_DecryptUpdate(), failed to unwrap TEK: %s", ERR_error_string(ERR_get_error(), NULL));
        EVP_CIPHER_CTX_free(ctx);
        return nullptr;
    }

    // finalize the wrapping (no output, just padding)
    int tempLen;
    if (EVP_DecryptFinal_ex(ctx, tempBuf + len, &tempLen) != 1) {
        LogError(LOG_P25, "EVP_DecryptFinal_ex(), failed to finalize unwrapping TEK: %s", ERR_error_string(ERR_get_error(), NULL));
        EVP_CIPHER_CTX_free(ctx);
        return nullptr;
    }
    len += tempLen;

    EVP_CIPHER_CTX_free(ctx);

    UInt8Array unwrappedKey = std::unique_ptr<uint8_t[]>(new uint8_t[len]);
    ::memset(unwrappedKey.get(), 0x00U, len);
    ::memcpy(unwrappedKey.get(), tempBuf, len);

    return unwrappedKey;
#else
    LogError(LOG_P25, "No OpenSSL, TEK encryption is not supported!");
    return nullptr;
#endif // ENABLE_SSL
}

/* Helper to generate a P25 KMM CBC MAC key with the given AES-256 KEK. */

UInt8Array P25Crypto::cryptAES_KMM_CBC_KDF(const uint8_t* kek, const uint8_t* msg, uint16_t msgLen)
{
#if defined(ENABLE_SSL)

    /*
    ** bryanb: some bizarre bullshit requiring a 8-byte IV -- thanks Ilya (https://github.com/ilyacodes) for helping look at this
    */

    uint8_t iv[AES::BLOCK_BYTES_LEN / 2] = {
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
    };

    uint16_t authLen = msgLen - KMM_AES_MAC_LENGTH;
    SET_UINT16(authLen, iv, 6U);

    int len;
    uint8_t tempBuf[TEMP_BUFFER_LEN];
    ::memset(tempBuf, 0x00U, TEMP_BUFFER_LEN);

    ERR_load_crypto_strings();

    EVP_CIPHER_CTX* ctx;

    // create and initialize a cipher context
    if (!(ctx = EVP_CIPHER_CTX_new())) {
        LogError(LOG_P25, "EVP_CIPHER_CTX_new(), failed to initialize cipher context: %s", ERR_error_string(ERR_get_error(), NULL));
        return nullptr;
    }

    // initialize the wrapper context with AES-256-WRAP
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_wrap(), NULL, kek, iv) != 1) {
        LogError(LOG_P25, "EVP_EncryptInit_ex(), failed to initialize cipher wrapping context: %s", ERR_error_string(ERR_get_error(), NULL));
        EVP_CIPHER_CTX_free(ctx);
        return nullptr;
    }

    // perform the wrapping operation
    if (EVP_EncryptUpdate(ctx, tempBuf, &len, kek, MAX_ENC_KEY_LENGTH_BYTES) != 1) {
        LogError(LOG_P25, "EVP_EncryptUpdate(), failed to wrap KEK: %s", ERR_error_string(ERR_get_error(), NULL));
        EVP_CIPHER_CTX_free(ctx);
        return nullptr;
    }

    // finalize the wrapping (no output, just padding)
    int tempLen;
    if (EVP_EncryptFinal_ex(ctx, tempBuf + len, &tempLen) != 1) {
        LogError(LOG_P25, "EVP_EncryptFinal_ex(), failed to finalize wrapping KEK: %s", ERR_error_string(ERR_get_error(), NULL));
        EVP_CIPHER_CTX_free(ctx);
        return nullptr;
    }
    len += tempLen;

    EVP_CIPHER_CTX_free(ctx);

    UInt8Array wrappedKey = std::unique_ptr<uint8_t[]>(new uint8_t[MAX_ENC_KEY_LENGTH_BYTES]);
    ::memset(wrappedKey.get(), 0x00U, MAX_ENC_KEY_LENGTH_BYTES);
    ::memcpy(wrappedKey.get(), tempBuf + 8U, MAX_ENC_KEY_LENGTH_BYTES);

    return wrappedKey;
#else
    LogError(LOG_P25, "No OpenSSL, CBC-MAC generation is not supported!");
    return nullptr;
#endif // ENABLE_SSL
}

/* Helper to generate a P25 KMM CBC-MAC with the given AES-256 CBC-MAC key. */

UInt8Array P25Crypto::cryptAES_KMM_CBC(const uint8_t* macKey, const uint8_t* msg, uint16_t msgLen)
{
    uint8_t iv[AES::BLOCK_BYTES_LEN] = {
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
    };

    AES aes = AES(AESKeyLength::AES_256);

    // pad the message as necessary
    size_t paddedLen = msgLen + (AES::BLOCK_BYTES_LEN - (msgLen % AES::BLOCK_BYTES_LEN));
    uint8_t paddedMessage[TEMP_BUFFER_LEN];
    ::memset(paddedMessage, 0x00U, TEMP_BUFFER_LEN);

    ::memcpy(paddedMessage, msg, msgLen - KMM_AES_MAC_LENGTH - 5U);
    ::memcpy(paddedMessage + msgLen - KMM_AES_MAC_LENGTH - 5U, msg + msgLen - 5U, 5U);

    // perform AES-CBC encryption
    uint8_t* tempBuf = aes.encryptCBC(paddedMessage, paddedLen, macKey, iv);

    UInt8Array wrappedKey = std::unique_ptr<uint8_t[]>(new uint8_t[8U]);
    ::memset(wrappedKey.get(), 0x00U, 8U);
    ::memcpy(wrappedKey.get(), tempBuf + (msgLen - AES::BLOCK_BYTES_LEN), 8U);

    delete[] tempBuf;
    return wrappedKey;
}

/* Helper to generate a P25 KMM CMAC MAC key with the given AES-256 KEK. */

UInt8Array P25Crypto::cryptAES_KMM_CMAC_KDF(const uint8_t* kek, const uint8_t* msg, uint16_t msgLen, bool hasMN)
{
#if defined(ENABLE_SSL)
    //                       O      T      A      R             M      A      C
    uint8_t label[8U] = { 0x4FU, 0x54U, 0x41U, 0x52U, 0x20U, 0x4DU, 0x41U, 0x43U };

    uint8_t context[12U];
    ::memset(context, 0x00U, 12U);

    uint8_t contextLen = 0U;
    if (hasMN) {
        ::memcpy(context, msg, 12U);
        contextLen = 12U;
    } else {
        ::memcpy(context, msg, 10U);
        contextLen = 10U;
    }

    /** DEBUG REMOVEME */
    Utils::dump(2U, "KEK", kek, MAX_ENC_KEY_LENGTH_BYTES);
    Utils::dump(2U, "Label", label, 8U);
    Utils::dump(2U, "Context", context, contextLen);
    /** DEBUG REMOVEME */

    size_t len;
    uint8_t tempBuf[TEMP_BUFFER_LEN];
    ::memset(tempBuf, 0x00U, TEMP_BUFFER_LEN);

    ERR_load_crypto_strings();

    // create a library context (required for OpenSSL 3.0+)
    OSSL_LIB_CTX* libCtx = OSSL_LIB_CTX_new();
    if (!libCtx) {
        LogError(LOG_P25, "OSSL_LIB_CTX_new(), failed to finalize OpenSSL: %s", ERR_error_string(ERR_get_error(), NULL));
        return nullptr;
    }

    // load the KDF algorithm
    EVP_KDF* kdf = EVP_KDF_fetch(NULL, "KBKDF", NULL);
    if (!kdf) {
        LogError(LOG_P25, "EVP_KDF_fetch(), failed to load OpenSSL KDF algorithm: %s", ERR_error_string(ERR_get_error(), NULL));
        OSSL_LIB_CTX_free(libCtx);
        return nullptr;
    }

    // create a context for the MAC operation
    EVP_KDF_CTX* ctx = EVP_KDF_CTX_new(kdf);
    if (!ctx) {
        LogError(LOG_P25, "EVP_KDF_CTX_new(), failed to create a OpenSSL KDF context: %s", ERR_error_string(ERR_get_error(), NULL));
        EVP_KDF_free(kdf);
        OSSL_LIB_CTX_free(libCtx);
        return nullptr;
    }

    // set the cipher to AES-256-CBC and initialize the MAC operation
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_MAC, "HMAC", 0),
        OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, "SHA-256", 0),
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, (void*)kek, MAX_ENC_KEY_LENGTH_BYTES),
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, (void*)label, 8U),
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, (void*)context, contextLen),
        OSSL_PARAM_END
    };

    // derive MAC key
    if (EVP_KDF_derive(ctx, tempBuf, MAX_ENC_KEY_LENGTH_BYTES, params) <= 0) {
        LogError(LOG_P25, "EVP_KDF_derive(), failed to derive MAC key: %s", ERR_error_string(ERR_get_error(), NULL));
        EVP_KDF_CTX_free(ctx);
        EVP_KDF_free(kdf);
        OSSL_LIB_CTX_free(libCtx);
        return nullptr;
    }

    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    OSSL_LIB_CTX_free(libCtx);

    /** DEBUG REMOVEME */
    Utils::dump(2U, "tempBuf", tempBuf, 128U);
    /** DEBUG REMOVEME */

    UInt8Array wrappedKey = std::unique_ptr<uint8_t[]>(new uint8_t[MAX_ENC_KEY_LENGTH_BYTES]);
    ::memset(wrappedKey.get(), 0x00U, MAX_ENC_KEY_LENGTH_BYTES);
    ::memcpy(wrappedKey.get(), tempBuf, MAX_ENC_KEY_LENGTH_BYTES);

    return wrappedKey;
#else
    LogError(LOG_P25, "No OpenSSL, CMAC generation is not supported!");
    return nullptr;
#endif // ENABLE_SSL
}

/* Helper to generate a P25 KMM CMAC with the given AES-256 CMAC key. */

UInt8Array P25Crypto::cryptAES_KMM_CMAC(const uint8_t* macKey, const uint8_t* msg, uint16_t msgLen)
{
#if defined(ENABLE_SSL)
    size_t len;
    uint8_t tempBuf[TEMP_BUFFER_LEN];
    ::memset(tempBuf, 0x00U, TEMP_BUFFER_LEN);

    uint8_t paddedMessage[TEMP_BUFFER_LEN];
    ::memset(paddedMessage, 0x00U, TEMP_BUFFER_LEN);

    ::memcpy(paddedMessage, msg, msgLen - KMM_AES_MAC_LENGTH - 5U);
    ::memcpy(paddedMessage + msgLen - KMM_AES_MAC_LENGTH - 5U, msg + msgLen - 5U, 5U);

    // create a library context (required for OpenSSL 3.0+)
    OSSL_LIB_CTX* libCtx = OSSL_LIB_CTX_new();
    if (!libCtx) {
        LogError(LOG_P25, "OSSL_LIB_CTX_new(), failed to finalize OpenSSL: %s", ERR_error_string(ERR_get_error(), NULL));
        return nullptr;
    }

    // fetch the CMAC implementation
    EVP_MAC* hmac = EVP_MAC_fetch(libCtx, "CMAC", NULL);
    if (!hmac) {
        LogError(LOG_P25, "EVP_MAC_fetch(), failed to fetch OpenSSL CMAC: %s", ERR_error_string(ERR_get_error(), NULL));
        OSSL_LIB_CTX_free(libCtx);
        return nullptr;
    }

    // create a context for the MAC operation
    EVP_MAC_CTX* ctx = EVP_MAC_CTX_new(hmac);
    if (!ctx) {
        LogError(LOG_P25, "EVP_MAC_CTX_new(), failed to create a OpenSSL CMAC context: %s", ERR_error_string(ERR_get_error(), NULL));
        EVP_MAC_free(hmac);
        OSSL_LIB_CTX_free(libCtx);
        return nullptr;
    }

    // set the cipher to AES-256-CBC and initialize the MAC operation
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_CIPHER, "AES-256-CBC", 0),
        OSSL_PARAM_END
    };

    // initialize the MAC operation
    if (!EVP_MAC_init(ctx, macKey, MAX_ENC_KEY_LENGTH_BYTES, params)) {
        LogError(LOG_P25, "EVP_MAC_init(), failed to initialize the AES-256-CBC MAC operation: %s", ERR_error_string(ERR_get_error(), NULL));
        EVP_MAC_CTX_free(ctx);
        EVP_MAC_free(hmac);
        OSSL_LIB_CTX_free(libCtx);
        return nullptr;
    }

    // provide the message data to be authenticated
    if (!EVP_MAC_update(ctx, paddedMessage, msgLen - KMM_AES_MAC_LENGTH)) {
        LogError(LOG_P25, "EVP_MAC_update(), failed to set message data to authenticate: %s", ERR_error_string(ERR_get_error(), NULL));
        EVP_MAC_CTX_free(ctx);
        EVP_MAC_free(hmac);
        OSSL_LIB_CTX_free(libCtx);
        return nullptr;
    }

    // get the length of the MAC
    if (!EVP_MAC_final(ctx, NULL, &len, 0)) {
        LogError(LOG_P25, "EVP_MAC_final(), failed to get MAC length: %s", ERR_error_string(ERR_get_error(), NULL));
        EVP_MAC_CTX_free(ctx);
        EVP_MAC_free(hmac);
        OSSL_LIB_CTX_free(libCtx);
        return nullptr;
    }

    // generate the MAC
    if (!EVP_MAC_final(ctx, tempBuf, &len, len)) {
        LogError(LOG_P25, "EVP_MAC_final(), failed to get MAC length: %s", ERR_error_string(ERR_get_error(), NULL));
        EVP_MAC_CTX_free(ctx);
        EVP_MAC_free(hmac);
        OSSL_LIB_CTX_free(libCtx);
        return nullptr;
    }

    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(hmac);
    OSSL_LIB_CTX_free(libCtx);

    UInt8Array wrappedKey = std::unique_ptr<uint8_t[]>(new uint8_t[len]);
    ::memset(wrappedKey.get(), 0x00U, len);
    ::memcpy(wrappedKey.get(), tempBuf, len);

    return wrappedKey;
#else
    LogError(LOG_P25, "No OpenSSL, CMAC generation is not supported!");
    return nullptr;
#endif // ENABLE_SSL
}

/* Helper to crypt a P25 PDU frame using AES-256. */

void P25Crypto::cryptAES_PDU(uint8_t* frame, uint8_t frameLen)
{
    if (m_keystream == nullptr)
        return;

    uint32_t offset = 16U;
    for (uint8_t i = 0U; i < frameLen; i++) {
        if (offset > 240U) {
            offset = 16U;
        }

        frame[i] ^= m_keystream[offset];
        offset++;
    }
}

/* Helper to crypt IMBE audio using DES. */

void P25Crypto::cryptDES_IMBE(uint8_t* imbe, DUID::E duid)
{
    if (m_keystream == nullptr)
        return;

    uint32_t offset = 8U;
    if (duid == DUID::LDU2) {
        offset += 101U;
    }

    offset += (m_keystreamPos * RAW_IMBE_LENGTH_BYTES) + RAW_IMBE_LENGTH_BYTES + ((m_keystreamPos < 8U) ? 0U : 2U);
    m_keystreamPos = (m_keystreamPos + 1U) % 9U;

    for (uint8_t i = 0U; i < RAW_IMBE_LENGTH_BYTES; i++) {
        imbe[i] ^= m_keystream[offset + i];
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
