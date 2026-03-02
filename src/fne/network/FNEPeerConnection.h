// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file FNEPeerConnection.h
 * @ingroup fne_network
 * @file FNEPeerConnection.cpp
 * @ingroup fne_network
 */
#if !defined(__FNE_PEER_CONNECTION_H__)
#define __FNE_PEER_CONNECTION_H__

#include "fne/Defines.h"
#include "common/network/BaseNetwork.h"
#include "common/network/AdaptiveJitterBuffer.h"

#include <string>
#include <map>
#include <shared_mutex>

namespace network
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents an peer connection to the FNE.
     * @ingroup fne_network
     */
    class HOST_SW_API FNEPeerConnection : public RTPStreamMultiplex {
    public:
        auto operator=(FNEPeerConnection&) -> FNEPeerConnection& = delete;
        auto operator=(FNEPeerConnection&&) -> FNEPeerConnection& = delete;
        FNEPeerConnection(FNEPeerConnection&) = delete;

        /**
         * @brief Initializes a new instance of the FNEPeerConnection class.
         */
        FNEPeerConnection() : 
            RTPStreamMultiplex(),
            m_id(0U),
            m_masterId(0U),
            m_ccPeerId(0U),
            m_socketStorage(),
            m_sockStorageLen(0U),
            m_address(),
            m_port(),
            m_salt(0U),
            m_connected(false),
            m_connectionState(NET_STAT_INVALID),
            m_pingsReceived(0U),
            m_lastPing(0U),
            m_missedMetadataUpdates(0U),
            m_hasCallPriority(false),
            m_isNeighborFNEPeer(false),
            m_isReplica(false),
            m_isConventionalPeer(false),
            m_isSysView(false),
            m_config(),
            m_peerLockMtx(),
            m_jitterBuffers(),
            m_jitterMutex(),
            m_jitterBufferEnabled(false),
            m_jitterMaxSize(4U),
            m_jitterMaxWait(40000U)
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
            RTPStreamMultiplex(),
            m_id(id),
            m_masterId(0U),
            m_ccPeerId(0U),
            m_socketStorage(socketStorage),
            m_sockStorageLen(sockStorageLen),
            m_address(udp::Socket::address(socketStorage)),
            m_port(udp::Socket::port(socketStorage)),
            m_salt(0U),
            m_connected(false),
            m_connectionState(NET_STAT_INVALID),
            m_pingsReceived(0U),
            m_lastPing(0U),
            m_missedMetadataUpdates(0U),
            m_hasCallPriority(false),
            m_isNeighborFNEPeer(false),
            m_isReplica(false),
            m_isConventionalPeer(false),
            m_isSysView(false),
            m_config(),
            m_peerLockMtx(),
            m_jitterBuffers(),
            m_jitterMutex(),
            m_jitterBufferEnabled(false),
            m_jitterMaxSize(4U),
            m_jitterMaxWait(40000U)
        {
            assert(id > 0U);
            assert(sockStorageLen > 0U);
            assert(!m_address.empty());
            assert(m_port > 0U);
        }

        /**
         * @brief Returns the identity with qualifier symbols.
         * @return std::string Identity with qualifier.
         */
        std::string identWithQualifier() const 
        {
            if (isSysView())
                return "@" + identity();
            if (isReplica())
                return "%" + identity();
            if (isNeighborFNEPeer())
                return "+" + identity();

            return " " + m_identity;
        }

        /**
         * @brief Lock the peer.
         */
        inline void lock() const { m_peerLockMtx.lock(); }
        /**
         * @brief Unlock the peer.
         */
        inline void unlock() const { m_peerLockMtx.unlock(); }

        /**
         * @brief Gets or creates a jitter buffer for the specified stream.
         * @param streamId Stream ID.
         * @returns AdaptiveJitterBuffer* Jitter buffer instance.
         */
        AdaptiveJitterBuffer* getOrCreateJitterBuffer(uint64_t streamId);

        /**
         * @brief Cleans up jitter buffer for the specified stream.
         * @param streamId Stream ID.
         */
        void cleanupJitterBuffer(uint64_t streamId);

        /**
         * @brief Checks for timed-out buffered frames across all streams.
         */
        void checkJitterTimeouts();

        /**
         * @brief Gets jitter buffer enabled state.
         * @returns bool True if jitter buffer is enabled.
         */
        bool jitterBufferEnabled() const { return m_jitterBufferEnabled; }

        /**
         * @brief Sets jitter buffer parameters.
         * @param enabled Enable/disable jitter buffer.
         * @param maxSize Maximum buffer size in frames.
         * @param maxWait Maximum wait time in microseconds.
         */
        void setJitterBufferParams(bool enabled, uint16_t maxSize = 4U, uint32_t maxWait = 40000U)
        {
            m_jitterBufferEnabled = enabled;
            m_jitterMaxSize = maxSize;
            m_jitterMaxWait = maxWait;
        }

    public:
        /**
         * @brief Peer ID.
         */
        DECLARE_PROPERTY_PLAIN(uint32_t, id);
        /**
         * @brief Master Peer ID.
         */
        DECLARE_PROPERTY_PLAIN(uint32_t, masterId);
        /**
         * @brief Peer Identity.
         */
        DECLARE_PROPERTY_PLAIN(std::string, identity);

        /**
         * @brief Control Channel Peer ID.
         */
        DECLARE_PROPERTY_PLAIN(uint32_t, ccPeerId);

        /**
         * @brief Unix socket storage containing the connected address.
         */
        DECLARE_PROPERTY_PLAIN(sockaddr_storage, socketStorage);
        /**
         * @brief Length of the sockaddr_storage structure.
         */
        DECLARE_PROPERTY_PLAIN(uint32_t, sockStorageLen);

        /**
         * @brief         */
        DECLARE_PROPERTY_PLAIN(std::string, address);
        /**
         * @brief Port number peer connected with.
         */
        DECLARE_PROPERTY_PLAIN(uint16_t, port);

        /**
         * @brief Salt value used for peer authentication.
         */
        DECLARE_PROPERTY_PLAIN(uint32_t, salt);

        /**
         * @brief Flag indicating whether or not the peer is connected.
         */
        DECLARE_PROPERTY_PLAIN(bool, connected);
        /**
         * @brief Connection state.
         */
        DECLARE_PROPERTY_PLAIN(NET_CONN_STATUS, connectionState);

        /**
         * @brief Number of pings received.
         */
        DECLARE_PROPERTY_PLAIN(uint32_t, pingsReceived);
        /**
         * @brief Last ping received.
         */
        DECLARE_PROPERTY_PLAIN(uint64_t, lastPing);

        /**
         * @brief Number of missed network metadata updates.
         */
        DECLARE_PROPERTY_PLAIN(uint32_t, missedMetadataUpdates);

        /**
         * @brief Flag indicating this connection has call priority.
         */
        DECLARE_PROPERTY_PLAIN(bool, hasCallPriority);

        /**
         * @brief Flag indicating this connection is from an downstream neighbor FNE peer.
         */
        DECLARE_PROPERTY_PLAIN(bool, isNeighborFNEPeer);
        /**
         * @brief Flag indicating this connection is from a neighbor FNE peer that is replica enabled.
         */
        DECLARE_PROPERTY_PLAIN(bool, isReplica);

        /**
         * @brief Flag indicating this connection is from an conventional peer.
         */
        DECLARE_PROPERTY_PLAIN(bool, isConventionalPeer);
        /**
         * @brief Flag indicating this connection is from an SysView peer.
         */
        DECLARE_PROPERTY_PLAIN(bool, isSysView);

        /**
         * @brief JSON objecting containing peer configuration information.
         */
        DECLARE_PROPERTY_PLAIN(json::object, config);

    private:
        mutable std::mutex m_peerLockMtx;

        std::map<uint64_t, AdaptiveJitterBuffer*> m_jitterBuffers;
        mutable std::mutex m_jitterMutex;

        bool m_jitterBufferEnabled;
        uint16_t m_jitterMaxSize;
        uint32_t m_jitterMaxWait;
    };
} // namespace network

#endif // __FNE_PEER_CONNECTION_H__
