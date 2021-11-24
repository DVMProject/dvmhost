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
*   Copyright (C) 2019-2021 by Bryan Biedenkapp N2PLL
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
            /// <summary>Initializes a new instance of the CSBK class.</summary>
            CSBK(SiteData siteData, lookups::IdenTable entry);
            /// <summary>Initializes a new instance of the CSBK class.</summary>
            CSBK(SiteData siteData, lookups::IdenTable entry, bool verbose);
            /// <summary>Finalizes a instance of the CSBK class.</summary>
            ~CSBK();

            /// <summary>Decodes a DMR CSBK.</summary>
            bool decode(const uint8_t* bytes);
            /// <summary>Encodes a DMR CSBK.</summary>
            void encode(uint8_t* bytes);

        public:
            /// <summary>Flag indicating verbose log output.</summary>
            __PROPERTY(bool, verbose, Verbose);

            // Generic fields
            /// <summary>CSBK opcode.</summary>
            __PROPERTY(uint8_t, CSBKO, CSBKO);
            /// <summary>CSBK feature ID.</summayr>
            __PROPERTY(uint8_t, FID, FID);

            /// <summary>Flag indicating this is the last TSBK in a sequence of TSBKs.</summary>
            __PROPERTY(bool, lastBlock, LastBlock);

            // For BS Dwn Act
            __READONLY_PROPERTY(uint32_t, bsId, BSId);

            // For Pre
            /// <summary>Flag indicating whether the CSBK is group or individual.</summary>
            __PROPERTY(bool, GI, GI);

            // For Cdef blocks
            /// <summary>Flag indicating whether the CSBK is a Cdef block.</summary>
            __PROPERTY(bool, Cdef, Cdef);

            /// <summary>Source ID.</summary>
            __PROPERTY(uint32_t, srcId, SrcId);
            /// <summary>Destination ID.</summary>
            __PROPERTY(uint32_t, dstId, DstId);

            /// <summary></summary>
            __READONLY_PROPERTY(bool, dataContent, DataContent);

            /// <summary>Sets the number of blocks to follow.</summary>
            __PROPERTY(uint8_t, CBF, CBF);

            /// <summary>DMR access color code.</summary>
            __PROPERTY(uint8_t, colorCode, ColorCode);

            // Tier III
            /// <summary>Backoff Number.</summary>
            __PROPERTY(uint8_t, backoffNo, BackoffNo);

            /// <summary>Service Type.</summary>
            __PROPERTY(uint8_t, serviceType, serviceType);
            /// <summary>Service type.</summary>
            __PROPERTY(uint8_t, serviceOptions, ServiceOptions);
            /// <summary>Destination/Target address type.</summary>
            __PROPERTY(uint8_t, targetAddress, TargetAddress);

            /// <summary>Response type.</summary>
            __PROPERTY(uint8_t, response, Response);

            /// <summary>Broadcast Announcment Type.</summary>
            __PROPERTY(uint8_t, anncType, AnncType);
            /// <summary>Broadcast Hibernation Flag.</summary>
            __PROPERTY(bool, hibernating, Hibernating);

            /// <summary>Broadcast Announce/Withdraw Channel 1 Flag.</summary>
            __PROPERTY(bool, annWdCh1, AnnWdCh1);
            /// <summary>Broadcast Logical Channel ID 1.</summary>
            __PROPERTY(uint16_t, logicalCh1, LogicalCh1);
            /// <summary>Broadcast Announce/Withdraw Channel 2 Flag.</summary>
            __PROPERTY(bool, annWdCh2, AnnWdCh2);
            /// <summary>Broadcast Logical Channel ID 2.</summary>
            __PROPERTY(uint16_t, logicalCh2, LogicalCh2);

            /// <summary>Logical Channel Slot Number.</summary>
            __PROPERTY(uint8_t, slotNo, SlotNo);

            /// <summary>Aloha Site Time Slot Synchronization.</summary>
            __PROPERTY(bool, siteTSSync, SiteTSSync);
            /// <summary>Aloha site users offset timing.</summary>
            __PROPERTY(bool, siteOffsetTiming, SiteOffsetTiming);
            /// <summary>Aloha MS mask.</summary>
            __PROPERTY(uint8_t, alohaMask, AlohaMask);

            /** Local Site data */
            /// <summary>Local Site Data.</summary>
            __PROPERTY_PLAIN(SiteData, siteData, siteData);
            /// <summary>Local Site Identity Entry.</summary>
            __PROPERTY_PLAIN(lookups::IdenTable, siteIdenEntry, siteIdenEntry);

        private:
            /// <summary>Initializes a new instance of the CSBK class.</summary>
            CSBK(SiteData siteData);

            uint8_t* m_data;
        };
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC__CSBK_H__
