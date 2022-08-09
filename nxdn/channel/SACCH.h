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
#if !defined(__NXDN_CHANNEL__SACCH_H__)
#define  __NXDN_CHANNEL__SACCH_H__

#include "Defines.h"

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
            /// <summary>Flag indicating verbose log output.</summary>
            __PROPERTY(bool, verbose, Verbose);

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
