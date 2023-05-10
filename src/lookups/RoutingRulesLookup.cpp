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
#include "lookups/RoutingRulesLookup.h"
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
/// Initializes a new instance of the RoutingRulesLookup class.
/// </summary>
/// <param name="filename">Full-path to the routing rules file.</param>
/// <param name="reloadTime">Interval of time to reload the routing rules.</param>
RoutingRulesLookup::RoutingRulesLookup(const std::string& filename, uint32_t reloadTime) : Thread(),
    m_rulesFile(filename),
    m_reloadTime(reloadTime),
    m_groupHangTime(5U),
    m_sendTalkgroups(false),
    m_groupVoice()
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the RoutingRulesLookup class.
/// </summary>
RoutingRulesLookup::~RoutingRulesLookup()
{
    /* stub */
}

/// <summary>
///
/// </summary>
void RoutingRulesLookup::entry()
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
void RoutingRulesLookup::stop()
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
bool RoutingRulesLookup::read()
{
    bool ret = load();

    if (m_reloadTime > 0U)
        run();

    return ret;
}

/// <summary>
/// Clears all entries from the lookup table.
/// </summary>
void RoutingRulesLookup::clear()
{
    m_mutex.lock();
    {
        m_groupVoice.clear();
    }
    m_mutex.unlock();
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Loads the table from the passed lookup table file.
/// </summary>
/// <returns>True, if lookup table was loaded, otherwise false.</returns>
bool RoutingRulesLookup::load()
{
    if (m_rulesFile.length() <= 0) {
        return false;
    }

    try {
        bool ret = yaml::Parse(m_rules, m_rulesFile.c_str());
        if (!ret) {
            LogError(LOG_HOST, "Cannot open the lookup file - %s", m_rulesFile.c_str());
            return false;
        }
    }
    catch (yaml::OperationException const& e) {
        LogError(LOG_HOST, "Cannot open the lookup file - %s", e.message());
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
            RoutingRuleGroupVoice groupVoice = RoutingRuleGroupVoice(groupVoiceList[i]);
            m_groupVoice.push_back(groupVoice);

            ::LogInfoEx(LOG_HOST, "Rule (%s) NAME: %s SRC_TGID: %u SRC_TS: %u ACTIVE: %u ROUTABLE: %u AFFILIATED: %u", groupVoice.name(), groupVoice.source().tgId(), groupVoice.source().tgSlot(),
                groupVoice.config().active(), groupVoice.config().routable(), groupVoice.config().affiliated());
        }
    }
    m_mutex.unlock();

    size_t size = m_groupVoice.size();
    if (size == 0U)
        return false;

    LogInfoEx(LOG_HOST, "Loaded %u entries into lookup table", size);

    return true;
}
