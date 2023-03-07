/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Server
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
#if !defined(__NXDN_SITE_DATA_H__)
#define  __NXDN_SITE_DATA_H__

#include "Defines.h"
#include "nxdn/NXDNDefines.h"

namespace nxdn
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Represents site data for NXDN.
    // ---------------------------------------------------------------------------

    class HOST_SW_API SiteData {
    public:
        /// <summary>Initializes a new instance of the SiteData class.</summary>
        SiteData() :
            m_locId(1U),
            m_channelId(1U),
            m_channelNo(1U),
            m_serviceClass(NXDN_SIF1_VOICE_CALL_SVC | NXDN_SIF1_DATA_CALL_SVC),
            m_isAdjSite(false),
            m_callsign("CHANGEME"),
            m_requireReg(false),
            m_netActive(false)
        {
            /* stub */
        }
        /// <summary>Initializes a new instance of the SiteData class.</summary>
        /// <param name="locId">NXDN Location ID.</param>
        /// <param name="channelId">Channel ID.</param>
        /// <param name="channelNo">Channel Number.</param>
        /// <param name="serviceClass">Service class.</param>
        /// <param name="requireReg"></param>
        SiteData(uint32_t locId, uint8_t channelId, uint32_t channelNo, uint8_t serviceClass, bool requireReq) :
            m_locId(locId),
            m_channelId(1U),
            m_channelNo(1U),
            m_serviceClass(NXDN_SIF1_VOICE_CALL_SVC | NXDN_SIF1_DATA_CALL_SVC),
            m_isAdjSite(false),
            m_callsign("CHANGEME"),
            m_requireReg(requireReq),
            m_netActive(false)
        {
            if (m_locId > 0xFFFFFFU)
                m_locId = 0xFFFFFFU;

            // channel id clamping
            if (channelId > 15U)
                channelId = 15U;

            // channel number clamping
            if (m_channelNo == 0U) { // clamp to 1
                m_channelNo = 1U;
            }
            if (m_channelNo > 1023U) { // clamp to 1023
                m_channelNo = 1023U;
            }

            m_serviceClass = serviceClass;
        }

        /// <summary>Helper to set the site callsign.</summary>
        /// <param name="callsign">Callsign.</param>
        void setCallsign(std::string callsign)
        {
            m_callsign = callsign;
        }

        /// <summary>Helper to set the site network active flag.</summary>
        /// <param name="netActive">Network active.</param>
        void setNetActive(bool netActive)
        {
            m_netActive = netActive;
        }

        /// <summary>Helper to set adjacent site data.</summary>
        /// <param name="locId">NXDN Location ID.</param>
        /// <param name="channelId">Channel ID.</param>
        /// <param name="channelNo">Channel Number.</param>
        /// <param name="serviceClass">Service class.</param>
        void setAdjSite(uint32_t locId, uint8_t rfssId, uint8_t siteId, uint8_t channelId, uint32_t channelNo, uint8_t serviceClass)
        {
            if (m_locId > 0xFFFFFFU)
                m_locId = 0xFFFFFFU;

            // channel id clamping
            if (channelId > 15U)
                channelId = 15U;

            // channel number clamping
            if (channelNo == 0U) { // clamp to 1
                channelNo = 1U;
            }
            if (channelNo > 1023U) { // clamp to 1023
                channelNo = 1023U;
            }

            m_locId = locId;

            m_channelId = channelId;
            m_channelNo = channelNo;

            m_serviceClass = serviceClass;

            m_isAdjSite = true;

            m_callsign = "ADJSITE ";
            m_netActive = true; // adjacent sites are explicitly network active
        }

        /// <summary>Equals operator.</summary>
        /// <param name="data"></param>
        /// <returns></returns>
        SiteData & operator=(const SiteData & data)
        {
            if (this != &data) {
                m_locId = data.m_locId;

                m_channelId = data.m_channelId;
                m_channelNo = data.m_channelNo;

                m_serviceClass = data.m_serviceClass;

                m_isAdjSite = data.m_isAdjSite;

                m_callsign = data.m_callsign;

                m_requireReg = data.m_requireReg;

                m_netActive = data.m_netActive;
            }

            return *this;
        }

    public:
        /// <summary>NXDN location ID.</summary>
        __READONLY_PROPERTY_PLAIN(uint32_t, locId, locId);
        /// <summary>Channel ID.</summary>
        __READONLY_PROPERTY_PLAIN(uint8_t, channelId, channelId);
        /// <summary>Channel number.</summary>
        __READONLY_PROPERTY_PLAIN(uint32_t, channelNo, channelNo);
        /// <summary>Service class.</summary>
        __READONLY_PROPERTY_PLAIN(uint8_t, serviceClass, serviceClass);
        /// <summary>Flag indicating whether this site data is for an adjacent site.</summary>
        __READONLY_PROPERTY_PLAIN(bool, isAdjSite, isAdjSite);
        /// <summary>Callsign.</summary>
        __READONLY_PROPERTY_PLAIN(std::string, callsign, callsign);
        /// <summary>NXDN require registration.</summary>
        __READONLY_PROPERTY_PLAIN(bool, requireReg, requireReg);
        /// <summary>Flag indicating whether this site is a linked active network member.</summary>
        __READONLY_PROPERTY_PLAIN(bool, netActive, netActive);
    };
} // namespace nxdn

#endif // __NXDN_SITE_DATA_H__
