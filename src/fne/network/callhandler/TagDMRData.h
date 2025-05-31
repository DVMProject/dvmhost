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
#if !defined(__CALLHANDLER__TAG_DMR_DATA_H__)
#define __CALLHANDLER__TAG_DMR_DATA_H__

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
         * @brief Implements the DMR call handler and data FNE networking logic.
         * @ingroup fne_callhandler
         */
        class HOST_SW_API TagDMRData {
        public:
            /**
             * @brief Initializes a new instance of the TagDMRData class.
             * @param network Instance of the FNENetwork class.
             * @param debug Flag indicating whether network debug is enabled.
             */
            TagDMRData(FNENetwork* network, bool debug);
            /**
             * @brief Finalizes a instance of the TagDMRData class.
             */
            ~TagDMRData();

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
             * @brief Process a grant request frame from the network.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             * @param slot DMR slot number.
             * @param unitToUnit Flag indicating whether or not this grant is for unit-to-unit traffic.
             * @param peerId Peer ID.
             * @param pktSeq RTP packet sequence.
             * @param streamId Stream ID.
             * @returns bool True, if the grant was processed, otherwise false.
             */
            bool processGrantReq(uint32_t srcId, uint32_t dstId, uint8_t slot, bool unitToUnit, uint32_t peerId, uint16_t pktSeq, uint32_t streamId);

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
             * @brief Helper to write a extended function packet on the RF interface.
             * @param peerId Peer ID.
             * @param slot DMR slot number.
             * @param func Extended Function Operation.
             * @param arg Function Argument.
             * @param dstId Destination Radio ID.
             */
            void write_Ext_Func(uint32_t peerId, uint8_t slot, uint32_t func, uint32_t arg, uint32_t dstId);
            /**
             * @brief Helper to write a call alert packet on the RF interface.
             * @param peerId Peer ID.
             * @param slot DMR slot number.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             */
            void write_Call_Alrt(uint32_t peerId, uint8_t slot, uint32_t srcId, uint32_t dstId);

            /**
             * @brief Gets instance of the DMRPacketData class.
             * @returns DMRPacketData* Instance of the DMRPacketData class.
             */
            packetdata::DMRPacketData* packetData() { return m_packetData; }

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
                 * @brief DMR slot number.
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
                 * @brief DMR slot number.
                 */
                uint8_t slotNo;
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
                    slotNo = 0U;
                    streamId = 0U;
                    peerId = 0U;
                    activeCall = false;
                }
            };
            typedef std::pair<const uint32_t, RxStatus> StatusMapPair;
            concurrent::unordered_map<uint32_t, RxStatus> m_status;

            friend class packetdata::DMRPacketData;
            packetdata::DMRPacketData* m_packetData;

            bool m_debug;

            /**
             * @brief Helper to route rewrite the network data buffer.
             * @param buffer Frame buffer.
             * @param peerId Peer ID.
             * @param dmrData Instance of data::NetData DMR data container class.
             * @param dataType DMR Data Type.
             * @param dstId Destination ID.
             * @param slotNo DMR slot number.
             * @param outbound Flag indicating whether or not this is outbound traffic.
             */
            void routeRewrite(uint8_t* buffer, uint32_t peerId, dmr::data::NetData& dmrData, DMRDEF::DataType::E dataType, uint32_t dstId, uint32_t slotNo, bool outbound = true);
            /**
             * @brief Helper to route rewrite destination ID and slot.
             * @param peerId Peer ID.
             * @param dstId Destination ID.
             * @param slotNo DMR slot number.
             * @param outbound Flag indicating whether or not this is outbound traffic.
             * @returns bool True, if rewritten successfully, otherwise false.
             */
            bool peerRewrite(uint32_t peerId, uint32_t& dstId, uint32_t& slotNo, bool outbound = true);

            /**
             * @brief Helper to process CSBKs being passed from a peer.
             * @param buffer Frame buffer.
             * @param peerId Peer ID.
             * @param dmrData Instance of data::NetData DMR data container class.
             * @returns bool True, if allowed to pass, otherwise false.
             */
            bool processCSBK(uint8_t* buffer, uint32_t peerId, dmr::data::NetData& dmrData);

            /**
             * @brief Helper to determine if the peer is permitted for traffic.
             * @param peerId Peer ID.
             * @param dmrData Instance of data::NetData DMR data container class.
             * @param streamId Stream ID.
             * @param external Flag indicating this traffic came from an external peer.
             * @returns bool True, if valid, otherwise false.
             */
            bool isPeerPermitted(uint32_t peerId, dmr::data::NetData& data, uint32_t streamId, bool external = false);
            /**
             * @brief Helper to validate the DMR call stream.
             * @param peerId Peer ID.
             * @param dmrData Instance of data::NetData DMR data container class.
             * @param streamId Stream ID.
             * @returns bool True, if valid, otherwise false.
             */
            bool validate(uint32_t peerId, dmr::data::NetData& data, uint32_t streamId);

            /**
             * @brief Helper to write a grant packet.
             * @param peerId Peer ID.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             * @param serviceOptions Service Options.
             * @param grp Flag indicating the destination ID is a talkgroup.
             * @returns True, if granted, otherwise false.
             */
            bool write_CSBK_Grant(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t serviceOptions, bool grp);
            /**
             * @brief Helper to write a NACK RSP packet.
             * @param peerId Peer ID.
             * @param dstId Destination ID.
             * @param reason Denial Reason.
             * @param service Service being denied.
             */
            void write_CSBK_NACK_RSP(uint32_t peerId, uint32_t dstId, uint8_t reason, uint8_t service);

            /**
             * @brief Helper to write a network CSBK.
             * @param peerId Peer ID.
             * @param slot DMR slot number.
             * @param csbk Instance of dmr::lc::CSBK.
             */
            void write_CSBK(uint32_t peerId, uint8_t slot, dmr::lc::CSBK* csbk);
        };
    } // namespace callhandler
} // namespace network

#endif // __CALLHANDLER__TAG_DMR_DATA_H__
