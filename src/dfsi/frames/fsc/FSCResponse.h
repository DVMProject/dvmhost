// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - DFSI V.24/UDP Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file FSCResponse.h
 * @ingroup dfsi_fsc_frames
 * @file FSCResponse.cpp
 * @ingroup dfsi_fsc_frames
 */
#if !defined(__FSC_RESPONSE_H__)
#define __FSC_RESPONSE_H__

#include "Defines.h"
#include "common/Defines.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "frames/FrameDefines.h"

namespace p25
{
    namespace dfsi
    {
        namespace fsc
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief Base class FSC response messages derive from.
             * @ingroup dfsi_fsc_frames
             */
            class HOST_SW_API FSCResponse {
            public:
                static const uint8_t LENGTH = 1;

                /**
                 * @brief Initializes a copy instance of the FSCResponse class.
                 */
                FSCResponse();
                /**
                 * @brief Initializes a copy instance of the FSCResponse class.
                 * @param data Buffer to containing FSCResponse to decode.
                 */
                FSCResponse(uint8_t* data);

                /**
                 * @brief Decode a FSC message frame.
                 * @param[in] data Buffer to containing FSCResponse to decode.
                 */
                virtual bool decode(const uint8_t* data);
                /**
                 * @brief Encode a FSC message frame.
                 * @param[out] data Buffer to encode a FSCResponse.
                 */
                virtual void encode(uint8_t* data);
            
            public:
                /**
                 * @brief Response Version.
                 */
                __PROTECTED_READONLY_PROPERTY(uint8_t, version, Version);
            };
        } // namespace fsc
    } // namespace dfsi
} // namespace p25

#endif // __FSC_RESPONSE_H__