/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
*
*/
/*
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
#if !defined(__NXDN_LC_RCCH__MESSAGE_TYPE_DCALL_HDR_H__)
#define  __NXDN_LC_RCCH__MESSAGE_TYPE_DCALL_HDR_H__

#include "common/Defines.h"
#include "common/nxdn/lc/RCCH.h"

namespace nxdn
{
    namespace lc
    {
        namespace rcch
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      Implements DCALL_HDR - Data Call Header
            // ---------------------------------------------------------------------------

            class HOST_SW_API MESSAGE_TYPE_DCALL_HDR : public RCCH {
            public:
                /// <summary>Initializes a new instance of the MESSAGE_TYPE_DCALL_HDR class.</summary>
                MESSAGE_TYPE_DCALL_HDR();

                /// <summary>Decode layer 3 data.</summary>
                void decode(const uint8_t* data, uint32_t length, uint32_t offset = 0U) override;
                /// <summary>Encode layer 3 data.</summary>
                void encode(uint8_t* data, uint32_t length, uint32_t offset = 0U) override;

                /// <summary>Returns a string that represents the current RCCH.</summary>
                std::string toString(bool isp = false) override;
            };
        } // namespace rcch
    } // namespace lc
} // namespace nxdn

#endif // __NXDN_LC_RCCH__MESSAGE_TYPE_DCALL_HDR_H__
