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
 * @file CSBK_CALL_ALRT.h
 * @ingroup dmr_csbk
 * @file CSBK_CALL_ALRT.cpp
 * @ingroup dmr_csbk
 */
#if !defined(__DMR_LC_CSBK__CSBK_CALL_ALRT_H__)
#define  __DMR_LC_CSBK__CSBK_CALL_ALRT_H__

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
             * @brief Implements CALL ALRT - Call Alert
             * @ingroup dmr_csbk
             */
            class HOST_SW_API CSBK_CALL_ALRT : public CSBK {
            public:
                /**
                 * @brief Initializes a new instance of the CSBK_CALL_ALRT class.
                 */
                CSBK_CALL_ALRT();

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
            };
        } // namespace csbk
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC_CSBK__CSBK_CALL_ALRT_H__
