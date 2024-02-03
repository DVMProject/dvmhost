// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2016 Jonathan Naylor, G4KLX
*   Copyright (C) 2017-2022 Bryan Biedenkapp, N2PLL
*
*/
#include "lookups/RadioIdLookup.h"
#include "p25/P25Defines.h"
#include "Log.h"

using namespace lookups;

#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>

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
/// Toggles the specified radio ID enabled or disabled.
/// </summary>
/// <param name="id">Unique ID to toggle.</param>
/// <param name="enabled">Flag indicating if radio ID is enabled or not.</param>
void RadioIdLookup::toggleEntry(uint32_t id, bool enabled)
{
    RadioId rid = find(id);
    if (!rid.radioEnabled() && rid.radioDefault()) {
        if (enabled) {
            LogMessage(LOG_HOST, "Added enabled RID %u (%s) to RID ACL table", id, rid.radioAlias().c_str());
        }
        else {
            LogMessage(LOG_HOST, "Added disabled RID %u (%s) to RID ACL table", id, rid.radioAlias().c_str());
        }
    }

    if (!rid.radioEnabled() && !rid.radioDefault()) {
        if (enabled) {
            LogMessage(LOG_HOST, "Enabled RID %u (%s) in RID ACL table", id, rid.radioAlias().c_str());
        }
        else {
            LogMessage(LOG_HOST, "Disabled RID %u (%s) in RID ACL table", id, rid.radioAlias().c_str());
        }
    }

    addEntry(id, enabled, rid.radioAlias());
}

/// <summary>
/// Adds a new entry to the lookup table by the specified unique ID.
/// </summary>
/// <param name="id">Unique ID to add.</param>
/// <param name="enabled">Flag indicating if radio ID is enabled or not.</param>
/// <param name="alias">Alias for the radio ID</param>
void RadioIdLookup::addEntry(uint32_t id, bool enabled, const std::string& alias)
{
    if ((id == p25::P25_WUID_ALL) || (id == p25::P25_WUID_FNE)) {
        return;
    }

    RadioId entry = RadioId(enabled, false, alias);

    m_mutex.lock();
    {
        try {
            RadioId _entry = m_table.at(id);

            // if the enabled value doesn't match -- override with the intended
            if (_entry.radioEnabled() != enabled) {
                _entry = RadioId(enabled, false, alias);
                m_table[id] = _entry;
            }
        } catch (...) {
            m_table[id] = entry;
        }
    }
    m_mutex.unlock();
}

/// <summary>
/// Erases an existing entry from the lookup table by the specified unique ID.
/// </summary>
/// <param name="id">Unique ID to erase.</param>
void RadioIdLookup::eraseEntry(uint32_t id)
{
    m_mutex.lock();
    {
        try {
            m_table.at(id);
            m_table.erase(id);
        } catch (...) {
            /* stub */
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
/// Saves loaded talkgroup rules.
/// </summary>
void RadioIdLookup::commit()
{
    // bryanb: TODO TODO TODO
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
/// Loads the table from the passed lookup table file.
/// </summary>
/// <returns>True, if lookup table was loaded, otherwise false.</returns>
bool RadioIdLookup::load()
{
    if (m_filename.empty()) {
        return false;
    }

    std::ifstream file (m_filename, std::ifstream::in);
    if (file.fail()) {
        LogError(LOG_HOST, "Cannot open the radio ID lookup file - %s", m_filename.c_str());
        return false;
    }

    // clear table
    clear();

    m_mutex.lock();
    {
        // read lines from file
        std::string line;
        while (std::getline(file, line)) {
            if (line.length() > 0) {
                // Skip comments with #
                if (line.at(0) == '#')
                    continue;

                // tokenize line
                std::string next;
                std::vector<std::string> parsed;
                char delim = ',';

                for (char c : line) {
                    if (c == delim) {
                        if (!next.empty()) {
                            parsed.push_back(next);
                            next.clear();
                        }
                    }
                    else
                        next += c;
                }
                if (!next.empty())
                    parsed.push_back(next);

                // parse tokenized line
                uint32_t id = ::atoi(parsed[0].c_str());
                bool radioEnabled = ::atoi(parsed[1].c_str()) == 1;
                bool radioDefault = false;

                // Check for an optional alias field
                if (parsed.size() >= 3) {
                    m_table[id] = RadioId(radioEnabled, radioDefault, parsed[2]);
                    LogDebug(LOG_HOST, "Loaded RID %u (%s) into RID lookup table", id, parsed[2].c_str());
                } else {
                    m_table[id] = RadioId(radioEnabled, radioDefault);
                    LogDebug(LOG_HOST, "Loaded RID %u into RID lookup table", id);
                }
            }
        }
    }
    m_mutex.unlock();

    file.close();

    size_t size = m_table.size();
    if (size == 0U)
        return false;

    LogInfoEx(LOG_HOST, "Loaded %u entries into lookup table", size);

    return true;
}
