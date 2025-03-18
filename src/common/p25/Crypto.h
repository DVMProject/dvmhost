// SPDX-License-Identifier: MIT
/*
 * Digital Voice Modem - Common Library
 * MIT Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup p25_crypto Project 25 Cryptography
 * @brief Defines and implements cryptography routines for P25.
 * @ingroup p25
 * 
 * @file Crypto.h
 * @ingroup p25
 * @file Crypto.cpp
 * @ingroup p25
 */
#if !defined(__P25_CRYPTO_H__)
#define __P25_CRYPTO_H__

#include "common/Defines.h"
#include "common/p25/P25Defines.h"
#include "common/Utils.h"

#include <random>

namespace p25
{
    namespace crypto
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Project 25 Cryptography.
         * @ingroup p25_crypto
         */
        class HOST_SW_API P25Crypto {
        public:
            /**
             * @brief Initializes a new instance of the P25Crypto class.
             */
            P25Crypto();
            /**
             * @brief Finalizes a instance of the P25Crypto class.
             */
            ~P25Crypto();

            /**
             * @brief Helper given to generate a new initial seed MI.
             * @param mi 
             */
            void generateMI();
            /**
             * @brief Helper given the last MI, generate the next MI using LFSR.
             */
            void generateNextMI();

            /**
             * @brief Helper to check if there is a valid encryption keystream.
             * @return bool True, if there is a valid keystream, otherwise false.
             */
            bool hasValidKeystream();
            /**
             * @brief Helper to generate the encryption keystream.
             */
            void generateKeystream();
            /**
             * @brief Helper to reset the encryption keystream.
             */
            void resetKeystream();

            /**
             * @brief Helper to crypt P25 IMBE audio using AES-256.
             * @param imbe Buffer containing IMBE to crypt.
             * @param duid P25 DUID.
             */
            void cryptAES_IMBE(uint8_t* imbe, P25DEF::DUID::E duid);
            /**
             * @brief Helper to crypt P25 IMBE audio using ARC4.
             * @param imbe Buffer containing IMBE to crypt.
             * @param duid P25 DUID.
             */
            void cryptARC4_IMBE(uint8_t* imbe, P25DEF::DUID::E duid);

            /**
             * @brief Helper to check if there is a valid encryption message indicator.
             * @return bool True, if there is a valid encryption message indicator, otherwise false.
             */
            bool hasValidMI();
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
            /**
             * @brief Clears the encryption message indicator.
             */
            void clearMI();

            /**
             * @brief Sets the encryption key.
             * @param[in] mi Buffer containing the encryption key.
             * @param[in] len Length of the key.
             */
            void setKey(const uint8_t* key, uint8_t len);
            /**
             * @brief Gets the encryption key,
             * @param[out] mi Buffer containing the encryption key.
             */
            void getKey(uint8_t* key) const;
            /**
             * @brief Clears the stored encryption key.
             */
            void clearKey();

        public:
            /**
             * @brief Traffic Encryption Key Algorithm ID.
             */
            __PROPERTY(uint8_t, tekAlgoId, TEKAlgoId);
            /**
             * @brief Traffic Encryption Key ID.
             */
            __PROPERTY(uint16_t, tekKeyId, TEKKeyId);

            /**
             * @brief Traffic Encryption Key Length.
             */
            __READONLY_PROPERTY(uint8_t, tekLength, TEKLength);

        private:
            uint8_t* m_keystream;
            uint32_t m_keystreamPos;
    
            uint8_t* m_mi;

            UInt8Array m_tek;

            std::mt19937 m_random;

            /**
             * @brief 
             * @param lfsr 
             * @return uint64_t
             */
            uint64_t stepLFSR(uint64_t& lfsr);
            /**
             * @brief Expands the 9-byte MI into a proper 16-byte IV.
             * @return uint8_t* Buffer containing expanded 16-byte IV.
             */
            uint8_t* expandMIToIV();
        };
    } // namespace crypto
} // namespace p25

#endif // __P25_CRYPTO_H__
