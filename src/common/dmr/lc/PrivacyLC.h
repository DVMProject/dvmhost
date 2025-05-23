// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*  Copyright (C) 2021,2025 Bryan Biedenkapp, N2PLL
*
*/
/**
 * @file PrivacyLC.h
 * @ingroup dmr_lc
 * @file PrivacyLC.cpp
 * @ingroup dmr_lc
 */
#if !defined(__DMR_LC__PRIVACY_LC_H__)
#define __DMR_LC__PRIVACY_LC_H__

#include "common/Defines.h"

namespace dmr
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents DMR privacy indicator link control data.
         * @ingroup dmr_lc
         */
        class HOST_SW_API PrivacyLC {
        public:
            /**
             * @brief Initializes a new instance of the PrivacyLC class.
             * @param data Buffer containing PrivacyLC data.
             */
            PrivacyLC(const uint8_t* data);
            /**
             * @brief Initializes a new instance of the PrivacyLC class.
             * @param bits Boolean bit buffer containing PrivacyLC data.
             */
            PrivacyLC(const bool* bits);
            /**
             * @brief Initializes a new instance of the PrivacyLC class.
             */
            PrivacyLC();
            /**
             * @brief Finalizes a instance of the PrivacyLC class.
             */
            ~PrivacyLC();

            /**
             * @brief Gets LC data as bytes.
             * @param[out] data Buffer containing LC data.
             */
            void getData(uint8_t* data) const;
            /**
             * @brief Gets LC data as bits.
             * @param[out] bits Boolean bit buffer containing LC data.
             */
            void getData(bool* bits) const;

            /** @name Encryption data */
            /**
             * @brief Sets the encryption message indicator.
             * @param[in] mi Buffer containing the 9-byte Message Indicator.
             */
            void setMI(const uint8_t* mi);
            /**
             * @brief Gets the encryption message indicator.
             * @param[out] mi Buffer containing the 9-byte Message Indicator.
             */
            void getMI(uint8_t* mi) const;
            /** @} */

        public:
            /** @name Common Data */
            /**
             * @brief Feature ID
             */
            __PROPERTY(uint8_t, FID, FID);

            /**
             * @brief Destination ID.
             */
            __PROPERTY(uint32_t, dstId, DstId);
            /** @} */

            /** @name Service Options */
            /**
             * @brief Flag indicating a group/talkgroup operation.
             */
            __PROPERTY(bool, group, Group);
            /** @} */

            /** @name Encryption data */
            /**
             * @brief Encryption algorithm ID.
             */
            __PROPERTY(uint8_t, algId, AlgId);
            /**
             * @brief Encryption key ID.
             */
            __PROPERTY(uint32_t, kId, KId);
            /** @} */

        private:
            // Encryption data
            uint8_t* m_mi;
        };
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC__PRIVACY_LC_H__
