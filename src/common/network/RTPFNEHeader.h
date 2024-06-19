// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023,2024 Bryan Biedenkapp, N2PLL
*
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

#define RTP_FNE_HEADER_LENGTH_BYTES 16
#define RTP_FNE_HEADER_LENGTH_EXT_LEN 4

#define RTP_END_OF_CALL_SEQ 65535

namespace network
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    /// <summary>
    /// Network Functions
    /// </summary>
    namespace NET_FUNC {
        enum ENUM : uint8_t {
            ILLEGAL = 0xFFU,                        //

            PROTOCOL = 0x00U,                       // Network Protocol Function

            MASTER = 0x01U,                         // Network Master Function

            RPTL = 0x60U,                           // Repeater Login
            RPTK = 0x61U,                           // Repeater Authorisation
            RPTC = 0x62U,                           // Repeater Configuration

            RPT_CLOSING = 0x70U,                    // Repeater Closing
            MST_CLOSING = 0x71U,                    // Master Closing

            PING = 0x74U,                           // Ping
            PONG = 0x75U,                           // Pong

            GRANT_REQ = 0x7AU,                      // Grant Request

            ACK = 0x7EU,                            // Packet Acknowledge
            NAK = 0x7FU,                            // Packet Negative Acknowledge

            TRANSFER = 0x90U,                       // Network Transfer Function

            ANNOUNCE = 0x91U                        // Network Announce Function
        };
    };

    /// <summary>
    /// Network Sub-Functions
    /// </summary>
    namespace NET_SUBFUNC {
        enum ENUM : uint8_t {
            NOP = 0xFFU,                            // No Operation Sub-Function

            PROTOCOL_SUBFUNC_DMR = 0x00U,           // DMR
            PROTOCOL_SUBFUNC_P25 = 0x01U,           // P25
            PROTOCOL_SUBFUNC_NXDN = 0x02U,          // NXDN

            MASTER_SUBFUNC_WL_RID = 0x00U,          // Whitelist RIDs
            MASTER_SUBFUNC_BL_RID = 0x01U,          // Blacklist RIDs
            MASTER_SUBFUNC_ACTIVE_TGS = 0x02U,      // Active TGIDs
            MASTER_SUBFUNC_DEACTIVE_TGS = 0x03U,    // Deactive TGIDs

            TRANSFER_SUBFUNC_ACTIVITY = 0x01U,      // Activity Log Transfer
            TRANSFER_SUBFUNC_DIAG = 0x02U,          // Diagnostic Log Transfer
            TRANSFER_SUBFUNC_STATUS = 0x03U,        // Status Transfer

            ANNC_SUBFUNC_GRP_AFFIL = 0x00U,         // Announce Group Affiliation
            ANNC_SUBFUNC_UNIT_REG = 0x01U,          // Announce Unit Registration
            ANNC_SUBFUNC_UNIT_DEREG = 0x02U,        // Announce Unit Deregistration
            ANNC_SUBFUNC_AFFILS = 0x90U,            // Update All Affiliations
            ANNC_SUBFUNC_SITE_VC = 0x9AU            // Announce Site VCs
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
        //      Represents the FNE RTP Extension header.
        // ---------------------------------------------------------------------------

        class HOST_SW_API RTPFNEHeader : public RTPExtensionHeader {
        public:
            /// <summary>Initializes a new instance of the RTPFNEHeader class.</summary>
            RTPFNEHeader();
            /// <summary>Finalizes a instance of the RTPFNEHeader class.</summary>
            ~RTPFNEHeader();

            /// <summary>Decode a RTP header.</summary>
            bool decode(const uint8_t* data) override;
            /// <summary>Encode a RTP header.</summary>
            void encode(uint8_t* data) override;

        public:
            /// <summary>Traffic payload packet CRC-16.</summary>
            __PROPERTY(uint16_t, crc16, CRC);
            /// <summary>Function.</summary>
            __PROPERTY(NET_FUNC::ENUM, func, Function);
            /// <summary>Sub-function.</summary>
            __PROPERTY(NET_SUBFUNC::ENUM, subFunc, SubFunction);
            /// <summary>Traffic Stream ID.</summary>
            __PROPERTY(uint32_t, streamId, StreamId);
            /// <summary>Traffic Peer ID.</summary>
            __PROPERTY(uint32_t, peerId, PeerId);
            /// <summary>Traffic Message Length.</summary>
            __PROPERTY(uint32_t, messageLength, MessageLength);
        };
    } // namespace frame
} // namespace network

#endif // __RTP_FNE_HEADER_H__