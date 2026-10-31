// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2022,2025-2026 Bryan Biedenkapp, N2PLL
 *  Copyright (c) 2024 Patrick McDonnell, W3AXL
 *
 */
#include "lookups/RadioIdLookup.h"
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

std::mutex RadioIdLookup::s_mutex;
bool RadioIdLookup::s_locked = false;

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

/* Initializes a new instance of the RadioIdLookup class. */

RadioIdLookup::RadioIdLookup(const std::string& filename, uint32_t reloadTime, bool ridAcl, bool verbose) : LookupTable(filename, reloadTime),
    m_acl(ridAcl),
    m_verbose(verbose)
{
    /* stub */
}

/* Clears all entries from the lookup table. */

void RadioIdLookup::clear()
{
    __LOCK_TABLE();

    m_table.clear();

    __UNLOCK_TABLE();
}

/* Toggles the specified radio ID enabled or disabled. */

void RadioIdLookup::toggleEntry(uint32_t id, bool enabled)
{
    RadioId rid = find(id);
    addEntry(id, enabled, rid.radioAlias(), rid.radioIPAddress(), rid.canRequestKeys(), rid.canRekey(), rid.allowedKIds());
}

/* Adds a new entry to the lookup table by the specified unique ID. */

void RadioIdLookup::addEntry(uint32_t id, bool enabled, const std::string& alias, const std::string& ipAddress,
    bool canRequestKeys, bool canRekey, const std::vector<uint16_t>& allowedKIds)
{
    if ((id == p25::defines::WUID_ALL) || (id == p25::defines::WUID_FNE)) {
        return;
    }

    RadioId entry = RadioId(enabled, false, alias, ipAddress, canRequestKeys, canRekey, allowedKIds);

    __LOCK_TABLE();

    try {
        RadioId _entry = m_table.at(id);
        // if any configured parameter doesn't match, update the entry
        if (_entry.radioEnabled() != enabled || _entry.radioAlias() != alias ||
            _entry.radioIPAddress() != ipAddress || _entry.canRequestKeys() != canRequestKeys ||
            _entry.canRekey() != canRekey || _entry.allowedKIds() != allowedKIds) {
            //LogDebug(LOG_HOST, "Updating existing RID %d (%s) in ACL", id, alias.c_str());
            _entry = RadioId(enabled, false, alias, ipAddress, canRequestKeys, canRekey, allowedKIds);
            m_table[id] = _entry;
        } else {
            //LogDebug(LOG_HOST, "No changes made to RID %d (%s) in ACL", id, alias.c_str());
        }
    } catch (...) {
        //LogDebug(LOG_HOST, "Adding new RID %d (%s) to ACL", id, alias.c_str());
        m_table[id] = entry;
    }

    __UNLOCK_TABLE();
}

/* Erases an existing entry from the lookup table by the specified unique ID. */

void RadioIdLookup::eraseEntry(uint32_t id)
{
    __LOCK_TABLE();

    try {
        RadioId entry = m_table.at(id); // this value will get discarded
        (void)entry;                    // but some variants of C++ mark the unordered_map<>::at as nodiscard
        m_table.erase(id);
    }
    catch (...) {
        /* stub */
    }

    __UNLOCK_TABLE();
}

/* Finds a table entry in this lookup table. */

RadioId RadioIdLookup::find(uint32_t id)
{
    RadioId entry;

    if ((id == p25::defines::WUID_ALL) || (id == p25::defines::WUID_FNE)) {
        return RadioId(true, false);
    }

    __SPINLOCK();

    try {
        entry = m_table.at(id);
    } catch (...) {
        entry = RadioId(false, true);
    }

    return entry;
}

/* Saves loaded talkgroup rules. */

void RadioIdLookup::commit(bool quiet)
{
    save(quiet);
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
                LogError(LOG_HOST, "Invalid entry in radio ID lookup table - %s", line.c_str());
                continue;
            }

            // parse tokenized line
            uint32_t id = ::atoi(parsed[0].c_str());
            bool radioEnabled = ::atoi(parsed[1].c_str()) == 1;
            std::string alias = "";
            std::string ipAddress = "";
            bool canRequestKeys = false;
            bool canRekey = false;
            std::vector<uint16_t> allowedKIds;

            // check for an optional alias field
            if (parsed.size() >= 3) {
                alias = parsed[2];
            }

            // check for an optional IP address field
            if (parsed.size() >= 4) {
                ipAddress = parsed[3];
            }

            // check for optional key-request permission
            if (parsed.size() >= 5) {
                canRequestKeys = ::atoi(parsed[4].c_str()) == 1;
            }

            // check for optional rekey permission
            if (parsed.size() >= 6) {
                canRekey = ::atoi(parsed[5].c_str()) == 1;
            }

            // check for optional allowed key IDs list (pipe-delimited)
            if (parsed.size() >= 7) {
                allowedKIds = parseKIdList(parsed[6]);
            }

            m_table[id] = RadioId(radioEnabled, false, alias, ipAddress, canRequestKeys, canRekey, allowedKIds);

            if (m_verbose) {
                std::string kIdList;
                if (allowedKIds.empty()) {
                    kIdList = "ALL";
                } else {
                    kIdList = serializeKIdList(allowedKIds);
                }

                LogInfoEx(LOG_HOST, "Radio NAME: %s RID: %u ENABLED: %u IPADDR: %s CANREQKEYS: %u CANREKEY: %u ALLOWEDKIDS: %s",
                    alias.c_str(), id, radioEnabled, ipAddress.c_str(), canRequestKeys, canRekey, kIdList.c_str());
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

    LogInfoEx(LOG_HOST, "Loaded %lu entries into radio ID lookup table", size);

    return true;
}

/* Saves the table to the passed lookup table file. */

bool RadioIdLookup::save(bool quiet)
{
    if (m_filename.empty()) {
        return false;
    }

    std::ofstream file (m_filename, std::ofstream::out);
    if (file.fail()) {
        LogError(LOG_HOST, "Cannot open the radio ID lookup file - %s", m_filename.c_str());
        return false;
    }

    if (!quiet)
        LogInfoEx(LOG_HOST, "Saving RID lookup file to %s", m_filename.c_str());

    // Counter for lines written
    unsigned int lines = 0;

    std::lock_guard<std::mutex> lock(s_mutex);

    // String for writing
    std::string line;

    // iterate over each entry in the RID lookup and write it to the open file
    for (auto& entry: m_table) {
        // get the parameters
        uint32_t rid = entry.first;
        bool enabled = entry.second.radioEnabled();
        std::string alias = entry.second.radioAlias();
        std::string ipAddress = entry.second.radioIPAddress();
        bool canRequestKeys = entry.second.canRequestKeys();
        bool canRekey = entry.second.canRekey();
        std::string allowedKIds = serializeKIdList(entry.second.allowedKIds());

        // format into a string
        line = std::to_string(rid) + "," + std::to_string(enabled) + ",";
        line += alias;
        line += ",";
        line += ipAddress;
        line += ",";
        line += std::to_string(canRequestKeys);
        line += ",";
        line += std::to_string(canRekey);
        line += ",";
        line += allowedKIds;
        line += ",";

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

/* Parses a string of KIDs from the lookup file into a vector of uint16_t KIDs. */

std::vector<uint16_t> RadioIdLookup::parseKIdList(const std::string& input)
{
    std::vector<uint16_t> kids;

    if (input.empty()) {
        return kids;
    }

    // tokenize the input string by pipe delimiter and parse each token as a hex value for a KID, ensuring no 
    // duplicates and that each KID is in the valid range of 0x0000 to 0xFFFF
    std::stringstream ss(input);
    std::string token;
    while (std::getline(ss, token, '|')) {
        if (token.empty()) {
            continue;
        }

        // parse token as hex value for KID
        uint32_t value = (uint32_t)::strtoul(token.c_str(), nullptr, 16);
        if (value > 0xFFFFU) {
            continue;
        }

        uint16_t kid = (uint16_t)value;

        // check for duplicates before adding to the list
        if (std::find(kids.begin(), kids.end(), kid) == kids.end()) {
            kids.push_back(kid);
        }
    }

    return kids;
}

/* Serializes a list of KIDs into a string for storage in the lookup file. */

std::string RadioIdLookup::serializeKIdList(const std::vector<uint16_t>& kids)
{
    if (kids.empty()) {
        return "";
    }

    // serialize the list of KIDs into a pipe-delimited string
    std::stringstream ss;
    for (size_t i = 0U; i < kids.size(); i++) {
        ss << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << kids[i];
        if (i + 1U < kids.size()) {
            ss << "|";
        }
    }

    return ss.str();
}
