// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
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
#include "common/network/Network.h"
#include "common/network/PacketBuffer.h"
#include "common/ThreadPool.h"
#include "fne/network/SpanningTree.h"
#include "fne/network/HAParameters.h"

#include <string>
#include <cstdint>
#include <vector>

namespace network
{
    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents the data required for a network packet handler thread.
     * @ingroup fne_network
     */
    struct PeerPacketRequest : thread_t {
        uint32_t peerId;                    //!< Peer ID for this request.
        uint32_t streamId;                  //!< Stream ID for this request.

        frame::RTPHeader rtpHeader;         //!< RTP Header
        frame::RTPFNEHeader fneHeader;      //!< RTP FNE Header
        int length = 0U;                    //!< Length of raw data buffer
        uint8_t* buffer = nullptr;          //!< Raw data buffer

        network::NET_SUBFUNC::ENUM subFunc; //!< Sub-function of the packet

        uint64_t pktRxTime;                 //!< Packet receive time
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements the FNE upstream peer networking logic.
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
         * @param analog Flag indicating whether analog is enabled.
         * @param slot1 Flag indicating whether DMR slot 1 is enabled for network traffic.
         * @param slot2 Flag indicating whether DMR slot 2 is enabled for network traffic.
         * @param allowActivityTransfer Flag indicating that the system activity logs will be sent to the network.
         * @param allowDiagnosticTransfer Flag indicating that the system diagnostic logs will be sent to the network.
         * @param updateLookup Flag indicating that the system will accept radio ID and talkgroup ID lookups from the network.
         */
        PeerNetwork(const std::string& address, uint16_t port, uint16_t localPort, uint32_t peerId, const std::string& password,
            bool duplex, bool debug, bool dmr, bool p25, bool nxdn, bool analog, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, bool updateLookup, bool saveLookup);
        /**
         * @brief Finalizes a instance of the PeerNetwork class.
         */
        ~PeerNetwork() override;

        /**
         * @brief Set the peer ID of this FNE's master.
         * @param masterPeerId Master Peer ID.
         */
        void setMasterPeerId(uint32_t masterPeerId) { m_masterPeerId = masterPeerId; }
        /**
         * @brief Sets the instances of the Peer List lookup tables.
         * @param pidLookup Peer List Lookup Table Instance
         */
        void setPeerLookups(lookups::PeerListLookup* pidLookup);

        /**
         * @brief Opens connection to the network.
         * @returns bool True, if networking has started, otherwise false.
         */
        bool open() override;

        /**
         * @brief Closes connection to the network.
         */
        void close() override;

        /**
         * @brief Helper to set the DMR protocol callback.
         * @param callback 
         */
        void setDMRCallback(std::function<void(PeerNetwork*, const uint8_t*, uint32_t, uint32_t, const frame::RTPFNEHeader&, const frame::RTPHeader&)>&& callback) { m_dmrCallback = callback; }
        /**
         * @brief Helper to set the P25 protocol callback.
         * @param callback 
         */
        void setP25Callback(std::function<void(PeerNetwork*, const uint8_t*, uint32_t, uint32_t, const frame::RTPFNEHeader&, const frame::RTPHeader&)>&& callback) { m_p25Callback = callback; }
        /**
         * @brief Helper to set the NXDN protocol callback.
         * @param callback 
         */
        void setNXDNCallback(std::function<void(PeerNetwork*, const uint8_t*, uint32_t, uint32_t, const frame::RTPFNEHeader&, const frame::RTPHeader&)>&& callback) { m_nxdnCallback = callback; }
        /**
         * @brief Helper to set the analog protocol callback.
         * @param callback 
         */
        void setAnalogCallback(std::function<void(PeerNetwork*, const uint8_t*, uint32_t, uint32_t, const frame::RTPFNEHeader&, const frame::RTPHeader&)>&& callback) { m_analogCallback = callback; }

        /**
         * @brief Helper to set the network tree disconnect callback.
         * @param callback 
         */
        void setNetTreeDiscCallback(std::function<void(PeerNetwork*, const uint32_t offendingPeerId)>&& callback) { m_netTreeDiscCallback = callback; }
        /**
         * @brief Helper to set the peer replica notification callback.
         * @param callback 
         */
        void setNotifyPeerReplicaCallback(std::function<void(PeerNetwork*)>&& callback) { m_peerReplicaCallback = callback; }

        /**
         * @brief Writes a complete update of this CFNE's active peer list to the network.
         * \code{.unparsed}
         *  Below is the representation of the data layout for the active peer listmessage.
         *  The active peer list message is a JSON body, and is a packet buffer compressed message.
         *
         *  Byte 0               1               2               3
         *  Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Protocol Tag (REPL)                                           |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Reserved                                                      |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Variable Length JSON Payload ................................ |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *
         * The JSON payload is variable length and looks like this:
         *  {
         *      "peerId": <Peer ID>,
         *      "address": "<Peer IP Address>",
         *      "port": <Peer Port>,
         *      "connected": <Boolean flag indicating whether or not this peer is connected>,
         *      "connectionState": <Numerical connection state value>,
         *      "pingsReceived": <Number of pings received>,
         *      "lastPing": <Last ping time>,
         *      "controlChannel": <Control Channel Peer ID>,
         *      "config": {
         *          <This is the JSON object from writeConfig()>
         *      },
         *      "voiceChannels": [
         *          <Peer ID>,
         *      ]
         *  }
         * \endcode
         * @param peerList List of active peers.
         * @returns bool True, if list was sent, otherwise false.
         */
        bool writePeerLinkPeers(json::array* peerList);
        /**
         * @brief Writes a complete update of this CFNE's known spanning tree upstream to the network.
         * \code{.unparsed}
         *  Below is the representation of the data layout for the spanning tree message.
         *  The spanning tree message is a JSON body, and is a packet buffer compressed message.
         * 
         *  Byte 0               1               2               3
         *  Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Protocol Tag (REPL)                                           |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Reserved                                                      |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Variable Length JSON Payload ................................ |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * 
         * The JSON payload is variable length and looks like this:
         *  {
         *      "id": <FNE Peer ID>,
         *      "masterId": <FNE Master ID>,
         *      "identity": "<FNE Peer Identity>",
         *      "children": [
         *          "id": <FNE Peer ID>,
         *          "masterId": <FNE Master ID>,
         *          "identity": "<FNE Peer Identity>",
         *          "children": [],
         *      ]
         *  }
         * \endcode
         * @param treeRoot Root of the master tree.
         * @returns bool True, if list was sent, otherwise false.
         */
        bool writeSpanningTree(SpanningTree* treeRoot);
        /**
         * @brief Writes a complete update of this CFNE's HA parameters to the network.
         * \code{.unparsed}
         *  Below is the representation of the data layout for the repeater/end point login message.
         *  The message is variable bytes in length.
         * 
         *  Byte 0               1               2               3
         *  Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Total length of all included entries                          |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Entry: Peer ID                                                |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Entry: IP Address                                             |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Entry: Port                   |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * \endcode
         * @param haParams List of HA parameters.
         * @returns bool True, if list was sent, otherwise false.
         */
        bool writeHAParams(std::vector<HAParameters>& haParams);

        /**
         * @brief Returns flag indicating whether or not this peer connection is peer replication enabled.
         * @returns bool True, if peer replication enabled, otherwise false.
         */
        bool isReplica() const { return m_peerReplica; }

        /**
         * @brief Enables the option that will save replicated ACL data to the local ACL files.
         * @param enabled Flag to enable replicated ACL data saving.
         */
        void setPeerReplicationSaveACL(bool enabled) { m_peerReplicaSavesACL = enabled; }

        /**
         * @brief Gets the remote peer ID.
         * @returns uint32_t Remote Peer ID.
         */
        uint32_t getRemotePeerId() const { return m_remotePeerId; }

    public:
        /**
         * @brief Flag indicating whether or not this peer network has a key response handler attached.
         */
        DECLARE_PROPERTY(bool, attachedKeyRSPHandler, AttachedKeyRSPHandler);

    protected:
        /**
         * @brief DMR Protocol Callback.
         *  (This is called when the master sends a DMR packet.)
         */
        std::function<void(PeerNetwork* peer, const uint8_t* data, uint32_t length, uint32_t streamId, const frame::RTPFNEHeader& fneHeader, const frame::RTPHeader& rtpHeader)> m_dmrCallback;
        /**
         * @brief P25 Protocol Callback.
         *  (This is called when the master sends a P25 packet.)
         */
        std::function<void(PeerNetwork* peer, const uint8_t* data, uint32_t length, uint32_t streamId, const frame::RTPFNEHeader& fneHeader, const frame::RTPHeader& rtpHeader)> m_p25Callback;
        /**
         * @brief NXDN Protocol Callback.
         *  (This is called when the master sends a NXDN packet.)
         */
        std::function<void(PeerNetwork* peer, const uint8_t* data, uint32_t length, uint32_t streamId, const frame::RTPFNEHeader& fneHeader, const frame::RTPHeader& rtpHeader)> m_nxdnCallback;
        /**
         * @brief Analog Protocol Callback.
         *  (This is called when the master sends a analog packet.)
         */
        std::function<void(PeerNetwork* peer, const uint8_t* data, uint32_t length, uint32_t streamId, const frame::RTPFNEHeader& fneHeader, const frame::RTPHeader& rtpHeader)> m_analogCallback;

        /**
         * @brief Network Tree Disconnect Callback.
         */
        std::function<void(PeerNetwork* peer, const uint32_t offendingPeerId)> m_netTreeDiscCallback;
        /**
         * @brief Peer Replica Notification Callback.
         */
        std::function<void(PeerNetwork* peer)> m_peerReplicaCallback;

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
        uint32_t m_masterPeerId;

        lookups::PeerListLookup* m_pidLookup;
        bool m_peerReplica;
        bool m_peerReplicaSavesACL;

        PacketBuffer m_tgidPkt;
        PacketBuffer m_ridPkt;
        PacketBuffer m_pidPkt;

        ThreadPool m_threadPool;

        uint32_t m_prevSpanningTreeChildren;

        /**
         * @brief Entry point to process a given network packet.
         * @param req Instance of the PeerPacketRequest structure.
         */
        static void taskNetworkRx(PeerPacketRequest* req);
    };
} // namespace network

#endif // __PEER_NETWORK_H__
