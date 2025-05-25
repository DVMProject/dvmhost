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
 * @file KMMNegativeAck.h
 * @ingroup p25_kmm
 * @file KMMNegativeAck.cpp
 * @ingroup p25_kmm
 */
#if !defined(__P25_KMM__KMM_NEGATIVE_ACK_H__)
#define  __P25_KMM__KMM_NEGATIVE_ACK_H__

#include "common/Defines.h"
#include "common/p25/kmm/KMMFrame.h"
#include "common/Utils.h"

#include <string>
#include <vector>

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

         const uint32_t KMM_NEGATIVE_ACK_LENGTH = KMM_FRAME_LENGTH + 4U;

         /** @} */
 
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        class HOST_SW_API KMMNegativeAck : public KMMFrame {
        public:
            /**
             * @brief Initializes a new instance of the KMMNegativeAck class.
             */
            KMMNegativeAck();
            /**
             * @brief Finalizes a instance of the KMMNegativeAck class.
             */
            ~KMMNegativeAck();

            /**
             * @brief Decode a KMM NAK.
             * @param[in] data Buffer containing KMM frame data to decode.
             * @returns bool True, if decoded, otherwise false.
             */
            bool decode(const uint8_t* data) override;
            /**
             * @brief Encode a KMM NAK.
             * @param[out] data Buffer to encode KMM frame data to.
             */
            void encode(uint8_t* data) override;

        public:
            /**
             * @brief 
             */
            DECLARE_PROPERTY(uint8_t, messageId, MessageId);
            /**
             * @brief 
             */
            DECLARE_PROPERTY(uint16_t, messageNo, MessageNumber);
            /**
             * @brief 
             */
            DECLARE_PROPERTY(uint8_t, status, Status);

            DECLARE_COPY(KMMNegativeAck);
        };
    } // namespace kmm
} // namespace p25

#endif // __P25_KMM__KMM_NEGATIVE_ACK_H__
