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
*   Copyright (C) 2021 by Bryan Biedenkapp N2PLL
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
#if !defined(__DMR_SITE_DATA_H__)
#define  __DMR_SITE_DATA_H__

#include "Defines.h"
#include "dmr/DMRDefines.h"

namespace dmr
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Represents site data for DMR.
    // ---------------------------------------------------------------------------

    class HOST_SW_API SiteData {
    public:
        /// <summary>Initializes a new instance of the SiteData class.</summary>
        SiteData() :
            m_siteModel(SITE_MODEL_SMALL),
            m_netId(1U),
            m_siteId(1U),
            m_parId(3U),
            m_requireReg(false),
            m_netActive(false)
        {
            /* stub */
        }
        /// <summary>Initializes a new instance of the SiteData class.</summary>
        /// <param name="siteModel">DMR site model.</param>
        /// <param name="netId">DMR Network ID.</param>
        /// <param name="siteId">DMR Site ID.</param>
        /// <param name="parId">DMR partition ID.</param>
        /// <param name="requireReg"></param>
        SiteData(uint8_t siteModel, uint16_t netId, uint16_t siteId, uint8_t parId, bool requireReq) :
            m_siteModel(siteModel),
            m_netId(netId),
            m_siteId(siteId),
            m_parId(parId),
            m_requireReg(requireReq),
            m_netActive(false)
        {
            // siteModel clamping
            if (siteModel > SITE_MODEL_HUGE)
                siteModel = SITE_MODEL_SMALL;

            // netId clamping
            if (netId == 0U) // clamp to 1
                netId = 1U;

            switch (siteModel) {
            case SITE_MODEL_TINY:
            {
                if (netId > 0x1FFU) // clamp to $1FF
                    netId = 0x1FFU;
            }
            break;
            case SITE_MODEL_SMALL:
            {
                if (netId > 0x7FU) // clamp to $7F
                    netId = 0x7FU;
            }
            break;
            case SITE_MODEL_LARGE:
            {
                if (netId > 0x1FU) // clamp to $1F
                    netId = 0x1FU;
            }
            break;
            case SITE_MODEL_HUGE:
            {
                if (netId > 0x03U) // clamp to $3
                    netId = 0x03U;
            }
            break;
            }

            m_netId = netId;

            // siteId clamping
            if (siteId == 0U) // clamp to 1
                siteId = 1U;

            switch (siteModel)
            {
            case SITE_MODEL_TINY:
            {
                if (siteId > 0x07U) // clamp to $7
                    siteId = 0x07U;
            }
            break;
            case SITE_MODEL_SMALL:
            {
                if (siteId > 0x1FU) // clamp to $1F
                    siteId = 0x1FU;
            }
            break;
            case SITE_MODEL_LARGE:
            {
                if (siteId > 0xFFU) // clamp to $FF
                    siteId = 0xFFU;
            }
            break;
            case SITE_MODEL_HUGE:
            {
                if (siteId > 0x7FFU) // clamp to $7FF
                    siteId = 0x7FFU;
            }
            break;
            }

            m_siteId = siteId;

            // parId clamping
            if (parId == 0U)
                parId = 3U;
            if (parId > 3U)
                parId = 3U;
        }

        /// <summary>Helper to set the site network active flag.</summary>
        /// <param name="netActive">Network active.</param>
        void setNetActive(bool netActive)
        {
            m_netActive = netActive;
        }

        /// <summary>Returns the DMR system identity value.</summary>
        /// <param name="msb"></param>
        /// <returns></returns>
        const uint32_t systemIdentity(bool msb = false)
        {
            uint32_t value = m_siteModel;

            switch (m_siteModel)
            {
            case SITE_MODEL_TINY:
            {
                value = (value << 9) + m_netId;
                value = (value << 3) + m_siteId;
            }
            break;
            case SITE_MODEL_SMALL:
            {
                value = (value << 7) + m_netId;
                value = (value << 5) + m_siteId;
            }
            break;
            case SITE_MODEL_LARGE:
            {
                value = (value << 5) + m_netId;
                value = (value << 7) + m_siteId;
            }
            break;
            case SITE_MODEL_HUGE:
            {
                value = (value << 2) + m_netId;
                value = (value << 10) + m_siteId;
            }
            break;
            }

            if (!msb) {
                value = (value << 2) + m_parId;
            }

            return value & 0xFFFFU;
        }

        /// <summary>Equals operator.</summary>
        /// <param name="data"></param>
        /// <returns></returns>
        SiteData & operator=(const SiteData & data)
        {
            if (this != &data) {
                m_siteModel = data.m_siteModel;

                m_netId = data.m_netId;
                m_siteId = data.m_siteId;

                m_requireReg = data.m_requireReg;

                m_netActive = data.m_netActive;
            }

            return *this;
        }

    public:
        /// <summary>DMR site model type.</summary>
        __READONLY_PROPERTY_PLAIN(uint8_t, siteModel, siteModel);
        /// <summary>DMR site network ID.</summary>
        __READONLY_PROPERTY_PLAIN(uint16_t, netId, netId);
        /// <summary>DMR site ID.</summary>
        __READONLY_PROPERTY_PLAIN(uint16_t, siteId, siteId);
        /// <summary>DMR partition ID.</summary>
        __READONLY_PROPERTY_PLAIN(uint8_t, parId, parId);
        /// <summary>DMR require registration.</summary>
        __READONLY_PROPERTY_PLAIN(bool, requireReg, requireReg);
        /// <summary>Flag indicating whether this site is a linked active network member.</summary>
        __READONLY_PROPERTY_PLAIN(bool, netActive, netActive);
    };
} // namespace dmr

#endif // __DMR_SITE_DATA_H__
