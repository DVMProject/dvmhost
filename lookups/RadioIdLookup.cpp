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
#include "lookups/RadioIdLookup.h"
#include "p25/P25Defines.h"
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
/// Initializes a new instance of the RadioIdLookup class.
/// </summary>
/// <param name="filename">Full-path to the radio ID table file.</param>
/// <param name="reloadTime">Interval of time to reload the radio ID table.</param>
/// <param name="ridAcl">Flag indicating whether radio ID access control is enabled.</param>
RadioIdLookup::RadioIdLookup(const std::string& filename, uint32_t reloadTime, bool ridAcl) : LookupTable(filename, reloadTime), 
    m_acl(ridAcl)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the RadioIdLookup class.
/// </summary>
RadioIdLookup::~RadioIdLookup()
{
    /* stub */
}

/// <summary>
/// Toggles the specified radio ID enabled or disabled.
/// </summary>
/// <param name="id">Unique ID to toggle.</param>
/// <param name="enabled">Flag indicating if radio ID is enabled or not.</param>
void RadioIdLookup::toggleEntry(uint32_t id, bool enabled)
{
    RadioId rid = find(id);
    if (rid.radioEnabled() == false && rid.radioDefault() == true) {
        if (enabled) {
            LogMessage(LOG_HOST, "Added enabled RID %u to RID ACL table", id);
        }
        else {
            LogMessage(LOG_HOST, "Added disabled RID %u to RID ACL table", id);
        }
    }

    if (rid.radioEnabled() == false && rid.radioDefault() == false) {
        if (enabled) {
            LogMessage(LOG_HOST, "Enabled RID %u in RID ACL table", id);
        }
        else {
            LogMessage(LOG_HOST, "Disabled RID %u in RID ACL table", id);
        }
    }

    addEntry(id, enabled);
}

/// <summary>
/// Adds a new entry to the lookup table by the specified unique ID.
/// </summary>
/// <param name="id">Unique ID to add.</param>
/// <param name="enabled">Flag indicating if radio ID is enabled or not.</param>
void RadioIdLookup::addEntry(uint32_t id, bool enabled)
{
    if ((id == p25::P25_WUID_ALL) || (id == p25::P25_WUID_FNE)) {
        return;
    }

    RadioId entry = RadioId(enabled, false);

    m_mutex.lock();
    {
        try {
            RadioId _entry = m_table.at(id);

            // if the enabled value doesn't match -- override with the intended
            if (_entry.radioEnabled() != enabled) {
                _entry = RadioId(enabled, false);
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
RadioId RadioIdLookup::find(uint32_t id)
{
    RadioId entry;

    if ((id == p25::P25_WUID_ALL) || (id == p25::P25_WUID_FNE)) {
        return RadioId(true, false);
    }

    m_mutex.lock();
    {
        try {
            entry = m_table.at(id);
        } catch (...) {
            entry = RadioId(false, true);
        }
    }
    m_mutex.unlock();

    return entry;
}

/// <summary>
/// Flag indicating whether radio ID access control is enabled or not.
/// </summary>
/// <returns>True, if radio ID access control is enabled, otherwise false.</returns>
bool RadioIdLookup::getACL()
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
RadioId RadioIdLookup::parse(std::string tableEntry)
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

    bool radioEnabled = ::atoi(parsed[1].c_str()) == 1;
    bool radioDefault = false;

    return RadioId(radioEnabled, radioDefault);
}
