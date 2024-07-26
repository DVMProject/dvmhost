// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2022 Bryan Biedenkapp, N2PLL
 *  Copyright (c) 2024 Patrick McDonnell, W3AXL
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

/* Initializes a new instance of the RadioIdLookup class. */

RadioIdLookup::RadioIdLookup(const std::string& filename, uint32_t reloadTime, bool ridAcl) : LookupTable(filename, reloadTime),
    m_acl(ridAcl)
{
    /* stub */
}

/* Clears all entries from the lookup table. */

void RadioIdLookup::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_table.clear();
}

/* Toggles the specified radio ID enabled or disabled. */

void RadioIdLookup::toggleEntry(uint32_t id, bool enabled)
{
    RadioId rid = find(id);
    addEntry(id, enabled, rid.radioAlias());
}

/* Adds a new entry to the lookup table by the specified unique ID. */

void RadioIdLookup::addEntry(uint32_t id, bool enabled, const std::string& alias, const std::string& ipAddress)
{
    if ((id == p25::defines::WUID_ALL) || (id == p25::defines::WUID_FNE)) {
        return;
    }

    RadioId entry = RadioId(enabled, false, alias, ipAddress);

    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        RadioId _entry = m_table.at(id);
        // if either the alias or the enabled flag doesn't match, update the entry
        if (_entry.radioEnabled() != enabled || _entry.radioAlias() != alias) {
            //LogDebug(LOG_HOST, "Updating existing RID %d (%s) in ACL", id, alias.c_str());
            _entry = RadioId(enabled, false, alias, ipAddress);
            m_table[id] = _entry;
        } else {
            //LogDebug(LOG_HOST, "No changes made to RID %d (%s) in ACL", id, alias.c_str());
        }
    } catch (...) {
        //LogDebug(LOG_HOST, "Adding new RID %d (%s) to ACL", id, alias.c_str());
        m_table[id] = entry;
    }
}

/* Erases an existing entry from the lookup table by the specified unique ID. */

void RadioIdLookup::eraseEntry(uint32_t id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        RadioId entry = m_table.at(id); // this value will get discarded
        (void)entry;                    // but some variants of C++ mark the unordered_map<>::at as nodiscard
        m_table.erase(id);
    } catch (...) {
        /* stub */
    }
}

/* Finds a table entry in this lookup table. */

RadioId RadioIdLookup::find(uint32_t id)
{
    RadioId entry;

    if ((id == p25::defines::WUID_ALL) || (id == p25::defines::WUID_FNE)) {
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

/* Saves loaded talkgroup rules. */

void RadioIdLookup::commit()
{
    save();
}

/* Flag indicating whether radio ID access control is enabled or not. */

bool RadioIdLookup::getACL()
{
    return m_acl;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Loads the table from the passed lookup table file. */

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
            std::string alias = "";
            std::string ipAddress = "";

            // check for an optional alias field
            if (parsed.size() >= 3) {
                alias = parsed[2];
            }

            // check for an optional IP address field
            if (parsed.size() >= 4) {
                ipAddress = parsed[3];
            }

            m_table[id] = RadioId(radioEnabled, false, alias, ipAddress);
            if (alias != "") {
                LogDebug(LOG_HOST, "Loaded RID %u (%s) into RID lookup table", id, parsed[2].c_str());
            } else {
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

/* Saves the table to the passed lookup table file. */

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
        std::string ipAddress = entry.second.radioIPAddress();

        // Format into a string
        line = std::to_string(rid) + "," + std::to_string(enabled) + ",";

        // Add the alias if we have one
        if (alias.length() > 0) {
            line += alias;
            line += ",";
        }

        // Add the IP address if we have one
        if (ipAddress.length() > 0) {
            line += ipAddress;
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