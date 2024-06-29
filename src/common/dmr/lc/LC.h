// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2020-2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup dmr_lc Link Control
 * @brief Implementation for the data handling of ETSI TS-102 link control data.
 * @ingroup dmr
 * 
 * @file LC.h
 * @ingroup dmr_lc
 * @file LC.cpp
 * @ingroup dmr_lc
 */
#if !defined(__DMR_LC__LC_H__)
#define __DMR_LC__LC_H__

#include "common/Defines.h"
#include "common/dmr/DMRDefines.h"

namespace dmr
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents DMR link control data.
         * @ingroup dmr_lc
         */
        class HOST_SW_API LC {
        public:
            /**
             * @brief Initializes a new instance of the LC class.
             * @param flco Full-link Control Opcode.
             * @param srcId Source ID.
             * @param dstId Destination ID.
             */
            LC(defines::FLCO::E flco, uint32_t srcId, uint32_t dstId);
            /**
             * @brief Initializes a new instance of the LC class.
             * @param data Buffer containing LC data.
             */
            LC(const uint8_t* data);
            /**
             * @brief Initializes a new instance of the LC class.
             * @param bits Boolean bit buffer containing LC data.
             */
            LC(const bool* bits);
            /**
             * @brief Initializes a new instance of the LC class.
             */
            LC();
            /**
             * @brief Finalizes a instance of the LC class.
             */
            ~LC();

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

        public:
            /** @name Common Data */
            /**
             * @brief Flag indicating whether link protection is enabled.
             */
            __PROPERTY(bool, PF, PF);

            /**
             * @brief Full-link control opcode.
             */
            __PROPERTY(defines::FLCO::E, FLCO, FLCO);

            /**
             * @brief Feature ID
             */
            __PROPERTY(uint8_t, FID, FID);

            /**
             * @brief Source ID.
             */
            __PROPERTY(uint32_t, srcId, SrcId);
            /**
             * @brief Destination ID.
             */
            __PROPERTY(uint32_t, dstId, DstId);
            /** @} */

            /** @name Service Options */
            /**
             * @brief Flag indicating the emergency bits are set.
             */
            __PROPERTY(bool, emergency, Emergency);
            /**
             * @brief Flag indicating that encryption is enabled.
             */
            __PROPERTY(bool, encrypted, Encrypted);
            /**
             * @brief Flag indicating broadcast operation.
             */
            __PROPERTY(bool, broadcast, Broadcast);
            /**
             * @brief Flag indicating OVCM operation.
             */
            __PROPERTY(bool, ovcm, OVCM);
            /**
             * @brief Priority level for the traffic.
             */
            __PROPERTY(uint8_t, priority, Priority);
            /** @} */

        private:
            bool m_R;
        };
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC__LC_H__
