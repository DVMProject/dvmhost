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
*   Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__NXDN_CHANNEL__LICH_H__)
#define  __NXDN_CHANNEL__LICH_H__

#include "common/Defines.h"
#include "common/nxdn/NXDNDefines.h"

namespace nxdn
{
    namespace channel
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements NXDN Link Information Channel.
        // ---------------------------------------------------------------------------

        class HOST_SW_API LICH {
        public:
            /// <summary>Initializes a new instance of the LICH class.</summary>
            LICH();
            /// <summary>Initializes a copy instance of the LICH class.</summary>
            LICH(const LICH& lich);
            /// <summary>Finalizes a instance of the LICH class.</summary>
            ~LICH();

            /// <summary>Equals operator.</summary>
            LICH& operator=(const LICH& lich);

            /// <summary>Decode a link information channel.</summary>
            bool decode(const uint8_t* data);
            /// <summary>Encode a link information channel.</summary>
            void encode(uint8_t* data);

        public:
            /** Common Data */
            /// <summary>RF Channel Type</summary>
            __PROPERTY(defines::RFChannelType::E, rfct, RFCT);
            /// <summary>Functional Channel Type</summary>
            __PROPERTY(defines::FuncChannelType::E, fct, FCT);
            /// <summary>Channel Options</summary>
            __PROPERTY(defines::ChOption::E, option, Option);
            /// <summary>Flag indicating outbound traffic direction</summary>
            __PROPERTY(bool, outbound, Outbound);

        private:
            uint8_t m_lich;

            /// <summary>Internal helper to copy the class.</summary>
            void copy(const LICH& data);

            /// <summary></summary>
            bool getParity() const;
        };
    } // namespace channel
} // namespace nxdn

#endif // __NXDN_CHANNEL__LICH_H__
