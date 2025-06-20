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
 * @file TagAnalogData.h
 * @ingroup fne_callhandler
 * @file TagAnalogData.cpp
 * @ingroup fne_callhandler
 */
#if !defined(__CALLHANDLER__TAG_ANALOG_DATA_H__)
#define __CALLHANDLER__TAG_ANALOG_DATA_H__

#include "fne/Defines.h"
#include "common/concurrent/deque.h"
#include "common/concurrent/unordered_map.h"
#include "common/dmr/DMRDefines.h"
#include "common/dmr/data/NetData.h"
#include "common/dmr/lc/CSBK.h"
#include "common/Clock.h"
#include "network/FNENetwork.h"
#include "network/callhandler/packetdata/DMRPacketData.h"

namespace network
{
    namespace callhandler
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements the analog call handler and data FNE networking logic.
         * @ingroup fne_callhandler
         */
        class HOST_SW_API TagAnalogData {
        public:
            /**
             * @brief Initializes a new instance of the TagAnalogData class.
             * @param network Instance of the FNENetwork class.
             * @param debug Flag indicating whether network debug is enabled.
             */
            TagAnalogData(FNENetwork* network, bool debug);
            /**
             * @brief Finalizes a instance of the TagAnalogData class.
             */
            ~TagAnalogData();

            /**
             * @brief Process a data frame from the network.
             * @param data Network data buffer.
             * @param len Length of data.
             * @param peerId Peer ID.
             * @param ssrc RTP Synchronization Source ID.
             * @param pktSeq RTP packet sequence.
             * @param streamId Stream ID.
             * @param external Flag indicating traffic is from an external peer.
             * @returns bool True, if frame is processed, otherwise false.
             */
            bool processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint32_t ssrc, uint16_t pktSeq, uint32_t streamId, bool external = false);

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

                /**
                 * @brief RTP Packet Sequence.
                 */
                uint16_t pktSeq;
                /**
                 * @brief Call Stream ID.
                 */
                uint32_t streamId;
                /**
                 * @brief Peer ID.
                 */
                uint32_t peerId;

                /**
                 * @brief Source ID.
                 */
                uint32_t srcId;
                /**
                 * @brief Destination ID.
                 */
                uint32_t dstId;
            };
            concurrent::deque<ParrotFrame> m_parrotFrames;
            bool m_parrotFramesReady;

            /**
             * @brief Represents the receive status of a call.
             */
            class RxStatus {
            public:
                system_clock::hrc::hrc_t callStartTime;
                system_clock::hrc::hrc_t lastPacket;
                /**
                 * @brief Source ID.
                 */
                uint32_t srcId;
                /**
                 * @brief Destination ID.
                 */
                uint32_t dstId;
                /**
                 * @brief Call Stream ID.
                 */
                uint32_t streamId;
                /**
                 * @brief Peer ID.
                 */
                uint32_t peerId;
                /**
                 * @brief Flag indicating this call is active with traffic currently in progress.
                 */
                bool activeCall;

                /**
                 * @brief Helper to reset call status.
                 */
                void reset() 
                {
                    srcId = 0U;
                    dstId = 0U;
                    streamId = 0U;
                    peerId = 0U;
                    activeCall = false;
                }
            };
            typedef std::pair<const uint32_t, RxStatus> StatusMapPair;
            concurrent::unordered_map<uint32_t, RxStatus> m_status;

            bool m_debug;

            /**
             * @brief Helper to route rewrite the network data buffer.
             * @param buffer Frame buffer.
             * @param peerId Peer ID.
             * @param dstId Destination ID.
             * @param outbound Flag indicating whether or not this is outbound traffic.
             */
            void routeRewrite(uint8_t* buffer, uint32_t peerId, uint32_t dstId, bool outbound = true);
            /**
             * @brief Helper to route rewrite destination ID and slot.
             * @param peerId Peer ID.
             * @param dstId Destination ID.
             * @param outbound Flag indicating whether or not this is outbound traffic.
             * @returns bool True, if rewritten successfully, otherwise false.
             */
            bool peerRewrite(uint32_t peerId, uint32_t& dstId, bool outbound = true);

            /**
             * @brief Helper to determine if the peer is permitted for traffic.
             * @param peerId Peer ID.
             * @param data Instance of data::NetData Analog data container class.
             * @param streamId Stream ID.
             * @param external Flag indicating this traffic came from an external peer.
             * @returns bool True, if valid, otherwise false.
             */
            bool isPeerPermitted(uint32_t peerId, analog::data::NetData& data, uint32_t streamId, bool external = false);
            /**
             * @brief Helper to validate the DMR call stream.
             * @param peerId Peer ID.
             * @param data Instance of data::NetData Analog data container class.
             * @param streamId Stream ID.
             * @returns bool True, if valid, otherwise false.
             */
            bool validate(uint32_t peerId, analog::data::NetData& data, uint32_t streamId);
        };
    } // namespace callhandler
} // namespace network

#endif // __CALLHANDLER__TAG_ANALOG_DATA_H__
