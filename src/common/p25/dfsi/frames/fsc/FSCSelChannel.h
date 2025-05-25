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
 * @file FSCReportSelModes.h
 * @ingroup dfsi_fsc_frames
 * @file FSCReportSelModes.cpp
 * @ingroup dfsi_fsc_frames
 */
#if !defined(__FSC_SEL_CHANNEL_H__)
#define __FSC_SEL_CHANNEL_H__

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
                 * @brief Implements the FSC Select Channel Message.
                 * @ingroup dfsi_fsc_frames
                 */
                class HOST_SW_API FSCSelChannel : public FSCMessage {
                public:
                    static const uint8_t LENGTH = 3U;

                    /**
                     * @brief Initializes a copy instance of the FSCSelChannel class.
                     */
                    FSCSelChannel();

                    /**
                     * @brief Decode a FSC select channel frame.
                     * @param[in] data Buffer to containing FSCSelChannel to decode.
                     */
                    bool decode(const uint8_t* data) override;
                    /**
                     * @brief Encode a FSC select channel frame.
                     * @param[out] data Buffer to encode a FSCSelChannel.
                     */
                    void encode(uint8_t* data) override;

                public:
                    /**
                     * @brief Receive Channel Number.
                     */
                    DECLARE_PROPERTY(uint8_t, rxChan, RxChan);
                    /**
                     * @brief Transmit Channel Number.
                     */
                    DECLARE_PROPERTY(uint8_t, txChan, TxChan);
                };
            } // namespace fsc
        } // namespace frames
    } // namespace dfsi
} // namespace p25

#endif // __FSC_SEL_CHANNEL_H__