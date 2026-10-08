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
#include <string>
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
        const std::vector<uint8_t>& packet, uint32_t llId, uint32_t& payloadSize,
        uint8_t outerAlgoId = P25DEF::ALGO_UNENCRYPT, uint16_t outerKId = 0U,
        bool dataLinkIndependent = false);
    /**
     * @brief Builds an OTAR rekey command for the specified logical link ID and KMM RSI.
     * @param network The TrafficNetwork instance.
     * @param llId The logical link ID.
     * @param kmmRSI The KMM RSI value.
     * @return A vector of vectors containing the OTAR rekey frames.
     */
    static std::vector<std::vector<uint8_t>> buildOTARRekey(network::TrafficNetwork& network,
        uint32_t llId, uint32_t kmmRSI);
    /**
     * @brief Resolves the outer security context selected for a generated response.
     * @param network The TrafficNetwork instance.
     * @param packet The OTAR response packet data.
     * @param encrypted Output flag indicating if the packet is encrypted.
     * @param algoId Output parameter for the algorithm ID used for encryption.
     * @param kid Output parameter for the key ID used for encryption.
     * @return True if the security parameters were successfully resolved, false otherwise.
     */
    static bool resolveOTARResponseSecurity(network::TrafficNetwork& network,
        const std::vector<uint8_t>& packet, bool& encrypted, uint8_t& algoId, uint16_t& kid);
    /**
     * @brief Passes a DLD KMM through the public P25 OTAR bearer entry point.
     * @param network The TrafficNetwork that owns the OTAR service.
     * @param packet Encoded DLD bytes.
     * @param llId Logical Link ID associated with the message.
     * @param n Sequence number of the DLD packet.
     * @param encrypted Whether the packet is encrypted.
     * @param algoId The algorithm ID used for encryption.
     * @param kid The key ID used for encryption.
     * @param mi The message integrity value.
     * @return True if the packet was successfully processed, false otherwise.
     */
    static bool processOTARDLD(network::TrafficNetwork& network, const std::vector<uint8_t>& packet,
        uint32_t llId, uint8_t n, bool encrypted = false, uint8_t algoId = P25DEF::ALGO_UNENCRYPT,
        uint16_t kid = 0U, const uint8_t* mi = nullptr);
    /** 
     * @brief Encodes and injects a complete DLD KMM through the P25 PDU assembler path.
     * @param network The TrafficNetwork that owns the OTAR service.
     * @param packet Encoded DLD bytes.
     * @param llId Logical Link ID associated with the message.
     * @param encrypted Whether the packet is encrypted.
     * @param algoId The algorithm ID used for encryption.
     * @param kid The key ID used for encryption.
     * @param mi The message integrity value.
     * @return True if the packet was successfully processed, false otherwise.
     */
    static bool processOTARDLDPDU(network::TrafficNetwork& network, const std::vector<uint8_t>& packet,
        uint32_t llId, bool encrypted = false, uint8_t algoId = P25DEF::ALGO_UNENCRYPT,
        uint16_t kid = 0U, const uint8_t* mi = nullptr);
    /**
     * @brief Injects a one-block PDU response through the FNE packet-data receiver.
     * @param network The TrafficNetwork that owns the packet-data receiver.
     * @param llId Logical Link ID associated with the response.
     * @param responseStatus Response status/N(R) value.
     * @return True if the response was processed successfully.
     */
    static bool processP25PDUResponse(network::TrafficNetwork& network, uint32_t llId,
        uint8_t responseStatus);
    /**
     * @brief Passes a complete Version-0 DLI datagram through the network receive task.
     * @param network The TrafficNetwork that owns the OTAR service.
     * @param datagram Encoded DLI bytes.
     */
    static void processOTARDLI(network::TrafficNetwork& network, const std::vector<uint8_t>& datagram);
    /** 
     * @brief Opens the real DLI UDP endpoint for an integration test.
     * @param network The TrafficNetwork that owns the OTAR service.
     * @param address The IP address of the DLI UDP endpoint.
     * @param port The port number of the DLI UDP endpoint.
     * @return True if the endpoint was successfully opened, false otherwise.
     */
    static bool openOTARDLI(network::TrafficNetwork& network, const std::string& address, uint16_t port);
    /** 
     * @brief Polls the real DLI UDP endpoint.
     * @param network The TrafficNetwork that owns the OTAR service.
     * @param ms The number of milliseconds to wait.
     */
    static void clockOTARDLI(network::TrafficNetwork& network, uint32_t ms = 1U);
    /** 
     * @brief Closes the real DLI UDP endpoint.
     * @param network The TrafficNetwork that owns the OTAR service.
     */
    static void closeOTARDLI(network::TrafficNetwork& network);
    /** 
     * @brief Applies the OTAR outer encryption/decryption implementation.
     * @param network The TrafficNetwork that owns the OTAR service.
     * @param algoId The algorithm ID used for encryption/decryption.
     * @param kid The key ID used for encryption/decryption.
     * @param mi The message integrity value.
     * @param packet The packet data to be encrypted/decrypted.
     * @param encrypt True to encrypt, false to decrypt.
     * @return The resulting encrypted/decrypted packet.
     */
    static std::vector<uint8_t> cryptOTARKMM(network::TrafficNetwork& network, uint8_t algoId,
        uint16_t kid, uint8_t* mi, const std::vector<uint8_t>& packet, bool encrypt);
    /**
     * @brief Checks if the specified inbound message number exists for the given RSI.
     * @param network The TrafficNetwork that owns the OTAR service.
     * @param rsi The RSI to check.
     * @param mn The message number to check.
     * @return True if the inbound message number exists and matches, false otherwise.
     */
    static bool hasOTARInboundMessageNumber(network::TrafficNetwork& network, uint32_t rsi, uint16_t mn);
    /**
     * @brief Enables or disables KMF services for the specified TrafficNetwork.
     * @param network The TrafficNetwork that owns the KMF services.
     * @param enabled True to enable KMF services, false to disable.
     */
    static void setKMFServicesEnabled(network::TrafficNetwork& network, bool enabled);
    /**
     * @brief Adds a cryptographic key to the network's crypto lookup.
     * @param network The TrafficNetwork instance.
     * @param key The cryptographic key to add.
     */
    static void addCryptoKey(network::TrafficNetwork& network, const EKCKeyItem& key);
};

#endif // __FNE_TEST_HOOKS_H__
