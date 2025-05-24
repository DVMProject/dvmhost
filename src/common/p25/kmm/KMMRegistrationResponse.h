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
 * @file KMMRegistrationResponse.h
 * @ingroup p25_kmm
 * @file KMMRegistrationResponse.cpp
 * @ingroup p25_kmm
 */
#if !defined(__P25_KMM__KMM_REGISTRATION_RSP_H__)
#define  __P25_KMM__KMM_REGISTRATION_RSP_H__

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

         const uint32_t KMM_REGISTRATION_RSP_LENGTH = KMM_FRAME_LENGTH + 1U;

         /** @} */
 
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        class HOST_SW_API KMMRegistrationResponse : public KMMFrame {
        public:
            /**
             * @brief Initializes a new instance of the KMMRegistrationResponse class.
             */
            KMMRegistrationResponse();
            /**
             * @brief Finalizes a instance of the KMMRegistrationResponse class.
             */
            ~KMMRegistrationResponse();

            /**
             * @brief Decode a KMM deregistration response.
             * @param[in] data Buffer containing KMM frame data to decode.
             * @returns bool True, if decoded, otherwise false.
             */
            bool decode(const uint8_t* data) override;
            /**
             * @brief Encode a KMM deregistration response.
             * @param[out] data Buffer to encode KMM frame data to.
             */
            void encode(uint8_t* data) override;

        public:
            /**
             * @brief 
             */
            DECLARE_PROPERTY(uint8_t, status, Status);

            DECLARE_COPY(KMMRegistrationResponse);
        };
    } // namespace kmm
} // namespace p25

#endif // __P25_KMM__KMM_REGISTRATION_RSP_H__
