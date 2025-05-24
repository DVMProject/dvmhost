// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file RTPHeader.h
 * @ingroup network_core
 * @file RTPHeader.cpp
 * @ingroup network_core
 */
#if !defined(__RTP_HEADER_H__)
#define __RTP_HEADER_H__

#include "common/Defines.h"

#include <chrono>
#include <string>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define INVALID_TS UINT32_MAX

#define RTP_HEADER_LENGTH_BYTES 12
#define RTP_GENERIC_CLOCK_RATE 8000

namespace network
{
    namespace frame
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents an RTP header.
         * \code{.unparsed}
         * Byte 0               1               2               3
         * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
         *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *     |Ver|P|E| CSRC  |M| Payload Type| Sequence                      |
         *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *     | Timestamp                                                     |
         *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *     | SSRC                                                          |
         *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * 12 bytes
         * \endcode
         * @ingroup network_core
         */
        class HOST_SW_API RTPHeader {
        public:
            /**
             * @brief Initializes a new instance of the RTPHeader class.
             */
            RTPHeader();
            /**
             * @brief Finalizes a instance of the RTPHeader class.
             */
            ~RTPHeader();

            /**
             * @brief Decode a RTP header.
             * @param[in] data Buffer containing RTP header to decode.
             */
            virtual bool decode(const uint8_t* data);
            /**
             * @brief Encode a RTP header.
             * @param[out] data Buffer to encode an RTP header.
             */
            virtual void encode(uint8_t* data);

            /**
             * @brief Helper to reset the start timestamp.
             */
            static void resetStartTime();

        public:
            /**
             * @brief RTP Protocol Version.
             */
            DECLARE_RO_PROPERTY(uint8_t, version, Version);
            /**
             * @brief Flag indicating if the packet has trailing padding.
             */
            DECLARE_RO_PROPERTY(bool, padding, Padding);
            /**
             * @brief Flag indicating the presence of an extension header.
             */
            DECLARE_PROPERTY(bool, extension, Extension);
            /**
             * @brief Count of contributing source IDs that follow the SSRC.
             */
            DECLARE_RO_PROPERTY(uint8_t, cc, CSRCCount);
            /**
             * @brief Flag indicating application-specific behavior.
             */
            DECLARE_PROPERTY(bool, marker, Marker);
            /**
             * @brief Format of the payload contained within the packet.
             */
            DECLARE_PROPERTY(uint8_t, payloadType, PayloadType);
            /**
             * @brief Sequence number for the RTP packet.
             */
            DECLARE_PROPERTY(uint16_t, seq, Sequence);
            /**
             * @brief RTP packet timestamp.
             */
            DECLARE_PROPERTY(uint32_t, timestamp, Timestamp);
            /**
             * @brief Synchronization Source ID.
             */
            DECLARE_PROPERTY(uint32_t, ssrc, SSRC);
        
        private:
            static std::chrono::time_point<std::chrono::high_resolution_clock> m_wcStart;
        };
    } // namespace frame
} // namespace network

#endif // __RTP_HEADER_H__