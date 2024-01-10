/**
* Digital Voice Modem - Conference FNE Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Conference FNE Software
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
#include "fne/Defines.h"
#include "common/edac/SHA256.h"
#include "common/lookups/AffiliationLookup.h"
#include "common/network/json/json.h"
#include "common/Log.h"
#include "common/Thread.h"
#include "common/Utils.h"
#include "fne/network/RESTAPI.h"
#include "HostFNE.h"
#include "FNEMain.h"

using namespace network;
using namespace network::rest;
using namespace network::rest::http;

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>

#include <memory>
#include <stdexcept>
#include <unordered_map>

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

#define REST_API_BIND(funcAddr, classInstance) std::bind(&funcAddr, classInstance, std::placeholders::_1,  std::placeholders::_2, std::placeholders::_3)

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

template<typename ... FormatArgs>
std::string string_format(const std::string& format, FormatArgs ... args)
{
    int size_s = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; // extra space for '\0'
    if (size_s <= 0)
        throw std::runtime_error("Error during string formatting.");

    auto size = static_cast<size_t>(size_s);
    std::unique_ptr<char[]> buf(new char[ size ]);
    std::snprintf(buf.get(), size, format.c_str(), args ...);

    return std::string(buf.get(), buf.get() + size - 1);
}

/// <summary>
///
/// </summary>
/// <param name="obj"></param>
void setResponseDefaultStatus(json::object& obj)
{
    int s = (int)HTTPPayload::OK;
    obj["status"].set<int>(s);
}

/// <summary>
///
/// </summary>
/// <param name="reply"></param>
/// <param name="message"></param>
/// <param name="status"></param>
void errorPayload(HTTPPayload& reply, std::string message, HTTPPayload::StatusType status = HTTPPayload::BAD_REQUEST)
{
    HTTPPayload rep;
    rep.status = status;

    json::object response = json::object();

    int s = (int)rep.status;
    response["status"].set<int>(s);
    response["message"].set<std::string>(message);

    reply.payload(response);
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="obj"></param>
/// <returns></returns>
bool parseRequestBody(const HTTPPayload& request, HTTPPayload& reply, json::object& obj)
{
    std::string contentType = request.headers.find("Content-Type");
    if (contentType != "application/json") {
        reply = HTTPPayload::statusPayload(HTTPPayload::BAD_REQUEST, "application/json");
        return false;
    }

    // parse JSON body
    json::value v;
    std::string err = json::parse(v, request.content);
    if (!err.empty()) {
        errorPayload(reply, err);
        return false;
    }

    // ensure parsed JSON is an object
    if (!v.is<json::object>()) {
        errorPayload(reply, "Request was not a valid JSON object.");
        return false;
    }

    obj = v.get<json::object>();
    return true;
}

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the RESTAPI class.
/// </summary>
/// <param name="address">Network Hostname/IP address to connect to.</param>
/// <param name="port">Network port number.</param>
/// <param name="password">Authentication password.</param>
/// <param name="host">Instance of the Host class.</param>
/// <param name="debug"></param>
RESTAPI::RESTAPI(const std::string& address, uint16_t port, const std::string& password, HostFNE* host, bool debug) :
    m_dispatcher(debug),
    m_restServer(address, port),
    m_random(),
    m_password(password),
    m_passwordHash(nullptr),
    m_debug(debug),
    m_host(host),
    m_network(nullptr),
    m_ridLookup(nullptr),
    m_tidLookup(nullptr),
    m_authTokens()
{
    assert(!address.empty());
    assert(port > 0U);
    assert(!password.empty());

    size_t size = password.size();

    uint8_t* in = new uint8_t[size];
    for (size_t i = 0U; i < size; i++)
        in[i] = password.at(i);

    m_passwordHash = new uint8_t[32U];
    ::memset(m_passwordHash, 0x00U, 32U);

    edac::SHA256 sha256;
    sha256.buffer(in, (uint32_t)(size), m_passwordHash);

    delete[] in;

    if (m_debug) {
        Utils::dump("REST Password Hash", m_passwordHash, 32U);
    }

    std::random_device rd;
    std::mt19937 mt(rd());
    m_random = mt;
}

/// <summary>
/// Finalizes a instance of the RESTAPI class.
/// </summary>
RESTAPI::~RESTAPI()
{
    /* stub */
}

/// <summary>
/// Sets the instances of the Radio ID and Talkgroup ID lookup tables.
/// </summary>
/// <param name="ridLookup">Radio ID Lookup Table Instance</param>
/// <param name="tidLookup">Talkgroup Rules Lookup Table Instance</param>
void RESTAPI::setLookups(lookups::RadioIdLookup* ridLookup, lookups::TalkgroupRulesLookup* tidLookup)
{
    m_ridLookup = ridLookup;
    m_tidLookup = tidLookup;
}

/// <summary>
/// Sets the instance of the FNE network.
/// </summary>
/// <param name="network">FNE Network Instance</param>
void RESTAPI::setNetwork(network::FNENetwork* network)
{
    m_network = network;
}

/// <summary>
/// Opens connection to the network.
/// </summary>
/// <returns></returns>
bool RESTAPI::open()
{
    initializeEndpoints();
    m_restServer.setHandler(m_dispatcher);

    return run();
}

/// <summary>
/// Closes connection to the network.
/// </summary>
void RESTAPI::close()
{
    m_restServer.stop();
    wait();
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
///
/// </summary>
void RESTAPI::entry()
{
    m_restServer.run();
}

/// <summary>
/// Helper to initialize REST API endpoints.
/// </summary>
void RESTAPI::initializeEndpoints()
{
    m_dispatcher.match(PUT_AUTHENTICATE).put(REST_API_BIND(RESTAPI::restAPI_PutAuth, this));

    m_dispatcher.match(GET_VERSION).get(REST_API_BIND(RESTAPI::restAPI_GetVersion, this));
    m_dispatcher.match(GET_STATUS).get(REST_API_BIND(RESTAPI::restAPI_GetStatus, this));
    m_dispatcher.match(FNE_GET_PEERLIST).get(REST_API_BIND(RESTAPI::restAPI_GetPeerList, this));
    m_dispatcher.match(FNE_GET_TGID_LIST).get(REST_API_BIND(RESTAPI::restAPI_GetTGIDList, this));

    m_dispatcher.match(FNE_GET_FORCE_UPDATE).get(REST_API_BIND(RESTAPI::restAPI_GetForceUpdate, this));
}

/// <summary>
///
/// </summary>
/// <param name="host"></param>
void RESTAPI::invalidateHostToken(const std::string host)
{
    auto token = std::find_if(m_authTokens.begin(), m_authTokens.end(), [&](const AuthTokenValueType& tok) { return tok.first == host; });
    if (token != m_authTokens.end()) {
        m_authTokens.erase(host);
    }
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
bool RESTAPI::validateAuth(const HTTPPayload& request, HTTPPayload& reply)
{
    std::string host = request.headers.find("RemoteHost");
    std::string headerToken = request.headers.find("X-DVM-Auth-Token");
#if DEBUG_HTTP_PAYLOAD
    ::LogDebug(LOG_REST, "RESTAPI::validateAuth() token, host = %s, token = %s", host.c_str(), headerToken.c_str());
#endif
    if (headerToken == "") {
        errorPayload(reply, "no authentication token", HTTPPayload::UNAUTHORIZED);
        return false;
    }

    for (auto& token : m_authTokens) {
#if DEBUG_HTTP_PAYLOAD
        ::LogDebug(LOG_REST, "RESTAPI::validateAuth() valid list, host = %s, token = %s", token.first.c_str(), std::to_string(token.second).c_str());
#endif
        if (token.first.compare(host) == 0) {
#if DEBUG_HTTP_PAYLOAD
            ::LogDebug(LOG_REST, "RESTAPI::validateAuth() storedToken = %s, passedToken = %s", std::to_string(token.second).c_str(), headerToken.c_str());
#endif
            if (std::to_string(token.second).compare(headerToken) == 0) {
                return true;
            } else {
                m_authTokens.erase(host); // devalidate host
                errorPayload(reply, "invalid authentication token", HTTPPayload::UNAUTHORIZED);
                return false;
            }
        }
    }

    errorPayload(reply, "illegal authentication token", HTTPPayload::UNAUTHORIZED);
    return false;
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_PutAuth(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    std::string host = request.headers.find("RemoteHost");
    json::object response = json::object();
    setResponseDefaultStatus(response);

    json::object req = json::object();
    if (!parseRequestBody(request, reply, req)) {
        return;
    }

    // validate auth is a string within the JSON blob
    if (!req["auth"].is<std::string>()) {
        invalidateHostToken(host);
        errorPayload(reply, "password was not a valid string");
        return;
    }

    std::string auth = req["auth"].get<std::string>();
    if (auth.empty()) {
        invalidateHostToken(host);
        errorPayload(reply, "auth cannot be empty");
        return;
    }

    if (auth.size() > 64) {
        invalidateHostToken(host);
        errorPayload(reply, "auth cannot be longer than 64 characters");
        return;
    }

    if (!(auth.find_first_not_of("0123456789abcdefABCDEF", 2) == std::string::npos)) {
        invalidateHostToken(host);
        errorPayload(reply, "auth contains invalid characters");
        return;
    }

    if (m_debug) {
        ::LogDebug(LOG_REST, "/auth auth = %s", auth.c_str());
    }

    const char* authPtr = auth.c_str();
    uint8_t* passwordHash = new uint8_t[32U];
    ::memset(passwordHash, 0x00U, 32U);

    for (uint8_t i = 0; i < 32U; i++) {
        char t[4] = {authPtr[0], authPtr[1], 0};
        passwordHash[i] = (uint8_t)::strtoul(t, NULL, 16);
        authPtr += 2 * sizeof(char);
    }

    if (m_debug) {
        Utils::dump("Password Hash", passwordHash, 32U);
    }

    // compare hashes
    if (::memcmp(m_passwordHash, passwordHash, 32U) != 0) {
        invalidateHostToken(host);
        errorPayload(reply, "invalid password");
        return;
    }

    delete[] passwordHash;

    invalidateHostToken(host);
    std::uniform_int_distribution<uint64_t> dist(DVM_RAND_MIN, DVM_REST_RAND_MAX);
    uint64_t salt = dist(m_random);

    m_authTokens[host] = salt;
    response["token"].set<std::string>(std::to_string(salt));
    reply.payload(response);
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetVersion(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object response = json::object();
    setResponseDefaultStatus(response);
    response["version"].set<std::string>(std::string((__PROG_NAME__ " " __VER__ " (built " __BUILD__ ")")));

    reply.payload(response);
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetStatus(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object response = json::object();
    setResponseDefaultStatus(response);

    yaml::Node systemConf = m_host->m_conf["system"];
    yaml::Node masterConf = m_host->m_conf["master"];
    {
        uint8_t state = FNE_STATE;
        response["state"].set<uint8_t>(state);
        response["dmrEnabled"].set<bool>(m_host->m_dmrEnabled);
        response["p25Enabled"].set<bool>(m_host->m_p25Enabled);
        response["nxdnEnabled"].set<bool>(m_host->m_nxdnEnabled);

        uint32_t peerId = masterConf["peerId"].as<uint32_t>();
        response["peerId"].set<uint32_t>(peerId);
    }

    reply.payload(response);
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetPeerList(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object response = json::object();
    setResponseDefaultStatus(response);

    json::array peers = json::array();
    if (m_network != nullptr) {
        if (m_network->m_peers.size() > 0) {
            for (auto entry : m_network->m_peers) {
                uint32_t peerId = entry.first;
                network::FNEPeerConnection peer = entry.second;

                json::object peerObj = json::object();
                peerObj["peerId"].set<uint32_t>(peerId);

                std::string address = peer.address();
                peerObj["address"].set<std::string>(address);
                uint16_t port = peer.port();
                peerObj["port"].set<uint16_t>(port);
                bool connected = peer.connected();
                peerObj["connected"].set<bool>(connected);
                uint32_t connectionState = (uint32_t)peer.connectionState();
                peerObj["connectionState"].set<uint32_t>(connectionState);
                uint32_t pingsReceived = peer.pingsReceived();
                peerObj["pingsReceived"].set<uint32_t>(pingsReceived);
                uint64_t lastPing = peer.lastPing();
                peerObj["lastPing"].set<uint64_t>(lastPing);

                json::object peerConfig = peer.config();
                if (peerConfig["rcon"].is<json::object>())
                    peerConfig.erase("rcon");
                peerObj["config"].set<json::object>(peerConfig);

                peers.push_back(json::value(peerObj));
            }
        }
    }

    response["peers"].set<json::array>(peers);
    reply.payload(response);
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetTGIDList(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object response = json::object();
    setResponseDefaultStatus(response);

    json::array tgs = json::array();
    if (m_tidLookup != nullptr) {
        if (m_tidLookup->groupVoice().size() > 0) {
            for (auto entry : m_tidLookup->groupVoice()) {
                json::object tg = json::object();

                std::string tgName = entry.name();
                tg["name"].set<std::string>(tgName);
                bool invalid = entry.isInvalid();
                tg["invalid"].set<bool>(invalid);

                {
                    json::object source = json::object();
                    uint32_t tgId = entry.source().tgId();
                    source["tgid"].set<uint32_t>(tgId);
                    uint8_t tgSlot = entry.source().tgSlot();
                    source["slot"].set<uint8_t>(tgSlot);
                    tg["source"].set<json::object>(source);
                }

                {
                    json::object config = json::object();
                    bool active = entry.config().active();
                    config["active"].set<bool>(active);
                    bool parrot = entry.config().parrot();
                    config["parrot"].set<bool>(parrot);

                    json::array inclusions = json::array();
                    std::vector<uint32_t> inclusion = entry.config().inclusion();
                    if (inclusion.size() > 0) {
                        for (auto inclEntry : inclusion) {
                            uint32_t peerId = inclEntry;
                            inclusions.push_back(json::value((double)peerId));
                        }
                    }
                    config["inclusion"].set<json::array>(inclusions);

                    json::array exclusions = json::array();
                    std::vector<uint32_t> exclusion = entry.config().exclusion();
                    if (exclusion.size() > 0) {
                        for (auto exclEntry : exclusion) {
                            uint32_t peerId = exclEntry;
                            exclusions.push_back(json::value((double)peerId));
                        }
                    }
                    config["exclusion"].set<json::array>(exclusions);

                    json::array rewrites = json::array();
                    std::vector<lookups::TalkgroupRuleRewrite> rewrite = entry.config().rewrite();
                    if (rewrite.size() > 0) {
                        for (auto rewrEntry : rewrite) {
                            json::object rewrite = json::object();
                            uint32_t peerId = rewrEntry.peerId();
                            rewrite["peerId"].set<uint32_t>(peerId);
                            uint32_t tgId = rewrEntry.tgId();
                            rewrite["tgid"].set<uint32_t>(tgId);
                            uint8_t tgSlot = rewrEntry.tgSlot();
                            rewrite["slot"].set<uint8_t>(tgSlot);

                            exclusions.push_back(json::value(rewrite));
                        }
                    }
                    config["rewrite"].set<json::array>(rewrites);
                    tg["config"].set<json::object>(config);
                }

                tgs.push_back(json::value(tg));
            }
        }
    }

    response["tgs"].set<json::array>(tgs);
    reply.payload(response);
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetForceUpdate(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object response = json::object();
    setResponseDefaultStatus(response);
    if (m_network != nullptr) {
        m_network->m_forceListUpdate = true;
    }

    reply.payload(response);
}
