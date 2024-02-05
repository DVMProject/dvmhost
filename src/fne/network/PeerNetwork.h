// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Converged FNE Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Converged FNE Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__PEER_NETWORK_H__)
#define __PEER_NETWORK_H__

#include "Defines.h"
#include "host/network/Network.h"

#include <string>
#include <cstdint>
#include <vector>

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

        /// <summary>Gets the blocked traffic peer ID table.</summary>
        std::vector<uint32_t> blockTrafficTo() const { return m_blockTrafficToTable; }
        /// <summary>Adds an entry to the blocked traffic peer ID table.</summary>
        void addBlockedTrafficPeer(uint32_t peerId) { m_blockTrafficToTable.push_back(peerId); }
        /// <summary>Checks if the passed peer ID is blocked from sending to this peer.</summary>
        bool checkBlockedPeer(uint32_t peerId);

    protected:
        std::vector<uint32_t> m_blockTrafficToTable;

        /// <summary>Writes configuration to the network.</summary>
        bool writeConfig() override;
    };
} // namespace network

#endif // __PEER_NETWORK_H__
