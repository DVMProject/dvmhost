// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *   Copyright (C) 2022 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file RCCH.h
 * @ingroup nxdn_lc
 * @file RCCH.cpp
 * @ingroup nxdn_lc
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
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents link control data for control channel NXDN calls.
         * @ingroup nxdn_lc
         */
        class HOST_SW_API RCCH {
        public:
            /**
             * @brief Initializes a new instance of the RCCH class.
             */
            RCCH();
            /**
             * @brief Initializes a copy instance of the RCCH class.
             * @param data Instance of RCCH to copy.
             */
            RCCH(const RCCH& data);
            /**
             * @brief Finalizes a instance of the RCCH class.
             */
            virtual ~RCCH();

            /**
             * @brief Decode RCCH data.
             * @param[in] data Buffer containing a RCCH to decode.
             * @param length Length of data buffer.
             * @param offset Offset for RCCH in data buffer.
             */
            virtual void decode(const uint8_t* data, uint32_t length, uint32_t offset = 0U) = 0;
            /**
             * @brief Encode RCCH data.
             * @param[out] data Buffer to encode a RCCH.
             * @param length Length of data buffer.
             * @param offset Offset for RCCH in data buffer.
             */
            virtual void encode(uint8_t* data, uint32_t length, uint32_t offset = 0U) = 0;

            /**
             * @brief Returns a string that represents the current RCCH.
             * @returns std::string String representation of the RCCH.
             */
            virtual std::string toString(bool isp = false);

            /**
             * @brief Gets the flag indicating verbose log output.
             * @returns bool True, if the TSBK is verbose logging, otherwise false.
             */
            static bool getVerbose() { return m_verbose; }
            /**
             * @brief Sets the flag indicating verbose log output.
             * @param verbose Flag indicating verbose log output.
             */
            static void setVerbose(bool verbose) { m_verbose = verbose; }

            /** @name Local Site data */
            /**
             * @brief Sets the callsign.
             * @param callsign Callsign.
             */
            static void setCallsign(std::string callsign);

            /**
             * @brief Gets the local site data.
             * @returns SiteData Currently set site data for the RCCH class.
             */
            static SiteData getSiteData() { return m_siteData; }
            /**
             * @brief Sets the local site data.
             * @param siteData Site data to set for the RCCH class.
             */
            static void setSiteData(SiteData siteData) { m_siteData = siteData; }
            /** @} */

        public:
            /** @name Common Data */
            /**
             * @brief Message Type
             */
            DECLARE_PROTECTED_PROPERTY(uint8_t, messageType, MessageType);

            /**
             * @brief Source ID.
             */
            DECLARE_PROTECTED_PROPERTY(uint16_t, srcId, SrcId);
            /**
             * @brief Destination ID.
             */
            DECLARE_PROTECTED_PROPERTY(uint16_t, dstId, DstId);

            /**
             * @brief Location ID.
             */
            DECLARE_PROTECTED_PROPERTY(uint32_t, locId, LocId);
            /**
             * @brief Registration Option.
             */
            DECLARE_PROTECTED_PROPERTY(uint8_t, regOption, RegOption);

            /**
             * @brief Version Number.
             */
            DECLARE_PROTECTED_PROPERTY(uint8_t, version, Version);

            /**
             * @brief Cause Response.
             */
            DECLARE_PROTECTED_PROPERTY(uint8_t, causeRsp, CauseResponse);

            /**
             * @brief Voice channel number.
             */
            DECLARE_PROTECTED_PROPERTY(uint32_t, grpVchNo, GrpVchNo);
            /** @} */

            /** @name Call Data */
            /**
             * @brief Call Type
             */
            DECLARE_PROTECTED_PROPERTY(uint8_t, callType, CallType);
            /** @} */

            /** @name Common Call Options */
            /**
             * @brief Flag indicating the emergency bits are set.
             */
            DECLARE_PROTECTED_PROPERTY(bool, emergency, Emergency);
            /**
             * @brief Flag indicating that encryption is enabled.
             */
            DECLARE_PROTECTED_PROPERTY(bool, encrypted, Encrypted);
            /**
             * @brief Flag indicating priority paging.
             */
            DECLARE_PROTECTED_PROPERTY(bool, priority, Priority);
            /**
             * @brief Flag indicating a group/talkgroup operation.
             */
            DECLARE_PROTECTED_PROPERTY(bool, group, Group);
            /**
             * @brief Flag indicating a half/full duplex operation.
             */
            DECLARE_PROTECTED_PROPERTY(bool, duplex, Duplex);

            /**
             * @brief Transmission mode.
             */
            DECLARE_PROTECTED_PROPERTY(uint8_t, transmissionMode, TransmissionMode);
            /** @} */

            /** @name Local Site data */
            /**
             * @brief Local Site Identity Entry.
             */
            DECLARE_PROTECTED_PROPERTY_PLAIN(lookups::IdenTable, siteIdenEntry);
            /** @} */

        protected:
            static bool m_verbose;

            // Local Site data
            static uint8_t* m_siteCallsign;
            static SiteData m_siteData;

            /**
             * @brief Internal helper to decode a RCCH link control message.
             * @param[in] data Buffer containing a RCCH to decode.
             * @param[out] rcch
             * @param length Length of data buffer.
             * @param offset Offset for RCCH in data buffer.
             */
            void decode(const uint8_t* data, uint8_t* rcch, uint32_t length, uint32_t offset = 0U);
            /**
             * @brief Internal helper to encode a RCCH link control message.
             * @param[out] data Buffer to encode a RCCH.
             * @param[in] rcch
             * @param length Length of data buffer.
             * @param offset Offset for RCCH in data buffer.
             */
            void encode(uint8_t* data, const uint8_t* rcch, uint32_t length, uint32_t offset = 0U);

            DECLARE_PROTECTED_COPY(RCCH);
        };
    } // namespace lc
} // namespace nxdn

#endif // __NXDN_LC__RCCH_H__
