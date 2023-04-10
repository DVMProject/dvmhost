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
#if !defined(__P25_LC_TSBK__MBT_OSP_ADJ_STS_BCAST_H__)
#define  __P25_LC_TSBK__MBT_OSP_ADJ_STS_BCAST_H__

#include "Defines.h"
#include "p25/lc/AMBT.h"

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
                virtual bool decodeMBT(const data::DataHeader dataHeader, const data::DataBlock* blocks);
                /// <summary>Encode a alternate trunking signalling block.</summary>
                virtual void encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData);

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
