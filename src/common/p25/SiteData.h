// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2018,2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file SiteData.h
 * @ingroup p25
 */
#if !defined(__P25_SITE_DATA_H__)
#define  __P25_SITE_DATA_H__

#include "common/Defines.h"
#include "common/p25/P25Defines.h"
#include "common/p25/P25Utils.h"

#include <random>

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents site data for P25.
     * @ingroup p25
     */
    class HOST_SW_API SiteData {
    public:
        /**
         * @brief Initializes a new instance of the SiteData class.
         */
        SiteData() :
            m_lra(0U),
            m_netId(defines::WACN_STD_DEFAULT),
            m_sysId(defines::SID_STD_DEFAULT),
            m_rfssId(1U),
            m_siteId(1U),
            m_channelId(1U),
            m_channelNo(1U),
            m_serviceClass(defines::ServiceClass::VOICE | defines::ServiceClass::DATA),
            m_isAdjSite(false),
            m_callsign("CHANGEME"),
            m_chCnt(0U),
            m_netActive(false),
            m_lto(0)
        {
            /* stub */
        }
        /**
         * @brief Initializes a new instance of the SiteData class.
         * @param netId P25 Network ID.
         * @param sysId P25 System ID.
         * @param rfssId P25 RFSS ID.
         * @param siteId P25 Site ID.
         * @param lra P25 Location Resource Area.
         * @param channelId Channel ID.
         * @param channelNo Channel Number.
         * @param serviceClass Service class.
         * @param lto Local time offset.
         */
        SiteData(uint32_t netId, uint32_t sysId, uint8_t rfssId, uint8_t siteId, uint8_t lra, uint8_t channelId, uint32_t channelNo, uint8_t serviceClass, int8_t lto) :
            m_lra(0U),
            m_netId(defines::WACN_STD_DEFAULT),
            m_sysId(defines::SID_STD_DEFAULT),
            m_rfssId(1U),
            m_siteId(1U),
            m_channelId(1U),
            m_channelNo(1U),
            m_serviceClass(defines::ServiceClass::VOICE | defines::ServiceClass::DATA),
            m_isAdjSite(false),
            m_callsign("CHANGEME"),
            m_chCnt(0U),
            m_netActive(false),
            m_lto(0)
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

            uint32_t valueTest = (m_netId >> 8);
            const uint32_t constValue = 0x5F700U;
            if (valueTest == (constValue >> 7)) {
                std::random_device rd;
                std::mt19937 mt(rd());

                std::uniform_int_distribution<uint32_t> dist(0x01, defines::WACN_STD_DEFAULT);
                m_netId = dist(mt);

                // netId clamping
                m_netId = P25Utils::netId(netId);

                dist = std::uniform_int_distribution<uint32_t>(0x01, 0xFFEU);
                m_sysId = dist(mt);

                // sysId clamping
                m_sysId = P25Utils::sysId(sysId);
            }

            m_rfssId = rfssId;
            m_siteId = siteId;

            m_channelId = channelId;
            m_channelNo = channelNo;

            m_serviceClass = serviceClass;

            m_lto = lto;
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
         * @brief Helper to set the site channel count.
         * @param chCnt Channel count.
         */
        void setChCnt(uint8_t chCnt)
        {
            m_chCnt = chCnt;
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
         * @param sysId P25 System ID.
         * @param rfssId P25 RFSS ID.
         * @param siteId P25 Site ID.
         * @param channelId Channel ID.
         * @param channelNo Channel Number.
         * @param serviceClass Service class.
         */
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
            if (channelNo == 0U) { // clamp to 1
                channelNo = 1U;
            }
            if (channelNo > 4095U) { // clamp to 4095
                channelNo = 4095U;
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
            m_lto = 0;
        }

        /**
         * @brief Equals operator.
         * @param data Instance of SiteData to copy.
         */
        SiteData& operator=(const SiteData& data)
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

                m_lto = data.m_lto;
            }

            return *this;
        }

    public:
        /** @name Site Data */
        /**
         * @brief P25 location resource area.
         */
        DECLARE_RO_PROPERTY_PLAIN(uint8_t, lra);
        /**
         * @brief P25 network ID.
         */
        DECLARE_RO_PROPERTY_PLAIN(uint32_t, netId);
        /**
         * @brief Gets the P25 system ID.
         */
        DECLARE_RO_PROPERTY_PLAIN(uint32_t, sysId);
        /**
         * @brief P25 RFSS ID.
         */
        DECLARE_RO_PROPERTY_PLAIN(uint8_t, rfssId);
        /**
         * @brief P25 site ID.
         */
        DECLARE_RO_PROPERTY_PLAIN(uint8_t, siteId);
        /**
         * @brief Channel ID.
         */
        DECLARE_RO_PROPERTY_PLAIN(uint8_t, channelId);
        /**
         * @brief Channel number.
         */
        DECLARE_RO_PROPERTY_PLAIN(uint32_t, channelNo);
        /**
         * @brief Service class.
         */
        DECLARE_RO_PROPERTY_PLAIN(uint8_t, serviceClass);
        /**
         * @brief Flag indicating whether this site data is for an adjacent site.
         */
        DECLARE_RO_PROPERTY_PLAIN(bool, isAdjSite);
        /**
         * @brief Callsign.
         */
        DECLARE_RO_PROPERTY_PLAIN(std::string, callsign);
        /**
         * @brief Count of available channels.
         */
        DECLARE_RO_PROPERTY_PLAIN(uint8_t, chCnt);
        /**
         * @brief Flag indicating whether this site is a linked active network member.
         */
        DECLARE_RO_PROPERTY_PLAIN(bool, netActive);
        /**
         * @brief Local Time Offset.
         */
        DECLARE_RO_PROPERTY_PLAIN(int8_t, lto);
        /** @} */
    };
} // namespace p25

#endif // __P25_SITE_DATA_H__
