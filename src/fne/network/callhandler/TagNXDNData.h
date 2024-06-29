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
 * @file TagNXDNData.h
 * @ingroup fne_callhandler
 * @file TagNXDNData.cpp
 * @ingroup fne_callhandler
 */
#if !defined(__CALLHANDLER__TAG_NXDN_DATA_H__)
#define __CALLHANDLER__TAG_NXDN_DATA_H__

#include "fne/Defines.h"
#include "common/Clock.h"
#include "common/nxdn/NXDNDefines.h"
#include "common/nxdn/lc/RTCH.h"
#include "common/nxdn/lc/RCCH.h"
#include "network/FNENetwork.h"

#include <deque>

namespace network
{
    namespace callhandler
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements the NXDN call handler and data FNE networking logic.
         * @ingroup fne_callhandler
         */
        class HOST_SW_API TagNXDNData {
        public:
            /**
             * @brief Initializes a new instance of the TagNXDNData class.
             * @param network Instance of the FNENetwork class.
             * @param debug Flag indicating whether network debug is enabled.
             */
            TagNXDNData(FNENetwork* network, bool debug);
            /**
             * @brief Finalizes a instance of the TagNXDNData class.
             */
            ~TagNXDNData();

            /**
             * @brief Process a data frame from the network.
             * @param data Network data buffer.
             * @param len Length of data.
             * @param peerId Peer ID.
             * @param pktSeq RTP packet sequence.
             * @param streamId Stream ID.
             * @param external Flag indicating traffic is from an external peer.
             * @returns bool True, if frame is processed, otherwise false.
             */
            bool processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint16_t pktSeq, uint32_t streamId, bool external = false);
            /**
             * @brief Process a grant request frame from the network.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             * @param unitToUnit Flag indicating whether or not this grant is for unit-to-unit traffic.
             * @param peerId Peer ID.
             * @param pktSeq RTP packet sequence.
             * @param streamId Stream ID.
             * @returns bool True, if the grant was processed, otherwise false.
             */
            bool processGrantReq(uint32_t srcId, uint32_t dstId, bool unitToUnit, uint32_t peerId, uint16_t pktSeq, uint32_t streamId);

            /**
             * @brief Helper to playback a parrot frame to the network.
             */
            void playbackParrot();
            /**
             * @brief Helper to determine if there are stored parrot frames.
             * @returns True, if there are queued parrot frames to playback, otherwise false.
             */
            bool hasParrotFrames() const { return m_parrotFramesReady && !m_parrotFrames.empty(); }

        private:
            FNENetwork* m_network;

            /**
             * @brief Represents a stored parrot frame.
             */
            class ParrotFrame {
            public:
                uint8_t* buffer;
                uint32_t bufferLen;

                uint16_t pktSeq;
                uint32_t streamId;
                uint32_t peerId;

                uint32_t srcId;
                uint32_t dstId;
            };
            std::deque<ParrotFrame> m_parrotFrames;
            bool m_parrotFramesReady;

            /**
             * @brief Represents the receive status of a call.
             */
            class RxStatus {
            public:
                system_clock::hrc::hrc_t callStartTime;
                uint32_t srcId;
                uint32_t dstId;
                uint32_t streamId;
                uint32_t peerId;
            };
            typedef std::pair<const uint32_t, RxStatus> StatusMapPair;
            std::unordered_map<uint32_t, RxStatus> m_status;

            bool m_debug;

            /**
             * @brief Helper to route rewrite the network data buffer.
             * @param buffer Frame buffer.
             * @param peerId Peer ID.
             * @param messageType Message Type.
             * @param dstId Destination ID.
             * @param outbound Flag indicating whether or not this is outbound traffic.
             */
            void routeRewrite(uint8_t* buffer, uint32_t peerId, uint8_t messageType, uint32_t dstId, bool outbound = true);
            /**
             * @brief Helper to route rewrite destination ID.
             * @param peerId Peer ID.
             * @param dstId Destination ID.
             * @param outbound Flag indicating whether or not this is outbound traffic.
             * @returns bool True, if rewritten successfully, otherwise false.
             */
            bool peerRewrite(uint32_t peerId, uint32_t& dstId, bool outbound = true);

            /**
             * @brief Helper to determine if the peer is permitted for traffic.
             * @param peerId Peer ID.
             * @param lc Instance of nxdn::lc::RTCH.
             * @param messageType Message Type.
             * @param streamId Stream ID.
             * @param external Flag indicating this traffic came from an external peer.
             * @returns bool True, if permitted, otherwise false.
             */
            bool isPeerPermitted(uint32_t peerId, nxdn::lc::RTCH& lc, uint8_t messageType, uint32_t streamId, bool external = false);
            /**
             * @brief Helper to validate the NXDN call stream.
             * @param peerId Peer ID.
             * @param control Instance of nxdn::lc::RTCH.
             * @param messageType Message Type.
             * @param streamId Stream ID.
             * @returns bool True, if valid, otherwise false.
             */
            bool validate(uint32_t peerId, nxdn::lc::RTCH& control, uint8_t messageType, uint32_t streamId);

            /**
             * @brief Helper to write a grant packet.
             * @param peerId Peer ID.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             * @param serviceOptions Service Options.
             * @param grp Flag indicating the destination ID is a talkgroup.
             * @returns True, if granted, otherwise false.
             */
            bool write_Message_Grant(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t serviceOptions, bool grp);
            /**
             * @brief Helper to write a deny packet.
             * @param peerId Peer ID.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             * @param reason Denial Reason.
             * @param service Service being denied.
             */
            void write_Message_Deny(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t reason, uint8_t service);

            /**
             * @brief Helper to write a network RCCH.
             * @param peerId Peer ID.
             * @param rcch Instance of nxdn::lc::RCCH.
             */
            void write_Message(uint32_t peerId, nxdn::lc::RCCH* rcch);
        };
    } // namespace callhandler
} // namespace network

#endif // __CALLHANDLER__TAG_NXDN_DATA_H__
