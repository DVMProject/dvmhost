// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file BlockHeader.h
 * @ingroup dfsi_frames
 * @file BlockHeader.cpp
 * @ingroup dfsi_frames
 */
#if !defined(__BLOCK_HEADER_H__)
#define __BLOCK_HEADER_H__

#include "Defines.h"
#include "common/Defines.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "common/p25/dfsi/frames/FrameDefines.h"

namespace p25
{
    namespace dfsi
    {
        namespace frames
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief Implements a DFSI block header packet.
             * \code{.unparsed}
             * Compact Form
             * Byte 0
             * Bit  7 6 5 4 3 2 1 0
             *     +-+-+-+-+-+-+-+-+
             *     |E|      BT     |
             *     +-+-+-+-+-+-+-+-+
             * 
             * Verbose Form
             * Byte 0               1               2               3
             * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |E|      BT     |             TSO           |         BL        |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             * \endcode
             * @ingroup dfsi_frames
             */
            class HOST_SW_API BlockHeader {
            public:
                static const uint8_t LENGTH = 1;
                static const uint8_t VERBOSE_LENGTH = 4;

                /**
                 * @brief Initializes a copy instance of the BlockHeader class.
                 */
                BlockHeader();
                /**
                 * @brief Initializes a copy instance of the BlockHeader class.
                 * @param data Buffer to containing BlockHeader to decode.
                 * @param verbose Flag indicating verbose form of BlockHeader.
                 */
                BlockHeader(uint8_t* data, bool verbose = false);

                /**
                 * @brief Decode a block header frame.
                 * @param[in] data Buffer to containing BlockHeader to decode.
                 * @param verbose Flag indicating verbose form of BlockHeader.
                 */
                bool decode(const uint8_t* data, bool verbose = false);
                /**
                 * @brief Encode a block header frame.
                 * @param[out] data Buffer to encode a BlockHeader.
                 * @param verbose Flag indicating verbose form of BlockHeader.
                 */
                void encode(uint8_t *data, bool verbose = false);

            public:
                /**
                 * @brief Payload type.
                 * This simple boolean marks this header as either IANA standard, or profile specific.
                 */
                __PROPERTY(bool, payloadType, PayloadType);
                /**
                 * @brief Block type.
                 */
                __PROPERTY(BlockType::E, blockType, BlockType);
                /**
                 * @brief Timestamp Offset.
                 */
                __PROPERTY(uint16_t, timestampOffset, TimestampOffset);
                /**
                 * @brief Block length.
                 */
                __PROPERTY(uint16_t, blockLength, BlockLength);
            };
        } // namespace frames
    } // namespace dfsi
} // namespace p25

#endif // __BLOCK_HEADER_H__