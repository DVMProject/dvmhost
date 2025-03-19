// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2022,2024,2025 Bryan Biedenkapp, N2PLL
 *  Copyright (c) 2024 Patrick McDonnell, W3AXL
 *  Copyright (c) 2024 Caleb, KO4UYJ
 *
 */
#include "PeerListLookup.h"
#include "Log.h"

using namespace lookups;

#include <fstream>
#include <algorithm>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::mutex PeerListLookup::m_mutex;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the PeerListLookup class. */

PeerListLookup::PeerListLookup(const std::string& filename, Mode mode, uint32_t reloadTime, bool peerAcl) : LookupTable(filename, reloadTime),
    m_acl(peerAcl), m_mode(mode)
{
    /* stub */
}

/* Clears all entries from the list. */

void PeerListLookup::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_table.clear();
}

/* Adds a new entry to the list. */

void PeerListLookup::addEntry(uint32_t id, const std::string& alias, const std::string& password, bool peerLink, bool canRequestKeys)
{
    PeerId entry = PeerId(id, alias, password, peerLink, canRequestKeys, false);

    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        PeerId _entry = m_table.at(id);
        // if either the alias or the enabled flag doesn't match, update the entry
        if (_entry.peerId() == id) {
            _entry = PeerId(id, alias, password, peerLink, canRequestKeys, false);
            m_table[id] = _entry;
        }
    } catch (...) {
        m_table[id] = entry;
    }
}

/* Removes an existing entry from the list. */

void PeerListLookup::eraseEntry(uint32_t id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        PeerId entry = m_table.at(id);  // this value will get discarded
        (void)entry;                    // but some variants of C++ mark the unordered_map<>::at as nodiscard
        m_table.erase(id);
    } catch (...) {
        /* stub */
    }
}

/* Finds a table entry in this lookup table. */

PeerId PeerListLookup::find(uint32_t id)
{
    PeerId entry;

    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        entry = m_table.at(id);
    } catch (...) {
        entry = PeerId(0U, "", "", false, false, true);
    }

    return entry;
}

/* Commit the table. */

void PeerListLookup::commit()
{
    save();
}

/* Gets whether the lookup is enabled. */

bool PeerListLookup::getACL() const
{
    return m_acl;
}

/* Checks if a peer ID is in the list. */

bool PeerListLookup::isPeerInList(uint32_t id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_table.find(id) != m_table.end()) {
        return true;
    }

    return false;
}

/* Checks if a peer ID is allowed based on the mode and enabled flag. */

bool PeerListLookup::isPeerAllowed(uint32_t id) const
{
    if (!m_acl) {
        return true; // if not enabled, allow all peers
    }

    bool allowed = false;
    if (m_mode == WHITELIST) {
        allowed = isPeerInList(id);
    } else if (m_mode == BLACKLIST) {
        allowed = !isPeerInList(id);
    }

    return allowed;
}

/* Sets the mode to either WHITELIST or BLACKLIST. */

void PeerListLookup::setMode(Mode mode)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mode = mode;
}

/* Gets the current mode. */

PeerListLookup::Mode PeerListLookup::getMode() const 
{
    return m_mode;
}

/* Gets the entire peer ID table. */

std::vector<PeerId> PeerListLookup::tableAsList() const
{
    std::vector<PeerId> ret = std::vector<PeerId>();

    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto entry : m_table) {
        ret.push_back(entry.second);
    }

    return ret;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Loads the table from the passed lookup table file. */

bool PeerListLookup::load()
{
    if (m_filename.empty()) {
        return false;
    }

    std::ifstream file(m_filename, std::ifstream::in);
    if (file.fail()) {
        LogError(LOG_HOST, "Cannot open the peer ID lookup file - %s", m_filename.c_str());
        return false;
    }

    m_table.clear();
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
                    //if (!next.empty()) {
                        parsed.push_back(next);
                        next.clear();
                    //}
                }
                else
                    next += c;
            }
            if (!next.empty())
                parsed.push_back(next);

            // parse tokenized line
            uint32_t id = ::atoi(parsed[0].c_str());

            // Parse optional alias field (at end of line to avoid breaking change with existing lists)
            std::string alias = "";
            if (parsed.size() >= 4)
                alias = parsed[3].c_str();

            // Parse peer link flag
            bool peerLink = false;
            if (parsed.size() >= 3)
                peerLink = ::atoi(parsed[2].c_str()) == 1;

            // Parse can request keys flag
            bool canRequestKeys = false;
            if (parsed.size() >= 5)
                canRequestKeys = ::atoi(parsed[4].c_str()) == 1;

            // Parse optional password
            std::string password = "";
            if (parsed.size() >= 2)
                password = parsed[1].c_str();

            // Load into table
            m_table[id] = PeerId(id, alias, password, peerLink, canRequestKeys, false);

            // Log depending on what was loaded
            LogMessage(LOG_HOST, "Loaded peer ID %u%s into peer ID lookup table, %s%s", id,
                (!alias.empty() ? (" (" + alias + ")").c_str() : ""),
                (!password.empty() ? "using unique peer password" : "using master password"),
                (peerLink) ? ", Peer-Link Enabled" : "",
                (canRequestKeys) ? ", Can Request Keys" : "");
        }
    }

    file.close();

    size_t size = m_table.size();
    if (size == 0U)
        return false;

    LogInfoEx(LOG_HOST, "Loaded %lu entries into peer list lookup table", size);
    return true;
}

/* Saves the table to the passed lookup table file. */

bool PeerListLookup::save()
{
    if (m_filename.empty()) {
        return false;
    }

    std::ofstream file(m_filename, std::ofstream::out);
    if (file.fail()) {
        LogError(LOG_HOST, "Cannot open the peer ID lookup file - %s", m_filename.c_str());
        return false;
    }

    LogMessage(LOG_HOST, "Saving peer lookup file to %s", m_filename.c_str());

    // Counter for lines written
    unsigned int lines = 0;

    std::lock_guard<std::mutex> lock(m_mutex);

    // String for writing
    std::string line;
    // iterate over each entry in the RID lookup and write it to the open file
    for (auto& entry: m_table) {
        // Get the parameters
        uint32_t peerId = entry.first;
        std::string alias = entry.second.peerAlias();
        std::string password = entry.second.peerPassword();
        // Format into a string
        line = std::to_string(peerId) + ",";
        // Add the password if we have one
        if (password.length() > 0) {
            line += password;
        }
        line += ",";
        // Add peerLink flag
        bool peerLink = entry.second.peerLink();
        if (peerLink) {
            line += "1,";
        } else {
            line += "0,";
        }
        // Add alias if we have one
        if (alias.length() > 0) {
            line += alias;
            line += ",";
        } else {
            line += ",";
        }
        // Add canRequestKeys flag
        bool canRequestKeys = entry.second.canRequestKeys();
        if (canRequestKeys) {
            line += "1,";
        } else {
            line += "0,";
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