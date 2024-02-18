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
*   Copyright (c) 2024 Patrick McDonnell, W3AXL
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
//  Static Class Members
// ---------------------------------------------------------------------------

std::mutex RadioIdLookup::m_mutex;

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
/// Clears all entries from the lookup table.
/// </summary>
void RadioIdLookup::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_table.clear();
}

/// <summary>
/// Toggles the specified radio ID enabled or disabled.
/// </summary>
/// <param name="id">Unique ID to toggle.</param>
/// <param name="enabled">Flag indicating if radio ID is enabled or not.</param>
void RadioIdLookup::toggleEntry(uint32_t id, bool enabled)
{
    RadioId rid = find(id);
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

    std::lock_guard<std::mutex> lock(m_mutex);
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

/// <summary>
/// Erases an existing entry from the lookup table by the specified unique ID.
/// </summary>
/// <param name="id">Unique ID to erase.</param>
void RadioIdLookup::eraseEntry(uint32_t id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        m_table.at(id);
        m_table.erase(id);
    } catch (...) {
        /* stub */
    }
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

    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        entry = m_table.at(id);
    } catch (...) {
        entry = RadioId(false, true);
    }

    return entry;
}

/// <summary>
/// Saves loaded talkgroup rules.
/// </summary>
void RadioIdLookup::commit()
{
    save();
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

    std::lock_guard<std::mutex> lock(m_mutex);

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

    file.close();

    size_t size = m_table.size();
    if (size == 0U)
        return false;

    LogInfoEx(LOG_HOST, "Loaded %u entries into lookup table", size);

    return true;
}

/// <summary>
/// Saves the table to the passed lookup table file.
/// </summary>
/// <returns>True, if lookup table was saved, otherwise false.</returns>
bool RadioIdLookup::save()
{
    LogDebug(LOG_HOST, "Saving RID lookup file to %s", m_filename.c_str());

    if (m_filename.empty()) {
        return false;
    }

    std::ofstream file (m_filename, std::ofstream::out);
    if (file.fail()) {
        LogError(LOG_HOST, "Cannot open the radio ID lookup file - %s", m_filename.c_str());
        return false;
    }

    // Counter for lines written
    unsigned int lines = 0;

    std::lock_guard<std::mutex> lock(m_mutex);

    // String for writing
    std::string line;
    // iterate over each entry in the RID lookup and write it to the open file
    for (auto& entry: m_table) {
        // Get the parameters
        uint32_t rid = entry.first;
        bool enabled = entry.second.radioEnabled();
        std::string alias = entry.second.radioAlias();
        // Format into a string
        line = std::to_string(rid) + "," + std::to_string(enabled) + ",";
        // Add the alias if we have one
        if (alias.length() > 0) {
            line += alias;
            line += ",";
        }
        // Add the newline
        line += "\n";
        // Write to file
        file << line;
        // Increment
        lines++;
    }

    file.close();

    if (lines != m_table.size())
        return false;

    LogInfoEx(LOG_HOST, "Saved %u entries to lookup table file %s", lines, m_filename.c_str());

    return true;
}