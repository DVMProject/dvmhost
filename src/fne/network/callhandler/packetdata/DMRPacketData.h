// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 * 
 *  Copyright (C) 2024-2026 Bryan Biedenkapp, N2PLL
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
#include "network/TrafficNetwork.h"
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
                 * @param network Instance of the TrafficNetwork class.
                 * @param tag Instance of the TagDMRData class.
                 * @param debug Flag indicating whether network debug is enabled.
                 */
                DMRPacketData(TrafficNetwork* network, TagDMRData* tag, bool debug);
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
                 * @param fromUpstream Flag indicating traffic is from a upstream master.
                 * @returns bool True, if frame is processed, otherwise false.
                 */
                bool processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint16_t pktSeq, uint32_t streamId, bool fromUpstream = false);

                /**
                 * @brief Process a data frame from the virtual IP network.
                 * @param data Network data buffer.
                 * @param len Length of data.
                 * @param alreadyQueued Flag indicating the data frame being processed is already queued.
                 */
                void processPacketFrame(const uint8_t* data, uint32_t len, bool alreadyQueued = false);

                /**
                 * @brief Helper to write a PDU acknowledge response.
                 * @param ackClass Acknowledgement Class.
                 * @param ackType Acknowledgement Type.
                 * @param ackStatus Acknowledgement Status.
                 * @param srcId Source Radio ID.
                 * @param dstId Destination Radio ID.
                 */
                void write_PDU_Ack_Response(uint8_t ackClass, uint8_t ackType, uint8_t ackStatus, uint32_t srcId, uint32_t dstId);

                /**
                 * @brief Updates the timer by the passed number of milliseconds.
                 * @param ms Number of milliseconds.
                 */
                void clock(uint32_t ms);

                /**
                 * @brief Helper to cleanup any call's left in a dangling state without any further updates.
                 */
                void cleanupStale();

            private:
                TrafficNetwork* m_network;
                TagDMRData *m_tag;

                /**
                 * @brief Represents a queued data frame from the VTUN.
                 */
                class QueuedDataFrame {
                public:
                    dmr::data::DataHeader* header;  //!< Instance of a PDU data header.
                    uint32_t dstId;                 //!< Destination Radio ID
                    uint32_t tgtProtoAddr;          //!< Target Protocol Address

                    uint8_t* userData;              //!< Raw data buffer
                    uint32_t userDataLen;           //!< Length of raw data buffer

                    uint64_t timestamp;             //!< Timestamp in milliseconds
                    uint8_t retryCnt;               //!< Packet Retry Counter
                    bool extendRetry;               //!< Flag indicating whether or not to extend the retry count for this packet.
                };
                concurrent::deque<QueuedDataFrame*> m_queuedFrames;

                /**
                 * @brief Represents the receive status of a call.
                 */
                class RxStatus {
                public:
                    system_clock::hrc::hrc_t callStartTime;     //!< Data call start time
                    system_clock::hrc::hrc_t lastPacket;        //!< Last packet time
                    uint32_t srcId;                             //!< Source Radio ID
                    uint32_t dstId;                             //!< Destination Radio ID
                    uint8_t slotNo;                             //!< DMR Slot Number
                    uint32_t streamId;                          //!< Stream ID
                    uint32_t peerId;                            //!< Peer ID

                    std::unordered_map<uint16_t, uint8_t*> receivedBlocks;
                    dmr::data::DataHeader header;               //!< PDU Data Header
                    bool hasRxHeader;                           //!< Flag indicating whether or not a valid Rx header has been received
                    uint16_t dataBlockCnt;                      //!< Number of data blocks received
                    uint8_t frames;                             //!< Number of frames received

                    bool callBusy;                              //!< Flag indicating whether or not the call is busy

                    uint8_t* pduUserData;                       //!< PDU user data buffer
                    uint32_t pduDataOffset;                     //!< Offset within the PDU data buffer

                    /**
                     * @brief Initializes a new instance of the RxStatus class
                     */
                    RxStatus() :
                        srcId(0U),
                        dstId(0U),
                        slotNo(0U),
                        streamId(0U),
                        peerId(0U),
                        receivedBlocks(),
                        header(),
                        hasRxHeader(false),
                        dataBlockCnt(0U),
                        frames(0U),
                        callBusy(false),
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
                        if (!receivedBlocks.empty()) {
                            for (auto& it : receivedBlocks) {
                                if (it.second != nullptr) {
                                    delete[] it.second;
                                    it.second = nullptr;
                                }
                            }
                            receivedBlocks.clear();
                        }

                        delete[] pduUserData;
                    }
                };
                typedef std::pair<const uint32_t, RxStatus*> StatusMapPair;
                concurrent::unordered_map<uint32_t, RxStatus*> m_status;

                typedef std::pair<const uint32_t, uint32_t> ArpTablePair;
                std::unordered_map<uint32_t, uint32_t> m_arpTable;
                typedef std::pair<const uint32_t, bool> ReadyForNextPktPair;
                std::unordered_map<uint32_t, bool> m_readyForNextPkt;
                std::unordered_map<uint32_t, uint8_t> m_suSendSeq;

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
                /**
                 * @brief Helper to dispatch PDU user data back to the local FNE network. (Will not transmit to neighbor FNE peers.)
                 * @param dataHeader Instance of a PDU data header.
                 * @param pduUserData Buffer containing user data to transmit.
                 */
                void dispatchUserFrameToFNE(dmr::data::DataHeader& dataHeader, uint8_t* pduUserData);

                /**
                 * @brief Helper write ARP request to the network.
                 * @param addr IP Address.
                 */
                void write_PDU_ARP(uint32_t addr);
                /**
                 * @brief Helper write ARP reply to the network.
                 * @param targetAddr Target IP Address.
                 * @param requestorId Requestor Radio ID.
                 * @param requestorAddr Requestor IP Address.
                 * @param targetId Target Radio ID.
                 */
                void write_PDU_ARP_Reply(uint32_t targetAddr, uint32_t requestorId, uint32_t requestorAddr, uint32_t targetId = 0U);

                /**
                 * @brief Helper to determine if the radio ID has an ARP entry.
                 * @param id Radio ID.
                 * @returns bool True, if ARP entry exists, otherwise false.
                 */
                bool hasARPEntry(uint32_t id) const;
                /**
                 * @brief Helper to get the IP address for the given radio ID.
                 * @param id Radio ID.
                 * @returns uint32_t IP address.
                 */
                uint32_t getIPAddress(uint32_t id);
                /**
                 * @brief Helper to get the radio ID for the given IP address.
                 * @param addr IP Address.
                 * @returns uint32_t Radio ID.
                 */
                uint32_t getRadioIdAddress(uint32_t addr);
            };
        } // namespace packetdata
    } // namespace callhandler
} // namespace network

#endif // __PACKETDATA__DMR_PACKET_DATA_H__
