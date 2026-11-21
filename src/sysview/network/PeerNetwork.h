// SPDX-License-Identifier: PROPRIETARY
/*
 * Digital Voice Modem - FNE System View
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024-2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup fneSysView_network Networking
 * @brief Implementation for the SysView networking.
 * @ingroup fneSysView
 *
 * @file PeerNetwork.h
 * @ingroup fneSysView
 * @file PeerNetwork.cpp
 * @ingroup fneSysView
 */
#if !defined(__PEER_NETWORK_H__)
#define __PEER_NETWORK_H__

#include "Defines.h"
#include "common/network/Network.h"
#include "common/network/PacketBuffer.h"

#include <string>
#include <cstdint>
#include <mutex>

namespace network
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements the SysView peer networking logic.
     * @ingroup fneAffView_network
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
         * @param allowActivityTransfer Flag indicating that the system activity logs will be sent to the network.
         * @param allowDiagnosticTransfer Flag indicating that the system diagnostic logs will be sent to the network.
         * @param updateLookup Flag indicating that the system will accept radio ID and talkgroup ID lookups from the network.
         */
        PeerNetwork(const std::string& address, uint16_t port, uint16_t localPort, uint32_t peerId, const std::string& password,
            bool duplex, bool debug, bool allowActivityTransfer, bool allowDiagnosticTransfer, bool updateLookup, bool saveLookup);

        /**
         * @brief Flag indicating whether or not SysView has received peer replication data transfers.
         */
        bool hasPeerReplica() const { return m_peerReplica; }

        /**
         * @brief Helper to lock the peer status mutex.
         */
        void lockPeerStatus() { s_peerStatusMutex.lock(); }
        /**
         * @brief Helper to unlock the peer status mutex.
         */
        void unlockPeerStatus() { s_peerStatusMutex.unlock(); }

        /**
         * @brief Gets the current DMR stream ID.
         * @param slotNo DMR slot to get stream ID for.
         * @return uint32_t Stream ID for the given DMR slot.
         */
        uint32_t getRxDMRStreamId(uint32_t slotNo) const;
        /**
         * @brief Gets the current P25 stream ID.
         * @return uint32_t Stream ID.
         */
        uint32_t getRxP25StreamId() const { return m_rxP25StreamId; }
        /**
         * @brief Gets the current P25 Phase 2 stream ID.
         * @param slotNo P25 Phase 2 slot to get stream ID for.
         * @return uint32_t Stream ID for the given P25 Phase 2 slot.
         */
        uint32_t getRxP25P2StreamId(uint32_t slotNo) const;
        /**
         * @brief Gets the current NXDN stream ID.
         * @return uint32_t Stream ID.
         */
        uint32_t getRxNXDNStreamId() const { return m_rxNXDNStreamId; }
        /**
         * @brief Gets the current analog stream ID.
         * @return uint32_t Stream ID.
         */
        uint32_t getRxAnalogStreamId() const { return m_rxAnalogStreamId; }

        /**
         * @brief Map of peer status.
         */
        std::unordered_map<uint32_t, json::object> peerStatus;
        /**
         * @brief Map of peer status timers, used to track when peer status entries should be expired and 
         *  removed from the map.
         */
        std::unordered_map<uint32_t, Timer> peerStatusTimers;

    protected:
        /**
         * @brief User overrideable handler that allows user code to process network packets not handled by this class.
         * @param peerId Peer ID.
         * @param opcode FNE network opcode pair.
         * @param[in] data Buffer containing message to send to peer.
         * @param length Length of buffer.
         * @param streamId Stream ID.
         * @param fneHeader RTP FNE Header.
         * @param rtpHeader RTP Header.
         */
        void userPacketHandler(uint32_t peerId, FrameQueue::OpcodePair opcode, const uint8_t* data = nullptr, uint32_t length = 0U,
            uint32_t streamId = 0U, const frame::RTPFNEHeader& fneHeader = frame::RTPFNEHeader(), const frame::RTPHeader& rtpHeader = frame::RTPHeader()) override;

        /**
         * @brief Writes configuration to the network.
         * @returns bool True, if configuration was sent, otherwise false.
         */
        bool writeConfig() override;

    private:
        static std::mutex s_peerStatusMutex;
        bool m_peerReplica;

        PacketBuffer m_tgidPkt;
        PacketBuffer m_ridPkt;
    };
} // namespace network

#endif // __PEER_NETWORK_H__
