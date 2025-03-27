// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "lookups/ChannelLookup.h"
#include "Log.h"

using namespace lookups;

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::mutex ChannelLookup::m_mutex;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the ChannelLookup class. */

ChannelLookup::ChannelLookup() :
    m_rfChTable(),
    m_rfChDataTable()
{
    m_rfChTable.clear();
}

/* Finalizes a instance of the ChannelLookup class. */

ChannelLookup::~ChannelLookup() = default;

/* Helper to get RF channel data. */

VoiceChData ChannelLookup::getRFChData(uint32_t chNo) const
{
    if (chNo == 0U) {
        return VoiceChData();
    }

    VoiceChData data;
    try {
        data = m_rfChDataTable.at(chNo);
    } catch (...) {
        data = VoiceChData();
    }

    return data;
}

/* Helper to add a RF channel. */

bool ChannelLookup::addRFCh(uint32_t chNo, bool force)
{ 
    std::lock_guard<std::mutex> lock(m_mutex);
    if (chNo == 0U) {
        return false;
    }

    if (force) {
        m_rfChTable.push_back(chNo);
        return true;
    }

    auto it = std::find(m_rfChTable.begin(), m_rfChTable.end(), chNo);
    if (it == m_rfChTable.end()) {
        m_rfChTable.push_back(chNo);
        return true;
    }

    return false;
}

/* Helper to remove a RF channel. */

bool ChannelLookup::removeRFCh(uint32_t chNo)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (chNo == 0U) {
        return false;
    }

    auto it = std::find(m_rfChTable.begin(), m_rfChTable.end(), chNo);
    if (it != m_rfChTable.end()) {
        m_rfChTable.erase(it);
        return true;
    }

    return false;
}
