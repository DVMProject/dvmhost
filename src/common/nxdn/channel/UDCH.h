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
*   Copyright (C) 2018 Jonathan Naylor, G4KLX
*   Copyright (C) 2022 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__NXDN_CHANNEL__UDCH_H__)
#define  __NXDN_CHANNEL__UDCH_H__

#include "common/Defines.h"

namespace nxdn
{
    namespace channel
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements NXDN User Data Channel.
        // ---------------------------------------------------------------------------

        class HOST_SW_API UDCH {
        public:
            /// <summary>Initializes a new instance of the UDCH class.</summary>
            UDCH();
            /// <summary>Initializes a copy instance of the UDCH class.</summary>
            UDCH(const UDCH& data);
            /// <summary>Finalizes a instance of the UDCH class.</summary>
            ~UDCH();

            /// <summary>Equals operator.</summary>
            UDCH& operator=(const UDCH& data);

            /// <summary>Decode a user data channel.</summary>
            bool decode(const uint8_t* data);
            /// <summary>Encode a user data channel.</summary>
            void encode(uint8_t* data) const;

            /// <summary>Gets the raw UDCH data.</summary>
            void getData(uint8_t* data) const;
            /// <summary>Sets the raw UDCH data.</summary>
            void setData(const uint8_t* data);

        public:
            /** Common Data */
            /// <summary>Radio Access Number</summary>
            __PROPERTY(uint8_t, ran, RAN);

        private:
            uint8_t* m_data;

            /// <summary>Internal helper to copy the class.</summary>
            void copy(const UDCH& data);
        };
    } // namespace channel
} // namespace nxdn

#endif // __NXDN_CHANNEL__UDCH_H__
