// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file RadioAliasLookup.h
 * @ingroup lookups_rid
 * @file RadioAliasLookup.cpp
 * @ingroup lookups_rid
 */
#if !defined(__RADIO_ALIAS_LOOKUP_H__)
#define __RADIO_ALIAS_LOOKUP_H__

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
     * @brief Implements a threading lookup table class that contains a radio alias
     *  lookup table.
     * @ingroup lookups_rid
     */
    class DVM_COMMON_API RadioAliasLookup : public LookupTable<std::string> {
    public:
        /**
         * @brief Initializes a new instance of the RadioAliasLookup class.
         * @param filename Full-path to the radio ID table file.
         * @param reloadTime Interval of time to reload the radio ID table.
         * @param verbose Flag indicating if logging should be enabled for this lookup table.
         */
        RadioAliasLookup(const std::string& filename, uint32_t reloadTime, bool verbose = false);

        /**
         * @brief Clears all entries from the lookup table.
         */
        void clear() override;

        /**
         * @brief Adds a new entry to the lookup table by the specified unique ID, with an alias.
         * @param id Unique ID to add.
         * @param alias Alias for the radio ID
         */
        void addEntry(uint32_t id, const std::string& alias);
        /**
         * @brief Erases an existing entry from the lookup table by the specified unique ID.
         * @param id Unique ID to erase.
         */
        void eraseEntry(uint32_t id);

        /**
         * @brief Helper to return the lookup table.
         * @returns std::unordered_map<uint32_t, std::string> Table.
         */
        std::unordered_map<uint32_t, std::string> table() override
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            return m_table;
        }

        /**
         * @brief Finds a table entry in this lookup table.
         * @param id Unique identifier for table entry.
         * @returns RadioAlias Table entry.
         */
        std::string find(uint32_t id) override;

        /**
         * @brief Saves loaded radio alias lookups.
         * @param quiet Disable logging during save operation.
         */
        void commit(bool quiet = false);

    protected:
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
    };
} // namespace lookups

#endif // __RADIO_ALIAS_LOOKUP_H__
