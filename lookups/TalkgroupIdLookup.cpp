/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2016 by Jonathan Naylor G4KLX
*   Copyright (C) 2017-2019 by Bryan Biedenkapp N2PLL
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
#include "lookups/TalkgroupIdLookup.h"
#include "Log.h"
#include "Timer.h"

using namespace lookups;

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <vector>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the TalkgroupIdLookup class.
/// </summary>
/// <param name="filename">Full-path to the talkgroup ID table file.</param>
/// <param name="reloadTime">Interval of time to reload the talkgroup ID table.</param>
/// <param name="tidAcl">Flag indicating whether talkgroup ID access control is enabled.</param>
TalkgroupIdLookup::TalkgroupIdLookup(const std::string& filename, uint32_t reloadTime, bool tidAcl) : LookupTable(filename, reloadTime),
    m_acl(tidAcl)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the TalkgroupIdLookup class.
/// </summary>
TalkgroupIdLookup::~TalkgroupIdLookup()
{
    /* stub */
}

/// <summary>
/// Adds a new entry to the lookup table by the specified unique ID.
/// </summary>
/// <param name="id">Unique ID to add.</param>
/// <param name="slot">DMR slot this talkgroup is valid on.</param>
/// <param name="enabled">Flag indicating if talkgroup ID is enabled or not.</param>
void TalkgroupIdLookup::addEntry(uint32_t id, unsigned char slot, bool enabled)
{
    TalkgroupId entry = TalkgroupId(enabled, slot, false);

    m_mutex.lock();
    {
        try {
            TalkgroupId _entry = m_table.at(id);

            // if the enabled value doesn't match -- override with the intended
            if (_entry.tgEnabled() != enabled) {
                _entry = TalkgroupId(enabled, _entry.tgSlot(), false);
                m_table[id] = _entry;
            }
        } catch (...) {
            m_table[id] = entry;
        }
    }
    m_mutex.unlock();
}

/// <summary>
/// Finds a table entry in this lookup table.
/// </summary>
/// <param name="id">Unique identifier for table entry.</param>
/// <returns>Table entry.</returns>
TalkgroupId TalkgroupIdLookup::find(uint32_t id)
{
    TalkgroupId entry;

    m_mutex.lock();
    {
        try {
            entry = m_table.at(id);
        } catch (...) {
            entry = TalkgroupId(false, 0U, true);
        }
    }
    m_mutex.unlock();

    return entry;
}

/// <summary>
/// Flag indicating whether talkgroup ID access control is enabled or not.
/// </summary>
/// <returns>True, if talkgroup ID access control is enabled, otherwise false.</returns>
bool TalkgroupIdLookup::getACL()
{
    return m_acl;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Parses a table entry from the passed comma delimited string.
/// </summary>
/// <param name="tableEntry">Comma delimited string to process into table entry.</param>
/// <returns>Table entry.</returns>
TalkgroupId TalkgroupIdLookup::parse(std::string tableEntry)
{
    std::string next;
    std::vector<std::string> parsed;
    char delim = ',';

    for (auto it = tableEntry.begin(); it != tableEntry.end(); it++) {
        if (*it == delim) {
            if (!next.empty()) {
                parsed.push_back(next);
                next.clear();
            }
        }
        else
            next += *it;
    }
    if (!next.empty())
        parsed.push_back(next);

    bool tgEnabled = ::atoi(parsed[1].c_str()) == 1;
    uint8_t tgSlot = (uint8_t)::atoi(parsed[2].c_str());
    bool tgDefault = false;

    return TalkgroupId(tgEnabled, tgSlot, tgDefault);
}
