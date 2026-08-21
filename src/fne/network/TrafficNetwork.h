// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023-2026 Bryan Biedenkapp, N2PLL
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
 * @file TrafficNetwork.h
 * @ingroup fne_network
 * @file TrafficNetwork.cpp
 * @ingroup fne_network
 */
#if !defined(__TRAFFIC_NETWORK_H__)
#define __TRAFFIC_NETWORK_H__

#include "fne/Defines.h"
#include "common/concurrent/unordered_map.h"
#include "common/concurrent/shared_unordered_map.h"
#include "common/json/json.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/RadioAliasLookup.h"
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

#include "fne/sqlite3/sqlite3.h"

#include <string>
#include <cstdint>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <array>

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------

#if defined(CATCH2_TEST_COMPILATION)
class FNETestHooks;
#endif

class HOST_SW_API HostFNE;
namespace fne_restapi { class HOST_SW_API RESTAPI; }
namespace network { namespace callhandler { class HOST_SW_API TagDMRData; } }
namespace network { namespace callhandler { namespace packetdata { class HOST_SW_API DMRPacketData; } } }
namespace network { namespace callhandler { class HOST_SW_API TagP25Data; } }
namespace network { namespace callhandler { namespace packetdata { class HOST_SW_API P25PacketData; } } }
namespace network { namespace callhandler { class HOST_SW_API TagP25P2Data; } }
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

    #define DB_ERRSTR_DISABLED_SRC_RID "disabled source RID"
    #define DB_ERRSTR_DISABLED_DST_RID "disabled destination RID"
    #define DB_ERRSTR_INV_TALKGROUP "illegal/invalid talkgroup"
    #define DB_ERRSTR_DISABLED_TALKGROUP "disabled talkgroup"
    #define DB_ERRSTR_ENC_TALKGROUP_CLR "encrypted talkgroup with clear traffic"
    #define DB_ERRSTR_CLR_TALKGROUP_ENC "clear talkgroup with encrypted traffic"
    #define DB_ERRSTR_INV_SLOT "invalid slot for talkgroup"
    #define DB_ERRSTR_RID_NOT_PERMITTED "RID not permitted for talkgroup"
    #define DB_ERRSTR_ILLEGAL_RID_ACCESS "illegal/unknown RID attempted access"
    #define DB_ERRSTR_CALL_NOT_PERMITTED "call not permitted for talkgroup"

    const uint32_t MAX_HARD_CONN_CAP = 250U;
    const size_t PEER_STATE_LOCK_STRIPES = 256U;

    const int32_t REPEATER_PCKT_HDR_LEN = 8;
    const int32_t REPEATER_AUTH_HASH_LEN = 32;
    const int32_t TRANSFER_PCKT_HDR_LEN = 11;

    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    class HOST_SW_API MetadataNetwork;
    class HOST_SW_API TrafficNetwork;

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
        void* metadataObj;                  //!< Network metadata network object.

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
     * @brief Implements the core traffic networking logic.
     * @ingroup fne_network
     */
    class HOST_SW_API TrafficNetwork : public BaseNetwork {
    public:
        /**
         * @brief Initializes a new instance of the TrafficNetwork class.
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
         * @param p25P2 Flag indicating whether P25 Phase 2 is enabled.
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
        TrafficNetwork(HostFNE* host, const std::string& address, uint16_t port, uint32_t peerId, const std::string& password,
            std::string identity, bool debug, bool kmfDebug, bool verbose, bool reportPeerPing,
            bool dmr, bool p25, bool p25P2, bool nxdn, bool analog,
            uint32_t parrotDelay, bool parrotGrantDemand, bool allowActivityTransfer, bool allowDiagnosticTransfer, 
            uint32_t pingTime, uint32_t updateLookupTime, uint16_t workerCnt);
        /**
         * @brief Finalizes a instance of the TrafficNetwork class.
         */
        ~TrafficNetwork() override;

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
         * @brief Gets the instance of the P25 Phase 2 call handler.
         */
        callhandler::TagP25P2Data* p25P2TrafficHandler() const { return m_tagP25P2; }
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
         * @brief Sets the instances of the Radio ID, Radio Alias, Talkgroup ID, Peer List, and Crypto lookup tables.
         * @param ridLookup Radio ID Lookup Table Instance
         * @param ridAliasLookup Radio Alias Lookup Table Instance
         * @param tidLookup Talkgroup Rules Lookup Table Instance
         * @param peerListLookup Peer List Lookup Table Instance
         * @param cryptoLookup Crypto Container Lookup Table Instance
         * @param adjSiteMapLookup Adjacent Site Map Lookup Table Instance
         */
        void setLookups(lookups::RadioIdLookup* ridLookup, lookups::RadioAliasLookup* ridAliasLookup, lookups::TalkgroupRulesLookup* tidLookup, 
            lookups::PeerListLookup* peerListLookup, CryptoContainer* cryptoLookup, lookups::AdjSiteMapLookup* adjSiteMapLookup);
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
#if defined(CATCH2_TEST_COMPILATION)
        friend class ::FNETestHooks;
#endif
        friend class MetadataNetwork;
        friend class callhandler::TagDMRData;
        friend class callhandler::packetdata::DMRPacketData;
        callhandler::TagDMRData* m_tagDMR;
        friend class callhandler::TagP25Data;
        friend class callhandler::packetdata::P25PacketData;
        callhandler::TagP25Data* m_tagP25;
        friend class callhandler::TagP25P2Data;
        callhandler::TagP25P2Data* m_tagP25P2;
        friend class callhandler::TagNXDNData;
        callhandler::TagNXDNData* m_tagNXDN;
        friend class callhandler::TagAnalogData;
        callhandler::TagAnalogData* m_tagAnalog;

        friend class P25OTARService;
        P25OTARService* m_p25OTARService;

        friend class ::fne_restapi::RESTAPI;
        HostFNE* m_host;

        std::string m_address;
        uint16_t m_port;

        std::string m_password;

        bool m_encryptedTrafficConn;

        bool m_isReplica;

        bool m_dmrEnabled;
        bool m_p25Enabled;
        bool m_p25P2Enabled;
        bool m_nxdnEnabled;
        bool m_analogEnabled;

        uint32_t m_parrotDelay;
        Timer m_parrotDelayTimer;
        bool m_parrotGrantDemand;
        bool m_parrotOnlyOriginating;
        uint32_t m_parrotOverrideSrcId;

        bool m_kmfServicesEnabled;
        bool m_kmfAllowRID0;
        bool m_kmfEncKeyRequest;
        uint8_t* m_kmfPresharedKey;

        lookups::RadioIdLookup* m_ridLookup;
        lookups::RadioAliasLookup* m_ridAliasLookup;
        lookups::TalkgroupRulesLookup* m_tidLookup;
        lookups::PeerListLookup* m_peerListLookup;
        lookups::AdjSiteMapLookup* m_adjSiteMapLookup;

        CryptoContainer* m_cryptoLookup;

        NET_CONN_STATUS m_status;

        typedef std::pair<const uint32_t, network::FNEPeerConnection*> PeerMapPair;
        concurrent::shared_unordered_map<uint32_t, FNEPeerConnection*> m_peers;
        concurrent::unordered_map<uint32_t, json::array> m_peerReplicaPeers;
        typedef std::pair<const uint32_t, std::shared_ptr<fne_lookups::AffiliationLookup>> PeerAffiliationMapPair;
        concurrent::unordered_map<uint32_t, std::shared_ptr<fne_lookups::AffiliationLookup>> m_peerAffiliations;
        mutable std::mutex m_peerAffiliationsMutex;
        concurrent::shared_unordered_map<uint32_t, std::vector<uint32_t>> m_ccPeerMap;
        static std::timed_mutex s_keyQueueMutex;
        std::unordered_map<uint32_t, uint16_t> m_peerReplicaKeyQueue;
        static std::timed_mutex s_llaKeyQueueMutex;
        std::unordered_map<uint32_t, uint32_t> m_peerReplicaLLAKeyQueue;

        fne_lookups::AffiliationLookup* m_globalAff;

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
        bool m_disallowRadioMonitor;
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

        bool m_enableMetrics;
        bool m_metricsLogRawData;

        bool m_enableInfluxDB;
        std::string m_influxServerAddress;
        uint16_t m_influxServerPort;
        std::string m_influxServerToken;
        std::string m_influxOrg;
        std::string m_influxBucket;
        influxdb::ServerInfo m_influxServer;

        bool m_enableSQLite;
        std::string m_sqliteDBFile;
        sqlite3* m_sqliteDB;
        uint32_t m_sqlitePruneAfterDays;
        uint32_t m_sqlitePruneIntervalMinutes;

        bool m_jitterBufferEnabled;
        uint16_t m_jitterMaxSize;
        uint32_t m_jitterMaxWait;

        ThreadPool m_threadPool;
        ThreadPool m_metadataUpdateThreadPool;

        /**
         * @brief Represents the state of a metadata update for a given peer ID.
         * @ingroup fne_network
         */
        struct MetadataUpdateState {
            /**
             * @brief Flag indicating whether a metadata update is currently in flight for this peer ID.
             */
            bool inFlight = false;
            /**
             * @brief Flag indicating whether a metadata update is pending for this peer ID.
             */
            bool pending = false;
        };
        std::mutex m_metadataUpdateMutex;
        std::unordered_map<uint32_t, MetadataUpdateState> m_metadataUpdateState;

        bool m_disablePacketData;
        bool m_dumpPacketData;
        bool m_verbosePacketData;

        uint32_t m_vtunQueueMaxFrames;
        uint32_t m_vtunQueueMaxBytes;

        uint32_t m_sndcpStartAddr;
        uint32_t m_sndcpEndAddr;

        bool m_logDenials;
        bool m_logUpstreamCallStartEnd;
        bool m_reportPeerPing;
        bool m_verbose;

        static std::array<std::mutex, PEER_STATE_LOCK_STRIPES> s_peerStateLocks;

        /**
         * @brief Gets the mutex for a specific peer ID.
         * @param peerId The ID of the peer.
         * @return A reference to the mutex associated with the peer ID.
         */
        static std::mutex& getPeerStateLock(uint32_t peerId) { return s_peerStateLocks[peerId % PEER_STATE_LOCK_STRIPES]; }

        /**
         * @brief Entry point to parrot handler thread.
         * @param arg Instance of the thread_t structure.
         * @returns void* (Ignore)
         */
        static void* threadParrotHandler(void* arg);

        /*
        ** Packet Processing
        */

        using PacketHandlerFunc = void (*)(TrafficNetwork* network, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId, uint64_t now);

        /**
         * @brief Implements the packet handler functions for the TrafficNetwork class.
         */
        class PacketHandler {
        public:
            /**
             * @brief Handles NET_FUNC::PROTOCOL packets.
             * @param network Instance of the TrafficNetwork class.
             * @param req Instance of the NetPacketRequest structure.
             * @param peerId Peer ID.
             * @param ssrc RTP synchronization source ID.
             * @param streamId Stream ID.
             * @param now Current time in milliseconds.
             */
            static void protocol(TrafficNetwork* network, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId, uint64_t now);

            /**
             * @brief Handles NET_FUNC::RPTL packets.
             * @param network Instance of the TrafficNetwork class.
             * @param req Instance of the NetPacketRequest structure.
             * @param peerId Peer ID.
             * @param ssrc RTP synchronization source ID.
             * @param streamId Stream ID.
             * @param now Current time in milliseconds.
             */
            static void repeaterLogin(TrafficNetwork* network, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId, uint64_t now);
            /**
             * @brief Handles NET_FUNC::RPTK packets.
             * @param network Instance of the TrafficNetwork class.
             * @param req Instance of the NetPacketRequest structure.
             * @param peerId Peer ID.
             * @param ssrc RTP synchronization source ID.
             * @param streamId Stream ID.
             * @param now Current time in milliseconds.
             */
            static void repeaterAuth(TrafficNetwork* network, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId, uint64_t now);
            /**
             * @brief Handles NET_FUNC::RPTC packets.
             * @param network Instance of the TrafficNetwork class.
             * @param req Instance of the NetPacketRequest structure.
             * @param peerId Peer ID.
             * @param ssrc RTP synchronization source ID.
             * @param streamId Stream ID.
             * @param now Current time in milliseconds.
             */
            static void repeaterConfig(TrafficNetwork* network, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId, uint64_t now);
            /**
             * @brief Handles NET_FUNC::RPT_DISC packets.
             * @param network Instance of the TrafficNetwork class.
             * @param req Instance of the NetPacketRequest structure.
             * @param peerId Peer ID.
             * @param ssrc RTP synchronization source ID.
             * @param streamId Stream ID.
             * @param now Current time in milliseconds.
             */
            static void repeaterDisconnect(TrafficNetwork* network, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId, uint64_t now);

            /**
             * @brief Handles NET_FUNC::PING packets.
             * @param network Instance of the TrafficNetwork class.
             * @param req Instance of the NetPacketRequest structure.
             * @param peerId Peer ID.
             * @param ssrc RTP synchronization source ID.
             * @param streamId Stream ID.
             * @param now Current time in milliseconds.
             */
            static void ping(TrafficNetwork* network, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId, uint64_t now);

            /**
             * @brief Handles NET_FUNC::GRANT_REQ packets.
             * @param network Instance of the TrafficNetwork class.
             * @param req Instance of the NetPacketRequest structure.
             * @param peerId Peer ID.
             * @param ssrc RTP synchronization source ID.
             * @param streamId Stream ID.
             * @param now Current time in milliseconds.
             */
            static void grantRequest(TrafficNetwork* network, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId, uint64_t now);

            /**
             * @brief Handles NET_FUNC::INCALL_CTRL packets.
             * @param network Instance of the TrafficNetwork class.
             * @param req Instance of the NetPacketRequest structure.
             * @param peerId Peer ID.
             * @param ssrc RTP synchronization source ID.
             * @param streamId Stream ID.
             * @param now Current time in milliseconds.
             */
            static void inCallControl(TrafficNetwork* network, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId, uint64_t now);

            /**
             * @brief Handles NET_FUNC::KEY_REQ packets.
             * @param network Instance of the TrafficNetwork class.
             * @param req Instance of the NetPacketRequest structure.
             * @param peerId Peer ID.
             * @param ssrc RTP synchronization source ID.
             * @param streamId Stream ID.
             * @param now Current time in milliseconds.
             */
            static void keyRequest(TrafficNetwork* network, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId, uint64_t now);
            /**
             * @brief Handles NET_FUNC::KEY_LLA_REQ packets.
             * @param network Instance of the TrafficNetwork class.
             * @param req Instance of the NetPacketRequest structure.
             * @param peerId Peer ID.
             * @param ssrc RTP synchronization source ID.
             * @param streamId Stream ID.
             * @param now Current time in milliseconds.
             */
            static void llaKeyRequest(TrafficNetwork* network, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId, uint64_t now);
        };

        /**
         * @brief Entry point to process a given network packet.
         * @param req Instance of the NetPacketRequest structure.
         */
        static void taskNetworkRx(NetPacketRequest* req);

        /*
        ** General Helper Functions
        */

        /**
         * @brief Checks if the passed length is valid for a repeater authentication packet.
         * @param length Length of the packet.
         * @returns bool True, if the length is valid for a repeater authentication packet, otherwise false.
         */
        static constexpr bool validRepeaterAuthLength(int length) { return length == REPEATER_PCKT_HDR_LEN + REPEATER_AUTH_HASH_LEN; }

        /**
         * @brief Checks if the passed length is valid for a repeater configuration packet.
         * @param length Length of the packet.
         * @returns bool True, if the length is valid for a repeater configuration packet, otherwise false.
         */
        static constexpr bool validRepeaterConfigLength(int length) { return length > REPEATER_PCKT_HDR_LEN && length <= (int)(DATA_PACKET_LENGTH); }

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
         * @brief Applies jitter buffer configuration to a peer connection.
         * @param peerId Peer ID.
         * @param connection Instance of the FNEPeerConnection class.
         */
        void applyJitterBufferConfig(uint32_t peerId, FNEPeerConnection* connection);

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
         * @brief Helper to get the peer affiliations entry for a peer.
         * @param peerId Peer ID.
         * @returns std::shared_ptr<fne_lookups::AffiliationLookup> Shared affiliations lookup instance.
         */
        std::shared_ptr<fne_lookups::AffiliationLookup> getPeerAffiliations(uint32_t peerId) const;
        /**
         * @brief Helper to create a snapshot of all peer affiliation entries.
         * @returns std::vector<PeerAffiliationMapPair> Snapshot of peer affiliation entries.
         */
        std::vector<PeerAffiliationMapPair> peerAffiliationsSnapshot() const;
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
         * @brief Helper to send the list of radio aliases to the specified peer.
         * @note This doesn't have a data layout document because it is *only* sent as a packet buffered message.
         * @param peerId Peer ID.
         * @param streamId Stream ID for this message.
         */
        void writeRadioAliasList(uint32_t peerId, uint32_t streamId);
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

        /**
         * @brief Helper to process a FNE KMM LLA response.
         * @param srcId Source Radio ID for the LLA response.
         * @param ki Key Item.
         * @param keyLength Length of key in bytes.
         */
        void processLLAResponse(uint32_t srcId, p25::kmm::KeyItem* ki, uint8_t keyLength);

        /*
        ** Metrics Helpers
        */

        /**
         * @brief Implements the packet handler functions for the TrafficNetwork class.
         */
        class MetricsLogging {
        public:
            /**
             * @brief Initializes metric sinks for the specified TrafficNetwork instance.
             * @param network Pointer to the TrafficNetwork instance.
             */
            static void initialize(TrafficNetwork* network);
            /**
             * @brief Finalizes metric sinks for the specified TrafficNetwork instance.
             * @param network Pointer to the TrafficNetwork instance.
             */
            static void finalize(TrafficNetwork* network);

            /**
             * @brief Increments the active call counter.
             * @param network Pointer to the TrafficNetwork instance.
             */
            static void incrementActiveCalls(TrafficNetwork* network);
            /**
             * @brief Decrements the active call counter with floor at zero.
             * @param network Pointer to the TrafficNetwork instance.
             */
            static void decrementActiveCalls(TrafficNetwork* network);
            /**
             * @brief Resets the active call counter to zero.
             */
            static void resetActiveCalls();
            /**
             * @brief Increments the total processed calls counter.
             * @param network Pointer to the TrafficNetwork instance.
             */
            static void incrementCallsProcessed(TrafficNetwork* network);
            /**
             * @brief Increments the total call collisions counter.
             * @param network Pointer to the TrafficNetwork instance.
             */
            static void incrementCallCollisions(TrafficNetwork* network);
            /**
             * @brief Increments the total call switches counter.
             * @param network Pointer to the TrafficNetwork instance.
             */
            static void incrementCallSwitches(TrafficNetwork* network);
            /**
             * @brief Resets the total processed calls counter to zero.
             * @param network Pointer to the TrafficNetwork instance.
             */
            static void resetCallsProcessed(TrafficNetwork* network);
            /**
             * @brief Resets the total call collisions counter to zero.
             * @param network Pointer to the TrafficNetwork instance.
             */
            static void resetCallCollisions(TrafficNetwork* network);
            /**
             * @brief Resets the total call switches counter to zero.
             * @param network Pointer to the TrafficNetwork instance.
             */
            static void resetCallSwitches(TrafficNetwork* network);
            /**
             * @brief Gets the active call counter.
             * @return Active call counter value.
             */
            static int32_t getTotalActiveCalls();
            /**
             * @brief Gets the total processed calls counter.
             * @return Total processed calls counter value.
             */
            static uint64_t getTotalCallsProcessed();
            /**
             * @brief Gets the total call collisions counter.
             * @return Total call collisions counter value.
             */
            static uint64_t getTotalCallCollisions();
            /**
             * @brief Gets the total call switches counter.
             * @return Total call switches counter value.
             */
            static uint64_t getTotalCallSwitches();

            /**
             * @brief Logs a activity transfer event.
             * @param network Pointer to the TrafficNetwork instance.
             * @param peerId Peer ID.
             * @param identity Peer identity string.
             * @param msg Activity payload.
             */
            static void logActivity(TrafficNetwork* network, uint32_t peerId, const std::string& identity, const std::string& msg);

            /**
             * @brief Logs a activity transfer event.
             * @param network Pointer to the TrafficNetwork instance.
             * @param peerId Peer ID.
             * @param identity Peer identity string.
             * @param msg Activity payload.
             */
            static void logDiag(TrafficNetwork* network, uint32_t peerId, const std::string& identity, const std::string& msg);
            /**
             * @brief Logs a call event.
             * @param network Pointer to the TrafficNetwork instance.
             * @param mode Call mode.
             * @param peerId Peer ID.
             * @param streamId Stream ID.
             * @param srcId Source ID.
             * @param dstId Destination ID.
             * @param durationMs Call duration in milliseconds.
             */
            static void logCallEvent(TrafficNetwork* network, const char* mode, uint32_t peerId, uint32_t streamId, uint32_t srcId, uint32_t dstId, uint64_t durationMs);
            /**
             * @brief Logs a call event with slot number.
             * @param network Pointer to the TrafficNetwork instance.
             * @param mode Call mode.
             * @param peerId Peer ID.
             * @param streamId Stream ID.
             * @param srcId Source ID.
             * @param dstId Destination ID.
             * @param durationMs Call duration in milliseconds.
             * @param slotNo Slot number.
             */
            static void logCallEvent(TrafficNetwork* network, const char* mode, uint32_t peerId, uint32_t streamId, uint32_t srcId, uint32_t dstId, uint64_t durationMs, uint8_t slotNo);
            /**
             * @brief Logs a call error event.
             * @param network Pointer to the TrafficNetwork instance.
             * @param peerId Peer ID.
             * @param streamId Stream ID.
             * @param srcId Source ID.
             * @param dstId Destination ID.
             * @param message Error message.
             */
            static void logCallErrorEvent(TrafficNetwork* network, uint32_t peerId, uint32_t streamId, uint32_t srcId, uint32_t dstId, const std::string& message);
            /**
             * @brief Logs a call error event with slot number.
             * @param network Pointer to the TrafficNetwork instance.
             * @param peerId Peer ID.
             * @param streamId Stream ID.
             * @param srcId Source ID.
             * @param dstId Destination ID.
             * @param message Error message.
             * @param slotNo Slot number.
             */
            static void logCallErrorEvent(TrafficNetwork* network, uint32_t peerId, uint32_t streamId, uint32_t srcId, uint32_t dstId, const std::string& message, uint8_t slotNo);
            /**
             * @brief Logs a call collision event with slot number.
             * @param network Pointer to the TrafficNetwork instance.
             * @param peerId Peer ID.
             * @param streamId Stream ID.
             * @param srcId Source ID.
             * @param dstId Destination ID.
             * @param slotNo Slot number.
             * @param rxPeerId Received peer ID.
             * @param rxStreamId Received stream ID.
             * @param rxSrcId Received source ID.
             * @param rxDstId Received destination ID.
             * @param rxSlot Received slot number.
             */
            static void logCallCollisionEvent(TrafficNetwork* network, uint32_t peerId, uint32_t streamId, uint32_t srcId, uint32_t dstId, uint8_t slotNo,
                uint32_t rxPeerId, uint32_t rxStreamId, uint32_t rxSrcId, uint32_t rxDstId, uint8_t rxSlot);
            /**
             * @brief Logs a P25 TSBK raw event.
             * @param network Pointer to the TrafficNetwork instance.
             * @param peerId Peer ID.
             * @param lco LCO tag value.
             * @param tsbk TSBK description.
             * @param raw Raw payload string.
             */
            static void logTSBKEvent(TrafficNetwork* network, uint32_t peerId, const std::string& lco, const std::string& tsbk, const std::string& raw);
            /**
             * @brief Logs a DMR CSBK raw event.
             * @param network Pointer to the TrafficNetwork instance.
             * @param peerId Peer ID.
             * @param lco LCO tag value.
             * @param csbk CSBK description.
             * @param raw Raw payload string.
             */
            static void logCSBKEvent(TrafficNetwork* network, uint32_t peerId, const std::string& lco, const std::string& csbk, const std::string& raw);

        private:
            static std::atomic<int32_t> s_totalActiveCalls;
            static std::atomic<uint64_t> s_totalCallsProcessed;
            static std::atomic<uint64_t> s_totalCallCollisions;
            static std::atomic<uint64_t> s_totalCallSwitches;

            /**
             * @brief Checks if the SQLite database for the specified TrafficNetwork instance is blank.
             * @param network Pointer to the TrafficNetwork instance.
             * @return True if the SQLite database is blank, false otherwise.
             */
            static bool isSQLiteBlank(TrafficNetwork* network);
            /**
             * @brief Initializes the SQLite database for the specified TrafficNetwork instance.
             * @param network Pointer to the TrafficNetwork instance.
             */
            static void initializeSQLite(TrafficNetwork* network);
            /**
             * @brief Ensures SQLite tables required for persisted metrics counters exist.
             * @param network Pointer to the TrafficNetwork instance.
             * @return True if the table exists (or was created), false on error.
             */
            static bool ensureSQLiteCounterTable(TrafficNetwork* network);
            /**
             * @brief Ensures all SQLite metrics tables and indexes exist.
             * @param network Pointer to the TrafficNetwork instance.
             * @return True if all schema objects exist (or were created), false on error.
             */
            static bool ensureSQLiteMetricTables(TrafficNetwork* network);
            /**
             * @brief Loads persisted metrics counters from SQLite.
             * @param network Pointer to the TrafficNetwork instance.
             */
            static void loadSQLiteCounters(TrafficNetwork* network);
            /**
             * @brief Persists a single metrics counter to SQLite.
             * @param network Pointer to the TrafficNetwork instance.
             * @param key Counter key.
             * @param value Counter value.
             */
            static void persistSQLiteCounter(TrafficNetwork* network, const char* key, uint64_t value);
        };
    };
} // namespace network

#endif // __TRAFFIC_NETWORK_H__
