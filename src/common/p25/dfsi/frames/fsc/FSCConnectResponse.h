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
 * @file FSCConnectResponse.h
 * @ingroup dfsi_fsc_frames
 * @file FSCConnectResponse.cpp
 * @ingroup dfsi_fsc_frames
 */
#if !defined(__FSC_CONNECT_RESPONSE_H__)
#define __FSC_CONNECT_RESPONSE_H__

#include "Defines.h"
#include "common/Defines.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "common/p25/dfsi/frames/FrameDefines.h"
#include "common/p25/dfsi/frames/fsc/FSCResponse.h"

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
                 * @brief Implements the FSC Connect Response Message.
                 * @ingroup dfsi_fsc_frames
                 */
                class HOST_SW_API FSCConnectResponse : public FSCResponse {
                public:
                    static const uint8_t LENGTH = 3;

                    /**
                     * @brief Initializes a copy instance of the FSCConnectResponse class.
                     */
                    FSCConnectResponse();
                    /**
                     * @brief Initializes a copy instance of the FSCConnectResponse class.
                     * @param data Buffer to containing FSCConnectResponse to decode.
                     */
                    FSCConnectResponse(uint8_t* data);

                    /**
                     * @brief Decode a FSC connect response frame.
                     * @param[in] data Buffer to containing FSCConnectResponse to decode.
                     */
                    bool decode(const uint8_t* data) override;
                    /**
                     * @brief Encode a FSC connect response frame.
                     * @param[out] data Buffer to encode a FSCConnectResponse.
                     */
                    void encode(uint8_t* data) override;
                
                public:
                    /**
                     * @brief Voice Conveyance RTP Port.
                     */
                    __PROPERTY(uint16_t, vcBasePort, VCBasePort);
                };
            } // namespace fsc
        } // namespace frames
    } // namespace dfsi
} // namespace p25

#endif // __FSC_CONNECT_RESPONSE_H__