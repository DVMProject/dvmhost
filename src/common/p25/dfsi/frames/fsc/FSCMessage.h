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
 * @file FSCMessage.h
 * @ingroup dfsi_fsc_frames
 * @file FSCMessage.cpp
 * @ingroup dfsi_fsc_frames
 */
#if !defined(__FSC_MESSAGE_H__)
#define __FSC_MESSAGE_H__

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
            namespace fsc
            {
                // ---------------------------------------------------------------------------
                //  Class Declaration
                // ---------------------------------------------------------------------------

                /**
                 * @brief Base class FSC messages derive from.
                 * @ingroup dfsi_fsc_frames
                 */
                class HOST_SW_API FSCMessage {
                public:
                    static const uint8_t LENGTH = 3;

                    /**
                     * @brief Initializes a copy instance of the FSCMessage class.
                     */
                    FSCMessage();
                    /**
                     * @brief Initializes a copy instance of the FSCMessage class.
                     * @param data Buffer to containing FSCMessage to decode.
                     */
                    FSCMessage(const uint8_t* data);

                    /**
                     * @brief Decode a FSC message frame.
                     * @param[in] data Buffer to containing FSCMessage to decode.
                     */
                    virtual bool decode(const uint8_t* data);
                    /**
                     * @brief Encode a FSC message frame.
                     * @param[out] data Buffer to encode a FSCMessage.
                     */
                    virtual void encode(uint8_t* data);

                    /**
                     * @brief Create an instance of a FSCMessage.
                     * @param[in] data Buffer containing FSCMessage packet data to decode.
                     * @returns FSCMessage* Instance of a FSCMessage representing the decoded data.
                     */
                    static std::unique_ptr<FSCMessage> createMessage(const uint8_t* data);

                public:
                    /**
                     * @brief Message ID.
                     */
                    __PROTECTED_PROPERTY(FSCMessageType::E, messageId, MessageId);
                    /**
                     * @brief Message Version.
                     */
                    __PROTECTED_READONLY_PROPERTY(uint8_t, version, Version);
                    /**
                     * @brief 
                     */
                    __PROPERTY(uint8_t, correlationTag, CorrelationTag);
                };
            } // namespace fsc
        } // namespace frames
    } // namespace dfsi
} // namespace p25

#endif // __FSC_MESSAGE_H__
