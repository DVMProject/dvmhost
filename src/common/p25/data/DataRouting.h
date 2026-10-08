// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file DataRouting.h
 * @ingroup p25_pdu
 * @file DataRouting.cpp
 * @ingroup p25_pdu
 */
#if !defined(__P25_DATA__DATAROUTING_H__)
#define  __P25_DATA__DATAROUTING_H__

#include "common/Defines.h"
#include "common/p25/data/PacketDataState.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace p25
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Constants
        // ---------------------------------------------------------------------------

        /**
         * @addtogroup p25_pdu
         * @{
         */

        /** 
         * @brief Access mode used to reach a P25 packet data subscriber. 
         */
        enum class AccessMode : uint8_t {
            CONVENTIONAL,               //!< Conventional access mode.
            TRUNKED                     //!< Trunked access mode.
        };

        /** 
         * @brief Policy applied when no current subscriber location is known. 
         */
        enum class UnknownLocationPolicy : uint8_t {
            QUEUE,                      //!< Queue the packet until location is known.
            ARP,                        //!< Attempt ARP resolution.
            DROP                        //!< Drop the packet if location is unknown.
        };
        /* @} */

        // ---------------------------------------------------------------------------
        //  Struct Declaration
        // ---------------------------------------------------------------------------

        /** 
         * @brief Last observed conventional location for a subscriber. 
         * @ingroup p25_pdu
         */
        struct DVM_COMMON_API ConventionalLocation {
            /**
             * @brief Peer ID of the subscriber.
             */
            uint32_t peerId = 0U;
            /**
             * @brief Channel ID of the subscriber.
             */
            uint8_t channelId = 0U;
            /**
             * @brief Channel number of the subscriber.
             */
            uint16_t channelNo = 0U;
            /**
             * @brief Timestamp of the last observation.
             */
            uint64_t lastSeen = 0U;
        };

        // ---------------------------------------------------------------------------
        //  Struct Declaration
        // ---------------------------------------------------------------------------

        /** 
         * @brief Resolved RF route for a P25 packet data subscriber.
         * @ingroup p25_pdu
         */
        struct DVM_COMMON_API DataRoute {
            /**
             * @brief Access mode used to reach the subscriber.
             */
            AccessMode mode = AccessMode::CONVENTIONAL;
            /**
             * @brief Peer ID of the subscriber.
             */
            uint32_t peerId = 0U;
            /**
             * @brief Channel ID of the subscriber.
             */
            uint8_t channelId = 0U;
            /**
             * @brief Channel number of the subscriber.
             */
            uint16_t channelNo = 0U;
            /**
             * @brief Grant ID associated with the subscriber.
             */
            uint32_t grantId = 0U;
            /**
             * @brief Timestamp of the last observation.
             */
            uint64_t lastSeen = 0U;
            /**
             * @brief Indicates if the route is valid.
             */
            bool valid = false;
        };

        // ---------------------------------------------------------------------------
        //  Struct Declaration
        // ---------------------------------------------------------------------------

        /** 
         * @brief Observed ARP neighbor information without authorization semantics. 
         * @ingroup p25_pdu
         */
        struct DVM_COMMON_API RouteNeighbor {
            /**
             * @brief Link-layer ID of the neighbor.
             */
            uint32_t llId = 0U;
            /**
             * @brief IP address of the neighbor.
             */
            uint32_t ipAddress = 0U;
            /**
             * @brief Timestamp of the last observation.
             */
            uint64_t lastSeen = 0U;
        };

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /** 
         * @brief Stores ARP observations separately from authorized bindings. 
         * @ingroup p25_pdu
         */
        class DVM_COMMON_API RouteNeighborCache {
        public:
            /**
             * @brief Observes a route neighbor with the given link-layer ID and IP address.
             * @param llId Link-layer ID of the neighbor.
             * @param ipAddress IP address of the neighbor.
             * @param nowMs Timestamp of the observation in milliseconds.
             */
            void observe(uint32_t llId, uint32_t ipAddress, uint64_t nowMs = 0U);
            /**
             * @brief Erases a route neighbor with the given link-layer ID.
             * @param llId Link-layer ID of the neighbor to erase.
             * @return True if the neighbor was found and erased, false otherwise.
             */
            bool erase(uint32_t llId);
            /**
             * @brief Clears all observed route neighbors.
             */
            void clear();

            /**
             * @brief Finds a route neighbor by its link-layer ID.
             * @param llId Link-layer ID of the neighbor to find.
             * @return Pointer to the route neighbor if found, nullptr otherwise.
             */
            const RouteNeighbor* findByLLId(uint32_t llId) const;
            /**
             * @brief Finds a route neighbor by its IP address.
             * @param ipAddress IP address of the neighbor to find.
             * @return Pointer to the route neighbor if found, nullptr otherwise.
             */
            const RouteNeighbor* findByIPAddress(uint32_t ipAddress) const;
            /**
             * @brief Returns the number of observed route neighbors.
             * @return Number of observed route neighbors.
             */
            size_t size() const noexcept { return m_neighbors.size(); }

        private:
            std::unordered_map<uint32_t, RouteNeighbor> m_neighbors;
        };

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /** 
         * @brief Resolves conventional locations and future trunked data routes. 
         * @ingroup p25_pdu
         */
        class DVM_COMMON_API DataLocationRegistry {
        public:
            /**
             * @brief Initializes a new instance of the LocationRegistry class.
             * @param staleAfterMs Duration in milliseconds after which locations are considered stale.
             * @param learnFromInbound Whether to learn locations from inbound traffic.
             * @param unknownLocationPolicy Policy for handling unknown locations.
             */
            explicit DataLocationRegistry(uint64_t staleAfterMs = 0U, bool learnFromInbound = true,
                UnknownLocationPolicy unknownLocationPolicy = UnknownLocationPolicy::ARP);

            /**
             * @brief Updates the conventional location for a given link-layer ID.
             * @param llId Link-layer ID of the neighbor.
             * @param location Conventional location to update.
             * @return True if the location was updated, false otherwise.
             */
            bool updateConventional(uint32_t llId, const ConventionalLocation& location);
            /**
             * @brief Updates the trunked data route for a given link-layer ID.
             * @param llId Link-layer ID of the neighbor.
             * @param route Trunked data route to update.
             * @return True if the route was updated, false otherwise.
             */
            bool updateTrunked(uint32_t llId, const DataRoute& route);
            /**
             * @brief Erases the location or route for a given link-layer ID and access mode.
             * @param llId Link-layer ID of the neighbor.
             * @param mode Access mode (conventional or trunked).
             * @return True if the entry was erased, false otherwise.
             */
            bool erase(uint32_t llId, AccessMode mode);
            /**
             * @brief Clears all subscriber and group route state.
             */
            void clear();
            /**
             * @brief Expires stale conventional and trunked route state based on the current time.
             * @param nowMs Current time in milliseconds.
             */
            void expire(uint64_t nowMs);

            /**
             * @brief Resolves the data route for a given link-layer ID and access mode.
             * @param llId Link-layer ID of the neighbor.
             * @param mode Access mode (conventional or trunked).
             * @param nowMs Current time in milliseconds.
             * @return Resolved data route.
             */
            DataRoute resolve(uint32_t llId, AccessMode mode, uint64_t nowMs = 0U) const;

            /**
             * @brief Sets the group peers for group-based routing.
             * @param peerIds Vector of peer link-layer IDs.
             */
            void setGroupPeers(const std::vector<uint32_t>& peerIds);
            /**
             * @brief Resolves the data routes for a group of available peers.
             * @param availablePeerIds Vector of available peer link-layer IDs.
             * @return Vector of resolved data routes for the available peers.
             */
            std::vector<DataRoute> resolveGroup(const std::vector<uint32_t>& availablePeerIds) const;

            /**
             * @brief Sets whether to learn from inbound data routes.
             * @param enabled True to enable learning from inbound routes, false to disable.
             */
            void setLearnFromInbound(bool enabled) noexcept { m_learnFromInbound = enabled; }
            /**
             * @brief Checks whether learning from inbound data routes is enabled.
             * @return True if learning from inbound routes is enabled, false otherwise.
             */
            bool learnFromInbound() const noexcept { return m_learnFromInbound; }
            /**
             * @brief Sets the policy for handling unknown locations.
             * @param policy The unknown location policy to set.
             */
            void setUnknownLocationPolicy(UnknownLocationPolicy policy) noexcept { m_unknownLocationPolicy = policy; }
            /**
             * @brief Retrieves the current policy for handling unknown locations.
             * @return The current unknown location policy.
             */
            UnknownLocationPolicy unknownLocationPolicy() const noexcept { return m_unknownLocationPolicy; }

            /**
             * @brief Retrieves the count of conventional locations.
             * @return The number of conventional locations.
             */
            size_t conventionalCount() const noexcept { return m_conventional.size(); }
            /**
             * @brief Retrieves the count of trunked locations.
             * @return The number of trunked locations.
             */
            size_t trunkedCount() const noexcept { return m_trunked.size(); }

        private:
            uint64_t m_staleAfterMs;
            bool m_learnFromInbound;
            UnknownLocationPolicy m_unknownLocationPolicy;
            std::unordered_map<uint32_t, ConventionalLocation> m_conventional;
            std::unordered_map<uint32_t, DataRoute> m_trunked;
            std::unordered_set<uint32_t> m_groupPeers;

            /**
             * @brief Checks if a location is considered stale based on the last seen timestamp and the current time.
             * @param lastSeen The timestamp when the location was last seen.
             * @param nowMs The current time in milliseconds.
             * @return True if the location is stale, false otherwise.
             */
            bool stale(uint64_t lastSeen, uint64_t nowMs) const;
        };

    } // namespace data
} // namespace p25

#endif // __P25_DATA__DATAROUTING_H__
