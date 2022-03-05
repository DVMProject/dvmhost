/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2016 by Jonathan Naylor G4KLX
*   Copyright (C) 2017-2022 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#if !defined(__P25_LC__LC_H__)
#define  __P25_LC__LC_H__

#include "Defines.h"
#include "p25/lc/TSBK.h"
#include "p25/lc/TDULC.h"
#include "p25/SiteData.h"
#include "edac/RS634717.h"
#include "lookups/IdenTableLookup.h"

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
            /// <summary>Initializes a new instance of the LC class.</summary>
            LC(SiteData siteData);
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

            /** Encryption data */
            /// <summary>Sets the encryption message indicator.</summary>
            void setMI(const uint8_t* mi);
            /// <summary>Gets the encryption message indicator.</summary>
            void getMI(uint8_t* mi) const;

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

            /** Local Site data */
            /// <summary>Local Site Data.</summary>
            __PROPERTY_PLAIN(SiteData, siteData, siteData);

        private:
            friend class TSBK;
            friend class TDULC;
            edac::RS634717 m_rs;
            bool m_encryptOverride;
            bool m_tsbkVendorSkip;

            uint32_t m_callTimer;

            /** Encryption data */
            uint8_t* m_mi;

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
