// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file CryptoContainer.h
 * @ingroup fne
 * @file CryptoContainer.cpp
 * @ingroup fne
 */
#if !defined(__CRYPTO_CONTAINER_H__)
#define __CRYPTO_CONTAINER_H__

#include "common/Defines.h"
#include "common/Thread.h"
#include "common/Utils.h"

#include <cassert>
#include <string>
#include <mutex>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Represents an key item.
 * @ingroup fne
 */
class HOST_SW_API KeyItem {
public:
    /**
     * @brief Initializes a new instance of the KeyItem class.
     */
    KeyItem() :
        m_id(0U),
        m_name(),
        m_keysetId(0U),
        m_sln(0U),
        m_algId(0U),
        m_kId(0U),
        m_keyMaterial()
    {
        /* stub */
    }

    /**
     * @brief Equals operator. Copies this KeyItem to another KeyItem.
     * @param data Instance of KeyItem to copy.
     */
    virtual KeyItem& operator= (const KeyItem& data)
    {
        if (this != &data) {
            m_id = data.m_id;
            m_name = data.m_name;
            m_keysetId = data.m_keysetId;
            m_sln = data.m_sln;
            m_algId = data.m_algId;
            m_kId = data.m_kId;

            m_keyMaterial = data.m_keyMaterial;
        }

        return *this;
    }

    /**
     * @brief Helper to quickly determine if a key item entry is valid.
     * @returns bool True, if key item entry is valid, otherwise false.
     */
    bool isInvalid() const
    {
        if (m_sln == 0U)
            return true;
        if (m_algId == 0U)
            return true;
        if (m_kId == 0U)
            return true;
        if (m_keyMaterial.size() == 0U)
            return true;
        return false;
    }

    /**
     * @brief Gets the encryption key.
     * @param[out] key Buffer containing the key.
     * @returns uint8_t Length of encryption key.
     */
    uint8_t getKey(uint8_t* key) const
    {
        assert(key != nullptr);

        const char* rawKey = m_keyMaterial.c_str();
        ::memset(key, 0x00U, 32U);

        uint8_t len = 32U, charPos = 0U;
        for (uint8_t i = 0U; i < 32U; i++) {
            char t[4] = {rawKey[0], rawKey[1], 0};

            key[i] = (uint8_t)::strtoul(t, NULL, 16);

            if (charPos + 2U > m_keyMaterial.size()) {
                len = i;
                break;
            }

            rawKey += 2 * sizeof(char);
            charPos += 2U;
        }

        return len;
    }

public:
    /**
     * @brief 
     */
    DECLARE_PROPERTY_PLAIN(uint32_t, id);
    /**
     * @brief 
     */
    DECLARE_PROPERTY_PLAIN(std::string, name);

    /**
     * @brief 
     */
    DECLARE_PROPERTY_PLAIN(uint32_t, keysetId);
    /**
     * @brief 
     */
    DECLARE_PROPERTY_PLAIN(uint32_t, sln);

    /**
     * @brief Encryption algorithm ID.
     */
    DECLARE_PROPERTY_PLAIN(uint8_t, algId);
    /**
     * @brief Encryption key ID.
     */
    DECLARE_PROPERTY_PLAIN(uint32_t, kId);

    /**
     * @brief Encryption key material.
     */
    DECLARE_PROPERTY_PLAIN(std::string, keyMaterial);
};

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Implements a threading lookup table class that contains routing
 *  rules information.
 * @ingroup lookups_tgid
 */
class HOST_SW_API CryptoContainer : public Thread {
public:
    /**
     * @brief Initializes a new instance of the CryptoContainer class.
     * @param filename Full-path to the crypto container file.
     * @param password Crypto container file access password.
     * @param reloadTime Interval of time to reload the crypto container.
     * @param enabled Flag indicating if crypto container is enabled.
     */
    CryptoContainer(const std::string& filename, const std::string& password, uint32_t reloadTime, bool enabled);
    /**
     * @brief Finalizes a instance of the CryptoContainer class.
     */
    ~CryptoContainer() override;

    /**
     * @brief Thread entry point. This function is provided to run the thread
     *  for the lookup table.
     */
    void entry() override;

    /**
     * @brief Stops and unloads this lookup table.
     *  (NOTE: If the reload time for this lookup table is set to 0, a call to stop will also delete the object.)
     * @param noDestroy Flag indicating the lookup table should remain resident in memory after stopping.
     */
    void stop(bool noDestroy = false);
    /**
     * @brief Reads the lookup table from the specified lookup table file.
     * @returns bool True, if lookup table was read, otherwise false.
     */
    bool read();
    /**
     * @brief Reads the lookup table from the specified lookup table file.
     * @returns bool True, if lookup table was read, otherwise false.
     */
    bool reload() { return load(); }
    /**
     * @brief Clears all entries from the lookup table.
     */
    void clear();

    /**
     * @brief Adds a new entry to the lookup table.
     * @param key Key Item.
     */
    void addEntry(KeyItem key);
    /**
     * @brief Erases an existing entry from the lookup table by the specified unique ID.
     * @param id Unique ID to erase.
     */
    void eraseEntry(uint32_t id);
    /**
     * @brief Finds a table entry in this lookup table.
     * @param kId Unique identifier for table entry.
     * @returns KeyItem Table entry.
     */
    virtual KeyItem find(uint32_t kId);

    /**
     * @brief Helper to return the flag indicating whether or not the crypto container is enabled.
     * @returns bool 
     */
    bool isEnabled() const { return m_enabled; }

    /**
     * @brief Helper to set the reload time of this lookup table.
     * @param reloadTime Lookup time in seconds.
     */
    void setReloadTime(uint32_t reloadTime) { m_reloadTime = reloadTime; }

private:
    std::string m_file;
    std::string m_password;
    uint32_t m_reloadTime;

    bool m_enabled;
    bool m_stop;

    static std::mutex m_mutex;

    /**
     * @brief Loads the table from the passed lookup table file.
     * @return True, if lookup table was loaded, otherwise false.
     */
    bool load();

public:
    /**
     * @brief List of keys.
     */
    DECLARE_PROPERTY_PLAIN(std::vector<KeyItem>, keys);
};

#endif // __CRYPTO_CONTAINER_H__
