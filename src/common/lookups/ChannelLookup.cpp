// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
#include "lookups/ChannelLookup.h"
#include "Log.h"

using namespace lookups;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the ChannelLookup class.
/// </summary>
ChannelLookup::ChannelLookup() :
    m_rfChTable(),
    m_rfChDataTable()
{
    m_rfChTable.clear();
}

/// <summary>
/// Finalizes a instance of the ChannelLookup class.
/// </summary>
ChannelLookup::~ChannelLookup() = default;

/// <summary>
/// Helper to get RF channel data.
/// </summary>
/// <param name="chNo"></param>
/// <returns></returns>
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

/// <summary>
/// Helper to add a RF channel.
/// </summary>
/// <param name="chNo"></param>
/// <param name="force"></param>
/// <returns></returns>
bool ChannelLookup::addRFCh(uint32_t chNo, bool force)
{ 
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

/// <summary>
/// Helper to remove a RF channel.
/// </summary>
/// <param name="chNo"></param>
/// <returns></returns>
void ChannelLookup::removeRFCh(uint32_t chNo)
{
    if (chNo == 0U) {
        return;
    }

    auto it = std::find(m_rfChTable.begin(), m_rfChTable.end(), chNo);
    m_rfChTable.erase(it);
}
