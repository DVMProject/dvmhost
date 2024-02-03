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
#include "fne/Defines.h"
#include "common/edac/SHA256.h"
#include "common/lookups/AffiliationLookup.h"
#include "common/network/json/json.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "fne/network/RESTAPI.h"
#include "HostFNE.h"

using namespace network;
using namespace network::rest;
using namespace network::rest::http;

using namespace lookups;

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

/// <summary>
/// Helper to convert a <see cref="TalkgroupRuleGroupVoice"/> to JSON.
/// </summary>
/// <param name="groupVoice"></param>
/// <returns></returns>
json::object tgToJson(const TalkgroupRuleGroupVoice& groupVoice) 
{
    json::object tg = json::object();

    std::string tgName = groupVoice.name();
    tg["name"].set<std::string>(tgName);
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

                exclusions.push_back(json::value(rewrite));
            }
        }
        config["rewrite"].set<json::array>(rewrites);
        tg["config"].set<json::object>(config);
    }

    return tg;
}

/// <summary>
/// Helper to convert JSON to a <see cref="TalkgroupRuleGroupVoice"/>.
/// </summary>
/// <param name="req"></param>
/// <returns></returns>
TalkgroupRuleGroupVoice jsonToTG(json::object& req, HTTPPayload& reply) 
{
    TalkgroupRuleGroupVoice groupVoice = TalkgroupRuleGroupVoice();

    // validate parameters
    if (!req["name"].is<std::string>()) {
        errorPayload(reply, "TG \"name\" was not a valid string");
        return TalkgroupRuleGroupVoice();
    }

    groupVoice.name(req["name"].get<std::string>());

    // source stanza
    {
        if (!req["source"].is<json::object>()) {
            errorPayload(reply, "TG \"source\" was not a valid JSON object");
            return TalkgroupRuleGroupVoice();
        }
        json::object sourceObj = req["source"].get<json::object>();

        if (!sourceObj["tgid"].is<uint32_t>()) {
            errorPayload(reply, "TG source \"tgid\" was not a valid number");
            return TalkgroupRuleGroupVoice();
        }

        if (!sourceObj["slot"].is<uint8_t>()) {
            errorPayload(reply, "TG source \"slot\" was not a valid number");
            return TalkgroupRuleGroupVoice();
        }

        TalkgroupRuleGroupVoiceSource source = groupVoice.source();
        source.tgId(sourceObj["tgid"].get<uint32_t>());
        source.tgSlot(sourceObj["slot"].get<uint8_t>());

        groupVoice.source(source);
    }

    // config stanza
    {
        if (!req["config"].is<json::object>()) {
            errorPayload(reply, "TG \"config\" was not a valid JSON object");
            return TalkgroupRuleGroupVoice();
        }
        json::object configObj = req["config"].get<json::object>();

        if (!configObj["active"].is<bool>()) {
            errorPayload(reply, "TG configuration \"active\" was not a valid boolean");
            return TalkgroupRuleGroupVoice();
        }

        if (!configObj["affiliated"].is<bool>()) {
            errorPayload(reply, "TG configuration \"affiliated\" was not a valid boolean");
            return TalkgroupRuleGroupVoice();
        }

        if (!configObj["parrot"].is<bool>()) {
            errorPayload(reply, "TG configuration \"parrot\" slot was not a valid boolean");
            return TalkgroupRuleGroupVoice();
        }

        TalkgroupRuleConfig config = groupVoice.config();
        config.active(configObj["active"].get<bool>());
        config.affiliated(configObj["affiliated"].get<bool>());
        config.parrot(configObj["parrot"].get<bool>());

        if (!req["inclusion"].is<json::array>()) {
            errorPayload(reply, "TG \"inclusion\" was not a valid JSON array");
            return TalkgroupRuleGroupVoice();
        }
        json::array inclusions = req["inclusion"].get<json::array>();

        std::vector<uint32_t> inclusion = groupVoice.config().inclusion();
        if (inclusions.size() > 0) {
            for (auto inclEntry : inclusions) {
                if (!inclEntry.is<uint32_t>()) {
                    errorPayload(reply, "TG inclusion value was not a valid number");
                    return TalkgroupRuleGroupVoice();
                }

                inclusion.push_back(inclEntry.get<uint32_t>());
            }
            config.inclusion(inclusion);
        }

        if (!req["exclusion"].is<json::array>()) {
            errorPayload(reply, "TG \"exclusion\" was not a valid JSON array");
            return TalkgroupRuleGroupVoice();
        }
        json::array exclusions = req["exclusion"].get<json::array>();

        std::vector<uint32_t> exclusion = groupVoice.config().exclusion();
        if (exclusions.size() > 0) {
            for (auto exclEntry : exclusions) {
                if (!exclEntry.is<uint32_t>()) {
                    errorPayload(reply, "TG exclusion value was not a valid number");
                    return TalkgroupRuleGroupVoice();
                }

                exclusion.push_back(exclEntry.get<uint32_t>());
            }
            config.exclusion(exclusion);
        }

        if (!req["rewrites"].is<json::array>()) {
            errorPayload(reply, "TG \"rewrites\" was not a valid JSON array");
            return TalkgroupRuleGroupVoice();
        }
        json::array rewrites = req["rewrites"].get<json::array>();

        std::vector<lookups::TalkgroupRuleRewrite> rewrite = groupVoice.config().rewrite();
        if (rewrites.size() > 0) {
            for (auto rewrEntry : rewrites) {
                if (!rewrEntry.is<json::object>()) {
                    errorPayload(reply, "TG rewrite value was not a valid JSON object");
                    return TalkgroupRuleGroupVoice();
                }
                json::object rewriteObj = rewrEntry.get<json::object>();

                TalkgroupRuleRewrite rewriteRule = TalkgroupRuleRewrite();

                if (!rewriteObj["peerid"].is<uint32_t>()) {
                    errorPayload(reply, "TG rewrite rule \"peerid\" was not a valid number");
                    return TalkgroupRuleGroupVoice();
                }

                if (!rewriteObj["tgid"].is<uint32_t>()) {
                    errorPayload(reply, "TG rewrite rule \"tgid\" was not a valid number");
                    return TalkgroupRuleGroupVoice();
                }

                if (!rewriteObj["slot"].is<uint8_t>()) {
                    errorPayload(reply, "TG rewrite rule \"slot\" was not a valid number");
                    return TalkgroupRuleGroupVoice();
                }

                rewriteRule.peerId(rewriteObj["peerid"].get<uint32_t>());
                rewriteRule.tgId(rewriteObj["tgid"].get<uint32_t>());
                rewriteRule.tgSlot(rewriteObj["slot"].get<uint8_t>());

                rewrite.push_back(rewriteRule);
            }
            config.rewrite(rewrite);
        }

        groupVoice.config(config);
    }

    return groupVoice;
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
RESTAPI::~RESTAPI() = default;

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

    m_dispatcher.match(FNE_GET_PEER_QUERY).get(REST_API_BIND(RESTAPI::restAPI_GetPeerQuery, this));

    m_dispatcher.match(FNE_GET_RID_QUERY).get(REST_API_BIND(RESTAPI::restAPI_GetRIDQuery, this));
    m_dispatcher.match(FNE_PUT_RID_ADD).put(REST_API_BIND(RESTAPI::restAPI_PutRIDAdd, this));
    m_dispatcher.match(FNE_PUT_RID_DELETE).put(REST_API_BIND(RESTAPI::restAPI_PutRIDDelete, this));
    m_dispatcher.match(FNE_GET_RID_COMMIT).get(REST_API_BIND(RESTAPI::restAPI_GetRIDCommit, this));

    m_dispatcher.match(FNE_GET_TGID_QUERY).get(REST_API_BIND(RESTAPI::restAPI_GetTGQuery, this));
    m_dispatcher.match(FNE_PUT_TGID_ADD).put(REST_API_BIND(RESTAPI::restAPI_PutTGAdd, this));
    m_dispatcher.match(FNE_PUT_TGID_DELETE).put(REST_API_BIND(RESTAPI::restAPI_PutTGDelete, this));
    m_dispatcher.match(FNE_GET_TGID_COMMIT).get(REST_API_BIND(RESTAPI::restAPI_GetTGCommit, this));

    m_dispatcher.match(FNE_GET_FORCE_UPDATE).get(REST_API_BIND(RESTAPI::restAPI_GetForceUpdate, this));

    m_dispatcher.match(FNE_GET_AFF_LIST).get(REST_API_BIND(RESTAPI::restAPI_GetAffList, this));
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
    if (headerToken.empty()) {
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
void RESTAPI::restAPI_GetPeerQuery(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
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
                network::FNEPeerConnection* peer = entry.second;
                if (peer != nullptr) {
                    json::object peerObj = json::object();
                    peerObj["peerId"].set<uint32_t>(peerId);

                    std::string address = peer->address();
                    peerObj["address"].set<std::string>(address);
                    uint16_t port = peer->port();
                    peerObj["port"].set<uint16_t>(port);
                    bool connected = peer->connected();
                    peerObj["connected"].set<bool>(connected);
                    uint32_t connectionState = (uint32_t)peer->connectionState();
                    peerObj["connectionState"].set<uint32_t>(connectionState);
                    uint32_t pingsReceived = peer->pingsReceived();
                    peerObj["pingsReceived"].set<uint32_t>(pingsReceived);
                    uint64_t lastPing = peer->lastPing();
                    peerObj["lastPing"].set<uint64_t>(lastPing);

                    json::object peerConfig = peer->config();
                    if (peerConfig["rcon"].is<json::object>())
                        peerConfig.erase("rcon");
                    peerObj["config"].set<json::object>(peerConfig);

                    peers.push_back(json::value(peerObj));
                }
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
void RESTAPI::restAPI_GetRIDQuery(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object response = json::object();
    setResponseDefaultStatus(response);

    json::array rids = json::array();
    if (m_ridLookup != nullptr) {
        if (m_ridLookup->table().size() > 0) {
            for (auto entry : m_ridLookup->table()) {
                json::object ridObj = json::object();

                uint32_t rid = entry.first;
                ridObj["id"].set<uint32_t>(rid);
                bool enabled = entry.second.radioEnabled();
                ridObj["enabled"].set<bool>(enabled);

                rids.push_back(json::value(ridObj));
            }
        }
    }

    response["rids"].set<json::array>(rids);
    reply.payload(response);
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_PutRIDAdd(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object req = json::object();
    if (!parseRequestBody(request, reply, req)) {
        return;
    }

    errorPayload(reply, "OK", HTTPPayload::OK);

    if (!req["rid"].is<uint32_t>()) {
        errorPayload(reply, "rid was not a valid integer");
        return;
    }

    uint32_t rid = req["rid"].get<uint32_t>();

    if (!req["enabled"].is<bool>()) {
        errorPayload(reply, "enabled was not a valid boolean");
        return;
    }

    bool enabled = req["enabled"].get<bool>();

    RadioId radioId = m_ridLookup->find(rid);
    if (radioId.radioDefault()) {
        m_ridLookup->addEntry(rid, enabled);
    }
    else {
        m_ridLookup->toggleEntry(rid, enabled);
    }

    if (m_network != nullptr) {
        m_network->m_forceListUpdate = true;
    }
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_PutRIDDelete(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object req = json::object();
    if (!parseRequestBody(request, reply, req)) {
        return;
    }

    errorPayload(reply, "OK", HTTPPayload::OK);

    if (!req["rid"].is<uint32_t>()) {
        errorPayload(reply, "rid was not a valid integer");
        return;
    }

    uint32_t rid = req["rid"].get<uint32_t>();

    RadioId radioId = m_ridLookup->find(rid);
    if (radioId.radioDefault()) {
        errorPayload(reply, "failed to find specified RID to delete");
        return;
    }

    m_ridLookup->eraseEntry(rid);

    if (m_network != nullptr) {
        m_network->m_forceListUpdate = true;
    }
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetRIDCommit(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object response = json::object();
    setResponseDefaultStatus(response);

    m_ridLookup->commit();

    reply.payload(response);
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetTGQuery(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
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
                json::object tg = tgToJson(entry);
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
void RESTAPI::restAPI_PutTGAdd(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object req = json::object();
    if (!parseRequestBody(request, reply, req)) {
        return;
    }

    errorPayload(reply, "OK", HTTPPayload::OK);

    TalkgroupRuleGroupVoice groupVoice = jsonToTG(req, reply);
    if (groupVoice.isInvalid()) {
        return;
    }

    std::string groupName = groupVoice.name();
    uint32_t tgId = groupVoice.source().tgId();
    uint8_t tgSlot = groupVoice.source().tgSlot();
    bool active = groupVoice.config().active();
    bool parrot = groupVoice.config().parrot();

    uint32_t incCount = groupVoice.config().inclusion().size();
    uint32_t excCount = groupVoice.config().exclusion().size();
    uint32_t rewrCount = groupVoice.config().rewrite().size();

    if (incCount > 0 && excCount > 0) {
        ::LogWarning(LOG_REST, "Talkgroup (%s) defines both inclusions and exclusions! Inclusions take precedence and exclusions will be ignored.", groupName.c_str());
    }

    ::LogInfoEx(LOG_REST, "Talkgroup NAME: %s SRC_TGID: %u SRC_TS: %u ACTIVE: %u PARROT: %u INCLUSIONS: %u EXCLUSIONS: %u REWRITES: %u", groupName.c_str(), tgId, tgSlot, active, parrot, incCount, excCount, rewrCount);

    m_tidLookup->addEntry(groupVoice);

    if (m_network != nullptr) {
        m_network->m_forceListUpdate = true;
    }
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_PutTGDelete(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object req = json::object();
    if (!parseRequestBody(request, reply, req)) {
        return;
    }

    errorPayload(reply, "OK", HTTPPayload::OK);

    // validate state is a string within the JSON blob
    if (!req["tgid"].is<uint32_t>()) {
        errorPayload(reply, "tgid was not a valid integer");
        return;
    }

    uint32_t tgid = req["tgid"].get<uint32_t>();

    TalkgroupRuleGroupVoice groupVoice = m_tidLookup->find(tgid);
    if (groupVoice.isInvalid()) {
        errorPayload(reply, "failed to find specified TGID to delete");
        return;
    }

    m_tidLookup->eraseEntry(groupVoice.source().tgId(), groupVoice.source().tgSlot());

    if (m_network != nullptr) {
        m_network->m_forceListUpdate = true;
    }
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetTGCommit(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object response = json::object();
    setResponseDefaultStatus(response);

    m_tidLookup->commit();

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

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetAffList(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object response = json::object();
    setResponseDefaultStatus(response);

    json::array affs = json::array();
    if (m_network != nullptr) {
        if (m_network->m_peers.size() > 0) {
            for (auto entry : m_network->m_peers) {
                uint32_t peerId = entry.first;
                network::FNEPeerConnection* peer = entry.second;
                if (peer != nullptr) {
                    lookups::AffiliationLookup* affLookup = m_network->m_peerAffiliations[peerId];
                    std::unordered_map<uint32_t, uint32_t> affTable = affLookup->grpAffTable();

                    json::object peerObj = json::object();
                    peerObj["peerId"].set<uint32_t>(peerId);

                    json::array peerAffs = json::array();
                    if (affLookup->grpAffSize() > 0U) {
                        for (auto entry : affTable) {
                            uint32_t srcId = entry.first;
                            uint32_t dstId = entry.second;

                            json::object affObj = json::object();
                            affObj["srcId"].set<uint32_t>(srcId);
                            affObj["dstId"].set<uint32_t>(dstId);
                            peerAffs.push_back(json::value(affObj));
                        }
                    }

                    peerObj["affiliations"].set<json::array>(peerAffs);
                    affs.push_back(json::value(peerObj));
                }
            }
        }
    }

    response["affiliations"].set<json::array>(affs);
    reply.payload(response);
}
