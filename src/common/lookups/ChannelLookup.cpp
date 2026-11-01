// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024-2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "lookups/ChannelLookup.h"
#include "Log.h"

#include <algorithm>

using namespace lookups;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the ChannelLookup class. */

ChannelLookup::ChannelLookup() :
    m_rfChMutex(),
    m_idxToCh(),
    m_chToIdx(),
    m_freeBits(),
    m_rfChDataTable()
{
    m_idxToCh.clear();
    m_chToIdx.clear();
    m_freeBits.clear();
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

/* Helper to get first available channel number. */

uint32_t ChannelLookup::getFirstRFChannel() const
{
    std::lock_guard<std::mutex> lock(m_rfChMutex);

    uint32_t idx = 0U;
    if (!findFirstClearBit(idx) || idx >= m_idxToCh.size()) {
        return 0U;
    }

    return m_idxToCh[idx];
}

/* Helper to atomically take first available RF channel. */

bool ChannelLookup::takeFirstRFChannel(uint32_t& chNo)
{
    chNo = 0U;

    std::lock_guard<std::mutex> lock(m_rfChMutex);

    uint32_t idx = 0U;
    if (!findFirstClearBit(idx) || idx >= m_idxToCh.size()) {
        return false;
    }

    setBit(idx);
    chNo = m_idxToCh[idx];

    return true;
}

/* Helper to get the count of RF channels. */

uint8_t ChannelLookup::rfChSize() const
{
    std::lock_guard<std::mutex> lock(m_rfChMutex);

    uint32_t count = 0U;
    for (uint32_t idx = 0U; idx < m_idxToCh.size(); ++idx) {
        count++;
    }

    if (count > 255U) {
        return 255U;
    }

    return (uint8_t)count;
}

/* Helper to get RF channels table. */

std::vector<uint32_t> ChannelLookup::rfChTable() const
{
    std::lock_guard<std::mutex> lock(m_rfChMutex);

    std::vector<uint32_t> channels;
    channels.reserve(m_idxToCh.size());

    for (uint32_t idx = 0U; idx < m_idxToCh.size(); ++idx) {
        channels.push_back(m_idxToCh[idx]);
    }

    return channels;
}

/* Helper to initialize a RF channel. */

bool ChannelLookup::initializeRFCh(uint32_t chNo)
{ 
    if (chNo == 0U) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_rfChMutex);

    // check if channel already exists, if not add it and mark as free (clear bit)
    auto it = m_chToIdx.find(chNo);
    if (it == m_chToIdx.end()) {
        uint32_t idx = (uint32_t)m_idxToCh.size();
        m_chToIdx[chNo] = idx;
        m_idxToCh.push_back(chNo);
        ensureBitCapacity(idx);
        clearBit(idx);
        return true;
    }

    return false;
}

/* Helper to mark a RF channel allocated for use. */

bool ChannelLookup::allocRFCh(uint32_t chNo)
{ 
    if (chNo == 0U) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_rfChMutex);

    // check if channel already exists, if not add it and mark allocated (set bit)
    auto it = m_chToIdx.find(chNo);
    if (it == m_chToIdx.end()) {
        uint32_t idx = (uint32_t)m_idxToCh.size();
        m_chToIdx[chNo] = idx;
        m_idxToCh.push_back(chNo);
        ensureBitCapacity(idx);
        setBit(idx);
        return true;
    }

    // channel exists, set bit if currently free
    uint32_t idx = it->second;
    if (!isBitSet(idx)) {
        setBit(idx);
        return true;
    }

    return false;
}

/* Helper to mark a RF channel as free for use. */

bool ChannelLookup::freeRFCh(uint32_t chNo)
{
    if (chNo == 0U) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_rfChMutex);

    // check if channel exists and bit is set (allocated), then clear to free
    auto it = m_chToIdx.find(chNo);
    if (it == m_chToIdx.end()) {
        return false;
    }

    // channel exists, clear bit if allocated
    uint32_t idx = it->second;
    if (!isBitSet(idx)) {
        return false;
    }

    clearBit(idx);

    return true;
}

/* Helper to determine if there are any RF channels available. */

bool ChannelLookup::isRFChAvailable() const
{
    std::lock_guard<std::mutex> lock(m_rfChMutex);

    for (uint32_t idx = 0U; idx < m_idxToCh.size(); ++idx) {
        if (!isBitSet(idx)) {
            return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Ensures bitset has enough words for the given index. */

void ChannelLookup::ensureBitCapacity(uint32_t idx)
{
    size_t wordIdx = (size_t)(idx / BITS_PER_WORD);
    if (wordIdx >= m_freeBits.size()) {
        m_freeBits.resize(wordIdx + 1U, 0ULL);
    }
}

/* Determines if the bit for index is currently set. */

bool ChannelLookup::isBitSet(uint32_t idx) const
{
    size_t wordIdx = (size_t)(idx / BITS_PER_WORD);
    if (wordIdx >= m_freeBits.size()) {
        return false;
    }

    uint32_t bit = idx % BITS_PER_WORD;
    return ((m_freeBits[wordIdx] >> bit) & 1ULL) != 0ULL;
}

/* Sets the bit for index. */

void ChannelLookup::setBit(uint32_t idx)
{
    ensureBitCapacity(idx);
    size_t wordIdx = (size_t)(idx / BITS_PER_WORD);
    uint32_t bit = idx % BITS_PER_WORD;
    m_freeBits[wordIdx] |= (1ULL << bit);
}

/* Clears the bit for index. */

void ChannelLookup::clearBit(uint32_t idx)
{
    size_t wordIdx = (size_t)(idx / BITS_PER_WORD);
    if (wordIdx >= m_freeBits.size()) {
        return;
    }

    uint32_t bit = idx % BITS_PER_WORD;
    m_freeBits[wordIdx] &= ~(1ULL << bit);
}

/* Finds the first available (clear) channel bit index. */

bool ChannelLookup::findFirstClearBit(uint32_t& idx) const
{
    for (uint32_t candidateIdx = 0U; candidateIdx < m_idxToCh.size(); ++candidateIdx) {
        if (!isBitSet(candidateIdx)) {
            idx = candidateIdx;
            return true;
        }
    }

    return false;
}
