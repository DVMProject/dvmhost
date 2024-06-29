// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2018 Jonathan Naylor, G4KLX
 *  Copyright (C) 2022 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup nxdn_lc Link Control
 * @brief Implementation for the data handling of NXDN traffic channel messages.
 * @ingroup nxdn
 * 
 * @file RTCH.h
 * @ingroup nxdn_lc
 * @file RTCH.cpp
 * @ingroup nxdn_lc
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
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents link control data for traffic channel NXDN calls.
         * @ingroup nxdn_lc
         */
        class HOST_SW_API RTCH {
        public:
            /**
             * @brief Initializes a new instance of the RTCH class.
             */
            RTCH();
            /**
             * @brief Initializes a copy instance of the RTCH class.
             * @param data Instance of RTCH to copy.
             */
            RTCH(const RTCH& data);
            /**
             * @brief Finalizes a instance of the RTCH class.
             */
            ~RTCH();

            /**
             * @brief Equals operator.
             * @param data Instance of RTCH to copy.
             */
            RTCH& operator=(const RTCH& data);

            /**
             * @brief Decode RTCH data.
             * @param[in] data Buffer containing a RTCH to decode.
             * @param length Length of data buffer.
             * @param offset Offset for RTCH in data buffer.
             */
            void decode(const uint8_t* data, uint32_t length, uint32_t offset = 0U);
            /**
             * @brief Encode RTCH data.
             * @param[out] data Buffer to encode a RTCH.
             * @param length Length of data buffer.
             * @param offset Offset for RTCH in data buffer.
             */
            void encode(uint8_t* data, uint32_t length, uint32_t offset = 0U);

            /**
             * @brief Helper to reset data values to defaults.
             */
            void reset();

            /**
             * @brief Sets the flag indicating verbose log output.
             * @param verbose Flag indicating verbose log output.
             */
            static void setVerbose(bool verbose) { m_verbose = verbose; }

        public:
            /** @name Common Data */
            /**
             * @brief Message Type
             */
            __PROPERTY(uint8_t, messageType, MessageType);

            /**
             * @brief Call Type
             */
            __PROPERTY(uint8_t, callType, CallType);

            /**
             * @brief Source ID.
             */
            __PROPERTY(uint16_t, srcId, SrcId);
            /**
             * @brief Destination ID.
             */
            __PROPERTY(uint16_t, dstId, DstId);
            /** @} */

            /** @name Common Call Options */
            /**
             * @brief Flag indicating the emergency bits are set.
             */
            __PROPERTY(bool, emergency, Emergency);
            /**
             * @brief Flag indicating that encryption is enabled.
             */
            __PROPERTY(bool, encrypted, Encrypted);
            /**
             * @brief Flag indicating priority paging.
             */
            __PROPERTY(bool, priority, Priority);
            /**
             * @brief Flag indicating a group/talkgroup operation.
             */
            __PROPERTY(bool, group, Group);
            /**
             * @brief Flag indicating a half/full duplex operation.
             */
            __PROPERTY(bool, duplex, Duplex);

            /**
             * @brief Transmission mode.
             */
            __PROPERTY(uint8_t, transmissionMode, TransmissionMode);
            /** @} */

            /** @name Data Call Data */
            /**
             * @brief Data packet information.
             */
            __PROPERTY(PacketInformation, packetInfo, PacketInfo);
            /**
             * @brief Data packet information.
             */
            __PROPERTY(PacketInformation, rsp, Response);
            /**
             * @brief Data packet frame number.
             */
            __PROPERTY(uint8_t, dataFrameNumber, DataFrameNumber);
            /**
             * @brief Data packet block number.
             */
            __PROPERTY(uint8_t, dataBlockNumber, DataBlockNumber);
            /** @} */

            /** @name Header Delay Data */
            /**
             * @brief Delay count.
             */
            __PROPERTY(uint16_t, delayCount, DelayCount);
            /** @} */

            /** @name Encryption data */
            /**
             * @brief Encryption algorithm ID.
             */
            __PROPERTY(uint8_t, algId, AlgId);
            /**
             * @brief Encryption key ID.
             */
            __PROPERTY(uint8_t, kId, KId);
            /** @} */

            /**
             * @brief Cause Response.
             */
            __PROPERTY(uint8_t, causeRsp, CauseResponse);

        private:
            static bool m_verbose;

            // Encryption data
            uint8_t* m_mi;

            /**
             * @brief Internal helper to decode a RTCH link control message.
             * @param[in] data Buffer containing a link control to decode.
             */
            bool decodeLC(const uint8_t* data);
            /**
             * @brief Internal helper to encode a RTCH link control message.
             * @param[out] data Buffer to encode a link control.
             */
            void encodeLC(uint8_t* data);

            /**
             * @brief Internal helper to copy the class.
             * @param data Instance of RTCH to copy.
             */
            void copy(const RTCH& data);
        };
    } // namespace lc
} // namespace nxdn

#endif // __NXDN_LC__RTCH_H__
