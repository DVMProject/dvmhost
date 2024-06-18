// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023,2024 Bryan Biedenkapp, N2PLL
*   Copyright (C) 2024 Patrick McDonnell, W3AXL
*
*/
#if !defined(__TALKGROUP_RULES_LOOKUP_H__)
#define __TALKGROUP_RULES_LOOKUP_H__

#include "common/Defines.h"
#include "common/lookups/LookupTable.h"
#include "common/yaml/Yaml.h"
#include "common/Utils.h"

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
        /// <summary>Initializes a new instance of the TalkgroupRuleGroupVoiceSource class.</summary>
        TalkgroupRuleGroupVoiceSource() :
            m_tgId(0U),
            m_tgSlot(0U)
        {
            /* stub */
        }
        /// <summary>Initializes a new instance of the TalkgroupRuleGroupVoiceSource class.</summary>
        /// <param name="node"></param>
        TalkgroupRuleGroupVoiceSource(yaml::Node& node) :
            TalkgroupRuleGroupVoiceSource()
        {
            m_tgId = node["tgid"].as<uint32_t>(0U);
            m_tgSlot = (uint8_t)node["slot"].as<uint32_t>(0U);
        }

        /// <summary>Equals operator. Copies this TalkgroupRuleGroupVoiceSource to another TalkgroupRuleGroupVoiceSource.</summary>
        virtual TalkgroupRuleGroupVoiceSource& operator= (const TalkgroupRuleGroupVoiceSource& data)
        {
            if (this != &data) {
                m_tgId = data.m_tgId;
                m_tgSlot = data.m_tgSlot;
            }

            return *this;
        }

        /// <summary>Return the YAML structure for this TalkgroupRuleGroupVoiceSource.</summary>
        void getYaml(yaml::Node &node)
        {
            node["tgid"] = __INT_STR(m_tgId);
            node["slot"] = __INT_STR(m_tgSlot);
        }

    public:
        /// <summary>Talkgroup ID.</summary>
        __PROPERTY_PLAIN(uint32_t, tgId);
        /// <summary>Talkgroup DMR slot.</summary>
        __PROPERTY_PLAIN(uint8_t, tgSlot);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //     Represents an rewrite block for a routing rule rewrites.
    // ---------------------------------------------------------------------------

    class HOST_SW_API TalkgroupRuleRewrite {
    public:
        /// <summary>Initializes a new instance of the TalkgroupRuleRewrite class.</summary>
        TalkgroupRuleRewrite() :
            m_peerId(0U),
            m_tgId(0U),
            m_tgSlot(0U)
        {
            /* stub */
        }
        /// <summary>Initializes a new instance of the TalkgroupRuleRewrite class.</summary>
        /// <param name="node"></param>
        TalkgroupRuleRewrite(yaml::Node& node) :
            TalkgroupRuleRewrite()
        {
            m_peerId = node["peerid"].as<uint32_t>(0U);
            m_tgId = node["tgid"].as<uint32_t>(0U);
            m_tgSlot = (uint8_t)node["slot"].as<uint32_t>(0U);
        }

        /// <summary>Equals operator. Copies this TalkgroupRuleRewrite to another TalkgroupRuleRewrite.</summary>
        virtual TalkgroupRuleRewrite& operator= (const TalkgroupRuleRewrite& data)
        {
            if (this != &data) {
                m_peerId = data.m_peerId;
                m_tgId = data.m_tgId;
                m_tgSlot = data.m_tgSlot;
            }

            return *this;
        }

        /// <summary>Return the YAML structure for this TalkgroupRuleRewrite.</summary>
        void getYaml(yaml::Node &node)
        {
            node["peerid"] = __INT_STR(m_peerId);
            node["tgid"] = __INT_STR(m_tgId);
            node["slot"] = __INT_STR(m_tgSlot);
        }

    public:
        /// <summary>Peer ID.</summary>
        __PROPERTY_PLAIN(uint32_t, peerId);
        /// <summary>Talkgroup ID.</summary>
        __PROPERTY_PLAIN(uint32_t, tgId);
        /// <summary>Talkgroup DMR slot.</summary>
        __PROPERTY_PLAIN(uint8_t, tgSlot);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //     Represents an configuration block for a routing rule.
    // ---------------------------------------------------------------------------

    class HOST_SW_API TalkgroupRuleConfig {
    public:
        /// <summary>Initializes a new instance of the TalkgroupRuleConfig class.</summary>
        TalkgroupRuleConfig() :
            m_active(false),
            m_affiliated(false),
            m_parrot(false),
            m_inclusion(),
            m_exclusion(),
            m_rewrite(),
            m_alwaysSend(),
            m_preferred(),
            m_nonPreferred(false)
        {
            /* stub */
        }
        /// <summary>Initializes a new instance of the TalkgroupRuleConfig class.</summary>
        /// <param name="node"></param>
        TalkgroupRuleConfig(yaml::Node& node) :
            TalkgroupRuleConfig()
        {
            m_active = node["active"].as<bool>(false);
            m_affiliated = node["affiliated"].as<bool>(false);
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

            yaml::Node& rewriteList = node["rewrite"];
            if (rewriteList.size() > 0U) {
                for (size_t i = 0; i < rewriteList.size(); i++) {
                    TalkgroupRuleRewrite rewrite = TalkgroupRuleRewrite(rewriteList[i]);
                    m_rewrite.push_back(rewrite);
                }
            }

            yaml::Node& alwaysSendList = node["always"];
            if (alwaysSendList.size() > 0U) {
                for (size_t i = 0; i < alwaysSendList.size(); i++) {
                    uint32_t peerId = alwaysSendList[i].as<uint32_t>(0U);
                    m_alwaysSend.push_back(peerId);
                }
            }

            yaml::Node& preferredList = node["preferred"];
            if (preferredList.size() > 0U) {
                for (size_t i = 0; i < preferredList.size(); i++) {
                    uint32_t peerId = preferredList[i].as<uint32_t>(0U);
                    m_preferred.push_back(peerId);
                }
            }
        }

        /// <summary>Equals operator. Copies this TalkgroupRuleConfig to another TalkgroupRuleConfig.</summary>
        virtual TalkgroupRuleConfig& operator= (const TalkgroupRuleConfig& data)
        {
            if (this != &data) {
                m_active = data.m_active;
                m_affiliated = data.m_affiliated;
                m_parrot = data.m_parrot;
                m_inclusion = data.m_inclusion;
                m_exclusion = data.m_exclusion;
                m_rewrite = data.m_rewrite;
                m_alwaysSend = data.m_alwaysSend;
                m_preferred = data.m_preferred;
                m_nonPreferred = data.m_nonPreferred;
            }

            return *this;
        }

        /// <summary>Gets the count of inclusions.</summary>
        uint8_t inclusionSize() const { return m_inclusion.size(); }
        /// <summary>Gets the count of exclusions.</summary>
        uint8_t exclusionSize() const { return m_exclusion.size(); }
        /// <summary>Gets the count of rewrites.</summary>
        uint8_t rewriteSize() const { return m_rewrite.size(); }
        /// <summary>Gets the count of always send.</summary>
        uint8_t alwaysSendSize() const { return m_alwaysSend.size(); }
        /// <summary>Gets the count of rewrites.</summary>
        uint8_t preferredSize() const { return m_preferred.size(); }

        /// <summary>Return the YAML structure for this TalkgroupRuleConfig.</summary>
        void getYaml(yaml::Node &node)
        {
            // We have to convert the bools back to strings to pass to the yaml node
            node["active"] = __BOOL_STR(m_active);
            node["affiliated"] = __BOOL_STR(m_affiliated);
            node["parrot"] = __BOOL_STR(m_parrot);
            
            // Get the lists
            yaml::Node inclusionList;
            if (m_inclusion.size() > 0U) {
                for (auto inc : m_inclusion) {
                    yaml::Node& newIn = inclusionList.push_back();
                    newIn = __INT_STR(inc);
                }
            }
            node["inclusion"] = inclusionList;
            
            yaml::Node exclusionList;
            if (m_exclusion.size() > 0U) {
                for (auto exc : m_exclusion) {
                    yaml::Node& newEx = exclusionList.push_back();
                    newEx = __INT_STR(exc);
                }
            }
            node["exclusion"] = exclusionList;
            
            yaml::Node rewriteList;
            if (m_rewrite.size() > 0U) {
                for (auto rule : m_rewrite) {
                    yaml::Node& rewrite = rewriteList.push_back();
                    rule.getYaml(rewrite);
                }
            }
            node["rewrite"] = rewriteList;

            yaml::Node alwaysSendList;
            if (m_alwaysSend.size() > 0U) {
                for (auto alw : m_alwaysSend) {
                    yaml::Node& newAlw = alwaysSendList.push_back();
                    newAlw = __INT_STR(alw);
                }
            }
            node["always"] = alwaysSendList;

            yaml::Node preferredList;
            if (m_preferred.size() > 0U) {
                for (auto pref : m_preferred) {
                    yaml::Node& newPref = preferredList.push_back();
                    newPref = __INT_STR(pref);
                }
            }
            node["preferred"] = preferredList;
        }

    public:
        /// <summary>Flag indicating whether the rule is active.</summary>
        __PROPERTY_PLAIN(bool, active);
        /// <summary>Flag indicating whether this talkgroup will only repeat with affiliations.</summary>
        __PROPERTY_PLAIN(bool, affiliated);
        /// <summary>Flag indicating whether or not the talkgroup is a parrot.</summary>
        __PROPERTY_PLAIN(bool, parrot);
        /// <summary>List of peer IDs included by this rule.</summary>
        __PROPERTY_PLAIN(std::vector<uint32_t>, inclusion);
        /// <summary>List of peer IDs excluded by this rule.</summary>
        __PROPERTY_PLAIN(std::vector<uint32_t>, exclusion);
        /// <summary>List of rewrites performed by this rule.</summary>
        __PROPERTY_PLAIN(std::vector<TalkgroupRuleRewrite>, rewrite);
        /// <summary>List of always send performed by this rule.</summary>
        __PROPERTY_PLAIN(std::vector<uint32_t>, alwaysSend);
        /// <summary>List of peer IDs preferred by this rule.</summary>
        __PROPERTY_PLAIN(std::vector<uint32_t>, preferred);

        /// <summary>Flag indicating whether or not the talkgroup is a non-preferred.</summary>
        __PROPERTY_PLAIN(bool, nonPreferred);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //     Represents an group voice block for a routing rule.
    // ---------------------------------------------------------------------------

    class HOST_SW_API TalkgroupRuleGroupVoice {
    public:
        /// <summary>Initializes a new instance of the TalkgroupRuleGroupVoice class.</summary>
        TalkgroupRuleGroupVoice() :
            m_name(),
            m_config(),
            m_source()
        {
            /* stub */
        }
        /// <summary>Initializes a new instance of the TalkgroupRuleGroupVoice class.</summary>
        /// <param name="node"></param>
        TalkgroupRuleGroupVoice(yaml::Node& node) :
            TalkgroupRuleGroupVoice()
        {
            m_name = node["name"].as<std::string>();
            m_config = TalkgroupRuleConfig(node["config"]);
            m_source = TalkgroupRuleGroupVoiceSource(node["source"]);
        }

        /// <summary>Equals operator. Copies this TalkgroupRuleGroupVoice to another TalkgroupRuleGroupVoice.</summary>
        virtual TalkgroupRuleGroupVoice& operator= (const TalkgroupRuleGroupVoice& data)
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

        /// <summary>Return the YAML structure for this TalkgroupRuleGroupVoice.</summary>
        void getYaml(yaml::Node &node)
        {
            // Get all the properties
            node["name"] = m_name;

            yaml::Node config, source;
            m_config.getYaml(config);
            m_source.getYaml(source);

            node["config"] = config;
            node["source"] = source;
        }


    public:
        /// <summary>Textual name for the routing rule.</summary>
        __PROPERTY_PLAIN(std::string, name);
        /// <summary>Configuration for the routing rule.</summary>
        __PROPERTY_PLAIN(TalkgroupRuleConfig, config);
        /// <summary>Source talkgroup information for the routing rule.</summary>
        __PROPERTY_PLAIN(TalkgroupRuleGroupVoiceSource, source);
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
        ~TalkgroupRulesLookup() override;

        /// <summary></summary>
        void entry() override;

        /// <summary>Stops and unloads this lookup table.</summary>
        void stop();
        /// <summary>Reads the lookup table from the specified lookup table file.</summary>
        /// <returns>True, if lookup table was read, otherwise false.</returns>
        bool read();
        /// <summary>Reads the lookup table from the specified lookup table file.</summary>
        /// <returns>True, if lookup table was read, otherwise false.</returns>
        bool reload() { return load(); }
        /// <summary>Clears all entries from the lookup table.</summary>
        void clear();

        /// <summary>Adds a new entry to the lookup table.</summary>
        void addEntry(uint32_t id, uint8_t slot, bool enabled, bool nonPreferred = false);
        /// <summary>Adds a new entry to the lookup table.</summary>
        void addEntry(TalkgroupRuleGroupVoice groupVoice);
        /// <summary>Erases an existing entry from the lookup table by the specified unique ID.</summary>
        void eraseEntry(uint32_t id, uint8_t slot);
        /// <summary>Finds a table entry in this lookup table.</summary>
        virtual TalkgroupRuleGroupVoice find(uint32_t id, uint8_t slot = 0U);
        /// <summary>Finds a table entry in this lookup table by rewrite.</summary>
        virtual TalkgroupRuleGroupVoice findByRewrite(uint32_t peerId, uint32_t id, uint8_t slot = 0U);

        /// <summary>Saves loaded talkgroup rules.</summary>
        bool commit();

        /// <summary>Flag indicating whether talkgroup ID access control is enabled or not.</summary>
        bool getACL();

    private:
        const std::string m_rulesFile;
        uint32_t m_reloadTime;
        yaml::Node m_rules;

        bool m_acl;

        static std::mutex m_mutex;
        bool m_stop;

        /// <summary>Loads the table from the passed lookup table file.</summary>
        /// <returns>True, if lookup table was loaded, otherwise false.</returns>
        bool load();
        /// <summary>Saves the table to the passed lookup table file.</summary>
        /// <returns>True, if lookup table was saved, otherwise false.</returns>
        bool save();

    public:
        /// <summary>Number indicating the number of seconds to hang on a talkgroup.</summary>
        __PROPERTY_PLAIN(uint32_t, groupHangTime);
        /// <summary>Flag indicating whether or not the network layer should send the talkgroups to peers.</summary>
        __PROPERTY_PLAIN(bool, sendTalkgroups);
        /// <summary>List of group voice rules.</summary>
        __PROPERTY_PLAIN(std::vector<TalkgroupRuleGroupVoice>, groupVoice);
    };
} // namespace lookups

#endif // __TALKGROUP_RULES_LOOKUP_H__
