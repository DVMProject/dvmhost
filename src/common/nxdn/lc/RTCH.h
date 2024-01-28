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
*   Copyright (C) 2018 Jonathan Naylor, G4KLX
*   Copyright (C) 2022 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__NXDN_LC__RTCH_H__)
#define  __NXDN_LC__RTCH_H__

#include "common/Defines.h"
#include "common/nxdn/lc/PacketInformation.h"

namespace nxdn
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents link control data for traffic channel NXDN calls.
        // ---------------------------------------------------------------------------

        class HOST_SW_API RTCH {
        public:
            /// <summary>Initializes a new instance of the RTCH class.</summary>
            RTCH();
            /// <summary>Initializes a copy instance of the RTCH class.</summary>
            RTCH(const RTCH& data);
            /// <summary>Finalizes a instance of the RTCH class.</summary>
            ~RTCH();

            /// <summary>Equals operator.</summary>
            RTCH& operator=(const RTCH& data);

            /// <summary>Decode layer 3 data.</summary>
            void decode(const uint8_t* data, uint32_t length, uint32_t offset = 0U);
            /// <summary>Encode layer 3 data.</summary>
            void encode(uint8_t* data, uint32_t length, uint32_t offset = 0U);

            /// <summary></summary>
            void reset();

            /// <summary>Sets the flag indicating verbose log output.</summary>
            static void setVerbose(bool verbose) { m_verbose = verbose; }

        public:
            /** Common Data */
            /// <summary>Message Type</summary>
            __PROPERTY(uint8_t, messageType, MessageType);

            /// <summary>Call Type</summary>
            __PROPERTY(uint8_t, callType, CallType);

            /// <summary>Source ID.</summary>
            __PROPERTY(uint16_t, srcId, SrcId);
            /// <summary>Destination ID.</summary>
            __PROPERTY(uint16_t, dstId, DstId);

            /** Common Call Options */
            /// <summary>Flag indicating the emergency bits are set.</summary>
            __PROPERTY(bool, emergency, Emergency);
            /// <summary>Flag indicating that encryption is enabled.</summary>
            __PROPERTY(bool, encrypted, Encrypted);
            /// <summary>Flag indicating priority paging.</summary>
            __PROPERTY(bool, priority, Priority);
            /// <summary>Flag indicating a group/talkgroup operation.</summary>
            __PROPERTY(bool, group, Group);
            /// <summary>Flag indicating a half/full duplex operation.</summary>
            __PROPERTY(bool, duplex, Duplex);

            /// <summary>Transmission mode.</summary>
            __PROPERTY(uint8_t, transmissionMode, TransmissionMode);

            /** Data Call Data */
            /// <summary>Data packet information.</summary>
            __PROPERTY(PacketInformation, packetInfo, PacketInfo);
            /// <summary>Data packet information.</summary>
            __PROPERTY(PacketInformation, rsp, Response);
            /// <summary>Data packet frame number.</summary>
            __PROPERTY(uint8_t, dataFrameNumber, DataFrameNumber);
            /// <summary>Data packet block number.</summary>
            __PROPERTY(uint8_t, dataBlockNumber, DataBlockNumber);

            /** Header Delay Data */
            /// <summary>Delay count.</summary>
            __PROPERTY(uint16_t, delayCount, DelayCount);

            /** Encryption data */
            /// <summary>Encryption algorithm ID.</summary>
            __PROPERTY(uint8_t, algId, AlgId);
            /// <summary>Encryption key ID.</summary>
            __PROPERTY(uint8_t, kId, KId);

            /// <summary>Cause Response.</summary>
            __PROPERTY(uint8_t, causeRsp, CauseResponse);

        private:
            static bool m_verbose;

            /** Encryption data */
            uint8_t* m_mi;

            /// <summary>Decode link control.</summary>
            bool decodeLC(const uint8_t* data);
            /// <summary>Encode link control.</summary>
            void encodeLC(uint8_t* data);

            /// <summary>Internal helper to copy the class.</summary>
            void copy(const RTCH& data);
        };
    } // namespace lc
} // namespace nxdn

#endif // __NXDN_LC__RTCH_H__
