// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024-2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file MetadataNetwork.h
 * @ingroup fne_network
 * @file MetadataNetwork.cpp
 * @ingroup fne_network
 */
#if !defined(__METADATA_NETWORK_H__)
#define __METADATA_NETWORK_H__

#include "fne/Defines.h"
#include "common/network/BaseNetwork.h"
#include "common/network/PacketBuffer.h"
#include "common/ThreadPool.h"
#include "fne/network/TrafficNetwork.h"

#include <memory>
#include <mutex>
#include <string>

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------

#if defined(CATCH2_TEST_COMPILATION)
class FNETestHooks;
#endif

class HOST_SW_API HostFNE;

namespace network
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    const uint32_t TIMEOUT_MAX_REPL = 5000U; // 5 seconds

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements the diagnostic/activity log networking logic.
     * @ingroup fne_network
     */
    class HOST_SW_API MetadataNetwork : public BaseNetwork {
    public:
        /**
         * @brief Initializes a new instance of the MetadataNetwork class.
         * @param host Instance of the HostFNE class.
         * @param network Instance of the TrafficNetwork class.
         * @param address Network Hostname/IP address to listen on.
         * @param port Network port number.
         * @param workerCnt Number of worker threads.
         */
        MetadataNetwork(HostFNE* host, TrafficNetwork* trafficNetwork, const std::string& address, uint16_t port, uint16_t workerCnt);
        /**
         * @brief Finalizes a instance of the MetadataNetwork class.
         */
        ~MetadataNetwork() override;

        /**
         * @brief Gets the current status of the network.
         * @returns NET_CONN_STATUS Current network status.
         */
        NET_CONN_STATUS getStatus() { return m_status; }

        /**
         * @brief Sets endpoint preshared encryption key.
         * @param presharedKey Encryption preshared key for networking.
         */
        void setPresharedKey(const uint8_t* presharedKey);

        /**
         * @brief Process a data frames from the network.
         */
        void processNetwork();

        /**
         * @brief Updates the timer by the passed number of milliseconds.
         * @param ms Number of milliseconds.
         */
        void clock(uint32_t ms) override;

        /**
         * @brief Opens connection to the network.
         * @returns bool True, if networking has started, otherwise false.
         */
        bool open() override;

        /**
         * @brief Closes connection to the network.
         */
        void close() override;

    private:
#if defined(CATCH2_TEST_COMPILATION)
        friend class ::FNETestHooks;
#endif
        friend class TrafficNetwork;
        bool writePeerMetadata(FNEPeerConnection* connection, uint32_t ssrc, FrameQueue::OpcodePair opcode, const uint8_t* data,
            uint32_t length, uint16_t pktSeq, uint32_t streamId) const;

        TrafficNetwork* m_trafficNetwork;
        HostFNE* m_host;

        std::string m_address;
        uint16_t m_port;

        NET_CONN_STATUS m_status;

        /**
         * @brief Represents a packet buffer entry in a map.
         */
        class PacketBufferEntry {
        public:
            /**
             * @brief Initializes a new instance of the PacketBufferEntry class.
             */
            PacketBufferEntry() :
                streamId(0U),
                buffer(nullptr)
            {
                /* stub */
            }

            std::mutex mutex;

            /**
             * @brief Stream ID of the packet.
             */
            uint32_t streamId;

            /**
             * @brief Packet fragment buffer.
             */
            std::unique_ptr<PacketBuffer> buffer;
        };
        using PacketBufferEntryPtr = std::shared_ptr<PacketBufferEntry>;
        using PacketBufferMap = concurrent::unordered_map<uint32_t, PacketBufferEntryPtr>;

        PacketBufferMap m_peerKeyUpdatePkt;
        PacketBufferMap m_peerReplicaActPkt;
        PacketBufferMap m_peerPatchStatusPkt;
        PacketBufferMap m_peerTreeListPkt;

        ThreadPool m_threadPool;

        /**
         * @brief Finds a packet buffer entry in the map.
         * @param pktMap Instance of the PacketBufferMap class.
         * @param peerId Peer ID of the packet buffer entry.
         * @returns PacketBufferEntryPtr Instance of the PacketBufferEntry class.
         */
        static PacketBufferEntryPtr findPacketBufferEntry(PacketBufferMap& pktMap, uint32_t peerId);
        /**
         * @brief Finds or creates a packet buffer entry in the map.
         * @param pktMap Instance of the PacketBufferMap class.
         * @param peerId Peer ID of the packet buffer entry.
         * @param name Name of the packet buffer entry.
         * @param streamId Stream ID of the packet buffer entry.
         * @param created Optional pointer to a boolean that will be set to true if a new entry was created, false otherwise.
         * @returns PacketBufferEntryPtr Instance of the PacketBufferEntry class.
         */
        static PacketBufferEntryPtr findOrCreatePacketBufferEntry(PacketBufferMap& pktMap, uint32_t peerId, const char* name, uint32_t streamId, bool* created = nullptr);
        /**
         * @brief Erases a packet buffer entry from the map.
         * @param pktMap Instance of the PacketBufferMap class.
         * @param peerId Peer ID of the packet buffer entry.
         * @param pkt Expected packet buffer entry. The map entry is only erased if
         * it still refers to this instance.
         */
        static void erasePacketBufferEntry(PacketBufferMap& pktMap, uint32_t peerId, const PacketBufferEntryPtr& pkt);

        /*
        ** Packet Processing
        */

        using PacketHandlerFunc = void (*)(TrafficNetwork* network, MetadataNetwork* mdNetwork, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId);

        /**
         * @brief Implements the packet handler functions for the MetadataNetwork class.
         */
        class PacketHandler {
        public:
            /**
             * @brief Handles NET_FUNC::TRANSFER packets.
             * @param network Instance of the TrafficNetwork class.
             * @param mdNetwork Instance of the MetadataNetwork class.
             * @param req Instance of the NetPacketRequest structure.
             * @param peerId Peer ID of the packet.
             * @param ssrc SSRC of the packet.
             * @param streamId Stream ID of the packet.
             */
            static void transfer(TrafficNetwork* network, MetadataNetwork* mdNetwork, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId);
            /**
             * @brief Handles NET_FUNC::ANNOUNCE packets.
             * @param network Instance of the TrafficNetwork class.
             * @param mdNetwork Instance of the MetadataNetwork class.
             * @param req Instance of the NetPacketRequest structure.
             * @param peerId Peer ID of the packet.
             * @param ssrc SSRC of the packet.
             * @param streamId Stream ID of the packet.
             */
            static void announce(TrafficNetwork* network, MetadataNetwork* mdNetwork, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId);

            /**
             * @brief Handles NET_FUNC::RADIO_ALIAS_SYNC packets.
             * @param network Instance of the TrafficNetwork class.
             * @param mdNetwork Instance of the MetadataNetwork class.
             * @param req Instance of the NetPacketRequest structure.
             * @param peerId Peer ID of the packet.
             * @param ssrc SSRC of the packet.
             * @param streamId Stream ID of the packet.
             */
            static void radioAliasSync(TrafficNetwork* network, MetadataNetwork* mdNetwork, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId);

            /**
             * @brief Handles NET_FUNC::KEYS_INVENTORY packets.
             * @param network Instance of the TrafficNetwork class.
             * @param mdNetwork Instance of the MetadataNetwork class.
             * @param req Instance of the NetPacketRequest structure.
             * @param peerId Peer ID of the packet.
             * @param ssrc SSRC of the packet.
             * @param streamId Stream ID of the packet.
             */
            static void keysInventory(TrafficNetwork* network, MetadataNetwork* mdNetwork, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId);
            /**
             * @brief Handles NET_FUNC::KEYS_UPDATE packets.
             * @param network Instance of the TrafficNetwork class.
             * @param mdNetwork Instance of the MetadataNetwork class.
             * @param req Instance of the NetPacketRequest structure.
             * @param peerId Peer ID of the packet.
             * @param ssrc SSRC of the packet.
             * @param streamId Stream ID of the packet.
             */
            static void keysUpdate(TrafficNetwork* network, MetadataNetwork* mdNetwork, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId);

            /**
             * @brief Handles NET_FUNC::REPL packets.
             * @param network Instance of the TrafficNetwork class.
             * @param mdNetwork Instance of the MetadataNetwork class.
             * @param req Instance of the NetPacketRequest structure.
             * @param peerId Peer ID of the packet.
             * @param ssrc SSRC of the packet.
             * @param streamId Stream ID of the packet.
             */
            static void replication(TrafficNetwork* network, MetadataNetwork* mdNetwork, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId);

            /**
             * @brief Handles NET_FUNC::NET_TREE packets.
             * @param network Instance of the TrafficNetwork class.
             * @param mdNetwork Instance of the MetadataNetwork class.
             * @param req Instance of the NetPacketRequest structure.
             * @param peerId Peer ID of the packet.
             * @param ssrc SSRC of the packet.
             * @param streamId Stream ID of the packet.
             */
            static void networkTree(TrafficNetwork* network, MetadataNetwork* mdNetwork, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId);
        };

        /**
         * @brief Entry point to process a given network packet.
         * @param req Instance of the NetPacketRequest structure.
         */
        static void taskNetworkRx(NetPacketRequest* req);
    };
} // namespace network

#endif // __METADATA_NETWORK_H__
