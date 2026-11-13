// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/network/UnitRouteRegistry.h"

using namespace network;

#include <algorithm>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a unit-route registry. */

UnitRouteRegistry::UnitRouteRegistry(uint64_t lifetimeMs) :
    m_lifetimeMs(lifetimeMs),
    m_mutex(),
    m_routes()
{
    /* stub */
}

/* Sets the lifetime of traffic-learned routes. */

void UnitRouteRegistry::setLifetime(uint64_t lifetimeMs)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lifetimeMs = lifetimeMs;
}

/* Adds or refreshes a route. */

bool UnitRouteRegistry::update(uint32_t unitId, uint32_t peerId, uint32_t ssrc, bool learned, uint64_t nowMs)
{
    if (unitId == 0U || peerId == 0U)
        return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_routes.find(unitId);
    if (it != m_routes.end() && !it->second.learned && learned) {
        if (it->second.peerId != peerId)
            return false;
        it->second.lastSeen = nowMs;
        return true;
    }

    UnitRoute& route = m_routes[unitId];
    route.unitId = unitId;
    route.peerId = peerId;
    route.ssrc = ssrc != 0U ? ssrc : peerId;
    route.lastSeen = nowMs;
    route.learned = learned;
    route.valid = true;
    return true;
}

/* Replaces all explicit routes learned through one peer. */

void UnitRouteRegistry::replacePeer(uint32_t peerId, const std::vector<uint32_t>& unitIds, uint32_t ssrc, uint64_t nowMs)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_routes.begin(); it != m_routes.end();) {
        if (it->second.peerId == peerId && !it->second.learned &&
            std::find(unitIds.cbegin(), unitIds.cend(), it->first) == unitIds.cend()) {
            it = m_routes.erase(it);
        }
        else {
            ++it;
        }
    }

    for (uint32_t unitId : unitIds) {
        if (unitId == 0U)
            continue;
        UnitRoute& route = m_routes[unitId];
        route.unitId = unitId;
        route.peerId = peerId;
        route.ssrc = ssrc != 0U ? ssrc : peerId;
        route.lastSeen = nowMs;
        route.learned = false;
        route.valid = true;
    }
}

/* Removes a route when it is currently owned by the specified peer. */

bool UnitRouteRegistry::erase(uint32_t unitId, uint32_t peerId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_routes.find(unitId);
    if (it == m_routes.end() || it->second.peerId != peerId)
        return false;
    m_routes.erase(it);
    return true;
}

/* Removes every route using a peer as its next hop. */

std::vector<uint32_t> UnitRouteRegistry::erasePeer(uint32_t peerId)
{
    std::vector<uint32_t> removed;
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_routes.begin(); it != m_routes.end();) {
        if (it->second.peerId == peerId) {
            removed.push_back(it->first);
            it = m_routes.erase(it);
        }
        else {
            ++it;
        }
    }
    return removed;
}

/* Removes expired traffic-learned routes. */

std::vector<UnitRoute> UnitRouteRegistry::expire(uint64_t nowMs)
{
    std::vector<UnitRoute> expired;
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_routes.begin(); it != m_routes.end();) {
        if (stale(it->second, nowMs)) {
            expired.push_back(it->second);
            it = m_routes.erase(it);
        }
        else {
            ++it;
        }
    }
    return expired;
}

/* Resolves a current route for a subscriber unit. */

UnitRoute UnitRouteRegistry::resolve(uint32_t unitId, uint64_t nowMs) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_routes.find(unitId);
    if (it == m_routes.end() || stale(it->second, nowMs))
        return UnitRoute();
    return it->second;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Checks whether a traffic-learned route is stale. */

bool UnitRouteRegistry::stale(const UnitRoute& route, uint64_t nowMs) const
{
    return route.learned && m_lifetimeMs != 0U && nowMs >= route.lastSeen && nowMs - route.lastSeen >= m_lifetimeMs;
}
