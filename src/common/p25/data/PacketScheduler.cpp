// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/data/PacketScheduler.h"

#include <utility>

using namespace p25::data;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the PacketScheduler class. */

PacketScheduler::PacketScheduler(uint32_t maxFrames, uint32_t maxBytes) :
    m_maxFrames(maxFrames),
    m_maxBytes(maxBytes),
    m_byteCount(0U),
    m_packets()
{
    /* stub */
}

/* Enqueues a scheduled packet into the scheduler. */

uint32_t PacketScheduler::enqueue(ScheduledDataPacket&& packet)
{
    uint32_t dropped = 0U;
    uint32_t packetBytes = uint32_t(packet.userData.size());
    if (m_maxBytes != 0U && packetBytes > m_maxBytes)
        return 1U;

    while (!m_packets.empty() &&
        ((m_maxFrames != 0U && m_packets.size() >= m_maxFrames) ||
         (m_maxBytes != 0U && m_byteCount + packetBytes > m_maxBytes))) {
        pop();
        dropped++;
    }

    m_byteCount += packetBytes;
    m_packets.emplace_back(std::move(packet));
    return dropped;
}

/* Gets the next queued packet. */

ScheduledDataPacket* PacketScheduler::front()
{
    return m_packets.empty() ? nullptr : &m_packets.front();
}

/* Removes the next queued packet. */

void PacketScheduler::pop()
{
    if (m_packets.empty())
        return;

    uint32_t packetBytes = uint32_t(m_packets.front().userData.size());
    m_byteCount = packetBytes <= m_byteCount ? m_byteCount - packetBytes : 0U;
    m_packets.pop_front();
}

/* Moves the next queued packet to the back of the queue. */

void PacketScheduler::rotate()
{
    if (m_packets.size() < 2U)
        return;

    m_packets.emplace_back(std::move(m_packets.front()));
    m_packets.pop_front();
}

/* Clears all queued packets from the scheduler. */

void PacketScheduler::clear()
{
    m_packets.clear();
    m_byteCount = 0U;
}
