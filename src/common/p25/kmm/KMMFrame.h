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
 * @defgroup p25_kmm Key Management Message
 * @brief Implementation for the data handling of the TIA-102.AACA Project 25 standard.
 * @ingroup p25
 * 
 * @file KMMFrame.h
 * @ingroup p25_kmm
 * @file KMMFrame.cpp
 * @ingroup p25_kmm
 */
#if !defined(__P25_KMM__KMM_FRAME_H__)
#define  __P25_KMM__KMM_FRAME_H__

#include "common/Defines.h"
#include "common/p25/P25Defines.h"
#include "common/Utils.h"

#include <string>

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

        const uint32_t KMM_FRAME_LENGTH = 9U;

        /** @} */

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents a KMM frame packet header.
         * @ingroup p25_kmm
         */
        class HOST_SW_API KMMFrame {
        public:
            /**
             * @brief Initializes a copy instance of the KMMFrame class.
             * @param data Instance of KMMFrame to copy/
             */
            KMMFrame(const KMMFrame& data);
            /**
             * @brief Initializes a new instance of the KMMFrame class.
             */
            KMMFrame();
            /**
             * @brief Finalizes a instance of the KMMFrame class.
             */
            ~KMMFrame();

            /**
             * @brief Gets the byte length of this KMMFrame.
             * @return uint32_t Length of KMMFrame.
             */
            virtual uint32_t length() const
            {
                uint32_t len = KMM_FRAME_LENGTH;
                if (m_messageNumber > 0U)
                    len += 2U;
                if (m_macType == P25DEF::KMM_MAC::ENH_MAC)
                    len += P25DEF::KMM_AES_MAC_LENGTH + 5U;

                return len;
            }

            /**
             * @brief Gets the full byte length of this KMMFrame.
             * @return uint32_t Full Length of KMMFrame.
             */
            uint32_t fullLength()
            {
                m_messageLength = length();
                m_messageFullLength = m_messageLength + 3U;
                return m_messageFullLength;
            }

            /**
             * @brief Decode a KMM frame.
             * @param[in] data Buffer containing KMM frame data to decode.
             * @returns bool True, if decoded, otherwise false.
             */
            virtual bool decode(const uint8_t* data) = 0;
            /**
             * @brief Encode a KMM frame.
             * @param[out] data Buffer to encode KMM frame data to.
             */
            virtual void encode(uint8_t* data) = 0;

            /**
             * @brief Generate a MAC code for the given KMM frame.
             * @param kek Key Encryption Key
             * @param[out] data Buffer to encode KMM MAC to.
             */
            void generateMAC(uint8_t* kek, uint8_t* data);

            /**
             * @brief Returns a string that represents the current KMM frame.
             * @returns std::string String representation of the KMM frame.
             */
            virtual std::string toString();

        public:
            // Common Data
            /**
             * @brief KMM Message ID.
             */
            DECLARE_PROTECTED_PROPERTY(uint8_t, messageId, MessageId);
            /**
             * @brief Message Length.
             */
            DECLARE_PROTECTED_PROPERTY(uint16_t, messageLength, MessageLength);
            
            /**
             * @brief Response Kind.
             */
            DECLARE_PROTECTED_PROPERTY(uint8_t, respKind, ResponseKind);

            /**
             * @brief Message Authentication Type.
             */
            DECLARE_PROTECTED_PROPERTY(uint8_t, macType, MACType);
            /**
             * @brief Message Authentication Algorithm ID.
             */
            DECLARE_PROTECTED_PROPERTY(uint8_t, macAlgId, MACAlgId);
            /**
             * @brief Message Authentication Key ID.
             */
            DECLARE_PROTECTED_PROPERTY(uint16_t, macKId, MACKId);
            /**
             * @brief Message Authentication Format.
             */
            DECLARE_PROTECTED_PROPERTY(uint16_t, macFormat, MACFormat);
            /**
             * @brief Message Number.
             */
            DECLARE_PROTECTED_PROPERTY(uint16_t, messageNumber, MessageNumber);

            /**
             * @brief Destination Logical link ID.
             */
            DECLARE_PROTECTED_PROPERTY(uint32_t, dstLlId, DstLLId);
            /**
             * @brief Source Logical link ID.
             */
            DECLARE_PROTECTED_PROPERTY(uint32_t, srcLlId, SrcLLId);

            /**
             * @brief Flag indicating the KMM frame is complete.
             */
            DECLARE_PROTECTED_PROPERTY(bool, complete, Complete);

        protected:
            uint16_t m_messageFullLength;   //!< Complete length of entire frame in bytes.
            uint8_t m_bodyOffset;           //!< Offset to KMM frame body data.

            uint8_t* m_mac;

            /**
             * @brief Internal helper to decode a KMM header.
             * @param data Buffer containing KMM packet data to decode.
             * @returns bool True, if decoded, otherwise false.
             */
            bool decodeHeader(const uint8_t* data);
            /**
             * @brief Internal helper to encode a KMM header.
             * @param data Buffer to encode KMM packet data to.
             */
            void encodeHeader(uint8_t* data);

            DECLARE_PROTECTED_COPY(KMMFrame);
        };
    } // namespace kmm
} // namespace p25

#endif // __P25_KMM__KMM_FRAME_H__
