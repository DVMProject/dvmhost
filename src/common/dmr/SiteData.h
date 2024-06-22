// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2021,2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__DMR_SITE_DATA_H__)
#define  __DMR_SITE_DATA_H__

#include "common/Defines.h"
#include "common/dmr/DMRDefines.h"
#include "common/dmr/DMRUtils.h"

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
            m_siteModel(defines::SiteModel::SMALL),
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
        SiteData(defines::SiteModel::E siteModel, uint16_t netId, uint16_t siteId, uint8_t parId, bool requireReq) :
            m_siteModel(siteModel),
            m_netId(netId),
            m_siteId(siteId),
            m_parId(parId),
            m_requireReg(requireReq),
            m_netActive(false)
        {
            using namespace dmr::defines;
            // siteModel clamping
            if (siteModel > SiteModel::HUGE)
                siteModel = SiteModel::SMALL;

            // netId clamping
            m_netId = DMRUtils::netId(netId, siteModel);

            // siteId clamping
            m_siteId = DMRUtils::siteId(siteId, siteModel);

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
            using namespace dmr::defines;
            uint32_t value = m_siteModel;

            switch (m_siteModel)
            {
            case SiteModel::TINY:
            {
                value = (value << 9) + (m_netId & 0x1FFU);
                value = (value << 3) + (m_siteId & 0x07U);
            }
            break;
            case SiteModel::SMALL:
            {
                value = (value << 7) + (m_netId & 0x7FU);
                value = (value << 5) + (m_siteId & 0x1FU);
            }
            break;
            case SiteModel::LARGE:
            {
                value = (value << 5) + (m_netId & 0x1FU);
                value = (value << 7) + (m_siteId & 0x7FU);
            }
            break;
            case SiteModel::HUGE:
            {
                value = (value << 2) + (m_netId & 0x03U);
                value = (value << 10) + (m_siteId & 0x3FFU);
            }
            break;
            }

            if (!msb) {
                value = (value << 2) + (m_parId & 0x03U);
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
        __READONLY_PROPERTY_PLAIN(defines::SiteModel::E, siteModel);
        /// <summary>DMR site network ID.</summary>
        __READONLY_PROPERTY_PLAIN(uint16_t, netId);
        /// <summary>DMR site ID.</summary>
        __READONLY_PROPERTY_PLAIN(uint16_t, siteId);
        /// <summary>DMR partition ID.</summary>
        __READONLY_PROPERTY_PLAIN(uint8_t, parId);
        /// <summary>DMR require registration.</summary>
        __READONLY_PROPERTY_PLAIN(bool, requireReg);
        /// <summary>Flag indicating whether this site is a linked active network member.</summary>
        __READONLY_PROPERTY_PLAIN(bool, netActive);
    };
} // namespace dmr

#endif // __DMR_SITE_DATA_H__
