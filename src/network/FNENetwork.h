/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2023 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#if !defined(__FNE_NETWORK_H__)
#define __FNE_NETWORK_H__

#include "Defines.h"
#include "network/BaseNetwork.h"
#include "network/Network.h"
#include "network/json/json.h"
#include "lookups/RadioIdLookup.h"
#include "lookups/RoutingRulesLookup.h"

#include <string>
#include <cstdint>
#include <unordered_map>

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------

class HOST_SW_API HostFNE;
namespace network { namespace fne { class HOST_SW_API TagDMRData; } }
namespace network { namespace fne { class HOST_SW_API TagP25Data; } }
namespace network { namespace fne { class HOST_SW_API TagNXDNData; } }

namespace network
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //     Represents an peer connection to the FNE.
    // ---------------------------------------------------------------------------

    class HOST_SW_API FNEPeerConnection {
    public:
        /// <summary>Initializes a new insatnce of the FNEPeerConnection class.</summary>
        FNEPeerConnection() :
            m_id(0U),
            m_socketStorage(),
            m_sockStorageLen(0U),
            m_address(),
            m_port(),
            m_salt(0U),
            m_connected(false),
            m_connectionState(NET_STAT_INVALID),
            m_pingsReceived(0U),
            m_config()
        {
            /* stub */
        }
        /// <summary>Initializes a new insatnce of the FNEPeerConnection class.</summary>
        /// <param name="id">Unique ID of this modem on the network.</param>
        /// <param name="socketStorage"></param>
        /// <param name="sockStorageLen"></param>
        FNEPeerConnection(uint32_t id, sockaddr_storage& socketStorage, uint32_t sockStorageLen) :
            m_id(id),
            m_socketStorage(socketStorage),
            m_sockStorageLen(sockStorageLen),
            m_address(UDPSocket::address(socketStorage)),
            m_port(UDPSocket::port(socketStorage)),
            m_salt(0U),
            m_connected(false),
            m_connectionState(NET_STAT_INVALID),
            m_pingsReceived(0U),
            m_config()
        {
            assert(id > 0U);
            assert(sockStorageLen > 0U);
            assert(!m_address.empty());
            assert(m_port > 0U);
        }

        /// <summary>Equals operator. Copies this FNEPeerConnection to another FNEPeerConnection.</summary>
        virtual FNEPeerConnection& operator=(const FNEPeerConnection& data)
        {
            if (this != &data) {
                m_id = data.m_id;
                m_socketStorage = data.m_socketStorage;
                m_sockStorageLen = data.m_sockStorageLen;
                m_address = data.m_address;
                m_port = data.m_port;
                m_salt = data.m_salt;
                m_connected = data.m_connected;
                m_connectionState = data.m_connectionState;
                m_pingsReceived = data.m_pingsReceived;
                m_config = data.m_config;
            }

            return *this;
        }

    public:
        /// <summary>Peer ID.</summary>
        __PROPERTY_PLAIN(uint32_t, id, id);

        /// <summary></<summary>
        __PROPERTY_PLAIN(sockaddr_storage, socketStorage, socketStorage);
        /// <summary></<summary>
        __PROPERTY_PLAIN(uint32_t, sockStorageLen, sockStorageLen);

        /// <summary>IP address peer connected with.</<summary>
        __PROPERTY_PLAIN(std::string, address, address);
        /// <summary>Port number peer connected with.</summary>
        __PROPERTY_PLAIN(uint16_t, port, port);

        /// <summary>Salt value used for peer authentication.</summary>
        __PROPERTY_PLAIN(uint32_t, salt, salt);

        /// <summary>Flag indicating whether or not the peer is connected.</summary>
        __PROPERTY_PLAIN(bool, connected, connected);
        /// <summary>Connection state.</summary>
        __PROPERTY_PLAIN(NET_CONN_STATUS, connectionState, connectionState);

        /// <summary>Number of pings received.</summary>
        __PROPERTY_PLAIN(uint32_t, pingsReceived, pingsReceived);
        /// <summary>Last ping received.</summary>
        __PROPERTY_PLAIN(uint64_t, lastPing, lastPing);

        /// <summary>JSON objecting containing peer configuration information.</summary>
        __PROPERTY_PLAIN(json::object, config, config);
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements the core FNE networking logic.
    // ---------------------------------------------------------------------------

    class HOST_SW_API FNENetwork : public BaseNetwork {
    public:
        /// <summary>Initializes a new instance of the FNENetwork class.</summary>
        FNENetwork(HostFNE* host, const std::string& address, uint16_t port, const std::string& password,
            bool debug, bool dmr, bool p25, bool nxdn, bool allowActivityTransfer, bool allowDiagnosticTransfer, 
            uint32_t pingTime, uint32_t updateLookupTime);
        /// <summary>Finalizes a instance of the FNENetwork class.</summary>
        ~FNENetwork();

        /// <summary>Sets the instances of the Radio ID and Routing Rules lookup tables.</summary>
        void setLookups(lookups::RadioIdLookup* ridLookup, lookups::RoutingRulesLookup* routingLookup);
        /// <summary>Gets the current status of the network.</summary>
        uint8_t getStatus();

        /// <summary>Updates the timer by the passed number of milliseconds.</summary>
        void clock(uint32_t ms);

        /// <summary>Opens connection to the network.</summary>
        bool open();

        /// <summary>Sets flag enabling network communication.</summary>
        void enable(bool enabled);

        /// <summary>Closes connection to the network.</summary>
        void close();

    private:
        friend class TagDMRData;
        fne::TagDMRData* m_tagDMR;
        friend class TagP25Data;
        fne::TagP25Data* m_tagP25;
        friend class TagNXDNData;
        fne::TagNXDNData* m_tagNXDN;
        
        HostFNE* m_host;

        std::string m_address;
        uint16_t m_port;

        std::string m_password;

        bool m_enabled;

        bool m_dmrEnabled;
        bool m_p25Enabled;
        bool m_nxdnEnabled;

        lookups::RadioIdLookup* m_ridLookup;
        lookups::RoutingRulesLookup* m_routingLookup;

        typedef std::pair<const unsigned int, network::FNEPeerConnection> PeerMapPair;
        std::unordered_map<uint32_t, FNEPeerConnection> m_peers;

        Timer m_maintainenceTimer;
        Timer m_updateLookupTimer;

        /// <summary>Helper to send the list of whitelisted RIDs to the specified peer.</summary>
        void writeWhitelistRIDs(uint32_t peerId);
        /// <summary>Helper to send the list of whitelisted RIDs to connected peers.</summary>
        void writeWhitelistRIDs();
        /// <summary>Helper to send the list of blacklisted RIDs to the specified peer.</summary>
        void writeBlacklistRIDs(uint32_t peerId);
        /// <summary>Helper to send the list of blacklisted RIDs to connected peers.</summary>
        void writeBlacklistRIDs();

        /// <summary>Helper to send the list of active TGIDs to the specified peer.</summary>
        void writeTGIDs(uint32_t peerId);
        /// <summary>Helper to send the list of active TGIDs to connected peers.</summary>
        void writeTGIDs();
        /// <summary>Helper to send the list of deactivated TGIDs to the specified peer.</summary>
        void writeDeactiveTGIDs(uint32_t peerId);
        /// <summary>Helper to send the list of deactivated TGIDs to connected peers.</summary>
        void writeDeactiveTGIDs();

        /// <summary>Helper to send a raw message to the specified peer.</summary>
        bool writePeer(uint32_t peerId, const uint8_t* data, uint32_t length);
        /// <summary>Helper to send a tagged message to the specified peer.</summary>
        bool writePeerTagged(uint32_t peerId, const char* tag, const uint8_t* data, uint32_t length);
        /// <summary>Helper to send a ACK response to the specified peer.</summary>
        bool writePeerACK(uint32_t peerId);
        /// <summary>Helper to send a NAK response to the specified peer.</summary>
        bool writePeerNAK(uint32_t peerId, const char* tag);
        /// <summary>Helper to send a NAK response to the specified peer.</summary>
        bool writePeerNAK(uint32_t peerId, const char* tag, sockaddr_storage& addr, uint32_t addrLen);

        /// <summary>Helper to send a raw message to the connected peers.</summary>
        void writePeers(const uint8_t* data, uint32_t length);
        /// <summary>Helper to send a tagged message to the connected peers.</summary>
        void writePeersTagged(const char* tag, const uint8_t* data, uint32_t length);
    };
} // namespace network

#endif // __FNE_NETWORK_H__
