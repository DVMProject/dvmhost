// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file TSBK.h
 * @ingroup p25_lc
 * @file TSBK.cpp
 * @ingroup p25_lc
 */
#if !defined(__P25_LC__TSBK_H__)
#define  __P25_LC__TSBK_H__

#include "common/Defines.h"
#include "common/edac/Trellis.h"
#include "common/p25/lc/LC.h"
#include "common/p25/lc/TDULC.h"
#include "common/p25/SiteData.h"
#include "common/edac/RS634717.h"
#include "common/lookups/IdenTableLookup.h"
#include "common/Utils.h"

#include <string>

namespace p25
{
    namespace dfsi
    {
        // ---------------------------------------------------------------------------
        //  Class Prototypes
        // ---------------------------------------------------------------------------

        class HOST_SW_API LC;
    }

    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Prototypes
        // ---------------------------------------------------------------------------

        class HOST_SW_API LC;
        class HOST_SW_API TDULC;

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents link control data for TSDU packets.
         * @ingroup p25_lc
         */
        class HOST_SW_API TSBK {
        public:
            /**
             * @brief Initializes a copy instance of the TSBK class.
             * @param data Instance of TSBK to copy.
             */
            TSBK(const TSBK& data);
            /**
             * @brief Initializes a new instance of the TSBK class.
             * @param lc Instance of LC to derive a TSBK from.
             */
            TSBK(LC* lc);
            /**
             * @brief Initializes a new instance of the TSBK class.
             */
            TSBK();
            /**
             * @brief Finalizes a instance of the TSBK class.
             */
            virtual ~TSBK();

            /**
             * @brief Decode a trunking signalling block.
             * @param[in] data Buffer containing a TSBK to decode.
             * @param rawTSBK Flag indicating whether or not the passed buffer is raw.
             * @returns bool True, if TSBK decoded, otherwise false.
             */
            virtual bool decode(const uint8_t* data, bool rawTSBK = false) = 0;
            /**
             * @brief Encode a trunking signalling block.
             * @param[out] data Buffer to encode a TSBK.
             * @param rawTSBK Flag indicating whether or not the output buffer is raw.
             * @param noTrellis Flag indicating whether or not the encoded data should be Trellis encoded.
             */
            virtual void encode(uint8_t* data, bool rawTSBK = false, bool noTrellis = false) = 0;

            /**
             * @brief Returns a string that represents the current TSBK.
             * @returns std::string String representation of the TSBK.
             */
            virtual std::string toString(bool isp = false);

            /**
             * @brief Returns a copy of the raw decoded TSBK bytes.
             * This will only return data for a *decoded* TSBK, not a created or copied TSBK.
             * @returns uint8_t* Raw decoded TSBK bytes.
             */
            uint8_t* getDecodedRaw() const;

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
            /**
             * @brief Sets the flag indicating CRC-errors should be warnings and not errors.
             * @param warnCRC Flag indicating CRC-errors should be treated as warnings.
             */
            static void setWarnCRC(bool warnCRC) { m_warnCRC = warnCRC; }

            /** @name Local Site data */
            /**
             * @brief Sets the callsign.
             * @param callsign Callsign.
             */
            static void setCallsign(std::string callsign);

            /**
             * @brief Gets the local site data.
             * @returns SiteData Currently set site data for the TSBK class.
             */
            static SiteData getSiteData() { return m_siteData; }
            /**
             * @brief Sets the local site data.
             * @param siteData Site data to set for the TSBK class.
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
             * @brief Flag indicating this is the last TSBK in a sequence of TSBKs.
             */
            DECLARE_PROTECTED_PROPERTY(bool, lastBlock, LastBlock);
            /**
             * @brief Flag indicating this TSBK contains additional information.
             */
            DECLARE_PROTECTED_PROPERTY(bool, aivFlag, AIV);
            /**
             * @brief Flag indicating this TSBK contains extended addressing.
             */
            DECLARE_PROTECTED_PROPERTY(bool, extendedAddrFlag, EX);

            /**
             * @brief Service type.
             */
            DECLARE_PROTECTED_PROPERTY(uint8_t, service, Service);
            /**
             * @brief Response type.
             */
            DECLARE_PROTECTED_PROPERTY(uint8_t, response, Response);

            /**
             * @brief Configured network ID.
             */
            DECLARE_PROTECTED_RO_PROPERTY(uint32_t, netId, NetId);
            /**
             * @brief Configured system ID.
             */
            DECLARE_PROTECTED_RO_PROPERTY(uint32_t, sysId, SysId);

            /**
             * @brief Voice channel ID.
             */
            DECLARE_PROTECTED_PROPERTY(uint8_t, grpVchId, GrpVchId);
            /**
             * @brief Voice channel number.
             */
            DECLARE_PROTECTED_PROPERTY(uint32_t, grpVchNo, GrpVchNo);
            /** @} */

            /** @name Common Service Options */
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
            friend class dfsi::LC;
            friend class LC;
            friend class TDULC;
            edac::RS634717 m_rs;
            edac::Trellis m_trellis;

            static bool m_verbose;
            static bool m_warnCRC;

            // Local Site data
            static uint8_t* m_siteCallsign;
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
             * @brief Internal helper to decode a trunking signalling block.
             * @param[in] data Raw data.
             * @param[out] payload TSBK payload buffer.
             * @param rawTSBK Flag indicating whether or not the passed buffer is raw.
             * @returns bool True, if TSBK decoded, otherwise false.
             */
            bool decode(const uint8_t* data, uint8_t* payload, bool rawTSBK = false);
            /**
             * @brief Internal helper to encode a trunking signalling block.
             * @param[out] data Raw data.
             * @param[in] payload TSBK payload buffer.
             * @param rawTSBK Flag indicating whether or not the output buffer is raw.
             * @param noTrellis Flag indicating whether or not the encoded data should be Trellis encoded.
             */
            void encode(uint8_t* data, const uint8_t* payload, bool rawTSBK = false, bool noTrellis = false);

            DECLARE_PROTECTED_COPY(TSBK);

        private:
            uint8_t* m_raw;
        };
    } // namespace lc
} // namespace p25

#endif // __P25_LC__TSBK_H__
