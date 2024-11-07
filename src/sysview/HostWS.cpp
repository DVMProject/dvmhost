// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - FNE System View
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#if !defined(NO_WEBSOCKETS)
#include "Defines.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "common/Log.h"
#include "common/StopWatch.h"
#include "common/Thread.h"
#include "common/Utils.h"
#include "fne/network/RESTDefines.h"
#include "remote/RESTClient.h"
#include "network/PeerNetwork.h"
#include "HostWS.h"
#include "SysViewMain.h"

using namespace lookups;

#include <unistd.h>
#include <pwd.h>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define IDLE_WARMUP_MS 5U


// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/**
 * @brief Helper to convert a TalkgroupRuleGroupVoice to JSON.
 * @param groupVoice Instance of TalkgroupRuleGroupVoice to convert to JSON.
 * @returns json::object JSON object.
 */
json::object tgToJson(const TalkgroupRuleGroupVoice& groupVoice) 
{
    json::object tg = json::object();

    std::string tgName = groupVoice.name();
    tg["name"].set<std::string>(tgName);
    std::string tgAlias = groupVoice.nameAlias();
    tg["alias"].set<std::string>(tgAlias);
    bool invalid = groupVoice.isInvalid();
    tg["invalid"].set<bool>(invalid);

    // source stanza
    {
        json::object source = json::object();
        uint32_t tgId = groupVoice.source().tgId();
        source["tgid"].set<uint32_t>(tgId);
        uint8_t tgSlot = groupVoice.source().tgSlot();
        source["slot"].set<uint8_t>(tgSlot);
        tg["source"].set<json::object>(source);
    }

    // config stanza
    {
        json::object config = json::object();
        bool active = groupVoice.config().active();
        config["active"].set<bool>(active);
        bool affiliated = groupVoice.config().affiliated();
        config["affiliated"].set<bool>(affiliated);
        bool parrot = groupVoice.config().parrot();
        config["parrot"].set<bool>(parrot);

        json::array inclusions = json::array();
        std::vector<uint32_t> inclusion = groupVoice.config().inclusion();
        if (inclusion.size() > 0) {
            for (auto inclEntry : inclusion) {
                uint32_t peerId = inclEntry;
                inclusions.push_back(json::value((double)peerId));
            }
        }
        config["inclusion"].set<json::array>(inclusions);

        json::array exclusions = json::array();
        std::vector<uint32_t> exclusion = groupVoice.config().exclusion();
        if (exclusion.size() > 0) {
            for (auto exclEntry : exclusion) {
                uint32_t peerId = exclEntry;
                exclusions.push_back(json::value((double)peerId));
            }
        }
        config["exclusion"].set<json::array>(exclusions);

        json::array rewrites = json::array();
        std::vector<lookups::TalkgroupRuleRewrite> rewrite = groupVoice.config().rewrite();
        if (rewrite.size() > 0) {
            for (auto rewrEntry : rewrite) {
                json::object rewrite = json::object();
                uint32_t peerId = rewrEntry.peerId();
                rewrite["peerid"].set<uint32_t>(peerId);
                uint32_t tgId = rewrEntry.tgId();
                rewrite["tgid"].set<uint32_t>(tgId);
                uint8_t tgSlot = rewrEntry.tgSlot();
                rewrite["slot"].set<uint8_t>(tgSlot);

                rewrites.push_back(json::value(rewrite));
            }
        }
        config["rewrite"].set<json::array>(rewrites);

        json::array always = json::array();
        std::vector<uint32_t> alwaysSend = groupVoice.config().alwaysSend();
        if (alwaysSend.size() > 0) {
            for (auto alwaysEntry : alwaysSend) {
                uint32_t peerId = alwaysEntry;
                always.push_back(json::value((double)peerId));
            }
        }
        config["always"].set<json::array>(always);

        json::array preferreds = json::array();
        std::vector<uint32_t> preferred = groupVoice.config().preferred();
        if (preferred.size() > 0) {
            for (auto prefEntry : preferred) {
                uint32_t peerId = prefEntry;
                preferreds.push_back(json::value((double)peerId));
            }
        }
        config["preferred"].set<json::array>(preferreds);

        tg["config"].set<json::object>(config);
    }

    return tg;
}

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the HostWS class. */

HostWS::HostWS(const std::string& confFile) :
    m_confFile(confFile),
    m_conf(),
    m_websocketPort(8443U),
    m_wsServer(),
    m_wsConList(),
    m_debug(false)
{
    /* stub */
}

/* Finalizes a instance of the HostWS class. */

HostWS::~HostWS() = default;

/* Executes the main FNE processing loop. */

int HostWS::run()
{
    bool ret = false;
    try {
        ret = yaml::Parse(m_conf, m_confFile.c_str());
        if (!ret) {
            ::fatal("cannot read the configuration file, %s\n", m_confFile.c_str());
        }
    }
    catch (yaml::OperationException const& e) {
        ::fatal("cannot read the configuration file - %s (%s)", m_confFile.c_str(), e.message());
    }

    bool m_daemon = m_conf["daemon"].as<bool>(false);
    if (m_daemon && g_foreground)
        m_daemon = false;

    // re-initialize system logging
    yaml::Node logConf = m_conf["log"];
    ret = ::LogInitialise(logConf["filePath"].as<std::string>(), logConf["fileRoot"].as<std::string>(),
        logConf["fileLevel"].as<uint32_t>(0U), logConf["displayLevel"].as<uint32_t>(0U));
    if (!ret) {
        ::fatal("unable to open the log file\n");
    }

    // handle POSIX process forking
    if (m_daemon) {
        // create new process
        pid_t pid = ::fork();
        if (pid == -1) {
            ::fprintf(stderr, "%s: Couldn't fork() , exiting\n", g_progExe.c_str());
            ::LogFinalise();
            return EXIT_FAILURE;
        }
        else if (pid != 0) {
            ::LogFinalise();
            exit(EXIT_SUCCESS);
        }

        // create new session and process group
        if (::setsid() == -1) {
            ::fprintf(stderr, "%s: Couldn't setsid(), exiting\n", g_progExe.c_str());
            ::LogFinalise();
            return EXIT_FAILURE;
        }

        // set the working directory to the root directory
        if (::chdir("/") == -1) {
            ::fprintf(stderr, "%s: Couldn't cd /, exiting\n", g_progExe.c_str());
            ::LogFinalise();
            return EXIT_FAILURE;
        }

        ::close(STDIN_FILENO);
        ::close(STDOUT_FILENO);
        ::close(STDERR_FILENO);
    }

    // read base parameters from configuration
    ret = readParams();
    if (!ret)
        return EXIT_FAILURE;

    // setup peer network connection
    ret = createPeerNetwork();
    if (!ret)
        return EXIT_FAILURE;

    yaml::Node fne = g_conf["fne"];
    std::string fneRESTAddress = fne["restAddress"].as<std::string>("127.0.0.1");
    uint16_t fneRESTPort = (uint16_t)fne["restPort"].as<uint32_t>(9990U);
    std::string fnePassword = fne["restPassword"].as<std::string>("PASSWORD");
    bool fneSSL = fne["restSsl"].as<bool>(false);

    // initialize websockets
    m_wsServer.init_asio();
    m_wsServer.set_reuse_addr(true);
    m_wsServer.set_open_handler(websocketpp::lib::bind(&HostWS::wsOnConOpen, this, 
        websocketpp::lib::placeholders::_1));
    m_wsServer.set_close_handler(websocketpp::lib::bind(&HostWS::wsOnConClose, this, 
        websocketpp::lib::placeholders::_1));
    m_wsServer.set_message_handler(websocketpp::lib::bind(&HostWS::wsOnMessage, this, 
        websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));

    if (m_debug) {
        m_wsServer.set_access_channels(websocketpp::log::alevel::all);
    } else {
        m_wsServer.set_access_channels(websocketpp::log::alevel::none);
    }

    /** WebSocket Thread */
    if (!Thread::runAsThread(this, threadWebSocket))
        return EXIT_FAILURE;

    ::LogInfoEx(LOG_HOST, "SysView is up and running");

    StopWatch stopWatch;
    stopWatch.start();

    g_logDisplayLevel = 0U;

    std::ostringstream logOutput;
    __InternalOutputStream(logOutput);

    Timer peerListUpdate(1000U, 10U);
    peerListUpdate.start();
    Timer affListUpdate(1000U, 10U);
    affListUpdate.start();
    Timer peerStatusUpdate(1000U, 0U, 175U);
    peerStatusUpdate.start();

    Timer tgDataUpdate(1000U, 30U);
    tgDataUpdate.start();
    Timer ridDataUpdate(1000U, 30U);
    ridDataUpdate.start();

    setNetDataEventCallback([=](json::object obj) { netDataEvent(obj); });

    // main execution loop
    while (!g_killed) {
        uint32_t ms = stopWatch.elapsed();

        ms = stopWatch.elapsed();
        stopWatch.start();

        if (m_wsConList.size() > 0U) {
            // send log messages
            if (!logOutput.str().empty()) {
                std::string str = std::string(logOutput.str());
                logOutput.str("");

                json::object wsObj = json::object();
                std::string type = "log";
                wsObj["type"].set<std::string>(type);
                wsObj["payload"].set<std::string>(str);
                send(wsObj);
            }

            // update peer status
            peerStatusUpdate.clock(ms);
            if (peerStatusUpdate.isRunning() && peerStatusUpdate.hasExpired()) {
                peerStatusUpdate.start();

                getNetwork()->lockPeerStatus();
                std::map<uint32_t, json::object> peerStatus(getNetwork()->peerStatus.begin(), getNetwork()->peerStatus.end());
                getNetwork()->unlockPeerStatus();

                for (auto entry : peerStatus) {
                    json::object wsObj = json::object();
                    std::string type = "peer_status";
                    wsObj["type"].set<std::string>(type);
                    uint32_t peerId = entry.first;
                    wsObj["peerId"].set<uint32_t>(peerId);
                    json::object peerStatus = entry.second;
                    wsObj["payload"].set<json::object>(peerStatus);
                    send(wsObj);
                }
            }

            // update peer list data
            peerListUpdate.clock(ms);
            if (peerListUpdate.isRunning() && peerListUpdate.hasExpired()) {
                peerListUpdate.start();
                // callback REST API to get status of the channel we represent
                json::object req = json::object();
                json::object rsp = json::object();
            
                int ret = RESTClient::send(fneRESTAddress, fneRESTPort, fnePassword,
                    HTTP_GET, FNE_GET_PEER_QUERY, req, rsp, fneSSL, g_debug);
                if (ret != network::rest::http::HTTPPayload::StatusType::OK) {
                    ::LogError(LOG_HOST, "[AFFVIEW] failed to query peers for %s:%u", fneRESTAddress.c_str(), fneRESTPort);
                }
                else {
                    try {
                        json::object wsObj = json::object();
                        std::string type = "peer_list";
                        wsObj["type"].set<std::string>(type);
                        wsObj["payload"].set<json::object>(rsp);
                        send(wsObj);
                    }
                    catch (std::exception& e) {
                        ::LogWarning(LOG_HOST, "[AFFVIEW] %s:%u, failed to properly handle peer query request, %s", fneRESTAddress.c_str(), fneRESTPort, e.what());
                    }
                }
            }

            // update affiliation list data
            affListUpdate.clock(ms);
            if (affListUpdate.isRunning() && affListUpdate.hasExpired()) {
                affListUpdate.start();
                // callback REST API to get status of the channel we represent
                json::object req = json::object();
                json::object rsp = json::object();
            
                int ret = RESTClient::send(fneRESTAddress, fneRESTPort, fnePassword,
                    HTTP_GET, FNE_GET_AFF_LIST, req, rsp, fneSSL, g_debug);
                if (ret != network::rest::http::HTTPPayload::StatusType::OK) {
                    ::LogError(LOG_HOST, "[AFFVIEW] failed to query peers for %s:%u", fneRESTAddress.c_str(), fneRESTPort);
                }
                else {
                    try {
                        json::object wsObj = json::object();
                        std::string type = "aff_list";
                        wsObj["type"].set<std::string>(type);
                        wsObj["payload"].set<json::object>(rsp);
                        send(wsObj);
                    }
                    catch (std::exception& e) {
                        ::LogWarning(LOG_HOST, "[AFFVIEW] %s:%u, failed to properly handle peer query request, %s", fneRESTAddress.c_str(), fneRESTPort, e.what());
                    }
                }
            }

            // send full talkgroup list data
            tgDataUpdate.clock(ms);
            if (tgDataUpdate.isRunning() && tgDataUpdate.hasExpired()) {
                tgDataUpdate.start();

                json::object wsObj = json::object();
                std::string type = "tg_data";

                json::array tgs = json::array();
                if (g_tidLookup != nullptr) {
                    if (g_tidLookup->groupVoice().size() > 0) {
                        for (auto entry : g_tidLookup->groupVoice()) {
                            json::object tg = tgToJson(entry);
                            tgs.push_back(json::value(tg));
                        }
                    }
                }

                wsObj["payload"].set<json::array>(tgs);
                send(wsObj);
            }

            // send full radio ID list data
            ridDataUpdate.clock(ms);
            if (ridDataUpdate.isRunning() && ridDataUpdate.hasExpired()) {
                ridDataUpdate.start();

                json::object wsObj = json::object();
                std::string type = "rid_data";

                json::array rids = json::array();
                if (g_ridLookup != nullptr) {
                    if (g_ridLookup->table().size() > 0) {
                        for (auto entry : g_ridLookup->table()) {
                            json::object ridObj = json::object();

                            uint32_t rid = entry.first;
                            ridObj["id"].set<uint32_t>(rid);
                            bool enabled = entry.second.radioEnabled();
                            ridObj["enabled"].set<bool>(enabled);
                            std::string alias = entry.second.radioAlias();
                            ridObj["alias"].set<std::string>(alias);

                            rids.push_back(json::value(ridObj));
                        }
                    }
                }

                wsObj["payload"].set<json::array>(rids);
                send(wsObj);
            }
        } else {
            // clear ostream
            logOutput.str("");
        }

        if (ms < 2U)
            Thread::sleep(1U);
    }

    g_logDisplayLevel = 1U;

    if (g_killed)
        m_wsServer.stop();

    return EXIT_SUCCESS;
}

/* */

void HostWS::send(json::object obj)
{
    try {
        json::value v = json::value(obj);
        std::string json = std::string(v.serialize());

        wsConList::iterator it;
        for (it = m_wsConList.begin(); it != m_wsConList.end(); ++it) {
            m_wsServer.send(*it, json, websocketpp::frame::opcode::text);
        }
    }
    catch (websocketpp::exception) { /* stub */ }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Reads basic configuration parameters from the YAML configuration file. */

bool HostWS::readParams()
{
    yaml::Node websocketConf = m_conf["websocket"];
    m_websocketPort = websocketConf["port"].as<uint16_t>(8443U);
    m_debug = websocketConf["debug"].as<bool>(false);

    LogInfo("General Parameters");
    LogInfo("    Port: %u", m_websocketPort);

    if (m_debug) {
        LogInfo("    Debug: yes");
    }

    return true;
}

/* Called when a network data event occurs. */

void HostWS::netDataEvent(json::object obj)
{
    json::object wsObj = json::object();
    std::string type = "net_event";
    wsObj["type"].set<std::string>(type);
    wsObj["payload"].set<json::object>(obj);
    send(wsObj);
}

/* Called when a WebSocket connection is opened. */

void HostWS::wsOnConOpen(websocketpp::connection_hdl handle)
{
    m_wsConList.insert(handle);
}

/* Called when a WebSocket connection is closed. */

void HostWS::wsOnConClose(websocketpp::connection_hdl handle)
{
    m_wsConList.erase(handle);
}

/* Called when a WebSocket message is received. */

void HostWS::wsOnMessage(websocketpp::connection_hdl handle, wsServer::message_ptr msg)
{
    /* stub */
}

/* Entry point to WebSocket thread. */

void* HostWS::threadWebSocket(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
#if defined(_WIN32)
        ::CloseHandle(th->thread);
#else
        ::pthread_detach(th->thread);
#endif // defined(_WIN32)

        std::string threadName("sysview:ws-thread");
        HostWS* ws = (HostWS*)th->obj;
        if (ws == nullptr) {
            delete th;
            return nullptr;
        }

        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogDebug(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        ws->m_wsServer.listen(websocketpp::lib::asio::ip::tcp::v4(), ws->m_websocketPort);
        ws->m_wsServer.start_accept();
        ws->m_wsServer.run();

        LogDebug(LOG_HOST, "[STOP] %s", threadName.c_str());
        delete th;
    }

    return nullptr;
}

#endif // !defined(NO_WEBSOCKETS)
