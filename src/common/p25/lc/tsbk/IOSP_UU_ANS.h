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
#if !defined(__P25_LC_TSBK__IOSP_UU_ANS_H__)
#define  __P25_LC_TSBK__IOSP_UU_ANS_H__

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
            //      Implements UU ANS RSP - Unit-to-Unit Answer Response (ISP) and
            //          UU ANS REQ - Unit-to-Unit Answer Request (OSP)
            // ---------------------------------------------------------------------------

            class HOST_SW_API IOSP_UU_ANS : public TSBK {
            public:
                /// <summary>Initializes a new instance of the IOSP_UU_ANS class.</summary>
                IOSP_UU_ANS();

                /// <summary>Equals operator.</summary>
                IOSP_UU_ANS& operator=(const IOSP_UU_ANS& data);

                /// <summary>Decode a trunking signalling block.</summary>
                bool decode(const uint8_t* data, bool rawTSBK = false) override;
                /// <summary>Encode a trunking signalling block.</summary>
                void encode(uint8_t* data, bool rawTSBK = false, bool noTrellis = false) override;

                /// <summary>Returns a string that represents the current TSBK.</summary>
                std::string toString(bool isp = false) override;
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__IOSP_UU_ANS_H__
