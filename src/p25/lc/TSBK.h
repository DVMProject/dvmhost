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
#if !defined(__P25_LC__TSBK_H__)
#define  __P25_LC__TSBK_H__

#include "Defines.h"
#include "p25/edac/Trellis.h"
#include "p25/lc/LC.h"
#include "p25/lc/TDULC.h"
#include "p25/SiteData.h"
#include "edac/RS634717.h"
#include "lookups/IdenTableLookup.h"

#include <string>

namespace p25
{
    namespace dfsi
    {
        // ---------------------------------------------------------------------------
        //  Class Prototypes
        // ---------------------------------------------------------------------------

        class HOST_SW_API LC;
    }

    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Prototypes
        // ---------------------------------------------------------------------------

        class HOST_SW_API LC;
        class HOST_SW_API TDULC;

        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents link control data for TSDU packets.
        // ---------------------------------------------------------------------------

        class HOST_SW_API TSBK {
        public:
            /// <summary>Initializes a copy instance of the TSBK class.</summary>
            TSBK(const TSBK& data);
            /// <summary>Initializes a new instance of the TSBK class.</summary>
            TSBK(LC* lc);
            /// <summary>Initializes a new instance of the TSBK class.</summary>
            TSBK();
            /// <summary>Finalizes a instance of the TSBK class.</summary>
            virtual ~TSBK();

            /// <summary>Decode a trunking signalling block.</summary>
            virtual bool decode(const uint8_t* data, bool rawTSBK = false) = 0;
            /// <summary>Encode a trunking signalling block.</summary>
            virtual void encode(uint8_t* data, bool rawTSBK = false, bool noTrellis = false) = 0;

            /// <summary>Sets the flag indicating verbose log output.</summary>
            static void setVerbose(bool verbose) { m_verbose = verbose; }
            /// <summary>Sets the flag indicating CRC-errors should be warnings and not errors.</summary>
            static void setWarnCRC(bool warnCRC) { m_warnCRC = warnCRC; }

            /** Local Site data */
            /// <summary>Sets the callsign.</summary>
            static void setCallsign(std::string callsign);

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

            /// <summary>Flag indicating this is the last TSBK in a sequence of TSBKs.</summary>
            __PROTECTED_PROPERTY(bool, lastBlock, LastBlock);
            /// <summary>Flag indicating this TSBK contains additional information.</summary>
            __PROTECTED_PROPERTY(bool, aivFlag, AIV);
            /// <summary>Flag indicating this TSBK contains extended addressing.</summary>
            __PROTECTED_PROPERTY(bool, extendedAddrFlag, EX);

            /// <summary>Service type.</summary>
            __PROTECTED_PROPERTY(uint8_t, service, Service);
            /// <summary>Response type.</summary>
            __PROTECTED_PROPERTY(uint8_t, response, Response);

            /// <summary>Configured network ID.</summary>
            __PROTECTED_READONLY_PROPERTY(uint32_t, netId, NetId);
            /// <summary>Configured system ID.</summary>
            __PROTECTED_READONLY_PROPERTY(uint32_t, sysId, SysId);

            /// <summary>Voice channel ID.</summary>
            __PROTECTED_PROPERTY(uint32_t, grpVchId, GrpVchId);
            /// <summary>Voice channel number.</summary>
            __PROTECTED_PROPERTY(uint32_t, grpVchNo, GrpVchNo);

            /** Common Service Options */
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
            friend class dfsi::LC;
            friend class LC;
            friend class TDULC;
            edac::RS634717 m_rs;
            edac::Trellis m_trellis;

            static bool m_verbose;
            static bool m_warnCRC;

            /** Local Site data */
            static uint8_t* m_siteCallsign;
            static SiteData m_siteData;

            /// <summary>Internal helper to convert TSBK bytes to a 64-bit long value.</summary>
            static ulong64_t toValue(const uint8_t* tsbk);
            /// <summary>Internal helper to convert a 64-bit long value to TSBK bytes.</summary>
            static std::unique_ptr<uint8_t[]> fromValue(const ulong64_t tsbkValue);

            /// <summary>Internal helper to decode a trunking signalling block.</summary>
            bool decode(const uint8_t* data, uint8_t* tsbk, bool rawTSBK = false);
            /// <summary>Internal helper to encode a trunking signalling block.</summary>
            void encode(uint8_t* data, const uint8_t* tsbk, bool rawTSBK = false, bool noTrellis = false);

            __PROTECTED_COPY(TSBK);
        };
    } // namespace lc
} // namespace p25

#endif // __P25_LC__TSBK_H__
