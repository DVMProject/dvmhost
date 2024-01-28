// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__P25_LC_TSBK__OSP_TSBK_RAW_H__)
#define  __P25_LC_TSBK__OSP_TSBK_RAW_H__

#include "common/Defines.h"
#include "common/p25/lc/TSBK.h"

namespace p25
{
    namespace lc
    {
        namespace tsbk
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      Implements a mechanism to generate raw TSBK data from bytes.
            // ---------------------------------------------------------------------------

            class HOST_SW_API OSP_TSBK_RAW : public TSBK {
            public:
                /// <summary>Initializes a new instance of the OSP_TSBK_RAW class.</summary>
                OSP_TSBK_RAW();
                /// <summary>Finalizes a instance of the OSP_TSBK_RAW class.</summary>
                ~OSP_TSBK_RAW();

                /// <summary>Decode a trunking signalling block.</summary>
                bool decode(const uint8_t* data, bool rawTSBK = false) override;
                /// <summary>Encode a trunking signalling block.</summary>
                void encode(uint8_t* data, bool rawTSBK = false, bool noTrellis = false) override;

                /// <summary>Sets the TSBK to encode.</summary>
                void setTSBK(const uint8_t* tsbk);

            private:
                uint8_t* m_tsbk;
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__OSP_TSBK_RAW_H__
