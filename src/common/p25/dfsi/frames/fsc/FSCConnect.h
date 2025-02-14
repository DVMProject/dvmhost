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
 * @file FSCConnect.h
 * @ingroup dfsi_fsc_frames
 * @file FSCConnect.cpp
 * @ingroup dfsi_fsc_frames
 */
#if !defined(__FSC_CONNECT_H__)
#define __FSC_CONNECT_H__

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
                 * @brief Implements the FSC Connect Message.
                 * @ingroup dfsi_fsc_frames
                 */
                class HOST_SW_API FSCConnect : public FSCMessage {
                public:
                    static const uint8_t LENGTH = 11U;

                    /**
                     * @brief Initializes a copy instance of the FSCConnect class.
                     */
                    FSCConnect();

                    /**
                     * @brief Decode a FSC connect frame.
                     * @param[in] data Buffer to containing FSCConnect to decode.
                     */
                    bool decode(const uint8_t* data) override;
                    /**
                     * @brief Encode a FSC connect frame.
                     * @param[out] data Buffer to encode a FSCConnect.
                     */
                    void encode(uint8_t* data) override;

                public:
                    /**
                     * @brief Voice Conveyance RTP Port.
                     */
                    __PROPERTY(uint16_t, vcBasePort, VCBasePort);
                    /**
                     * @brief SSRC Identifier for all RTP transmissions.
                     */
                    __PROPERTY(uint32_t, vcSSRC, VCSSRC);
                    /**
                     * @brief Fixed Station Heartbeat Period.
                     */
                    __PROPERTY(uint8_t, fsHeartbeatPeriod, FSHeartbeatPeriod);
                    /**
                     * @brief Host Heartbeat Period.
                     */
                    __PROPERTY(uint8_t, hostHeartbeatPeriod, HostHeartbeatPeriod);
                };
            } // namespace fsc
        } // namespace frames
    } // namespace dfsi
} // namespace p25

#endif // __FSC_CONNECT_H__