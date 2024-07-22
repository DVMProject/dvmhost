// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023-2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup fne_network FNE Networking
 * @brief Implementation for the FNE networking.
 * @ingroup fne
 *
 * @defgroup fne_callhandler Call Handlers
 * @brief Implementation for the FNE call handlers.
 * @ingroup fne_network
 *
 * @file FNENetwork.h
 * @ingroup fne_network
 * @file FNENetwork.cpp
 * @ingroup fne_network
 */
#if !defined(__FNE_NETWORK_H__)
#define __FNE_NETWORK_H__

#include "fne/Defines.h"
#include "common/network/BaseNetwork.h"
#include "common/network/json/json.h"
#include "common/lookups/AffiliationLookup.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "common/lookups/PeerListLookup.h"
#include "fne/network/influxdb/InfluxDB.h"
#include "host/network/Network.h"

#include <string>
#include <cstdint>
#include <unordered_map>
#include <mutex>

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------

class HOST_SW_API HostFNE;
class HOST_SW_API RESTAPI;
namespace network { namespace callhandler { class HOST_SW_API TagDMRData; } }
namespace network { namespace callhandler { namespace packetdata { class HOST_SW_API DMRPacketData; } } }
namespace network { namespace callhandler { class HOST_SW_API TagP25Data; } }
namespace network { namespace callhandler { namespace packetdata { class HOST_SW_API P25PacketData; } } }
namespace network { namespace callhandler { class HOST_SW_API TagNXDNData; } }

namespace network
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    /**
     * @brief DVM states.
     */
    enum DVM_STATE {
        STATE_IDLE = 0U,        //! Idle
        // DMR
        STATE_DMR = 1U,         //! Digital Mobile Radio
        // Project 25
        STATE_P25 = 2U,         //! Project 25
        // NXDN
        STATE_NXDN = 3U,        //! NXDN
    };

    #define INFLUXDB_ERRSTR_DISABLED_SRC_RID "disabled source RID"
    #define INFLUXDB_ERRSTR_DISABLED_DST_RID "disabled destination RID"
    #define INFLUXDB_ERRSTR_INV_TALKGROUP "illegal/invalid talkgroup"
    #define INFLUXDB_ERRSTR_DISABLED_TALKGROUP "disabled talkgroup"
    #define INFLUXDB_ERRSTR_INV_SLOT "invalid slot for talkgroup"

    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    class HOST_SW_API DiagNetwork;
    class HOST_SW_API FNENetwork;

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents an peer connection to the FNE.
     * @ingroup fne_network
     */
    class HOST_SW_API FNEPeerConnection {
    public:
        auto operator=(FNEPeerConnection&) -> FNEPeerConnection& = delete;
        auto operator=(FNEPeerConnection&&) -> FNEPeerConnection& = delete;
        FNEPeerConnection(FNEPeerConnection&) = delete;

        /**
         * @brief Initializes a new instance of the FNEPeerConnection class.
         */
        FNEPeerConnection() :
            m_id(0U),
            m_ccPeerId(0U),
            m_currStreamId(0U),
            m_socketStorage(),
            m_sockStorageLen(0U),
            m_address(),
            m_port(),
            m_salt(0U),
            m_connected(false),
            m_connectionState(NET_STAT_INVALID),
            m_pingsReceived(0U),
            m_lastPing(0U),
            m_lastACLUpdate(0U),
            m_isExternalPeer(false),
            m_config(),
            m_pktLastSeq(RTP_END_OF_CALL_SEQ),
            m_pktNextSeq(1U)
        {
            /* stub */
        }
        /**
         * @brief Initializes a new instance of the FNEPeerConnection class.
         * @param id Unique ID of this modem on the network.
         * @param socketStorage 
         * @param sockStorageLen 
         */
        FNEPeerConnection(uint32_t id, sockaddr_storage& socketStorage, uint32_t sockStorageLen) :
            m_id(id),
            m_ccPeerId(0U),
            m_currStreamId(0U),
            m_socketStorage(socketStorage),
            m_sockStorageLen(sockStorageLen),
            m_address(udp::Socket::address(socketStorage)),
            m_port(udp::Socket::port(socketStorage)),
            m_salt(0U),
            m_connected(false),
            m_connectionState(NET_STAT_INVALID),
            m_pingsReceived(0U),
            m_lastPing(0U),
            m_lastACLUpdate(0U),
            m_isExternalPeer(false),
            m_config(),
            m_pktLastSeq(RTP_END_OF_CALL_SEQ),
            m_pktNextSeq(1U)
        {
            assert(id > 0U);
            assert(sockStorageLen > 0U);
            assert(!m_address.empty());
            assert(m_port > 0U);
        }

    public:
        /**
         * @brief Peer ID.
         */
        __PROPERTY_PLAIN(uint32_t, id);
        /**
         * @brief Peer Identity.
         */
        __PROPERTY_PLAIN(std::string, identity);

        /**
         * @brief Control Channel Peer ID.
         */
        __PROPERTY_PLAIN(uint32_t, ccPeerId);

        /**
         * @brief Current Stream ID.
         */
        __PROPERTY_PLAIN(uint32_t, currStreamId);

        /**
         * @brief Unix socket storage containing the connected address.
         */
        __PROPERTY_PLAIN(sockaddr_storage, socketStorage);
        /**
         * @brief Length of the sockaddr_storage structure.
         */
        __PROPERTY_PLAIN(uint32_t, sockStorageLen);

        /**
         * @brief         */
        __PROPERTY_PLAIN(std::string, address);
        /**
         * @brief Port number peer connected with.
         */
        __PROPERTY_PLAIN(uint16_t, port);

        /**
         * @brief Salt value used for peer authentication.
         */
        __PROPERTY_PLAIN(uint32_t, salt);

        /**
         * @brief Flag indicating whether or not the peer is connected.
         */
        __PROPERTY_PLAIN(bool, connected);
        /**
         * @brief Connection state.
         */
        __PROPERTY_PLAIN(NET_CONN_STATUS, connectionState);

        /**
         * @brief Number of pings received.
         */
        __PROPERTY_PLAIN(uint32_t, pingsReceived);
        /**
         * @brief Last ping received.
         */
        __PROPERTY_PLAIN(uint64_t, lastPing);

        /**
         * @brief Last ACL update sent.
         */
        __PROPERTY_PLAIN(uint64_t, lastACLUpdate);

        /**
         * @brief Flag indicating this connection is from an external peer.
         */
        __PROPERTY_PLAIN(bool, isExternalPeer);
        /**
         * @brief Flag indicating this connection is from an conventional peer.
         * 
         * 
         */
        __PROPERTY_PLAIN(bool, isConventionalPeer);

        /**
         * @brief JSON objecting containing peer configuration information.
         */
        __PROPERTY_PLAIN(json::object, config);

        /**
         * @brief Last received RTP sequence.
         */
        __PROPERTY_PLAIN(uint16_t, pktLastSeq);
        /**
         * @brief Calculated next RTP sequence.
         */
        __PROPERTY_PLAIN(uint16_t, pktNextSeq);
    };

    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents the data required for a peer ACL update request thread.
     * @ingroup fne_network
     */
    struct ACLUpdateRequest : thread_t {
        uint32_t peerId;        //! Peer ID for this request.
    };

    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents the data required for a network packet handler thread.
     * @ingroup fne_network
     */
    struct NetPacketRequest : thread_t {
        uint32_t peerId;                    //! Peer ID for this request.

        sockaddr_storage address;           //! IP Address and Port. 
        uint32_t addrLen;                   //!
        frame::RTPHeader rtpHeader;         //! RTP Header
        frame::RTPFNEHeader fneHeader;      //! RTP FNE Header
        int length = 0U;                    //! Length of raw data buffer
        uint8_t *buffer;                    //! Raw data buffer
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements the core FNE networking logic.
     * @ingroup fne_network
     */
    class HOST_SW_API FNENetwork : public BaseNetwork {
    public:
        /**
         * @brief Initializes a new instance of the FNENetwork class.
         * @param host Instance of the HostFNE class.
         * @param address Network Hostname/IP address to listen on.
         * @param port Network port number.
         * @param peerId Unique ID on the network.
         * @param password Network authentication password.
         * @param debug Flag indicating whether network debug is enabled.
         * @param verbose Flag indicating whether network verbose logging is enabled.
         * @param reportPeerPing Flag indicating whether peer pinging is reported.
         * @param dmr Flag indicating whether DMR is enabled.
         * @param p25 Flag indicating whether P25 is enabled.
         * @param nxdn Flag indicating whether NXDN is enabled.
         * @param parrotDelay Delay for end of call to parrot TG playback.
         * @param parrotGrantDemand Flag indicating whether a parrot TG will generate a grant demand.
         * @param allowActivityTransfer Flag indicating that the system activity logs will be sent to the network.
         * @param allowDiagnosticTransfer Flag indicating that the system diagnostic logs will be sent to the network.
         * @param pingTime 
         * @param updateLookupTime 
         */
        FNENetwork(HostFNE* host, const std::string& address, uint16_t port, uint32_t peerId, const std::string& password,
            bool debug, bool verbose, bool reportPeerPing, bool dmr, bool p25, bool nxdn, uint32_t parrotDelay, bool parrotGrantDemand,
            bool allowActivityTransfer, bool allowDiagnosticTransfer, uint32_t pingTime, uint32_t updateLookupTime);
        /**
         * @brief Finalizes a instance of the FNENetwork class.
         */
        ~FNENetwork() override;

        /**
         * @brief Helper to set configuration options.
         * @param conf Instance of the yaml::Node class.
         * @param printOptions Flag indicating whether or not options should be printed to log.
         */
        void setOptions(yaml::Node& conf, bool printOptions);

        /**
         * @brief Gets the current status of the network.
         * @returns NET_CONN_STATUS Current network status.
         */
        NET_CONN_STATUS getStatus() { return m_status; }

        /**
         * @brief Gets the instance of the DMR call handler.
         * @returns callhandler::TagDMRData* Instance of the TagDMRData call handler.
         */
        callhandler::TagDMRData* dmrTrafficHandler() const { return m_tagDMR; }
        /**
         * @brief Gets the instance of the P25 call handler.
         * @returns callhandler::TagP25Data* Instance of the TagP25Data call handler.
         */
        callhandler::TagP25Data* p25TrafficHandler() const { return m_tagP25; }
        /**
         * @brief Gets the instance of the NXDN call handler.
         * @returns callhandler::TagNXDNData* Instance of the TagNXDNData call handler.
         */
        callhandler::TagNXDNData* nxdnTrafficHandler() const { return m_tagNXDN; }

        /**
         * @brief Sets the instances of the Radio ID, Talkgroup ID and Peer List lookup tables.
         * @param ridLookup Radio ID Lookup Table Instance
         * @param tidLookup Talkgroup Rules Lookup Table Instance
         * @param peerListLookup Peer List Lookup Table Instance
         */
        void setLookups(lookups::RadioIdLookup* ridLookup, lookups::TalkgroupRulesLookup* tidLookup, lookups::PeerListLookup* peerListLookup);
        /**
         * @brief Sets endpoint preshared encryption key.
         * @param presharedKey Encryption preshared key for networking.
         */
        void setPresharedKey(const uint8_t* presharedKey);

        /**
         * @brief Process data frames from the network.
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

        /**
         * @brief Helper to reset a peer connection.
         * @param peerId Peer ID to reset.
         * @returns bool True, if connection state is reset, otherwise false.
         */
        bool resetPeer(uint32_t peerId);

    private:
        friend class DiagNetwork;
        friend class callhandler::TagDMRData;
        friend class callhandler::packetdata::DMRPacketData;
        callhandler::TagDMRData* m_tagDMR;
        friend class callhandler::TagP25Data;
        friend class callhandler::packetdata::P25PacketData;
        callhandler::TagP25Data* m_tagP25;
        friend class callhandler::TagNXDNData;
        callhandler::TagNXDNData* m_tagNXDN;
        
        friend class ::RESTAPI;
        HostFNE* m_host;

        std::string m_address;
        uint16_t m_port;

        std::string m_password;

        bool m_dmrEnabled;
        bool m_p25Enabled;
        bool m_nxdnEnabled;

        uint32_t m_parrotDelay;
        Timer m_parrotDelayTimer;
        bool m_parrotGrantDemand;
        bool m_parrotOnlyOriginating;

        lookups::RadioIdLookup* m_ridLookup;
        lookups::TalkgroupRulesLookup* m_tidLookup;
        lookups::PeerListLookup* m_peerListLookup;

        NET_CONN_STATUS m_status;

        static std::mutex m_peerMutex;
        typedef std::pair<const uint32_t, network::FNEPeerConnection*> PeerMapPair;
        std::unordered_map<uint32_t, FNEPeerConnection*> m_peers;
        typedef std::pair<const uint32_t, lookups::AffiliationLookup*> PeerAffiliationMapPair;
        std::unordered_map<uint32_t, lookups::AffiliationLookup*> m_peerAffiliations;
        std::unordered_map<uint32_t, std::vector<uint32_t>> m_ccPeerMap;

        Timer m_maintainenceTimer;

        uint32_t m_updateLookupTime;
        uint32_t m_softConnLimit;

        bool m_callInProgress;

        bool m_disallowAdjStsBcast;
        bool m_disallowExtAdjStsBcast;
        bool m_allowConvSiteAffOverride;
        bool m_restrictGrantToAffOnly;

        bool m_filterHeaders;
        bool m_filterTerminators;

        bool m_forceListUpdate;

        std::vector<uint32_t> m_dropU2UPeerTable;

        bool m_enableInfluxDB;
        std::string m_influxServerAddress;
        uint16_t m_influxServerPort;
        std::string m_influxServerToken;
        std::string m_influxOrg;
        std::string m_influxBucket;
        bool m_influxLogRawData;
        influxdb::ServerInfo m_influxServer;

        bool m_disablePacketData;
        bool m_dumpDataPacket;

        bool m_reportPeerPing;
        bool m_verbose;

        /**
         * @brief Entry point to process a given network packet.
         * @param arg Instance of the NetPacketRequest structure.
         * @returns void* (Ignore)
         */
        static void* threadedNetworkRx(void* arg);

        /**
         * @brief Checks if the passed peer ID is blocked from unit-to-unit traffic.
         * @param peerId Peer ID.
         * @returns bool True, if peer is blocked from unit-to-unit traffic, otherwise false.
         */
        bool checkU2UDroppedPeer(uint32_t peerId);

        /**
         * @brief Helper to create a peer on the peers affiliations list.
         * @param peerId Peer ID.
         * @param peerName Textual peer name for the given peer ID.
         */
        void createPeerAffiliations(uint32_t peerId, std::string peerName);
        /**
         * @brief Helper to erase the peer from the peers affiliations list.
         * @param peerId Peer ID.
         * @returns bool True, if the peer affiliations were deleted, otherwise false.
         */
        bool erasePeerAffiliations(uint32_t peerId);
        /**
         * @brief Helper to erase the peer from the peers list.
         * @param peerId Peer ID.
         * @returns bool True, if peer was deleted, otherwise false.
         */
        bool erasePeer(uint32_t peerId);

        /**
         * @brief Helper to resolve the peer ID to its identity string.
         * @param peerId Peer ID.
         * @returns std::string Textual peer name for the given peer ID.
         */
        std::string resolvePeerIdentity(uint32_t peerId);

        /**
         * @brief Helper to complete setting up a repeater login request.
         * @param peerId Peer ID.
         * @param connection Instance of the FNEPeerConnection class.
         */
        void setupRepeaterLogin(uint32_t peerId, FNEPeerConnection* connection);

        /**
         * @brief Helper to send the ACL lists to the specified peer in a separate thread.
         * @param peerId Peer ID.
         */
        void peerACLUpdate(uint32_t peerId);
        /**
         * @brief Entry point to send the ACL lists to the specified peer in a separate thread.
         * @param arg Instance of the ACLUpdateRequest structure.
         * @returns void* (Ignore)
         */
        static void* threadedACLUpdate(void* arg);

        /**
         * @brief Helper to send the list of whitelisted RIDs to the specified peer.
         * @param peerId Peer ID.
         */
        void writeWhitelistRIDs(uint32_t peerId);
        /**
         * @brief Helper to send the list of blacklisted RIDs to the specified peer.
         * @param peerId Peer ID.
         */
        void writeBlacklistRIDs(uint32_t peerId);
        /**
         * @brief Helper to send the list of active TGIDs to the specified peer.
         * @param peerId Peer ID.
         */
        void writeTGIDs(uint32_t peerId);
        /**
         * @brief Helper to send the list of deactivated TGIDs to the specified peer.
         * @param peerId Peer ID.
         */
        void writeDeactiveTGIDs(uint32_t peerId);

        /**
         * @brief Helper to send a data message to the specified peer.
         * @param peerId Peer ID.
         * @param opcode FNE network opcode pair.
         * @param[in] data Buffer containing message to send to peer.
         * @param length Length of buffer.
         * @param pktSeq RTP packet sequence for this message.
         * @param streamId Stream ID for this message.
         * @param queueOnly Flag indicating this message should be queued for transmission.
         * @param directWrite Flag indicating this message should be immediately directly written.
         */
        bool writePeer(uint32_t peerId, FrameQueue::OpcodePair opcode, const uint8_t* data, uint32_t length, 
            uint16_t pktSeq, uint32_t streamId, bool queueOnly = false, bool directWrite = false) const;
        /**
         * @brief Helper to send a data message to the specified peer.
         * @param peerId Peer ID.
         * @param opcode FNE network opcode pair.
         * @param[in] data Buffer containing message to send to peer.
         * @param length Length of buffer.
         * @param streamId Stream ID for this message.
         * @param queueOnly Flag indicating this message should be queued for transmission.
         * @param incPktSeq Flag indicating the message should increment the packet sequence after transmission.
         * @param directWrite Flag indicating this message should be immediately directly written.
         */
        bool writePeer(uint32_t peerId, FrameQueue::OpcodePair opcode, const uint8_t* data, uint32_t length, 
            uint32_t streamId, bool queueOnly = false, bool incPktSeq = false, bool directWrite = false) const;

        /**
         * @brief Helper to send a command message to the specified peer.
         * @param peerId Peer ID.
         * @param opcode FNE network opcode pair.
         * @param[in] data Buffer containing message to send to peer.
         * @param length Length of buffer.
         * @param incPktSeq Flag indicating the message should increment the packet sequence after transmission.
         */
        bool writePeerCommand(uint32_t peerId, FrameQueue::OpcodePair opcode, const uint8_t* data = nullptr, uint32_t length = 0U, 
            bool incPktSeq = false) const;

        /**
         * @brief Helper to send a ACK response to the specified peer.
         * @param peerId Peer ID.
         * @param[in] data Buffer containing response data to send to peer.
         * @param length Length of buffer.
         */
        bool writePeerACK(uint32_t peerId, const uint8_t* data = nullptr, uint32_t length = 0U);

        /**
         * @brief Helper to log a warning specifying which NAK reason is being sent a peer.
         * @param peerId Peer ID.
         * @param tag Tag.
         * @param reason NAK reason.
         */
        void logPeerNAKReason(uint32_t peerId, const char* tag, NET_CONN_NAK_REASON reason);
        /**
         * @brief Helper to send a NAK response to the specified peer.
         * @param peerId Peer ID.
         * @param tag Tag.
         * @param reason NAK reason.
         */
        bool writePeerNAK(uint32_t peerId, const char* tag, NET_CONN_NAK_REASON reason = NET_CONN_NAK_GENERAL_FAILURE);
        /**
         * @brief Helper to send a NAK response to the specified peer.
         * @param peerId Peer ID.
         * @param tag Tag.
         * @param reason NAK reason.
         * @param addr IP Address and Port.
         * @param addrLen 
         */
        bool writePeerNAK(uint32_t peerId, const char* tag, NET_CONN_NAK_REASON reason, sockaddr_storage& addr, uint32_t addrLen);
    };
} // namespace network

#endif // __FNE_NETWORK_H__
