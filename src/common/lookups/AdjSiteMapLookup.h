// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup lookups_asm Adjacent Site Map Lookups
 * @brief Implementation for adjacent site map lookup tables.
 * @ingroup lookups
 * 
 * @file AdjSiteMapLookup.h
 * @ingroup lookups_asm
 * @file AdjSiteMapLookup.cpp
 * @ingroup lookups_asm
 */
#if !defined(__ADJ_SITE_MAP_LOOKUP_H__)
#define __ADJ_SITE_MAP_LOOKUP_H__

#include "common/Defines.h"
#include "common/lookups/LookupTable.h"
#include "common/yaml/Yaml.h"
#include "common/Utils.h"

#include <string>
#include <mutex>
#include <vector>

namespace lookups
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents an adjacent peer map entry.
     * @ingroup lookups_asm
     */
    class HOST_SW_API AdjPeerMapEntry {
    public:
        /**
         * @brief Initializes a new instance of the AdjPeerMapEntry class.
         */
        AdjPeerMapEntry() :
            m_active(false),
            m_peerId(0U),
            m_neighbors()
        {
            /* stub */
        }
        /**
         * @brief Initializes a new instance of the AdjPeerMapEntry class.
         * @param node YAML node for this configuration block.
         */
        AdjPeerMapEntry(yaml::Node& node) :
            AdjPeerMapEntry()
        {
            m_active = node["active"].as<bool>(false);
            m_peerId = node["peer_id"].as<uint32_t>(0U);

            yaml::Node& neighborList = node["neighbors"];
            if (neighborList.size() > 0U) {
                for (size_t i = 0; i < neighborList.size(); i++) {
                    uint32_t peerId = neighborList[i].as<uint32_t>(0U);
                    m_neighbors.push_back(peerId);
                }
            }
        }

        /**
         * @brief Equals operator. Copies this AdjPeerMapEntry to another AdjPeerMapEntry.
         * @param data Instance of AdjPeerMapEntry to copy.
         */
        virtual AdjPeerMapEntry& operator= (const AdjPeerMapEntry& data)
        {
            if (this != &data) {
                m_active = data.m_active;
                m_peerId = data.m_peerId;
                m_neighbors = data.m_neighbors;
            }

            return *this;
        }

        /**
         * @brief Gets the count of neighbors.
         * @returns uint8_t Total count of peer neighbors.
         */
        uint8_t neighborSize() const { return m_neighbors.size(); }

        /**
         * @brief Helper to quickly determine if a entry is valid.
         * @returns bool True, if entry is valid, otherwise false.
         */
        bool isEmpty() const
        {
            if (m_neighbors.size() > 0U)
                return true;
            return false;
        }

        /**
         * @brief Return the YAML structure for this TalkgroupRuleConfig.
         * @param[out] node YAML node.
         */
        void getYaml(yaml::Node &node)
        {
            // We have to convert the bools back to strings to pass to the yaml node
            node["active"] = __BOOL_STR(m_active);
            node["peerId"] = __INT_STR(m_peerId);
            
            // Get the lists
            yaml::Node neighborList;
            if (m_neighbors.size() > 0U) {
                for (auto neighbor : m_neighbors) {
                    yaml::Node& newNeighbor = neighborList.push_back();
                    newNeighbor = __INT_STR(neighbor);
                }
            }
        }

    public:
        /**
         * @brief Flag indicating whether the rule is active.
         */
        DECLARE_PROPERTY_PLAIN(bool, active);
        /**
         * @brief Peer ID.
         */
        DECLARE_PROPERTY_PLAIN(uint32_t, peerId);

        /**
         * @brief List of neighbor peers.
         */
        DECLARE_PROPERTY_PLAIN(std::vector<uint32_t>, neighbors);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements a threading lookup table class that contains adjacent site map 
     *  information.
     * @ingroup lookups_asm
     */
    class HOST_SW_API AdjSiteMapLookup : public Thread {
    public:
        /**
         * @brief Initializes a new instance of the AdjSiteMapLookup class.
         * @param filename Full-path to the routing rules file.
         * @param reloadTime Interval of time to reload the routing rules.
         */
        AdjSiteMapLookup(const std::string& filename, uint32_t reloadTime);
        /**
         * @brief Finalizes a instance of the AdjSiteMapLookup class.
         */
        ~AdjSiteMapLookup() override;

        /**
         * @brief Thread entry point. This function is provided to run the thread
         *  for the lookup table.
         */
        void entry() override;

        /**
         * @brief Stops and unloads this lookup table.
         * @param noDestroy Flag indicating the lookup table should remain resident in memory after stopping.
         */
        void stop(bool noDestroy = false);
        /**
         * @brief Reads the lookup table from the specified lookup table file.
         *  (NOTE: If the reload time for this lookup table is set to 0, a call to stop will also delete the object.)
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
         * @param entry Peer map entry.
         */
        void addEntry(AdjPeerMapEntry entry);
        /**
         * @brief Erases an existing entry from the lookup table by the specified unique ID.
         * @param id Unique ID to erase.
         */
        void eraseEntry(uint32_t id);
        /**
         * @brief Finds a table entry in this lookup table.
         * @param id Unique identifier for table entry.
         * @returns AdjPeerMapEntry Table entry.
         */
        virtual AdjPeerMapEntry find(uint32_t id);

        /**
         * @brief Saves loaded talkgroup rules.
         */
        bool commit();

        /**
         * @brief Returns the filename used to load this lookup table.
         * @return std::string Full-path to the lookup table file.
         */
        const std::string filename() { return m_rulesFile; };
        /**
         * @brief Sets the filename used to load this lookup table.
         * @param filename Full-path to the routing rules file.
         */
        void filename(std::string filename) { m_rulesFile = filename; };

        /**
         * @brief Helper to set the reload time of this lookup table.
         * @param reloadTime Lookup time in seconds.
         */
        void setReloadTime(uint32_t reloadTime) { m_reloadTime = reloadTime; }

    private:
        std::string m_rulesFile;
        uint32_t m_reloadTime;
        yaml::Node m_rules;

        bool m_stop;

        static std::mutex m_mutex;  //! Mutex used for change locking.
        static bool m_locked;       //! Flag used for read locking (prevents find lookups), should be used when atomic operations (add/erase/etc) are being used.

        /**
         * @brief Loads the table from the passed lookup table file.
         * @return True, if lookup table was loaded, otherwise false.
         */
        bool load();
        /**
         * @brief Saves the table to the passed lookup table file.
         * @return True, if lookup table was saved, otherwise false.
         */
        bool save();

    public:
        /**
         * @brief List of adjacent site map entries.
         */
        DECLARE_PROPERTY_PLAIN(std::vector<AdjPeerMapEntry>, adjPeerMap);
    };
} // namespace lookups

#endif // __ADJ_SITE_MAP_LOOKUP_H__
