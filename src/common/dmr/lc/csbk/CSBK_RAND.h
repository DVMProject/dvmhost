// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file CSBK_RAND.h
 * @ingroup dmr_csbk
 * @file CSBK_RAND.cpp
 * @ingroup dmr_csbk
 */
#if !defined(__DMR_LC_CSBK__CSBK_RAND_H__)
#define  __DMR_LC_CSBK__CSBK_RAND_H__

#include "common/Defines.h"
#include "common/dmr/lc/CSBK.h"

namespace dmr
{
    namespace lc
    {
        namespace csbk
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief Implements RAND - Random Access
             * @ingroup dmr_csbk
             */
            class HOST_SW_API CSBK_RAND : public CSBK {
            public:
                /**
                 * @brief Initializes a new instance of the CSBK_RAND class.
                 */
                CSBK_RAND();

                /**
                 * @brief Decodes a DMR CSBK.
                 * @param[in] data Buffer containing a CSBK to decode.
                 */
                bool decode(const uint8_t* data) override;
                /**
                 * @brief Encodes a DMR CSBK.
                 * @param[out] data Buffer to encode a CSBK.
                 */
                void encode(uint8_t* data) override;

                /**
                 * @brief Returns a string that represents the current CSBK.
                 * @returns std::string String representation of the CSBK.
                 */
                std::string toString() override;

            public:
                /**
                 * @brief Service Options.
                 */
                __PROPERTY(uint8_t, serviceOptions, ServiceOptions);
                /**
                 * @brief Service Extra Options.
                 */
                __PROPERTY(uint8_t, serviceExtra, ServiceExtra);
                /**
                 * @brief Service Kind.
                 */
                __PROPERTY(uint8_t, serviceKind, ServiceKind);

                __COPY(CSBK_RAND);
            };
        } // namespace csbk
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC_CSBK__CSBK_RAND_H__
