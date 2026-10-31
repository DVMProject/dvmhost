// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2022,2024,2025-2026 Bryan Biedenkapp, N2PLL
 *  Copyright (c) 2024 Patrick McDonnell, W3AXL
 *
 */
/**
 * @defgroup lookups_rid Radio ID Lookups
 * @brief Implementation for radio ID lookup tables.
 * @ingroup lookups
 * 
 * @file RadioIdLookup.h
 * @ingroup lookups_rid
 * @file RadioIdLookup.cpp
 * @ingroup lookups_rid
 */
#if !defined(__RADIO_ID_LOOKUP_H__)
#define __RADIO_ID_LOOKUP_H__

#include "common/Defines.h"
#include "common/lookups/LookupTable.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace lookups
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents an individual entry in the radio ID table.
     * @ingroup lookups_rid
     */
    class DVM_COMMON_API RadioId {
    public:
        /**
         * @brief Initializes a new instance of the RadioId class.
         */
        RadioId() :
            m_radioEnabled(false),
            m_radioDefault(false),
            m_radioAlias(""),
            m_radioIPAddress(""),
            m_canRequestKeys(false),
            m_canRekey(false),
            m_allowedKIds()
        {
            /* stub */
        }

        /**
         * @brief Initializes a new instance of the RadioId class.
         * @param radioEnabled Flag indicating radio is enabled.
         * @param radioDefault Flag indicating this is a "default" (i.e. undefined) radio.
         */
        RadioId(bool radioEnabled, bool radioDefault) :
            m_radioEnabled(radioEnabled),
            m_radioDefault(radioDefault),
            m_radioAlias(""),
            m_radioIPAddress(""),
            m_canRequestKeys(false),
            m_canRekey(false),
            m_allowedKIds()
        {
            /* stub */
        }

        /**
         * @brief Initializes a new instance of the RadioId class.
         * @param radioEnabled Flag indicating radio is enabled.
         * @param radioDefault Flag indicating this is a "default" (i.e. undefined) radio.
         * @param radioAlias Textual alias for the radio.
         * @param ipAddress Textual IP Address for the radio.
         */
        RadioId(bool radioEnabled, bool radioDefault, const std::string& radioAlias, const std::string& ipAddress = "",
            bool canRequestKeys = false, bool canRekey = false, const std::vector<uint16_t>& allowedKIds = std::vector<uint16_t>()) :
            m_radioEnabled(radioEnabled),
            m_radioDefault(radioDefault),
            m_radioAlias(radioAlias),
            m_radioIPAddress(ipAddress),
            m_canRequestKeys(canRequestKeys),
            m_canRekey(canRekey),
            m_allowedKIds(allowedKIds)
        {
            /* stub */
        }

        /**
         * @brief Equals operator. Copies this RadioId to another RadioId.
         * @param data Instance of RadioId to copy.
         */
        RadioId& operator=(const RadioId& data)
        {
            if (this != &data) {
                m_radioEnabled = data.m_radioEnabled;
                m_radioDefault = data.m_radioDefault;
                m_radioAlias = data.m_radioAlias;
                m_radioIPAddress = data.m_radioIPAddress;
                m_canRequestKeys = data.m_canRequestKeys;
                m_canRekey = data.m_canRekey;
                m_allowedKIds = data.m_allowedKIds;
            }

            return *this;
        }

        /**
         * @brief Sets flag values.
         * @param radioEnabled Flag indicating radio is enabled.
         * @param radioDefault Flag indicating this is a "default" (i.e. undefined) radio.
         * @param radioAlias Textual alias for the radio.
         * @param ipAddress Textual IP Address for the radio.
         */
        void set(bool radioEnabled, bool radioDefault, const std::string& radioAlias, const std::string& ipAddress = "",
            bool canRequestKeys = false, bool canRekey = false, const std::vector<uint16_t>& allowedKIds = std::vector<uint16_t>())
        {
            m_radioEnabled = radioEnabled;
            m_radioDefault = radioDefault;
            m_radioAlias = radioAlias;
            m_radioIPAddress = ipAddress;
            m_canRequestKeys = canRequestKeys;
            m_canRekey = canRekey;
            m_allowedKIds = allowedKIds;
        }

    public:
        /**
         * @brief Flag indicating if the radio is enabled.
         */
        DECLARE_RO_PROPERTY_PLAIN(bool, radioEnabled);
        /**
         * @brief Flag indicating if the radio is default.
         */
        DECLARE_RO_PROPERTY_PLAIN(bool, radioDefault);
        /**
         * @brief Alias for the radio.
         */
        DECLARE_RO_PROPERTY_PLAIN(std::string, radioAlias);
        /**
         * @brief IP Address for the radio.
         */
        DECLARE_RO_PROPERTY_PLAIN(std::string, radioIPAddress);
        /**
         * @brief Flag indicating if the radio can request keys from FNE.
         */
        DECLARE_RO_PROPERTY_PLAIN(bool, canRequestKeys);
        /**
         * @brief Flag indicating if the radio can receive OTAR rekey command payloads.
         */
        DECLARE_RO_PROPERTY_PLAIN(bool, canRekey);
        /**
         * @brief Allowed encryption key IDs for this radio.
         */
        DECLARE_RO_PROPERTY_PLAIN(std::vector<uint16_t>, allowedKIds);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements a threading lookup table class that contains a radio ID
     *  lookup table.
     * @ingroup lookups_rid
     */
    class DVM_COMMON_API RadioIdLookup : public LookupTable<RadioId> {
    public:
        /**
         * @brief Initializes a new instance of the RadioIdLookup class.
         * @param filename Full-path to the radio ID table file.
         * @param reloadTime Interval of time to reload the radio ID table.
         * @param ridAcl Flag indicating whether radio ID access control is enabled.
         * @param verbose Flag indicating if logging should be enabled for this lookup table.
         */
        RadioIdLookup(const std::string& filename, uint32_t reloadTime, bool ridAcl, bool verbose = false);

        /**
         * @brief Clears all entries from the lookup table.
         */
        void clear() override;

        /**
         * @brief Toggles the specified radio ID enabled or disabled.
         * @param id Unique ID to toggle.
         * @param enabled Flag indicating if radio ID is enabled or not.
         */
        void toggleEntry(uint32_t id, bool enabled);

        /**
         * @brief Adds a new entry to the lookup table by the specified unique ID, with an alias.
         * @param id Unique ID to add.
         * @param enabled Flag indicating if radio ID is enabled or not.
         * @param alias Alias for the radio ID
         * @param ipAddress IP Address for Radio
         */
        void addEntry(uint32_t id, bool enabled, const std::string& alias, const std::string& ipAddress = "",
            bool canRequestKeys = false, bool canRekey = false, const std::vector<uint16_t>& allowedKIds = std::vector<uint16_t>());
        /**
         * @brief Erases an existing entry from the lookup table by the specified unique ID.
         * @param id Unique ID to erase.
         */
        void eraseEntry(uint32_t id);

        /**
         * @brief Helper to return the lookup table.
         * @returns std::unordered_map<uint32_t, RadioId> Table.
         */
        std::unordered_map<uint32_t, RadioId> table() override
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            return m_table;
        }

        /**
         * @brief Finds a table entry in this lookup table.
         * @param id Unique identifier for table entry.
         * @returns RadioId Table entry.
         */
        RadioId find(uint32_t id) override;

        /**
         * @brief Saves loaded radio ID lookups.
         * @param quiet Disable logging during save operation.
         */
        void commit(bool quiet = false);

        /**
         * @brief Flag indicating whether radio ID access control is enabled or not.
         */
        bool getACL();

    protected:
        bool m_acl;

        bool m_verbose;

        /**
         * @brief Loads the table from the passed lookup table file.
         * @return True, if lookup table was loaded, otherwise false.
         */
        bool load() override;

        /**
         * @brief Saves the table to the passed lookup table file.
         * @param quiet Disable logging during save operation.
         * @return True, if lookup table was saved, otherwise false.
         */
        bool save(bool quiet = false) override;

    private:
        static std::mutex s_mutex;  //!< Mutex used for change locking.
        static bool s_locked;       //!< Flag used for read locking (prevents find lookups), should be used when atomic operations (add/erase/etc) are being used.

        /**
         * @brief Parses a string of KIDs from the lookup file into a vector of uint16_t KIDs.
         * @param input String representation of KID list from the lookup file.
         * @returns Vector of KIDs parsed from the input string.
         */
        std::vector<uint16_t> parseKIdList(const std::string& input);
        /**
         * @brief Serializes a list of KIDs into a string for storage in the lookup file.
         * @param kids List of KIDs to serialize.
         * @returns String representation of the KID list.
         */
        std::string serializeKIdList(const std::vector<uint16_t>& kids);
    };
} // namespace lookups

#endif // __RADIO_ID_LOOKUP_H__
