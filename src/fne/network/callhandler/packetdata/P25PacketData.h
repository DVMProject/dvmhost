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
 * @file P25PacketData.h
 * @ingroup fne_callhandler
 * @file P25PacketData.cpp
 * @ingroup fne_callhandler
 */
#if !defined(__PACKETDATA__P25_PACKET_DATA_H__)
#define __PACKETDATA__P25_PACKET_DATA_H__

#include "fne/Defines.h"
#include "common/Clock.h"
#include "common/p25/P25Defines.h"
#include "common/p25/data/DataBlock.h"
#include "common/p25/data/DataHeader.h"
#include "network/FNENetwork.h"
#include "network/PeerNetwork.h"
#include "network/callhandler/TagP25Data.h"

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
             * @brief Implements the P25 packet data handler.
             * @ingroup fne_callhandler
             */
            class HOST_SW_API P25PacketData {
            public:
                /**
                 * @brief Initializes a new instance of the P25PacketData class.
                 * @param network Instance of the FNENetwork class.
                 * @param tag Instance of the TagP25Data class.
                 * @param debug Flag indicating whether network debug is enabled.
                 */
                P25PacketData(FNENetwork* network, TagP25Data* tag, bool debug);
                /**
                 * @brief Finalizes a instance of the P25PacketData class.
                 */
                ~P25PacketData();

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
                 * @brief Process a data frame from the virtual IP network.
                 * @param data Network data buffer.
                 * @param len Length of data.
                 * @param alreaedyQueued Flag indicating the data frame being processed is already queued.
                 */
                void processPacketFrame(const uint8_t* data, uint32_t len, bool alreadyQueued = false);

                /**
                 * @brief Updates the timer by the passed number of milliseconds.
                 * @param ms Number of milliseconds.
                 */
                void clock(uint32_t ms);

            private:
                FNENetwork* m_network;
                TagP25Data *m_tag;

                static uint8_t m_sendSeqNo;

                /**
                 * @brief Represents a queued data frame from the VTUN.
                 */
                class VTUNDataFrame {
                public:
                    uint32_t srcHWAddr;
                    uint32_t srcProtoAddr;
                    uint32_t tgtHWAddr;
                    uint32_t tgtProtoAddr;

                    uint8_t* buffer;
                    uint32_t bufferLen;
                    
                    uint16_t pktLen;
                };
                std::deque<VTUNDataFrame> m_dataFrames;

                /**
                 * @brief Represents the receive status of a call.
                 */
                class RxStatus {
                public:
                    system_clock::hrc::hrc_t callStartTime;
                    uint32_t llId;
                    uint32_t streamId;
                    uint32_t peerId;

                    p25::data::DataBlock* blockData;
                    p25::data::DataHeader header;
                    bool extendedAddress;
                    uint32_t dataOffset;
                    uint8_t dataBlockCnt;
                    uint8_t* netPDU;
                    uint32_t netPDUCount;

                    uint8_t* pduUserData;
                    uint32_t pduUserDataLength;

                    /**
                     * @brief Initializes a new instance of the RxStatus class
                     */
                    RxStatus() :
                        llId(0U),
                        streamId(0U),
                        peerId(0U),
                        blockData(nullptr),
                        header(),
                        extendedAddress(false),
                        dataOffset(0U),
                        dataBlockCnt(0U),
                        netPDU(nullptr),
                        netPDUCount(0U),
                        pduUserData(nullptr),
                        pduUserDataLength(0U)
                    {
                        blockData = new p25::data::DataBlock[P25DEF::P25_MAX_PDU_BLOCKS];

                        netPDU = new uint8_t[P25DEF::P25_PDU_FRAME_LENGTH_BYTES + 2U];
                        ::memset(netPDU, 0x00U, P25DEF::P25_PDU_FRAME_LENGTH_BYTES + 2U);

                        pduUserData = new uint8_t[P25DEF::P25_MAX_PDU_BLOCKS * P25DEF::P25_PDU_CONFIRMED_LENGTH_BYTES + 2U];
                        ::memset(pduUserData, 0x00U, P25DEF::P25_MAX_PDU_BLOCKS * P25DEF::P25_PDU_CONFIRMED_LENGTH_BYTES + 2U);
                    }
                    /**
                     * @brief Finalizes a instance of the RxStatus class
                     */
                    ~RxStatus()
                    {
                        delete[] blockData;
                        delete[] netPDU;
                        delete[] pduUserData;
                    }
                };
                typedef std::pair<const uint32_t, RxStatus*> StatusMapPair;
                std::unordered_map<uint32_t, RxStatus*> m_status;

                std::unordered_map<uint32_t, uint32_t> m_arpTable;
                std::unordered_map<uint32_t, bool> m_readyForPkt;
                std::unordered_map<uint32_t, Timer> m_suNotReadyTimeout;

                bool m_debug;

                /**
                 * @brief Helper to dispatch PDU user data.
                 * @param peerId Peer ID.
                 */
                void dispatch(uint32_t peerId);
                /**
                 * @brief Helper to dispatch PDU user data back to the FNE network.
                 * @param peerId Peer ID.
                 */
                void dispatchToFNE(uint32_t peerId);
                /**
                 * @brief Helper to dispatch PDU user data back to the FNE network.
                 * @param dataHeader Instance of a PDU data header.
                 * @param extendedAddress Flag indicating whether or not to extended addressing is in use.
                 * @param pduUserData Buffer containing user data to transmit.
                 */
                void dispatchUserFrameToFNE(p25::data::DataHeader& dataHeader, bool extendedAddress, uint8_t* pduUserData);

                /**
                 * @brief Helper used to process SNDCP control data from PDU data.
                 * @param status Instance of the RxStatus class.
                 * @returns bool True, if SNDCP control data was processed, otherwise false.
                 */
                bool processSNDCPControl(RxStatus* status);

                /**
                 * @brief Helper write ARP request to the network.
                 * @param addr IP Address.
                 */
                void write_PDU_ARP(uint32_t addr);
                /**
                 * @brief Helper write ARP reply to the network.
                 * @param targetAddr Target IP Address.
                 * @param requestorLlid Requestor Logical Link Address.
                 * @param requestorAddr Requestor IP Address.
                 * @param targetLlId Target Logical Link Address.
                 */
                void write_PDU_ARP_Reply(uint32_t targetAddr, uint32_t requestorLlid, uint32_t requestorAddr, uint32_t targetLlid = 0U);

                /**
                 * @brief Helper to write a PDU acknowledge response.
                 * @param ackClass Acknowledgement Class.
                 * @param ackType Acknowledgement Type.
                 * @param ackStatus 
                 * @param llId Logical Link ID.
                 * @param srcLlId Source Logical Link ID.
                 */
                void write_PDU_Ack_Response(uint8_t ackClass, uint8_t ackType, uint8_t ackStatus, uint32_t llId, uint32_t srcLlId = 0U);

                /**
                 * @brief Helper to write user data as a P25 PDU packet.
                 * @param peerId Peer ID.
                 * @param peerNet Instance of PeerNetwork to use to send traffic.
                 * @param dataHeader Instance of a PDU data header.
                 * @param extendedAddress Flag indicating whether or not to extended addressing is in use.
                 * @param pduUserData Buffer containing user data to transmit.
                 */
                void write_PDU_User(uint32_t peerId, network::PeerNetwork* peerNet, p25::data::DataHeader& dataHeader,
                    bool extendedAddress, uint8_t* pduUserData, bool queueOnly = false);

                /**
                 * @brief Write data processed to the network.
                 * @param peerId Peer ID.
                 * @param peerNet Instance of PeerNetwork to use to send traffic.
                 * @param dataHeader Instance of a PDU data header.
                 * @param currentBlock Current Block ID.
                 * @param data Buffer containing block data.
                 * @param len Length of buffer.
                 * @param pktSeq RTP packet sequence.
                 * @param streamId Stream ID.
                 */
                bool writeNetwork(uint32_t peerId, network::PeerNetwork* peerNet, const p25::data::DataHeader& dataHeader, const uint8_t currentBlock, 
                    const uint8_t* data, uint32_t len, uint16_t pktSeq, uint32_t streamId, bool queueOnly = false);

                /**
                 * @brief Helper to determine if the logical link ID has an ARP entry.
                 * @param llId Logical Link Address.
                 * @returns bool True, if the logical link ID has an arp entry, otherwise false.
                 */
                bool hasARPEntry(uint32_t llId) const;
                /**
                 * @brief Helper to get the IP address for the given logical link ID.
                 * @param llId Logical Link Address.
                 * @returns uint32_t Numerical IP address.
                 */
                uint32_t getIPAddress(uint32_t llId);
                /**
                 * @brief Helper to get the logical link ID granted to the given IP address.
                 * @param addr Numerical IP address.
                 * @returns uint32_t Logical Link Address.
                 */
                uint32_t getLLIdAddress(uint32_t addr);
            };
        } // namespace packetdata
    } // namespace callhandler
} // namespace network

#endif // __PACKETDATA__P25_PACKET_DATA_H__
