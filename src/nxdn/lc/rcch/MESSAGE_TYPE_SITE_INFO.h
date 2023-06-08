/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#if !defined(__NXDN_LC_RCCH__MESSAGE_TYPE_SITE_INFO_H__)
#define  __NXDN_LC_RCCH__MESSAGE_TYPE_SITE_INFO_H__

#include "Defines.h"
#include "nxdn/lc/RCCH.h"

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
                virtual void decode(const uint8_t* data, uint32_t length, uint32_t offset = 0U);
                /// <summary>Encode layer 3 data.</summary>
                virtual void encode(uint8_t* data, uint32_t length, uint32_t offset = 0U);

                /// <summary>Returns a string that represents the current RCCH.</summary>
                virtual std::string toString(bool isp = false);

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
