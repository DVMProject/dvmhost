// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file RPCHeader.h
 * @ingroup network_core
 * @file RPCHeader.cpp
 * @ingroup network_core
 */
#if !defined(__RPC_HEADER_H__)
#define __RPC_HEADER_H__

#include "common/Defines.h"

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define RPC_HEADER_LENGTH_BYTES 8
#define RPC_REPLY_FUNC 0x8000U

namespace network
{
    namespace frame
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents the RPC header.
         * \code{.unparsed}
         * Byte 0               1               2               3
         * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
         *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *     | Payload CRC-16                | Function                      |
         *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *     | Message Length                                                |
         *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * 8 bytes
         * \endcode
         */
        class HOST_SW_API RPCHeader {
        public:
            /**
             * @brief Initializes a new instance of the RPCHeader class.
             */
            RPCHeader();
            /**
             * @brief Finalizes a instance of the RPCHeader class.
             */
            ~RPCHeader();

            /**
             * @brief Decode a RPC header.
             * @param[in] data Buffer containing RPC header to decode.
             */
            bool decode(const uint8_t* data);
            /**
             * @brief Encode a RPC header.
             * @param[out] data Buffer to encode an RPC header.
             */
            void encode(uint8_t* data);

        public:
            /**
             * @brief Payload packet CRC-16.
             */
            __PROPERTY(uint16_t, crc16, CRC);
            /**
             * @brief Function.
             */
            __PROPERTY(uint16_t, func, Function);
            /**
             * @brief Message Length.
             */
            __PROPERTY(uint32_t, messageLength, MessageLength);
        };
    } // namespace frame
} // namespace network

#endif // __RPC_HEADER_H__
