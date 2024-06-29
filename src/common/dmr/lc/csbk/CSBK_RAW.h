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
 * @file CSBK_RAW.h
 * @ingroup dmr_csbk
 * @file CSBK_RAW.cpp
 * @ingroup dmr_csbk
 */
#if !defined(__DMR_LC_CSBK__CSBK_RAW_H__)
#define  __DMR_LC_CSBK__CSBK_RAW_H__

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
             * @brief Implements a mechanism to generate raw CSBK data from bytes.
             * @ingroup dmr_csbk
             */
            class HOST_SW_API CSBK_RAW : public CSBK {
            public:
                /**
                 * @brief Initializes a new instance of the CSBK_RAW class.
                 */
                CSBK_RAW();
                /**
                 * @brief Finalizes a instance of the CSBK_RAW class.
                 */
                ~CSBK_RAW();

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
                 * @brief Sets the CSBK to encode.
                 * @param[in] csbk Buffer containing CSBK to encode.
                 */
                void setCSBK(const uint8_t* csbk);

            private:
                uint8_t* m_csbk;
            };
        } // namespace csbk
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC_CSBK__CSBK_RAW_H__
