/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Server
*
*/
/*
*   Copyright (C) 2018 by Bryan Biedenkapp N2PLL
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
#if !defined(__P25_SITE_DATA_H__)
#define  __P25_SITE_DATA_H__

#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/P25Utils.h"

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Represents site data for P25.
    // ---------------------------------------------------------------------------

    class HOST_SW_API SiteData {
    public:
        /// <summary>Initializes a new instance of the SiteData class.</summary>
        SiteData() :
            m_lra(0U),
            m_netId(P25_WACN_STD_DEFAULT),
            m_sysId(P25_SID_STD_DEFAULT),
            m_rfssId(1U),
            m_siteId(1U),
            m_channelId(1U),
            m_channelNo(1U),
            m_serviceClass(P25_SVC_CLS_VOICE | P25_SVC_CLS_DATA),
            m_isAdjSite(false),
            m_callsign("CHANGEME"),
            m_chCnt(0U),
            m_netActive(false)
        {
            /* stub */
        }
        /// <summary>Initializes a new instance of the SiteData class.</summary>
        /// <param name="netId">P25 Network ID.</param>
        /// <param name="sysId">P25 System ID.</param>
        /// <param name="rfssId">P25 RFSS ID.</param>
        /// <param name="siteId">P25 Site ID.</param>
        /// <param name="lra">P25 Location Resource Area.</param>
        /// <param name="channelId">Channel ID.</param>
        /// <param name="channelNo">Channel Number.</param>
        /// <param name="serviceClass">Service class.</param>
        SiteData(uint32_t netId, uint32_t sysId, uint8_t rfssId, uint8_t siteId, uint8_t lra, uint8_t channelId, uint32_t channelNo, uint8_t serviceClass) :
            m_lra(0U),
            m_netId(P25_WACN_STD_DEFAULT),
            m_sysId(P25_SID_STD_DEFAULT),
            m_rfssId(1U),
            m_siteId(1U),
            m_channelId(1U),
            m_channelNo(1U),
            m_serviceClass(P25_SVC_CLS_VOICE | P25_SVC_CLS_DATA),
            m_isAdjSite(false),
            m_callsign("CHANGEME"),
            m_chCnt(0U),
            m_netActive(false)
        {
            // lra clamping
            if (lra > 0xFFU) // clamp to $FF
                lra = 0xFFU;

            // netId clamping
            netId = P25Utils::netId(netId);

            // sysId clamping
            sysId = P25Utils::sysId(sysId);

            // rfssId clamping
            rfssId = P25Utils::rfssId(rfssId);

            // siteId clamping
            siteId = P25Utils::siteId(siteId);

            // channel id clamping
            if (channelId > 15U)
                channelId = 15U;

            // channel number clamping
            if (m_channelNo == 0U) { // clamp to 1
                m_channelNo = 1U;
            }
            if (m_channelNo > 4095U) { // clamp to 4095
                m_channelNo = 4095U;
            }

            m_lra = lra;

            m_netId = netId;
            m_sysId = sysId;

            m_rfssId = rfssId;
            m_siteId = siteId;

            m_channelId = channelId;
            m_channelNo = channelNo;

            m_serviceClass = serviceClass;
        }

        /// <summary>Helper to set the site callsign.</summary>
        /// <param name="callsign">Callsign.</param>
        void setCallsign(std::string callsign)
        {
            m_callsign = callsign;
        }

        /// <summary>Helper to set the site channel count.</summary>
        /// <param name="chCnt">Channel count.</param>
        void setChCnt(uint8_t chCnt)
        {
            m_chCnt = m_chCnt;
        }

        /// <summary>Helper to set the site network active flag.</summary>
        /// <param name="netActive">Network active.</param>
        void setNetActive(bool netActive)
        {
            m_netActive = netActive;
        }

        /// <summary>Helper to set adjacent site data.</summary>
        /// <param name="sysId">P25 System ID.</param>
        /// <param name="rfssId">P25 RFSS ID.</param>
        /// <param name="siteId">P25 Site ID.</param>
        /// <param name="channelId">Channel ID.</param>
        /// <param name="channelNo">Channel Number.</param>
        /// <param name="serviceClass">Service class.</param>
        void setAdjSite(uint32_t sysId, uint8_t rfssId, uint8_t siteId, uint8_t channelId, uint32_t channelNo, uint8_t serviceClass)
        {
            // sysId clamping
            sysId = P25Utils::sysId(sysId);

            // rfssId clamping
            rfssId = P25Utils::rfssId(rfssId);

            // siteId clamping
            siteId = P25Utils::siteId(siteId);

            // channel id clamping
            if (channelId > 15U)
                channelId = 15U;

            // channel number clamping
            if (m_channelNo == 0U) { // clamp to 1
                m_channelNo = 1U;
            }
            if (m_channelNo > 4095U) { // clamp to 4095
                m_channelNo = 4095U;
            }

            m_lra = 0U;

            m_netId = 0U;
            m_sysId = sysId;

            m_rfssId = rfssId;
            m_siteId = siteId;

            m_channelId = channelId;
            m_channelNo = channelNo;

            m_serviceClass = serviceClass;

            m_isAdjSite = true;

            m_callsign = "ADJSITE ";
            m_chCnt = -1; // don't store channel count for adjacent sites
            m_netActive = true; // adjacent sites are explicitly network active
        }

        /// <summary>Equals operator.</summary>
        /// <param name="data"></param>
        /// <returns></returns>
        SiteData & operator=(const SiteData & data)
        {
            if (this != &data) {
                m_lra = data.m_lra;

                m_netId = data.m_netId;
                m_sysId = data.m_sysId;

                m_rfssId = data.m_rfssId;
                m_siteId = data.m_siteId;

                m_channelId = data.m_channelId;
                m_channelNo = data.m_channelNo;

                m_serviceClass = data.m_serviceClass;

                m_isAdjSite = data.m_isAdjSite;

                m_callsign = data.m_callsign;
                m_chCnt = data.m_chCnt;

                m_netActive = data.m_netActive;
            }

            return *this;
        }

    public:
        /// <summary>P25 location resource area.</summary>
        __READONLY_PROPERTY_PLAIN(uint8_t, lra, lra);
        /// <summary>P25 network ID.</summary>
        __READONLY_PROPERTY_PLAIN(uint32_t, netId, netId);
        /// <summary>Gets the P25 system ID.</summary>
        __READONLY_PROPERTY_PLAIN(uint32_t, sysId, sysId);
        /// <summary>P25 RFSS ID.</summary>
        __READONLY_PROPERTY_PLAIN(uint8_t, rfssId, rfssId);
        /// <summary>P25 site ID.</summary>
        __READONLY_PROPERTY_PLAIN(uint8_t, siteId, siteId);
        /// <summary>Channel ID.</summary>
        __READONLY_PROPERTY_PLAIN(uint8_t, channelId, channelId);
        /// <summary>Channel number.</summary>
        __READONLY_PROPERTY_PLAIN(uint32_t, channelNo, channelNo);
        /// <summary>Channel number.</summary>
        __READONLY_PROPERTY_PLAIN(uint8_t, serviceClass, serviceClass);
        /// <summary>Flag indicating whether this site data is for an adjacent site.</summary>
        __READONLY_PROPERTY_PLAIN(bool, isAdjSite, isAdjSite);
        /// <summary>Callsign.</summary>
        __READONLY_PROPERTY_PLAIN(std::string, callsign, callsign);
        /// <summary>Count of available channels.</summary>
        __READONLY_PROPERTY_PLAIN(uint8_t, chCnt, chCnt);
        /// <summary>Flag indicating whether this site is a linked active network member.</summary>
        __READONLY_PROPERTY_PLAIN(bool, netActive, netActive);
    };
} // namespace p25

#endif // __P25_SITE_DATA_H__
