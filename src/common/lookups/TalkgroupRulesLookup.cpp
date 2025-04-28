// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023,2024,2025 Bryan Biedenkapp, N2PLL
 *  Copyright (C) 2024 Patrick McDonnell, W3AXL
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
//  Static Class Members
// ---------------------------------------------------------------------------

std::mutex TalkgroupRulesLookup::m_mutex;
bool TalkgroupRulesLookup::m_locked = false;

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

// Lock the table.
#define __LOCK_TABLE()                          \
    std::lock_guard<std::mutex> lock(m_mutex);  \
    m_locked = true;

// Unlock the table.
#define __UNLOCK_TABLE() m_locked = false;

// Spinlock wait for table to be read unlocked.
#define __SPINLOCK()                            \
    if (m_locked) {                             \
        while (m_locked)                        \
            Thread::sleep(2U);                  \
    }

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the TalkgroupRulesLookup class. */

TalkgroupRulesLookup::TalkgroupRulesLookup(const std::string& filename, uint32_t reloadTime, bool acl) : Thread(),
    m_rulesFile(filename),
    m_reloadTime(reloadTime),
    m_rules(),
    m_acl(acl),
    m_stop(false),
    m_groupHangTime(5U),
    m_sendTalkgroups(false),
    m_groupVoice()
{
    /* stub */
}

/* Finalizes a instance of the TalkgroupRulesLookup class. */

TalkgroupRulesLookup::~TalkgroupRulesLookup() = default;

/* Thread entry point. This function is provided to run the thread for the lookup table. */

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

/* Stops and unloads this lookup table. */

void TalkgroupRulesLookup::stop(bool noDestroy)
{
    if (m_reloadTime == 0U) {
        if (!noDestroy)
            delete this;
        return;
    }

    m_stop = true;

    wait();
}

/* Reads the lookup table from the specified lookup table file. */

bool TalkgroupRulesLookup::read()
{
    bool ret = load();

    if (m_reloadTime > 0U)
        run();
    setName("host:tg-lookup-tbl");

    return ret;
}

/* Clears all entries from the lookup table. */

void TalkgroupRulesLookup::clear()
{
    __LOCK_TABLE();

    m_groupVoice.clear();

    __UNLOCK_TABLE();
}

/* Adds a new entry to the lookup table by the specified unique ID. */

void TalkgroupRulesLookup::addEntry(uint32_t id, uint8_t slot, bool enabled, bool affiliated, bool nonPreferred)
{
    TalkgroupRuleGroupVoiceSource source;
    TalkgroupRuleConfig config;
    source.tgId(id);
    source.tgSlot(slot);
    config.active(enabled);
    config.affiliated(affiliated);
    config.nonPreferred(nonPreferred);

    __LOCK_TABLE();

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
        config.affiliated(affiliated);
        config.nonPreferred(nonPreferred);

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

    __UNLOCK_TABLE();
}

/* Adds a new entry to the lookup table by the specified unique ID. */

void TalkgroupRulesLookup::addEntry(TalkgroupRuleGroupVoice groupVoice)
{
    if (groupVoice.isInvalid())
        return;

    TalkgroupRuleGroupVoice entry = groupVoice;
    uint32_t id = entry.source().tgId();
    uint8_t slot = entry.source().tgSlot();

    __LOCK_TABLE();

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

    __UNLOCK_TABLE();
}

/* Erases an existing entry from the lookup table by the specified unique ID. */

void TalkgroupRulesLookup::eraseEntry(uint32_t id, uint8_t slot)
{
    __LOCK_TABLE();

    auto it = std::find_if(m_groupVoice.begin(), m_groupVoice.end(), [&](TalkgroupRuleGroupVoice x) { return x.source().tgId() == id && x.source().tgSlot() == slot; });
    if (it != m_groupVoice.end()) {
        m_groupVoice.erase(it);
    }

    __UNLOCK_TABLE();
}

/* Finds a table entry in this lookup table. */

TalkgroupRuleGroupVoice TalkgroupRulesLookup::find(uint32_t id, uint8_t slot)
{
    TalkgroupRuleGroupVoice entry;

    __SPINLOCK();

    std::lock_guard<std::mutex> lock(m_mutex);
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

    return entry;
}

/* Finds a table entry in this lookup table. */

TalkgroupRuleGroupVoice TalkgroupRulesLookup::findByRewrite(uint32_t peerId, uint32_t id, uint8_t slot)
{
    TalkgroupRuleGroupVoice entry;

    __SPINLOCK();

    std::lock_guard<std::mutex> lock(m_mutex);
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

    return entry;
}

/* Saves loaded talkgroup rules. */

bool TalkgroupRulesLookup::commit()
{
    return save();
}

/* Flag indicating whether talkgroup ID access control is enabled or not. */

bool TalkgroupRulesLookup::getACL()
{
    return m_acl;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Loads the table from the passed lookup table file. */

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

    __LOCK_TABLE();

    yaml::Node& groupVoiceList = m_rules["groupVoice"];

    if (groupVoiceList.size() == 0U) {
        ::LogError(LOG_HOST, "No group voice rules list defined!");
        m_locked = false;
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
        bool affil = groupVoice.config().affiliated();

        uint32_t incCount = groupVoice.config().inclusion().size();
        uint32_t excCount = groupVoice.config().exclusion().size();
        uint32_t rewrCount = groupVoice.config().rewrite().size();
        uint32_t alwyCount = groupVoice.config().alwaysSend().size();
        uint32_t prefCount = groupVoice.config().preferred().size();
        uint32_t permRIDCount = groupVoice.config().permittedRIDs().size();

        if (incCount > 0 && excCount > 0) {
            ::LogWarning(LOG_HOST, "Talkgroup (%s) defines both inclusions and exclusions! Inclusion rules take precedence and exclusion rules will be ignored.", groupName.c_str());
        }

        if (alwyCount > 0 && affil) {
            ::LogWarning(LOG_HOST, "Talkgroup (%s) is marked as affiliation required and has a defined always send list! Always send peers take rule precedence and defined peers will always receive traffic.", groupName.c_str());
        }

        ::LogInfoEx(LOG_HOST, "Talkgroup NAME: %s SRC_TGID: %u SRC_TS: %u ACTIVE: %u PARROT: %u AFFILIATED: %u INCLUSIONS: %u EXCLUSIONS: %u REWRITES: %u ALWAYS: %u PREFERRED: %u PERMITTED RIDS: %u", groupName.c_str(), tgId, tgSlot, active, parrot, affil, incCount, excCount, rewrCount, alwyCount, prefCount, permRIDCount);
    }

    __UNLOCK_TABLE();

    size_t size = m_groupVoice.size();
    if (size == 0U) {
        return false;
    }

    LogInfoEx(LOG_HOST, "Loaded %lu entries into talkgroup rules table", size);

    return true;
}

/* Saves the table to the passed lookup table file. */

bool TalkgroupRulesLookup::save()
{
    // Make sure file is valid
    if (m_rulesFile.length() <= 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    
    // New list for our new group voice rules
    yaml::Node groupVoiceList;
    yaml::Node newRules;

    for (auto entry : m_groupVoice) {
        yaml::Node& gv = groupVoiceList.push_back();
        entry.getYaml(gv);
        //LogDebugEx(LOG_HOST, "TalkgroupRulesLookup::save()", "Added TGID %s to yaml TG list", gv["name"].as<std::string>().c_str());
    }

    //LogDebugEx(LOG_HOST, "TalkgroupRulesLookup::save()", "Got final GroupVoiceList YAML size of %u", groupVoiceList.size());
    
    // Set the new rules
    newRules["groupVoice"] = groupVoiceList;

    // Make sure we actually did stuff right
    if (newRules["groupVoice"].size() != m_groupVoice.size()) {
        LogError(LOG_HOST, "Generated YAML node for group lists did not match loaded group size! (%u != %u)", newRules["groupVoice"].size(), m_groupVoice.size());
        return false;
    }

    try {
        LogMessage(LOG_HOST, "Saving talkgroup rules file to %s", m_rulesFile.c_str());
        yaml::Serialize(newRules, m_rulesFile.c_str());
        LogDebug(LOG_HOST, "Saved TGID config file to %s", m_rulesFile.c_str());
    }
    catch (yaml::OperationException const& e) {
        LogError(LOG_HOST, "Cannot open the talkgroup rules lookup file - %s (%s)", m_rulesFile.c_str(), e.message());
        return false;
    }

    return true;
}
