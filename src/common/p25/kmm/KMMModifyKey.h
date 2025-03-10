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
 * @file KMMModifyKey.h
 * @ingroup p25_kmm
 * @file KMMModifyKey.cpp
 * @ingroup p25_kmm
 */
#if !defined(__P25_KMM__KMM_MODIFY_KEY_H__)
#define  __P25_KMM__KMM_MODIFY_KEY_H__

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
        //  Constants
        // ---------------------------------------------------------------------------

        /**
         * @addtogroup p25_kmm
         * @{
         */

         const uint32_t KMM_MODIFY_KEY_LENGTH = KMM_FRAME_LENGTH + 4U;

         /** @} */
 
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        class HOST_SW_API KMMModifyKey : public KMMFrame {
        public:
            /**
             * @brief Initializes a new instance of the KMMModifyKey class.
             */
            KMMModifyKey();
            /**
             * @brief Finalizes a instance of the KMMModifyKey class.
             */
            ~KMMModifyKey();

            /**
             * @brief Gets the byte length of this KMMFrame.
             * @return uint32_t Length of KMMFrame.
             */
            uint32_t length() const override;

            /**
             * @brief Decode a KMM modify key packet.
             * @param[in] data Buffer containing KMM packet data to decode.
             * @returns bool True, if decoded, otherwise false.
             */
            bool decode(const uint8_t* data);
            /**
             * @brief Encode a KMM modify key packet.
             * @param[out] data Buffer to encode KMM  packet data to.
             */
            void encode(uint8_t* data);

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
            /**
             * @brief 
             */
            __PROPERTY(uint8_t, decryptInfoFmt, DecryptInfoFmt);
            /**
             * @brief Encryption algorithm ID.
             */
            __PROPERTY(uint8_t, algId, AlgId);
            /**
             * @brief Encryption key ID.
             */
            __PROPERTY(uint32_t, kId, KId);

            /**
             * @brief 
             */
            __PROPERTY(KeysetItem, keysetItem, KeysetItem);

            __COPY(KMMModifyKey);

        private:
            // Encryption data
            bool m_miSet;
            uint8_t* m_mi;
        };
    } // namespace kmm
} // namespace p25

#endif // __P25_KMM__KMM_MODIFY_KEY_H__
