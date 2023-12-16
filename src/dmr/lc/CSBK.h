/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2015,2016 by Jonathan Naylor G4KLX
*   Copyright (C) 2019-2023 by Bryan Biedenkapp N2PLL
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
#if !defined(__DMR_LC__CSBK_H__)
#define __DMR_LC__CSBK_H__

#include "Defines.h"
#include "dmr/DMRDefines.h"
#include "dmr/SiteData.h"
#include "lookups/IdenTableLookup.h"

namespace dmr
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents DMR control signalling block data.
        // ---------------------------------------------------------------------------

        class HOST_SW_API CSBK {
        public:
            /// <summary>Initializes a copy instance of the CSBK class.</summary>
            CSBK(const CSBK& data);
            /// <summary>Initializes a new instance of the CSBK class.</summary>
            CSBK();
            /// <summary>Finalizes a instance of the CSBK class.</summary>
            virtual ~CSBK();

            /// <summary>Decodes a DMR CSBK.</summary>
            virtual bool decode(const uint8_t* data) = 0;
            /// <summary>Encodes a DMR CSBK.</summary>
            virtual void encode(uint8_t* data) = 0;

            /// <summary>Returns a string that represents the current CSBK.</summary>
            virtual std::string toString();

            /// <summary>Regenerate a DMR CSBK without decoding.</summary>
            /// <remarks>This is because the DMR archeticture allows fall-thru of unsupported CSBKs.</remarks>
            static bool regenerate(uint8_t* data, uint8_t dataType);

            /// <summary>Gets the flag indicating verbose log output.</summary>
            static bool getVerbose() { return m_verbose; }
            /// <summary>Sets the flag indicating verbose log output.</summary>
            static void setVerbose(bool verbose) { m_verbose = verbose; }

            /** Local Site data */
            /// <summary>Gets the local site data.</summary>
            static SiteData getSiteData() { return m_siteData; }
            /// <summary>Sets the local site data.</summary>
            static void setSiteData(SiteData siteData) { m_siteData = siteData; }

        public:
            /** Common Data */
            /// <summary>DMR access color code.</summary>
            __PROTECTED_PROPERTY(uint8_t, colorCode, ColorCode);

            /// <summary>Flag indicating this is the last CSBK in a sequence of CSBKs.</summary>
            __PROTECTED_PROPERTY(bool, lastBlock, LastBlock);
            /// <summary>Flag indicating whether the CSBK is a Cdef block.</summary>
            __PROTECTED_PROPERTY(bool, Cdef, Cdef);

            /// <summary>CSBK opcode.</summary>
            __PROTECTED_PROPERTY(uint8_t, CSBKO, CSBKO);
            /// <summary>CSBK feature ID.</summayr>
            __PROTECTED_PROPERTY(uint8_t, FID, FID);

            /// <summary>Flag indicating whether the CSBK is group or individual.</summary>
            __PROTECTED_PROPERTY(bool, GI, GI);

            /// <summary>Source ID.</summary>
            __PROTECTED_PROPERTY(uint32_t, srcId, SrcId);
            /// <summary>Destination ID.</summary>
            __PROTECTED_PROPERTY(uint32_t, dstId, DstId);

            /// <summary></summary>
            __PROTECTED_READONLY_PROPERTY(bool, dataContent, DataContent);

            /// <summary>Number of blocks to follow.</summary>
            __PROTECTED_PROPERTY(uint8_t, CBF, CBF);

            /// <summary>Data type for this CSBK.</summary>
            __PROTECTED_PROPERTY(uint8_t, dataType, DataType);

            /** Common Service Options */
            /// <summary>Flag indicating the emergency bits are set.</summary>
            __PROTECTED_PROPERTY(bool, emergency, Emergency);
            /// <summary>Flag indicating that privacy is enabled.</summary>
            __PROTECTED_PROPERTY(bool, privacy, Privacy);
            /// <summary>Flag indicating that supplementary data is required.</summary>
            __PROTECTED_PROPERTY(bool, supplementData, SupplementData);
            /// <summary>Priority level for the traffic.</summary>
            __PROTECTED_PROPERTY(uint8_t, priority, Priority);
            /// <summary>Flag indicating a broadcast service.</summary>
            __PROTECTED_PROPERTY(bool, broadcast, Broadcast);
            /// <summary>Flag indicating a proxy.</summary>
            __PROTECTED_PROPERTY(bool, proxy, Proxy);

            /// <summary>Response information.</summary>
            __PROTECTED_PROPERTY(uint8_t, response, Response);
            /// <summary>Reason type.</summary>
            __PROTECTED_PROPERTY(uint8_t, reason, Reason);

            /** Tier 3 */
            /// <summary>Site offset timing.</summary>
            __PROTECTED_PROPERTY(bool, siteOffsetTiming, SiteOffsetTiming);

            /// <summary>Broadcast Logical Channel ID 1.</summary>
            __PROTECTED_PROPERTY(uint16_t, logicalCh1, LogicalCh1);
            /// <summary>Broadcast Logical Channel ID 2.</summary>
            __PROTECTED_PROPERTY(uint16_t, logicalCh2, LogicalCh2);
            /// <summary>Logical Channel Slot Number.</summary>
            __PROTECTED_PROPERTY(uint8_t, slotNo, SlotNo);

            /** Local Site data */
            /// <summary>Local Site Identity Entry.</summary>
            __PROTECTED_PROPERTY_PLAIN(::lookups::IdenTable, siteIdenEntry, siteIdenEntry);

        protected:
            static bool m_verbose;

            /** Local Site data */
            static SiteData m_siteData;

            /// <summary>Internal helper to convert payload bytes to a 64-bit long value.</summary>
            static ulong64_t toValue(const uint8_t* payload);
            /// <summary>Internal helper to convert a 64-bit long value to payload bytes.</summary>
            static UInt8Array fromValue(const ulong64_t value);

            /// <summary>Internal helper to decode a control signalling block.</summary>
            bool decode(const uint8_t* data, uint8_t* payload);
            /// <summary>Internal helper to encode a control signalling block.</summary>
            void encode(uint8_t* data, const uint8_t* payload);

            __PROTECTED_COPY(CSBK);
        };
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC__CSBK_H__
