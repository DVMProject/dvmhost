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
#if !defined(__NXDN_LC__RCCH_H__)
#define  __NXDN_LC__RCCH_H__

#include "Defines.h"
#include "nxdn/lc/PacketInformation.h"
#include "nxdn/SiteData.h"
#include "lookups/IdenTableLookup.h"

namespace nxdn
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents link control data for control channel NXDN calls.
        // ---------------------------------------------------------------------------

        class HOST_SW_API RCCH {
        public:
            /// <summary>Initializes a copy instance of the RCCH class.</summary>
            RCCH(const RCCH& data);
            /// <summary>Initializes a new instance of the TSBK class.</summary>
            RCCH(SiteData siteData, lookups::IdenTable entry);
            /// <summary>Initializes a new instance of the TSBK class.</summary>
            RCCH(SiteData siteData, lookups::IdenTable entry, bool verbose);
            /// <summary>Finalizes a instance of the RCCH class.</summary>
            ~RCCH();

            /// <summary>Equals operator.</summary>
            RCCH& operator=(const RCCH& data);

            /// <summary>Decode layer 3 data.</summary>
            void decode(const uint8_t* data, uint32_t length, uint32_t offset = 0U);
            /// <summary>Encode layer 3 data.</summary>
            void encode(uint8_t* data, uint32_t length, uint32_t offset = 0U);

            /// <summary></summary>
            void reset();

            /// <summary>Sets the callsign.</summary>
            void setCallsign(std::string callsign);

        public:
            /// <summary>Flag indicating verbose log output.</summary>
            __PROPERTY(bool, verbose, Verbose);

            /** Common Data */
            /// <summary>Message Type</summary>
            __PROPERTY(uint8_t, messageType, MessageType);

            /// <summary>Source ID.</summary>
            __PROPERTY(uint16_t, srcId, SrcId);
            /// <summary>Destination ID.</summary>
            __PROPERTY(uint16_t, dstId, DstId);

            /// <summary>Location ID.</summary>
            __PROPERTY(uint32_t, locId, LocId);
            /// <summary>Registration Option.</summary>
            __PROPERTY(uint8_t, regOption, RegOption);

            /// <summary>Version Number.</summary>
            __PROPERTY(uint8_t, version, Version);

            /// <summary>Cause Response.</summary>
            __PROPERTY(uint8_t, causeRsp, CauseResponse);

            /// <summary>Voice channel number.</summary>
            __PROPERTY(uint32_t, grpVchNo, GrpVchNo);

            /** Call Data */
            /// <summary>Call Type</summary>
            __PROPERTY(uint8_t, callType, CallType);

            /** Common Call Options */
            /// <summary>Flag indicating the emergency bits are set.</summary>
            __PROPERTY(bool, emergency, Emergency);
            /// <summary>Flag indicating that encryption is enabled.</summary>
            __PROPERTY(bool, encrypted, Encrypted);
            /// <summary>Flag indicating priority paging.</summary>
            __PROPERTY(bool, priority, Priority);
            /// <summary>Flag indicating a group/talkgroup operation.</summary>
            __PROPERTY(bool, group, Group);
            /// <summary>Flag indicating a half/full duplex operation.</summary>
            __PROPERTY(bool, duplex, Duplex);

            /// <summary>Transmission mode.</summary>
            __PROPERTY(uint8_t, transmissionMode, TransmissionMode);

            /** Local Site data */
            /// <summary>Local Site Data.</summary>
            __PROPERTY_PLAIN(SiteData, siteData, siteData);
            /// <summary>Local Site Identity Entry.</summary>
            __PROPERTY_PLAIN(lookups::IdenTable, siteIdenEntry, siteIdenEntry);

        private:
            /// <summary>Initializes a new instance of the RCCH class.</summary>
            RCCH();
            /// <summary>Initializes a new instance of the RCCH class.</summary>
            RCCH(SiteData siteData);

            uint8_t* m_data;

            /** Local Site data */
            uint8_t* m_siteCallsign;

            /// <summary>Decode link control.</summary>
            bool decodeLC(const uint8_t* data);
            /// <summary>Encode link control.</summary>
            void encodeLC(uint8_t* data);

            /// <summary>Internal helper to copy the class.</summary>
            void copy(const RCCH& data);
        };
    } // namespace lc
} // namespace nxdn

#endif // __NXDN_LC__RCCH_H__
