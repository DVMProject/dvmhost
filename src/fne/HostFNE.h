// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Converged FNE Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Converged FNE Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__HOST_FNE_H__)
#define __HOST_FNE_H__

#include "Defines.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "common/yaml/Yaml.h"
#include "common/Timer.h"
#include "network/FNENetwork.h"
#include "network/DiagNetwork.h"
#include "network/PeerNetwork.h"
#include "network/RESTAPI.h"

#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------

namespace network { namespace fne { class HOST_SW_API TagDMRData; } }
namespace network { namespace fne { class HOST_SW_API TagP25Data; } }
namespace network { namespace fne { class HOST_SW_API TagNXDNData; } }

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
    friend class network::fne::TagDMRData;
    friend class network::fne::TagP25Data;
    friend class network::fne::TagNXDNData;
    network::FNENetwork* m_network;
    network::DiagNetwork* m_diagNetwork;

    bool m_dmrEnabled;
    bool m_p25Enabled;
    bool m_nxdnEnabled;

    lookups::RadioIdLookup* m_ridLookup;
    lookups::TalkgroupRulesLookup* m_tidLookup;

    std::unordered_map<std::string, network::PeerNetwork*> m_peerNetworks;

    uint32_t m_pingTime;
    uint32_t m_maxMissedPings;
    uint32_t m_updateLookupTime;

    bool m_useAlternatePortForDiagnostics;

    bool m_allowActivityTransfer;
    bool m_allowDiagnosticTransfer;

    friend class RESTAPI;
    RESTAPI* m_RESTAPI;

    /// <summary>Reads basic configuration parameters from the INI.</summary>
    bool readParams();
    /// <summary>Initializes REST API services.</summary>
    bool initializeRESTAPI();
    /// <summary>Initializes master FNE network connectivity.</summary>
    bool createMasterNetwork();
    /// <summary>Initializes peer FNE network connectivity.</summary>
    bool createPeerNetworks();

    /// <summary>Processes any peer network traffic.</summary>
    void processPeer(network::PeerNetwork* peerNetwork);
};

#endif // __HOST_FNE_H__
