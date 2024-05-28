// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / DFSI peer application
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Patrick McDonnell, W3AXL
*
*/
#if !defined(__DFSI_H__)
#define __DFSI_H__

#include "Defines.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "common/yaml/Yaml.h"
#include "common/Timer.h"
#include "network/DfsiPeerNetwork.h"
#include "network/SerialService.h"

#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the core service logic.
// ---------------------------------------------------------------------------

class HOST_SW_API Dfsi {
public:
    /// <summary>Initializes a new instance of the HostTest class.</summary>
    Dfsi(const std::string& confFile);
    /// <summary>Finalizes a instance of the HostTest class.</summary>
    ~Dfsi();

    /// <summary>Executes the main host processing loop.</summary>
    int run();

private:
    const std::string& m_confFile;
    yaml::Node m_conf;

    network::DfsiPeerNetwork* m_network;

    lookups::RadioIdLookup* m_ridLookup;
    lookups::TalkgroupRulesLookup* m_tidLookup;

    uint32_t m_pingTime;
    uint32_t m_maxMissedPings;

    uint32_t m_updateLookupTime;

    bool m_debug;

    bool m_repeatTraffic;

    /// <summary>Reads basic configuration parameters from the INI.</summary>
    bool readParams();
    /// <summary>Initializes peer network connectivity.</summary>
    bool createPeerNetwork();
};

#endif // __DFSI_H__