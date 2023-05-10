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
#if !defined(__ROUTING_RULES_LOOKUP_H__)
#define __ROUTING_RULES_LOOKUP_H__

#include "Defines.h"
#include "lookups/LookupTable.h"
#include "Thread.h"
#include "Mutex.h"
#include "yaml/Yaml.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace lookups
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //     Represents an source block for a routing rule.
    // ---------------------------------------------------------------------------

    class HOST_SW_API RoutingRuleGroupVoiceSource {
    public:
        /// <summary>Initializes a new insatnce of the RoutingRuleGroupVoiceSource class.</summary>
        RoutingRuleGroupVoiceSource() :
            m_tgId(0U),
            m_tgSlot(0U)
        {
            /* stub */
        }
        /// <summary>Initializes a new insatnce of the RoutingRuleGroupVoiceSource class.</summary>
        /// <param name="node"></param>
        /// <param name="tgSlot"></param>
        RoutingRuleGroupVoiceSource(yaml::Node& node) :
            RoutingRuleGroupVoiceSource()
        {
            m_tgId = node["tgid"].as<uint32_t>(0U);
            m_tgSlot = node["slot"].as<uint8_t>(0U);
        }

        /// <summary>Equals operator. Copies this RoutingRuleGroupVoiceSource to another RoutingRuleGroupVoiceSource.</summary>
        virtual RoutingRuleGroupVoiceSource& operator=(const RoutingRuleGroupVoiceSource& data)
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
    //     Represents an destination block for a routing rule.
    // ---------------------------------------------------------------------------

    class HOST_SW_API RoutingRuleGroupVoiceDestination {
    public:
        /// <summary>Initializes a new insatnce of the RoutingRuleGroupVoiceDestination class.</summary>
        RoutingRuleGroupVoiceDestination() :
            m_network(),
            m_tgId(0U),
            m_tgSlot(0U)
        {
            /* stub */
        }
        /// <summary>Initializes a new insatnce of the RoutingRuleGroupVoiceDestination class.</summary>
        /// <param name="node"></param>
        RoutingRuleGroupVoiceDestination(yaml::Node& node) :
            RoutingRuleGroupVoiceDestination()
        {
            /* stub */
        }

        /// <summary>Equals operator. Copies this RoutingRuleGroupVoiceDestination to another RoutingRuleGroupVoiceDestination.</summary>
        virtual RoutingRuleGroupVoiceDestination& operator=(const RoutingRuleGroupVoiceDestination& data)
        {
            if (this != &data) {
                m_network = data.m_network;
                m_tgId = data.m_tgId;
                m_tgSlot = data.m_tgSlot;
            }

            return *this;
        }

    public:
        /// <summary>Network name to route to.</summary>
        __PROPERTY_PLAIN(std::string, network, network);
        /// <summary>Talkgroup ID.</summary>
        __PROPERTY_PLAIN(uint32_t, tgId, tgId);
        /// <summary>Talkgroup DMR slot.</summary>
        __PROPERTY_PLAIN(uint8_t, tgSlot, tgSlot);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //     Represents an configuration block for a routing rule.
    // ---------------------------------------------------------------------------

    class HOST_SW_API RoutingRuleConfig {
    public:
        /// <summary>Initializes a new insatnce of the RoutingRuleConfig class.</summary>
        RoutingRuleConfig() :
            m_active(false),
            m_affiliated(false),
            m_routable(false),
            m_ignored()
        {
            /* stub */
        }
        /// <summary>Initializes a new insatnce of the RoutingRuleConfig class.</summary>
        /// <param name="node"></param>
        /// <param name="affilated"></param>
        /// <param name="routable"></param>
        RoutingRuleConfig(yaml::Node& node) :
            RoutingRuleConfig()
        {
            m_active = node["active"].as<bool>(false);
            m_affiliated = node["affiliated"].as<bool>(false);
            m_routable = node["routable"].as<bool>(false);

            yaml::Node& ignoredList = node["ignored"];
            if (ignoredList.size() > 0U) {
                for (size_t i = 0; i < ignoredList.size(); i++) {
                    uint32_t peerId = ignoredList[i].as<uint32_t>(0U);
                    m_ignored.push_back(peerId);
                }
            }
        }

        /// <summary>Equals operator. Copies this RoutingRuleConfig to another RoutingRuleConfig.</summary>
        virtual RoutingRuleConfig& operator=(const RoutingRuleConfig& data)
        {
            if (this != &data) {
                m_active = data.m_active;
                m_affiliated = data.m_affiliated;
                m_routable = data.m_routable;
                m_ignored = data.m_ignored;
            }

            return *this;
        }

    public:
        /// <summary>Flag indicating whether the rule is active.</summary>
        __PROPERTY_PLAIN(bool, active, active);
        /// <summary>Flag indicating whether or not affiliations are requires to repeat traffic.</summary>
        __PROPERTY_PLAIN(bool, affiliated, affiliated);
        /// <summary>Flag indicating whether or not this rule is routable.</summary>
        __PROPERTY_PLAIN(bool, routable, routable);
        /// <summary>List of peer IDs ignored by this rule.</summary>
        __PROPERTY_PLAIN(std::vector<uint32_t>, ignored, ignored);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //     Represents an group voice block for a routing rule.
    // ---------------------------------------------------------------------------

    class HOST_SW_API RoutingRuleGroupVoice {
    public:
        /// <summary>Initializes a new insatnce of the RoutingRuleGroupVoice class.</summary>
        RoutingRuleGroupVoice() :
            m_name(),
            m_config(),
            m_source(),
            m_destination()
        {
            /* stub */
        }
        /// <summary>Initializes a new insatnce of the RoutingRuleGroupVoice class.</summary>
        /// <param name="node"></param>
        RoutingRuleGroupVoice(yaml::Node& node) :
            RoutingRuleGroupVoice()
        {
            m_name = node["name"].as<std::string>();
            m_config = RoutingRuleConfig(node["config"]);
            m_source = RoutingRuleGroupVoiceSource(node["source"]);
            
            yaml::Node& destList = node["destination"];
            if (destList.size() > 0U) {
                for (size_t i = 0; i < destList.size(); i++) {
                    RoutingRuleGroupVoiceDestination destination = RoutingRuleGroupVoiceDestination(destList[i]);
                    m_destination.push_back(destination);
                }
            }
        }

        /// <summary>Equals operator. Copies this RoutingRuleGroupVoice to another RoutingRuleGroupVoice.</summary>
        virtual RoutingRuleGroupVoice& operator=(const RoutingRuleGroupVoice& data)
        {
            if (this != &data) {
                m_name = data.m_name;
                m_config = data.m_config;
                m_source = data.m_source;
                m_destination = data.m_destination;
            }

            return *this;
        }

    public:
        /// <summary>Textual name for the routing rule.</summary>
        __PROPERTY_PLAIN(std::string, name, name);
        /// <summary>Configuration for the routing rule.</summary>
        __PROPERTY_PLAIN(RoutingRuleConfig, config, config);
        /// <summary>Source talkgroup information for the routing rule.</summary>
        __PROPERTY_PLAIN(RoutingRuleGroupVoiceSource, source, source);
        /// <summary>Destination(s) talkgroup information for the routing rule.</summary>
        __PROPERTY_PLAIN(std::vector<RoutingRuleGroupVoiceDestination>, destination, destination);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements a threading lookup table class that contains routing
    //      rules information.
    // ---------------------------------------------------------------------------

    class HOST_SW_API RoutingRulesLookup : public Thread {
    public:
        /// <summary>Initializes a new instance of the RoutingRulesLookup class.</summary>
        RoutingRulesLookup(const std::string& filename, uint32_t reloadTime);
        /// <summary>Finalizes a instance of the RoutingRulesLookup class.</summary>
        virtual ~RoutingRulesLookup();

        /// <summary></summary>
        void entry();

        /// <summary>Stops and unloads this lookup table.</summary>
        void stop();
        /// <summary>Reads the lookup table from the specified lookup table file.</summary>
        /// <returns>True, if lookup table was read, otherwise false.</returns>
        bool read();
        /// <summary>Clears all entries from the lookup table.</summary>
        void clear();

    private:
        const std::string& m_rulesFile;
        uint32_t m_reloadTime;
        yaml::Node m_rules;

        Mutex m_mutex;
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
        __PROPERTY_PLAIN(std::vector<RoutingRuleGroupVoice>, groupVoice, groupVoice);
    };
} // namespace lookups

#endif // __ROUTING_RULES_LOOKUP_H__
