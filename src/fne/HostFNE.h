// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023-2025 Bryan Biedenkapp, N2PLL
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
#include "common/lookups/AdjSiteMapLookup.h"
#include "common/network/viface/VIFace.h"
#include "common/yaml/Yaml.h"
#include "common/Timer.h"
#include "network/FNENetwork.h"
#include "network/DiagNetwork.h"
#include "network/PeerNetwork.h"
#include "network/RESTAPI.h"
#include "CryptoContainer.h"

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
     * @brief Virtual Network Packet Data Digital Mode
     */
    enum PacketDataMode {
        DMR,            //! Digital Mobile Radio
        PROJECT25       //! Project 25
    };

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
    friend class network::DiagNetwork;
    friend class network::callhandler::TagDMRData;
    friend class network::callhandler::packetdata::DMRPacketData;
    friend class network::callhandler::TagP25Data;
    friend class network::callhandler::packetdata::P25PacketData;
    friend class network::callhandler::TagNXDNData;
    network::FNENetwork* m_network;
    network::DiagNetwork* m_diagNetwork;

    bool m_vtunEnabled;
    PacketDataMode m_packetDataMode;
#if !defined(_WIN32)
    network::viface::VIFace* m_tun;
#endif // !defined(_WIN32)

    bool m_dmrEnabled;
    bool m_p25Enabled;
    bool m_nxdnEnabled;

    lookups::RadioIdLookup* m_ridLookup;
    lookups::TalkgroupRulesLookup* m_tidLookup;
    lookups::PeerListLookup* m_peerListLookup;
    lookups::AdjSiteMapLookup* m_adjSiteMapLookup;

    CryptoContainer* m_cryptoLookup;

    std::unordered_map<uint32_t, network::PeerNetwork*> m_peerNetworks;

    uint32_t m_pingTime;
    uint32_t m_maxMissedPings;
    uint32_t m_updateLookupTime;

    bool m_peerLinkSavesACL;

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
     * @brief Entry point to master FNE network thread.
     * @param arg Instance of the thread_t structure.
     * @returns void* (Ignore)
     */
    static void* threadMasterNetwork(void* arg);
    /**
     * @brief Entry point to master FNE diagnostics network thread.
     * @param arg Instance of the thread_t structure.
     * @returns void* (Ignore)
     */
    static void* threadDiagNetwork(void* arg);
    /**
     * @brief Initializes peer FNE network connectivity.
     * @returns bool True, if network connectivity was initialized, otherwise false.
     */
    bool createPeerNetworks();

    /**
     * @brief Initializes virtual networking.
     * @returns bool True, if network connectivity was initialized, otherwise false.
     */
    bool createVirtualNetworking();
#if !defined(_WIN32)
    /**
     * @brief Entry point to virtual networking thread.
     * @param arg Instance of the thread_t structure.
     * @returns void* (Ignore)
     */
    static void* threadVirtualNetworking(void* arg);
    /**
     * @brief Entry point to virtual networking clocking thread.
     * @param arg Instance of the thread_t structure.
     * @returns void* (Ignore)
     */
    static void* threadVirtualNetworkingClock(void* arg);
#endif // !defined(_WIN32)
    /**
     * @brief Processes DMR peer network traffic.
     * @param data Buffer containing DMR data.
     * @param length Length of the buffer.
     * @param streamId Stream ID.
     * @param fneHeader FNE header for the packet.
     * @param rtpHeader RTP header for the packet.
     */
    void processPeerDMR(network::PeerNetwork* peerNetwork, const uint8_t* data, uint32_t length, uint32_t streamId, 
        const network::frame::RTPFNEHeader& fneHeader, const network::frame::RTPHeader& rtpHeader);
    /**
     * @brief Processes P25 peer network traffic.
     * @param data Buffer containing P25 data.
     * @param length Length of the buffer.
     * @param streamId Stream ID.
     * @param fneHeader FNE header for the packet.
     * @param rtpHeader RTP header for the packet.
     */
    void processPeerP25(network::PeerNetwork* peerNetwork, const uint8_t* data, uint32_t length, uint32_t streamId, 
        const network::frame::RTPFNEHeader& fneHeader, const network::frame::RTPHeader& rtpHeader);
    /**
     * @brief Processes NXDN peer network traffic.
     * @param data Buffer containing NXDN data.
     * @param length Length of the buffer.
     * @param streamId Stream ID.
     * @param fneHeader FNE header for the packet.
     * @param rtpHeader RTP header for the packet.
     */
    void processPeerNXDN(network::PeerNetwork* peerNetwork, const uint8_t* data, uint32_t length, uint32_t streamId, 
        const network::frame::RTPFNEHeader& fneHeader, const network::frame::RTPHeader& rtpHeader);
};

#endif // __HOST_FNE_H__
