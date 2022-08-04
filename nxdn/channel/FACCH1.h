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
#if !defined(__NXDN_FACCH1_H__)
#define  __NXDN_FACCH1_H__

#include "Defines.h"

namespace nxdn
{
    namespace channel
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements NXDN Fast Associated Control Channel 1.
        // ---------------------------------------------------------------------------

        class HOST_SW_API FACCH1 {
        public:
            /// <summary>Initializes a copy instance of the FACCH1 class.</summary>
            FACCH1(const FACCH1& data);
            /// <summary>Initializes a new instance of the FACCH1 class.</summary>
            FACCH1();
            /// <summary>Finalizes a instance of the FACCH1 class.</summary>
            ~FACCH1();

            /// <summary>Equals operator.</summary>
            FACCH1& operator=(const FACCH1& data);

            /// <summary>Decode a fast associated control channel 1.</summary>
            bool decode(const uint8_t* data, uint32_t offset);
            /// <summary>Encode a fast associated control channel 1.</summary>
            void encode(uint8_t* data, uint32_t offset) const;

            /// <summary>Gets the raw FACCH1 data.</summary>
            void getData(uint8_t* data) const;
            /// <summary>Sets the raw FACCH1 data.</summary>
            void setData(const uint8_t* data);
        
        public:
            /// <summary>Flag indicating verbose log output.</summary>
            __PROPERTY(bool, verbose, Verbose);

        private:
            uint8_t* m_data;
        };
    } // namespace channel
} // namespace nxdn

#endif // __NXDN_FACCH1_H__
