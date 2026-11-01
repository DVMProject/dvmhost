// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file KMMChangeover.h
 * @ingroup p25_kmm
 * @file KMMChangeover.cpp
 * @ingroup p25_kmm
 */
#if !defined(__P25_KMM__KMM_CHANGEOVER_H__)
#define  __P25_KMM__KMM_CHANGEOVER_H__

#include "common/Defines.h"
#include "common/p25/kmm/KMMFrame.h"
#include "common/p25/kmm/KeysetItem.h"
#include "common/Utils.h"

#include <string>
#include <vector>

namespace p25
{
    namespace kmm
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents a KMM changeover command/response.
         * @ingroup p25_kmm
         */
        class DVM_COMMON_API KMMChangeover : public KMMFrame {
        public:
            /**
             * @brief Initializes a new instance of the KMMChangeover class.
             */
            KMMChangeover();
            /**
             * @brief Finalizes a instance of the KMMChangeover class.
             */
            ~KMMChangeover();

            /**
             * @brief Gets the byte length of this KMMFrame.
             * @return uint32_t Length of KMMFrame.
             */
            uint32_t length() const override;

            /**
             * @brief Decode a KMM changeover command.
             * @param[in] data Buffer containing KMM frame data to decode.
             * @returns bool True, if decoded, otherwise false.
             */
            bool decode(const uint8_t* data) override;
            /**
             * @brief Encode a KMM changeover command.
             * @param[out] data Buffer to encode KMM frame data to.
             */
            void encode(uint8_t* data) override;

            /**
             * @brief Returns a string that represents the current KMM frame.
             * @returns std::string String representation of the KMM frame.
             */
            std::string toString() override;

        public:
            /**
             * @brief Superseded keyset ID.
             */
            DECLARE_PROPERTY(uint8_t, supersededKeysetId, SupersededKeysetId);
            /**
             * @brief Active keyset ID.
             */
            DECLARE_PROPERTY(uint8_t, activeKeysetId, ActiveKeysetId);

            DECLARE_COPY(KMMChangeover);
        };
    } // namespace kmm
} // namespace p25

#endif // __P25_KMM__KMM_CHANGEOVER_H__
