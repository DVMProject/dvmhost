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
#if !defined(__P25_LC_TSBK__IOSP_STS_UPDT_H__)
#define  __P25_LC_TSBK__IOSP_STS_UPDT_H__

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
            //      Implements STS UPDT REQ - Status Update Request (ISP) and
            //          STS UPDT - Status Update (OSP)
            // ---------------------------------------------------------------------------

            class HOST_SW_API IOSP_STS_UPDT : public TSBK {
            public:
                /// <summary>Initializes a new instance of the IOSP_STS_UPDT class.</summary>
                IOSP_STS_UPDT();

                /// <summary>Decode a trunking signalling block.</summary>
                bool decode(const uint8_t* data, bool rawTSBK = false) override;
                /// <summary>Encode a trunking signalling block.</summary>
                void encode(uint8_t* data, bool rawTSBK = false, bool noTrellis = false) override;

                /// <summary>Returns a string that represents the current TSBK.</summary>
                std::string toString(bool isp = false) override;

            public:
                /// <summary>Status value.</summary>
                __PROPERTY(uint8_t, statusValue, Status);

                __COPY(IOSP_STS_UPDT);
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__IOSP_STS_UPDT_H__
