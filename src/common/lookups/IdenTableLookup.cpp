// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2018-2022,2024 Bryan Biedenkapp, N2PLL
 *  Copyright (c) 2024 Patrick McDonnell, W3AXL
 *
 */
#include "lookups/IdenTableLookup.h"
#include "Log.h"

using namespace lookups;

#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::mutex IdenTableLookup::m_mutex;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the IdenTableLookup class. */

IdenTableLookup::IdenTableLookup(const std::string& filename, uint32_t reloadTime) : LookupTable(filename, reloadTime)
{
    /* stub */
}

/* Clears all entries from the lookup table. */

void IdenTableLookup::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_table.clear();
}

/* Finds a table entry in this lookup table. */

IdenTable IdenTableLookup::find(uint32_t id)
{
    IdenTable entry;

    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        entry = m_table.at(id);
    } catch (...) {
        /* stub */
    }

    float chBandwidthKhz = entry.chBandwidthKhz();
    if (chBandwidthKhz == 0.0F)
        chBandwidthKhz = 12.5F;
    float chSpaceKhz = entry.chSpaceKhz();
    if (chSpaceKhz < 0.125F)    // clamp to 125Hz
        chSpaceKhz = 0.125F;
    if (chSpaceKhz > 125000.0F)   // clamp to 125KHz
        chSpaceKhz = 125000.0F;

    return IdenTable(entry.channelId(), entry.baseFrequency(), chSpaceKhz, entry.txOffsetMhz(), chBandwidthKhz);
}

/* Returns the list of entries in this lookup table. */

std::vector<IdenTable> IdenTableLookup::list()
{
    std::vector<IdenTable> list = std::vector<IdenTable>();
    if (m_table.size() > 0) {
        for (auto entry : m_table) {
            list.push_back(entry.second);
        }
    }

    return list;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Loads the table from the passed lookup table file. */

bool IdenTableLookup::load()
{
    if (m_filename.empty()) {
        return false;
    }

    std::ifstream file (m_filename, std::ifstream::in);
    if (file.fail()) {
        LogError(LOG_HOST, "Cannot open the identity table lookup file - %s", m_filename.c_str());
        return false;
    }

    // clear table
    clear();

    std::lock_guard<std::mutex> lock(m_mutex);

    // read lines from file
    std::string line;
    while (std::getline(file, line)) {
        if (line.length() > 0) {
            if (line.at(0) == '#')
                continue;

            // tokenize line
            std::string next;
            std::vector<std::string> parsed;
            char delim = ',';

            for (auto it = line.begin(); it != line.end(); it++) {
                if (*it == delim) {
                    if (!next.empty()) {
                        parsed.push_back(next);
                        next.clear();
                    }
                }
                else
                    next += *it;
            }
            if (!next.empty())
                parsed.push_back(next);

            // ensure we have at least 5 fields
            if (parsed.size() < 5) {
                LogError(LOG_HOST, "Invalid entry in identity table lookup file - %s", line.c_str());
                continue;
            }

            // parse tokenized line
            uint8_t channelId = (uint8_t)::atoi(parsed[0].c_str());
            uint32_t baseFrequency = (uint32_t)::atoi(parsed[1].c_str());
            float chSpaceKhz = float(::atof(parsed[2].c_str()));
            float txOffsetMhz = float(::atof(parsed[3].c_str()));
            float chBandwidthKhz = float(::atof(parsed[4].c_str()));

            if (chSpaceKhz == 0.0F)
                chSpaceKhz = chBandwidthKhz / 2;
            if (chSpaceKhz < 0.125F)    // clamp to 125 Hz
                chSpaceKhz = 0.125F;
            if (chSpaceKhz > 125000.0F)   // clamp to 125 kHz
                chSpaceKhz = 125000.0F;

            IdenTable entry = IdenTable(channelId, baseFrequency, chSpaceKhz, txOffsetMhz, chBandwidthKhz);

            LogMessage(LOG_HOST, "Channel Id %u: BaseFrequency = %uHz, TXOffsetMhz = %fMHz, BandwidthKhz = %fKHz, SpaceKhz = %fKHz",
                entry.channelId(), entry.baseFrequency(), entry.txOffsetMhz(), entry.chBandwidthKhz(), entry.chSpaceKhz());

            m_table[channelId] = entry;
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

bool IdenTableLookup::save()
{
    return false;
}