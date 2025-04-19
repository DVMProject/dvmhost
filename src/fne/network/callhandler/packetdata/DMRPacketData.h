// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 * 
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file DMRPacketData.h
 * @ingroup fne_callhandler
 * @file DMRPacketData.cpp
 * @ingroup fne_callhandler
 */
#if !defined(__PACKETDATA__DMR_PACKET_DATA_H__)
#define __PACKETDATA__DMR_PACKET_DATA_H__

#include "fne/Defines.h"
#include "common/Clock.h"
#include "common/concurrent/deque.h"
#include "common/concurrent/unordered_map.h"
#include "common/dmr/DMRDefines.h"
#include "common/dmr/data/DataHeader.h"
#include "network/FNENetwork.h"
#include "network/PeerNetwork.h"
#include "network/callhandler/TagDMRData.h"

#include <deque>

namespace network
{
    namespace callhandler
    {
        namespace packetdata
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief Implements the DMR packet data handler.
             * @ingroup fne_callhandler
             */
            class HOST_SW_API DMRPacketData {
            public:
                /**
                 * @brief Initializes a new instance of the DMRPacketData class.
                 * @param network Instance of the FNENetwork class.
                 * @param tag Instance of the TagDMRData class.
                 * @param debug Flag indicating whether network debug is enabled.
                 */
                DMRPacketData(FNENetwork* network, TagDMRData* tag, bool debug);
                /**
                 * @brief Finalizes a instance of the P25PacketData class.
                 */
                ~DMRPacketData();

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

            private:
                FNENetwork* m_network;
                TagDMRData *m_tag;

                /**
                 * @brief Represents the receive status of a call.
                 */
                class RxStatus {
                public:
                    system_clock::hrc::hrc_t callStartTime;
                    uint32_t srcId;
                    uint32_t dstId;
                    uint8_t slotNo;
                    uint32_t streamId;
                    uint32_t peerId;

                    dmr::data::DataHeader header;
                    uint8_t dataBlockCnt;
                    uint8_t frames;

                    uint8_t* pduUserData;
                    uint32_t pduDataOffset;

                    /**
                     * @brief Initializes a new instance of the RxStatus class
                     */
                    RxStatus() :
                        srcId(0U),
                        dstId(0U),
                        slotNo(0U),
                        streamId(0U),
                        peerId(0U),
                        header(),
                        dataBlockCnt(0U),
                        pduUserData(nullptr),
                        pduDataOffset(0U)
                    {
                        pduUserData = new uint8_t[DMRDEF::MAX_PDU_COUNT * DMRDEF::DMR_PDU_UNCODED_LENGTH_BYTES + 2U];
                        ::memset(pduUserData, 0x00U, DMRDEF::MAX_PDU_COUNT * DMRDEF::DMR_PDU_UNCODED_LENGTH_BYTES + 2U);
                    }
                    /**
                     * @brief Finalizes a instance of the RxStatus class
                     */
                    ~RxStatus()
                    {
                        delete[] pduUserData;
                    }
                };
                typedef std::pair<const uint32_t, RxStatus*> StatusMapPair;
                concurrent::unordered_map<uint32_t, RxStatus*> m_status;

                bool m_debug;

                /**
                 * @brief Helper to dispatch PDU user data.
                 * @param peerId Peer ID.
                 * @param dmrData Instance of data::NetData DMR data container class.
                 * @param data Network data buffer.
                 * @param len Length of data.
                 */
                void dispatch(uint32_t peerId, dmr::data::NetData& dmrData, const uint8_t* data, uint32_t len);
                /**
                 * @brief Helper to dispatch PDU user data back to the FNE network.
                 * @param peerId Peer ID.
                 * @param dmrData Instance of data::NetData DMR data container class.
                 * @param data Network data buffer.
                 * @param len Length of data.
                 * @param seqNo 
                 * @param pktSeq RTP packet sequence.
                 * @param streamId Stream ID.
                 */
                void dispatchToFNE(uint32_t peerId, dmr::data::NetData& dmrData, const uint8_t* data, uint32_t len, uint8_t seqNo, uint16_t pktSeq, uint32_t streamId);
            };
        } // namespace packetdata
    } // namespace callhandler
} // namespace network

#endif // __PACKETDATA__DMR_PACKET_DATA_H__
