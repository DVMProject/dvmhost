// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__DMR_LC_CSBK__CSBK_EXT_FNCT_H__)
#define  __DMR_LC_CSBK__CSBK_EXT_FNCT_H__

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
            //      Implements EXT FNCT - Extended Function
            // ---------------------------------------------------------------------------

            class HOST_SW_API CSBK_EXT_FNCT : public CSBK {
            public:
                /// <summary>Initializes a new instance of the CSBK_EXT_FNCT class.</summary>
                CSBK_EXT_FNCT();

                /// <summary>Decode a control signalling block.</summary>
                bool decode(const uint8_t* data) override;
                /// <summary>Encode a control signalling block.</summary>
                void encode(uint8_t* data) override;

                /// <summary>Returns a string that represents the current CSBK.</summary>
                std::string toString() override;

            public:
                /// <summary>Extended function opcode.</summary>
                __PROPERTY(uint8_t, extendedFunction, ExtendedFunction);

                __COPY(CSBK_EXT_FNCT);
            };
        } // namespace csbk
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC_CSBK__CSBK_EXT_FNCT_H__
