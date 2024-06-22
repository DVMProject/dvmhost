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
            p25::defines::FrameType::E frameType);
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
        void createP25_MessageHdr(uint8_t* buffer, p25::defines::DUID::E duid, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, 
            p25::defines::FrameType::E frameType = p25::defines::FrameType::DATA_UNIT);

        /// <summary>Creates an P25 LDU1 frame message.</summary>
        UInt8Array createP25_LDU1Message(uint32_t& length, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, 
            const uint8_t* data, p25::defines::FrameType::E frameType);
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
