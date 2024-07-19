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
 * @file ControlOctet.h
 * @ingroup dfsi_frames
 * @file ControlOctet.cpp
 * @ingroup dfsi_frames
 */
#if !defined(__CONTROL_OCTET_H__)
#define __CONTROL_OCTET_H__

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
             * @brief Implements a DFSI control octet packet.
             * \code{.unparsed}
             * Byte 0
             * Bit  7 6 5 4 3 2 1 0
             *     +-+-+-+-+-+-+-+-+
             *     |S|C|   BHC     |
             *     +-+-+-+-+-+-+-+-+
             * \endcode
             * @ingroup dfsi_frames
             */
            class HOST_SW_API ControlOctet {
            public:
                static const uint8_t LENGTH = 1;

                /**
                 * @brief Initializes a copy instance of the ControlOctet class.
                 */
                ControlOctet();
                /**
                 * @brief Initializes a copy instance of the ControlOctet class.
                 * @param data Buffer to containing ControlOctet to decode.
                 */
                ControlOctet(uint8_t* data);

                /**
                 * @brief Decode a control octet frame.
                 * @param[in] data Buffer to containing ControlOctet to decode.
                 */
                bool decode(const uint8_t* data);
                /**
                 * @brief Encode a control octet frame.
                 * @param[out] data Buffer to encode a ControlOctet.
                 */
                void encode(uint8_t* data);
            
            public:
                /**
                 * @brief 
                 */
                __PROPERTY(bool, signal, Signal);
                /**
                 * @brief Indicates a compact (1) or verbose (0) block header.
                 */
                __PROPERTY(bool, compact, Compact);
                /**
                 * @brief Number of block headers following this control octet.
                 */
                __PROPERTY(uint8_t, blockHeaderCnt, BlockHeaderCnt);
            };
        } // namespace frames
    } // namespace dfsi
} // namespace p25

#endif // __CONTROL_OCTET_H__