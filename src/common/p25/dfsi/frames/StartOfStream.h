// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file StartOfStream.h
 * @ingroup dfsi_frames
 * @file StartOfStream.cpp
 * @ingroup dfsi_frames
 */
#if !defined(__START_OF_STREAM_H__)
#define __START_OF_STREAM_H__

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
             * @brief Implements a P25 DFSI start of stream packet.
             * \code{.unparsed}
             * Byte 0               1               2
             * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |              NID              | Rsvd  | Err C |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             * \endcode
             * @ingroup dfsi_frames
             */
            class HOST_SW_API StartOfStream {
            public:
                static const uint8_t LENGTH = 4U;

                /**
                 * @brief Initializes a copy instance of the StartOfStream class.
                 */
                StartOfStream();
                /**
                 * @brief Initializes a copy instance of the StartOfStream class.
                 * @param data Buffer to containing StartOfStream to decode.
                 */
                StartOfStream(uint8_t* data);

                /**
                 * @brief Decode a start of stream frame.
                 * @param[in] data Buffer to containing StartOfStream to decode.
                 */
                bool decode(const uint8_t* data);
                /**
                 * @brief Encode a start of stream frame.
                 * @param[out] data Buffer to encode a StartOfStream.
                 */
                void encode(uint8_t* data);
            
            public:
                /**
                 * @brief Network Identifier.
                 */
                DECLARE_PROPERTY(uint16_t, nid, NID);
                /**
                 * @brief Error count.
                 */
                DECLARE_PROPERTY(uint8_t, errorCount, ErrorCount);
            };
        } // namespace frames
    } // namespace dfsi
} // namespace p25

#endif // __START_OF_STREAM_H__