/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2021 Bryan Biedenkapp N2PLL <gatekeep@gmail.com>
*   Copyright (C) 2020-2021 by Bryan Biedenkapp N2PLL
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
#if !defined(__DMR_LC__PRIVACY_LC_H__)
#define __DMR_LC__PRIVACY_LC_H__

#include "Defines.h"
#include "dmr/DMRDefines.h"

namespace dmr
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents DMR privacy indicator link control data.
        // ---------------------------------------------------------------------------

        class HOST_SW_API PrivacyLC {
        public:
            /// <summary>Initializes a new instance of the PrivacyLC class.</summary>
            PrivacyLC(const uint8_t* bytes);
            /// <summary>Initializes a new instance of the PrivacyLC class.</summary>
            PrivacyLC(const bool* bits);
            /// <summary>Initializes a new instance of the PrivacyLC class.</summary>
            PrivacyLC();
            /// <summary>Finalizes a instance of the PrivacyLC class.</summary>
            ~PrivacyLC();

            /// <summary>Gets LC data as bytes.</summary>
            void getData(uint8_t* bytes) const;
            /// <summary>Gets LC data as bits.</summary>
            void getData(bool* bits) const;

        public:
            /// <summary>Feature ID.</summayr>
            __PROPERTY(uint8_t, FID, FID);

            /// <summary>Destination ID.</summary>
            __PROPERTY(uint32_t, dstId, DstId);

            /** Service Options */
            /// <summary>Flag indicating a group/talkgroup operation.</summary>
            __PROPERTY(bool, group, Group);

            /** Encryption data */
            /// <summary>Encryption algorithm ID.</summary>
            __PROPERTY(uint8_t, algId, AlgId);
            /// <summary>Encryption key ID.</summary>
            __PROPERTY(uint32_t, kId, KId);

        private:
            /** Encryption data */
            uint8_t* m_mi;
        };
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC__PRIVACY_LC_H__
