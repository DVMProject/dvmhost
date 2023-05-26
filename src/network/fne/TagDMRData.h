/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2023 by Bryan Biedenkapp N2PLL
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
#if !defined(__FNE__TAG_DMR_DATA_H__)
#define __FNE__TAG_DMR_DATA_H__

#include "Defines.h"
#include "dmr/DMRDefines.h"
#include "dmr/data/Data.h"
#include "network/FNENetwork.h"

namespace network
{
    namespace fne
    {  
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements the DMR data FNE networking logic.
        // ---------------------------------------------------------------------------

        class HOST_SW_API TagDMRData {
        public:
            /// <summary>Initializes a new instance of the TagDMRData class.</summary>
            TagDMRData(FNENetwork* network, bool debug);
            /// <summary>Finalizes a instance of the TagDMRData class.</summary>
            ~TagDMRData();

            /// <summary>Process a data frame from the network.</summary>
            bool processFrame(const uint8_t* data, uint32_t len, uint32_t streamId, sockaddr_storage& address);

        private:
            FNENetwork* m_network;

            bool m_debug;

            /// <summary>Helper to determine if the peer is permitted for traffic.</summary>
            bool isPeerPermitted(uint32_t peerId, dmr::data::Data& data, uint32_t streamId);
            /// <summary>Helper to validate the DMR call stream.</summary>
            bool validate(uint32_t peerId, dmr::data::Data& data, uint32_t streamId);
        };
    } // namespace fne
} // namespace network

#endif // __FNE__TAG_DMR_DATA_H__
