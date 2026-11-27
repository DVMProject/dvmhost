// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file UnitRouteRegistry.h
 * @ingroup fne_network
 * @file UnitRouteRegistry.cpp
 * @ingroup fne_network
 */
#if !defined(__UNIT_ROUTE_REGISTRY_H__)
#define __UNIT_ROUTE_REGISTRY_H__

#include "fne/Defines.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace network
{
    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Describes the current next-hop route to a registered subscriber unit.
     * @ingroup fne_network
     */
    struct HOST_SW_API UnitRoute {
        uint32_t unitId = 0U;        //!< Subscriber unit ID.
        uint32_t peerId = 0U;        //!< Immediate next-hop peer ID.
        uint32_t ssrc = 0U;          //!< Advertised route origin or immediate peer SSRC.
        uint64_t lastSeen = 0U;      //!< Last update time in milliseconds.
        bool learned = false;        //!< True when learned from validated traffic rather than registration.
        bool valid = false;          //!< True when the resolved route is current.
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Stores timestamped subscriber-unit next-hop routes shared by all digital protocols.
     * @ingroup fne_network
     */
    class HOST_SW_API UnitRouteRegistry {
    public:
        /**
         * @brief Initializes a unit-route registry.
         * @param lifetimeMs Lifetime of traffic-learned routes in milliseconds.
         */
        explicit UnitRouteRegistry(uint64_t lifetimeMs = 0U);

        /**
         * @brief Sets the lifetime of traffic-learned routes.
         * @param lifetimeMs Route lifetime in milliseconds.
         */
        void setLifetime(uint64_t lifetimeMs);
        /**
         * @brief Adds or refreshes a route.
         * @param unitId Subscriber unit ID.
         * @param peerId Immediate next-hop peer ID.
         * @param ssrc Advertised route origin or immediate peer SSRC.
         * @param learned True for a traffic-learned route; false for an explicit registration.
         * @param nowMs Update time in milliseconds.
         * @returns bool True when a valid route was stored; otherwise, false.
         */
        bool update(uint32_t unitId, uint32_t peerId, uint32_t ssrc, bool learned, uint64_t nowMs);
        /**
         * @brief Replaces all explicit routes learned through one peer.
         * @param peerId Immediate next-hop peer ID.
         * @param unitIds Complete registration snapshot for the peer.
         * @param ssrc Advertised route origin or immediate peer SSRC.
         * @param nowMs Update time in milliseconds.
         */
        void replacePeer(uint32_t peerId, const std::vector<uint32_t>& unitIds, uint32_t ssrc, uint64_t nowMs);
        /**
         * @brief Removes a route when it is currently owned by the specified peer.
         * @param unitId Subscriber unit ID.
         * @param peerId Immediate next-hop peer ID.
         * @returns bool True when a route was removed; otherwise, false.
         */
        bool erase(uint32_t unitId, uint32_t peerId);
        /**
         * @brief Removes every route using a peer as its next hop.
         * @param peerId Immediate next-hop peer ID.
         * @returns std::vector<uint32_t> Removed subscriber unit IDs.
         */
        std::vector<uint32_t> erasePeer(uint32_t peerId);
        /**
         * @brief Removes expired traffic-learned routes.
         * @param nowMs Current time in milliseconds.
         * @returns std::vector<UnitRoute> Routes removed by expiration.
         */
        std::vector<UnitRoute> expire(uint64_t nowMs);
        /**
         * @brief Resolves a current route for a subscriber unit.
         * @param unitId Subscriber unit ID.
         * @param nowMs Current time in milliseconds.
         * @returns UnitRoute Current route, or an invalid route when unavailable.
         */
        UnitRoute resolve(uint32_t unitId, uint64_t nowMs) const;

    private:
        uint64_t m_lifetimeMs;
        mutable std::mutex m_mutex;
        std::unordered_map<uint32_t, UnitRoute> m_routes;

        /**
         * @brief Checks whether a traffic-learned route is stale.
         * @param route Route to check.
         * @param nowMs Current time in milliseconds.
         * @returns bool True when the route is stale; otherwise, false.
         */
        bool stale(const UnitRoute& route, uint64_t nowMs) const;
    };
} // namespace network

#endif // __UNIT_ROUTE_REGISTRY_H__
