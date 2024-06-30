// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - DFSI V.24/UDP Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Patrick McDonnell, W3AXL
 *
 */
/**
 * @file Dfsi.h
 * @ingroup dfsi
 * @file Dfsi.cpp
 * @ingroup dfsi
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
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the core service logic.
 * @ingroup dfsi
 */
class HOST_SW_API Dfsi {
public:
    /**
     * @brief Initializes a new instance of the HostTest class.
     * @param confFile Full-path to the configuration file.
     */
    Dfsi(const std::string& confFile);
    /**
     * @brief Finalizes a instance of the HostTest class.
     */
    ~Dfsi();

    /**
     * @brief Executes the main host processing loop.
     * @returns int Zero if successful, otherwise error occurred.
     */
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

    network::SerialService* m_serial;

    /**
     * @brief Reads basic configuration parameters from the INI.
     * @returns bool True, if configuration was read successfully, otherwise false.
     */
    bool readParams();
    /**
     * @brief Initializes peer network connectivity.
     * @returns bool True, if network connectivity was initialized, otherwise false.
     */
    bool createPeerNetwork();
    /**
     * @brief Initializes serial V.24 network.
     * @returns bool True, if serial connectivity was initialized, otherwise false.
     */
    bool createSerialNetwork(uint32_t p25BufferSize, uint16_t callTimeout);
};

#endif // __DFSI_H__