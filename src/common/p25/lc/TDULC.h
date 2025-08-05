// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2017-2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file TDULC.h
 * @ingroup p25_lc
 * @file TDULC.cpp
 * @ingroup p25_lc
 */
#if !defined(__P25_LC__TDULC_H__)
#define  __P25_LC__TDULC_H__

#include "common/Defines.h"
#include "common/p25/P25Defines.h"
#include "common/p25/lc/LC.h"
#include "common/p25/lc/TSBK.h"
#include "common/p25/SiteData.h"
#include "common/edac/RS634717.h"
#include "common/lookups/IdenTableLookup.h"
#include "common/Utils.h"

#include <string>

namespace p25
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Prototypes
        // ---------------------------------------------------------------------------

        class HOST_SW_API LC;
        class HOST_SW_API TSBK;

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents link control data for TDULC packets.
         * @ingroup p25_lc
         */
        class HOST_SW_API TDULC {
        public:
            /**
             * @brief Initializes a copy instance of the TDULC class.
             * @param data Instance of TDULC to copy.
             */
            TDULC(const TDULC& data);
            /**
             * @brief Initializes a new instance of the TDULC class.
             * @param lc Instance of LC to derive a TDULC from.
             */
            TDULC(LC* lc);
            /**
             * @brief Initializes a new instance of the TDULC class.
             */
            TDULC();
            /**
             * @brief Finalizes a instance of the TDULC class.
             */
            virtual ~TDULC();

            /**
             * @brief Decode a terminator data unit w/ link control.
             * @param[in] data Buffer containing a TDULC to decode.
             * @returns bool True, if TDULC decoded, otherwise false.
             */
            virtual bool decode(const uint8_t* data) = 0;
            /**
             * @brief Encode a terminator data unit w/ link control.
             * @param[out] data Buffer to encode a TDULC.
             */
            virtual void encode(uint8_t* data) = 0;

            /**
             * @brief Returns a copy of the raw decoded TDULC bytes.
             * This will only return data for a *decoded* TDULC, not a created or copied TDULC.
             * @returns uint8_t* Raw decoded TDULC bytes.
             */
            uint8_t* getDecodedRaw() const;

            /**
             * @brief Sets the flag indicating verbose log output.
             * @param verbose Flag indicating verbose log output.
             */
            static void setVerbose(bool verbose) { m_verbose = verbose; }

            /** @name Local Site data */
            /**
             * @brief Gets the local site data.
             * @returns SiteData Currently set site data for the TDULC class.
             */
            static SiteData getSiteData() { return m_siteData; }
            /**
             * @brief Sets the local site data.
             * @param siteData Site data to set for the TDULC class.
             */
            static void setSiteData(SiteData siteData) { m_siteData = siteData; }
            /** @} */

        public:
            /** @name Common Data */
            /**
             * @brief Flag indicating the link control data is protected.
             */
            DECLARE_PROTECTED_PROPERTY(bool, protect, Protect);
            /**
             * @brief Link control opcode.
             */
            DECLARE_PROTECTED_PROPERTY(uint8_t, lco, LCO);
            /**
             * @brief Manufacturer ID.
             */
            DECLARE_PROTECTED_PROPERTY(uint8_t, mfId, MFId);

            /**
             * @brief Source ID.
             */
            DECLARE_PROTECTED_PROPERTY(uint32_t, srcId, SrcId);
            /**
             * @brief Destination ID.
             */
            DECLARE_PROTECTED_PROPERTY(uint32_t, dstId, DstId);

            /**
             * @brief Voice channel number.
             */
            DECLARE_PROTECTED_PROPERTY(uint32_t, grpVchNo, GrpVchNo);
            /** @} */

            /** @name Service Options */
            /**
             * @brief Flag indicating the emergency bits are set.
             */
            DECLARE_PROTECTED_PROPERTY(bool, emergency, Emergency);
            /**
             * @brief Flag indicating that encryption is enabled.
             */
            DECLARE_PROTECTED_PROPERTY(bool, encrypted, Encrypted);
            /**
             * @brief Priority level for the traffic.
             */
            DECLARE_PROTECTED_PROPERTY(uint8_t, priority, Priority);
            /**
             * @brief Flag indicating a group/talkgroup operation.
             */
            DECLARE_PROTECTED_PROPERTY(bool, group, Group);
            /** @} */

            /** @name Local Site data */
            /**
             * @brief Local Site Identity Entry.
             */
            DECLARE_PROTECTED_PROPERTY_PLAIN(::lookups::IdenTable, siteIdenEntry);
            /** @} */

        protected:
            friend class LC;
            friend class TSBK;
            edac::RS634717 m_rs;

            bool m_implicit;
            uint32_t m_callTimer;

            static bool m_verbose;

            // Local Site data
            static SiteData m_siteData;

            /**
             * @brief Internal helper to convert payload bytes to a 64-bit long value.
             * @param[in] payload Buffer containing payload to convert.
             * @returns ulong64_t 64-bit packed value containing the buffer.
             */
            static ulong64_t toValue(const uint8_t* payload);
            /**
             * @brief Internal helper to convert a 64-bit long value to payload bytes.
             * @param[in] value 64-bit packed value.
             * @returns UInt8Array Buffer containing the unpacked payload.
             */
            static UInt8Array fromValue(const ulong64_t value);

            /**
             * @brief Internal helper to decode terminator data unit w/ link control.
             * @param[in] data Raw data.
             * @param[out] payload TDULC payload buffer.
             * @param rawTDULC Flag indicating whether or not the passed buffer is raw.
             */
            bool decode(const uint8_t* data, uint8_t* payload, bool rawTDULC = false);
            /**
             * @brief Internal helper to encode terminator data unit w/ link control.
             * @param[out] data Raw data.
             * @param[in] payload TDULC payload buffer.
             * @param rawTDULC Flag indicating whether or not the passed buffer is raw.
             */
            void encode(uint8_t* data, const uint8_t* payload, bool rawTDULC = false);

            DECLARE_PROTECTED_COPY(TDULC);

        private:
            uint8_t* m_raw;
        };
    } // namespace lc
} // namespace p25

#endif // __P25_LC__TDULC_H__
