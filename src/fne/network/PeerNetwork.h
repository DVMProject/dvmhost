// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file PeerNetwork.h
 * @ingroup fne_network
 * @file PeerNetwork.cpp
 * @ingroup fne_network
 */
#if !defined(__PEER_NETWORK_H__)
#define __PEER_NETWORK_H__

#include "Defines.h"
#include "common/lookups/PeerListLookup.h"
#include "host/network/Network.h"

#include <string>
#include <cstdint>
#include <vector>

namespace network
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements the core peer networking logic.
     * @ingroup fne_network
     */
    class HOST_SW_API PeerNetwork : public Network {
    public:
        /**
         * @brief Initializes a new instance of the PeerNetwork class.
         * @param address Network Hostname/IP address to connect to.
         * @param port Network port number.
         * @param local 
         * @param peerId Unique ID on the network.
         * @param password Network authentication password.
         * @param duplex Flag indicating full-duplex operation.
         * @param debug Flag indicating whether network debug is enabled.
         * @param dmr Flag indicating whether DMR is enabled.
         * @param p25 Flag indicating whether P25 is enabled.
         * @param nxdn Flag indicating whether NXDN is enabled.
         * @param slot1 Flag indicating whether DMR slot 1 is enabled for network traffic.
         * @param slot2 Flag indicating whether DMR slot 2 is enabled for network traffic.
         * @param allowActivityTransfer Flag indicating that the system activity logs will be sent to the network.
         * @param allowDiagnosticTransfer Flag indicating that the system diagnostic logs will be sent to the network.
         * @param updateLookup Flag indicating that the system will accept radio ID and talkgroup ID lookups from the network.
         */
        PeerNetwork(const std::string& address, uint16_t port, uint16_t localPort, uint32_t peerId, const std::string& password,
            bool duplex, bool debug, bool dmr, bool p25, bool nxdn, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, bool updateLookup, bool saveLookup);

        /**
         * @brief Sets the instances of the Peer List lookup tables.
         * @param pidLookup Peer List Lookup Table Instance
         */
        void setPeerLookups(lookups::PeerListLookup* pidLookup);

        /**
         * @brief Gets the blocked traffic peer ID table.
         * @returns std::vector<uint32_t> List of peer IDs this peer network cannot send traffic to.
         */
        std::vector<uint32_t> blockTrafficTo() const { return m_blockTrafficToTable; }
        /**
         * @brief Adds an entry to the blocked traffic peer ID table.
         * @param peerId Peer ID to add to the blocked traffic table.
         */
        void addBlockedTrafficPeer(uint32_t peerId) { m_blockTrafficToTable.push_back(peerId); }
        /**
         * @brief Checks if the passed peer ID is blocked from sending to this peer.
         * @returns bool True, if blocked peer table is cleared, otherwise false.
         */
        bool checkBlockedPeer(uint32_t peerId);

        /**
         * @brief Writes a complete update of this CFNE's active peer list to the network.
         * @param peerList List of active peers.
         * @returns bool True, if list was sent, otherwise false.
         */
        bool writePeerLinkPeers(json::array* peerList);

        /**
         * @brief Returns flag indicating whether or not this peer connection is Peer-Link enabled.
         * @returns bool True, if Peer-Link enabled, otherwise false.
         */
        bool isPeerLink() const { return m_peerLink; }

    protected:
        std::vector<uint32_t> m_blockTrafficToTable;

        /**
         * @brief User overrideable handler that allows user code to process network packets not handled by this class.
         * @param peerId Peer ID.
         * @param opcode FNE network opcode pair.
         * @param[in] data Buffer containing message to send to peer.
         * @param length Length of buffer.
         * @param streamId Stream ID.
         */
        void userPacketHandler(uint32_t peerId, FrameQueue::OpcodePair opcode, const uint8_t* data = nullptr, uint32_t length = 0U,
            uint32_t streamId = 0U) override;

        /**
         * @brief Writes configuration to the network.
         * @returns bool True, if configuration was sent, otherwise false.
         */
        bool writeConfig() override;

    private:
        lookups::PeerListLookup* m_pidLookup;
        bool m_peerLink;

        uint32_t m_tgidCompressedSize;
        uint32_t m_tgidSize;

        uint8_t* m_tgidBuffer;

        uint32_t m_ridCompressedSize;
        uint32_t m_ridSize;

        uint8_t* m_ridBuffer;

        uint32_t m_pidCompressedSize;
        uint32_t m_pidSize;

        uint8_t* m_pidBuffer;
    };
} // namespace network

#endif // __PEER_NETWORK_H__
