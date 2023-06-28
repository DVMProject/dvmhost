/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2023 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#if !defined(__TALKGROUP_RULES_LOOKUP_H__)
#define __TALKGROUP_RULES_LOOKUP_H__

#include "Defines.h"
#include "lookups/LookupTable.h"
#include "Thread.h"
#include "yaml/Yaml.h"

#include <string>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace lookups
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //     Represents an source block for a routing rule.
    // ---------------------------------------------------------------------------

    class HOST_SW_API TalkgroupRuleGroupVoiceSource {
    public:
        /// <summary>Initializes a new insatnce of the TalkgroupRuleGroupVoiceSource class.</summary>
        TalkgroupRuleGroupVoiceSource() :
            m_tgId(0U),
            m_tgSlot(0U)
        {
            /* stub */
        }
        /// <summary>Initializes a new insatnce of the TalkgroupRuleGroupVoiceSource class.</summary>
        /// <param name="node"></param>
        TalkgroupRuleGroupVoiceSource(yaml::Node& node) :
            TalkgroupRuleGroupVoiceSource()
        {
            m_tgId = node["tgid"].as<uint32_t>(0U);
            m_tgSlot = (uint8_t)node["slot"].as<uint32_t>(0U);
        }

        /// <summary>Equals operator. Copies this TalkgroupRuleGroupVoiceSource to another TalkgroupRuleGroupVoiceSource.</summary>
        virtual TalkgroupRuleGroupVoiceSource& operator=(const TalkgroupRuleGroupVoiceSource& data)
        {
            if (this != &data) {
                m_tgId = data.m_tgId;
                m_tgSlot = data.m_tgSlot;
            }

            return *this;
        }

    public:
        /// <summary>Talkgroup ID.</summary>
        __PROPERTY_PLAIN(uint32_t, tgId, tgId);
        /// <summary>Talkgroup DMR slot.</summary>
        __PROPERTY_PLAIN(uint8_t, tgSlot, tgSlot);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //     Represents an configuration block for a routing rule.
    // ---------------------------------------------------------------------------

    class HOST_SW_API TalkgroupRuleConfig {
    public:
        /// <summary>Initializes a new insatnce of the TalkgroupRuleConfig class.</summary>
        TalkgroupRuleConfig() :
            m_active(false),
            m_parrot(false),
            m_inclusion(),
            m_exclusion()
        {
            /* stub */
        }
        /// <summary>Initializes a new insatnce of the TalkgroupRuleConfig class.</summary>
        /// <param name="node"></param>
        TalkgroupRuleConfig(yaml::Node& node) :
            TalkgroupRuleConfig()
        {
            m_active = node["active"].as<bool>(false);
            m_parrot = node["parrot"].as<bool>(false);

            yaml::Node& inclusionList = node["inclusion"];
            if (inclusionList.size() > 0U) {
                for (size_t i = 0; i < inclusionList.size(); i++) {
                    uint32_t peerId = inclusionList[i].as<uint32_t>(0U);
                    m_inclusion.push_back(peerId);
                }
            }

            yaml::Node& exclusionList = node["exclusion"];
            if (exclusionList.size() > 0U) {
                for (size_t i = 0; i < exclusionList.size(); i++) {
                    uint32_t peerId = exclusionList[i].as<uint32_t>(0U);
                    m_exclusion.push_back(peerId);
                }
            }
        }

        /// <summary>Equals operator. Copies this TalkgroupRuleConfig to another TalkgroupRuleConfig.</summary>
        virtual TalkgroupRuleConfig& operator=(const TalkgroupRuleConfig& data)
        {
            if (this != &data) {
                m_active = data.m_active;
                m_parrot = data.m_parrot;
                m_inclusion = data.m_inclusion;
                m_exclusion = data.m_exclusion;
            }

            return *this;
        }

    public:
        /// <summary>Flag indicating whether the rule is active.</summary>
        __PROPERTY_PLAIN(bool, active, active);
        /// <summary>Flag indicating whether or not the talkgroup is a parrot.</summary>
        __PROPERTY_PLAIN(bool, parrot, parrot);
        /// <summary>List of peer IDs included by this rule.</summary>
        __PROPERTY_PLAIN(std::vector<uint32_t>, inclusion, inclusion);
        /// <summary>List of peer IDs excluded by this rule.</summary>
        __PROPERTY_PLAIN(std::vector<uint32_t>, exclusion, exclusion);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //     Represents an group voice block for a routing rule.
    // ---------------------------------------------------------------------------

    class HOST_SW_API TalkgroupRuleGroupVoice {
    public:
        /// <summary>Initializes a new insatnce of the TalkgroupRuleGroupVoice class.</summary>
        TalkgroupRuleGroupVoice() :
            m_name(),
            m_config(),
            m_source()
        {
            /* stub */
        }
        /// <summary>Initializes a new insatnce of the TalkgroupRuleGroupVoice class.</summary>
        /// <param name="node"></param>
        TalkgroupRuleGroupVoice(yaml::Node& node) :
            TalkgroupRuleGroupVoice()
        {
            m_name = node["name"].as<std::string>();
            m_config = TalkgroupRuleConfig(node["config"]);
            m_source = TalkgroupRuleGroupVoiceSource(node["source"]);
        }

        /// <summary>Equals operator. Copies this TalkgroupRuleGroupVoice to another TalkgroupRuleGroupVoice.</summary>
        virtual TalkgroupRuleGroupVoice& operator=(const TalkgroupRuleGroupVoice& data)
        {
            if (this != &data) {
                m_name = data.m_name;
                m_config = data.m_config;
                m_source = data.m_source;
            }

            return *this;
        }

        /// <summary>Helper to quickly determine if a group voice entry is valid.</summary>
        bool isInvalid() const
        {
            if (m_source.tgId() == 0U)
                return true;
            return false;
        }

    public:
        /// <summary>Textual name for the routing rule.</summary>
        __PROPERTY_PLAIN(std::string, name, name);
        /// <summary>Configuration for the routing rule.</summary>
        __PROPERTY_PLAIN(TalkgroupRuleConfig, config, config);
        /// <summary>Source talkgroup information for the routing rule.</summary>
        __PROPERTY_PLAIN(TalkgroupRuleGroupVoiceSource, source, source);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements a threading lookup table class that contains routing
    //      rules information.
    // ---------------------------------------------------------------------------

    class HOST_SW_API TalkgroupRulesLookup : public Thread {
    public:
        /// <summary>Initializes a new instance of the TalkgroupRulesLookup class.</summary>
        TalkgroupRulesLookup(const std::string& filename, uint32_t reloadTime, bool acl);
        /// <summary>Finalizes a instance of the TalkgroupRulesLookup class.</summary>
        virtual ~TalkgroupRulesLookup();

        /// <summary></summary>
        void entry();

        /// <summary>Stops and unloads this lookup table.</summary>
        void stop();
        /// <summary>Reads the lookup table from the specified lookup table file.</summary>
        /// <returns>True, if lookup table was read, otherwise false.</returns>
        bool read();
        /// <summary>Clears all entries from the lookup table.</summary>
        void clear();

        /// <summary>Adds a new entry to the lookup table.</summary>
        void addEntry(uint32_t id, uint8_t slot, bool enabled);
        /// <summary>Adds a new entry to the lookup table.</summary>
        void addEntry(TalkgroupRuleGroupVoice groupVoice);
        /// <summary>Adds a new entry to the lookup table.</summary>
        void eraseEntry(uint32_t id, uint8_t slot);
        /// <summary>Finds a table entry in this lookup table.</summary>
        virtual TalkgroupRuleGroupVoice find(uint32_t id, uint8_t slot = 0U);

        /// <summary>Flag indicating whether talkgroup ID access control is enabled or not.</summary>
        bool getACL();

    private:
        const std::string& m_rulesFile;
        uint32_t m_reloadTime;
        yaml::Node m_rules;

        bool m_acl;

        std::mutex m_mutex;
        bool m_stop;

        /// <summary>Loads the table from the passed lookup table file.</summary>
        /// <returns>True, if lookup table was loaded, otherwise false.</returns>
        bool load();

    public:
        /// <summary>Number indicating the number of seconds to hang on a talkgroup.</summary>
        __PROPERTY_PLAIN(uint32_t, groupHangTime, groupHangTime);
        /// <summary>Flag indicating whether or not the network layer should send the talkgroups to peers.</summary>
        __PROPERTY_PLAIN(bool, sendTalkgroups, sendTalkgroups);
        /// <summary>List of group voice rules.</summary>
        __PROPERTY_PLAIN(std::vector<TalkgroupRuleGroupVoice>, groupVoice, groupVoice);
    };
} // namespace lookups

#endif // __TALKGROUP_RULES_LOOKUP_H__
