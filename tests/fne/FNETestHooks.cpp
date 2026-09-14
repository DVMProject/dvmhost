// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/FNETestHooks.h"
#include "fne/HostFNE.h"
#include "fne/network/P25OTARService.h"

#include <cstring>
#include <chrono>
#include <stdexcept>

using namespace network;

// ---------------------------------------------------------------------------
//  Global Variables
// ---------------------------------------------------------------------------

bool g_promiscuousHub = false;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/**
 * @brief Initializes an instance of the HostFNE class.
 */
HostFNE::HostFNE(const std::string& confFile) :
    m_confFile(confFile), m_conf(), m_network(nullptr), m_mdNetwork(nullptr),
    m_vtunEnabled(false), m_packetDataMode(PacketDataMode::PROJECT25),
#if !defined(_WIN32)
    m_tun(nullptr),
#endif
    m_dmrEnabled(false), m_p25Enabled(false), m_p25P2Enabled(false),
    m_nxdnEnabled(false), m_analogEnabled(false), m_ridLookup(nullptr),
    m_tidLookup(nullptr), m_peerListLookup(nullptr), m_adjSiteMapLookup(nullptr),
    m_cryptoLookup(nullptr), m_peerNetworks(), m_pingTime(5U), m_maxMissedPings(5U),
    m_updateLookupTime(10U), m_peerReplicaSavesACL(false),
    m_allowActivityTransfer(false), m_allowDiagnosticTransfer(false), m_RESTAPI(nullptr)
{
    /* stub */
}

/**
 * @brief Finalizes an instance of the HostFNE class.
 */
HostFNE::~HostFNE() = default;

/**
 * @brief Adds a peer to the specified TrafficNetwork.
 * @param network The TrafficNetwork instance.
 * @param peerId The ID of the peer to add.
 * @param state The connection state of the peer.
 * @param connected Whether the peer is connected.
 * @return Reference to the added FNEPeerConnection.
 */
FNEPeerConnection& FNETestHooks::addPeer(TrafficNetwork& network, uint32_t peerId,
    NET_CONN_STATUS state, bool connected)
{
    FNEPeerConnection* connection = new FNEPeerConnection();
    connection->m_id = peerId;
    connection->m_connectionState = state;
    connection->m_connected = connected;
    network.m_peers[peerId] = connection;
    return *connection;
}

std::unique_ptr<uint8_t[]> FNETestHooks::processOTARKMM(TrafficNetwork& network,
    const std::vector<uint8_t>& packet, uint32_t llId, uint32_t& payloadSize)
{
    payloadSize = 0U;
    if (packet.empty() || network.m_p25OTARService == nullptr)
        return nullptr;

    return network.m_p25OTARService->processKMM(packet.data(), (uint32_t)packet.size(), llId,
        false, &payloadSize);
}

void FNETestHooks::setKMFServicesEnabled(TrafficNetwork& network, bool enabled)
{
    network.m_kmfServicesEnabled = enabled;
}

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

/**
 * @brief Checks if the specified peer exists in the TrafficNetwork.
 * @param network The TrafficNetwork instance.
 * @param peerId The ID of the peer to check.
 * @return True if the peer exists, false otherwise.
 */
bool FNETestHooks::hasPeer(const TrafficNetwork& network, uint32_t peerId)
{
    return network.m_peers.find(peerId) != network.m_peers.end();
}

/**
 * @brief Retrieves the number of peers in the TrafficNetwork.
 * @param network The TrafficNetwork instance.
 * @return The number of peers.
 */
size_t FNETestHooks::peerCount(const TrafficNetwork& network)
{
    return network.m_peers.size();
}

/**
 * @brief Retrieves the connection state of the specified peer.
 * @param network The TrafficNetwork instance.
 * @param peerId The ID of the peer.
 * @return The connection state of the peer.
 */
NET_CONN_STATUS FNETestHooks::peerState(const TrafficNetwork& network, uint32_t peerId)
{
    auto it = network.m_peers.find(peerId);
    if (it == network.m_peers.end() || it->second == nullptr)
        throw std::out_of_range("peer does not exist");
    return it->second->connectionState();
}

/**
 * @brief Clears all peers from the specified TrafficNetwork.
 * @param network The TrafficNetwork instance.
 */
void FNETestHooks::clearPeers(TrafficNetwork& network)
{
    for (auto peer : network.m_peers)
        delete peer.second;
    network.m_peers.clear();
}

/**
 * @brief Creates a NetPacketRequest for the specified network, metadata, function, subFunction, peerId, and packet.
 * @param network The TrafficNetwork instance.
 * @param metadata The MetadataNetwork instance (can be nullptr).
 * @param function The network function.
 * @param subFunction The network sub-function.
 * @param peerId The ID of the peer.
 * @param packet The packet data.
 * @return Pointer to the created NetPacketRequest.
 */
static NetPacketRequest* makeRequest(TrafficNetwork& network, MetadataNetwork* metadata,
    NET_FUNC::ENUM function, NET_SUBFUNC::ENUM subFunction, uint32_t peerId,
    const std::vector<uint8_t>& packet)
{
    NetPacketRequest* request = new NetPacketRequest();
    request->obj = &network;
    request->metadataObj = metadata;
    request->peerId = peerId;
    request->fneHeader.setFunction(function);
    request->fneHeader.setSubFunction(subFunction);
    request->fneHeader.setPeerId(peerId);
    request->fneHeader.setStreamId(1U);
    request->rtpHeader.setSSRC(peerId);
    request->pktRxTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    request->length = static_cast<int>(packet.size());
    if (!packet.empty()) {
        request->buffer = new uint8_t[packet.size()];
        ::memcpy(request->buffer, packet.data(), packet.size());
    }
    return request;
}

/**
 * @brief Dispatches a traffic packet to the specified TrafficNetwork.
 * @param network The TrafficNetwork instance.
 * @param function The network function.
 * @param subFunction The network sub-function.
 * @param peerId The ID of the peer.
 * @param packet The packet data.
 */
void FNETestHooks::dispatchTraffic(TrafficNetwork& network, NET_FUNC::ENUM function,
    NET_SUBFUNC::ENUM subFunction, uint32_t peerId, const std::vector<uint8_t>& packet)
{
    TrafficNetwork::taskNetworkRx(makeRequest(network, nullptr, function, subFunction, peerId, packet));
}

/**
 * @brief Dispatches a metadata packet to the specified TrafficNetwork.
 * @param network The TrafficNetwork instance.
 * @param metadata The MetadataNetwork instance.
 * @param function The network function.
 * @param subFunction The network sub-function.
 * @param peerId The ID of the peer.
 * @param packet The packet data.
 */
void FNETestHooks::dispatchMetadata(TrafficNetwork& network, MetadataNetwork& metadata,
    NET_FUNC::ENUM function, NET_SUBFUNC::ENUM subFunction, uint32_t peerId,
    const std::vector<uint8_t>& packet)
{
    MetadataNetwork::taskNetworkRx(makeRequest(network, &metadata, function, subFunction, peerId, packet));
}
