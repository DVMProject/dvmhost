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
 * @file FSCACK.h
 * @ingroup dfsi_fsc_frames
 * @file FSCACK.cpp
 * @ingroup dfsi_fsc_frames
 */
#if !defined(__FSC_ACK_H__)
#define __FSC_ACK_H__

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
                 * @brief Implements the FSC Acknowledgement Message.
                 * @ingroup dfsi_fsc_frames
                 */
                class HOST_SW_API FSCACK : public FSCMessage {
                public:
                    static const uint8_t LENGTH = 7U;

                    /**
                     * @brief Initializes a copy instance of the FSCACK class.
                     */
                    FSCACK();

                    /**
                     * @brief Decode a FSC ACK frame.
                     * @param[in] data Buffer to containing FSCACK to decode.
                     */
                    bool decode(const uint8_t* data) override;
                    /**
                     * @brief Encode a FSC ACK frame.
                     * @param[out] data Buffer to encode a FSCACK.
                     */
                    void encode(uint8_t* data) override;
                
                public:
                    uint8_t* responseData; // ?? - this should probably be private with getters/setters

                    /**
                     * @brief Acknowledged Message ID.
                     */
                    DECLARE_PROPERTY(FSCMessageType::E, ackMessageId, AckMessageId);
                    /**
                     * @brief Acknowledged Message Version.
                     */
                    DECLARE_RO_PROPERTY(uint8_t, ackVersion, AckVersion);
                    /**
                     * @brief 
                     */
                    DECLARE_PROPERTY(uint8_t, ackCorrelationTag, AckCorrelationTag);
                    /**
                     * @brief Response code.
                     */
                    DECLARE_PROPERTY(FSCAckResponseCode::E, responseCode, ResponseCode);
                    /**
                     * @brief Response Data Length.
                     */
                    DECLARE_PROPERTY(uint8_t, respLength, ResponseLength);
                };
            } // namespace fsc
        } // namespace frames
    } // namespace dfsi
} // namespace p25

#endif // __FSC_ACK_H__