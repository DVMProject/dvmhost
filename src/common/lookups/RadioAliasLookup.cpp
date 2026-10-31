// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "lookups/RadioAliasLookup.h"
#include "p25/P25Defines.h"
#include "Log.h"

using namespace lookups;

#include <cstdlib>
#include <iomanip>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::mutex RadioAliasLookup::s_mutex;
bool RadioAliasLookup::s_locked = false;

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

// Lock the table.
#define __LOCK_TABLE()                          \
    std::lock_guard<std::mutex> lock(s_mutex);  \
    s_locked = true;

// Unlock the table.
#define __UNLOCK_TABLE() s_locked = false;

// Spinlock wait for table to be read unlocked.
#define __SPINLOCK()                            \
    if (s_locked) {                             \
        while (s_locked)                        \
            Thread::sleep(2U);                  \
    }

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the RadioAliasLookup class. */

RadioAliasLookup::RadioAliasLookup(const std::string& filename, uint32_t reloadTime, bool verbose) : LookupTable(filename, reloadTime),
    m_verbose(verbose)
{
    /* stub */
}

/* Clears all entries from the lookup table. */

void RadioAliasLookup::clear()
{
    __LOCK_TABLE();

    m_table.clear();

    __UNLOCK_TABLE();
}

/* Adds a new entry to the lookup table by the specified unique ID. */

void RadioAliasLookup::addEntry(uint32_t id, const std::string& alias)
{
    if ((id == p25::defines::WUID_ALL) || (id == p25::defines::WUID_FNE)) {
        return;
    }

    __LOCK_TABLE();

    m_table[id] = alias;

    __UNLOCK_TABLE();
}

/* Erases an existing entry from the lookup table by the specified unique ID. */

void RadioAliasLookup::eraseEntry(uint32_t id)
{
    __LOCK_TABLE();

    try {
        std::string entry = m_table.at(id); // this value will get discarded
        (void)entry;                        // but some variants of C++ mark the unordered_map<>::at as nodiscard
        m_table.erase(id);
    }
    catch (...) {
        /* stub */
    }

    __UNLOCK_TABLE();
}

/* Finds a table entry in this lookup table. */

std::string RadioAliasLookup::find(uint32_t id)
{
    std::string entry;

    if ((id == p25::defines::WUID_ALL) || (id == p25::defines::WUID_FNE)) {
        return ("SYSTEM");
    }

    __SPINLOCK();

    try {
        entry = m_table.at(id);
    } catch (...) {
        entry = ("UNKNOWN");
    }

    return entry;
}

/* Saves loaded talkgroup rules. */

void RadioAliasLookup::commit(bool quiet)
{
    save(quiet);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Loads the table from the passed lookup table file. */

bool RadioAliasLookup::load()
{
    if (m_filename.empty()) {
        return false;
    }

    std::ifstream file (m_filename, std::ifstream::in);
    if (file.fail()) {
        LogError(LOG_HOST, "Cannot open the radio alias lookup file - %s", m_filename.c_str());
        return false;
    }

    // clear table
    clear();

    __LOCK_TABLE();

    // read lines from file
    std::string line;
    while (std::getline(file, line)) {
        if (line.length() > 0) {
            // Skip comments with #
            if (line.at(0) == '#')
                continue;

            // tokenize line
            std::vector<std::string> parsed;
            std::stringstream ss(line);
            std::string field;
            char delim = ',';

            while (std::getline(ss, field, delim))
                parsed.push_back(field);

            // ensure we have at least 2 fields
            if (parsed.size() < 2) {
                LogError(LOG_HOST, "Invalid entry in radio alias lookup table - %s", line.c_str());
                continue;
            }

            // parse tokenized line
            uint32_t id = ::atoi(parsed[0].c_str());
            std::string alias = "";

            // check for an optional alias field
            if (parsed.size() >= 2) {
                alias = parsed[1];
            }

            m_table[id] = alias;

            if (m_verbose) {
                LogInfoEx(LOG_HOST, "Radio NAME: %s RID: %u", alias.c_str(), id);
            }
        }
    }

    file.close();
    __UNLOCK_TABLE();

    size_t size = m_table.size();
    if (size == 0U)
        return false;

    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    m_lastLoadTime = now;

    LogInfoEx(LOG_HOST, "Loaded %lu entries into radio alias lookup table", size);

    return true;
}

/* Saves the table to the passed lookup table file. */

bool RadioAliasLookup::save(bool quiet)
{
    if (m_filename.empty()) {
        return false;
    }

    std::ofstream file (m_filename, std::ofstream::out);
    if (file.fail()) {
        LogError(LOG_HOST, "Cannot open the radio alias lookup file - %s", m_filename.c_str());
        return false;
    }

    if (!quiet)
        LogInfoEx(LOG_HOST, "Saving RID alias file to %s", m_filename.c_str());

    // Counter for lines written
    unsigned int lines = 0;

    std::lock_guard<std::mutex> lock(s_mutex);

    // String for writing
    std::string line;

    // iterate over each entry in the RID lookup and write it to the open file
    for (auto& entry: m_table) {
        // get the parameters
        uint32_t rid = entry.first;
        std::string alias = entry.second;

        // format into a string
        line = std::to_string(rid) + "," + alias + ",";

        line += "\n";
        file << line;
        lines++;
    }

    file.close();

    if (lines != m_table.size())
        return false;

    if (!quiet)
        LogInfoEx(LOG_HOST, "Saved %u entries to lookup table file %s", lines, m_filename.c_str());

    return true;
}
