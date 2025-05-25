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
 * @defgroup p25_kmm Key Management Message
 * @brief Implementation for the data handling of the TIA-102.AACA Project 25 standard.
 * @ingroup p25
 * 
 * @file KMMFrame.h
 * @ingroup p25_kmm
 * @file KMMFrame.cpp
 * @ingroup p25_kmm
 */
#if !defined(__P25_KMM__KMM_FRAME_H__)
#define  __P25_KMM__KMM_FRAME_H__

#include "common/Defines.h"
#include "common/Utils.h"

#include <string>

namespace p25
{
    namespace kmm
    {
        // ---------------------------------------------------------------------------
        //  Constants
        // ---------------------------------------------------------------------------

        /**
         * @addtogroup p25_kmm
         * @{
         */

        const uint32_t KMM_FRAME_LENGTH = 9U;

        /** @} */

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents a KMM frame packet header.
         * @ingroup p25_kmm
         */
        class HOST_SW_API KMMFrame {
        public:
            /**
             * @brief Initializes a copy instance of the KMMFrame class.
             * @param data Instance of KMMFrame to copy/
             */
            KMMFrame(const KMMFrame& data);
            /**
             * @brief Initializes a new instance of the KMMFrame class.
             */
            KMMFrame();
            /**
             * @brief Finalizes a instance of the KMMFrame class.
             */
            ~KMMFrame();

            /**
             * @brief Gets the byte length of this KMMFrame.
             * @return uint32_t Length of KMMFrame.
             */
            virtual uint32_t length() const { return KMM_FRAME_LENGTH; }

            /**
             * @brief Decode a KMM frame.
             * @param[in] data Buffer containing KMM frame data to decode.
             * @returns bool True, if decoded, otherwise false.
             */
            virtual bool decode(const uint8_t* data) = 0;
            /**
             * @brief Encode a KMM frame.
             * @param[out] data Buffer to encode KMM frame data to.
             */
            virtual void encode(uint8_t* data) = 0;

        public:
            // Common Data
            /**
             * @brief KMM Message ID.
             */
            DECLARE_PROTECTED_PROPERTY(uint8_t, messageId, MessageId);
            /**
             * @brief Message Length.
             */
            DECLARE_PROTECTED_PROPERTY(uint16_t, messageLength, MessageLength);
            
            /**
             * @brief Response Kind.
             */
            DECLARE_PROTECTED_PROPERTY(uint8_t, respKind, ResponseKind);

            /**
             * @brief Destination Logical link ID.
             */
            DECLARE_PROTECTED_PROPERTY(uint32_t, dstLlId, DstLLId);
            /**
             * @brief Source Logical link ID.
             */
            DECLARE_PROTECTED_PROPERTY(uint32_t, srcLlId, SrcLLId);

            /**
             * @brief Flag indicating the KMM frame is complete.
             */
            DECLARE_PROTECTED_PROPERTY(bool, complete, Complete);

        protected:
            uint8_t m_mfMessageNumber;
            uint8_t m_mfMac;

            /**
             * @brief Internal helper to decode a KMM header.
             * @param data Buffer containing KMM packet data to decode.
             * @returns bool True, if decoded, otherwise false.
             */
            bool decodeHeader(const uint8_t* data);
            /**
             * @brief Internal helper to encode a KMM header.
             * @param data Buffer to encode KMM packet data to.
             */
            void encodeHeader(uint8_t* data);

            DECLARE_PROTECTED_COPY(KMMFrame);
        };
    } // namespace kmm
} // namespace p25

#endif // __P25_KMM__KMM_FRAME_H__
