// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2022,2024 Bryan Biedenkapp, N2PLL
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

void PeerListLookup::addEntry(uint32_t id, const std::string& password, bool peerLink)
{
    PeerId entry = PeerId(id, password, peerLink, false);

    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        PeerId _entry = m_table.at(id);
        // if either the alias or the enabled flag doesn't match, update the entry
        if (_entry.peerId() == id) {
            _entry = PeerId(id, password, peerLink, false);
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
        entry = PeerId(0U, "", false, true);
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
            bool peerLink = false;
            if (parsed.size() >= 3)
                peerLink = ::atoi(parsed[2].c_str()) == 1;

            // Check for an optional alias field
            if (parsed.size() >= 2) {
                if (!parsed[1].empty()) {
                    m_table[id] = PeerId(id, parsed[1], peerLink, false);
                    LogDebug(LOG_HOST, "Loaded peer ID %u into peer ID lookup table, using unique peer password%s", id,
                        (peerLink) ? ", Peer-Link Enabled" : "");
                    continue;
                }
            }

            m_table[id] = PeerId(id, "", peerLink, false);
            LogDebug(LOG_HOST, "Loaded peer ID %u into peer ID lookup table, using master password%s", id,
                (peerLink) ? ", Peer-Link Enabled" : "");
        }
    }

    file.close();

    size_t size = m_table.size();
    if (size == 0U)
        return false;

    LogInfoEx(LOG_HOST, "Loaded %lu peers into list", size);
    return true;
}

/* Saves the table to the passed lookup table file. */

bool PeerListLookup::save()
{
    LogDebug(LOG_HOST, "Saving peer lookup file to %s", m_filename.c_str());

    if (m_filename.empty()) {
        return false;
    }

    std::ofstream file(m_filename, std::ofstream::out);
    if (file.fail()) {
        LogError(LOG_HOST, "Cannot open the peer ID lookup file - %s", m_filename.c_str());
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
        uint32_t peerId = entry.first;
        std::string password = entry.second.peerPassword();
        // Format into a string
        line = std::to_string(peerId) + ",";
        // Add the alias if we have one
        if (password.length() > 0) {
            line += password;
            line += ",";
        }
        bool peerLink = entry.second.peerLink();
        if (peerLink) {
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