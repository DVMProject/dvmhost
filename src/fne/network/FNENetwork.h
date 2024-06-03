// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Converged FNE Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Converged FNE Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023-2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__FNE_NETWORK_H__)
#define __FNE_NETWORK_H__

#include "fne/Defines.h"
#include "common/network/BaseNetwork.h"
#include "common/network/json/json.h"
#include "common/lookups/AffiliationLookup.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "fne/network/influxdb/InfluxDB.h"
#include "host/network/Network.h"

#include <string>
#include <cstdint>
#include <unordered_map>
#include <mutex>

#include <pthread.h>

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------

class HOST_SW_API HostFNE;
class HOST_SW_API RESTAPI;
namespace network { namespace fne { class HOST_SW_API TagDMRData; } }
namespace network { namespace fne { class HOST_SW_API TagP25Data; } }
namespace network { namespace fne { class HOST_SW_API TagNXDNData; } }

namespace network
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    enum DVM_STATE {
        STATE_IDLE = 0U,
        // DMR
        STATE_DMR = 1U,
        // Project 25
        STATE_P25 = 2U,
        // NXDN
        STATE_NXDN = 3U,
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
    //     Represents an peer connection to the FNE.
    // ---------------------------------------------------------------------------

    class HOST_SW_API FNEPeerConnection {
    public:
        auto operator=(FNEPeerConnection&) -> FNEPeerConnection& = delete;
        auto operator=(FNEPeerConnection&&) -> FNEPeerConnection& = delete;
        FNEPeerConnection(FNEPeerConnection&) = delete;

        /// <summary>Initializes a new instance of the FNEPeerConnection class.</summary>
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
            m_pktLastSeq(0U),
            m_pktNextSeq(1U)
        {
            /* stub */
        }
        /// <summary>Initializes a new instance of the FNEPeerConnection class.</summary>
        /// <param name="id">Unique ID of this modem on the network.</param>
        /// <param name="socketStorage"></param>
        /// <param name="sockStorageLen"></param>
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
            m_pktLastSeq(0U),
            m_pktNextSeq(1U)
        {
            assert(id > 0U);
            assert(sockStorageLen > 0U);
            assert(!m_address.empty());
            assert(m_port > 0U);
        }

    public:
        /// <summary>Peer ID.</summary>
        __PROPERTY_PLAIN(uint32_t, id);
        /// <summary>Peer Identity.</summary>
        __PROPERTY_PLAIN(std::string, identity);

        /// <summary>Control Channel Peer ID.</summary>
        __PROPERTY_PLAIN(uint32_t, ccPeerId);

        /// <summary>Current Stream ID.</summary>
        __PROPERTY_PLAIN(uint32_t, currStreamId);

        /// <summary>Unix socket storage containing the connected address.</summary>
        __PROPERTY_PLAIN(sockaddr_storage, socketStorage);
        /// <summary>Length of the sockaddr_storage structure.</summary>
        __PROPERTY_PLAIN(uint32_t, sockStorageLen);

        /// <summary>IP address peer connected with.</<summary>
        __PROPERTY_PLAIN(std::string, address);
        /// <summary>Port number peer connected with.</summary>
        __PROPERTY_PLAIN(uint16_t, port);

        /// <summary>Salt value used for peer authentication.</summary>
        __PROPERTY_PLAIN(uint32_t, salt);

        /// <summary>Flag indicating whether or not the peer is connected.</summary>
        __PROPERTY_PLAIN(bool, connected);
        /// <summary>Connection state.</summary>
        __PROPERTY_PLAIN(NET_CONN_STATUS, connectionState);

        /// <summary>Number of pings received.</summary>
        __PROPERTY_PLAIN(uint32_t, pingsReceived);
        /// <summary>Last ping received.</summary>
        __PROPERTY_PLAIN(uint64_t, lastPing);

        /// <summary>Last ACL update sent.</summary>
        __PROPERTY_PLAIN(uint64_t, lastACLUpdate);

        /// <summary>Flag indicating this connection is from an external peer.</summary>
        __PROPERTY_PLAIN(bool, isExternalPeer);
        /// <summary>Flag indicating this connection is from an conventional peer.</summary>
        /// <remarks>This flag is specifically used to determine whether affiliation based checking is performed.</summary>
        __PROPERTY_PLAIN(bool, isConventionalPeer);

        /// <summary>JSON objecting containing peer configuration information.</summary>
        __PROPERTY_PLAIN(json::object, config);

        /// <summary>Last received RTP sequence.</summary>
        __PROPERTY_PLAIN(uint16_t, pktLastSeq);
        /// <summary>Calculated next RTP sequence.</summary>
        __PROPERTY_PLAIN(uint16_t, pktNextSeq);
    };

    // ---------------------------------------------------------------------------
    //  Structure Declaration
    //      Represents the data required for a peer ACL update request thread.
    // ---------------------------------------------------------------------------

    struct ACLUpdateRequest {
        FNENetwork* network;
        uint32_t peerId;

        pthread_t thread;
    };

    // ---------------------------------------------------------------------------
    //  Structure Declaration
    //      Represents the data required for a network packet handler thread.
    // ---------------------------------------------------------------------------

    struct NetPacketRequest {
        FNENetwork* network;
        uint32_t peerId;

        sockaddr_storage address;
        uint32_t addrLen;
        frame::RTPHeader rtpHeader;
        frame::RTPFNEHeader fneHeader;
        int length = 0U;
        uint8_t *buffer;

        pthread_t thread;
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements the core FNE networking logic.
    // ---------------------------------------------------------------------------

    class HOST_SW_API FNENetwork : public BaseNetwork {
    public:
        /// <summary>Initializes a new instance of the FNENetwork class.</summary>
        FNENetwork(HostFNE* host, const std::string& address, uint16_t port, uint32_t peerId, const std::string& password,
            bool debug, bool verbose, bool reportPeerPing, bool dmr, bool p25, bool nxdn, uint32_t parrotDelay, bool parrotGrantDemand,
            bool allowActivityTransfer, bool allowDiagnosticTransfer, uint32_t pingTime, uint32_t updateLookupTime);
        /// <summary>Finalizes a instance of the FNENetwork class.</summary>
        ~FNENetwork() override;

        /// <summary>Helper to set configuration options.</summary>
        void setOptions(yaml::Node& conf, bool printOptions);

        /// <summary>Gets the current status of the network.</summary>
        NET_CONN_STATUS getStatus() { return m_status; }

        /// <summary>Gets the instance of the DMR traffic handler.</summary>
        fne::TagDMRData* dmrTrafficHandler() const { return m_tagDMR; }
        /// <summary>Gets the instance of the P25 traffic handler.</summary>
        fne::TagP25Data* p25TrafficHandler() const { return m_tagP25; }
        /// <summary>Gets the instance of the NXDN traffic handler.</summary>
        fne::TagNXDNData* nxdnTrafficHandler() const { return m_tagNXDN; }

        /// <summary>Sets the instances of the Radio ID and Talkgroup Rules lookup tables.</summary>
        void setLookups(lookups::RadioIdLookup* ridLookup, lookups::TalkgroupRulesLookup* tidLookup);
        /// <summary>Sets endpoint preshared encryption key.</summary>
        void setPresharedKey(const uint8_t* presharedKey);

        /// <summary>Process a data frames from the network.</summary>
        void processNetwork();

        /// <summary>Updates the timer by the passed number of milliseconds.</summary>
        void clock(uint32_t ms) override;

        /// <summary>Opens connection to the network.</summary>
        bool open() override;

        /// <summary>Closes connection to the network.</summary>
        void close() override;

    private:
        friend class DiagNetwork;
        friend class fne::TagDMRData;
        fne::TagDMRData* m_tagDMR;
        friend class fne::TagP25Data;
        fne::TagP25Data* m_tagP25;
        friend class fne::TagNXDNData;
        fne::TagNXDNData* m_tagNXDN;
        
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

        NET_CONN_STATUS m_status;

        static std::mutex m_peerMutex;
        typedef std::pair<const uint32_t, network::FNEPeerConnection*> PeerMapPair;
        std::unordered_map<uint32_t, FNEPeerConnection*> m_peers;
        typedef std::pair<const uint32_t, lookups::AffiliationLookup*> PeerAffiliationMapPair;
        std::unordered_map<uint32_t, lookups::AffiliationLookup*> m_peerAffiliations;

        Timer m_maintainenceTimer;

        uint32_t m_updateLookupTime;
        uint32_t m_softConnLimit;

        bool m_callInProgress;

        bool m_disallowAdjStsBcast;
        bool m_disallowExtAdjStsBcast;
        bool m_allowConvSiteAffOverride;
        bool m_restrictGrantToAffOnly;

        bool m_enableInfluxDB;
        std::string m_influxServerAddress;
        uint16_t m_influxServerPort;
        std::string m_influxServerToken;
        std::string m_influxOrg;
        std::string m_influxBucket;
        bool m_influxLogRawData;
        influxdb::ServerInfo m_influxServer;

        bool m_reportPeerPing;
        bool m_verbose;

        /// <summary>Entry point to process a given network packet.</summary>
        static void* threadedNetworkRx(void* arg);

        /// <summary>Helper to create a peer on the peers affiliations list.</summary>
        void createPeerAffiliations(uint32_t peerId, std::string peerName);
        /// <summary>Helper to erase the peer from the peers affiliations list.</summary>
        bool erasePeerAffiliations(uint32_t peerId);
        /// <summary>Helper to erase the peer from the peers list.</summary>
        bool erasePeer(uint32_t peerId);

        /// <summary>Helper to resolve the peer ID to its identity string.</summary>
        std::string resolvePeerIdentity(uint32_t peerId);

        /// <summary>Helper to complete setting up a repeater login request.</summary>
        void setupRepeaterLogin(uint32_t peerId, FNEPeerConnection* connection);

        /// <summary>Helper to send the ACL lists to the specified peer in a separate thread.</summary>
        void peerACLUpdate(uint32_t peerId);
        /// <summary>Entry point to send the ACL lists to the specified peer in a separate thread.</summary>
        static void* threadedACLUpdate(void* arg);

        /// <summary>Helper to send the list of whitelisted RIDs to the specified peer.</summary>
        void writeWhitelistRIDs(uint32_t peerId);
        /// <summary>Helper to send the list of blacklisted RIDs to the specified peer.</summary>
        void writeBlacklistRIDs(uint32_t peerId);
        /// <summary>Helper to send the list of active TGIDs to the specified peer.</summary>
        void writeTGIDs(uint32_t peerId);
        /// <summary>Helper to send the list of deactivated TGIDs to the specified peer.</summary>
        void writeDeactiveTGIDs(uint32_t peerId);

        /// <summary>Helper to send a data message to the specified peer.</summary>
        bool writePeer(uint32_t peerId, FrameQueue::OpcodePair opcode, const uint8_t* data, uint32_t length, 
            uint16_t pktSeq, uint32_t streamId, bool queueOnly = false, bool directWrite = false) const;
        /// <summary>Helper to send a data message to the specified peer.</summary>
        bool writePeer(uint32_t peerId, FrameQueue::OpcodePair opcode, const uint8_t* data, uint32_t length, 
            uint32_t streamId, bool queueOnly = false, bool incPktSeq = false, bool directWrite = false) const;

        /// <summary>Helper to send a command message to the specified peer.</summary>
        bool writePeerCommand(uint32_t peerId, FrameQueue::OpcodePair opcode, const uint8_t* data = nullptr, uint32_t length = 0U, 
            bool incPktSeq = false) const;

        /// <summary>Helper to send a ACK response to the specified peer.</summary>
        bool writePeerACK(uint32_t peerId, const uint8_t* data = nullptr, uint32_t length = 0U);

        /// <summary>Helper to log a warning specifying which NAK reason is being sent a peer.</summary>
        void logPeerNAKReason(uint32_t peerId, const char* tag, NET_CONN_NAK_REASON reason);
        /// <summary>Helper to send a NAK response to the specified peer.</summary>
        bool writePeerNAK(uint32_t peerId, const char* tag, NET_CONN_NAK_REASON reason = NET_CONN_NAK_GENERAL_FAILURE);
        /// <summary>Helper to send a NAK response to the specified peer.</summary>
        bool writePeerNAK(uint32_t peerId, const char* tag, NET_CONN_NAK_REASON reason, sockaddr_storage& addr, uint32_t addrLen);
    };
} // namespace network

#endif // __FNE_NETWORK_H__
