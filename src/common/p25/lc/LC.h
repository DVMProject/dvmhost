// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup p25_lc Link Control
 * @brief Implementation for the data handling of the TIA-102.BAAA Project 25 standard.
 * @ingroup p25
 * 
 * @file LC.h
 * @ingroup p25_lc
 * @file LC.cpp
 * @ingroup p25_lc
 */
#if !defined(__P25_LC__LC_H__)
#define  __P25_LC__LC_H__

#include "common/Defines.h"
#include "common/p25/lc/TSBK.h"
#include "common/p25/lc/TDULC.h"
#include "common/p25/SiteData.h"
#include "common/edac/RS634717.h"

#include <string>

namespace p25
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Prototypes
        // ---------------------------------------------------------------------------

        class HOST_SW_API TSBK;
        class HOST_SW_API TDULC;

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents link control data for HDU, LDU1 and 2 packets.
         * @ingroup p25_lc
         */
        class HOST_SW_API LC {
        public:
            /**
             * @brief Initializes a copy instance of the LC class.
             * @param data Instance of LC class to copy from.
             */
            LC(const LC& data);
            /**
             * @brief Initializes a new instance of the LC class.
             */
            LC();
            /**
             * @brief Finalizes a instance of the LC class.
             */
            ~LC();

            /**
             * @brief Equals operator.
             * @param data Instance of LC class to copy from.
             */
            LC& operator=(const LC& data);

            /**
             * @brief Decode a header data unit.
             * @param[in] data Buffer containing the HDU to decode.
             * @returns True, if HDU decoded, otherwise false.
             */
            bool decodeHDU(const uint8_t* data);
            /**
             * @brief Encode a header data unit.
             * @param[out] data Buffer to encode an HDU.
             */
            void encodeHDU(uint8_t* data);

            /**
             * @brief Decode a logical link data unit 1.
             * @param[in] data Buffer containing an LDU1 to decode.
             * @returns True, if LDU1 decoded, otherwise false.
             */
            bool decodeLDU1(const uint8_t* data);
            /**
             * @brief Encode a logical link data unit 1.
             * @param[out] data Buffer to encode an LDU1.
             */
            void encodeLDU1(uint8_t* data);

            /**
             * @brief Decode a logical link data unit 2.
             * @param[in] data Buffer containing an LDU2 to decode.
             * @returns True, if LDU2 decoded, otherwise false.
             */
            bool decodeLDU2(const uint8_t* data);
            /**
             * @brief Encode a logical link data unit 2.
             * @param[out] data Buffer to encode an LDU2.
             */
            void encodeLDU2(uint8_t* data);

            /**
             * @brief Helper to determine if the MFId is a standard MFId.
             * @returns bool True, if the MFId contained for this LC is standard, otherwise false.
             */
            bool isStandardMFId() const;
            /**
             * @brief Helper to determine if we should utilize the raw RS data from the decode.
             * @returns bool True, if the raw LC value should be used, otherwise false.
             */
            bool isDemandUseRawLC() const { return m_demandUseRawLC; }

            /** @name Encryption data */
            /**
             * @brief Sets the encryption message indicator.
             * @param[in] mi Buffer containing the 9-byte Message Indicator.
             */
            void setMI(const uint8_t* mi);
            /**
             * @brief Gets the encryption message indicator.
             * @param[out] mi Buffer containing the 9-byte Message Indicator.
             */
            void getMI(uint8_t* mi) const;
            /** @} */

            /** @name Local Site data */
            /**
             * @brief Gets the local site data.
             * @returns SiteData Currently set site data for the LC class.
             */
            static SiteData getSiteData() { return m_siteData; }
            /**
             * @brief Sets the local site data.
             * @param siteData Site data to set for the LC class.
             */
            static void setSiteData(SiteData siteData) { m_siteData = siteData; }
            /** @} */

        public:
            /** @name Common Data */
            /**
             * @brief Flag indicating the link control data is protected.
             */
            __PROPERTY(bool, protect, Protect);
            /**
             * @brief Link control opcode.
             */
            __PROPERTY(uint8_t, lco, LCO);
            /**
             * @brief Manufacturer ID.
             */
            __PROPERTY(uint8_t, mfId, MFId);

            /**
             * @brief Source ID.
             */
            __PROPERTY(uint32_t, srcId, SrcId);
            /**
             * @brief Destination ID.
             */
            __PROPERTY(uint32_t, dstId, DstId);

            /**
             * @brief Voice channel number.
             */
            __PROPERTY(uint32_t, grpVchNo, GrpVchNo);

            /**
             * @brief Voice channel number.
             */
            __PROPERTY(uint32_t, grpVchNoB, GrpVchNoB);
            /**
             * @brief Destination ID.
             */
            __PROPERTY(uint32_t, dstIdB, DstIdB);

            /**
             * @brief Flag indicating explicit addressing.
             */
            __PROPERTY(bool, explicitId, ExplicitId);

            /**
             * @brief Network ID.
             */
            __PROPERTY(uint32_t, netId, NetId);
            /**
             * @brief System ID.
             */
            __PROPERTY(uint32_t, sysId, SysId);
            /** @} */

            /** @name Service Options */
            /**
             * @brief Flag indicating the emergency bits are set.
             */
            __PROPERTY(bool, emergency, Emergency);
            /**
             * @brief Flag indicating that encryption is enabled.
             */
            __PROPERTY(bool, encrypted, Encrypted);
            /**
             * @brief Priority level for the traffic.
             */
            __PROPERTY(uint8_t, priority, Priority);
            /**
             * @brief Flag indicating a group/talkgroup operation.
             */
            __PROPERTY(bool, group, Group);
            /** @} */

            /** @name Encryption data */
            /**
             * @brief Encryption algorithm ID.
             */
            __PROPERTY(uint8_t, algId, AlgId);
            /**
             * @brief Encryption key ID.
             */
            __PROPERTY(uint32_t, kId, KId);
            /** @} */

            /** @name Packed RS Data */
            /**
             * @brief Packed RS Data.
             */
            __PROPERTY(ulong64_t, rsValue, RS);
            /** @} */

        private:
            friend class TSBK;
            friend class TDULC;
            edac::RS634717 m_rs;
            bool m_encryptOverride;
            bool m_tsbkVendorSkip;
            bool m_demandUseRawLC;

            uint32_t m_callTimer;

            // Encryption data
            uint8_t* m_mi;

            // Local Site data
            static SiteData m_siteData;

            /**
             * @brief Internal helper to copy the class.
             */
            void copy(const LC& data);

            /**
             * @brief Decode link control.
             * @param[in] rs Buffer containing the decoded Reed-Solomon LC data.
             * @returns bool True, if LC is decoded, otherwise false.
             */
            bool decodeLC(const uint8_t* rs);
            /**
             * @brief Encode link control.
             * @param[out] rs Buffer to encode LC data.
             */
            void encodeLC(uint8_t* rs);

            /**
             * @brief Decode LDU hamming FEC.
             * @param[in] raw 
             * @param[out] data 
             */
            void decodeLDUHamming(const uint8_t* raw, uint8_t* data);
            /**
             * @brief Encode LDU hamming FEC.
             * @param[out] data 
             * @param[in] raw 
             */
            void encodeLDUHamming(uint8_t* data, const uint8_t* raw);

            /**
             * @brief Decode HDU Golay FEC.
             * @param[in] raw 
             * @param[out] data 
             */
            void decodeHDUGolay(const uint8_t* raw, uint8_t* data);
            /**
             * @brief Encode HDU Golay FEC.
             * @param[out] data 
             * @param[in] raw 
             */
            void encodeHDUGolay(uint8_t* data, const uint8_t* raw);
        };
    } // namespace lc
} // namespace p25

#endif // __P25_LC__LC_H__
