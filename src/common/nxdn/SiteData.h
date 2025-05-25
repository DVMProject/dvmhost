// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file SiteData.h
 * @ingroup nxdn
 */
#if !defined(__NXDN_SITE_DATA_H__)
#define  __NXDN_SITE_DATA_H__

#include "common/Defines.h"
#include "common/nxdn/NXDNDefines.h"

namespace nxdn
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents site data for NXDN.
     * @ingroup nxdn
     */
    class HOST_SW_API SiteData {
    public:
        /**
         * @brief Initializes a new instance of the SiteData class.
         */
        SiteData() :
            m_locId(1U),
            m_channelId(1U),
            m_channelNo(1U),
            m_siteInfo1(defines::SiteInformation1::VOICE_CALL_SVC | defines::SiteInformation1::DATA_CALL_SVC),
            m_siteInfo2(0U),
            m_isAdjSite(false),
            m_callsign("CHANGEME"),
            m_requireReg(false),
            m_netActive(false)
        {
            /* stub */
        }
        /**
         * @brief Initializes a new instance of the SiteData class.
         * @param locId NXDN Location ID.
         * @param channelId Channel ID.
         * @param channelNo Channel Number.
         * @param siteInfo1 Site Information 1.
         * @param siteInfo2 Site Information 2.
         * @param requireReg Flag indicating the site requires registration.
         */
        SiteData(uint32_t locId, uint8_t channelId, uint32_t channelNo, uint8_t siteInfo1, uint8_t siteInfo2, bool requireReq) :
            m_locId(locId),
            m_channelId(channelId),
            m_channelNo(channelNo),
            m_siteInfo1(defines::SiteInformation1::VOICE_CALL_SVC | defines::SiteInformation1::DATA_CALL_SVC),
            m_siteInfo2(0U),
            m_isAdjSite(false),
            m_callsign("CHANGEME"),
            m_requireReg(requireReq),
            m_netActive(false)
        {
            if (m_locId > 0xFFFFFFU)
                m_locId = 0xFFFFFFU;

            // channel id clamping
            if (m_channelId > 15U)
                m_channelId = 15U;

            // channel number clamping
            if (m_channelNo == 0U) { // clamp to 1
                m_channelNo = 1U;
            }
            if (m_channelNo > 1023U) { // clamp to 1023
                m_channelNo = 1023U;
            }

            m_siteInfo1 = siteInfo1;
            m_siteInfo2 = siteInfo2;
        }

        /**
         * @brief Helper to set the site callsign.
         * @param callsign Callsign.
         */
        void setCallsign(std::string callsign)
        {
            m_callsign = callsign;
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
         * @brief Helper to set adjacent site data.
         * @param locId NXDN Location ID.
         * @param channelId Channel ID.
         * @param channelNo Channel Number.
         * @param siteInfo1 Site Information 1.
         * @param siteInfo2 Site Information 2.
         */
        void setAdjSite(uint32_t locId, uint8_t rfssId, uint8_t siteId, uint8_t channelId, uint32_t channelNo, uint8_t siteInfo1, uint8_t siteInfo2)
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

            m_siteInfo1 = siteInfo1;
            m_siteInfo2 = siteInfo2;

            m_isAdjSite = true;

            m_callsign = "ADJSITE ";
            m_netActive = true; // adjacent sites are explicitly network active
        }

        /**
         * @brief Equals operator.
         * @param data Instance of SiteData to copy.
         */
        SiteData& operator=(const SiteData& data)
        {
            if (this != &data) {
                m_locId = data.m_locId;

                m_channelId = data.m_channelId;
                m_channelNo = data.m_channelNo;

                m_siteInfo1 = data.m_siteInfo1;
                m_siteInfo2 = data.m_siteInfo2;

                m_isAdjSite = data.m_isAdjSite;

                m_callsign = data.m_callsign;

                m_requireReg = data.m_requireReg;

                m_netActive = data.m_netActive;
            }

            return *this;
        }

    public:
        /** @name Site Data */
        /**
         * @brief NXDN location ID.
         */
        DECLARE_RO_PROPERTY_PLAIN(uint32_t, locId);
        /**
         * @brief Channel ID.
         */
        DECLARE_RO_PROPERTY_PLAIN(uint8_t, channelId);
        /**
         * @brief Channel number.
         */
        DECLARE_RO_PROPERTY_PLAIN(uint32_t, channelNo);
        /**
         * @brief Site Information 1.
         */
        DECLARE_RO_PROPERTY_PLAIN(uint8_t, siteInfo1);
        /**
         * @brief Site Information 2.
         */
        DECLARE_RO_PROPERTY_PLAIN(uint8_t, siteInfo2);
        /**
         * @brief Flag indicating whether this site data is for an adjacent site.
         */
        DECLARE_RO_PROPERTY_PLAIN(bool, isAdjSite);
        /**
         * @brief Callsign.
         */
        DECLARE_RO_PROPERTY_PLAIN(std::string, callsign);
        /**
         * @brief NXDN require registration.
         */
        DECLARE_RO_PROPERTY_PLAIN(bool, requireReg);
        /**
         * @brief Flag indicating whether this site is a linked active network member.
         */
        DECLARE_RO_PROPERTY_PLAIN(bool, netActive);
        /** @} */
    };
} // namespace nxdn

#endif // __NXDN_SITE_DATA_H__
