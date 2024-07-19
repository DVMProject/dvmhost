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
 * @file FSCHeartbeat.h
 * @ingroup dfsi_fsc_frames
 * @file FSCHeartbeat.cpp
 * @ingroup dfsi_fsc_frames
 */
#if !defined(__FSC_HEARTBEAT_H__)
#define __FSC_HEARTBEAT_H__

#include "Defines.h"
#include "common/Defines.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "common/p25/dfsi/frames/FrameDefines.h"
#include "common/p25/dfsi/frames/fsc/FSCMessage.h"

namespace p25
{
    namespace dfsi
    {
        namespace frames
        {
            namespace fsc
            {
                // ---------------------------------------------------------------------------
                //  Class Declaration
                // ---------------------------------------------------------------------------

                /**
                 * @brief Implements the FSC Heartbeat Message.
                 * @ingroup dfsi_fsc_frames
                 */
                class HOST_SW_API FSCHeartbeat : public FSCMessage {
                public:
                    static const uint8_t LENGTH = 3;

                    /**
                     * @brief Initializes a copy instance of the FSCHeartbeat class.
                     */
                    FSCHeartbeat();
                    /**
                     * @brief Initializes a copy instance of the FSCHeartbeat class.
                     * @param data Buffer to containing FSCHeartbeat to decode.
                     */
                    FSCHeartbeat(uint8_t* data);
                };
            } // namespace fsc
        } // namespace frames
    } // namespace dfsi
} // namespace p25

#endif // __FSC_HEARTBEAT_H__