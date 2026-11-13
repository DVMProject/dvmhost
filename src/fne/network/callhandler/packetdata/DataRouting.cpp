// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "network/callhandler/packetdata/DataRouting.h"

using namespace network::callhandler::packetdata;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Observes a route neighbor with the given link-layer ID and IP address. */

void RouteNeighborCache::observe(uint32_t subscriberId, uint32_t ipAddress, uint64_t nowMs)
{
    if (subscriberId == 0U || ipAddress == 0U)
        return;

    RouteNeighbor& neighbor = m_neighbors[subscriberId];
    neighbor.subscriberId = subscriberId;
    neighbor.ipAddress = ipAddress;
    neighbor.lastSeen = nowMs;
}

/* Erases a route neighbor with the given link-layer ID. */

bool RouteNeighborCache::erase(uint32_t subscriberId)
{
    return m_neighbors.erase(subscriberId) > 0U;
}

/* Clears all observed route neighbors. */

void RouteNeighborCache::clear()
{
    m_neighbors.clear();
}

/* Finds a route neighbor by its link-layer ID. */

const RouteNeighbor* RouteNeighborCache::findBySubscriberId(uint32_t subscriberId) const
{
    auto it = m_neighbors.find(subscriberId);
    return it == m_neighbors.end() ? nullptr : &it->second;
}

/* Finds a route neighbor by its IP address. */

const RouteNeighbor* RouteNeighborCache::findByIPAddress(uint32_t ipAddress) const
{
    for (const auto& entry : m_neighbors) {
        if (entry.second.ipAddress == ipAddress)
            return &entry.second;
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the DataDataLocationRegistry class. */

DataLocationRegistry::DataLocationRegistry(uint64_t staleAfterMs,
    bool learnFromInbound, UnknownLocationPolicy unknownLocationPolicy) :
    m_staleAfterMs(staleAfterMs),
    m_learnFromInbound(learnFromInbound),
    m_unknownLocationPolicy(unknownLocationPolicy),
    m_conventional(),
    m_trunked(),
    m_groupPeers()
{
    /* stub */
}

/* Updates the conventional location for a given link-layer ID. */

bool DataLocationRegistry::updateConventional(uint32_t llId, const ConventionalLocation& location)
{
    if (llId == 0U || location.peerId == 0U)
        return false;

    m_conventional[llId] = location;
    return true;
}

/* Updates the trunked data route for a given link-layer ID. */

bool DataLocationRegistry::updateTrunked(uint32_t llId, const DataRoute& route)
{
    if (llId == 0U || route.peerId == 0U || route.mode != AccessMode::TRUNKED)
        return false;

    m_trunked[llId] = route;
    return true;
}

/* Erases the location or route for a given link-layer ID and access mode. */

bool DataLocationRegistry::erase(uint32_t llId, AccessMode mode)
{
    if (mode == AccessMode::TRUNKED)
        return m_trunked.erase(llId) > 0U;
    return m_conventional.erase(llId) > 0U;
}

/* Clears all subscriber and group route state. */

void DataLocationRegistry::clear()
{
    m_conventional.clear();
    m_trunked.clear();
    m_groupPeers.clear();
}

/* Expires stale conventional and trunked route state based on the current time. */

void DataLocationRegistry::expire(uint64_t nowMs)
{
    for (auto it = m_conventional.begin(); it != m_conventional.end();) {
        if (stale(it->second.lastSeen, nowMs))
            it = m_conventional.erase(it);
        else
            ++it;
    }

    for (auto it = m_trunked.begin(); it != m_trunked.end();) {
        if (stale(it->second.lastSeen, nowMs))
            it = m_trunked.erase(it);
        else
            ++it;
    }
}

/* Resolves the data route for a given link-layer ID and access mode. */

DataRoute DataLocationRegistry::resolve(uint32_t llId, AccessMode mode, uint64_t nowMs) const
{
    DataRoute result;
    result.mode = mode;

    // check if the requested access mode is trunked and resolve accordingly
    if (mode == AccessMode::TRUNKED) {
        auto it = m_trunked.find(llId);
        if (it == m_trunked.end() || stale(it->second.lastSeen, nowMs))
            return result;

        result = it->second;
        result.valid = true;

        return result;
    }

    auto it = m_conventional.find(llId);
    if (it == m_conventional.end() || stale(it->second.lastSeen, nowMs))
        return result;

    result.peerId = it->second.peerId;
    result.channelId = it->second.channelId;
    result.channelNo = it->second.channelNo;
    result.slotNo = it->second.slotNo;
    result.lastSeen = it->second.lastSeen;
    result.valid = true;

    return result;
}

/* Sets the group peers for group-based routing. */

void DataLocationRegistry::setGroupPeers(const std::vector<uint32_t>& peerIds)
{
    m_groupPeers.clear();
    for (uint32_t peerId : peerIds) {
        if (peerId != 0U)
            m_groupPeers.insert(peerId);
    }
}

/* Resolves the data routes for a group of available peers. */

std::vector<DataRoute> DataLocationRegistry::resolveGroup(const std::vector<uint32_t>& availablePeerIds) const
{
    std::vector<DataRoute> routes;
    for (uint32_t peerId : availablePeerIds) {
        if (peerId == 0U || (!m_groupPeers.empty() && m_groupPeers.count(peerId) == 0U))
            continue;

        DataRoute route;
        route.mode = AccessMode::CONVENTIONAL;
        route.peerId = peerId;
        route.valid = true;
        routes.push_back(route);
    }

    return routes;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Checks if a location is considered stale based on the last seen timestamp and the current time. */

bool DataLocationRegistry::stale(uint64_t lastSeen, uint64_t nowMs) const
{
    return m_staleAfterMs != 0U && nowMs != 0U && nowMs >= lastSeen && nowMs - lastSeen >= m_staleAfterMs;
}
