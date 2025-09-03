// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "lookups/AdjSiteMapLookup.h"
#include "Log.h"
#include "Timer.h"
#include "Utils.h"

using namespace lookups;

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::mutex AdjSiteMapLookup::m_mutex;
bool AdjSiteMapLookup::m_locked = false;

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

// Lock the table.
#define __LOCK_TABLE()                          \
    std::lock_guard<std::mutex> lock(m_mutex);  \
    m_locked = true;

// Unlock the table.
#define __UNLOCK_TABLE() m_locked = false;

// Spinlock wait for table to be read unlocked.
#define __SPINLOCK()                            \
    if (m_locked) {                             \
        while (m_locked)                        \
            Thread::sleep(2U);                  \
    }

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the AdjSiteMapLookup class. */

AdjSiteMapLookup::AdjSiteMapLookup(const std::string& filename, uint32_t reloadTime) : Thread(),
    m_rulesFile(filename),
    m_reloadTime(reloadTime),
    m_rules(),
    m_stop(false),
    m_adjPeerMap()
{
    /* stub */
}

/* Finalizes a instance of the AdjSiteMapLookup class. */

AdjSiteMapLookup::~AdjSiteMapLookup() = default;

/* Thread entry point. This function is provided to run the thread for the lookup table. */

void AdjSiteMapLookup::entry()
{
    if (m_reloadTime == 0U) {
        return;
    }

    Timer timer(1U, 60U * m_reloadTime);
    timer.start();

    while (!m_stop) {
        sleep(1000U);

        timer.clock();
        if (timer.hasExpired()) {
            load();
            timer.start();
        }
    }
}

/* Stops and unloads this lookup table. */

void AdjSiteMapLookup::stop(bool noDestroy)
{
    if (m_reloadTime == 0U) {
        if (!noDestroy)
            delete this;
        return;
    }

    m_stop = true;

    wait();
}

/* Reads the lookup table from the specified lookup table file. */

bool AdjSiteMapLookup::read()
{
    bool ret = load();

    if (m_reloadTime > 0U)
        run();
    setName("host:adj-site-map");

    return ret;
}

/* Clears all entries from the lookup table. */

void AdjSiteMapLookup::clear()
{
    __LOCK_TABLE();

    m_adjPeerMap.clear();

    __UNLOCK_TABLE();
}

/* Adds a new entry to the lookup table by the specified unique ID. */

void AdjSiteMapLookup::addEntry(AdjPeerMapEntry entry)
{
    uint32_t id = entry.peerId();

    __LOCK_TABLE();

    auto it = std::find_if(m_adjPeerMap.begin(), m_adjPeerMap.end(),
        [&](AdjPeerMapEntry x)
        {
            return x.peerId() == id;
        });
    if (it != m_adjPeerMap.end()) {
        m_adjPeerMap[it - m_adjPeerMap.begin()] = entry;
    }
    else {
        m_adjPeerMap.push_back(entry);
    }

    __UNLOCK_TABLE();
}

/* Erases an existing entry from the lookup table by the specified unique ID. */

void AdjSiteMapLookup::eraseEntry(uint32_t id)
{
    __LOCK_TABLE();

    auto it = std::find_if(m_adjPeerMap.begin(), m_adjPeerMap.end(), [&](AdjPeerMapEntry x) { return x.peerId() == id; });
    if (it != m_adjPeerMap.end()) {
        m_adjPeerMap.erase(it);
    }

    __UNLOCK_TABLE();
}

/* Finds a table entry in this lookup table. */

AdjPeerMapEntry AdjSiteMapLookup::find(uint32_t id)
{
    AdjPeerMapEntry entry;

    __SPINLOCK();

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find_if(m_adjPeerMap.begin(), m_adjPeerMap.end(),
        [&](AdjPeerMapEntry x)
        {
            return x.peerId() == id;
        });
    if (it != m_adjPeerMap.end()) {
        entry = *it;
    } else {
        entry = AdjPeerMapEntry();
    }

    return entry;
}

/* Saves loaded talkgroup rules. */

bool AdjSiteMapLookup::commit()
{
    return save();
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Loads the table from the passed lookup table file. */

bool AdjSiteMapLookup::load()
{
    if (m_rulesFile.length() <= 0) {
        return false;
    }

    try {
        bool ret = yaml::Parse(m_rules, m_rulesFile.c_str());
        if (!ret) {
            LogError(LOG_HOST, "Cannot open the adjacent site map lookup file - %s - error parsing YML", m_rulesFile.c_str());
            return false;
        }
    }
    catch (yaml::OperationException const& e) {
        LogError(LOG_HOST, "Cannot open the adjacent site map lookup file - %s (%s)", m_rulesFile.c_str(), e.message());
        return false;
    }

    // clear table
    clear();

    __LOCK_TABLE();

    yaml::Node& peerList = m_rules["peers"];

    if (peerList.size() == 0U) {
        ::LogError(LOG_HOST, "No adj site map peer list defined!");
        m_locked = false;
        return false;
    }

    for (size_t i = 0; i < peerList.size(); i++) {
        AdjPeerMapEntry entry = AdjPeerMapEntry(peerList[i]);
        m_adjPeerMap.push_back(entry);
    }

    __UNLOCK_TABLE();

    size_t size = m_adjPeerMap.size();
    if (size == 0U) {
        return false;
    }

    LogInfoEx(LOG_HOST, "Loaded %lu entries into adjacent site map table", size);

    return true;
}

/* Saves the table to the passed lookup table file. */

bool AdjSiteMapLookup::save()
{
    // Make sure file is valid
    if (m_rulesFile.length() <= 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    
    // New list for our new group voice rules
    yaml::Node peerList;
    yaml::Node newRules;

    for (auto entry : m_adjPeerMap) {
        yaml::Node& gv = peerList.push_back();
        entry.getYaml(gv);
    }
    
    // Set the new rules
    newRules["peers"] = peerList;

    // Make sure we actually did stuff right
    if (newRules["peers"].size() != m_adjPeerMap.size()) {
        LogError(LOG_HOST, "Generated YAML node for group lists did not match loaded map size! (%u != %u)", newRules["peers"].size(), m_adjPeerMap.size());
        return false;
    }

    try {
        LogMessage(LOG_HOST, "Saving adjacent site map file to %s", m_rulesFile.c_str());
        yaml::Serialize(newRules, m_rulesFile.c_str());
        LogDebug(LOG_HOST, "Saved adj. site map file to %s", m_rulesFile.c_str());
    }
    catch (yaml::OperationException const& e) {
        LogError(LOG_HOST, "Cannot save the adjacent site map lookup file - %s (%s)", m_rulesFile.c_str(), e.message());
        return false;
    }

    return true;
}
