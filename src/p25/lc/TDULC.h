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
            TDULC(LC* lc);
            /// <summary>Initializes a new instance of the TDULC class.</summary>
            TDULC();
            /// <summary>Finalizes a instance of the TDULC class.</summary>
            virtual ~TDULC();

            /// <summary>Decode a terminator data unit w/ link control.</summary>
            virtual bool decode(const uint8_t* data) = 0;
            /// <summary>Encode a terminator data unit w/ link control.</summary>
            virtual void encode(uint8_t* data) = 0;

            /// <summary>Sets the flag indicating verbose log output.</summary>
            static void setVerbose(bool verbose) { m_verbose = verbose; }

            /** Local Site data */
            /// <summary>Gets the local site data.</summary>
            static SiteData getSiteData() { return m_siteData; }
            /// <summary>Sets the local site data.</summary>
            static void setSiteData(SiteData siteData) { m_siteData = siteData; }

        public:
            /** Common Data */
            /// <summary>Flag indicating the link control data is protected.</summary>
            __PROTECTED_PROPERTY(bool, protect, Protect);
            /// <summary>Link control opcode.</summary>
            __PROTECTED_PROPERTY(uint8_t, lco, LCO);
            /// <summary>Manufacturer ID.</summary>
            __PROTECTED_PROPERTY(uint8_t, mfId, MFId);

            /// <summary>Source ID.</summary>
            __PROTECTED_PROPERTY(uint32_t, srcId, SrcId);
            /// <summary>Destination ID.</summary>
            __PROTECTED_PROPERTY(uint32_t, dstId, DstId);

            /// <summary>Voice channel number.</summary>
            __PROTECTED_PROPERTY(uint32_t, grpVchNo, GrpVchNo);

            /** Service Options */
            /// <summary>Flag indicating the emergency bits are set.</summary>
            __PROTECTED_PROPERTY(bool, emergency, Emergency);
            /// <summary>Flag indicating that encryption is enabled.</summary>
            __PROTECTED_PROPERTY(bool, encrypted, Encrypted);
            /// <summary>Priority level for the traffic.</summary>
            __PROTECTED_PROPERTY(uint8_t, priority, Priority);
            /// <summary>Flag indicating a group/talkgroup operation.</summary>
            __PROTECTED_PROPERTY(bool, group, Group);

            /** Local Site data */
            /// <summary>Local Site Identity Entry.</summary>
            __PROTECTED_PROPERTY_PLAIN(::lookups::IdenTable, siteIdenEntry, siteIdenEntry);

        protected:
            friend class LC;
            friend class TSBK;
            edac::RS634717 m_rs;

            bool m_implicit;
            uint32_t m_callTimer;

            static bool m_verbose;

            /** Local Site data */
            static SiteData m_siteData;

            /// <summary>Internal helper to convert RS bytes to a 64-bit long value.</summary>
            static ulong64_t toValue(const uint8_t* rs);
            /// <summary>Internal helper to convert a 64-bit long value to RS bytes.</summary>
            static std::unique_ptr<uint8_t[]> fromValue(const ulong64_t rsValue);

            /// <summary>Internal helper to decode terminator data unit w/ link control.</summary>
            bool decode(const uint8_t* data, uint8_t* rs);
            /// <summary>Internal helper to encode terminator data unit w/ link control.</summary>
            void encode(uint8_t* data, const uint8_t* rs);

            __PROTECTED_COPY(TDULC);
        };
    } // namespace lc
} // namespace p25

#endif // __P25_LC__TDULC_H__
