// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023,2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file RTPFNEHeader.h
 * @ingroup network_core
 * @file RTPFNEHeader.cpp
 * @ingroup network_core
 */
#if !defined(__RTP_FNE_HEADER_H__)
#define __RTP_FNE_HEADER_H__

#include "common/Defines.h"
#include "common/network/RTPExtensionHeader.h"

#include <chrono>
#include <random>
#include <string>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define PEER_LINK_BLOCK_SIZE 534

#define RTP_FNE_HEADER_LENGTH_BYTES 16
#define RTP_FNE_HEADER_LENGTH_EXT_LEN 4

#define RTP_END_OF_CALL_SEQ 65535

namespace network
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    /**
     * @brief Network Functions
     * @ingroup network_core
     */
    namespace NET_FUNC {
        enum ENUM : uint8_t {
            ILLEGAL = 0xFFU,                        //! Illegal Function

            PROTOCOL = 0x00U,                       //! Network Protocol Function

            MASTER = 0x01U,                         //! Network Master Function

            RPTL = 0x60U,                           //! Repeater Login
            RPTK = 0x61U,                           //! Repeater Authorisation
            RPTC = 0x62U,                           //! Repeater Configuration

            RPT_CLOSING = 0x70U,                    //! Repeater Closing
            MST_CLOSING = 0x71U,                    //! Master Closing

            PING = 0x74U,                           //! Ping
            PONG = 0x75U,                           //! Pong

            GRANT_REQ = 0x7AU,                      //! Grant Request
            INCALL_CTRL = 0x7BU,                    //! In-Call Control

            ACK = 0x7EU,                            //! Packet Acknowledge
            NAK = 0x7FU,                            //! Packet Negative Acknowledge

            TRANSFER = 0x90U,                       //! Network Transfer Function

            ANNOUNCE = 0x91U,                       //! Network Announce Function

            PEER_LINK = 0x92U                       //! FNE Peer-Link Function
        };
    };

    /**
     * @brief Network Sub-Functions
     * @ingroup network_core
     */
    namespace NET_SUBFUNC {
        enum ENUM : uint8_t {
            NOP = 0xFFU,                            //! No Operation Sub-Function

            PROTOCOL_SUBFUNC_DMR = 0x00U,           //! DMR
            PROTOCOL_SUBFUNC_P25 = 0x01U,           //! P25
            PROTOCOL_SUBFUNC_NXDN = 0x02U,          //! NXDN

            MASTER_SUBFUNC_WL_RID = 0x00U,          //! Whitelist RIDs
            MASTER_SUBFUNC_BL_RID = 0x01U,          //! Blacklist RIDs
            MASTER_SUBFUNC_ACTIVE_TGS = 0x02U,      //! Active TGIDs
            MASTER_SUBFUNC_DEACTIVE_TGS = 0x03U,    //! Deactive TGIDs

            TRANSFER_SUBFUNC_ACTIVITY = 0x01U,      //! Activity Log Transfer
            TRANSFER_SUBFUNC_DIAG = 0x02U,          //! Diagnostic Log Transfer
            TRANSFER_SUBFUNC_STATUS = 0x03U,        //! Status Transfer

            ANNC_SUBFUNC_GRP_AFFIL = 0x00U,         //! Announce Group Affiliation
            ANNC_SUBFUNC_UNIT_REG = 0x01U,          //! Announce Unit Registration
            ANNC_SUBFUNC_UNIT_DEREG = 0x02U,        //! Announce Unit Deregistration
            ANNC_SUBFUNC_GRP_UNAFFIL = 0x03U,       //! Announce Group Affiliation Removal
            ANNC_SUBFUNC_AFFILS = 0x90U,            //! Update All Affiliations
            ANNC_SUBFUNC_SITE_VC = 0x9AU,           //! Announce Site VCs

            PL_TALKGROUP_LIST = 0x00U,              //! FNE Peer-Link Talkgroup Transfer
            PL_RID_LIST = 0x01U,                    //! FNE Peer-Link Radio ID Transfer
            PL_PEER_LIST = 0x02U,                   //! FNE Peer-Link Peer List Transfer

            PL_ACT_PEER_LIST = 0xA2U,               //! FNE Peer-Link Active Peer List Transfer
        };
    };

    /**
     * @brief Network In-Call Control
     * @ingroup network_core
     */
    namespace NET_ICC {
        enum ENUM : uint8_t {
            NOP = 0xFFU,                            //! No Operation Sub-Function

            BUSY_DENY = 0x00U,                      //! Busy Deny
            REJECT_TRAFFIC = 0x01U,                 //! Reject Active Traffic
        };
    };

    namespace frame
    {
        // ---------------------------------------------------------------------------
        //  Constants
        // ---------------------------------------------------------------------------

        const uint8_t DVM_FRAME_START = 0xFEU;

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents the FNE RTP Extension header.
         * \code{.unparsed}
         * Byte 0               1               2               3
         * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
         *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *     | Encoded RTP Extension Header                                  |
         *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *     | Payload CRC-16                | Function      | Sub-function  |
         *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *     | Stream ID                                                     |
         *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *     | Peer ID                                                       |
         *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *     | Message Length                                                |
         *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * 20 bytes (16 bytes without RTP Extension Header)
         * \endcode
         */
        class HOST_SW_API RTPFNEHeader : public RTPExtensionHeader {
        public:
            /**
             * @brief Initializes a new instance of the RTPFNEHeader class.
             */
            RTPFNEHeader();
            /**
             * @brief Finalizes a instance of the RTPFNEHeader class.
             */
            ~RTPFNEHeader();

            /**
             * @brief Decode a RTP header.
             * @param[in] data Buffer containing RTP FNE header to decode.
             */
            bool decode(const uint8_t* data) override;
            /**
             * @brief Encode a RTP header.
             * @param[out] data Buffer to encode an RTP FNE header.
             */
            void encode(uint8_t* data) override;

        public:
            /**
             * @brief Traffic payload packet CRC-16.
             */
            __PROPERTY(uint16_t, crc16, CRC);
            /**
             * @brief Function.
             */
            __PROPERTY(NET_FUNC::ENUM, func, Function);
            /**
             * @brief Sub-function.
             */
            __PROPERTY(NET_SUBFUNC::ENUM, subFunc, SubFunction);
            /**
             * @brief Traffic Stream ID.
             */
            __PROPERTY(uint32_t, streamId, StreamId);
            /**
             * @brief Traffic Peer ID.
             */
            __PROPERTY(uint32_t, peerId, PeerId);
            /**
             * @brief Traffic Message Length.
             */
            __PROPERTY(uint32_t, messageLength, MessageLength);
        };
    } // namespace frame
} // namespace network

#endif // __RTP_FNE_HEADER_H__