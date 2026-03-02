// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2026 Bryan Biedenkapp, N2PLL
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

            /** Project 25 Phase I CAI (TIA-102.BAAA-B Section 4.2, 4.5) */

            /**
             * @brief Decode a header data unit.
             * @param[in] data Buffer containing the HDU to decode.
             * @param rawOnly Flag indicating only the raw bytes of the LC should be decoded.
             * @returns True, if HDU decoded, otherwise false.
             */
            bool decodeHDU(const uint8_t* data, bool rawOnly = false);
            /**
             * @brief Encode a header data unit.
             * @param[out] data Buffer to encode an HDU.
             * @param rawOnly Flag indicating only the raw bytes of the LC should be encoded.
             */
            void encodeHDU(uint8_t* data, bool rawOnly = false);

            /**
             * @brief Decode a logical link data unit 1.
             * @param[in] data Buffer containing an LDU1 to decode.
             * @param rawOnly Flag indicating only the raw bytes of the LC should be decoded.
             * @returns bool True, if LDU1 decoded, otherwise false.
             */
            bool decodeLDU1(const uint8_t* data, bool rawOnly = false);
            /**
             * @brief Encode a logical link data unit 1.
             * @param[out] data Buffer to encode an LDU1.
             */
            void encodeLDU1(uint8_t* data);

            /**
             * @brief Decode a logical link data unit 2.
             * @param[in] data Buffer containing an LDU2 to decode.
             * @returns bool  True, if LDU2 decoded, otherwise false.
             */
            bool decodeLDU2(const uint8_t* data);
            /**
             * @brief Encode a logical link data unit 2.
             * @param[out] data Buffer to encode an LDU2.
             */
            void encodeLDU2(uint8_t* data);

            /** Project 25 Phase II (TIA-102.BBAD-D Section 2) */

            /**
             * @brief Decode a IEMI VCH MAC PDU.
             * @param data Buffer containing the MAC PDU to decode.
             * @param sync Flag indicating if sync is included (true=276 bits with RS, false=312 bits no RS).
             * @return bool True, if MAC PDU decoded, otherwise false.
             */
            bool decodeVCH_MACPDU_IEMI(const uint8_t* data, bool sync);
            /**
             * @brief Decode a xOEMI VCH MAC PDU.
             * @param data Buffer containing the MAC PDU to decode.
             * @param sync Flag indicating if sync is included.
             * @return bool True, if MAC PDU decoded, otherwise false.
             */
            bool decodeVCH_MACPDU_OEMI(const uint8_t* data, bool sync);
            /**
             * @brief Encode a VCH MAC PDU.
             * @param[out] data Buffer to encode a MAC PDU.
             * @param sync Flag indicating if sync is to be included.
             */
            void encodeVCH_MACPDU(uint8_t* data, bool sync);

            /**
             * @brief Helper to determine if the MFId is a standard MFId.
             * @returns bool True, if the MFId contained for this LC is standard, otherwise false.
             */
            bool isStandardMFId() const;

            /**
             * @brief Decode link control.
             * @param[in] rs Buffer containing the decoded Reed-Solomon LC data.
             * @param rawOnly Flag indicating only the raw bytes of the LC should be decoded.
             * @returns bool True, if LC is decoded, otherwise false.
             */
            bool decodeLC(const uint8_t* rs, bool rawOnly = false);
            /**
             * @brief Encode link control.
             * @param[out] rs Buffer to encode LC data.
             */
            void encodeLC(uint8_t* rs);

            /**
             * @brief Decode MAC PDU.
             * @param[in] raw Buffer containing the decoded Reed-Solomon MAC PDU data.
             * @param macLength MAC PDU length in bits (156 for IEMI/S-OEMI, 180 for I-OEMI).
             * @returns bool True, if MAC PDU is decoded, otherwise false.
             */
            bool decodeMACPDU(const uint8_t* raw, uint32_t macLength = defines::P25_P2_IOEMI_MAC_LENGTH_BITS);
            /**
             * @brief Encode MAC PDU.
             * @param[out] raw Buffer to encode MAC PDU data.
             * @param macLength MAC PDU length in bits (156 for IEMI/S-OEMI, 180 for I-OEMI).
             */
            void encodeMACPDU(uint8_t* raw, uint32_t macLength = defines::P25_P2_IOEMI_MAC_LENGTH_BITS);

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

            /** @name User Alias data */
            /**
             * @brief Gets the user alias.
             * @returns std::string User Alias.
             */
            std::string getUserAlias() const;
            /**
             * @brief Sets the user alias.
             * @param alias User alias.
             */
            void setUserAlias(std::string alias);

            /** @name Local Site data */
            /**
             * @brief Gets the local site data.
             * @returns SiteData Currently set site data for the LC class.
             */
            static SiteData getSiteData() { return s_siteData; }
            /**
             * @brief Sets the local site data.
             * @param siteData Site data to set for the LC class.
             */
            static void setSiteData(SiteData siteData) { s_siteData = siteData; }
            /** @} */

            /**
             * @brief Sets the flag indicating CRC-errors should be warnings and not errors.
             * @param warnCRC Flag indicating CRC-errors should be treated as warnings.
             */
            static void setWarnCRC(bool warnCRC) { s_warnCRC = warnCRC; }

        public:
            /** @name Common Data */
            /**
             * @brief Flag indicating the link control data is protected.
             */
            DECLARE_PROPERTY(bool, protect, Protect);
            /**
             * @brief Link control opcode.
             */
            DECLARE_PROPERTY(uint8_t, lco, LCO);
            /**
             * @brief Manufacturer ID.
             */
            DECLARE_PROPERTY(uint8_t, mfId, MFId);

            /**
             * @brief Source ID.
             */
            DECLARE_PROPERTY(uint32_t, srcId, SrcId);
            /**
             * @brief Destination ID.
             */
            DECLARE_PROPERTY(uint32_t, dstId, DstId);

            /**
             * @brief Voice channel number.
             */
            DECLARE_PROPERTY(uint32_t, grpVchNo, GrpVchNo);

            /**
             * @brief Voice channel number.
             */
            DECLARE_PROPERTY(uint32_t, grpVchNoB, GrpVchNoB);
            /**
             * @brief Destination ID.
             */
            DECLARE_PROPERTY(uint32_t, dstIdB, DstIdB);

            /**
             * @brief Flag indicating explicit addressing.
             */
            DECLARE_PROPERTY(bool, explicitId, ExplicitId);

            /**
             * @brief Network ID.
             */
            DECLARE_PROPERTY(uint32_t, netId, NetId);
            /**
             * @brief System ID.
             */
            DECLARE_PROPERTY(uint32_t, sysId, SysId);
            /** @} */

            /** @name Service Options */
            /**
             * @brief Flag indicating the emergency bits are set.
             */
            DECLARE_PROPERTY(bool, emergency, Emergency);
            /**
             * @brief Flag indicating that encryption is enabled.
             */
            DECLARE_PROPERTY(bool, encrypted, Encrypted);
            /**
             * @brief Priority level for the traffic.
             */
            DECLARE_PROPERTY(uint8_t, priority, Priority);
            /**
             * @brief Flag indicating a group/talkgroup operation.
             */
            DECLARE_PROPERTY(bool, group, Group);
            /** @} */

            /** @name Encryption data */
            /**
             * @brief Encryption algorithm ID.
             */
            DECLARE_PROPERTY(uint8_t, algId, AlgId);
            /**
             * @brief Encryption key ID.
             */
            DECLARE_PROPERTY(uint32_t, kId, KId);
            /** @} */

            /** @name Phase 2 Data */
            /**
             * @brief Slot Number.
             */
            DECLARE_PROPERTY(uint8_t, slotNo, SlotNo);

            /**
             * @brief Phase 2 DUID.
             */
            DECLARE_PROPERTY(uint8_t, p2DUID, P2DUID);
            /**
             * @brief Color Code.
             */
            DECLARE_PROPERTY(uint16_t, colorCode, ColorCode);
            /**
             * @brief MAC PDU Opcode.
             */
            DECLARE_PROPERTY(uint8_t, macPduOpcode, MACPDUOpcode);
            /**
             * @brief MAC PDU SACCH Offset.
             */
            DECLARE_PROPERTY(uint8_t, macPduOffset, MACPDUOffset);
            /**
             * @brief MAC Partition.
             */
            DECLARE_PROPERTY(uint8_t, macPartition, MACPartition);
            /** @} */

            /** @name Packed RS Data */
            /**
             * @brief Packed RS Data.
             */
            DECLARE_PROPERTY(ulong64_t, rsValue, RS);
            /** @} */

            /** @name Phase 2 Raw MCO Data */
            uint8_t* p2MCOData; // ?? - this should probably be private with getters/setters
            /** @} */

        private:
            friend class TSBK;
            friend class TDULC;
            edac::RS634717 m_rs;
            bool m_encryptOverride;
            bool m_tsbkVendorSkip;

            uint32_t m_callTimer;

            // Encryption data
            uint8_t* m_mi;

            // User Alias data
            uint8_t* m_userAlias;
            bool m_gotUserAliasPartA;
            bool m_gotUserAlias;

            static bool s_warnCRC;

            // Local Site data
            static SiteData s_siteData;

            /**
             * @brief Internal helper to copy the class.
             */
            void copy(const LC& data);

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

            /**
             * @brief Decode Phase 2 DUID hamming FEC.
             * @param[in] raw 
             * @param[out] data 
             */
            void decodeP2_DUIDHamming(const uint8_t* raw, uint8_t* data);
            /**
             * @brief Encode Phase 2 DUID hamming FEC.
             * @param[out] data 
             * @param[in] raw 
             */
            void encodeP2_DUIDHamming(uint8_t* data, const uint8_t* raw);
        };
    } // namespace lc
} // namespace p25

#endif // __P25_LC__LC_H__
