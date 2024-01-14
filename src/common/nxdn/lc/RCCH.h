/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
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

#include "common/Defines.h"
#include "common/nxdn/SiteData.h"
#include "common/lookups/IdenTableLookup.h"

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
            /// <summary>Initializes a new instance of the RCCH class.</summary>
            RCCH();
            /// <summary>Finalizes a instance of the RCCH class.</summary>
            virtual ~RCCH();

            /// <summary>Decode layer 3 data.</summary>
            virtual void decode(const uint8_t* data, uint32_t length, uint32_t offset = 0U) = 0;
            /// <summary>Encode layer 3 data.</summary>
            virtual void encode(uint8_t* data, uint32_t length, uint32_t offset = 0U) = 0;

            /// <summary>Returns a string that represents the current RCCH.</summary>
            virtual std::string toString(bool isp = false);

            /// <summary>Gets the flag indicating verbose log output.</summary>
            static bool getVerbose() { return m_verbose; }
            /// <summary>Sets the flag indicating verbose log output.</summary>
            static void setVerbose(bool verbose) { m_verbose = verbose; }

            /** Local Site data */
            /// <summary>Sets the callsign.</summary>
            static void setCallsign(std::string callsign);

            /// <summary>Gets the local site data.</summary>
            static SiteData getSiteData() { return m_siteData; }
            /// <summary>Sets the local site data.</summary>
            static void setSiteData(SiteData siteData) { m_siteData = siteData; }

        public:
            /** Common Data */
            /// <summary>Message Type</summary>
            __PROTECTED_PROPERTY(uint8_t, messageType, MessageType);

            /// <summary>Source ID.</summary>
            __PROTECTED_PROPERTY(uint16_t, srcId, SrcId);
            /// <summary>Destination ID.</summary>
            __PROTECTED_PROPERTY(uint16_t, dstId, DstId);

            /// <summary>Location ID.</summary>
            __PROTECTED_PROPERTY(uint32_t, locId, LocId);
            /// <summary>Registration Option.</summary>
            __PROTECTED_PROPERTY(uint8_t, regOption, RegOption);

            /// <summary>Version Number.</summary>
            __PROTECTED_PROPERTY(uint8_t, version, Version);

            /// <summary>Cause Response.</summary>
            __PROTECTED_PROPERTY(uint8_t, causeRsp, CauseResponse);

            /// <summary>Voice channel number.</summary>
            __PROTECTED_PROPERTY(uint32_t, grpVchNo, GrpVchNo);

            /** Call Data */
            /// <summary>Call Type</summary>
            __PROTECTED_PROPERTY(uint8_t, callType, CallType);

            /** Common Call Options */
            /// <summary>Flag indicating the emergency bits are set.</summary>
            __PROTECTED_PROPERTY(bool, emergency, Emergency);
            /// <summary>Flag indicating that encryption is enabled.</summary>
            __PROTECTED_PROPERTY(bool, encrypted, Encrypted);
            /// <summary>Flag indicating priority paging.</summary>
            __PROTECTED_PROPERTY(bool, priority, Priority);
            /// <summary>Flag indicating a group/talkgroup operation.</summary>
            __PROTECTED_PROPERTY(bool, group, Group);
            /// <summary>Flag indicating a half/full duplex operation.</summary>
            __PROTECTED_PROPERTY(bool, duplex, Duplex);

            /// <summary>Transmission mode.</summary>
            __PROTECTED_PROPERTY(uint8_t, transmissionMode, TransmissionMode);

            /** Local Site data */
            /// <summary>Local Site Identity Entry.</summary>
            __PROTECTED_PROPERTY_PLAIN(lookups::IdenTable, siteIdenEntry);

        protected:
            static bool m_verbose;

            /** Local Site data */
            static uint8_t* m_siteCallsign;
            static SiteData m_siteData;

            /// <summary>Internal helper to decode a RCCH.</summary>
            void decode(const uint8_t* data, uint8_t* rcch, uint32_t length, uint32_t offset = 0U);
            /// <summary>Internal helper to encode a RCCH.</summary>
            void encode(uint8_t* data, const uint8_t* rcch, uint32_t length, uint32_t offset = 0U);

            __PROTECTED_COPY(RCCH);
        };
    } // namespace lc
} // namespace nxdn

#endif // __NXDN_LC__RCCH_H__
