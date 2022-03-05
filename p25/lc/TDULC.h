/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2017-2022 by Bryan Biedenkapp N2PLL
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
#if !defined(__P25_LC__TDULC_H__)
#define  __P25_LC__TDULC_H__

#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/lc/LC.h"
#include "p25/lc/TSBK.h"
#include "p25/SiteData.h"
#include "edac/RS634717.h"
#include "lookups/IdenTableLookup.h"

#include <string>

namespace p25
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Prototypes
        // ---------------------------------------------------------------------------
        class HOST_SW_API LC;
        class HOST_SW_API TSBK;

        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents link control data for TDULC packets.
        // ---------------------------------------------------------------------------

        class HOST_SW_API TDULC {
        public:
            /// <summary>Initializes a copy instance of the TDULC class.</summary>
            TDULC(const TDULC& data);
            /// <summary>Initializes a new instance of the TDULC class.</summary>
            TDULC(SiteData siteData, lookups::IdenTable entry);
            /// <summary>Initializes a new instance of the TDULC class.</summary>
            TDULC(SiteData siteData, lookups::IdenTable entry, bool verbose);
            /// <summary>Initializes a new instance of the TDULC class.</summary>
            TDULC(LC* lc);
            /// <summary>Finalizes a instance of the TDULC class.</summary>
            ~TDULC();

            /// <summary>Equals operator.</summary>
            TDULC& operator=(const TDULC& data);

            /// <summary>Decode a trunking signalling block.</summary>
            bool decode(const uint8_t* data);
            /// <summary>Encode a trunking signalling block.</summary>
            void encode(uint8_t* data);

        public:
            /// <summary>Flag indicating verbose log output.</summary>
            __PROPERTY(bool, verbose, Verbose);

            /** Common Data */
            /// <summary>Flag indicating the link control data is protected.</summary>
            __PROPERTY(bool, protect, Protect);
            /// <summary>Link control opcode.</summary>
            __PROPERTY(uint8_t, lco, LCO);
            /// <summary>Manufacturer ID.</summary>
            __PROPERTY(uint8_t, mfId, MFId);

            /// <summary>Source ID.</summary>
            __PROPERTY(uint32_t, srcId, SrcId);
            /// <summary>Destination ID.</summary>
            __PROPERTY(uint32_t, dstId, DstId);

            /// <summary>Voice channel number.</summary>
            __PROPERTY(uint32_t, grpVchNo, GrpVchNo);

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

            /** Service Options */
            /// <summary>Flag indicating the emergency bits are set.</summary>
            __PROPERTY(bool, emergency, Emergency);
            /// <summary>Flag indicating that encryption is enabled.</summary>
            __PROPERTY(bool, encrypted, Encrypted);
            /// <summary>Priority level for the traffic.</summary>
            __PROPERTY(uint8_t, priority, Priority);
            /// <summary>Flag indicating a group/talkgroup operation.</summary>
            __PROPERTY(bool, group, Group);

            /** Local Site data */
            /// <summary>Local Site Data.</summary>
            __PROPERTY_PLAIN(SiteData, siteData, siteData);
            /// <summary>Local Site Identity Entry.</summary>
            __PROPERTY_PLAIN(lookups::IdenTable, siteIdenEntry, siteIdenEntry);

        private:
            /// <summary>Initializes a new instance of the TDULC class.</summary>
            TDULC();
            /// <summary>Initializes a new instance of the TDULC class.</summary>
            TDULC(SiteData siteData);

            friend class LC;
            friend class TSBK;
            edac::RS634717 m_rs;

            uint32_t m_callTimer;

            /// <summary>Internal helper to copy the class.</summary>
            void copy(const TDULC& data);

            /// <summary>Decode link control.</summary>
            bool decodeLC(const uint8_t* rs);
            /// <summary>Encode link control.</summary>
            void encodeLC(uint8_t* rs);
        };
    } // namespace lc
} // namespace p25

#endif // __P25_LC__TDULC_H__
