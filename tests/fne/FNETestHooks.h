// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#if !defined(__FNE_TEST_HOOKS_H__)
#define __FNE_TEST_HOOKS_H__

#include "fne/network/MetadataNetwork.h"
#include "fne/network/TrafficNetwork.h"

#include <cstdint>
#include <memory>
#include <vector>

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Test hooks for the FNE network components.
 */
class FNETestHooks {
public:
    /**
     * @brief Adds a peer to the specified TrafficNetwork.
     * @param network The TrafficNetwork instance.
     * @param peerId The ID of the peer to add.
     * @param state The connection state of the peer.
     * @param connected Whether the peer is connected.
     * @return Reference to the added FNEPeerConnection.
     */
    static network::FNEPeerConnection& addPeer(network::TrafficNetwork& network, uint32_t peerId,
        network::NET_CONN_STATUS state, bool connected = false);
    /**
     * @brief Checks if the specified peer exists in the TrafficNetwork.
     * @param network The TrafficNetwork instance.
     * @param peerId The ID of the peer to check.
     * @return True if the peer exists, false otherwise.
     */
    static bool hasPeer(const network::TrafficNetwork& network, uint32_t peerId);
    /**
     * @brief Retrieves the number of peers in the TrafficNetwork.
     * @param network The TrafficNetwork instance.
     * @return The number of peers.
     */
    static size_t peerCount(const network::TrafficNetwork& network);
    /**
     * @brief Retrieves the connection state of the specified peer.
     * @param network The TrafficNetwork instance.
     * @param peerId The ID of the peer.
     * @return The connection state of the peer.
     */
    static network::NET_CONN_STATUS peerState(const network::TrafficNetwork& network, uint32_t peerId);
    /**
     * @brief Clears all peers from the TrafficNetwork.
     * @param network The TrafficNetwork instance.
     */
    static void clearPeers(network::TrafficNetwork& network);

    /**
     * @brief Dispatches a traffic packet to the specified peer in the TrafficNetwork.
     * @param network The TrafficNetwork instance.
     * @param function The network function.
     * @param subFunction The network sub-function.
     * @param peerId The ID of the peer.
     * @param packet The packet data.
     */
    static void dispatchTraffic(network::TrafficNetwork& network, network::NET_FUNC::ENUM function,
        network::NET_SUBFUNC::ENUM subFunction, uint32_t peerId, const std::vector<uint8_t>& packet);
    /**
     * @brief Dispatches a metadata packet to the specified peer in the TrafficNetwork.
     * @param network The TrafficNetwork instance.
     * @param metadata The MetadataNetwork instance.
     * @param function The network function.
     * @param subFunction The network sub-function.
     * @param peerId The ID of the peer.
     * @param packet The packet data.
     */
    static void dispatchMetadata(network::TrafficNetwork& network, network::MetadataNetwork& metadata,
        network::NET_FUNC::ENUM function, network::NET_SUBFUNC::ENUM subFunction, uint32_t peerId,
        const std::vector<uint8_t>& packet);

    /**
     * @brief Passes an encoded KMM through the FNE OTAR message dispatcher.
     * @param network The TrafficNetwork that owns the OTAR service.
     * @param packet Encoded KMM bytes.
     * @param llId Logical Link ID associated with the message.
     * @param[out] payloadSize Size of the returned KMM, or zero when no response is required.
     * @return Encoded response, or nullptr when the dispatcher produces no response.
     */
    static std::unique_ptr<uint8_t[]> processOTARKMM(network::TrafficNetwork& network,
        const std::vector<uint8_t>& packet, uint32_t llId, uint32_t& payloadSize);
    /** @brief Enables or disables KMF services for a test TrafficNetwork. */
    static void setKMFServicesEnabled(network::TrafficNetwork& network, bool enabled);
};

#endif // __FNE_TEST_HOOKS_H__
