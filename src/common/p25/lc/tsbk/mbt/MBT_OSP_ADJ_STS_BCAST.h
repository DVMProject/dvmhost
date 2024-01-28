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
#if !defined(__P25_LC_TSBK__MBT_OSP_ADJ_STS_BCAST_H__)
#define  __P25_LC_TSBK__MBT_OSP_ADJ_STS_BCAST_H__

#include "common/Defines.h"
#include "common/p25/lc/AMBT.h"

namespace p25
{
    namespace lc
    {
        namespace tsbk
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      Implements ADJ STS BCAST - Adjacent Site Status Broadcast
            // ---------------------------------------------------------------------------

            class HOST_SW_API MBT_OSP_ADJ_STS_BCAST : public AMBT {
            public:
                /// <summary>Initializes a new instance of the MBT_OSP_ADJ_STS_BCAST class.</summary>
                MBT_OSP_ADJ_STS_BCAST();

                /// <summary>Decode a alternate trunking signalling block.</summary>
                bool decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks) override;
                /// <summary>Encode a alternate trunking signalling block.</summary>
                void encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData) override;

                /// <summary>Returns a string that represents the current TSBK.</summary>
                std::string toString(bool isp = false) override;

            public:
                /** Adjacent Site Data */
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

                __COPY(MBT_OSP_ADJ_STS_BCAST);
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__MBT_OSP_ADJ_STS_BCAST_H__
