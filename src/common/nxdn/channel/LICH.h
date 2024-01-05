/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2018 by Jonathan Naylor G4KLX
*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
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
#if !defined(__NXDN_CHANNEL__LICH_H__)
#define  __NXDN_CHANNEL__LICH_H__

#include "common/Defines.h"

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
            __PROPERTY(uint8_t, rfct, RFCT);
            /// <summary>Functional Channel Type</summary>
            __PROPERTY(uint8_t, fct, FCT);
            /// <summary>Channel Options</summary>
            __PROPERTY(uint8_t, option, Option);
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
