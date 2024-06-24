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
*   Copyright (C) 2016 Jonathan Naylor, G4KLX
*   Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
*
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
        //      Represents link control data for HDU, LDU1 and 2 packets.
        // ---------------------------------------------------------------------------

        class HOST_SW_API LC {
        public:
            /// <summary>Initializes a new instance of the LC class.</summary>
            LC();
            /// <summary>Initializes a copy instance of the LC class.</summary>
            LC(const LC& data);
            /// <summary>Finalizes a instance of the LC class.</summary>
            ~LC();

            /// <summary>Equals operator.</summary>
            LC& operator=(const LC& data);

            /// <summary>Decode a header data unit.</summary>
            bool decodeHDU(const uint8_t* data);
            /// <summary>Encode a header data unit.</summary>
            void encodeHDU(uint8_t* data);

            /// <summary>Decode a logical link data unit 1.</summary>
            bool decodeLDU1(const uint8_t* data);
            /// <summary>Encode a logical link data unit 1.</summary>
            void encodeLDU1(uint8_t* data);

            /// <summary>Decode a logical link data unit 2.</summary>
            bool decodeLDU2(const uint8_t* data);
            /// <summary>Encode a logical link data unit 2.</summary>
            void encodeLDU2(uint8_t* data);

            /// <summary>Helper to determine if the MFId is a standard MFId.</summary>
            bool isStandardMFId() const;

            /** Encryption data */
            /// <summary>Sets the encryption message indicator.</summary>
            void setMI(const uint8_t* mi);
            /// <summary>Gets the encryption message indicator.</summary>
            void getMI(uint8_t* mi) const;

            /** Local Site data */
            /// <summary>Gets the local site data.</summary>
            static SiteData getSiteData() { return m_siteData; }
            /// <summary>Sets the local site data.</summary>
            static void setSiteData(SiteData siteData) { m_siteData = siteData; }

        public:
            /** Common Data */
            /// <summary>Flag indicating the link control data is protected.</summary>
            __PROPERTY(bool, protect, Protect);
            /// <summary>Link control opcode.</summary>
            __PROPERTY(uint8_t, lco, LCO);
            /// <summary>Manufacturer ID.</summary>
            __PROPERTY(uint8_t, mfId, MFId);

            /// <summary>Source ID.</summary>
            __PROPERTY(uint32_t, srcId, SrcId);
            /// <summary>Destination ID.</summary>
            __PROPERTY(uint32_t, dstId, DstId);

            /// <summary>Voice channel number.</summary>
            __PROPERTY(uint32_t, grpVchNo, GrpVchNo);

            /// <summary>Voice channel number.</summary>
            __PROPERTY(uint32_t, grpVchNoB, GrpVchNoB);
            /// <summary>Destination ID.</summary>
            __PROPERTY(uint32_t, dstIdB, DstIdB);

            /// <summary>Flag indicating explicit addressing.</summary>
            __PROPERTY(bool, explicitId, ExplicitId);

            /// <summary>Network ID.</summary>
            __PROPERTY(uint32_t, netId, NetId);
            /// <summary>System ID.</summary>
            __PROPERTY(uint32_t, sysId, SysId);

            /** Service Options */
            /// <summary>Flag indicating the emergency bits are set.</summary>
            __PROPERTY(bool, emergency, Emergency);
            /// <summary>Flag indicating that encryption is enabled.</summary>
            __PROPERTY(bool, encrypted, Encrypted);
            /// <summary>Priority level for the traffic.</summary>
            __PROPERTY(uint8_t, priority, Priority);
            /// <summary>Flag indicating a group/talkgroup operation.</summary>
            __PROPERTY(bool, group, Group);

            /** Encryption data */
            /// <summary>Encryption algorithm ID.</summary>
            __PROPERTY(uint8_t, algId, AlgId);
            /// <summary>Encryption key ID.</summary>
            __PROPERTY(uint32_t, kId, KId);

            /** Packed RS Data */
            /// <summary>Packed RS Data.</summary>
            __PROPERTY(ulong64_t, rsValue, RS);

        private:
            friend class TSBK;
            friend class TDULC;
            edac::RS634717 m_rs;
            bool m_encryptOverride;
            bool m_tsbkVendorSkip;

            uint32_t m_callTimer;

            /** Encryption data */
            uint8_t* m_mi;

            /** Local Site data */
            static SiteData m_siteData;

            /// <summary>Internal helper to copy the class.</summary>
            void copy(const LC& data);

            /// <summary>Decode link control.</summary>
            bool decodeLC(const uint8_t* rs);
            /// <summary>Encode link control.</summary>
            void encodeLC(uint8_t* rs);

            /// <summary>Decode LDU hamming FEC.</summary>
            void decodeLDUHamming(const uint8_t* raw, uint8_t* data);
            /// <summary>Encode LDU hamming FEC.</summary>
            void encodeLDUHamming(uint8_t* data, const uint8_t* raw);

            /// <summary>Decode HDU Golay FEC.</summary>
            void decodeHDUGolay(const uint8_t* raw, uint8_t* data);
            /// <summary>Encode HDU Golay FEC.</summary>
            void encodeHDUGolay(uint8_t* data, const uint8_t* raw);
        };
    } // namespace lc
} // namespace p25

#endif // __P25_LC__LC_H__
