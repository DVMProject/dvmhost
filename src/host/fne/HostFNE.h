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
#if !defined(__HOST_FNE_H__)
#define __HOST_FNE_H__

#include "Defines.h"
#include "network/Network.h"
#include "network/FNENetwork.h"
#include "Timer.h"
#include "lookups/RadioIdLookup.h"
#include "lookups/RoutingRulesLookup.h"
#include "yaml/Yaml.h"

#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the core FNE service logic.
// ---------------------------------------------------------------------------

class HOST_SW_API HostFNE {
public:
    /// <summary>Initializes a new instance of the HostFNE class.</summary>
    HostFNE(const std::string& confFile);
    /// <summary>Finalizes a instance of the HostFNE class.</summary>
    ~HostFNE();

    /// <summary>Executes the main FNE host processing loop.</summary>
    int run();

private:
    const std::string& m_confFile;
    yaml::Node m_conf;

    friend class network::FNENetwork;
    network::FNENetwork* m_network;

    bool m_dmrEnabled;
    bool m_p25Enabled;
    bool m_nxdnEnabled;

    lookups::RadioIdLookup* m_ridLookup;
    lookups::RoutingRulesLookup* m_routingLookup;

    std::unordered_map<std::string, lookups::RoutingRulesLookup*> m_peerRoutingLookups;
    std::unordered_map<std::string, network::Network*> m_peerNetworks;

    uint32_t m_pingTime;
    uint32_t m_maxMissedPings;
    uint32_t m_updateLookupTime;

    bool m_allowActivityTransfer;
    bool m_allowDiagnosticTransfer;

    /// <summary>Reads basic configuration parameters from the INI.</summary>
    bool readParams();
    /// <summary>Initializes master FNE network connectivity.</summary>
    bool createMasterNetwork();
    /// <summary>Initializes peer FNE network connectivity.</summary>
    bool createPeerNetworks();
};

#endif // __HOST_FNE_H__
