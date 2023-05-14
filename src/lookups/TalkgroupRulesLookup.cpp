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
#include "lookups/TalkgroupRulesLookup.h"
#include "Log.h"
#include "Timer.h"

using namespace lookups;

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>
#include <fstream>

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
    m_acl(false),
    m_groupHangTime(5U),
    m_sendTalkgroups(false),
    m_groupVoice()
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the TalkgroupRulesLookup class.
/// </summary>
TalkgroupRulesLookup::~TalkgroupRulesLookup()
{
    /* stub */
}

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
/// Flag indicating whether talkgroup ID access control is enabled or not.
/// </summary>
/// <returns>True, if talkgroup ID access control is enabled, otherwise false.</returns>
bool TalkgroupRulesLookup::getACL()
{
    return m_acl;
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
            LogError(LOG_HOST, "Cannot open the talkgroup rules lookup file - %s", m_rulesFile.c_str());
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
        m_groupHangTime = m_rules["groupHangTime"].as<uint32_t>(5U);
        m_sendTalkgroups = m_rules["sendTalkgroups"].as<bool>(false);

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
            bool affiliated = groupVoice.config().affiliated();
            bool parrot = groupVoice.config().parrot();

            uint32_t incCount = groupVoice.config().inclusion().size();
            uint32_t excCount = groupVoice.config().exclusion().size();

            if (incCount > 0 && excCount > 0) {
                ::LogWarning(LOG_HOST, "Talkgroup (%s) defines both inclusions and exclusions! Inclusions take precedence and exclusions will be ignored.", groupName.c_str());
            }

            ::LogInfoEx(LOG_HOST, "Talkgroup NAME: %s SRC_TGID: %u SRC_TS: %u ACTIVE: %u AFFILIATED: %u PARROT: %u INCLUSIONS: %u EXCLUSIONS: %u", groupName.c_str(), tgId, tgSlot, active, affiliated, parrot, incCount, excCount);
        }
    }
    m_mutex.unlock();

    size_t size = m_groupVoice.size();
    if (size == 0U)
        return false;

    LogInfoEx(LOG_HOST, "Loaded %u entries into lookup table", size);

    return true;
}
