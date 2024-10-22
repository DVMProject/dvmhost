// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017,2018 Jonathan Naylor, G4KLX
 *  Copyright (C) 2020-2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup network_core Networking Core
 * @brief Implementation for the core networking.
 * @ingroup common
 * @defgroup socket Sockets
 * @brief Implementation for handling network sockets.
 * @ingroup network_core
 * 
 * @file BaseNetwork.h
 * @ingroup network_core
 * @file BaseNetwork.cpp
 * @ingroup network_core
 */
#if !defined(__BASE_NETWORK_H__)
#define __BASE_NETWORK_H__

#include "common/Defines.h"
#include "common/dmr/data/NetData.h"
#include "common/p25/data/DataHeader.h"
#include "common/p25/data/LowSpeedData.h"
#include "common/p25/lc/LC.h"
#include "common/p25/Audio.h"
#include "common/nxdn/lc/RTCH.h"
#include "common/network/FrameQueue.h"
#include "common/network/json/json.h"
#include "common/network/udp/Socket.h"
#include "common/RingBuffer.h"
#include "common/Utils.h"

#include <string>
#include <cstdint>
#include <random>
#include <unordered_map>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define DVM_RAND_MIN            0x00000001
#define DVM_RAND_MAX            0xfffffffe

#define TAG_DMR_DATA            "DMRD"
#define TAG_P25_DATA            "P25D"
#define TAG_NXDN_DATA           "NXDD"

#define TAG_REPEATER_LOGIN      "RPTL"
#define TAG_REPEATER_AUTH       "RPTK"
#define TAG_REPEATER_CONFIG     "RPTC"

#define TAG_REPEATER_PING       "RPTP"
#define TAG_REPEATER_GRANT      "RPTG"

#define TAG_TRANSFER            "TRNS"
#define TAG_TRANSFER_ACT_LOG    "TRNSLOG"
#define TAG_TRANSFER_DIAG_LOG   "TRNSDIAG"
#define TAG_TRANSFER_STATUS     "TRNSSTS"

#define TAG_ANNOUNCE            "ANNC"
#define TAG_PEER_LINK           "PRLNK"

namespace network
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    const uint32_t  PACKET_PAD = 8U;

    const uint32_t  MSG_HDR_SIZE = 24U;
    const uint32_t  MSG_ANNC_GRP_AFFIL = 6U;
    const uint32_t  MSG_ANNC_GRP_UNAFFIL = 3U;
    const uint32_t  MSG_ANNC_UNIT_REG = 3U;
    const uint32_t  DMR_PACKET_LENGTH = 55U;        // 20 byte header + DMR_FRAME_LENGTH_BYTES + 2 byte trailer
    const uint32_t  P25_LDU1_PACKET_LENGTH = 193U;  // 24 byte header + DFSI data + 1 byte frame type + 12 byte enc sync
    const uint32_t  P25_LDU2_PACKET_LENGTH = 181U;  // 24 byte header + DFSI data + 1 byte frame type
    const uint32_t  P25_TSDU_PACKET_LENGTH = 69U;   // 24 byte header + TSDU data

    /**
     * @brief Network Peer Connection Status
     * @ingroup network_core
     */
    enum NET_CONN_STATUS {
        // Common States
        NET_STAT_WAITING_CONNECT,                   //! Waiting for Connection
        NET_STAT_WAITING_LOGIN,                     //! Waiting for Login
        NET_STAT_WAITING_AUTHORISATION,             //! Waiting for Authorization
        NET_STAT_WAITING_CONFIG,                    //! Waiting for Configuration
        NET_STAT_RUNNING,                           //! Peer Running

        // Master States
        NET_STAT_RPTL_RECEIVED,                     //! Login Received
        NET_STAT_CHALLENGE_SENT,                    //! Authentication Challenge Sent

        NET_STAT_MST_RUNNING,                       //! Master Running

        NET_STAT_INVALID = 0x7FFFFFF                //! Invalid
    };

    /**
     * @brief Network Peer NAK Reasons
     * @ingroup network_core
     */
    enum NET_CONN_NAK_REASON {
        NET_CONN_NAK_GENERAL_FAILURE,               //! General Failure

        NET_CONN_NAK_MODE_NOT_ENABLED,              //! Mode Not Enabled
        NET_CONN_NAK_ILLEGAL_PACKET,                //! Illegal Packet

        NET_CONN_NAK_FNE_UNAUTHORIZED,              //! FNE Unauthorized
        NET_CONN_NAK_BAD_CONN_STATE,                //! Bad Connection State
        NET_CONN_NAK_INVALID_CONFIG_DATA,           //! Invalid Configuration Data
        NET_CONN_NAK_PEER_RESET,                    //! Peer Reset
        NET_CONN_NAK_PEER_ACL,                      //! Peer ACL

        NET_CONN_NAK_FNE_MAX_CONN,                  //! FNE Maximum Connections

        NET_CONN_NAK_INVALID = 0xFFFF               //! Invalid
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements the base networking logic.
     * @ingroup network_core
     */
    class HOST_SW_API BaseNetwork {
    public:
        /**
         * @brief Initializes a new instance of the BaseNetwork class.
         * @param peerId Unique ID of this modem on the network.
         * @param duplex Flag indicating full-duplex operation.
         * @param debug Flag indicating whether network debug is enabled.
         * @param slot1 Flag indicating whether DMR slot 1 is enabled for network traffic.
         * @param slot2 Flag indicating whether DMR slot 2 is enabled for network traffic.
         * @param allowActivityTransfer Flag indicating that the system activity logs will be sent to the network.
         * @param allowDiagnosticTransfer Flag indicating that the system diagnostic logs will be sent to the network.
         * @param localPort Local port used to listen for incoming data.
         */
        BaseNetwork(uint32_t peerId, bool duplex, bool debug, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, 
            uint16_t localPort = 0U);
        /**
         * @brief Finalizes a instance of the BaseNetwork class.
         */
        virtual ~BaseNetwork();

        /**
         * @brief Gets the frame queue for the network.
         */
        FrameQueue* getFrameQueue() const { return m_frameQueue; }

        /**
         * @brief Writes a grant request to the network.
         * @param mode Digital Mode.
         * @param srcId Source Radio ID.
         * @param dstId Destination Radio ID.
         * @param slot DMR slot number (if DMR).
         * @param unitToUnit Flag indicating if this grant is unit-to-unit.
         * @returns bool True, if grant request was sent, otherwise false. 
         */
        bool writeGrantReq(const uint8_t mode, const uint32_t srcId, const uint32_t dstId, const uint8_t slot, const bool unitToUnit);

        /**
         * @brief Writes the local activity log to the network.
         * @param message Textual string to send as activity log information.
         * @returns bool True, if message was sent, otherwise false. 
         */
        virtual bool writeActLog(const char* message);

        /**
         * @brief Writes the local diagnostic logs to the network.
         * @param message Textual string to send as diagnostic log information.
         * @returns bool True, if message was sent, otherwise false. 
         */
        virtual bool writeDiagLog(const char* message);

        /**
         * @brief Writes the local status to the network.
         * @param obj JSON object representing the local peer status.
         * @returns bool True, if peer status was sent, otherwise false. 
         */
        virtual bool writePeerStatus(json::object obj);

        /**
         * @brief Writes a group affiliation to the network.
         * \code{.unparsed}
         *  Below is the representation of the data layout for the group affiliation
         *  announcement message. The message is 6 bytes in length.
         * 
         *  Byte 0               1               2               3
         *  Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Source ID                                     | Dest ID       |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      |                             |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * \endcode
         * @param srcId Source Radio ID.
         * @param dstId Destination Talkgroup ID.
         * @returns bool True, if group affiliation announcement was sent, otherwise false. 
         */
        virtual bool announceGroupAffiliation(uint32_t srcId, uint32_t dstId);
        /**
         * @brief Writes a group affiliation removal to the network.
         * @param srcId Source Radio ID.
         * @returns bool True, if group affiliation announcement was sent, otherwise false. 
         */
        virtual bool announceGroupAffiliationRemoval(uint32_t srcId);

        /**
         * @brief Writes a unit registration to the network.
         * \code{.unparsed}
         *  Below is the representation of the data layout for the unit registration
         *  announcement message. The message is 3 bytes in length.
         * 
         *  Byte 0               1               2
         *  Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Source ID                                     |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * \endcode
         * @param srcId Source Radio ID.
         * @returns bool True, if unit registration announcement was sent, otherwise false. 
         */
        virtual bool announceUnitRegistration(uint32_t srcId);
        /**
         * @brief Writes a unit deregistration to the network.
         * @param srcId Source Radio ID.
         * @returns bool True, if unit deregistration announcement was sent, otherwise false. 
         */
        virtual bool announceUnitDeregistration(uint32_t srcId);

        /**
         * @brief Writes a complete update of the peer affiliation list to the network.
         * @param affs Complete map of peer unit affiliations.
         * @returns bool True, if affiliation update announcement was sent, otherwise false. 
         */
        virtual bool announceAffiliationUpdate(const std::unordered_map<uint32_t, uint32_t> affs);

        /**
         * @brief Writes a complete update of the peer's voice channel list to the network.
         * @param peers List of voice channel peers.
         * @returns bool True, if peer update announcement was sent, otherwise false. 
         */
        virtual bool announceSiteVCs(const std::vector<uint32_t> peers);

        /**
         * @brief Updates the timer by the passed number of milliseconds.
         * @param ms Number of milliseconds.
         */
        virtual void clock(uint32_t ms) = 0;

        /**
         * @brief Opens connection to the network.
         * @returns bool True, if connection to network opened, otherwise false.
         */
        virtual bool open() = 0;

        /**
         * @brief Closes connection to the network.
         */
        virtual void close() = 0;

        /**
         * @brief Resets the DMR ring buffer for the given slot.
         * @param slotNo DMR slot number.
         */
        virtual void resetDMR(uint32_t slotNo);
        /**
         * @brief Resets the P25 ring buffer.
         */
        virtual void resetP25();
        /**
         * @brief Resets the NXDN ring buffer.
         */
        virtual void resetNXDN();

        /**
         * @brief Gets the current DMR stream ID.
         * @param slotNo DMR slot to get stream ID for.
         * @return uint32_t Stream ID for the given DMR slot.
         */
        uint32_t getDMRStreamId(uint32_t slotNo) const;
        /**
         * @brief Gets the current P25 stream ID.
         * @return uint32_t Stream ID.
         */
        uint32_t getP25StreamId() const { return m_p25StreamId; }
        /**
         * @brief Gets the current NXDN stream ID.
         * @return uint32_t Stream ID.
         */
        uint32_t getNXDNStreamId() const { return m_nxdnStreamId; }

        /**
         * @brief Helper to send a data message to the master.
         * @param opcode Opcode.
         * @param data Buffer to write to the network.
         * @param length Length of buffer to write.
         * @param pktSeq RTP packet sequence.
         * @param streamId Stream ID.
         * @param queueOnly Flag indicating this message should be queued instead of send immediately.
         * @param useAlternatePort Flag indicating the message shuold be sent using the alternate port (mainly for activity and diagnostics).
         * @param peerId If non-zero, overrides the peer ID sent in the packet to the master.
         * @returns bool True, if message was sent, otherwise false. 
         */
        bool writeMaster(FrameQueue::OpcodePair opcode, const uint8_t* data, uint32_t length, 
            uint16_t pktSeq, uint32_t streamId, bool queueOnly = false, bool useAlternatePort = false, uint32_t peerId = 0U);

        // Digital Mobile Radio
        /**
         * @brief Reads DMR raw frame data from the DMR ring buffer.
         * @param[out] ret Flag indicating whether or not data was received.
         * @param[out] frameLength Length in bytes of received frame.
         * @returns UInt8Array Buffer containing received frame.
         */
        virtual UInt8Array readDMR(bool& ret, uint32_t& frameLength);
        /**
         * @brief Writes DMR frame data to the network.
         * @param[in] data Instance of the dmr::data::NetData class containing the DMR message.
         * @param noSequence Flag indicating the message should be sent with no RTP sequence (65535).
         * @returns bool True, if message was sent, otherwise false.
         */
        virtual bool writeDMR(const dmr::data::NetData& data, bool noSequence = false);

        /**
         * @brief Helper to test if the DMR ring buffer has data.
         * @returns bool True, if the network DMR ring buffer has data, otherwise false.
         */
        bool hasDMRData() const;

        // Project 25
        /**
         * @brief Reads P25 raw frame data from the P25 ring buffer.
         * @param[out] ret Flag indicating whether or not data was received.
         * @param[out] frameLength Length in bytes of received frame.
         * @returns UInt8Array Buffer containing received frame.
         */
        virtual UInt8Array readP25(bool& ret, uint32_t& frameLength);
        /**
         * @brief Writes P25 LDU1 frame data to the network.
         * @param[in] control Instance of p25::lc::LC containing link control data.
         * @param[in] lsd Instance of p25::data::LowSpeedData containing low speed data.
         * @param[in] data Buffer containing P25 LDU1 data to send.
         * @param[in] frameType DVM P25 frame type.
         * @returns bool True, if message was sent, otherwise false.
         */
        virtual bool writeP25LDU1(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t* data, 
            p25::defines::FrameType::E frameType);
        /**
         * @brief Writes P25 LDU2 frame data to the network.
         * @param[in] control Instance of p25::lc::LC containing link control data.
         * @param[in] lsd Instance of p25::data::LowSpeedData containing low speed data.
         * @param[in] data Buffer containing P25 LDU2 data to send.
         * @returns bool True, if message was sent, otherwise false.
         */
        virtual bool writeP25LDU2(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t* data);
        /**
         * @brief Writes P25 TDU frame data to the network.
         * @param[in] control Instance of p25::lc::LC containing link control data.
         * @param[in] lsd Instance of p25::data::LowSpeedData containing low speed data.
         * @param[in] controlByte DVM control byte.
         * @returns bool True, if message was sent, otherwise false.
         */
        virtual bool writeP25TDU(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t controlByte = 0U);
        /**
         * @brief Writes P25 TSDU frame data to the network.
         * @param[in] control Instance of p25::lc::LC containing link control data.
         * @param[in] data Buffer containing P25 TSDU data to send.
         * @returns bool True, if message was sent, otherwise false.
         */
        virtual bool writeP25TSDU(const p25::lc::LC& control, const uint8_t* data);
        /**
         * @brief Writes P25 PDU frame data to the network.
         * @param[in] dataHeader Instance of p25::data::DataHeader containing PDU header data.
         * @param currentBlock Current block index being sent.
         * @param[in] data Buffer containing P25 PDU block data to send.
         * @param len Length of P25 PDU block data.
         * @param lastBlock Flag indicating whether or not this is the last block of PDU data.
         * @returns bool True, if message was sent, otherwise false.
         */
        virtual bool writeP25PDU(const p25::data::DataHeader& header, const uint8_t currentBlock, const uint8_t* data, 
            const uint32_t len, bool lastBlock);

        /**
         * @brief Helper to test if the P25 ring buffer has data.
         * @returns bool True, if the network P25 ring buffer has data, otherwise false.
         */
        bool hasP25Data() const;

        // Next Generation Digital Narrowband
        /**
         * @brief Reads NXDN raw frame data from the NXDN ring buffer.
         * @param[out] ret Flag indicating whether or not data was received.
         * @param[out] frameLength Length in bytes of received frame.
         * @returns UInt8Array Buffer containing received frame.
         */
        virtual UInt8Array readNXDN(bool& ret, uint32_t& frameLength);
        /**
         * @brief Writes NXDN frame data to the network.
         * @param[in] lc Instance of nxdn::lc::RTCH containing link control data.
         * @param[in] data Buffer containing RTCH data to send.
         * @param[in] len Length of buffer.
         * @param noSequence Flag indicating the message should be sent with no RTP sequence (65535).
         * @returns bool True, if message was sent, otherwise false.
         */
        virtual bool writeNXDN(const nxdn::lc::RTCH& lc, const uint8_t* data, const uint32_t len, bool noSequence = false);

        /**
         * @brief Helper to test if the NXDN ring buffer has data.
         * @returns bool True, if the network NXDN ring buffer has data, otherwise false.
         */
        bool hasNXDNData() const;

    public:
        /**
         * @brief Gets the peer ID of the network.
         */
        __PROTECTED_READONLY_PROPERTY(uint32_t, peerId, PeerId);
        /**
         * @brief Gets the current status of the network.
         */
        __PROTECTED_READONLY_PROPERTY(NET_CONN_STATUS, status, Status);

        /**
         * @brief Unix socket storage containing the connected address.
         */
        __PROTECTED_READONLY_PROPERTY_PLAIN(sockaddr_storage, addr);
        /**
         * @brief Length of the sockaddr_storage structure.
         */
        __PROTECTED_READONLY_PROPERTY_PLAIN(uint32_t, addrLen);

        /**
         * @brief Flag indicating whether network DMR slot 1 traffic is permitted.
         */
        __PROTECTED_READONLY_PROPERTY(bool, slot1, DMRSlot1);
        /**
         * @brief Flag indicating whether network DMR slot 2 traffic is permitted.
         */
        __PROTECTED_READONLY_PROPERTY(bool, slot2, DMRSlot2);

        /**
         * @brief Flag indicating whether network traffic is duplex.
         */
        __PROTECTED_READONLY_PROPERTY(bool, duplex, Duplex);

    protected:
        bool m_useAlternatePortForDiagnostics;

        bool m_allowActivityTransfer;
        bool m_allowDiagnosticTransfer;

        bool m_debug;

        udp::Socket* m_socket;
        FrameQueue* m_frameQueue;

        RingBuffer<uint8_t> m_rxDMRData;
        RingBuffer<uint8_t> m_rxP25Data;
        RingBuffer<uint8_t> m_rxNXDNData;

        std::mt19937 m_random;

        uint32_t* m_dmrStreamId;
        uint32_t m_p25StreamId;
        uint32_t m_nxdnStreamId;

        /**
         * @brief Helper to update the RTP packet sequence.
         * @param reset Flag indicating the current RTP packet sequence value should be reset.
         * @returns uint16_t RTP packet sequence.
         */
        uint16_t pktSeq(bool reset = false);

        /**
         * @brief Generates a new stream ID.
         * @returns uint32_t New stream ID.
         */
        uint32_t createStreamId() { std::uniform_int_distribution<uint32_t> dist(DVM_RAND_MIN, DVM_RAND_MAX); return dist(m_random); }

        /**
         * @brief Creates an DMR frame message.
         * \code{.unparsed}
         *  Below is the representation of the data layout for the DMR frame
         *  message header. The header is 20 bytes in length.
         * 
         *  Byte 0               1               2               3
         *  Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Protocol Tag (DMRD)                                           |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Seq No.       | Source ID                                     |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Destination ID                                | Reserved      |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Reserved                      | Control Flags |S|G| Data Type |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Reserved                                                      |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * 
         *  The data starting at offset 20 for 33 bytes if the raw DMR frame.
         * 
         *  DMR frame message has 2 trailing bytes:
         * 
         *  Byte 53              54
         *  Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | BER           | RSSI          |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * \endcode
         * @param[out] length Length of network message buffer.
         * @param streamId Stream ID.
         * @param data Instance of the dmr::data::Data class containing the DMR message.
         * @returns UInt8Array Buffer containing the built network message.
         */
        UInt8Array createDMR_Message(uint32_t& length, const uint32_t streamId, const dmr::data::NetData& data);

        /**
         * @brief Creates an P25 frame message header.
         * \code{.unparsed}
         *  Below is the representation of the data layout for the P25 frame
         *  message header. The header is 24 bytes in length.
         * 
         *  Byte 0               1               2               3
         *  Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Protocol Tag (P25D)                                           |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | LCO           | Source ID                                     |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Destination ID                                | System ID     |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | System ID     | Reserved      | Control Flags | MFId          |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Network ID                                    | Reserved      |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | LSD1          | LSD2          | DUID          | Frame Length  |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * 
         *  The data starting at offset 20 for variable number of bytes (DUID dependant)
         *  is the P25 frame.
         * 
         *  If the P25 frame message is a LDU1, it contains 13 trailing bytes that
         *  contain the frame type, and encryption data.
         * 
         *  Byte 180             181             182             183
         *  Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Frame Type    | Algorithm ID  | Key ID                        |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Message Indicator                                             |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      |                                                               |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      |               |
         *      +-+-+-+-+-+-+-+-+
         * \endcode
         * @param buffer Buffer to store the P25 network message header.
         * @param duid P25 DUID.
         * @param[in] control Instance of p25::lc::LC containing link control data.
         * @param[in] lsd Instance of p25::data::LowSpeedData containing low speed data.
         * @param[in] frameType DVM P25 frame type.
         */
        void createP25_MessageHdr(uint8_t* buffer, p25::defines::DUID::E duid, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, 
            p25::defines::FrameType::E frameType = p25::defines::FrameType::DATA_UNIT);

        /**
         * @brief Creates an P25 LDU1 frame message.
         * 
         *  The data packed into a P25 LDU1 frame message is near standard DFSI messaging, just instead of
         *  9 individual frames, they are packed into a single message one right after another.
         * 
         * @param[out] length Length of network message buffer.
         * @param[in] control Instance of p25::lc::LC containing link control data.
         * @param[in] lsd Instance of p25::data::LowSpeedData containing low speed data.
         * @param[in] data Buffer containing P25 LDU1 data to send.
         * @param[in] frameType DVM P25 frame type.
         * @returns UInt8Array Buffer containing the built network message.
         */
        UInt8Array createP25_LDU1Message(uint32_t& length, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, 
            const uint8_t* data, p25::defines::FrameType::E frameType);
        /**
         * @brief Creates an P25 LDU2 frame message.
         * 
         *  The data packed into a P25 LDU2 frame message is near standard DFSI messaging, just instead of
         *  9 individual frames, they are packed into a single message one right after another.
         * 
         * @param[out] length Length of network message buffer.
         * @param[in] control Instance of p25::lc::LC containing link control data.
         * @param[in] lsd Instance of p25::data::LowSpeedData containing low speed data.
         * @param[in] data Buffer containing P25 LDU2 data to send.
         * @returns UInt8Array Buffer containing the built network message.
         */
        UInt8Array createP25_LDU2Message(uint32_t& length, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, 
            const uint8_t* data);

        /**
         * @brief Creates an P25 TDU frame message.
         * 
         *  The data packed into a P25 TDU frame message is essentially just a message header with control bytes
         *  set.
         * 
         * @param[out] length Length of network message buffer.
         * @param[in] control Instance of p25::lc::LC containing link control data.
         * @param[in] lsd Instance of p25::data::LowSpeedData containing low speed data.
         * @param controlByte DVM control byte.
         * @returns UInt8Array Buffer containing the built network message.
         */
        UInt8Array createP25_TDUMessage(uint32_t& length, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd,
            const uint8_t controlByte);

        /**
         * @brief Creates an P25 TSDU frame message.
         * 
         *  The data packed into a P25 TSDU frame message is essentially just a message header with the FEC encoded
         *  raw TSDU data.
         * 
         * @param[out] length Length of network message buffer.
         * @param[in] control Instance of p25::lc::LC containing link control data.
         * @param[in] data Buffer containing P25 TSDU data to send.
         * @returns UInt8Array Buffer containing the built network message.
         */
        UInt8Array createP25_TSDUMessage(uint32_t& length, const p25::lc::LC& control, const uint8_t* data);

        /**
         * @brief Creates an P25 PDU frame message.
         * \code{.unparsed}
         *  Below is the representation of the data layout for the P25 frame
         *  message header used for a PDU. The header is 24 bytes in length.
         * 
         *  Byte 0               1               2               3
         *  Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Protocol Tag (P25D)                                           |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      |C| SAP         | Reserved                                      |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | PDU Length (Bytes)                            | Reserved      |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      |                                               | MFId          |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Reserved                                                      |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Blk to Flw    | Current Block | DUID          | Frame Length  |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * 
         *  The data starting at offset 24 for variable number of bytes (DUID dependant)
         *  is the P25 frame.
         * \endcode
         * @param[out] length Length of network message buffer.
         * @param[in] header Instance of p25::data::DataHeader containing PDU header data.
         * @param currentBlock Current block index being sent.
         * @param[in] data Buffer containing P25 PDU block data to send.
         * @param len Length of P25 PDU block data.
         * @returns UInt8Array Buffer containing the built network message.
         */
        UInt8Array createP25_PDUMessage(uint32_t& length, const p25::data::DataHeader& header, const uint8_t currentBlock,
            const uint8_t* data, const uint32_t len);
        
        /**
         * @brief Creates an NXDN frame message.
         * \code{.unparsed}
         *  Below is the representation of the data layout for the NXDN frame
         *  message header. The header is 24 bytes in length.
         * 
         *  Byte 0               1               2               3
         *  Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Protocol Tag (NXDD)                                           |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Message Type  | Source ID                                     |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Destination ID                                | Reserved      |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Reserved                      | Control Flags |R|G| Reserved  |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      | Reserved                                                      |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      |                                               | Frame Length  |
         *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * 
         *  The data starting at offset 24 for 48 bytes if the raw NXDN frame.
         * \endcode
         * @param[out] length Length of network message buffer.
         * @param[in] lc Instance of nxdn::lc::RTCH containing link control data.
         * @param[in] data Buffer containing RTCH data to send.
         * @param[in] len Length of buffer.
         * @returns UInt8Array Buffer containing the built network message.
         */
        UInt8Array createNXDN_Message(uint32_t& length, const nxdn::lc::RTCH& lc, const uint8_t* data, const uint32_t len);
    
    private:
        uint16_t m_pktSeq;

        p25::Audio m_audio;
    };
} // namespace network

#endif // __BASE_NETWORK_H__
