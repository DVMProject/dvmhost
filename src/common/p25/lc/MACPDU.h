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
 * @file MACPDU.h
 * @ingroup p25_lc
 * @file MACPDU.cpp
 * @ingroup p25_lc
 */
#if !defined(__P25_LC__MAC_PDU_H__)
#define __P25_LC__MAC_PDU_H__

#include "common/Defines.h"
#include "common/p25/lc/LC.h"

#include <cstdint>

namespace p25
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Base MAC object for a decoded Phase 2 MAC PDU.
         * @ingroup p25_lc
         */
        class DVM_COMMON_API MACPDU {
        public:
            /**
             * @brief Initializes a new instance of the MACPDU class.
             * @param opcode The message opcode for the MAC PDU.
             */
            explicit MACPDU(uint8_t opcode);
            /**
             * @brief Finalizes a instance of the MACPDU class.
             */
            virtual ~MACPDU() = default;

            /**
             * @brief Decodes a MAC PDU from the specified LC control data.
             * @param control The LC control data to decode from.
             * @returns bool True, if the MAC PDU was decoded, otherwise false.
             */
            virtual bool decode(const LC& control);
            /**
             * @brief Encodes the MAC PDU into the specified LC control data.
             * @param control The LC control data to encode into.
             */
            virtual void encode(LC& control) const;

            /**
             * @brief Returns a string that represents the current MAC PDU.
             * @returns std::string String representation of the MAC PDU.
             */
            virtual std::string toString(bool isp = false);

        public:
            /** @name Common Data */
            /**
             * @brief Message opcode for the MAC PDU.
             */
            DECLARE_PROPERTY(uint8_t, opcode, Opcode);

            /**
             * @brief Source ID.
             */
            DECLARE_PROPERTY(uint32_t, srcId, SrcId);
            /**
             * @brief Destination ID.
             */
            DECLARE_PROPERTY(uint32_t, dstId, DstId);
            /** @} */

            /** @name Service Options */
            /**
             * @brief Flag indicating the emergency bits are set.
             */
            DECLARE_PROPERTY(bool, emergency, Emergency);
            /**
             * @brief Flag indicating that encryption is enabled.
             */
            DECLARE_PROPERTY(bool, encrypted, Encrypted);
            /**
             * @brief Priority level for the traffic.
             */
            DECLARE_PROPERTY(uint8_t, priority, Priority);
            /**
             * @brief Flag indicating a group/talkgroup operation.
             */
            DECLARE_PROPERTY(bool, group, Group);
            /** @} */
        };
    }
}

#endif // __P25_LC__MAC_PDU_H__
