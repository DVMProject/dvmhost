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
 * @file P25PacketData.h
 * @ingroup fne_packetdata
 * @file P25PacketData.cpp
 * @ingroup fne_packetdata
 */
#if !defined(__PACKETDATA__P25_PACKET_DATA_H__)
#define __PACKETDATA__P25_PACKET_DATA_H__

#include "fne/Defines.h"
#include "common/Clock.h"
#include "common/concurrent/unordered_map.h"
#include "common/p25/P25Defines.h"
#include "common/p25/data/Assembler.h"
#include "common/p25/data/DataHeader.h"
#include "common/p25/data/DataBlock.h"
#include "network/TrafficNetwork.h"
#include "network/PeerNetwork.h"
#include "network/callhandler/TagP25Data.h"
#include "network/callhandler/packetdata/DataRouting.h"
#include "network/callhandler/packetdata/P25PacketScheduler.h"
#include "network/callhandler/packetdata/p25/ConventionalDataService.h"
#include "network/callhandler/packetdata/p25/IPConvergenceService.h"
#include "network/callhandler/packetdata/p25/PacketDataState.h"

#include <mutex>
#include <unordered_map>
#include <vector>

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
             * @ingroup fne_packetdata
             */
            class HOST_SW_API P25PacketData {
            public:
                /**
                 * @brief Initializes a new instance of the P25PacketData class.
                 * @param network Instance of the TrafficNetwork class.
                 * @param tag Instance of the TagP25Data class.
                 * @param debug Flag indicating whether network debug is enabled.
                 */
                P25PacketData(TrafficNetwork* network, TagP25Data* tag, bool debug);
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
                 * @param fromUpstream Flag indicating traffic is from a upstream master.
                 * @returns bool True, if frame is processed, otherwise false.
                 */
                bool processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint16_t pktSeq, uint32_t streamId, bool fromUpstream = false);

                /**
                 * @brief Process a data frame from the virtual IP network.
                 * @param data Network data buffer.
                 * @param len Length of data.
                 * @param alreaedyQueued Flag indicating the data frame being processed is already queued.
                 */
                void processPacketFrame(const uint8_t* data, uint32_t len, bool alreadyQueued = false);

                /**
                 * @brief Helper to write a PDU acknowledge response.
                 * @param ackClass Acknowledgement Class.
                 * @param ackType Acknowledgement Type.
                 * @param ackStatus 
                 * @param llId Logical Link ID.
                 * @param extendedAddress Flag indicating whether or not to extended addressing is in use.
                 * @param srcLlId Source Logical Link ID.
                 */
                void write_PDU_Ack_Response(uint8_t ackClass, uint8_t ackType, uint8_t ackStatus, uint32_t llId, bool extendedAddress,
                    uint32_t srcLlId = 0U);

                /**
                 * @brief Helper used to return a KMM to the calling SU.
                 * @param data Network data buffer.
                 * @param len Length of data.
                 * @param llId Logical Link ID.
                 * @param encrypted Flag indicating whether or not the KMM frame is encrypted.
                 * @param algId Encryption Algorithm ID.
                 * @param kId Encryption Key ID.
                 * @param mi 9-byte Encryption Message Indicator.
                 */
                bool write_PDU_KMM(const uint8_t* data, uint32_t len, uint32_t llId, bool encrypted,
                    uint8_t algId = P25DEF::ALGO_UNENCRYPT, uint16_t kId = 0U, const uint8_t* mi = nullptr);

                /**
                 * @brief Updates the timer by the passed number of milliseconds.
                 * @param ms Number of milliseconds.
                 */
                void clock(uint32_t ms);

                /**
                 * @brief Helper to cleanup any call's left in a dangling state without any further updates.
                 */
                void cleanupStale();

                /**
                 * @brief Process a SNDCP data channel grant relayed by a site's control channel.
                 * @param peerId Peer ID of the control channel that issued the grant.
                 * @param llId Logical Link ID of the SU.
                 */
                void processSNDCPChannelGrant(uint32_t peerId, uint32_t llId);

                /**
                 * @brief Sets the debug flag.
                 * @param debug Debug flag.
                 */
                void setDebug(bool debug) { m_debug = debug; }

            private:
                TrafficNetwork* m_network;
                TagP25Data* m_tag;

                ::p25::data::Assembler* m_assembler;

                /**
                 * @brief Represents the data required for a PDU assembler custom writer context.
                 * @ingroup fne_network
                 */
                struct UserContext {
                    void* obj;                                  //!< Instance of the P25PacketData class.
                    uint32_t peerId;                            //!< Peer ID for this request.
                    uint32_t srcPeerId;                         //!< Source Peer ID for this request.
                    network::PeerNetwork* peerNet;              //!< Instance of the peer network for an upstream peer.
                    ::p25::data::DataHeader* header;            //!< PDU data header.
                    uint16_t pktSeq;                            //!< Packet sequence.
                    uint32_t streamId;                          //!< Stream ID.
                    bool success;                               //!< Flag indicating every network block was accepted.
                };

                P25PacketScheduler m_packetScheduler;

                /**
                 * @brief Represents the receive status of a call.
                 */
                class RxStatus {
                public:
                    system_clock::hrc::hrc_t callStartTime;     //!< Data call start time
                    system_clock::hrc::hrc_t lastPacket;        //!< Last packet time
                    uint32_t llId;                              //!< Logical Link ID
                    uint32_t streamId;                          //!< Stream ID
                    uint32_t peerId;                            //!< Peer ID

                    std::unordered_map<uint16_t, uint8_t*> receivedBlocks;
                    ::p25::data::Assembler assembler;           //!< PDU Assembler Instance
                    bool hasRxHeader;                           //!< Flag indicating whether or not a valid Rx header has been received
                    uint16_t dataBlockCnt;                      //!< Number of data blocks received
                    uint16_t totalBlocks;                       //!< Total number of blocks expected

                    bool callBusy;                              //!< Flag indicating whether or not the call is busy

                    uint8_t* pduUserData;                       //!< PDU user data buffer
                    uint32_t pduUserDataLength;                 //!< Length of PDU user data buffer

                    /**
                     * @brief Initializes a new instance of the RxStatus class
                     */
                    RxStatus() :
                        llId(0U),
                        streamId(0U),
                        peerId(0U),
                        receivedBlocks(),
                        assembler(),
                        hasRxHeader(false),
                        dataBlockCnt(0U),
                        totalBlocks(0U),
                        callBusy(false),
                        pduUserData(nullptr),
                        pduUserDataLength(0U)
                    {
                        pduUserData = new uint8_t[P25DEF::P25_MAX_PDU_BLOCKS * P25DEF::P25_PDU_CONFIRMED_LENGTH_BYTES + 2U];
                        ::memset(pduUserData, 0x00U, P25DEF::P25_MAX_PDU_BLOCKS * P25DEF::P25_PDU_CONFIRMED_LENGTH_BYTES + 2U);
                    }
                    /**
                     * @brief Finalizes a instance of the RxStatus class
                     */
                    ~RxStatus()
                    {
                        clearReceivedBlocks();
                        if (pduUserData != nullptr)
                            delete[] pduUserData;
                    }

                    /**
                     * @brief Clears all received blocks and frees associated memory.
                     */
                    void clearReceivedBlocks()
                    {
                        totalBlocks = 0U;
                        dataBlockCnt = 0U;

                        if (!receivedBlocks.empty()) {
                            for (auto& it : receivedBlocks) {
                                if (it.second != nullptr) {
                                    delete[] it.second;
                                    it.second = nullptr;
                                }
                            }
                            receivedBlocks.clear();
                        }
                    }
                };
                typedef std::pair<const uint32_t, RxStatus*> StatusMapPair;
                concurrent::unordered_map<uint32_t, RxStatus*> m_status;

                RouteNeighborCache m_neighborCache;
                p25data::DataBindingRegistry m_bindingRegistry;
                DataLocationRegistry m_locationRegistry;
                p25data::DataLinkManager m_dataLinkManager;
                p25data::ConventionalDataService m_conventionalDataService;
                p25data::SCEPService m_scepService;

                /**
                 * @brief Trunked (SNDCP) packet data state for a subscriber.
                 */
                struct SNDCPContext {
                    bool active = false;            //!< Flag indicating a context has been accepted for the SU.
                    uint8_t nsapi = 0U;             //!< NSAPI of the context.
                    uint32_t ccPeerId = 0U;         //!< Control channel peer that last granted a data channel.
                    uint32_t vcPeerId = 0U;         //!< Data channel peer the SU was last heard on.
                    uint64_t grantedAt = 0U;        //!< Timestamp of the last data channel grant.
                    uint64_t onChannelUntil = 0U;   //!< Timestamp until which the SU is taken to be on the data channel.
                    uint64_t nextPageAt = 0U;       //!< Timestamp of the earliest next data page.
                    uint8_t pageCount = 0U;         //!< Number of data pages sent for the packet being held.
                };

                std::mutex m_sndcpContextLock;
                std::unordered_map<uint32_t, SNDCPContext> m_sndcpContexts;

                /**
                 * @brief Confirmed trunked (SNDCP) downlink awaiting the subscriber's link-layer ACK.
                 */
                struct SNDCPPendingAck {
                    ::p25::data::DataHeader header;     //!< Header as sent, carrying the N(S).
                    std::vector<uint8_t> userData;      //!< User data as sent.
                    uint32_t targetIPAddress = 0U;      //!< Target IP address of the datagram.
                    uint8_t ns = 0U;                    //!< N(S) the PDU was sent with.
                    uint64_t sentAt = 0U;               //!< Timestamp of the last send, or of queuing a retransmission.
                    uint8_t retxCount = 0U;             //!< Number of retransmissions made.
                    bool resendDue = false;             //!< Flag indicating a retransmission is queued.
                };

                /**
                 * @brief Result of checking a downlink that is awaiting its link-layer ACK.
                 */
                enum class SNDCPRetransmitResult : uint8_t {
                    QUEUED,                             //!< A retransmission was queued.
                    UNACKNOWLEDGED,                     //!< Retransmissions are exhausted, the PDU was dropped.
                    NEVER_SENT                          //!< A queued retransmission never left the scheduler.
                };

                /**
                 * @brief Outcome of a retransmission check for one subscriber.
                 */
                struct SNDCPRetransmitEvent {
                    uint32_t llId;                      //!< Logical Link ID of the SU.
                    uint8_t ns;                         //!< N(S) of the PDU.
                    uint8_t attempt;                    //!< Number of retransmissions made.
                    SNDCPRetransmitResult result;       //!< Result of the check.
                };

                std::mutex m_sndcpPendingLock;          // never nested with m_sndcpContextLock
                std::unordered_map<uint32_t, SNDCPPendingAck> m_sndcpPending;

                /**
                 * @brief Helper to refresh SNDCP state when a PDU is received from a subscriber.
                 * @param llId Logical Link ID of the SU.
                 * @param peerId Peer ID the PDU arrived on.
                 */
                void touchSNDCPContext(uint32_t llId, uint32_t peerId);
                /**
                 * @brief Helper to clear the downlink awaiting a link-layer ACK from a subscriber.
                 * @param llId Logical Link ID of the SU.
                 */
                void clearSNDCPPendingAck(uint32_t llId);
                /**
                 * @brief Helper to process a link-layer ACK from a subscriber for a confirmed downlink.
                 * @param llId Logical Link ID of the SU.
                 * @param ns N(S) the ACK acknowledges.
                 */
                void processSNDCPDownlinkAck(uint32_t llId, uint8_t ns);
                /**
                 * @brief Helper to retransmit, or give up on, downlinks that were not acknowledged.
                 * @param now Current timestamp in milliseconds.
                 */
                void processSNDCPRetransmit(uint64_t now);

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
                 * @brief Helper to dispatch PDU user data back to the local FNE network. (Will not transmit to neighbor FNE peers.)
                 * @param dataHeader Instance of a PDU data header.
                 * @param extendedAddress Flag indicating whether or not to extended addressing is in use.
                 * @param auxiliaryES Flag indicating whether or not an auxiliary ES is included.
                 * @param pduUserData Buffer containing user data to transmit.
                 * @param retransmit Flag indicating a retransmission, sent with the N(S) already in the header.
                 */
                bool dispatchUserFrameToFNE(::p25::data::DataHeader& dataHeader, bool extendedAddress, bool auxiliaryES, uint8_t* pduUserData, bool retransmit = false);

                /**
                 * @brief Helper used to process conventional data registration from PDU data.
                 * @param status Instance of the RxStatus class.
                 * @returns bool True, if conventional data registration data was processed, otherwise false.
                 */
                bool processConvDataReg(RxStatus* status);
                /**
                 * @brief Helper used to process SNDCP control data from PDU data.
                 * @param status Instance of the RxStatus class.
                 * @returns bool True, if SNDCP control data was processed, otherwise false.
                 */
                bool processSNDCPControl(RxStatus* status);
                /**
                 * @brief Helper used to process SNDCP user data (SN-Data) from PDU data.
                 * @param status Instance of the RxStatus class.
                 * @param peerId Peer ID.
                 * @returns bool True, if the SNDCP header was stripped leaving an IPv4 datagram, otherwise false.
                 */
                bool processSNDCPUserData(RxStatus* status, uint32_t peerId);

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
                 * @brief Helper to write user data as a P25 PDU packet.
                 * @param peerId Peer ID.
                 * @param srcPeerId Source Peer ID.
                 * @param peerNet Instance of PeerNetwork to use to send traffic.
                 * @param dataHeader Instance of a PDU data header.
                 * @param extendedAddress Flag indicating whether or not to extended addressing is in use.
                 * @param auxiliaryES Flag indicating whether or not an auxiliary ES is included.
                 * @param pduUserData Buffer containing user data to transmit.
                 */
                bool write_PDU_User(uint32_t peerId, uint32_t srcPeerId, network::PeerNetwork* peerNet, ::p25::data::DataHeader& dataHeader,
                    bool extendedAddress, bool auxiliaryES, uint8_t* pduUserData);

                /**
                 * @brief Write data processed to the network.
                 * @param peerId Peer ID.
                 * @param srcPeerId Source Peer ID.
                 * @param peerNet Instance of PeerNetwork to use to send traffic.
                 * @param dataHeader Instance of a PDU data header.
                 * @param currentBlock Current Block ID.
                 * @param data Buffer containing block data.
                 * @param len Length of buffer.
                 * @param pktSeq RTP packet sequence.
                 * @param streamId Stream ID.
                 */
                bool writeNetwork(uint32_t peerId, uint32_t srcPeerId, network::PeerNetwork* peerNet, const ::p25::data::DataHeader& dataHeader, const uint8_t currentBlock,
                    const uint8_t* data, uint32_t len, uint16_t pktSeq, uint32_t streamId);

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
                /**
                 * @brief Helper to allocate a dynamic IP address for SNDCP.
                 * @param llId Logical Link Address.
                 * @returns uint32_t Allocated IP address.
                 */
                uint32_t allocateIPAddress(uint32_t llId);
            };
        } // namespace packetdata
    } // namespace callhandler
} // namespace network

#endif // __PACKETDATA__P25_PACKET_DATA_H__
