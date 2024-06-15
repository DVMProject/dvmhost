// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2015,2016,2017,2018 Jonathan Naylor, G4KLX
*   Copyright (C) 2020-2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__BASE_NETWORK_H__)
#define __BASE_NETWORK_H__

#include "common/Defines.h"
#include "common/dmr/data/Data.h"
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

#define DVM_RAND_MIN 0x00000001
#define DVM_RAND_MAX 0xfffffffe

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

namespace network
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    const uint32_t  PACKET_PAD = 8U;

    const uint32_t  MSG_HDR_SIZE = 24U;
    const uint32_t  MSG_ANNC_GRP_AFFIL = 6U;
    const uint32_t  MSG_ANNC_UNIT_REG = 3U;
    const uint32_t  DMR_PACKET_LENGTH = 55U;        // 20 byte header + DMR_FRAME_LENGTH_BYTES + 2 byte trailer
    const uint32_t  P25_LDU1_PACKET_LENGTH = 193U;  // 24 byte header + DFSI data + 1 byte frame type + 12 byte enc sync
    const uint32_t  P25_LDU2_PACKET_LENGTH = 181U;  // 24 byte header + DFSI data + 1 byte frame type
    const uint32_t  P25_TSDU_PACKET_LENGTH = 69U;   // 24 byte header + TSDU data

    const uint8_t   NET_SUBFUNC_NOP = 0xFFU;                                    // No Operation Sub-Function

    const uint8_t   NET_FUNC_PROTOCOL = 0x00U;                                  // Network Protocol Function
    const uint8_t   NET_PROTOCOL_SUBFUNC_DMR = 0x00U;                           // DMR
    const uint8_t   NET_PROTOCOL_SUBFUNC_P25 = 0x01U;                           // P25
    const uint8_t   NET_PROTOCOL_SUBFUNC_NXDN = 0x02U;                          // NXDN

    const uint8_t   NET_FUNC_MASTER = 0x01U;                                    // Network Master Function
    const uint8_t   NET_MASTER_SUBFUNC_WL_RID = 0x00U;                          // Whitelist RIDs
    const uint8_t   NET_MASTER_SUBFUNC_BL_RID = 0x01U;                          // Blacklist RIDs
    const uint8_t   NET_MASTER_SUBFUNC_ACTIVE_TGS = 0x02U;                      // Active TGIDs
    const uint8_t   NET_MASTER_SUBFUNC_DEACTIVE_TGS = 0x03U;                    // Deactive TGIDs

    const uint8_t   NET_FUNC_RPTL = 0x60U;                                      // Repeater Login
    const uint8_t   NET_FUNC_RPTK = 0x61U;                                      // Repeater Authorisation
    const uint8_t   NET_FUNC_RPTC = 0x62U;                                      // Repeater Configuration
    
    const uint8_t   NET_FUNC_RPT_CLOSING = 0x70U;                               // Repeater Closing
    const uint8_t   NET_FUNC_MST_CLOSING = 0x71U;                               // Master Closing

    const uint8_t   NET_FUNC_PING = 0x74U;                                      // Ping
    const uint8_t   NET_FUNC_PONG = 0x75U;                                      // Pong

    const uint8_t   NET_FUNC_GRANT_REQ = 0x7AU;                                 // Grant Request

    const uint8_t   NET_FUNC_ACK = 0x7EU;                                       // Packet Acknowledge
    const uint8_t   NET_FUNC_NAK = 0x7FU;                                       // Packet Negative Acknowledge

    const uint8_t   NET_FUNC_TRANSFER = 0x90U;                                  // Network Transfer Function
    const uint8_t   NET_TRANSFER_SUBFUNC_ACTIVITY = 0x01U;                      // Activity Log Transfer
    const uint8_t   NET_TRANSFER_SUBFUNC_DIAG = 0x02U;                          // Diagnostic Log Transfer
    const uint8_t   NET_TRANSFER_SUBFUNC_STATUS = 0x03U;                        // Status Transfer

    const uint8_t   NET_FUNC_ANNOUNCE = 0x91U;                                  // Network Announce Function
    const uint8_t   NET_ANNC_SUBFUNC_GRP_AFFIL = 0x00U;                         // Announce Group Affiliation
    const uint8_t   NET_ANNC_SUBFUNC_UNIT_REG = 0x01U;                          // Announce Unit Registration
    const uint8_t   NET_ANNC_SUBFUNC_UNIT_DEREG = 0x02U;                        // Announce Unit Deregistration
    const uint8_t   NET_ANNC_SUBFUNC_AFFILS = 0x90U;                            // Update All Affiliations
    const uint8_t   NET_ANNC_SUBFUNC_SITE_VC = 0x9AU;                           // Announce Site VCs

    // ---------------------------------------------------------------------------
    //  Network Peer Connection Status
    // ---------------------------------------------------------------------------

    enum NET_CONN_STATUS {
        // Common States
        NET_STAT_WAITING_CONNECT,
        NET_STAT_WAITING_LOGIN,
        NET_STAT_WAITING_AUTHORISATION,
        NET_STAT_WAITING_CONFIG,
        NET_STAT_RUNNING,

        // Master States
        NET_STAT_RPTL_RECEIVED,
        NET_STAT_CHALLENGE_SENT,

        NET_STAT_MST_RUNNING,

        NET_STAT_INVALID = 0x7FFFFFF
    };

    // ---------------------------------------------------------------------------
    //  Network Peer NAK Reasons
    // ---------------------------------------------------------------------------

    enum NET_CONN_NAK_REASON {
        NET_CONN_NAK_GENERAL_FAILURE,

        NET_CONN_NAK_MODE_NOT_ENABLED,
        NET_CONN_NAK_ILLEGAL_PACKET,

        NET_CONN_NAK_FNE_UNAUTHORIZED,
        NET_CONN_NAK_BAD_CONN_STATE,
        NET_CONN_NAK_INVALID_CONFIG_DATA,
        NET_CONN_NAK_PEER_RESET,
        NET_CONN_NAK_PEER_ACL,

        NET_CONN_NAK_FNE_MAX_CONN,

        NET_CONN_NAK_INVALID = 0xFFFF
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements the base networking logic.
    // ---------------------------------------------------------------------------

    class HOST_SW_API BaseNetwork {
    public:
        /// <summary>Initializes a new instance of the BaseNetwork class.</summary>
        BaseNetwork(uint32_t peerId, bool duplex, bool debug, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, 
            uint16_t localPort = 0U);
        /// <summary>Finalizes a instance of the BaseNetwork class.</summary>
        virtual ~BaseNetwork();

        /// <summary>Gets the frame queue for the network.</summary>
        FrameQueue* getFrameQueue() const { return m_frameQueue; }

        /// <summary>Writes a grant request to the network.</summary>
        bool writeGrantReq(const uint8_t mode, const uint32_t srcId, const uint32_t dstId, const uint8_t slot, const bool unitToUnit);

        /// <summary>Writes the local activity log to the network.</summary>
        virtual bool writeActLog(const char* message);

        /// <summary>Writes the local diagnostic logs to the network.</summary>
        virtual bool writeDiagLog(const char* message);

        /// <summary>Writes the local status to the network.</summary>
        virtual bool writePeerStatus(json::object obj);

        /// <summary>Writes a group affiliation to the network.</summary>
        virtual bool announceGroupAffiliation(uint32_t srcId, uint32_t dstId);
        /// <summary>Writes a unit registration to the network.</summary>
        virtual bool announceUnitRegistration(uint32_t srcId);
        /// <summary>Writes a unit deregistration to the network.</summary>
        virtual bool announceUnitDeregistration(uint32_t srcId);
        /// <summary>Writes a complete update of the peer affiliation list to the network.</summary>
        virtual bool announceAffiliationUpdate(const std::unordered_map<uint32_t, uint32_t> affs);
        /// <summary>Writes a complete update of the peer's voice channel list to the network.</summary>
        virtual bool announceSiteVCs(const std::vector<uint32_t> peers);

        /// <summary>Updates the timer by the passed number of milliseconds.</summary>
        virtual void clock(uint32_t ms) = 0;

        /// <summary>Opens connection to the network.</summary>
        virtual bool open() = 0;

        /// <summary>Closes connection to the network.</summary>
        virtual void close() = 0;

        /// <summary>Resets the DMR ring buffer for the given slot.</summary>
        virtual void resetDMR(uint32_t slotNo);
        /// <summary>Resets the P25 ring buffer.</summary>
        virtual void resetP25();
        /// <summary>Resets the NXDN ring buffer.</summary>
        virtual void resetNXDN();

        /// <summary>Gets the current DMR stream ID.</summary>
        uint32_t getDMRStreamId(uint32_t slotNo) const;
        /// <summary>Gets the current P25 stream ID.</summary>
        uint32_t getP25StreamId() const { return m_p25StreamId; }
        /// <summary>Gets the current NXDN stream ID.</summary>
        uint32_t getNXDNStreamId() const { return m_nxdnStreamId; }

        /// <summary>Helper to send a data message to the master.</summary>
        bool writeMaster(FrameQueue::OpcodePair opcode, const uint8_t* data, uint32_t length, 
            uint16_t pktSeq, uint32_t streamId, bool queueOnly = false, bool useAlternatePort = false);

        /** Digital Mobile Radio */
        /// <summary>Reads DMR raw frame data from the DMR ring buffer.</summary>
        virtual UInt8Array readDMR(bool& ret, uint32_t& frameLength);
        /// <summary>Writes DMR frame data to the network.</summary>
        virtual bool writeDMR(const dmr::data::Data& data, bool noSequence = false);

        /// <summary>Helper to test if the DMR ring buffer has data.</summary>
        bool hasDMRData() const;

        /** Project 25 */
        /// <summary>Reads P25 raw frame data from the P25 ring buffer.</summary>
        virtual UInt8Array readP25(bool& ret, uint32_t& frameLength);
        /// <summary>Writes P25 LDU1 frame data to the network.</summary>
        virtual bool writeP25LDU1(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t* data, 
            uint8_t frameType);
        /// <summary>Writes P25 LDU2 frame data to the network.</summary>
        virtual bool writeP25LDU2(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t* data);
        /// <summary>Writes P25 TDU frame data to the network.</summary>
        virtual bool writeP25TDU(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t controlByte = 0U);
        /// <summary>Writes P25 TSDU frame data to the network.</summary>
        virtual bool writeP25TSDU(const p25::lc::LC& control, const uint8_t* data);
        /// <summary>Writes P25 PDU frame data to the network.</summary>
        virtual bool writeP25PDU(const p25::data::DataHeader& header, const uint8_t currentBlock, const uint8_t* data, 
            const uint32_t len, bool lastBlock);

        /// <summary>Helper to test if the P25 ring buffer has data.</summary>
        bool hasP25Data() const;

        /** Next Generation Digital Narrowband */
        /// <summary>Reads NXDN raw frame data from the NXDN ring buffer.</summary>
        virtual UInt8Array readNXDN(bool& ret, uint32_t& frameLength);
        /// <summary>Writes NXDN frame data to the network.</summary>
        virtual bool writeNXDN(const nxdn::lc::RTCH& lc, const uint8_t* data, const uint32_t len, bool noSequence = false);

        /// <summary>Helper to test if the NXDN ring buffer has data.</summary>
        bool hasNXDNData() const;

    public:
        /// <summary>Gets the peer ID of the network.</summary>
        __PROTECTED_READONLY_PROPERTY(uint32_t, peerId, PeerId);
        /// <summary>Gets the current status of the network.</summary>
        __PROTECTED_READONLY_PROPERTY(NET_CONN_STATUS, status, Status);

        /// <summary>Unix socket storage containing the connected address.</summary>
        __PROTECTED_READONLY_PROPERTY_PLAIN(sockaddr_storage, addr);
        /// <summary>Length of the sockaddr_storage structure.</summary>
        __PROTECTED_READONLY_PROPERTY_PLAIN(uint32_t, addrLen);

        /// <summary>Flag indicating whether network DMR slot 1 traffic is permitted.</summary>
        __PROTECTED_READONLY_PROPERTY(bool, slot1, DMRSlot1);
        /// <summary>Flag indicating whether network DMR slot 2 traffic is permitted.</summary>
        __PROTECTED_READONLY_PROPERTY(bool, slot2, DMRSlot2);

        /// <summary>Flag indicating whether network traffic is duplex.</summary>
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

        /// <summary>Helper to update the RTP packet sequence.</summary>
        uint16_t pktSeq(bool reset = false);

        /// <summary>Generates a new stream ID.</summary>
        uint32_t createStreamId() { std::uniform_int_distribution<uint32_t> dist(DVM_RAND_MIN, DVM_RAND_MAX); return dist(m_random); }

        /// <summary>Creates an DMR frame message.</summary>
        UInt8Array createDMR_Message(uint32_t& length, const uint32_t streamId, const dmr::data::Data& data);

        /// <summary>Creates an P25 frame message header.</summary>
        void createP25_MessageHdr(uint8_t* buffer, uint8_t duid, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, 
            uint8_t frameType = p25::P25_FT_DATA_UNIT);

        /// <summary>Creates an P25 LDU1 frame message.</summary>
        UInt8Array createP25_LDU1Message(uint32_t& length, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, 
            const uint8_t* data, uint8_t frameType);
        /// <summary>Creates an P25 LDU2 frame message.</summary>
        UInt8Array createP25_LDU2Message(uint32_t& length, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, 
            const uint8_t* data);

        /// <summary>Creates an P25 TDU frame message.</summary>
        UInt8Array createP25_TDUMessage(uint32_t& length, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd,
            const uint8_t controlByte);

        /// <summary>Creates an P25 TSDU frame message.</summary>
        UInt8Array createP25_TSDUMessage(uint32_t& length, const p25::lc::LC& control, const uint8_t* data);

        /// <summary>Creates an P25 PDU frame message.</summary>
        UInt8Array createP25_PDUMessage(uint32_t& length, const p25::data::DataHeader& header, const uint8_t currentBlock,
            const uint8_t* data, const uint32_t len);
        
        /// <summary>Creates an NXDN frame message.</summary>
        UInt8Array createNXDN_Message(uint32_t& length, const nxdn::lc::RTCH& lc, const uint8_t* data, const uint32_t len);
    
    private:
        uint16_t m_pktSeq;

        p25::Audio m_audio;
    };
} // namespace network

#endif // __BASE_NETWORK_H__
