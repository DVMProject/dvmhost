// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file HostFNE.h
 * @ingroup fne
 * @file HostFNE.cpp
 * @ingroup fne
 */
#if !defined(__HOST_FNE_H__)
#define __HOST_FNE_H__

#include "Defines.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "common/lookups/PeerListLookup.h"
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

namespace network { namespace callhandler { class HOST_SW_API TagDMRData; } }
namespace network { namespace callhandler { namespace packetdata { class HOST_SW_API DMRPacketData; } } }
namespace network { namespace callhandler { class HOST_SW_API TagP25Data; } }
namespace network { namespace callhandler { namespace packetdata { class HOST_SW_API P25PacketData; } } }
namespace network { namespace callhandler { class HOST_SW_API TagNXDNData; } }

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the core FNE service logic.
 * @ingroup fne
 */
class HOST_SW_API HostFNE {
public:
    /**
     * @brief Initializes a new instance of the HostFNE class.
     * @param confFile Full-path to the configuration file.
     */
    HostFNE(const std::string& confFile);
    /**
     * @brief Finalizes a instance of the HostFNE class.
     */
    ~HostFNE();

    /**
     * @brief Executes the main FNE host processing loop.
     * @returns int Zero if successful, otherwise error occurred.
     */
    int run();

private:
    const std::string& m_confFile;
    yaml::Node m_conf;

    friend class network::FNENetwork;
    friend class network::callhandler::TagDMRData;
    friend class network::callhandler::packetdata::DMRPacketData;
    friend class network::callhandler::TagP25Data;
    friend class network::callhandler::packetdata::P25PacketData;
    friend class network::callhandler::TagNXDNData;
    network::FNENetwork* m_network;
    network::DiagNetwork* m_diagNetwork;

    bool m_dmrEnabled;
    bool m_p25Enabled;
    bool m_nxdnEnabled;

    lookups::RadioIdLookup* m_ridLookup;
    lookups::TalkgroupRulesLookup* m_tidLookup;
    lookups::PeerListLookup* m_peerListLookup;

    std::unordered_map<std::string, network::PeerNetwork*> m_peerNetworks;

    uint32_t m_pingTime;
    uint32_t m_maxMissedPings;
    uint32_t m_updateLookupTime;

    bool m_useAlternatePortForDiagnostics;

    bool m_allowActivityTransfer;
    bool m_allowDiagnosticTransfer;

    friend class RESTAPI;
    RESTAPI* m_RESTAPI;

    /**
     * @brief Reads basic configuration parameters from the INI.
     * @returns bool True, if configuration was read successfully, otherwise false.
     */
    bool readParams();
    /**
     * @brief Initializes REST API services.
     * @returns bool True, if REST API services were initialized, otherwise false.
     */
    bool initializeRESTAPI();
    /**
     * @brief Initializes master FNE network connectivity.
     * @returns bool True, if network connectivity was initialized, otherwise false.
     */
    bool createMasterNetwork();
    /**
     * @brief Initializes peer FNE network connectivity.
     * @returns bool True, if network connectivity was initialized, otherwise false.
     */
    bool createPeerNetworks();

    /**
     * @brief Processes any peer network traffic.
     * @param peerNetwork Instance of PeerNetwork to process traffic for.
     */
    void processPeer(network::PeerNetwork* peerNetwork);
};

#endif // __HOST_FNE_H__
