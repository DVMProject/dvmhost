// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2021 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__DMR_LC__PRIVACY_LC_H__)
#define __DMR_LC__PRIVACY_LC_H__

#include "common/Defines.h"

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
            PrivacyLC(const uint8_t* data);
            /// <summary>Initializes a new instance of the PrivacyLC class.</summary>
            PrivacyLC(const bool* bits);
            /// <summary>Initializes a new instance of the PrivacyLC class.</summary>
            PrivacyLC();
            /// <summary>Finalizes a instance of the PrivacyLC class.</summary>
            ~PrivacyLC();

            /// <summary>Gets LC data as bytes.</summary>
            void getData(uint8_t* data) const;
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
