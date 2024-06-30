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
 * @file FSCDisconnect.h
 * @ingroup dfsi_fsc_frames
 * @file FSCDisconnect.cpp
 * @ingroup dfsi_fsc_frames
 */
#if !defined(__FSC_DISCONNECT_H__)
#define __FSC_DISCONNECT_H__

#include "Defines.h"
#include "common/Defines.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "frames/FrameDefines.h"
#include "frames/fsc/FSCMessage.h"

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
             * @brief Implements the FSC Disconnect Message.
             * @ingroup dfsi_fsc_frames
             */
            class HOST_SW_API FSCDisconnect : public FSCMessage {
            public:
                static const uint8_t LENGTH = 3;

                /**
                 * @brief Initializes a copy instance of the FSCDisconnect class.
                 */
                FSCDisconnect();
                /**
                 * @brief Initializes a copy instance of the FSCDisconnect class.
                 * @param data Buffer to containing FSCDisconnect to decode.
                 */
                FSCDisconnect(uint8_t* data);
            };
        } // namespace fsc
    } // namespace dfsi
} // namespace p25

#endif // __FSC_DISCONNECT_H__