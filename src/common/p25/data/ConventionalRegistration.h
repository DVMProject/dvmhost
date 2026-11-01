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
 * @file ConventionalRegistration.h
 * @ingroup p25_pdu
 * @file ConventionalRegistration.cpp
 * @ingroup p25_pdu
 */
#if !defined(__P25_DATA__CONVENTIONALREGISTRATION_H__)
#define  __P25_DATA__CONVENTIONALREGISTRATION_H__

#include "common/Defines.h"

#include <cstdint>

namespace p25
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Struct Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents a conventional packet data registration PDU.
         * @ingroup p25_pdu
         */
        struct DVM_COMMON_API ConventionalRegistration {
            static constexpr uint32_t DISCONNECT_LENGTH = 8U;
            static constexpr uint32_t LENGTH = 12U;

            uint8_t type = 0U;          //!< The type of the registration (e.g., CONNECT, DISCONNECT)
            uint8_t options = 0U;       //!< The options associated with the registration
            uint32_t llId = 0U;         //!< The logical link identifier for the registration
            uint32_t ipAddress = 0U;    //!< The IP address associated with the registration

            /**
             * @brief Decodes a conventional registration PDU from the given data buffer.
             * @param data The pointer to the data buffer containing the PDU.
             * @param length The length of the data buffer.
             * @param registration The ConventionalRegistration object to populate with the decoded data.
             * @return True if the decoding was successful, false otherwise.
             */
            static bool decode(const uint8_t* data, uint32_t length, ConventionalRegistration& registration);
            /**
             * @brief Encodes the conventional registration PDU into the given data buffer.
             * @param data The pointer to the data buffer to store the encoded PDU.
             * @param capacity The capacity of the data buffer.
             * @return True if the encoding was successful, false otherwise.
             */
            bool encode(uint8_t* data, uint32_t capacity) const;
        };
    } // namespace data
} // namespace p25

#endif // __P25_DATA__CONVENTIONALREGISTRATION_H__
