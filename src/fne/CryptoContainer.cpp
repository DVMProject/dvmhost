// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/AESCrypto.h"
#include "common/Log.h"
#include "common/Timer.h"
#include "common/Utils.h"
#include "common/zlib/zlib.h"
#if defined(ENABLE_SSL)
#include "xml/rapidxml.h"
#endif // ENABLE_SSL
#include "CryptoContainer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(ENABLE_SSL)
#include <openssl/bio.h>
#include <openssl/evp.h>
#endif // ENABLE_SSL

using namespace crypto;

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define CHUNK 16384

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

#if defined(ENABLE_SSL)
/**
 * @brief Calculates the length of a decoded base64 string.
 * @param b64input String containing the base64 encoded data.
 * @returns int Length of buffer to contain base64 encoded data.
 */
int calcDecodeLength(const char* b64input)
{
    int len = strlen(b64input);
    int padding = 0;
  
    // last two chars are =
    if (b64input[len-1] == '=' && b64input[len-2] == '=') 
        padding = 2;
    else if (b64input[len-1] == '=') // last char is =
        padding = 1;
  
    return (int)len * 0.75 - padding;
}

/**
 * @brief Decodes a base64 encoded string.
 * @param b64message String containing the base64 encoded data.
 * @param buffer Buffer pointer to place encoded data.
 * @returns int 
 */
int base64Decode(char* b64message, uint8_t** buffer)
{
    int decodeLen = calcDecodeLength(b64message), len = 0;
    
    *buffer = (uint8_t*)malloc(decodeLen + 1);
    FILE* stream = ::fmemopen(b64message, ::strlen(b64message), "r");
  
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* bio = BIO_new_fp(stream, BIO_NOCLOSE);
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // do not use newlines to flush buffer
    len = BIO_read(bio, *buffer, ::strlen(b64message));

    // can test here if len == decodeLen - if not, then return an error
    (*buffer)[len] = '\0';
  
    BIO_free_all(bio);
    ::fclose(stream);

    return decodeLen;
}
#endif // ENABLE_SSL

/**
 * @brief 
 * @param buffer 
 * @param len 
 * @param target 
 * @return int 
 */
int findFirstChar(const uint8_t* buffer, uint32_t len, char target) 
{
    for (uint32_t i = 0U; i < len; i++) {
      if (buffer[i] == target) {
        return (int)i; 
      }
    }

    return -1;
}

/**
 * @brief 
 * @param buffer 
 * @param len 
 * @param target 
 * @return int 
 */
int findLastChar(const uint8_t* buffer, uint32_t len, char target) 
{
    if (buffer == nullptr) {
        return -1; 
    }

    int lastIndex = -1;
    for (uint32_t i = 0U; i < len; i++) {
        if (buffer[i] == target) {
            lastIndex = i;
        }
    }
    return lastIndex;
}

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::mutex CryptoContainer::m_mutex;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the CryptoContainer class. */

CryptoContainer::CryptoContainer(const std::string& filename, const std::string& password, uint32_t reloadTime, bool enabled) : Thread(),
    m_file(filename),
    m_password(password),
    m_reloadTime(reloadTime),
#if !defined(ENABLE_SSL)
    m_enabled(false),
#else
    m_enabled(enabled),
#endif // !ENABLE_SSL
    m_stop(false),
    m_keys()
{
    /* stub */
}

/* Finalizes a instance of the CryptoContainer class. */

CryptoContainer::~CryptoContainer() = default;

/* Thread entry point. This function is provided to run the thread for the lookup table. */

void CryptoContainer::entry()
{
    if (m_reloadTime == 0U) {
        return;
    }

    Timer timer(1U, 60U * m_reloadTime);
    timer.start();

    while (!m_stop) {
        sleep(1000U);

        timer.clock();
        if (timer.hasExpired()) {
            load();
            timer.start();
        }
    }
}

/* Stops and unloads this lookup table. */

void CryptoContainer::stop(bool noDestroy)
{
    if (!m_enabled)
        return;

    if (m_reloadTime == 0U) {
        if (!noDestroy)
            delete this;
        return;
    }

    m_stop = true;

    wait();
}

/* Reads the lookup table from the specified lookup table file. */

bool CryptoContainer::read()
{
    if (!m_enabled)
        return false;

    bool ret = load();

    if (m_reloadTime > 0U)
        run();
    setName("fne:crypto-lookup-tbl");

    return ret;
}

/* Clears all entries from the lookup table. */

void CryptoContainer::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_keys.clear();
}

/* Adds a new entry to the lookup table by the specified unique ID. */

void CryptoContainer::addEntry(KeyItem key)
{
    if (key.isInvalid())
        return;

    KeyItem entry = key;
    uint32_t id = entry.id();
    uint32_t kId = entry.kId();

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find_if(m_keys.begin(), m_keys.end(),
        [&](KeyItem x)
        {
            return x.id() == id && x.kId() == kId;
        });
    if (it != m_keys.end()) {
        m_keys[it - m_keys.begin()] = entry;
    }
    else {
        m_keys.push_back(entry);
    }
}

/* Erases an existing entry from the lookup table by the specified unique ID. */

void CryptoContainer::eraseEntry(uint32_t id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find_if(m_keys.begin(), m_keys.end(), [&](KeyItem x) { return x.id() == id; });
    if (it != m_keys.end()) {
        m_keys.erase(it);
    }
}

/* Finds a table entry in this lookup table. */

KeyItem CryptoContainer::find(uint32_t kId)
{
    KeyItem entry;

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find_if(m_keys.begin(), m_keys.end(),
        [&](KeyItem x)
        {
            return x.kId() == kId;
        });
    if (it != m_keys.end()) {
        entry = *it;
    } else {
        entry = KeyItem();
    }

    return entry;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Loads the table from the passed lookup table file. */

bool CryptoContainer::load()
{
#if !defined(ENABLE_SSL)
    return false;
#else
    if (!m_enabled) {
        return false;
    }
    if (m_file.length() <= 0) {
        return false;
    }
    if (m_password.length() <= 0) {
        return false;
    }

    FILE* ekcFile = ::fopen(m_file.c_str(), "rb");
    if (ekcFile == nullptr) {
        LogError(LOG_HOST, "Cannot open the crypto container file - %s", m_file.c_str());
        return false;
    }

    // inflate file
    // compression structures
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    // set input data
    strm.avail_in = 0U;
    strm.next_in = Z_NULL;

    // initialize decompression
    int ret = inflateInit2(&strm, 16 + MAX_WBITS);
    if (ret != Z_OK) {
        LogError(LOG_HOST, "Error initializing ZLIB, ret = %d", ret);
        ::fclose(ekcFile);
        return false;
    }

    // skip 4 bytes (C# adds a header on the GZIP stream for the decompressed length)
    ::fseek(ekcFile, 4, SEEK_SET);

    // decompress data
    std::vector<uint8_t> decompressedData;
    uint8_t inBuffer[CHUNK];
    uint8_t outBuffer[CHUNK];
    do {
        strm.avail_in = fread(inBuffer, 1, CHUNK, ekcFile);
        if (::ferror(ekcFile)) {
            inflateEnd(&strm);
            ::fclose(ekcFile);
            return false;
        }

        if (strm.avail_in == 0)
            break;
        strm.next_in = inBuffer;

        uint32_t have = 0U;
        do {
            strm.avail_out = CHUNK;
            strm.next_out = outBuffer;

            ret = inflate(&strm, Z_NO_FLUSH);
            if (ret == Z_DATA_ERROR) {
                // deflate stream invalid
                LogError(LOG_HOST, "Error decompressing EKC: %s", (strm.msg == NULL) ? "compressed data error" : strm.msg);
                inflateEnd(&strm);
                ::fclose(ekcFile);
                return false;
            }

            if (ret == Z_STREAM_ERROR || ret < 0) {
                LogError(LOG_HOST, "Error decompressing EKC, ret = %d", ret);
                inflateEnd(&strm);
                ::fclose(ekcFile);
                return false;
            }

            have = CHUNK - strm.avail_out;
            decompressedData.insert(decompressedData.end(), outBuffer, outBuffer + have);
        } while (strm.avail_out == 0);
    } while (ret != Z_STREAM_END);

    // cleanup
    inflateEnd(&strm);
    ::fclose(ekcFile);

    try {
        // ensure zero termination
        decompressedData.push_back(0U);

        uint8_t* decompressed = decompressedData.data();

        // parse outer container DOM
        enum { PARSE_FLAGS = rapidxml::parse_full };
        rapidxml::xml_document<> ekcOuterContainer;
        ekcOuterContainer.parse<PARSE_FLAGS>(reinterpret_cast<char*>(decompressed));

        rapidxml::xml_node<>* outerRoot = ekcOuterContainer.first_node("OuterContainer");
        if (outerRoot != nullptr) {
            // get EKC version
            std::string version = "";
            rapidxml::xml_attribute<>* versionAttr = outerRoot->first_attribute("version");
            if (versionAttr != nullptr)
                version = std::string(versionAttr->value());

            // validate EKC version is set and is 1.0
            if (version == "") {
                ::LogError(LOG_HOST, "Error opening EKC: incorrect version, expected 1.0 got none");
                return false;
            }

            if (version != "1.0") {
                ::LogError(LOG_HOST, "Error opening EKC: incorrect version, expected 1.0 got %s", version.c_str());
                return false;
            }

            // get key derivation node
            rapidxml::xml_node<>* keyDerivation = outerRoot->first_node("KeyDerivation");
            if (keyDerivation == nullptr) {
                ::LogError(LOG_HOST, "Error opening EKC: failed to process XML");
                return false;
            }
            // retreive and parse salt
            uint8_t* salt = nullptr;
            rapidxml::xml_node<>* saltNode = keyDerivation->first_node("Salt");
            if (saltNode == nullptr) {
                ::LogError(LOG_HOST, "Error opening EKC: failed to process XML");
                return false;
            }
            int8_t saltBufLen = base64Decode(saltNode->value(), &salt);

            // retrieve interation count
            int32_t iterationCount = 0;
            rapidxml::xml_node<>* iterNode = keyDerivation->first_node("IterationCount");
            if (iterNode == nullptr) {
                ::LogError(LOG_HOST, "Error opening EKC: failed to process XML");
                return false;
            }
            iterationCount = ::strtoul(iterNode->value(), NULL, 10);
            
            // retrieve key length
            int32_t keyLength = 0;
            rapidxml::xml_node<>* keyLenNode = keyDerivation->first_node("KeyLength");
            if (keyLenNode == nullptr) {
                ::LogError(LOG_HOST, "Error opening EKC: failed to process XML");
                return false;
            }
            keyLength = ::strtoul(keyLenNode->value(), NULL, 10);

            // generate crypto key to decrypt inner container
            uint8_t key[EVP_MAX_KEY_LENGTH];
            ::memset(key, 0x00U, EVP_MAX_KEY_LENGTH);
            uint8_t iv[EVP_MAX_IV_LENGTH];
            ::memset(iv, 0x00U, EVP_MAX_IV_LENGTH);

            uint8_t keyIv[EVP_MAX_KEY_LENGTH + EVP_MAX_IV_LENGTH];
            ::memset(keyIv, 0x00U, EVP_MAX_KEY_LENGTH + EVP_MAX_IV_LENGTH);
            if (PKCS5_PBKDF2_HMAC(m_password.c_str(), m_password.size(), salt, saltBufLen, iterationCount, EVP_sha512(), keyLength + EVP_MAX_IV_LENGTH, keyIv)) {
                ::memcpy(key, keyIv, keyLength);
                ::memcpy(iv, keyIv + keyLength, EVP_MAX_IV_LENGTH);
            }

            // get inner container encrypted data
            // bryanb: annoying levels of XML encapsulation...
            rapidxml::xml_node<>* encryptedDataNode = outerRoot->first_node("EncryptedData");
            if (encryptedDataNode == nullptr) {
                ::LogError(LOG_HOST, "Error opening EKC: failed to process XML");
                return false;
            }
            rapidxml::xml_node<>* cipherDataNode = encryptedDataNode->first_node("CipherData");
            if (cipherDataNode == nullptr) {
                ::LogError(LOG_HOST, "Error opening EKC: failed to process XML");
                return false;
            }
            rapidxml::xml_node<>* cipherValue = cipherDataNode->first_node("CipherValue");
            if (cipherValue == nullptr) {
                ::LogError(LOG_HOST, "Error opening EKC: failed to process XML");
                return false;
            }

            uint8_t* innerContainerCrypted = nullptr;
            int innerContainerLen = base64Decode(cipherValue->value(), &innerContainerCrypted);

            // decrypt inner container
            AES aes = AES(AESKeyLength::AES_256);
            uint8_t* innerContainer = aes.decryptCBC(innerContainerCrypted, innerContainerLen, key, iv);

            /*
            ** bryanb: this is probably slightly error prone...
            */

            int xmlFirstTagChar = findFirstChar(innerContainer, innerContainerLen, '<');
            int xmlLastTagChar = findLastChar(innerContainer, innerContainerLen, '>');
            // zero all bytes after the last > character
            ::memset(innerContainer + xmlLastTagChar + 1U, 0x00U, innerContainerLen - xmlLastTagChar);

            rapidxml::xml_document<> ekcInnerContainer;
            ekcInnerContainer.parse<PARSE_FLAGS>(reinterpret_cast<char*>(innerContainer + xmlFirstTagChar));

            rapidxml::xml_node<>* innerRoot = ekcInnerContainer.first_node("InnerContainer");
            if (innerRoot != nullptr) {
                // clear table
                clear();

                std::lock_guard<std::mutex> lock(m_mutex);

                // get keys node
                rapidxml::xml_node<>* keys = innerRoot->first_node("Keys");
                if (keys != nullptr) {
                    uint32_t i = 0U;
                    for (rapidxml::xml_node<>* keyNode = keys->first_node("KeyItem"); keyNode; keyNode = keyNode->next_sibling()) {
                        KeyItem key = KeyItem();
                        key.id(i);

                        // get name
                        rapidxml::xml_node<>* nameNode = keyNode->first_node("Name");
                        if (nameNode == nullptr) {
                            continue;
                        }
                        key.name(nameNode->value());

                        // get keyset ID
                        rapidxml::xml_node<>* keysetIdNode = keyNode->first_node("KeysetId");
                        if (keysetIdNode == nullptr) {
                            continue;
                        }
                        key.keysetId(::strtoul(keysetIdNode->value(), NULL, 10));

                        // get SLN
                        rapidxml::xml_node<>* slnNode = keyNode->first_node("Sln");
                        if (slnNode == nullptr) {
                            continue;
                        }
                        key.sln(::strtoul(slnNode->value(), NULL, 10));

                        // get algorithm ID
                        rapidxml::xml_node<>* algIdNode = keyNode->first_node("AlgorithmId");
                        if (algIdNode == nullptr) {
                            continue;
                        }
                        key.algId(::strtoul(algIdNode->value(), NULL, 10));

                        // get key ID
                        rapidxml::xml_node<>* kIdNode = keyNode->first_node("KeyId");
                        if (kIdNode == nullptr) {
                            continue;
                        }
                        key.kId(::strtoul(kIdNode->value(), NULL, 10));

                        // get key material
                        rapidxml::xml_node<>* keyMatNode = keyNode->first_node("Key");
                        if (keyMatNode == nullptr) {
                            continue;
                        }
                        key.keyMaterial(keyMatNode->value());

                        ::LogInfoEx(LOG_HOST, "Key NAME: %s SLN: %u ALGID: $%02X, KID: $%04X", key.name().c_str(), key.sln(), key.algId(), key.kId());

                        m_keys.push_back(key);
                        i++;
                    }
                }
            }
        }
    } catch(const std::exception& e) {
        ::LogError(LOG_HOST, "Error opening EKC: %s", e.what());
        return false;
    }

    if (m_keys.size() == 0U) {
        ::LogError(LOG_HOST, "No encryption keys defined!");
        return false;
    }

    size_t size = m_keys.size();
    if (size == 0U)
        return false;

    LogInfoEx(LOG_HOST, "Loaded %lu entries into crypto lookup table", size);

    return true;
#endif // !ENABLE_SSL
}
