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
*   Copyright (C) 2017-2022,2024 Bryan Biedenkapp, N2PLL
*   Copyright (c) 2024 Patrick McDonnell, W3AXL
*   Copyright (c) 2024 Caleb, KO4UYJ
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

/// <summary>
/// Initializes a new instance of the PeerListLookup class.
/// </summary>
/// <param name="filename">Full-path to the list file.</param>
/// <param name="mode">Mode to operate in (WHITELIST or BLACKLIST).</param>
/// <param name="peerAcl">Flag indicating if the lookup is enabled.</param>
PeerListLookup::PeerListLookup(const std::string& filename, Mode mode, uint32_t reloadTime, bool peerAcl) : LookupTable(filename, reloadTime),
    m_acl(peerAcl), m_mode(mode)
{
    /* stub */
}

/// <summary>
/// Clears all entries from the list.
/// </summary>
void PeerListLookup::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_table.clear();
}

/// <summary>
/// Adds a new entry to the list.
/// </summary>
/// <param name="peerId">Unique peer ID to add.</param>
/// <param name="password"></param>
void PeerListLookup::addEntry(uint32_t id, const std::string& password)
{
    PeerId entry = PeerId(id, password, false);

    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        PeerId _entry = m_table.at(id);
        // if either the alias or the enabled flag doesn't match, update the entry
        if (_entry.peerId() == id) {
            _entry = PeerId(id, password, false);
            m_table[id] = _entry;
        }
    } catch (...) {
        m_table[id] = entry;
    }
}

/// <summary>
/// Removes an existing entry from the list.
/// </summary>
/// <param name="peerId">Unique peer ID to remove.</param>
void PeerListLookup::eraseEntry(uint32_t id)
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
PeerId PeerListLookup::find(uint32_t id)
{
    PeerId entry;

    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        entry = m_table.at(id);
    } catch (...) {
        entry = PeerId(0U, "", true);
    }

    return entry;
}

/// <summary>
/// Commit the table.
/// </summary>
/// <param name="mode">The mode to set.</param>
void PeerListLookup::commit()
{
    save();
}

/// <summary>
/// Gets whether the lookup is enabled.
/// </summary>
bool PeerListLookup::getACL() const
{
    return m_acl;
}

/// <summary>
/// Checks if a peer ID is in the list.
/// </summary>
/// <param name="id">Unique peer ID to check.</param>
/// <returns>True if the peer ID is in the list, otherwise false.</returns>
bool PeerListLookup::isPeerInList(uint32_t id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_table.find(id) != m_table.end()) {
        return true;
    }

    return false;
}

/// <summary>
/// Checks if a peer ID is allowed based on the mode and enabled flag.
/// </summary>
/// <param name="id">Unique peer ID to check.</param>
/// <returns>True if the peer ID is allowed, otherwise false.</returns>
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

/// <summary>
/// Sets the mode to either WHITELIST or BLACKLIST.
/// </summary>
/// <param name="mode">The mode to set.</param>
void PeerListLookup::setMode(Mode mode)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mode = mode;
}

/// <summary>
/// Gets the current mode.
/// </summary>
PeerListLookup::Mode PeerListLookup::getMode() const 
{
    return m_mode;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Loads the table from the passed lookup table file.
/// </summary>
/// <returns>True, if lookup table was loaded, otherwise false.</returns>
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

            // Check for an optional alias field
            if (parsed.size() >= 2) {
                m_table[id] = PeerId(id, parsed[1], false);
                LogDebug(LOG_HOST, "Loaded peer ID %u into peer ID lookup table, using unique peer password", id);
            } else {
                m_table[id] = PeerId(id, "", false);
                LogDebug(LOG_HOST, "Loaded peer ID %u into peer ID lookup table, using master password", id);
            }
        }
    }

    file.close();

    size_t size = m_table.size();
    if (size == 0U)
        return false;

    LogInfoEx(LOG_HOST, "Loaded %lu peers into list", size);
    return true;
}

/// <summary>
/// Saves the table to the passed lookup table file.
/// </summary>
/// <returns>True, if lookup table was saved, otherwise false.</returns>
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