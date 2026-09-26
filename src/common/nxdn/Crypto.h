// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 C. Lovell, Dev_Ranger
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup nxdn_crypto NXDN Cryptography
 * @brief Defines and implements cryptography routines for NXDN.
 * @ingroup nxdn
 * 
 * @file Crypto.h
 * @ingroup nxdn
 * @file Crypto.cpp
 * @ingroup nxdn
 */
#if !defined(__NXDN_CRYPTO_H__)
#define __NXDN_CRYPTO_H__

#include "common/Defines.h"
#include "common/nxdn/NXDNDefines.h"

#include <random>

namespace nxdn
{
    namespace crypto
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Generates and applies NXDN voice keystreams.
         * @ingroup nxdn_crypto
         */
        class DVM_COMMON_API NXDNCrypto {
        public:
            /**
             * @brief Initializes a new instance of the NXDNCrypto class.
             */
            NXDNCrypto();
            /**
             * @brief Finalizes a instance of the NXDNCrypto class.
             */
            ~NXDNCrypto();

            NXDNCrypto(const NXDNCrypto&) = delete;
            NXDNCrypto& operator=(const NXDNCrypto&) = delete;

            /**
             * @brief Helper to generate a new initial seed MI.
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
            bool hasValidKeystream() const;
            /**
             * @brief Helper to generate the encryption keystream.
             */
            void generateKeystream();
            /**
             * @brief Helper to reset the encryption keystream.
             */
            void resetKeystream();

            /**
             * @brief Helper to crypt an AMBE word using the EHR algorithm.
             * @param ambe Buffer containing the AMBE word to crypt.
             * @param n Index of the AMBE word within the session.
             */
            void cryptEHR_AMBE(uint8_t* ambe, uint8_t n) const;
            /**
             * @brief Helper to crypt an AMBE word using the DES algorithm.
             * @param ambe Buffer containing the AMBE word to crypt.
             * @param n Index of the AMBE word within the session.
             */
            void cryptDES_AMBE(uint8_t* ambe, uint8_t n) const;
            /**
             * @brief Helper to crypt an AMBE word using the AES algorithm.
             * @param ambe Buffer containing the AMBE word to crypt.
             * @param n Index of the AMBE word within the session.
             */
            void cryptAES_AMBE(uint8_t* ambe, uint8_t n) const;

            /**
             * @brief Helper to check if there is a valid encryption message indicator.
             * @return bool True, if there is a valid encryption message indicator, otherwise false.
             */
            bool hasValidMI() const;
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
             * @param[in] key Buffer containing the encryption key.
             * @param[in] len Length of the key.
             */
            void setKey(const uint8_t* key, uint8_t len);
            /**
             * @brief Gets the encryption key,
             * @param[out] key Buffer containing the encryption key.
             */
            void getKey(uint8_t* key) const;
            /**
             * @brief Clears the stored encryption key.
             */
            void clearKey();

        public:
            /**
             * @brief Traffic Cipher Type.
             */
            DECLARE_PROPERTY(uint8_t, tekCipherType, TEKCipherType);
            /**
             * @brief Traffic Encryption Key ID.
             */
            DECLARE_PROPERTY(uint8_t, tekKeyId, TEKKeyId);

            /**
             * @brief Traffic Encryption Key Length.
             */
            DECLARE_RO_PROPERTY(uint8_t, tekLength, TEKKeyLength);

        private:
            uint8_t* m_keystream;
            
            uint8_t* m_mi;
            
            UInt8Array m_tek;
            
            std::mt19937 m_random;

            /**
             * @brief Helper to crypt AMBE audio.
             * @param ambe Buffer containing AMBE to crypt.
             * @param wordIndex Index of the word within the session.
             * @param wordsPerSession Number of words per session.
             */
            void cryptAMBE(uint8_t* ambe, uint8_t wordIndex, uint8_t wordsPerSession) const;

            /**
             * @brief Helper to step the linear feedback shift register (LFSR).
             * @note This uses the polynomial: x^64 + x^62 + x^46 + x^38 + x^27 + x^15 + 1
             * @param lfsr Linear feedback shift register value.
             * @return uint64_t The next LFSR value.
             */
            static uint64_t stepLFSR(uint64_t& lfsr);

            /**
             * @brief Helper to check if a DES key is weak.
             * @param key DES key to check.
             * @returns bool True If the key is weak, otherwise false.
             */
            static bool isWeakDESKey(const uint8_t* key);
        };
    } // namespace crypto
} // namespace nxdn

#endif // __NXDN_CRYPTO_H__
