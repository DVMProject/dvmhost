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
            //      Implements a mechanism to generate raw CSBK data from bytes.
            // ---------------------------------------------------------------------------

            class HOST_SW_API CSBK_RAW : public CSBK {
            public:
                /// <summary>Initializes a new instance of the CSBK_RAW class.</summary>
                CSBK_RAW();
                /// <summary>Finalizes a instance of the CSBK_RAW class.</summary>
                ~CSBK_RAW();

                /// <summary>Decode a trunking signalling block.</summary>
                bool decode(const uint8_t* data) override;
                /// <summary>Encode a trunking signalling block.</summary>
                void encode(uint8_t* data) override;

                /// <summary>Sets the CSBK to encode.</summary>
                void setCSBK(const uint8_t* csbk);

            private:
                uint8_t* m_csbk;
            };
        } // namespace csbk
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC_CSBK__CSBK_RAW_H__
