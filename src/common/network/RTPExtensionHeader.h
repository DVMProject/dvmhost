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
 * @file RTPExtensionHeader.h
 * @ingroup network_core
 * @file RTPExtensionHeader.cpp
 * @ingroup network_core
 */
#if !defined(__RTP_EXTENSION_HEADER_H__)
#define __RTP_EXTENSION_HEADER_H__

#include "common/Defines.h"

#include <chrono>
#include <random>
#include <string>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define RTP_EXTENSION_HEADER_LENGTH_BYTES 4

namespace network
{
    namespace frame
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents an RTP Extension header.
         * \code{.unparsed}
         * Byte 0               1               2               3
         * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
         *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *     | Payload Type                  | Payload Length                |
         *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * \endcode
         */
        class HOST_SW_API RTPExtensionHeader {
        public:
            /**
             * @brief Initializes a new instance of the RTPExtensionHeader class.
             */
            RTPExtensionHeader();
            /**
             * @brief Finalizes a instance of the RTPExtensionHeader class.
             */
            ~RTPExtensionHeader();

            /**
             * @brief Decode a RTP header.
             * @param[in] data Buffer containing RTP extension header to decode.
             */
            virtual bool decode(const uint8_t* data);
            /**
             * @brief Encode a RTP header.
             * @param[out] data Buffer to encode an RTP extension header.
             */
            virtual void encode(uint8_t* data);

        public:
            /**
             * @brief Format of the extension header payload contained within the packet.
             */
            __PROTECTED_PROPERTY(uint16_t, payloadType, PayloadType);
            /**
             * @brief Length of the extension header payload (in 32-bit units).
             */
            __PROTECTED_PROPERTY(uint16_t, payloadLength, PayloadLength);
        };
    } // namespace frame
} // namespace network

#endif // __RTP_EXTENSION_HEADER_H__