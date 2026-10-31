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
 * @file KMMWarmStartCommand.h
 * @ingroup p25_kmm
 * @file KMMWarmStartCommand.cpp
 * @ingroup p25_kmm
 */
#if !defined(__P25_KMM__KMM_WARM_START_COMMAND_H__)
#define __P25_KMM__KMM_WARM_START_COMMAND_H__

#include "common/Defines.h"
#include "common/p25/kmm/KMMFrame.h"
#include "common/p25/kmm/KeysetItem.h"

namespace p25
{
    namespace kmm
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents a KMM Warm Start Command frame.
         * @ingroup p25_kmm
         */
        class DVM_COMMON_API KMMWarmStartCommand : public KMMFrame {
        public:
            /**
             * @brief Initializes a new instance of the KMMWarmStartCommand class.
             */
            KMMWarmStartCommand();
            /**
             * @brief Finalizes a instance of the KMMWarmStartCommand class.
             */
            ~KMMWarmStartCommand() = default;


            /**
             * @brief Gets the byte length of this KMMFrame.
             * @return uint32_t Length of KMMFrame.
             */
            uint32_t length() const override;

            /**
             * @brief Decode a KMM Warm-Start.
             * @param[in] data Buffer containing KMM frame data to decode.
             * @returns bool True, if decoded, otherwise false.
             */
            bool decode(const uint8_t* data) override;
            /**
             * @brief Encode a KMM Warm-Start.
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
             * @brief Gets or sets the decryption information format.
             */
            DECLARE_PROPERTY(uint8_t, decryptInfoFmt, DecryptInfoFmt);

            /**
             * @brief Gets or sets the KEK algorithm identifier.
             */
            DECLARE_PROPERTY(uint8_t, kekAlgId, KEKAlgId);
            /**
             * @brief Gets or sets the KEK key identifier.
             */
            DECLARE_PROPERTY(uint16_t, kekKId, KEKKId);
            /**
             * @brief Gets or sets the key length.
             */
            DECLARE_PROPERTY(uint8_t, keyLength, KeyLength);
            /**
             * @brief Gets or sets the TEK algorithm identifier.
             */
            DECLARE_PROPERTY(uint8_t, tekAlgId, TEKAlgId);
            /**
             * @brief Gets or sets the key item.
             */
            DECLARE_PROPERTY(KeyItem, key, Key);

            DECLARE_COPY(KMMWarmStartCommand);

        private:
            uint8_t m_mi[P25DEF::MI_LENGTH_BYTES];
        };
    } // namespace kmm
} // namespace p25

#endif // __P25_KMM__KMM_WARM_START_COMMAND_H__
