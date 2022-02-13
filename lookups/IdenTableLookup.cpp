/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2018-2019 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include "lookups/IdenTableLookup.h"
#include "Log.h"
#include "Timer.h"

using namespace lookups;

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <vector>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the IdenTableLookup class.
/// </summary>
/// <param name="filename">Full-path to the channel identity table file.</param>
/// <param name="reloadTime">Interval of time to reload the channel identity table.</param>
IdenTableLookup::IdenTableLookup(const std::string& filename, uint32_t reloadTime) : LookupTable(filename, reloadTime)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the IdenTableLookup class.
/// </summary>
IdenTableLookup::~IdenTableLookup()
{
    /* stub */
}

/// <summary>
/// Finds a table entry in this lookup table.
/// </summary>
/// <param name="id">Unique identifier for table entry.</param>
/// <returns>Table entry.</returns>
IdenTable IdenTableLookup::find(uint32_t id)
{
    IdenTable entry;

    m_mutex.lock();
    {
        try {
            entry = m_table.at(id);
        } catch (...) {
            /* stub */
        }
    }
    m_mutex.unlock();

    float chBandwidthKhz = entry.chBandwidthKhz();
    if (chBandwidthKhz == 0.0F)
        chBandwidthKhz = 12.5F;
    float chSpaceKhz = entry.chSpaceKhz();
    if (chSpaceKhz < 2.5F)    // clamp to 2.5
        chSpaceKhz = 2.5F;
    if (chSpaceKhz > 6.25F)   // clamp to 6.25
        chSpaceKhz = 6.25F;

    return IdenTable(entry.channelId(), entry.baseFrequency(), chSpaceKhz, entry.txOffsetMhz(), chBandwidthKhz);
}

/// <summary>
/// Returns the list of entries in this lookup table.
/// </summary>
/// <returns>List of all entries in the lookup table.</returns>
std::vector<IdenTable> IdenTableLookup::list()
{
    std::vector<IdenTable> list = std::vector<IdenTable>();
    if (m_table.size() > 0) {
        for (auto it = m_table.begin(); it != m_table.end(); ++it) {
            IdenTable entry = it->second;
            list.push_back(entry);
        }
    }

    return list;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Parses a table entry from the passed comma delimited string.
/// </summary>
/// <param name="tableEntry">Comma delimited string to process into table entry.</param>
/// <returns>Table entry.</returns>
IdenTable IdenTableLookup::parse(std::string tableEntry)
{
    std::string next;
    std::vector<std::string> parsed;
    char delim = ',';

    for (auto it = tableEntry.begin(); it != tableEntry.end(); it++) {
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

    uint8_t channelId = (uint8_t)::atoi(parsed[0].c_str());
    uint32_t baseFrequency = (uint32_t)::atoi(parsed[1].c_str());
    float chSpaceKhz = float(::atof(parsed[2].c_str()));
    float txOffsetMhz = float(::atof(parsed[3].c_str()));
    float chBandwidthKhz = float(::atof(parsed[4].c_str()));

    if (chSpaceKhz == 0.0F)
        chSpaceKhz = chBandwidthKhz / 2;
    if (chSpaceKhz < 2.5F)    // clamp to 2.5
        chSpaceKhz = 2.5F;
    if (chSpaceKhz > 6.25F)   // clamp to 6.25
        chSpaceKhz = 6.25F;

    IdenTable entry = IdenTable(channelId, baseFrequency, chSpaceKhz, txOffsetMhz, chBandwidthKhz);

    LogMessage(LOG_HOST, "Channel Id %u: BaseFrequency = %uHz, TXOffsetMhz = %fMHz, BandwidthKhz = %fKHz, SpaceKhz = %fKHz",
        entry.channelId(), entry.baseFrequency(), entry.txOffsetMhz(), entry.chBandwidthKhz(), entry.chSpaceKhz());

    return entry;
}
