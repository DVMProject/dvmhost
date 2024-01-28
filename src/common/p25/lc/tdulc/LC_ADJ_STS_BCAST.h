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
#if !defined(__P25_LC_TSBK__LC_ADJ_STS_BCAST_H__)
#define  __P25_LC_TSBK__LC_ADJ_STS_BCAST_H__

#include "common/Defines.h"
#include "common/p25/lc/TDULC.h"

namespace p25
{
    namespace lc
    {
        namespace tdulc
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      Implements ADJ STS BCAST - Adjacent Site Status Broadcast
            // ---------------------------------------------------------------------------

            class HOST_SW_API LC_ADJ_STS_BCAST : public TDULC {
            public:
                /// <summary>Initializes a new instance of the LC_ADJ_STS_BCAST class.</summary>
                LC_ADJ_STS_BCAST();

                /// <summary>Decode a terminator data unit w/ link control.</summary>
                bool decode(const uint8_t* data) override;
                /// <summary>Encode a terminator data unit w/ link control.</summary>
                void encode(uint8_t* data) override;

            public:
                /// <summary>Adjacent site CFVA flags.</summary>
                __PROPERTY(uint8_t, adjCFVA, AdjSiteCFVA);
                /// <summary>Adjacent site system ID.</summary>
                __PROPERTY(uint32_t, adjSysId, AdjSiteSysId);
                /// <summary>Adjacent site RFSS ID.</summary>
                __PROPERTY(uint8_t, adjRfssId, AdjSiteRFSSId);
                /// <summary>Adjacent site ID.</summary>
                __PROPERTY(uint8_t, adjSiteId, AdjSiteId);
                /// <summary>Adjacent site channel ID.</summary>
                __PROPERTY(uint8_t, adjChannelId, AdjSiteChnId);
                /// <summary>Adjacent site channel number.</summary>
                __PROPERTY(uint32_t, adjChannelNo, AdjSiteChnNo);
                /// <summary>Adjacent site service class.</summary>
                __PROPERTY(uint8_t, adjServiceClass, AdjSiteSvcClass);

                __COPY(LC_ADJ_STS_BCAST);
            };
        } // namespace tdulc
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__LC_ADJ_STS_BCAST_H__
