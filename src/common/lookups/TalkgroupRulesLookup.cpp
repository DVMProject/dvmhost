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
#include "lookups/TalkgroupRulesLookup.h"
#include "Log.h"
#include "Timer.h"
#include "Utils.h"

using namespace lookups;

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the TalkgroupRulesLookup class.
/// </summary>
/// <param name="filename">Full-path to the routing rules file.</param>
/// <param name="reloadTime">Interval of time to reload the routing rules.</param>
/// <param name="acl"></param>
TalkgroupRulesLookup::TalkgroupRulesLookup(const std::string& filename, uint32_t reloadTime, bool acl) : Thread(),
    m_rulesFile(filename),
    m_reloadTime(reloadTime),
    m_rules(),
    m_acl(acl),
    m_groupHangTime(5U),
    m_sendTalkgroups(false),
    m_groupVoice()
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the TalkgroupRulesLookup class.
/// </summary>
TalkgroupRulesLookup::~TalkgroupRulesLookup() = default;

/// <summary>
///
/// </summary>
void TalkgroupRulesLookup::entry()
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

/// <summary>
/// Stops and unloads this lookup table.
/// </summary>
void TalkgroupRulesLookup::stop()
{
    if (m_reloadTime == 0U) {
        delete this;
        return;
    }

    m_stop = true;

    wait();
}

/// <summary>
/// Reads the lookup table from the specified lookup table file.
/// </summary>
/// <returns>True, if lookup table was read, otherwise false.</returns>
bool TalkgroupRulesLookup::read()
{
    bool ret = load();

    if (m_reloadTime > 0U)
        run();

    return ret;
}

/// <summary>
/// Clears all entries from the lookup table.
/// </summary>
void TalkgroupRulesLookup::clear()
{
    m_mutex.lock();
    {
        m_groupVoice.clear();
    }
    m_mutex.unlock();
}

/// <summary>
/// Adds a new entry to the lookup table by the specified unique ID.
/// </summary>
/// <param name="id">Unique ID to add.</param>
/// <param name="slot">DMR slot this talkgroup is valid on.</param>
/// <param name="enabled">Flag indicating if talkgroup ID is enabled or not.</param>
void TalkgroupRulesLookup::addEntry(uint32_t id, uint8_t slot, bool enabled)
{
    TalkgroupRuleGroupVoiceSource source;
    TalkgroupRuleConfig config;
    source.tgId(id);
    source.tgSlot(slot);
    config.active(enabled);

    m_mutex.lock();
    {
        auto it = std::find_if(m_groupVoice.begin(), m_groupVoice.end(), 
            [&](TalkgroupRuleGroupVoice x) 
            { 
                if (slot != 0U) {
                    return x.source().tgId() == id && x.source().tgSlot() == slot;
                }

                return x.source().tgId() == id; 
            });
        if (it != m_groupVoice.end()) {
            source = it->source();
            source.tgId(id);
            source.tgSlot(slot);
            
            config = it->config();
            config.active(enabled);

            TalkgroupRuleGroupVoice entry = *it;
            entry.config(config);
            entry.source(source);

            m_groupVoice[it - m_groupVoice.begin()] = entry;
        }
        else {
            TalkgroupRuleGroupVoice entry;
            entry.config(config);
            entry.source(source);

            m_groupVoice.push_back(entry);
        }
    }
    m_mutex.unlock();
}

/// <summary>
/// Adds a new entry to the lookup table by the specified unique ID.
/// </summary>
/// <param name="groupVoice"></param>
void TalkgroupRulesLookup::addEntry(TalkgroupRuleGroupVoice groupVoice)
{
    if (groupVoice.isInvalid())
        return;

    TalkgroupRuleGroupVoice entry = groupVoice;
    uint32_t id = entry.source().tgId();
    uint8_t slot = entry.source().tgSlot();

    m_mutex.lock();
    {
        auto it = std::find_if(m_groupVoice.begin(), m_groupVoice.end(), 
            [&](TalkgroupRuleGroupVoice x) 
            { 
                if (slot != 0U) {
                    return x.source().tgId() == id && x.source().tgSlot() == slot;
                }

                return x.source().tgId() == id; 
            });
        if (it != m_groupVoice.end()) {
            m_groupVoice[it - m_groupVoice.begin()] = entry;
        }
        else {
            m_groupVoice.push_back(entry);
        }
    }
    m_mutex.unlock();
}

/// <summary>
/// Erases an existing entry from the lookup table by the specified unique ID.
/// </summary>
/// <param name="id">Unique ID to erase.</param>
/// <param name="slot">DMR slot this talkgroup is valid on.</param>
void TalkgroupRulesLookup::eraseEntry(uint32_t id, uint8_t slot)
{
    m_mutex.lock();
    {
        auto it = std::find_if(m_groupVoice.begin(), m_groupVoice.end(), [&](TalkgroupRuleGroupVoice x) { return x.source().tgId() == id && x.source().tgSlot() == slot; });
        if (it != m_groupVoice.end()) {
            m_groupVoice.erase(it);
        }
    }
    m_mutex.unlock();
}

/// <summary>
/// Finds a table entry in this lookup table.
/// </summary>
/// <param name="id">Unique identifier for table entry.</param>
/// <param name="slot">DMR slot this talkgroup is valid on.</param>
/// <returns>Table entry.</returns>
TalkgroupRuleGroupVoice TalkgroupRulesLookup::find(uint32_t id, uint8_t slot)
{
    TalkgroupRuleGroupVoice entry;

    m_mutex.lock();
    {
        auto it = std::find_if(m_groupVoice.begin(), m_groupVoice.end(),
            [&](TalkgroupRuleGroupVoice x)
            {
                if (slot != 0U) {
                    return x.source().tgId() == id && x.source().tgSlot() == slot;
                }

                return x.source().tgId() == id;
            });
        if (it != m_groupVoice.end()) {
            entry = *it;
        } else {
            entry = TalkgroupRuleGroupVoice();
        }
    }
    m_mutex.unlock();

    return entry;
}

/// <summary>
/// Finds a table entry in this lookup table.
/// </summary>
/// <param name="peerId">Unique identifier for table entry.</param>
/// <param name="id">Unique identifier for table entry.</param>
/// <param name="slot">DMR slot this talkgroup is valid on.</param>
/// <returns>Table entry.</returns>
TalkgroupRuleGroupVoice TalkgroupRulesLookup::findByRewrite(uint32_t peerId, uint32_t id, uint8_t slot)
{
    TalkgroupRuleGroupVoice entry;

    m_mutex.lock();
    {
        auto it = std::find_if(m_groupVoice.begin(), m_groupVoice.end(),
            [&](TalkgroupRuleGroupVoice x)
            {
                if (x.config().rewrite().size() == 0)
                    return false;

                std::vector<TalkgroupRuleRewrite> rewrite = x.config().rewrite();
                auto innerIt = std::find_if(rewrite.begin(), rewrite.end(),
                    [&](TalkgroupRuleRewrite y)
                    {
                        if (slot != 0U) {
                            return y.peerId() == peerId && y.tgId() == id && y.tgSlot() == slot;
                        }

                        return y.peerId() == peerId && y.tgId() == id;
                    });

                if (innerIt != rewrite.end())
                    return true;
                return false;
            });
        if (it != m_groupVoice.end()) {
            entry = *it;
        } else {
            entry = TalkgroupRuleGroupVoice();
        }
    }
    m_mutex.unlock();

    return entry;
}

/// <summary>
/// Saves loaded talkgroup rules.
/// </summary>
bool TalkgroupRulesLookup::commit()
{
    return save();
}

/// <summary>
/// Flag indicating whether talkgroup ID access control is enabled or not.
/// </summary>
/// <returns>True, if talkgroup ID access control is enabled, otherwise false.</returns>
bool TalkgroupRulesLookup::getACL()
{
    return m_acl;
}

void TalkgroupRuleGroupVoice::getYaml(yaml::Node &node)
{
    // Get all the properties
    node["name"] = m_name;
    yaml::Node config, source;
    m_config.getYaml(config);
    m_source.getYaml(source);
    node["config"] = config;
    node["source"] = source;
}

void TalkgroupRuleConfig::getYaml(yaml::Node &node)
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
}

void TalkgroupRuleGroupVoiceSource::getYaml(yaml::Node &node)
{
    node["tgid"] = __INT_STR(m_tgId);
    node["slot"] = __INT_STR(m_tgSlot);
}

void TalkgroupRuleRewrite::getYaml(yaml::Node &node)
{
    node["peerid"] = __INT_STR(m_peerId);
    node["tgid"] = __INT_STR(m_tgId);
    node["slot"] = __INT_STR(m_tgSlot);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Loads the table from the passed lookup table file.
/// </summary>
/// <returns>True, if lookup table was loaded, otherwise false.</returns>
bool TalkgroupRulesLookup::load()
{
    if (m_rulesFile.length() <= 0) {
        return false;
    }

    try {
        bool ret = yaml::Parse(m_rules, m_rulesFile.c_str());
        if (!ret) {
            LogError(LOG_HOST, "Cannot open the talkgroup rules lookup file - %s - error parsing YML", m_rulesFile.c_str());
            return false;
        }
    }
    catch (yaml::OperationException const& e) {
        LogError(LOG_HOST, "Cannot open the talkgroup rules lookup file - %s (%s)", m_rulesFile.c_str(), e.message());
        return false;
    }

    // clear table
    clear();

    m_mutex.lock();
    {
        yaml::Node& groupVoiceList = m_rules["groupVoice"];

        if (groupVoiceList.size() == 0U) {
            ::LogError(LOG_HOST, "No group voice rules list defined!");
            return false;
        }

        for (size_t i = 0; i < groupVoiceList.size(); i++) {
            TalkgroupRuleGroupVoice groupVoice = TalkgroupRuleGroupVoice(groupVoiceList[i]);
            m_groupVoice.push_back(groupVoice);

            std::string groupName = groupVoice.name();
            uint32_t tgId = groupVoice.source().tgId();
            uint8_t tgSlot = groupVoice.source().tgSlot();
            bool active = groupVoice.config().active();
            bool parrot = groupVoice.config().parrot();

            uint32_t incCount = groupVoice.config().inclusion().size();
            uint32_t excCount = groupVoice.config().exclusion().size();
            uint32_t rewrCount = groupVoice.config().rewrite().size();

            if (incCount > 0 && excCount > 0) {
                ::LogWarning(LOG_HOST, "Talkgroup (%s) defines both inclusions and exclusions! Inclusions take precedence and exclusions will be ignored.", groupName.c_str());
            }

            ::LogInfoEx(LOG_HOST, "Talkgroup NAME: %s SRC_TGID: %u SRC_TS: %u ACTIVE: %u PARROT: %u INCLUSIONS: %u EXCLUSIONS: %u REWRITES: %u", groupName.c_str(), tgId, tgSlot, active, parrot, incCount, excCount, rewrCount);
        }
    }
    m_mutex.unlock();

    size_t size = m_groupVoice.size();
    if (size == 0U)
        return false;

    LogInfoEx(LOG_HOST, "Loaded %u entries into lookup table", size);

    return true;
}

/// <summary>
/// Saves the table to the passed lookup table file.
/// </summary>
/// <returns>True, if lookup table was saved, otherwise false.</returns>
bool TalkgroupRulesLookup::save()
{
    // Make sure file is valid
    if (m_rulesFile.length() <= 0) {
        return false;
    }

    m_mutex.lock();
    
    // New list for our new group voice rules
    yaml::Node groupVoiceList;
    yaml::Node newRules;

    for (auto entry : m_groupVoice) {
        yaml::Node& gv = groupVoiceList.push_back();
        entry.getYaml(gv);
        LogDebug(LOG_HOST, "Added TGID %s to yaml TG list", gv["name"].as<std::string>().c_str());
    }

    LogDebug(LOG_HOST, "Got final GroupVoiceList YAML size of %u", groupVoiceList.size());
    
    // Set the new rules
    newRules["groupVoice"] = groupVoiceList;

    m_mutex.unlock();

    // Make sure we actually did stuff right
    if (newRules["groupVoice"].size() != m_groupVoice.size()) {
        LogError(LOG_HOST, "Generated YAML node for group lists did not match loaded group size! (%u != %u)", newRules["groupVoice"].size(), m_groupVoice.size());
        return false;
    }

    try {
        LogDebug(LOG_HOST, "Saving TGID file to %s", m_rulesFile.c_str());
        yaml::Serialize(newRules, m_rulesFile.c_str());
        LogDebug(LOG_HOST, "Saved TGID config file to %s", m_rulesFile.c_str());
    }
    catch (yaml::OperationException const& e) {
        LogError(LOG_HOST, "Cannot open the talkgroup rules lookup file - %s (%s)", m_rulesFile.c_str(), e.message());
        return false;
    }

    return true;
}