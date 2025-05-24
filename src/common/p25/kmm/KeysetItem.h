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
 * @file KeysetItem.h
 * @ingroup p25_kmm
 */
#if !defined(__P25_KMM__KEYSET_ITEM_H__)
#define  __P25_KMM__KEYSET_ITEM_H__

#include "common/Defines.h"
#include "common/Utils.h"

#include <cassert>
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

         const uint8_t MAX_ENC_KEY_LENGTH_BYTES = 32U;

        /** @} */

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents a key set item within a KMM frame packet.
         * @ingroup p25_kmm
         */
        class HOST_SW_API KeyItem {
        public:
            /**
             * @brief Initializes a new instance of the KeyItem class.
             */
            KeyItem() :
                m_keyFormat(0x80U/*P25DEF::KEY_FORMAT_TEK*/),
                m_sln(0U),
                m_kId(0U),
                m_keyLength(0U),
                m_keyMaterial()
            {
                ::memset(m_keyMaterial, 0x00U, MAX_ENC_KEY_LENGTH_BYTES);
            }

            /**
             * @brief Equals operator. Copies this KeyItem to another KeyItem.
             * @param data Instance of KeyItem to copy.
             */
            virtual KeyItem& operator= (const KeyItem& data)
            {
                if (this != &data) {
                    m_keyFormat = data.m_keyFormat;
                    m_sln = data.m_sln;
                    m_kId = data.m_kId;

                    if (data.m_keyLength > 0U) {
                        ::memset(m_keyMaterial, 0x00U, MAX_ENC_KEY_LENGTH_BYTES);

                        m_keyLength = data.m_keyLength;
                        ::memcpy(m_keyMaterial, data.m_keyMaterial, data.m_keyLength);
                    }
                }

                return *this;
            }

            /**
             * @brief Set the key material.
             * @param key 
             * @param keyLength 
             */
            void setKey(const uint8_t* key, uint32_t keyLength)
            {
                assert(key != nullptr);
                m_keyLength = keyLength;
                ::memset(m_keyMaterial, 0x00U, MAX_ENC_KEY_LENGTH_BYTES);
                ::memcpy(m_keyMaterial, key, keyLength);
            }

            /**
             * @brief Get the key material.
             * @param key 
             */
            void getKey(uint8_t* key) const
            {
                assert(key != nullptr);
                ::memcpy(key, m_keyMaterial, m_keyLength);
            }

        public:
            /**
             * @brief 
             */
            DECLARE_PROPERTY_PLAIN(uint8_t, keyFormat);
            /**
             * @brief 
             */
            DECLARE_PROPERTY_PLAIN(uint16_t, sln);
            /**
             * @brief 
             */
            DECLARE_PROPERTY_PLAIN(uint16_t, kId);

        private:
            uint32_t m_keyLength;
            uint8_t m_keyMaterial[MAX_ENC_KEY_LENGTH_BYTES];
        };

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents a key set item within a KMM frame packet.
         * @ingroup p25_kmm
         */
        class HOST_SW_API KeysetItem {
        public:
            /**
             * @brief Initializes a new instance of the KeysetItem class.
             */
            KeysetItem() :
                m_keysetId(0U),
                m_algId(0U),
                m_keyLength(0U),
                m_keys()
            {
                /* stub */
            }

            /**
             * @brief Equals operator. Copies this KeysetItem to another KeysetItem.
             * @param data Instance of KeysetItem to copy.
             */
            virtual KeysetItem& operator= (const KeysetItem& data)
            {
                if (this != &data) {
                    m_keysetId = data.m_keysetId;
                    m_algId = data.m_algId;
                    m_keyLength = data.m_keyLength;

                    m_keys.clear();
                    for (auto key : data.m_keys) {
                        KeyItem copy = key;
                        m_keys.push_back(copy);
                    }
                }

                return *this;
            }

            /**
             * @brief Gets the byte length of this keyset item.
             * @return uint32_t Length of keyset item.
             */
            uint32_t length() const 
            {
                uint32_t len = 4U;

                uint32_t keyItemLength = m_keys.size() * 5U;
                uint32_t combinedKeyLength = m_keys.size() * m_keyLength;
                len += keyItemLength + combinedKeyLength;

                return len;
            }

            /**
             * @brief Add a key to the key list.
             * @param key 
             */
            void push_back(KeyItem key)
            {
                m_keys.push_back(key);
            }

        public:
            /**
             * @brief 
             */
            DECLARE_PROPERTY_PLAIN(uint8_t, keysetId);
            /**
             * @brief Encryption algorithm ID.
             */
            DECLARE_PROPERTY_PLAIN(uint8_t, algId);
            /**
             * @brief 
             */
            DECLARE_PROPERTY_PLAIN(uint8_t, keyLength);

            /**
             * @brief List of keys.
             */
            DECLARE_PROPERTY_PLAIN(std::vector<KeyItem>, keys);
        };
    } // namespace kmm
} // namespace p25

#endif // __P25_KMM__KEYSET_ITEM_H__
