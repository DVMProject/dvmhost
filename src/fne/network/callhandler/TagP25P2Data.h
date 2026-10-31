// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 */
/**
 * @file TagP25P2Data.h
 * @ingroup fne_callhandler
 * @file TagP25P2Data.cpp
 * @ingroup fne_callhandler
 */
#if !defined(__CALLHANDLER__TAG_P25_P2_DATA_H__)
#define __CALLHANDLER__TAG_P25_P2_DATA_H__

#include "fne/Defines.h"
#include "common/Clock.h"
#include "common/concurrent/deque.h"
#include "common/concurrent/unordered_map.h"
#include "common/p25/P25Defines.h"
#include "common/p25/lc/LC.h"
#include "network/TrafficNetwork.h"

#include <cstdint>

namespace network 
{
    namespace callhandler 
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements the P25 Phase 2 data call handler.
         * @ingroup fne_callhandler
         */
        class HOST_SW_API TagP25P2Data {
        public:
            /**
             * @brief Initializes a new instance of the TagP25P2Data class.
             * @param network Instance of the TrafficNetwork class.
             * @param debug Flag indicating whether network debug is enabled.
             */
            TagP25P2Data(TrafficNetwork* network, bool debug);
            /**
             * @brief Finalizes an instance of the TagP25P2Data class.
             */
            ~TagP25P2Data() = default;

            /**
             * @brief Process a data frame from the network.
             * @param data Network data buffer.
             * @param len Length of data.
             * @param peerId Peer ID.
             * @param ssrc RTP Synchronization Source ID.
             * @param pktSeq RTP packet sequence.
             * @param streamId Stream ID.
             * @param fromUpstream Flag indicating traffic is from a upstream master.
             * @returns bool True, if frame is processed, otherwise false.
             */
            bool processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint32_t ssrc, uint16_t pktSeq, uint32_t streamId, bool fromUpstream = false);

            /**
             * @brief Helper to trigger a call takeover from a In-Call control event.
             * @param dstId Destination ID for the call takeover.
             */
            void triggerCallTakeover(uint32_t dstId);

            /**
             * @brief Helper to playback a parrot frame to the network.
             */
            void playbackParrot();
            /**
             * @brief Helper to determine if there are stored parrot frames.
             * @returns True, if there are queued parrot frames to playback, otherwise false.
             */
            bool hasParrotFrames() const { return m_parrotFramesReady && !m_parrotFrames.empty(); }

            /**
             * @brief Helper to determine if the parrot is playing back frames.
             * @returns True, if parrot playback was started, otherwise false.
             */
            bool isParrotPlayback() const { return m_parrotPlayback; }
            /**
             * @brief Helper to clear the parrot playback flag.
             */
            void clearParrotPlayback()
            {
                m_parrotPlayback = false;
                m_lastParrotPeerId = 0U;
                m_lastParrotSrcId = 0U;
                m_lastParrotDstId = 0U;
            }

            /**
             * @brief Returns the last processed peer ID for a parrot frame.
             * @return uint32_t Peer ID.
             */
            uint32_t lastParrotPeerId() const { return m_lastParrotPeerId; }
            /**
             * @brief Returns the last processed source ID for a parrot frame.
             * @return uint32_t Source ID.
             */
            uint32_t lastParrotSrcId() const { return m_lastParrotSrcId; }
            /**
             * @brief Returns the last processed destination ID for a parrot frame.
             * @return uint32_t Destination ID.
             */
            uint32_t lastParrotDstId() const { return m_lastParrotDstId; }

        private:
            TrafficNetwork* m_network;

            /**
             * @brief Represents a stored parrot frame.
             */
            class ParrotFrame {
            public:
                uint8_t* buffer;
                uint32_t bufferLen;

                /**
                 * @brief P25 Phase 2 slot number.
                 */
                uint8_t slotNo;

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
            bool m_parrotPlayback;
            uint32_t m_lastParrotPeerId;
            uint32_t m_lastParrotSrcId;
            uint32_t m_lastParrotDstId;

            /**
             * @brief Represents the receive status of a call.
             */
            struct RxStatus {
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
                 * @brief Synchronization Source.
                 */
                uint32_t ssrc;
                /**
                 * @brief Destination Peer ID for a registered private-call target.
                 */
                uint32_t dstPeerId;
                /**
                 * @brief Slot number.
                 */
                uint8_t slotNo;
                /**
                 * @brief Indicates if the call is unit-to-unit.
                 */
                bool unitToUnit;
                /**
                 * @brief Flag indicating this call is active with traffic currently in progress.
                 */
                bool activeCall;
                /**
                 * @brief Indicates if the call is a takeover.
                 */
                bool callTakeover;

                /**
                 * @brief Helper to reset call status.
                 */
                void reset() 
                {
                    srcId = 0U;
                    dstId = 0U;
                    streamId = 0U;
                    peerId = 0U;
                    ssrc = 0U;
                    dstPeerId = 0U;
                    slotNo = 0U;
                    unitToUnit = false;
                    activeCall = false;
                    callTakeover = false;
                }
            };
            typedef std::pair<const uint32_t, RxStatus> StatusMapPair;
            concurrent::unordered_map<uint32_t, RxStatus> m_status;
            concurrent::unordered_map<uint32_t, RxStatus> m_statusPVCall;
            concurrent::unordered_map<uint32_t, std::vector<uint32_t>> m_rejectedCallStreams;

            bool m_debug;

            /**
             * @brief Helper to route rewrite the network data buffer.
             * @param peerId Peer ID.
             * @param dstId Destination ID.
             * @param slotNo Slot number.
             * @param outbound Flag indicating whether or not this is outbound traffic.
             */
            void routeRewrite(uint8_t* buffer, uint32_t peerId, uint32_t dstId, uint8_t slotNo, bool outbound = true);
            /**
             * @brief Helper to route rewrite destination ID and slot.
             * @param peerId Peer ID.
             * @param dstId Destination ID.
             * @param slotNo slot number.
             * @param outbound Flag indicating whether or not this is outbound traffic.
             * @returns bool True, if rewritten successfully, otherwise false.
             */
            bool peerRewrite(uint32_t peerId, uint32_t& dstId, uint8_t& slotNo, bool outbound = true);

            /**
             * @brief Helper to determine if the peer is permitted for traffic.
             * @param peerId Peer ID.
             * @param dstId Destination ID.
             * @param slotNo Slot number.
             * @param unitToUnit Indicates if the call is unit-to-unit.
             * @param streamId Stream ID.
             * @param fromUpstream Flag indicating traffic is from a upstream master.
             * @returns bool True, if valid, otherwise false.
             */
            bool isPeerPermitted(uint32_t peerId, uint32_t dstId, uint8_t slotNo, bool unitToUnit, uint32_t streamId, bool fromUpstream = false);
            /**
             * @brief Helper to validate the P25 Phase 2 call stream.
             * @param peerId Peer ID.
             * @param srcId Source ID.
             * @param dstId Destination ID.
             * @param slotNo Slot number.
             * @param unitToUnit Indicates if the call is unit-to-unit.
             * @param streamId Stream ID.
             * @returns bool True, if valid, otherwise false.
             */
            bool validate(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t slotNo, bool unitToUnit, uint32_t streamId);
        };
    } // namespace callhandler
} // namespace network

#endif // __CALLHANDLER__TAG_P25_P2_DATA_H__
