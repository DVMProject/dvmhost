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
#if !defined(__NXDN_CHANNEL__SACCH_H__)
#define  __NXDN_CHANNEL__SACCH_H__

#include "common/Defines.h"

namespace nxdn
{
    namespace channel
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements NXDN Slow Associated Control Channel.
        // ---------------------------------------------------------------------------

        class HOST_SW_API SACCH {
        public:
            /// <summary>Initializes a new instance of the SACCH class.</summary>
            SACCH();
            /// <summary>Initializes a copy instance of the SACCH class.</summary>
            SACCH(const SACCH& data);
            /// <summary>Finalizes a instance of the SACCH class.</summary>
            ~SACCH();

            /// <summary>Equals operator.</summary>
            SACCH& operator=(const SACCH& data);

            /// <summary>Decode a slow associated control channel.</summary>
            bool decode(const uint8_t* data);
            /// <summary>Encode a slow associated control channel.</summary>
            void encode(uint8_t* data) const;

            /// <summary>Gets the raw SACCH data.</summary>
            void getData(uint8_t* data) const;
            /// <summary>Sets the raw SACCH data.</summary>
            void setData(const uint8_t* data);

        public:
            /** Common Data */
            /// <summary>Radio Access Number</summary>
            __PROPERTY(uint8_t, ran, RAN);
            /// <summary></summary>
            __PROPERTY(uint8_t, structure, Structure);

        private:
            uint8_t* m_data;

            /// <summary>Internal helper to copy the class.</summary>
            void copy(const SACCH& data);
        };
    } // namespace channel
} // namespace nxdn

#endif // __NXDN_CHANNEL__SACCH_H__
