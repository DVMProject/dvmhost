/**
* Digital Voice Modem - Converged FNE Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Converged FNE Software
*
*/
/*
*   Copyright (C) 2024 by Bryan Biedenkapp N2PLL
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
#if !defined(__PEER_NETWORK_H__)
#define __PEER_NETWORK_H__

#include "Defines.h"
#include "host/network/Network.h"

#include <string>
#include <cstdint>

namespace network
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements the core peer networking logic.
    // ---------------------------------------------------------------------------

    class HOST_SW_API PeerNetwork : public Network {
    public:
        /// <summary>Initializes a new instance of the PeerNetwork class.</summary>
        PeerNetwork(const std::string& address, uint16_t port, uint16_t localPort, uint32_t peerId, const std::string& password,
            bool duplex, bool debug, bool dmr, bool p25, bool nxdn, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, bool updateLookup);

    protected:
        /// <summary>Writes configuration to the network.</summary>
        bool writeConfig() override;
    };
} // namespace network

#endif // __PEER_NETWORK_H__
