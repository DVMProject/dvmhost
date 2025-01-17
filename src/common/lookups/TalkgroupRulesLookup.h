// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023,2024 Bryan Biedenkapp, N2PLL
 *  Copyright (C) 2024 Patrick McDonnell, W3AXL
 *
 */
/**
 * @defgroup lookups_tgid Talkgroup Rules Lookups
 * @brief Implementation for talkgroup rules lookup tables.
 * @ingroup lookups
 * 
 * @file TalkgroupRulesLookup.h
 * @ingroup lookups_tgid
 * @file TalkgroupRulesLookup.cpp
 * @ingroup lookups_tgid
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
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents an source block for a routing rule.
     * @ingroup lookups_tgid
     */
    class HOST_SW_API TalkgroupRuleGroupVoiceSource {
    public:
        /**
         * @brief Initializes a new instance of the TalkgroupRuleGroupVoiceSource class.
         */
        TalkgroupRuleGroupVoiceSource() :
            m_tgId(0U),
            m_tgSlot(0U)
        {
            /* stub */
        }
        /**
         * @brief Initializes a new instance of the TalkgroupRuleGroupVoiceSource class.
         * @param node YAML node for this source block.
         */
        TalkgroupRuleGroupVoiceSource(yaml::Node& node) :
            TalkgroupRuleGroupVoiceSource()
        {
            m_tgId = node["tgid"].as<uint32_t>(0U);
            m_tgSlot = (uint8_t)node["slot"].as<uint32_t>(0U);
        }

        /**
         * @brief Equals operator. Copies this TalkgroupRuleGroupVoiceSource to another TalkgroupRuleGroupVoiceSource.
         * @param data Instance of TalkgroupRuleGroupVoiceSource to copy.
         */
        virtual TalkgroupRuleGroupVoiceSource& operator= (const TalkgroupRuleGroupVoiceSource& data)
        {
            if (this != &data) {
                m_tgId = data.m_tgId;
                m_tgSlot = data.m_tgSlot;
            }

            return *this;
        }

        /**
         * @brief Return the YAML structure for this TalkgroupRuleGroupVoiceSource.
         * @param[out] node YAML Node.
         */
        void getYaml(yaml::Node &node)
        {
            node["tgid"] = __INT_STR(m_tgId);
            node["slot"] = __INT_STR(m_tgSlot);
        }

    public:
        /**
         * @brief Talkgroup ID.
         */
        __PROPERTY_PLAIN(uint32_t, tgId);
        /**
         * @brief Talkgroup DMR slot.
         */
        __PROPERTY_PLAIN(uint8_t, tgSlot);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents an rewrite block for a routing rule rewrites.
     * @ingroup lookups_tgid
     */
    class HOST_SW_API TalkgroupRuleRewrite {
    public:
        /**
         * @brief Initializes a new instance of the TalkgroupRuleRewrite class.
         */
        TalkgroupRuleRewrite() :
            m_peerId(0U),
            m_tgId(0U),
            m_tgSlot(0U)
        {
            /* stub */
        }
        /**
         * @brief Initializes a new instance of the TalkgroupRuleRewrite class.
         * @param node YAML node for this rewrite block.
         */
        TalkgroupRuleRewrite(yaml::Node& node) :
            TalkgroupRuleRewrite()
        {
            m_peerId = node["peerid"].as<uint32_t>(0U);
            m_tgId = node["tgid"].as<uint32_t>(0U);
            m_tgSlot = (uint8_t)node["slot"].as<uint32_t>(0U);
        }

        /**
         * @brief Equals operator. Copies this TalkgroupRuleRewrite to another TalkgroupRuleRewrite.
         * @param data Instance of TalkgroupRuleRewrite to copy.
         */
        virtual TalkgroupRuleRewrite& operator= (const TalkgroupRuleRewrite& data)
        {
            if (this != &data) {
                m_peerId = data.m_peerId;
                m_tgId = data.m_tgId;
                m_tgSlot = data.m_tgSlot;
            }

            return *this;
        }

        /**
         * @brief Return the YAML structure for this TalkgroupRuleRewrite.
         * @param[out] node YAML node.
         */
        void getYaml(yaml::Node &node)
        {
            node["peerid"] = __INT_STR(m_peerId);
            node["tgid"] = __INT_STR(m_tgId);
            node["slot"] = __INT_STR(m_tgSlot);
        }

    public:
        /**
         * @brief Peer ID.
         */
        __PROPERTY_PLAIN(uint32_t, peerId);
        /**
         * @brief Talkgroup ID.
         */
        __PROPERTY_PLAIN(uint32_t, tgId);
        /**
         * @brief Talkgroup DMR slot.
         */
        __PROPERTY_PLAIN(uint8_t, tgSlot);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents an configuration block for a routing rule.
     * @ingroup lookups_tgid
     */
    class HOST_SW_API TalkgroupRuleConfig {
    public:
        /**
         * @brief Initializes a new instance of the TalkgroupRuleConfig class.
         */
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
        /**
         * @brief Initializes a new instance of the TalkgroupRuleConfig class.
         * @param node YAML node for this configuration block.
         */
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

        /**
         * @brief Equals operator. Copies this TalkgroupRuleConfig to another TalkgroupRuleConfig.
         * @param data Instance of TalkgroupRuleConfig to copy.
         */
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

        /**
         * @brief Gets the count of inclusions.
         * @returns uint8_t Total count of peer inclusions.
         */
        uint8_t inclusionSize() const { return m_inclusion.size(); }
        /**
         * @brief Gets the count of exclusions.
         * @returns uint8_t Total count of peer exclusions.
         */
        uint8_t exclusionSize() const { return m_exclusion.size(); }
        /**
         * @brief Gets the count of rewrites.
         * @returns uint8_t Total count of rewrite rules.
         */
        uint8_t rewriteSize() const { return m_rewrite.size(); }
        /**
         * @brief Gets the count of always send.
         * @returns uint8_t Total count of always send rules.
         */
        uint8_t alwaysSendSize() const { return m_alwaysSend.size(); }
        /**
         * @brief Gets the count of preferred.
         * @returns uint8_t Total count of preferred peer rules.
         */
        uint8_t preferredSize() const { return m_preferred.size(); }

        /**
         * @brief Return the YAML structure for this TalkgroupRuleConfig.
         * @param[out] node YAML node.
         */
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
        /**
         * @brief Flag indicating whether the rule is active.
         */
        __PROPERTY_PLAIN(bool, active);
        /**
         * @brief Flag indicating whether this talkgroup will only repeat with affiliations.
         */
        __PROPERTY_PLAIN(bool, affiliated);
        /**
         * @brief Flag indicating whether or not the talkgroup is a parrot.
         */
        __PROPERTY_PLAIN(bool, parrot);
        /**
         * @brief List of peer IDs included by this rule.
         */
        __PROPERTY_PLAIN(std::vector<uint32_t>, inclusion);
        /**
         * @brief List of peer IDs excluded by this rule.
         */
        __PROPERTY_PLAIN(std::vector<uint32_t>, exclusion);
        /**
         * @brief List of rewrites performed by this rule.
         */
        __PROPERTY_PLAIN(std::vector<TalkgroupRuleRewrite>, rewrite);
        /**
         * @brief List of always send performed by this rule.
         */
        __PROPERTY_PLAIN(std::vector<uint32_t>, alwaysSend);
        /**
         * @brief List of peer IDs preferred by this rule.
         */
        __PROPERTY_PLAIN(std::vector<uint32_t>, preferred);

        /**
         * @brief Flag indicating whether or not the talkgroup is a non-preferred.
         */
        __PROPERTY_PLAIN(bool, nonPreferred);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents an group voice block for a routing rule.
     * @ingroup lookups_tgid
     */
    class HOST_SW_API TalkgroupRuleGroupVoice {
    public:
        /**
         * @brief Initializes a new instance of the TalkgroupRuleGroupVoice class.
         */
        TalkgroupRuleGroupVoice() :
            m_name(),
            m_nameAlias(),
            m_config(),
            m_source()
        {
            /* stub */
        }
        /**
         * @brief Initializes a new instance of the TalkgroupRuleGroupVoice class.
         * @param node YAML node for this group voice block.
         */
        TalkgroupRuleGroupVoice(yaml::Node& node) :
            TalkgroupRuleGroupVoice()
        {
            m_name = node["name"].as<std::string>();
            m_nameAlias = node["alias"].as<std::string>();
            m_config = TalkgroupRuleConfig(node["config"]);
            m_source = TalkgroupRuleGroupVoiceSource(node["source"]);
        }

        /**
         * @brief Equals operator. Copies this TalkgroupRuleGroupVoice to another TalkgroupRuleGroupVoice.
         * @param data Instance of TalkgroupRuleGroupVoice to copy.
         */
        virtual TalkgroupRuleGroupVoice& operator= (const TalkgroupRuleGroupVoice& data)
        {
            if (this != &data) {
                m_name = data.m_name;
                m_nameAlias = data.m_nameAlias;
                m_config = data.m_config;
                m_source = data.m_source;
            }

            return *this;
        }

        /**
         * @brief Helper to quickly determine if a group voice entry is valid.
         * @returns bool True, if group voice block is valid, otherwise false.
         */
        bool isInvalid() const
        {
            if (m_source.tgId() == 0U)
                return true;
            return false;
        }

        /**
         * @brief Return the YAML structure for this TalkgroupRuleGroupVoice.
         * @param[out] node YAML node.
         */
        void getYaml(yaml::Node &node)
        {
            // Get all the properties
            node["name"] = m_name;
            node["alias"] = m_nameAlias;

            yaml::Node config, source;
            m_config.getYaml(config);
            m_source.getYaml(source);

            node["config"] = config;
            node["source"] = source;
        }

    public:
        /**
         * @brief Textual name for the routing rule.
         */
        __PROPERTY_PLAIN(std::string, name);
        /**
         * @brief (Optional) Secondary textual name for the routing rule.
         */
        __PROPERTY_PLAIN(std::string, nameAlias);
        /**
         * @brief Configuration for the routing rule.
         */
        __PROPERTY_PLAIN(TalkgroupRuleConfig, config);
        /**
         * @brief Source talkgroup information for the routing rule.
         */
        __PROPERTY_PLAIN(TalkgroupRuleGroupVoiceSource, source);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements a threading lookup table class that contains routing
     *  rules information.
     * @ingroup lookups_tgid
     */
    class HOST_SW_API TalkgroupRulesLookup : public Thread {
    public:
        /**
         * @brief Initializes a new instance of the TalkgroupRulesLookup class.
         * @param filename Full-path to the routing rules file.
         * @param reloadTime Interval of time to reload the routing rules.
         * @param acl Flag indicating these rules are enabled for enforcing access control.
         */
        TalkgroupRulesLookup(const std::string& filename, uint32_t reloadTime, bool acl);
        /**
         * @brief Finalizes a instance of the TalkgroupRulesLookup class.
         */
        ~TalkgroupRulesLookup() override;

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
         * @param id Unique ID to add.
         * @param slot DMR slot this talkgroup is valid on.
         * @param enabled Flag indicating if talkgroup ID is enabled or not.
         * @param affiliated Flag indicating if talkgroup ID requires affiliated or not.
         * @param nonPreferred Flag indicating if the talkgroup ID is non-preferred.
         */
        void addEntry(uint32_t id, uint8_t slot, bool enabled, bool affiliated = false, bool nonPreferred = false);
        /**
         * @brief Adds a new entry to the lookup table.
         * @param groupVoice Group Voice Configuration Block.
         */
        void addEntry(TalkgroupRuleGroupVoice groupVoice);
        /**
         * @brief Erases an existing entry from the lookup table by the specified unique ID.
         * @param id Unique ID to erase.
         * @param slot DMR slot this talkgroup is valid on.
         */
        void eraseEntry(uint32_t id, uint8_t slot);
        /**
         * @brief Finds a table entry in this lookup table.
         * @param id Unique identifier for table entry.
         * @param slot DMR slot this talkgroup is valid on.
         * @returns TalkgroupRuleGroupVoice Table entry.
         */
        virtual TalkgroupRuleGroupVoice find(uint32_t id, uint8_t slot = 0U);
        /**
         * @brief Finds a table entry in this lookup table by rewrite.
         * @param peerId Unique identifier for table entry.
         * @param id Unique identifier for table entry.
         * @param slot DMR slot this talkgroup is valid on.
         * @return TalkgroupRuleGroupVoice Table entry.
         */
        virtual TalkgroupRuleGroupVoice findByRewrite(uint32_t peerId, uint32_t id, uint8_t slot = 0U);

        /**
         * @brief Saves loaded talkgroup rules.
         */
        bool commit();

        /**
         * @brief Flag indicating whether talkgroup ID access control is enabled or not.
         * @returns bool True, if talkgroup ID access control is enabled, otherwise false.
         */
        bool getACL();

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
        void setReloadTime(uint32_t reloadTime) { m_reloadTime = 0U; }

    private:
        std::string m_rulesFile;
        uint32_t m_reloadTime;
        yaml::Node m_rules;

        bool m_acl;
        bool m_stop;

        static std::mutex m_mutex;

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
         * @brief Number indicating the number of seconds to hang on a talkgroup.
         */
        __PROPERTY_PLAIN(uint32_t, groupHangTime);
        /**
         * @brief Flag indicating whether or not the network layer should send the talkgroups to peers.
         */
        __PROPERTY_PLAIN(bool, sendTalkgroups);
        /**
         * @brief List of group voice rules.
         */
        __PROPERTY_PLAIN(std::vector<TalkgroupRuleGroupVoice>, groupVoice);
    };
} // namespace lookups

#endif // __TALKGROUP_RULES_LOOKUP_H__
