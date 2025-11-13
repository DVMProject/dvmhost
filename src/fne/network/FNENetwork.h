// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023-2025 Bryan Biedenkapp, N2PLL
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
#include "common/concurrent/unordered_map.h"
#include "common/concurrent/shared_unordered_map.h"
#include "common/json/json.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "common/lookups/PeerListLookup.h"
#include "common/lookups/AdjSiteMapLookup.h"
#include "common/network/BaseNetwork.h"
#include "common/network/Network.h"
#include "common/network/PacketBuffer.h"
#include "common/ThreadPool.h"
#include "fne/lookups/AffiliationLookup.h"
#include "fne/network/influxdb/InfluxDB.h"
#include "fne/network/FNEPeerConnection.h"
#include "fne/network/SpanningTree.h"
#include "fne/network/HAParameters.h"
#include "fne/CryptoContainer.h"

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
namespace network { class HOST_SW_API P25OTARService; }
namespace network { namespace callhandler { class HOST_SW_API TagNXDNData; } }
namespace network { namespace callhandler { class HOST_SW_API TagAnalogData; } }

namespace network
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    #define MAX_QUEUED_PEER_MSGS 5U

    /**
     * @brief DVM states.
     */
    enum DVM_STATE {
        STATE_IDLE = 0U,        //!< Idle
        // DMR
        STATE_DMR = 1U,         //!< Digital Mobile Radio
        // Project 25
        STATE_P25 = 2U,         //!< Project 25
        // NXDN
        STATE_NXDN = 3U,        //!< NXDN
    };

    #define INFLUXDB_ERRSTR_DISABLED_SRC_RID "disabled source RID"
    #define INFLUXDB_ERRSTR_DISABLED_DST_RID "disabled destination RID"
    #define INFLUXDB_ERRSTR_INV_TALKGROUP "illegal/invalid talkgroup"
    #define INFLUXDB_ERRSTR_DISABLED_TALKGROUP "disabled talkgroup"
    #define INFLUXDB_ERRSTR_INV_SLOT "invalid slot for talkgroup"
    #define INFLUXDB_ERRSTR_RID_NOT_PERMITTED "RID not permitted for talkgroup"
    #define INFLUXDB_ERRSTR_ILLEGAL_RID_ACCESS "illegal/unknown RID attempted access"

    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    class HOST_SW_API DiagNetwork;
    class HOST_SW_API FNENetwork;

    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents the data required for a network metadata update request thread.
     * @ingroup fne_network
     */
    struct MetadataUpdateRequest : thread_t {
        uint32_t peerId;        //!< Peer ID for this request.
    };

    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents the data required for a network packet handler thread.
     * @ingroup fne_network
     */
    struct NetPacketRequest : thread_t {
        uint32_t peerId;                    //!< Peer ID for this request.
        void* diagObj;                      //!< Network diagnostics network object.

        sockaddr_storage address;           //!< IP Address and Port. 
        uint32_t addrLen;                   //!< 
        frame::RTPHeader rtpHeader;         //!< RTP Header
        frame::RTPFNEHeader fneHeader;      //!< RTP FNE Header
        int length = 0U;                    //!< Length of raw data buffer
        uint8_t* buffer = nullptr;          //!< Raw data buffer

        uint64_t pktRxTime;                 //!< Packet receive time
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
         * @param identity Textual identity of this FNE (this is used when peering with upstream FNEs).
         * @param debug Flag indicating whether network debug is enabled.
         * @param kmfDebug Flag indicating whether P25 OTAR KMF services debug is enabled.
         * @param verbose Flag indicating whether network verbose logging is enabled.
         * @param reportPeerPing Flag indicating whether peer pinging is reported.
         * @param dmr Flag indicating whether DMR is enabled.
         * @param p25 Flag indicating whether P25 is enabled.
         * @param nxdn Flag indicating whether NXDN is enabled.
         * @param analog Flag indicating whether analog is enabled.
         * @param parrotDelay Delay for end of call to parrot TG playback.
         * @param parrotGrantDemand Flag indicating whether a parrot TG will generate a grant demand.
         * @param allowActivityTransfer Flag indicating that the system activity logs will be sent to the network.
         * @param allowDiagnosticTransfer Flag indicating that the system diagnostic logs will be sent to the network.
         * @param pingTime 
         * @param updateLookupTime 
         * @param workerCnt Number of worker threads.
         */
        FNENetwork(HostFNE* host, const std::string& address, uint16_t port, uint32_t peerId, const std::string& password,
            std::string identity, bool debug, bool kmfDebug, bool verbose, bool reportPeerPing,
            bool dmr, bool p25, bool nxdn, bool analog,
            uint32_t parrotDelay, bool parrotGrantDemand, bool allowActivityTransfer, bool allowDiagnosticTransfer, 
            uint32_t pingTime, uint32_t updateLookupTime, uint16_t workerCnt);
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
         * @brief Gets the instance of the analog call handler.
         * @returns callhandler::TagAnalogData* Instance of the TagAnalogData call handler.
         */
        callhandler::TagAnalogData* analogTrafficHandler() const { return m_tagAnalog; }

        /**
         * @brief Sets the instances of the Radio ID, Talkgroup ID Peer List, and Crypto lookup tables.
         * @param ridLookup Radio ID Lookup Table Instance
         * @param tidLookup Talkgroup Rules Lookup Table Instance
         * @param peerListLookup Peer List Lookup Table Instance
         * @param cryptoLookup Crypto Container Lookup Table Instance
         * @param adjSiteMapLookup Adjacent Site Map Lookup Table Instance
         */
        void setLookups(lookups::RadioIdLookup* ridLookup, lookups::TalkgroupRulesLookup* tidLookup, lookups::PeerListLookup* peerListLookup,
            CryptoContainer* cryptoLookup, lookups::AdjSiteMapLookup* adjSiteMapLookup);
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
         * @brief Process network tree disconnect notification.
         * @param offendingPeerId Offending Peer ID.
         */
        void processNetworkTreeDisconnect(uint32_t peerId, uint32_t offendingPeerId);

        /**
         * @brief Helper to process an downstream peer In-Call Control message.
         * @param command In-Call Control Command.
         * @param subFunc Network Sub-Function.
         * @param dstId Destination ID.
         * @param slotNo Slot Number.
         * @param peerId Peer ID.
         * @param ssrc RTP synchronization source ID.
         * @param streamId Stream ID.
         */
        void processDownstreamInCallCtrl(network::NET_ICC::ENUM command, network::NET_SUBFUNC::ENUM subFunc, uint32_t dstId, 
            uint8_t slotNo, uint32_t peerId, uint32_t ssrc, uint32_t streamId);

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
         * @brief Helper to create a JSON representation of a FNE peer connection.
         * @param peerId Peer ID.
         * @param conn FNE Peer Connection.
         * @return json::object 
         */
        json::object fneConnObject(uint32_t peerId, FNEPeerConnection* conn);

        /**
         * @brief Helper to reset a peer connection.
         * @param peerId Peer ID to reset.
         * @returns bool True, if connection state is reset, otherwise false.
         */
        bool resetPeer(uint32_t peerId);

        /**
         * @brief Helper to set the master is upstream peer replica flag.
         * @param replica Flag indicating the master is a peer replica.
         */
        void setPeerReplica(bool replica);

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
        friend class callhandler::TagAnalogData;
        callhandler::TagAnalogData* m_tagAnalog;

        friend class P25OTARService;
        P25OTARService* m_p25OTARService;

        friend class ::RESTAPI;
        HostFNE* m_host;

        std::string m_address;
        uint16_t m_port;

        std::string m_password;

        bool m_isReplica;

        bool m_dmrEnabled;
        bool m_p25Enabled;
        bool m_nxdnEnabled;
        bool m_analogEnabled;

        uint32_t m_parrotDelay;
        Timer m_parrotDelayTimer;
        bool m_parrotGrantDemand;
        bool m_parrotOnlyOriginating;

        bool m_kmfServicesEnabled;

        lookups::RadioIdLookup* m_ridLookup;
        lookups::TalkgroupRulesLookup* m_tidLookup;
        lookups::PeerListLookup* m_peerListLookup;
        lookups::AdjSiteMapLookup* m_adjSiteMapLookup;

        CryptoContainer* m_cryptoLookup;

        NET_CONN_STATUS m_status;

        typedef std::pair<const uint32_t, network::FNEPeerConnection*> PeerMapPair;
        concurrent::shared_unordered_map<uint32_t, FNEPeerConnection*> m_peers;
        concurrent::unordered_map<uint32_t, json::array> m_peerReplicaPeers;
        typedef std::pair<const uint32_t, lookups::AffiliationLookup*> PeerAffiliationMapPair;
        concurrent::unordered_map<uint32_t, fne_lookups::AffiliationLookup*> m_peerAffiliations;
        concurrent::unordered_map<uint32_t, std::vector<uint32_t>> m_ccPeerMap;
        static std::timed_mutex s_keyQueueMutex;
        std::unordered_map<uint32_t, uint16_t> m_peerReplicaKeyQueue;

        SpanningTree* m_treeRoot;
        std::mutex m_treeLock;

        concurrent::vector<HAParameters> m_peerReplicaHAParams;
        std::string m_advertisedHAAddress;
        uint16_t m_advertisedHAPort;
        bool m_haEnabled;

        Timer m_maintainenceTimer;
        Timer m_updateLookupTimer;
        Timer m_haUpdateTimer;

        uint32_t m_softConnLimit;

        bool m_enableSpanningTree;
        bool m_logSpanningTreeChanges;
        bool m_spanningTreeFastReconnect;

        uint32_t m_callCollisionTimeout;

        bool m_disallowAdjStsBcast;
        bool m_disallowExtAdjStsBcast;
        bool m_allowConvSiteAffOverride;
        bool m_disallowCallTerm;
        bool m_restrictGrantToAffOnly;
        bool m_restrictPVCallToRegOnly;
        bool m_enableRIDInCallCtrl;
        bool m_disallowInCallCtrl;
        bool m_rejectUnknownRID;

        bool m_maskOutboundPeerID;
        bool m_maskOutboundPeerIDForNonPL;

        bool m_filterTerminators;

        bool m_forceListUpdate;

        bool m_disallowU2U;
        std::vector<uint32_t> m_dropU2UPeerTable;

        bool m_enableInfluxDB;
        std::string m_influxServerAddress;
        uint16_t m_influxServerPort;
        std::string m_influxServerToken;
        std::string m_influxOrg;
        std::string m_influxBucket;
        bool m_influxLogRawData;
        influxdb::ServerInfo m_influxServer;

        ThreadPool m_threadPool;

        bool m_disablePacketData;
        bool m_dumpPacketData;
        bool m_verbosePacketData;

        bool m_logDenials;
        bool m_logUpstreamCallStartEnd;
        bool m_reportPeerPing;
        bool m_verbose;

        /**
         * @brief Entry point to parrot handler thread.
         * @param arg Instance of the thread_t structure.
         * @returns void* (Ignore)
         */
        static void* threadParrotHandler(void* arg);
        /**
         * @brief Entry point to process a given network packet.
         * @param req Instance of the NetPacketRequest structure.
         */
        static void taskNetworkRx(NetPacketRequest* req);

        /**
         * @brief Checks if the passed peer ID is blocked from unit-to-unit traffic.
         * @param peerId Peer ID.
         * @returns bool True, if peer is blocked from unit-to-unit traffic, otherwise false.
         */
        bool checkU2UDroppedPeer(uint32_t peerId);

        /**
         * @brief Helper to dump the current spanning tree configuration to the log.
         * @param connection Instance of the FNEPeerConnection class.
         */
        void logSpanningTree(FNEPeerConnection* connection = nullptr);

        /**
         * @brief Erases a stream ID from the given peer ID connection.
         * @param peerId Peer ID.
         * @param streamId Stream ID.
         */
        void eraseStreamPktSeq(uint32_t peerId, uint32_t streamId);

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
         * @brief Helper to disconnect a downstream peer.
         * @param peerId Peer ID.
         * @param connection Instance of the FNEPeerConnection class.
         */
        void disconnectPeer(uint32_t peerId, FNEPeerConnection* connection);
        /**
         * @brief Helper to erase the peer from the peers list.
         * @note This does not delete or otherwise free the FNEConnection instance!
         * @param peerId Peer ID.
         * @returns bool True, if peer was deleted, otherwise false.
         */
        void erasePeer(uint32_t peerId);
        /**
         * @brief Helper to determine if the peer is local to this master.
         * @param peerId Peer ID.
         * @returns bool True, if peer is local, otherwise false.
         */
        bool isPeerLocal(uint32_t peerId);

        /**
         * @brief Helper to find the unit registration for the given source ID.
         * @param srcId Source Radio ID.
         * @returns uint32_t Peer ID, or 0 if not found.
         */
        uint32_t findPeerUnitReg(uint32_t srcId);

        /**
         * @brief Helper to resolve the peer ID to its identity string.
         * @param peerId Peer ID.
         * @returns std::string Textual peer name for the given peer ID.
         */
        std::string resolvePeerIdentity(uint32_t peerId);

        /**
         * @brief Helper to complete setting up a repeater login request.
         * @param peerId Peer ID.
         * @param streamId Stream ID for the login sequence.
         * @param connection Instance of the FNEPeerConnection class.
         */
        void setupRepeaterLogin(uint32_t peerId, uint32_t streamId, FNEPeerConnection* connection);

        /**
         * @brief Helper to process an In-Call Control message.
         * @param command In-Call Control Command.
         * @param subFunc Network Sub-Function.
         * @param dstId Destination ID.
         * @param slotNo Slot Number.
         * @param peerId Peer ID.
         * @param ssrc RTP synchronization source ID.
         * @param streamId Stream ID for this message.
         */
        void processInCallCtrl(network::NET_ICC::ENUM command, network::NET_SUBFUNC::ENUM subFunc, uint32_t dstId, 
            uint8_t slotNo, uint32_t peerId, uint32_t ssrc, uint32_t streamId);

        /**
         * @brief Helper to send the network metadata to the specified peer in a separate thread.
         * @param peerId Peer ID.
         */
        void peerMetadataUpdate(uint32_t peerId);
        /**
         * @brief Entry point to send the network metadata to the specified peer in a separate thread.
         * @param req Instance of the MetadataUpdateRequest structure.
         */
        static void taskMetadataUpdate(MetadataUpdateRequest* req);

        /*
        ** ACL Message Writing
        */

        /**
         * @brief Helper to send the list of whitelisted RIDs to the specified peer.
         * \code{.unparsed}
         *  Below is the representation of the data layout for the active/whitelisted RIDs message.
         *  The message is variable bytes in length. This layout does not apply for peer replication
         *  messages, as those messages are a packet buffered message of the entire RID ACL file.
         * 
         *  The RID ACL is chunked and sent in blocks of a maximum of 50 RIDs per message.
         * 
         *  Each radio ID ACL entry is 4 bytes.
         * 
         *  Byte 0               1               2               3
         *  Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Number of entries                                             |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Entry: Radio ID                                 |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * \endcode
         * @param peerId Peer ID.
         * @param streamId Stream ID for this message.
         * @param sendReplica Flag indicating the RID transfer is to an neighbor replica peer.
         */
        void writeWhitelistRIDs(uint32_t peerId, uint32_t streamId, bool sendReplica);
        /**
         * @brief Helper to send the list of blacklisted RIDs to the specified peer.
         * \code{.unparsed}
         *  Below is the representation of the data layout for the deactivated/blacklisted RIDs message.
         *  The message is variable bytes in length. 
         * 
         *  The RID ACL is chunked and sent in blocks of a maximum of 50 RIDs per message.
         * 
         *  Each radio ID ACL entry is 4 bytes.
         * 
         *  Byte 0               1               2               3
         *  Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Number of entries                                             |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Entry: Radio ID                                 |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * \endcode
         * @param peerId Peer ID.
         * @param streamId Stream ID for this message.
         */
        void writeBlacklistRIDs(uint32_t peerId, uint32_t streamId);
        /**
         * @brief Helper to send the list of active TGIDs to the specified peer.
         * \code{.unparsed}
         *  Below is the representation of the data layout for the active TGs message.
         *  The message is variable bytes in length. This layout does not apply for peer replication
         *  messages, as those messages are a packet buffered message of the entire talkgroup ACL file.
         * 
         *  Each talkgroup ACL entry is 5 bytes.
         * 
         *  Byte 0               1               2               3
         *  Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Number of entries                                             |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Entry: Talkgroup ID                             |N|A| Slot    |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * 
         *  N = Non-Preferred Flag
         *  A = Affiliated Flag
         * 
         * \endcode
         * @param peerId Peer ID.
         * @param streamId Stream ID for this message.
         * @param sendReplica Flag indicating the TGID transfer is to an neighbor replica peer.
         */
        void writeTGIDs(uint32_t peerId, uint32_t streamId, bool sendReplica);
        /**
         * @brief Helper to send the list of deactivated TGIDs to the specified peer.
         * \code{.unparsed}
         *  Below is the representation of the data layout for the deactivated TGs message.
         *  The message is variable bytes in length.
         * 
         *  Each talkgroup ACL entry is 5 bytes.
         * 
         *  Byte 0               1               2               3
         *  Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Number of entries                                             |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Entry: Talkgroup ID                             | R | Slot    |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * \endcode
         * @param peerId Peer ID.
         * @param streamId Stream ID for this message.
         */
        void writeDeactiveTGIDs(uint32_t peerId, uint32_t streamId);
        /**
         * @brief Helper to send the list of peers to the specified peer.
         * @note This doesn't have a data layout document because it is *only* sent as a packet buffered message.
         * @param peerId Peer ID.
         * @param streamId Stream ID for this message.
         */
        void writePeerList(uint32_t peerId, uint32_t streamId);
        /**
         * @brief Helper to send the HA parameters to the specified peer.
         * \code{.unparsed}
         *  Below is the representation of the data layout for the HA parameters message.
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
         * @param peerId Peer ID.
         * @param streamId Stream ID for this message.
         * @param sendReplica Flag indicating the HA transfer is to an neighbor replica peer.
         */
        void writeHAParameters(uint32_t peerId, uint32_t streamId, bool sendReplica);

        /**
         * @brief Helper to send a network tree disconnect to the specified peer.
         *  This will cause the peer to issue a link disconnect to the offending peer to prevent network loops.
         * \code{.unparsed}
         *  Below is the representation of the data layout for the tree disconnect message.
         *  The message is 4 bytes in length.
         * 
         *  Byte 0               1               2               3
         *  Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Offending Peer ID                                             |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * \endcode
         * @param peerId Peer ID.
         * @param offendingPeerId Offending Peer ID.
         */
        void writeTreeDisconnect(uint32_t peerId, uint32_t offendingPeerId);

        /**
         * @brief Helper to send a In-Call Control command to the specified peer.
         * \code{.unparsed}
         *  Below is the representation of the data layout for the In-Call control message.
         *  The message is 15 bytes in length.
         * 
         *  Byte 0               1               2               3
         *  Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Reserved                                                      |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      |                               | Peer ID                       |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Peer ID                       | ICC Command   | Destination   |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Destination ID                | Slot          |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * \endcode
         * @param peerId Peer ID.
         * @param streamId Stream ID for this message.
         * @param subFunc Network Sub-Function.
         * @param command In-Call Control Command.
         * @param dstId Destination ID.
         * @param slotNo DMR slot.
         * @param systemReq Flag indicating the ICC request is a system generated one not a automatic RID rule generated one.
         * @param toUpstream Flag indicating the ICC request is directed at an upstream peer.
         * @param ssrc RTP synchronization source ID.
         */
        bool writePeerICC(uint32_t peerId, uint32_t streamId, NET_SUBFUNC::ENUM subFunc = NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR, 
            NET_ICC::ENUM command = NET_ICC::NOP, uint32_t dstId = 0U, uint8_t slotNo = 0U, bool systemReq = false, bool toUpstream = false,
            uint32_t ssrc = 0U);

        /*
        ** Generic Message Writing
        */

        /**
         * @brief Helper to send a data message to the specified peer with a explicit packet sequence.
         * @param peerId Destination Peer ID.
         * @param ssrc RTP synchronization source ID.
         * @param opcode FNE network opcode pair.
         * @param[in] data Buffer containing message to send to peer.
         * @param length Length of buffer.
         * @param pktSeq RTP packet sequence for this message.
         * @param streamId Stream ID for this message.
         * @param incPktSeq Flag indicating the message should increment the packet sequence after transmission.
         */
        bool writePeer(uint32_t peerId, uint32_t ssrc, FrameQueue::OpcodePair opcode, const uint8_t* data, uint32_t length, 
            uint16_t pktSeq, uint32_t streamId, bool incPktSeq = false) const;
        /**
         * @brief Helper to queue a data message to the specified peer with a explicit packet sequence.
         * @param[in] buffers Buffer to contain queued messages.
         * @param peerId Destination Peer ID.
         * @param ssrc RTP synchronization source ID.
         * @param opcode FNE network opcode pair.
         * @param[in] data Buffer containing message to send to peer.
         * @param length Length of buffer.
         * @param pktSeq RTP packet sequence for this message.
         * @param streamId Stream ID for this message.
         * @param queueOnly Flag indicating this message should be queued for transmission.
         * @param incPktSeq Flag indicating the message should increment the packet sequence after transmission.
         * @param directWrite Flag indicating this message should be immediately directly written.
         */
        bool writePeerQueue(udp::BufferQueue* buffers, uint32_t peerId, uint32_t ssrc, FrameQueue::OpcodePair opcode, 
            const uint8_t* data, uint32_t length, uint16_t pktSeq, uint32_t streamId, bool incPktSeq = false) const;

        /**
         * @brief Helper to send a command message to the specified peer.
         * @param peerId Peer ID.
         * @param opcode FNE network opcode pair.
         * @param[in] data Buffer containing message to send to peer.
         * @param length Length of buffer.
         * @param streamId Stream ID for this message.
         * @param incPktSeq Flag indicating the message should increment the packet sequence after transmission.
         */
        bool writePeerCommand(uint32_t peerId, FrameQueue::OpcodePair opcode, const uint8_t* data, uint32_t length, 
            uint32_t streamId, bool incPktSeq) const;

        /**
         * @brief Helper to send a ACK response to the specified peer.
         * @param peerId Peer ID.
         * @param streamId Stream ID for this message.
         * @param[in] data Buffer containing response data to send to peer.
         * @param length Length of buffer.
         */
        bool writePeerACK(uint32_t peerId, uint32_t streamId, const uint8_t* data = nullptr, uint32_t length = 0U);

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
         * @param streamId Stream ID for this message.
         * @param tag Tag.
         * @param reason NAK reason.
         */
        bool writePeerNAK(uint32_t peerId, uint32_t streamId, const char* tag, NET_CONN_NAK_REASON reason = NET_CONN_NAK_GENERAL_FAILURE);
        /**
         * @brief Helper to send a NAK response to the specified peer.
         * @param peerId Peer ID.
         * @param tag Tag.
         * @param reason NAK reason.
         * @param addr IP Address and Port.
         * @param addrLen 
         */
        bool writePeerNAK(uint32_t peerId, const char* tag, NET_CONN_NAK_REASON reason, sockaddr_storage& addr, uint32_t addrLen);

        /*
        ** Internal KMM Callback.
        */

        /**
         * @brief Helper to process a FNE KMM TEK response.
         * @param ki Key Item.
         * @param algId Algorithm ID.
         * @param keyLength Length of key in bytes.
         */
        void processTEKResponse(p25::kmm::KeyItem* ki, uint8_t algId, uint8_t keyLength);
    };
} // namespace network

#endif // __FNE_NETWORK_H__
