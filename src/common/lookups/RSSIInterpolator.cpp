// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
 *
 */
#include "lookups/RSSIInterpolator.h"
#include "Log.h"

using namespace lookups;

#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the RSSIInterpolator class. */
RSSIInterpolator::RSSIInterpolator() :
    m_map()
{
    /* stub */
}

/* Finalizes a instance of the RSSIInterpolator class. */
RSSIInterpolator::~RSSIInterpolator()
{
    m_map.clear();
}

/* Loads the table from the passed RSSI mapping file. */
bool RSSIInterpolator::load(const std::string& filename)
{
    FILE* fp = ::fopen(filename.c_str(), "rt");
    if (fp == nullptr) {
        LogError(LOG_HOST, "Cannot open the RSSI data file - %s", filename.c_str());
        return false;
    }

    char buffer[100U];
    while (::fgets(buffer, 100, fp) != nullptr) {
        if (buffer[0U] == '#')
            continue;

        char* p1 = ::strtok(buffer, " \t\r\n");
        char* p2 = ::strtok(NULL, " \t\r\n");

        if (p1 != NULL && p2 != NULL) {
            uint16_t raw = uint16_t(::atoi(p1));
            int     rssi = ::atoi(p2);
            m_map.insert(std::pair<uint16_t, int>(raw, rssi));
        }
    }

    ::fclose(fp);

    LogInfoEx(LOG_HOST, "Loaded %u RSSI data mapping points from %s", m_map.size(), filename.c_str());

    return true;
}

/* Interpolates the given raw RSSI value with the lookup map. */
int RSSIInterpolator::interpolate(uint16_t val) const
{
    if (m_map.empty())
        return 0;

    std::map<uint16_t, int>::const_iterator it = m_map.lower_bound(val);

    if (it == m_map.end())
        return m_map.rbegin()->second;

    if (it == m_map.begin())
        return it->second;

    uint16_t x2 = it->first;
    int      y2 = it->second;

    --it;
    uint16_t x1 = it->first;
    int      y1 = it->second;

    float p = float(val - x1) / float(x2 - x1);

    return int((1.0F - p) * float(y1) + p * float(y2));
}
