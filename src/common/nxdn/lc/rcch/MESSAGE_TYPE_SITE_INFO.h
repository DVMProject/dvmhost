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
#if !defined(__NXDN_LC_RCCH__MESSAGE_TYPE_SITE_INFO_H__)
#define  __NXDN_LC_RCCH__MESSAGE_TYPE_SITE_INFO_H__

#include "common/Defines.h"
#include "common/nxdn/lc/RCCH.h"

namespace nxdn
{
    namespace lc
    {
        namespace rcch
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      Implements SITE_INFO - Site Information
            // ---------------------------------------------------------------------------

            class HOST_SW_API MESSAGE_TYPE_SITE_INFO : public RCCH {
            public:
                /// <summary>Initializes a new instance of the MESSAGE_TYPE_SITE_INFO class.</summary>
                MESSAGE_TYPE_SITE_INFO();

                /// <summary>Decode layer 3 data.</summary>
                void decode(const uint8_t* data, uint32_t length, uint32_t offset = 0U) override;
                /// <summary>Encode layer 3 data.</summary>
                void encode(uint8_t* data, uint32_t length, uint32_t offset = 0U) override;

                /// <summary>Returns a string that represents the current RCCH.</summary>
                std::string toString(bool isp = false) override;

            public:
                /** Channel Structure Data */
                /// <summary>Count of BCCH frames per RCCH superframe.</summary>
                __PROPERTY(uint8_t, bcchCnt, BcchCnt);
                /// <summary>Count of RCCH frame groupings per RCCH superframe.</summary>
                __PROPERTY(uint8_t, rcchGroupingCnt, RcchGroupingCnt);
                /// <summary>Count of CCCH/UPCH paging frames per RCCH superframe.</summary>
                __PROPERTY(uint8_t, ccchPagingCnt, CcchPagingCnt);
                /// <summary>Count of CCCH/UPCH multi-purpose frames per RCCH superframe.</summary>
                __PROPERTY(uint8_t, ccchMultiCnt, CcchMultiCnt);
                /// <summary>Count of group iterations per RCCH superframe.</summary>
                __PROPERTY(uint8_t, rcchIterateCnt, RcchIterateCount);

                __COPY(MESSAGE_TYPE_SITE_INFO);
            };
        } // namespace rcch
    } // namespace lc
} // namespace nxdn

#endif // __NXDN_LC_RCCH__MESSAGE_TYPE_SITE_INFO_H__
