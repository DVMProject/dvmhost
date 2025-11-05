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
 * @file KMMUnableToDecrypt.h
 * @ingroup p25_kmm
 * @file KMMUnableToDecrypt.cpp
 * @ingroup p25_kmm
 */
#if !defined(__P25_KMM__KMM_UNABLE_TO_DECRYPT_H__)
#define  __P25_KMM__KMM_UNABLE_TO_DECRYPT_H__

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

         const uint32_t KMM_BODY_UNABLE_TO_DECRYPT_LENGTH = 7U;

         /** @} */
 
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        class HOST_SW_API KMMUnableToDecrypt : public KMMFrame {
        public:
            /**
             * @brief Initializes a new instance of the KMMUnableToDecrypt class.
             */
            KMMUnableToDecrypt();
            /**
             * @brief Finalizes a instance of the KMMUnableToDecrypt class.
             */
            ~KMMUnableToDecrypt();

            /**
             * @brief Gets the byte length of this KMMFrame.
             * @return uint32_t Length of KMMFrame.
             */
            uint32_t length() const override;

            /**
             * @brief Decode a KMM Unable-To-Decrypt.
             * @param[in] data Buffer containing KMM frame data to decode.
             * @returns bool True, if decoded, otherwise false.
             */
            bool decode(const uint8_t* data) override;
            /**
             * @brief Encode a KMM Unable-To-Decrypt.
             * @param[out] data Buffer to encode KMM frame data to.
             */
            void encode(uint8_t* data) override;

            /**
             * @brief Returns a string that represents the current KMM frame.
             * @returns std::string String representation of the KMM frame.
             */
            std::string toString() override;

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
            DECLARE_PROPERTY(uint8_t, bodyFormat, BodyFormat);
            /**
             * @brief Encryption algorithm ID.
             */
            DECLARE_PROPERTY(uint8_t, algId, AlgId);
            /**
             * @brief Encryption key ID.
             */
            DECLARE_PROPERTY(uint16_t, kId, KId);
            /**
             * @brief 
             */
            DECLARE_PROPERTY(uint8_t, status, Status);

            /**
             * @brief 
             */
            DECLARE_PROPERTY(uint8_t, decryptInfoFmt, DecryptInfoFmt);
            /**
             * @brief Encryption algorithm ID.
             */
            DECLARE_PROPERTY(uint8_t, decryptAlgId, DecryptAlgId);
            /**
             * @brief Encryption key ID.
             */
            DECLARE_PROPERTY(uint16_t, decryptKId, DecryptKId);

            /**
             * @brief List of keys.
             */
            DECLARE_PROPERTY(KeyItem, key, Key);

            DECLARE_COPY(KMMUnableToDecrypt);

        private:
            // Encryption data
            bool m_miSet;
            uint8_t* m_mi;
        };
    } // namespace kmm
} // namespace p25

#endif // __P25_KMM__KMM_UNABLE_TO_DECRYPT_H__
