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
#if !defined(__NXDN_LAYER_3_H__)
#define  __NXDN_LAYER_3_H__

#include "Defines.h"

namespace nxdn
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements NXDN Layer 3 Connection Control.
        // ---------------------------------------------------------------------------

        class HOST_SW_API Layer3 {
        public:
            /// <summary>Initializes a copy instance of the Layer3 class.</summary>
            Layer3(const Layer3& data);
            /// <summary>Initializes a new instance of the Layer3 class.</summary>
            Layer3();
            /// <summary>Finalizes a instance of the Layer3 class.</summary>
            ~Layer3();

            /// <summary>Equals operator.</summary>
            Layer3& operator=(const Layer3& data);

            /// <summary>Decode layer 3 data.</summary>
            void decode(const uint8_t* data, uint32_t length, uint32_t offset = 0U);
            /// <summary>Encode layer 3 data.</summary>
            void encode(uint8_t* data, uint32_t length, uint32_t offset = 0U);

            /// <summary></summary>
            void reset();

            /// <summary>Gets the raw layer 3 data.</summary>
            void getData(uint8_t* data) const;
            /// <summary>Sets the raw layer 3 data.</summary>
            void setData(const uint8_t* data, uint32_t length);

        public:
            /// <summary>Flag indicating verbose log output.</summary>
            __PROPERTY(bool, verbose, Verbose);

            /** Common Data */
            /// <summary>Message Type</summary>
            __PROPERTY(uint8_t, messageType, MessageType);

            /// <summary>Source ID.</summary>
            __PROPERTY(uint16_t, srcId, SrcId);
            /// <summary>Destination ID.</summary>
            __PROPERTY(uint16_t, dstId, DstId);

            /// <summary>Flag indicating a group/talkgroup operation.</summary>
            __PROPERTY(bool, group, Group);

            /// <summary></summary>
            __PROPERTY(uint8_t, dataBlocks, DataBlocks);

        private:
            uint8_t* m_data;
        };
    } // namespace data
} // namespace nxdn

#endif // __NXDN_LAYER_3_H__
