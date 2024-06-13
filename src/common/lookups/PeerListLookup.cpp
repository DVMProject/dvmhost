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
/// <param name="listFile">Full-path to the list file.</param>
/// <param name="mode">Mode to operate in (WHITELIST or BLACKLIST).</param>
/// <param name="enabled">Flag indicating if the lookup is enabled.</param>
PeerListLookup::PeerListLookup(const std::string& listFile, Mode mode, uint32_t reloadTime, bool enabled)
    : LookupTable(listFile, reloadTime), m_mode(mode), m_enabled(enabled)
{
    /* stub */
}

/// <summary>
/// Get the list of peers in the table.
/// </summary>
std::vector<uint32_t> PeerListLookup::getPeerList() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_list;
}

/// <summary>
/// Clears all entries from the list.
/// </summary>
void PeerListLookup::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_list.clear();
}

/// <summary>
/// Adds a new entry to the list.
/// </summary>
/// <param name="peerId">Unique peer ID to add.</param>
void PeerListLookup::addEntry(uint32_t peerId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (std::find(m_list.begin(), m_list.end(), peerId) == m_list.end()) {
        m_list.push_back(peerId);
    }
}

/// <summary>
/// Removes an existing entry from the list.
/// </summary>
/// <param name="peerId">Unique peer ID to remove.</param>
void PeerListLookup::removeEntry(uint32_t peerId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_list.erase(std::remove(m_list.begin(), m_list.end(), peerId), m_list.end());
}

/// <summary>
/// Checks if a peer ID is in the list.
/// </summary>
/// <param name="peerId">Unique peer ID to check.</param>
/// <returns>True if the peer ID is in the list, otherwise false.</returns>
bool PeerListLookup::isPeerInList(uint32_t peerId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    bool found = std::find(m_list.begin(), m_list.end(), peerId) != m_list.end();

    return found;
}

/// <summary>
/// Checks if a peer ID is allowed based on the mode and enabled flag.
/// </summary>
/// <param name="peerId">Unique peer ID to check.</param>
/// <returns>True if the peer ID is allowed, otherwise false.</returns>
bool PeerListLookup::isPeerAllowed(uint32_t peerId) const
{
    if (!m_enabled) {
        return true; // if not enabled, allow all peers
    }

    bool allowed = false;
    if (m_mode == WHITELIST) {
        allowed = isPeerInList(peerId);
    } else if (m_mode == BLACKLIST) {
        allowed = !isPeerInList(peerId);
    }

    return allowed;
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
PeerListLookup::Mode PeerListLookup::getMode() const {
    return m_mode;
}

/// <summary>
/// Sets whether the lookup is enabled.
/// </summary>
/// <param name="enabled">Flag indicating if the lookup is enabled.</param>
void PeerListLookup::setEnabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enabled = enabled;
}

/// <summary>
/// Finds a table entry in this lookup table.
/// </summary>
/// <param name="id">Unique identifier for table entry.</param>
/// <returns>Table entry.</returns>
uint32_t PeerListLookup::find(uint32_t id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (isPeerInList(id)) {
        return id;
    } else {
        return 0;
    }
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
        LogError(LOG_NET, "List file is empty");
        return false;
    }

    std::ifstream file(m_filename, std::ifstream::in);
    if (file.fail()) {
        LogError("Cannot open the white/blacklist file: %s", m_filename.c_str());
        return false;
    }

    m_list.clear();
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string line;
    while (std::getline(file, line)) {
        if (line.length() > 0) {
            if (line.at(0) == '#') {
                continue;
            }

            uint32_t peerId = ::strtoul(line.c_str(), nullptr, 10);
            if (peerId != 0) {
                m_list.push_back(peerId);
                LogDebug(LOG_HOST, "Loaded peer ID %u into list from file %s", peerId, m_filename.c_str());
            }
        }
    }

    file.close();

    LogInfoEx(LOG_HOST, "Loaded %lu peers into list", m_list.size());
    return !m_list.empty();
}

/// <summary>
/// Saves the table to the passed lookup table file.
/// </summary>
/// <returns>True, if lookup table was saved, otherwise false.</returns>
bool PeerListLookup::save()
{
    if (m_filename.empty()) {
        LogError(LOG_NET, "List file is empty");
        return false;
    }

    std::ofstream file(m_filename, std::ofstream::out);
    if (file.fail()) {
        LogError("Cannot open the file: %s", m_filename.c_str());
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& peerId : m_list) {
        file << peerId << "\n";
    }

    file.close();

    LogInfoEx(LOG_HOST, "Saved %lu peers to list", m_list.size());
    return true;
}