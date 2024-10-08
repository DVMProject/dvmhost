// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - FNE System View
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file HostWS.h
 * @ingroup fneSysView
 * @file HostWS.cpp
 * @ingroup fneSysView
 */
#if !defined(__HOST_WS_H__)
#define __HOST_WS_H__

#if !defined(NO_WEBSOCKETS)

#include "Defines.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "common/yaml/Yaml.h"
#include "common/Timer.h"
#include "network/PeerNetwork.h"

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <string>
#include <set>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the core service logic.
 * @ingroup fneSysView
 */
class HOST_SW_API HostWS {
public:
    /**
     * @brief Initializes a new instance of the HostWS class.
     * @param confFile Full-path to the configuration file.
     */
    HostWS(const std::string& confFile);
    /**
     * @brief Finalizes a instance of the HostWS class.
     */
    ~HostWS();

    /**
     * @brief Executes the main host processing loop.
     * @returns int Zero if successful, otherwise error occurred.
     */
    int run();

    /**
     * @brief 
     * @param obj 
     */
    void send(json::object obj);

private:
    const std::string& m_confFile;
    yaml::Node m_conf;

    uint16_t m_websocketPort;

    typedef websocketpp::server<websocketpp::config::asio> wsServer;
    wsServer m_wsServer;
    typedef std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> wsConList;
    wsConList m_wsConList;

    bool m_debug;

    /**
     * @brief Reads basic configuration parameters from the INI.
     * @returns bool True, if configuration was read successfully, otherwise false.
     */
    bool readParams();

    /**
     * @brief Called when a WebSocket connection is opened.
     * @param handle Connection Handle.
     */
    void wsOnConOpen(websocketpp::connection_hdl handle);
    /**
     * @brief Called when a WebSocket connection is closed.
     * @param handle Connection Handle.
     */
    void wsOnConClose(websocketpp::connection_hdl handle);
    /**
     * @brief Called when a WebSocket message is received.
     * @param handle Connection Handle.
     * @param msg WebSocket Message.
     */
    void wsOnMessage(websocketpp::connection_hdl handle, wsServer::message_ptr msg);

    /**
     * @brief Entry point to WebSocket thread.
     * @param arg Instance of the thread_t structure.
     * @returns void* (Ignore)
     */
    static void* threadWebSocket(void* arg);
};

#endif // !defined(NO_WEBSOCKETS)

#endif // __HOST_WS_H__
