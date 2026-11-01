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
 * @file CSBK_AHOY.h
 * @ingroup dmr_csbk
 * @file CSBK_AHOY.cpp
 * @ingroup dmr_csbk
 */
 #if !defined(__DMR_LC_CSBK__CSBK_AHOY_H__)
#define __DMR_LC_CSBK__CSBK_AHOY_H__

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
             * @brief Implements the C_AHOY enquiry from a TSCC.
             * @ingroup dmr_csbk
             */
            class DVM_COMMON_API CSBK_AHOY : public CSBK {
            public:
                /**
                 * @brief Initializes a new instance of the CSBK_AHOY class.
                 */
                CSBK_AHOY();

                /**
                 * @brief Decodes a control signalling block.
                 * @param[in] data Buffer containing a CSBK to decode.
                 */
                bool decode(const uint8_t* data) override;
                /**
                 * @brief Encodes a control signalling block.
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
                DECLARE_PROPERTY(uint8_t, serviceOptionsMirror, ServiceOptionsMirror);
                /**
                 * @brief Service Kind Flag.
                 */
                DECLARE_PROPERTY(bool, serviceKindFlag, ServiceKindFlag);

                /**
                 * @brief Ambient Listening.
                 */
                DECLARE_PROPERTY(bool, ambientListening, AmbientListening);

                /**
                 * @brief Appended Blocks.
                 */
                DECLARE_PROPERTY(uint8_t, appendedBlocks, AppendedBlocks);

                /**
                 * @brief Service Kind.
                 */
                DECLARE_PROPERTY(uint8_t, serviceKind, ServiceKind);

                DECLARE_COPY(CSBK_AHOY);
            };
        } // namespace csbk
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC_CSBK__CSBK_AHOY_H__
