// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2021,2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file SiteData.h
 * @ingroup dmr
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
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents site data for DMR.
     * @ingroup dmr
     */
    class HOST_SW_API SiteData {
    public:
        /**
         * @brief Initializes a new instance of the SiteData class.
         */
        SiteData() :
            m_siteModel(defines::SiteModel::SM_SMALL),
            m_netId(1U),
            m_siteId(1U),
            m_parId(3U),
            m_requireReg(false),
            m_netActive(false)
        {
            /* stub */
        }
        /**
         * @brief Initializes a new instance of the SiteData class.
         * @param siteModel DMR site model.
         * @param netId DMR Network ID.
         * @param siteId DMR Site ID.
         * @param parId DMR partition ID.
         * @param requireReg Flag indicating the site requires registration.
         */
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
            if (siteModel > SiteModel::SM_HUGE)
                siteModel = SiteModel::SM_SMALL;

            // netId clamping
            m_netId = DMRUtils::netId(netId, siteModel);

            // siteId clamping
            m_siteId = DMRUtils::siteId(siteId, siteModel);

            // parId clamping
            if (m_parId == 0U)
                m_parId = 3U;
            if (m_parId > 3U)
                m_parId = 3U;
        }

        /**
         * @brief Helper to set the site network active flag.
         * @param netActive Network active.
         */
        void setNetActive(bool netActive)
        {
            m_netActive = netActive;
        }

        /**
         * @brief Returns the DMR system identity value.
         * @param msb 
         * @returns uint32_t System Identity Value.
         */
        const uint32_t systemIdentity(bool msb = false)
        {
            using namespace dmr::defines;
            uint32_t value = m_siteModel;

            switch (m_siteModel)
            {
            case SiteModel::SM_TINY:
            {
                value = (value << 9) + (m_netId & 0x1FFU);
                value = (value << 3) + (m_siteId & 0x07U);
            }
            break;
            case SiteModel::SM_SMALL:
            {
                value = (value << 7) + (m_netId & 0x7FU);
                value = (value << 5) + (m_siteId & 0x1FU);
            }
            break;
            case SiteModel::SM_LARGE:
            {
                value = (value << 5) + (m_netId & 0x1FU);
                value = (value << 7) + (m_siteId & 0x7FU);
            }
            break;
            case SiteModel::SM_HUGE:
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

        /**
         * @brief Equals operator.
         * @param data Instance of SiteData to copy.
         */
        SiteData& operator=(const SiteData& data)
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
        /** @name Site Data */
        /**
         * @brief DMR site model type.
         */
        __READONLY_PROPERTY_PLAIN(defines::SiteModel::E, siteModel);
        /**
         * @brief DMR site network ID.
         */
        __READONLY_PROPERTY_PLAIN(uint16_t, netId);
        /**
         * @brief DMR site ID.
         */
        __READONLY_PROPERTY_PLAIN(uint16_t, siteId);
        /**
         * @brief DMR partition ID.
         */
        __READONLY_PROPERTY_PLAIN(uint8_t, parId);
        /**
         * @brief DMR require registration.
         */
        __READONLY_PROPERTY_PLAIN(bool, requireReg);
        /**
         * @brief Flag indicating whether this site is a linked active network member.
         */
        __READONLY_PROPERTY_PLAIN(bool, netActive);
        /** @} */
    };
} // namespace dmr

#endif // __DMR_SITE_DATA_H__
