// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
*  Copyright (C) 2019-2024 Bryan Biedenkapp, N2PLL
*
*/
/**
 * @file CSBK.h
 * @ingroup dmr_lc
 * @file CSBK.cpp
 * @ingroup dmr_lc
 */
#if !defined(__DMR_LC__CSBK_H__)
#define __DMR_LC__CSBK_H__

#include "common/Defines.h"
#include "common/dmr/DMRDefines.h"
#include "common/dmr/SiteData.h"
#include "common/lookups/IdenTableLookup.h"
#include "common/Utils.h"

namespace dmr
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents DMR control signalling block data.
         * @ingroup dmr_lc
         */
        class HOST_SW_API CSBK {
        public:
            /**
             * @brief Initializes a copy instance of the CSBK class.
             * @param data Instance of CSBK to copy.
             */
            CSBK(const CSBK& data);
            /**
             * @brief Initializes a new instance of the CSBK class.
             */
            CSBK();
            /**
             * @brief Finalizes a instance of the CSBK class.
             */
            virtual ~CSBK();

            /**
             * @brief Decodes a DMR CSBK.
             * @param[in] data Buffer containing a CSBK to decode.
             */
            virtual bool decode(const uint8_t* data) = 0;
            /**
             * @brief Encodes a DMR CSBK.
             * @param[out] data Buffer to encode a CSBK.
             */
            virtual void encode(uint8_t* data) = 0;

            /**
             * @brief Returns a string that represents the current CSBK.
             * @returns std::string String representation of the CSBK.
             */
            virtual std::string toString();

            /**
             * @brief Returns a copy of the raw decoded CSBK bytes.
             * This will only return data for a *decoded* CSBK, not a created or copied CSBK.
             * @returns uint8_t* Raw decoded CSBK bytes.
             */
            uint8_t* getDecodedRaw() const;

            /**
             * @brief Regenerate a DMR CSBK without decoding.
             *  This is because the DMR architecture allows fall-thru of unsupported CSBKs.
             * @param data Buffer containing DMR CSBK to regenerate.
             * @param dataType Data Type
             * @returns bool True, if CSBK is regenerated, otherwise false.
             */
            static bool regenerate(uint8_t* data, uint8_t dataType);

            /**
             * @brief Gets the flag indicating verbose log output.
             * @returns bool True, if the CSBK is verbose logging, otherwise false.
             */
            static bool getVerbose() { return m_verbose; }
            /**
             * @brief Sets the flag indicating verbose log output.
             * @param verbose Flag indicating verbose log output.
             */
            static void setVerbose(bool verbose) { m_verbose = verbose; }

            /** @name Local Site data */
            /**
             * @brief Gets the local site data.
             * @returns SiteData Currently set site data for the CSBK class.
             */
            static SiteData getSiteData() { return m_siteData; }
            /**
             * @brief Sets the local site data.
             * @param siteData Site data to set for the CSBK class.
             */
            static void setSiteData(SiteData siteData) { m_siteData = siteData; }
            /** @} */

        public:
            /** @name Common Data */
            /**
             * @brief DMR access color code.
             */
            __PROTECTED_PROPERTY(uint8_t, colorCode, ColorCode);

            /**
             * @brief Flag indicating this is the last CSBK in a sequence of CSBKs.
             */
            __PROTECTED_PROPERTY(bool, lastBlock, LastBlock);
            /**
             * @brief Flag indicating whether the CSBK is a Cdef block.
             */
            __PROTECTED_PROPERTY(bool, Cdef, Cdef);

            /**
             * @brief CSBK opcode.
             */
            __PROTECTED_PROPERTY(uint8_t, CSBKO, CSBKO);
            /**
             * @brief             */
            __PROTECTED_PROPERTY(uint8_t, FID, FID);

            /**
             * @brief Flag indicating whether the CSBK is group or individual.
             */
            __PROTECTED_PROPERTY(bool, GI, GI);

            /**
             * @brief Source ID.
             */
            __PROTECTED_PROPERTY(uint32_t, srcId, SrcId);
            /**
             * @brief Destination ID.
             */
            __PROTECTED_PROPERTY(uint32_t, dstId, DstId);

            /**
             * @brief 
             */
            __PROTECTED_READONLY_PROPERTY(bool, dataContent, DataContent);

            /**
             * @brief Number of blocks to follow.
             */
            __PROTECTED_PROPERTY(uint8_t, CBF, CBF);

            /**
             * @brief Data type for this CSBK.
             */
            __PROTECTED_PROPERTY(defines::DataType::E, dataType, DataType);
            /** @} */

            /** @name Common Service Options */
            /**
             * @brief Flag indicating the emergency bits are set.
             */
            __PROTECTED_PROPERTY(bool, emergency, Emergency);
            /**
             * @brief Flag indicating that privacy is enabled.
             */
            __PROTECTED_PROPERTY(bool, privacy, Privacy);
            /**
             * @brief Flag indicating that supplementary data is required.
             */
            __PROTECTED_PROPERTY(bool, supplementData, SupplementData);
            /**
             * @brief Priority level for the traffic.
             */
            __PROTECTED_PROPERTY(uint8_t, priority, Priority);
            /**
             * @brief Flag indicating a broadcast service.
             */
            __PROTECTED_PROPERTY(bool, broadcast, Broadcast);
            /**
             * @brief Flag indicating a proxy.
             */
            __PROTECTED_PROPERTY(bool, proxy, Proxy);

            /**
             * @brief Response information.
             */
            __PROTECTED_PROPERTY(uint8_t, response, Response);
            /**
             * @brief Reason type.
             */
            __PROTECTED_PROPERTY(uint8_t, reason, Reason);
            /** @} */

            /** @name Tier 3 */
            /**
             * @brief Site offset timing.
             */
            __PROTECTED_PROPERTY(bool, siteOffsetTiming, SiteOffsetTiming);

            /**
             * @brief Broadcast Logical Channel ID 1.
             */
            __PROTECTED_PROPERTY(uint16_t, logicalCh1, LogicalCh1);
            /**
             * @brief Broadcast Logical Channel ID 2.
             */
            __PROTECTED_PROPERTY(uint16_t, logicalCh2, LogicalCh2);
            /**
             * @brief Logical Channel Slot Number.
             */
            __PROTECTED_PROPERTY(uint8_t, slotNo, SlotNo);
            /** @} */

            /** @name Local Site data */
            /**
             * @brief Local Site Identity Entry.
             */
            __PROTECTED_PROPERTY_PLAIN(::lookups::IdenTable, siteIdenEntry);
            /** @} */

        protected:
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
             * @brief Internal helper to decode a control signalling block.
             * @param[in] data Raw data.
             * @param[out] payload CSBK payload buffer.
             */
            bool decode(const uint8_t* data, uint8_t* payload);
            /**
             * @brief Internal helper to encode a control signalling block.
             * @param[out] data Raw data.
             * @param[in] payload CSBK payload buffer.
             */
            void encode(uint8_t* data, const uint8_t* payload);

            __PROTECTED_COPY(CSBK);

        private:
            uint8_t* m_raw;
        };
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC__CSBK_H__
