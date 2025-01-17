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
 * @file TagP25Data.h
 * @ingroup fne_callhandler
 * @file TagP25Data.cpp
 * @ingroup fne_callhandler
 */
#if !defined(__CALLHANDLER__TAG_P25_DATA_H__)
#define __CALLHANDLER__TAG_P25_DATA_H__

#include "fne/Defines.h"
#include "common/Clock.h"
#include "common/p25/P25Defines.h"
#include "common/p25/data/DataHeader.h"
#include "common/p25/data/LowSpeedData.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/p25/dfsi/LC.h"
#include "common/p25/lc/LC.h"
#include "common/p25/lc/TSBK.h"
#include "common/p25/lc/TDULC.h"
#include "network/FNENetwork.h"
#include "network/callhandler/packetdata/P25PacketData.h"

#include <deque>

namespace network
{
    namespace callhandler
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements the P25 call handler and data FNE networking logic.
         * @ingroup fne_callhandler
         */
        class HOST_SW_API TagP25Data {
        public:
            /**
             * @brief Initializes a new instance of the TagP25Data class.
             * @param network Instance of the FNENetwork class.
             * @param debug Flag indicating whether network debug is enabled.
             */
            TagP25Data(FNENetwork* network, bool debug);
            /**
             * @brief Finalizes a instance of the TagP25Data class.
             */
            ~TagP25Data();

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

            /**
             * @brief Helper to write a call alert packet.
             * @param peerId Peer ID.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             */
            void write_TSDU_Call_Alrt(uint32_t peerId, uint32_t srcId, uint32_t dstId);
            /**
             * @brief Helper to write a radio monitor packet.
             * @param peerId Peer ID.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             * @param txMult Tx Multiplier.
             */
            void write_TSDU_Radio_Mon(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t txMult);
            /**
             * @brief Helper to write a extended function packet.
             * @param peerId Peer ID.
             * @param func Extended Function Operation.
             * @param arg Function Argument.
             * @param dstId Destination Radio ID.
             */
            void write_TSDU_Ext_Func(uint32_t peerId, uint32_t func, uint32_t arg, uint32_t dstId);
            /**
             * @brief Helper to write a group affiliation query packet.
             * @param peerId Peer ID.
             * @param dstId Destination Radio ID.
             */
            void write_TSDU_Grp_Aff_Q(uint32_t peerId, uint32_t dstId);
            /**
             * @brief Helper to write a unit registration command packet.
             * @param peerId Peer ID.
             * @param dstId Destination Radio ID.
             */
            void write_TSDU_U_Reg_Cmd(uint32_t peerId, uint32_t dstId);

            /**
             * @brief Gets instance of the P25PacketData class.
             * @returns P25PacketData* Instance of the P25PacketData class.
             */
            packetdata::P25PacketData* packetData() { return m_packetData; }

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
            std::deque<ParrotFrame> m_parrotFrames;
            bool m_parrotFramesReady;
            bool m_parrotFirstFrame;

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
            std::unordered_map<uint32_t, RxStatus> m_status;

            friend class packetdata::P25PacketData;
            packetdata::P25PacketData* m_packetData;

            bool m_debug;

            /**
             * @brief Helper to route rewrite the network data buffer.
             * @param buffer Frame buffer.
             * @param peerId Peer ID.
             * @param duid DUID.
             * @param dstId Destination ID.
             * @param outbound Flag indicating whether or not this is outbound traffic.
             */
            void routeRewrite(uint8_t* buffer, uint32_t peerId, uint8_t duid, uint32_t dstId, bool outbound = true);
            /**
             * @brief Helper to route rewrite destination ID.
             * @param peerId Peer ID.
             * @param dstId Destination ID.
             * @param outbound Flag indicating whether or not this is outbound traffic.
             * @returns bool True, if rewritten successfully, otherwise false.
             */
            bool peerRewrite(uint32_t peerId, uint32_t& dstId, bool outbound = true);

            /**
             * @brief Helper to process TSDUs being passed from a peer.
             * @param buffer Frame buffer.
             * @param peerId Peer ID.
             * @param duid DUID.
             * @returns bool True, if allowed to pass, otherwise false.
             */
            bool processTSDUFrom(uint8_t* buffer, uint32_t peerId, uint8_t duid);
            /**
             * @brief Helper to process TSDUs being passed to a peer.
             * @param buffer Frame buffer.
             * @param peerId Peer ID.
             * @param duid DUID.
             * @returns bool True, if allowed to pass, otherwise false.
             */
            bool processTSDUTo(uint8_t* buffer, uint32_t peerId, uint8_t duid);
            /**
             * @brief Helper to process TSDUs being passed to an external peer.
             * @param buffer Frame buffer.
             * @param srcPeerId Source Peer ID.
             * @param dstPeerID Destination Peer ID.
             * @param duid DUID.
             * @returns bool True, if allowed to pass, otherwise false.
             */
            bool processTSDUToExternal(uint8_t* buffer, uint32_t srcPeerId, uint32_t dstPeerId, uint8_t duid);

            /**
             * @brief Helper to determine if the peer is permitted for traffic.
             * @param peerId Peer ID.
             * @param control Instance of p25::lc::LC.
             * @param duid DUID.
             * @param streamId Stream ID.
             * @param external Flag indicating this traffic came from an external peer.
             * @returns bool True, if permitted, otherwise false.
             */
            bool isPeerPermitted(uint32_t peerId, p25::lc::LC& control, P25DEF::DUID::E duid, uint32_t streamId, bool external = false);
            /**
             * @brief Helper to validate the P25 call stream.
             * @param peerId Peer ID.
             * @param control Instance of p25::lc::LC.
             * @param duid DUID.
             * @param[in] tsbk Instance of p25::lc::TSBK.
             * @param streamId Stream ID.
             * @returns bool True, if valid, otherwise false.
             */
            bool validate(uint32_t peerId, p25::lc::LC& control, P25DEF::DUID::E duid, const p25::lc::TSBK* tsbk, uint32_t streamId);

            /**
             * @brief Helper to write a grant packet.
             * @param peerId Peer ID.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             * @param serviceOptions Service Options.
             * @param grp Flag indicating the destination ID is a talkgroup.
             * @returns True, if granted, otherwise false.
             */
            bool write_TSDU_Grant(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t serviceOptions, bool grp);
            /**
             * @brief Helper to write a deny packet.
             * @param peerId Peer ID.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             * @param reason Denial Reason.
             * @param service Service being denied.
             * @param group Flag indicating the destination ID is a talkgroup.
             * @param aiv 
             */
            void write_TSDU_Deny(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t reason, uint8_t service, bool group = false, bool aiv = false);
            /**
             * @brief Helper to write a queue packet.
             * @param peerId Peer ID.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             * @param reason Queue Reason.
             * @param service Service being denied.
             * @param grp Flag indicating the destination ID is a talkgroup.
             * @param aiv 
             */
            void write_TSDU_Queue(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t reason, uint8_t service, bool group = false, bool aiv = false);

            /**
             * @brief Helper to write a network TSDU.
             * @param peerId Peer ID.
             * @param tsbk Instance of p25::lc::TSBK.
             */
            void write_TSDU(uint32_t peerId, p25::lc::TSBK* tsbk);
        };
    } // namespace callhandler
} // namespace network

#endif // __CALLHANDLER__TAG_P25_DATA_H__
