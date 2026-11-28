// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - IPSC Gateway
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file IPSCClient.h
 * @ingroup ipsc
 * @file IPSCClient.cpp
 * @ingroup ipsc
 */
#if !defined(__IPSC_PROTOCOL_CLIENT_H__)
#define __IPSC_PROTOCOL_CLIENT_H__

#include "Defines.h"
#include "common/network/udp/Socket.h"
#include "protocol/IPSCDefines.h"

#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>

namespace ipsc
{
    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents an IPSC peer advertised by the master.
     * @ingroup ipsc_protocol
     */
    struct Peer {
        uint32_t id;                //!< Full IPSC peer ID
        sockaddr_storage address;   //!< Peer network address
        uint32_t addressLength;     //!< Length of the peer network address
        std::chrono::steady_clock::time_point lastSeen; //!< Time at which traffic was most recently received from the peer
        bool connected;             //!< Flag indicating successful peer registration
    };

    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents a decoded IPSC voice packet.
     * @ingroup ipsc_protocol
     */
    struct VoicePacket {
        uint32_t peerId;            //!< Full ID of the originating IPSC peer
        uint32_t sourceId;          //!< DMR source radio ID
        uint32_t destinationId;     //!< DMR destination radio or talkgroup ID
        uint8_t slotNo;             //!< DMR slot number
        uint8_t sequence;           //!< IPSC call sequence number
        bool group;                 //!< Flag indicating group-call operation
        bool last;                  //!< Flag indicating the final packet of a call
        bool hasColorCode;          //!< Flag indicating that IPSC supplied a DMR color code
        uint8_t colorCode;          //!< DMR color code obtained from IPSC burst metadata
        const uint8_t* burst;       //!< Decoded IPSC burst payload, valid only for the duration of the callback
        uint32_t burstLength;       //!< Length of the decoded IPSC burst payload
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements IPSC link establishment and voice transport.
     * @ingroup ipsc_protocol
     */
    class IPSCClient {
    public:
        typedef std::function<void(const VoicePacket&)> VoiceCallback;

        /**
         * @brief Initializes a new instance of the IPSCClient class.
         * @param masterAddress Hostname or address of the IPSC master.
         * @param masterPort UDP port of the IPSC master.
         * @param localPort Local UDP port used by the IPSC peer.
         * @param peerId Full IPSC peer ID assigned to this application.
         * @param authenticationKey IPSC HMAC-SHA1 authentication key as hexadecimal text.
         * @param slot1 Flag indicating slot 1 support.
         * @param slot2 Flag indicating slot 2 support.
         * @param debug Flag indicating IPSC packet debugging is enabled.
         */
        IPSCClient(const std::string& masterAddress, uint16_t masterPort, uint16_t localPort, uint32_t peerId, const std::string& authenticationKey, bool slot1,
                   bool slot2, bool debug);
        /** 
         * @brief Finalizes an instance of the IPSCClient class. 
         */
        ~IPSCClient();

        /**
         * @brief Opens the IPSC UDP transport and begins registration.
         * @returns bool True if the transport was opened, otherwise false.
         */
        bool open();
        /** 
         * @brief Deregisters and closes the IPSC UDP transport. 
         */
        void close();

        /** 
         * @brief Processes received packets and session timers. 
         */
        void clock();
        /** 
         * @brief Checks if the client is registered with the IPSC network.
         * @returns bool True if the client is registered with the IPSC network. 
         */
        bool connected() const { return m_state == State::IPSC_CONNECTED; }

        /** 
         * @brief Sets the callback invoked for decoded IPSC voice packets. 
         */
        void setVoiceCallback(VoiceCallback callback) { m_voiceCallback = callback; }

        /**
         * @brief Writes an IPSC voice packet to the master and registered peers.
         * @returns bool True if the packet was written to at least one destination.
         */
        bool writeVoice(uint32_t sourceId, uint32_t destinationId, bool group, uint8_t slotNo, uint8_t sequence, bool last, const uint8_t* burst,
                        uint32_t burstLength);

    private:
        /**
         * @brief IPSC master registration state. 
         */
        enum State
        {
            IPSC_DISCONNECTED,
            IPSC_REGISTERING,
            IPSC_REQUESTING_MAP,
            IPSC_CONNECTED
        };

        std::string m_masterAddress;
        uint16_t m_masterPort;
        uint16_t m_localPort;

        uint32_t m_peerId;
        uint8_t m_authenticationKey[defines::AUTHENTICATION_KEY_LENGTH];
        bool m_authenticated;
        bool m_slot1;
        bool m_slot2;
        bool m_debug;

        network::udp::Socket m_socket;
        sockaddr_storage m_master;
        uint32_t m_masterLength;

        State m_state;
        uint8_t m_protocolVersion;
        std::unordered_map<uint32_t, Peer> m_peers;

        std::chrono::steady_clock::time_point m_lastMasterTraffic;
        std::chrono::steady_clock::time_point m_nextAction;
        std::chrono::steady_clock::time_point m_nextKeepAlive;

        VoiceCallback m_voiceCallback;

        uint16_t m_rtpSequence;
        uint32_t m_rtpTimestamp;
        uint32_t m_rtpSSRC;

        /** 
         * @brief Creates a registration or keepalive status message. 
         * @param type The type of status message to create.
         * @param packet Buffer that receives the status message.
         */
        void createStatusMessage(defines::MessageType type, uint8_t* packet) const;
        /** 
         * @brief Appends the configured IPSC HMAC authentication tag. 
         * @param packet Packet to authenticate.
         * @param length Length of the packet.
         * @param output Buffer that receives the authenticated packet.
         * @returns uint32_t Length of the authenticated packet.
         */
        uint32_t authenticate(const uint8_t* packet, uint32_t length, uint8_t* output) const;
        /** 
         * @brief Verifies and removes an IPSC HMAC authentication tag. 
         * @param packet Packet to verify.
         * @param[in,out] length Length of the packet, updated to exclude the authentication tag.
         * @returns bool True if the authentication tag was valid, false otherwise.
         */
        bool verify(const uint8_t* packet, uint32_t& length) const;

        /** 
         * @brief Sends an IPSC datagram to a resolved network address. 
         * @param packet Packet to send.
         * @param length Length of the packet.
         * @param address The resolved network address to send the packet to.
         * @param addressLength The length of the resolved network address.
         * @returns bool True if the packet was successfully sent, false otherwise.
         */
        bool send(const uint8_t* packet, uint32_t length, const sockaddr_storage& address, uint32_t addressLength);
        /** 
         * @brief Sends an IPSC datagram to the configured master. 
         * @param packet Packet to send.
         * @param length Length of the packet.
         * @returns bool True if the packet was successfully sent, false otherwise.
         */
        bool sendMaster(const uint8_t* packet, uint32_t length);

        /** 
         * @brief Sends a master registration request. 
         */
        void sendRegistration();
        /** 
         * @brief Sends a master peer-map request. 
         */
        void sendMapRequest();
        /** 
         * @brief Sends master and peer keepalive requests. 
         */
        void sendKeepAlives();

        /** 
         * @brief Dispatches a verified IPSC datagram. 
         * @param packet The verified IPSC datagram to process.
         * @param length Length of the datagram.
         * @param source The source network address of the datagram.
         * @param sourceLength The length of the source network address.
         */
        void process(const uint8_t* packet, uint32_t length, const sockaddr_storage& source, uint32_t sourceLength);
        /** 
         * @brief Processes a master peer-map broadcast. 
         * @param packet The master peer-map broadcast packet to process.
         * @param length Length of the packet.
         */
        void processMap(const uint8_t* packet, uint32_t length);
        /** 
         * @brief Decodes an IPSC voice message and RTP payload. 
         * @param packet The IPSC voice message packet to decode.
         * @param length Length of the packet.
         */
        void processVoice(const uint8_t* packet, uint32_t length);
        /** 
         * @brief Processes and logs an IPSC call-control response. 
         * @param packet The IPSC call-control response packet to process.
         * @param length Length of the packet.
         */
        void processCallResponse(const uint8_t* packet, uint32_t length) const;
        /** 
         * @brief Processes and logs an IPSC RCM call status message. 
         * @param packet The IPSC RCM call status message packet to process.
         * @param length Length of the packet.
         */
        void processCallMonitor(const uint8_t* packet, uint32_t length) const;

        /** 
         * @brief Sends a response to an inbound peer session message. 
         * @param type The type of the message to send.
         * @param peerId The ID of the peer to respond to.
         * @param source The source network address of the inbound message.
         * @param sourceLength The length of the source network address.
         */
        void reply(defines::MessageType type, uint32_t peerId, const sockaddr_storage& source, uint32_t sourceLength);
    };
} // namespace ipsc

#endif // __IPSC_PROTOCOL_CLIENT_H__
