/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
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
#include "Defines.h"
#include "edac/SHA256.h"
#include "dmr/Control.h"
#include "p25/Control.h"
#include "nxdn/Control.h"
#include "modem/Modem.h"
#include "host/Host.h"
#include "network/json/json.h"
#include "RESTAPI.h"
#include "HostMain.h"
#include "Log.h"
#include "Thread.h"
#include "Utils.h"

using namespace network;
using namespace network::rest;
using namespace network::rest::http;
using namespace modem;

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>

#include <memory>
#include <stdexcept>

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
RESTAPI::RESTAPI(const std::string& address, uint16_t port, const std::string& password, Host* host, bool debug) :
    m_dispatcher(debug),
    m_restServer(address, port),
    m_random(),
    m_p25MFId(p25::P25_MFG_STANDARD),
    m_password(password),
    m_passwordHash(nullptr),
    m_debug(debug),
    m_host(host),
    m_dmr(nullptr),
    m_p25(nullptr),
    m_nxdn(nullptr),
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
/// <param name="tidLookup">Talkgroup ID Lookup Table Instance</param>
void RESTAPI::setLookups(lookups::RadioIdLookup* ridLookup, lookups::TalkgroupIdLookup* tidLookup)
{
    m_ridLookup = ridLookup;
    m_tidLookup = tidLookup;
}

/// <summary>
/// Sets the instances of the digital radio protocols.
/// </summary>
/// <param name="dmr">Instance of the DMR Control class.</param>
/// <param name="p25">Instance of the P25 Control class.</param>
/// <param name="nxdn">Instance of the NXDN Control class.</param>
void RESTAPI::setProtocols(dmr::Control* dmr, p25::Control* p25, nxdn::Control* nxdn)
{
    m_dmr = dmr;
    m_p25 = p25;
    m_nxdn = nxdn;
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
    m_dispatcher.match(GET_VOICE_CH).get(REST_API_BIND(RESTAPI::restAPI_GetVoiceCh, this));

    m_dispatcher.match(PUT_MDM_MODE).put(REST_API_BIND(RESTAPI::restAPI_PutModemMode, this));
    m_dispatcher.match(PUT_MDM_KILL).put(REST_API_BIND(RESTAPI::restAPI_PutModemKill, this));

    m_dispatcher.match(PUT_SET_SUPERVISOR).put(REST_API_BIND(RESTAPI::restAPI_PutSetSupervisor, this));
    m_dispatcher.match(PUT_PERMIT_TG).put(REST_API_BIND(RESTAPI::restAPI_PutPermitTG, this));
    m_dispatcher.match(PUT_GRANT_TG).put(REST_API_BIND(RESTAPI::restAPI_PutGrantTG, this));
    m_dispatcher.match(GET_RELEASE_GRNTS).get(REST_API_BIND(RESTAPI::restAPI_GetReleaseGrants, this));
    m_dispatcher.match(GET_RELEASE_AFFS).get(REST_API_BIND(RESTAPI::restAPI_GetReleaseAffs, this));

    m_dispatcher.match(GET_RID_WHITELIST, true).get(REST_API_BIND(RESTAPI::restAPI_GetRIDWhitelist, this));
    m_dispatcher.match(GET_RID_BLACKLIST, true).get(REST_API_BIND(RESTAPI::restAPI_GetRIDBlacklist, this));

    /*
    ** Digital Mobile Radio
    */

    m_dispatcher.match(GET_DMR_BEACON).get(REST_API_BIND(RESTAPI::restAPI_GetDMRBeacon, this));
    m_dispatcher.match(GET_DMR_DEBUG, true).get(REST_API_BIND(RESTAPI::restAPI_GetDMRDebug, this));
    m_dispatcher.match(GET_DMR_DUMP_CSBK, true).get(REST_API_BIND(RESTAPI::restAPI_GetDMRDumpCSBK, this));
    m_dispatcher.match(PUT_DMR_RID).put(REST_API_BIND(RESTAPI::restAPI_PutDMRRID, this));
    m_dispatcher.match(GET_DMR_CC_DEDICATED).get(REST_API_BIND(RESTAPI::restAPI_GetDMRCCEnable, this));
    m_dispatcher.match(GET_DMR_CC_BCAST).get(REST_API_BIND(RESTAPI::restAPI_GetDMRCCBroadcast, this));
    m_dispatcher.match(PUT_DMR_TSCC_PAYLOAD_ACT).put(REST_API_BIND(RESTAPI::restAPI_PutTSCCPayloadActivate, this));

    /*
    ** Project 25
    */

    m_dispatcher.match(GET_P25_CC).get(REST_API_BIND(RESTAPI::restAPI_GetP25CC, this));
    m_dispatcher.match(GET_P25_DEBUG, true).get(REST_API_BIND(RESTAPI::restAPI_GetP25Debug, this));
    m_dispatcher.match(GET_P25_DUMP_TSBK, true).get(REST_API_BIND(RESTAPI::restAPI_GetP25DumpTSBK, this));
    m_dispatcher.match(PUT_P25_RID).put(REST_API_BIND(RESTAPI::restAPI_PutP25RID, this));
    m_dispatcher.match(GET_P25_CC_DEDICATED).get(REST_API_BIND(RESTAPI::restAPI_GetP25CCEnable, this));
    m_dispatcher.match(GET_P25_CC_BCAST).get(REST_API_BIND(RESTAPI::restAPI_GetP25CCBroadcast, this));
    m_dispatcher.match(PUT_P25_RAW_TSBK).put(REST_API_BIND(RESTAPI::restAPI_PutP25RawTSBK, this));

    /*
    ** Next Generation Digital Narrowband
    */

    m_dispatcher.match(GET_NXDN_CC).get(REST_API_BIND(RESTAPI::restAPI_GetNXDNCC, this));
    m_dispatcher.match(GET_NXDN_DEBUG).get(REST_API_BIND(RESTAPI::restAPI_GetNXDNDebug, this));
    m_dispatcher.match(GET_NXDN_DUMP_RCCH).get(REST_API_BIND(RESTAPI::restAPI_GetNXDNDumpRCCH, this));
    m_dispatcher.match(GET_NXDN_CC_DEDICATED).get(REST_API_BIND(RESTAPI::restAPI_GetNXDNCCEnable, this));
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
    response["version"].set<std::string>(std::string((__PROG_NAME__ " " __VER__ " (" DESCR_DMR DESCR_P25 DESCR_NXDN "CW Id, Network) (built " __BUILD__ ")")));

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
    {
        yaml::Node modemConfig = m_host->m_conf["system"]["modem"];
        std::string portType = modemConfig["protocol"]["type"].as<std::string>();
        response["portType"].set<std::string>(portType);

        yaml::Node uartConfig = modemConfig["protocol"]["uart"];
        std::string modemPort = uartConfig["port"].as<std::string>();
        response["modemPort"].set<std::string>(modemPort);
        uint32_t portSpeed = uartConfig["speed"].as<uint32_t>(115200U);
        response["portSpeed"].set<uint32_t>(portSpeed);

        response["state"].set<uint8_t>(m_host->m_state);
        bool dmrEnabled = m_dmr != nullptr;
        response["dmrEnabled"].set<bool>(dmrEnabled);
        bool p25Enabled = m_p25 != nullptr;
        response["p25Enabled"].set<bool>(p25Enabled);
        bool nxdnEnabled = m_nxdn != nullptr;
        response["nxdnEnabled"].set<bool>(nxdnEnabled);

        uint8_t protoVer = m_host->m_modem->getVersion();
        response["protoVer"].set<uint8_t>(protoVer);

        response["dmrCC"].set<bool>(m_host->m_dmrCtrlChannel);
        response["p25CC"].set<bool>(m_host->m_p25CtrlChannel);
        response["nxdnCC"].set<bool>(m_host->m_nxdnCtrlChannel);
    }

    {
        json::object modemInfo = json::object();
        if (!m_host->m_modem->isHotspot()) {
            modemInfo["pttInvert"].set<bool>(m_host->m_modem->m_pttInvert);
            modemInfo["rxInvert"].set<bool>(m_host->m_modem->m_rxInvert);
            modemInfo["txInvert"].set<bool>(m_host->m_modem->m_txInvert);
            modemInfo["dcBlocker"].set<bool>(m_host->m_modem->m_dcBlocker);
        }

        modemInfo["rxLevel"].set<float>(m_host->m_modem->m_rxLevel);
        modemInfo["cwTxLevel"].set<float>(m_host->m_modem->m_cwIdTXLevel);
        modemInfo["dmrTxLevel"].set<float>(m_host->m_modem->m_dmrTXLevel);
        modemInfo["p25TxLevel"].set<float>(m_host->m_modem->m_p25TXLevel);
        modemInfo["nxdnTxLevel"].set<float>(m_host->m_modem->m_nxdnTXLevel);

        modemInfo["rxDCOffset"].set<int>(m_host->m_modem->m_rxDCOffset);
        modemInfo["txDCOffset"].set<int>(m_host->m_modem->m_txDCOffset);

        if (!m_host->m_modem->isHotspot()) {
            modemInfo["dmrSymLevel3Adj"].set<int>(m_host->m_modem->m_dmrSymLevel3Adj);
            modemInfo["dmrSymLevel1Adj"].set<int>(m_host->m_modem->m_dmrSymLevel1Adj);
            modemInfo["p25SymLevel3Adj"].set<int>(m_host->m_modem->m_p25SymLevel3Adj);
            modemInfo["p25SymLevel1Adj"].set<int>(m_host->m_modem->m_p25SymLevel1Adj);

            // are we on a protocol version 3 firmware?
            if (m_host->m_modem->getVersion() >= 3U) {
                modemInfo["nxdnSymLevel3Adj"].set<int>(m_host->m_modem->m_nxdnSymLevel3Adj);
                modemInfo["nxdnSymLevel1Adj"].set<int>(m_host->m_modem->m_nxdnSymLevel1Adj);
            }
        }

        if (m_host->m_modem->isHotspot()) {
            modemInfo["dmrDiscBW"].set<int8_t>(m_host->m_modem->m_dmrDiscBWAdj);
            modemInfo["dmrPostBW"].set<int8_t>(m_host->m_modem->m_dmrPostBWAdj);
            modemInfo["p25DiscBW"].set<int8_t>(m_host->m_modem->m_p25DiscBWAdj);
            modemInfo["p25PostBW"].set<int8_t>(m_host->m_modem->m_p25PostBWAdj);

            // are we on a protocol version 3 firmware?
            if (m_host->m_modem->getVersion() >= 3U) {
                modemInfo["nxdnDiscBW"].set<int8_t>(m_host->m_modem->m_nxdnDiscBWAdj);
                modemInfo["nxdnPostBW"].set<int8_t>(m_host->m_modem->m_nxdnPostBWAdj);

                modemInfo["afcEnabled"].set<bool>(m_host->m_modem->m_afcEnable);
                modemInfo["afcKI"].set<uint8_t>(m_host->m_modem->m_afcKI);
                modemInfo["afcKP"].set<uint8_t>(m_host->m_modem->m_afcKP);
                modemInfo["afcRange"].set<uint8_t>(m_host->m_modem->m_afcRange);
            }

            switch (m_host->m_modem->m_adfGainMode) {
                case ADF_GAIN_AUTO_LIN:
                    modemInfo["gainMode"].set<std::string>("ADF7021 Gain Mode: Auto High Linearity");
                break;
                case ADF_GAIN_LOW:
                    modemInfo["gainMode"].set<std::string>("ADF7021 Gain Mode: Low");
                break;
                case ADF_GAIN_HIGH:
                    modemInfo["gainMode"].set<std::string>("ADF7021 Gain Mode: High");
                break;
                case ADF_GAIN_AUTO:
                default:
                    modemInfo["gainMode"].set<std::string>("ADF7021 Gain Mode: Auto");
                break;
            }
        }

        modemInfo["fdmaPreambles"].set<uint8_t>(m_host->m_modem->m_fdmaPreamble);
        modemInfo["dmrRxDelay"].set<uint8_t>(m_host->m_modem->m_dmrRxDelay);
        modemInfo["p25CorrCount"].set<uint8_t>(m_host->m_modem->m_p25CorrCount);

        modemInfo["rxFrequency"].set<uint32_t>(m_host->m_modem->m_rxFrequency);
        modemInfo["txFrequency"].set<uint32_t>(m_host->m_modem->m_txFrequency);
        modemInfo["rxTuning"].set<int>(m_host->m_modem->m_rxTuning);
        modemInfo["txTuning"].set<int>(m_host->m_modem->m_txTuning);
        uint32_t rxFreqEffective = m_host->m_modem->m_rxFrequency + m_host->m_modem->m_rxTuning;
        modemInfo["rxFrequencyEffective"].set<uint32_t>(rxFreqEffective);
        uint32_t txFreqEffective = m_host->m_modem->m_txFrequency + m_host->m_modem->m_txTuning;
        modemInfo["txFrequencyEffective"].set<uint32_t>(txFreqEffective);
        response["modem"].set<json::object>(modemInfo);
    }

    reply.payload(response);
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetVoiceCh(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object response = json::object();
    setResponseDefaultStatus(response);

    json::array channels = json::array();
    if (m_host->m_voiceChData.size() > 0) {
        for (auto entry : m_host->m_voiceChData) {
            uint32_t chNo = entry.first;
            lookups::VoiceChData data = entry.second;

            json::object channel = json::object();
            channel["chNo"].set<uint32_t>(chNo);
            std::string address = data.address();
            channel["address"].set<std::string>(address);
            uint16_t port = data.port();
            channel["port"].set<uint16_t>(port);

            channels.push_back(json::value(channel));
        }
    }

    response["channels"].set<json::array>(channels);
    reply.payload(response);
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_PutModemMode(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object response = json::object();
    setResponseDefaultStatus(response);

    json::object req = json::object();
    if (!parseRequestBody(request, reply, req)) {
        return;
    }

    // validate mode is a string within the JSON blob
    if (!req["mode"].is<std::string>()) {
        errorPayload(reply, "password was not a valid string");
        return;
    }

    std::string mode = ::strtolower(req["mode"].get<std::string>());

    if (mode == MODE_OPT_IDLE) {
        m_host->m_fixedMode = false;
        m_host->setState(STATE_IDLE);

        response["message"].set<std::string>(std::string("Dynamic mode"));
        uint8_t hostMode = m_host->m_state;
        response["mode"].set<uint8_t>(hostMode);

        reply.payload(response);
    }
    else if (mode == MODE_OPT_LCKOUT) {
        m_host->m_fixedMode = false;
        m_host->setState(HOST_STATE_LOCKOUT);

        response["message"].set<std::string>(std::string("Lockout mode"));
        uint8_t hostMode = m_host->m_state;
        response["mode"].set<uint8_t>(hostMode);

        reply.payload(response);
    }
#if defined(ENABLE_DMR)
    else if (mode == MODE_OPT_FDMR) {
        if (m_dmr != nullptr) {
            m_host->m_fixedMode = true;
            m_host->setState(STATE_DMR);

            response["message"].set<std::string>(std::string("Fixed mode"));
            uint8_t hostMode = m_host->m_state;
            response["mode"].set<uint8_t>(hostMode);

            reply.payload(response);
        }
        else {
            errorPayload(reply, "DMR mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        }
    }
#endif // defined(ENABLE_DMR)
#if defined(ENABLE_P25)
    else if (mode == MODE_OPT_FP25) {
        if (m_p25 != nullptr) {
            m_host->m_fixedMode = true;
            m_host->setState(STATE_P25);

            response["message"].set<std::string>(std::string("Fixed mode"));
            uint8_t hostMode = m_host->m_state;
            response["mode"].set<uint8_t>(hostMode);

            reply.payload(response);
        }
        else {
            errorPayload(reply, "P25 mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        }
    }
#endif // defined(ENABLE_P25)
#if defined(ENABLE_NXDN)
    else if (mode == MODE_OPT_FNXDN) {
        if (m_nxdn != nullptr) {
            m_host->m_fixedMode = true;
            m_host->setState(STATE_NXDN);

            response["message"].set<std::string>(std::string("Fixed mode"));
            uint8_t hostMode = m_host->m_state;
            response["mode"].set<uint8_t>(hostMode);

            reply.payload(response);
        }
        else {
            errorPayload(reply, "NXDN mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        }
    }
#endif // defined(ENABLE_NXDN)
    else {
        errorPayload(reply, "invalid mode");
    }
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_PutModemKill(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object req = json::object();
    if (!parseRequestBody(request, reply, req)) {
        return;
    }

    errorPayload(reply, "OK", HTTPPayload::OK);

    // validate mode is a string within the JSON blob
    if (!req["force"].is<bool>()) {
        errorPayload(reply, "force was not a valid value");
        return;
    }

    bool force = req["force"].get<bool>();

    if (!force) {
        g_killed = true;
    } else {
        g_killed = true;
        m_host->setState(HOST_STATE_QUIT);
    }
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_PutSetSupervisor(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object req = json::object();
    if (!parseRequestBody(request, reply, req)) {
        return;
    }

    errorPayload(reply, "OK", HTTPPayload::OK);

    if (!m_host->m_authoritative) {
        errorPayload(reply, "Host is not authoritative, cannot set supervisory state");
        return;
    }

    // validate state is a string within the JSON blob
    if (!req["state"].is<int>()) {
        errorPayload(reply, "state was not a valid integer");
        return;
    }

    DVM_STATE state = (DVM_STATE)req["state"].get<int>();

    // validate destination ID is a integer within the JSON blob
    if (!req["enable"].is<bool>()) {
        errorPayload(reply, "enable was not a boolean");
        return;
    }

    bool enable = req["enable"].get<bool>();

    switch (state) {
    case STATE_DMR:
#if defined(ENABLE_DMR)
    {
        if (m_dmr != nullptr) {
            m_dmr->setSupervisor(enable);
        }
        else {
            errorPayload(reply, "DMR mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        }
    }
#else
    {
        errorPayload(reply, "DMR operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
    }
#endif // defined(ENABLE_DMR)
    break;
    case STATE_P25:
#if defined(ENABLE_P25)
    {
        if (m_p25 != nullptr) {
            m_p25->setSupervisor(enable);
        }
        else {
          errorPayload(reply, "P25 mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        }
    }
#else
    {
        errorPayload(reply, "P25 operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
    }
#endif // defined(ENABLE_P25)
    break;
    case STATE_NXDN:
#if defined(ENABLE_NXDN)
    {
        if (m_nxdn != nullptr) {
            m_nxdn->setSupervisor(enable);
        }
        else {
            errorPayload(reply, "NXDN mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        }
    }
#else
    {
        errorPayload(reply, "NXDN operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
    }
#endif // defined(ENABLE_NXDN)
    break;
    default:
        errorPayload(reply, "invalid mode");
        break;
    }
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_PutPermitTG(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object req = json::object();
    if (!parseRequestBody(request, reply, req)) {
        return;
    }

    errorPayload(reply, "OK", HTTPPayload::OK);

    if (m_host->m_authoritative) {
        errorPayload(reply, "Host is authoritative, cannot permit TG");
        return;
    }

    // validate state is a string within the JSON blob
    if (!req["state"].is<int>()) {
        errorPayload(reply, "state was not a valid integer");
        return;
    }

    DVM_STATE state = (DVM_STATE)req["state"].get<int>();

    // validate destination ID is a integer within the JSON blob
    if (!req["dstId"].is<int>()) {
        errorPayload(reply, "destination ID was not a valid integer");
        return;
    }

    uint32_t dstId = req["dstId"].get<uint32_t>();

    switch (state) {
    case STATE_DMR:
#if defined(ENABLE_DMR)
    {
        // validate slot is a integer within the JSON blob
        if (!req["slot"].is<int>()) {
            errorPayload(reply, "slot was not a valid integer");
            return;
        }

        uint8_t slot = (uint8_t)req["slot"].get<int>();

        if (slot == 0U || slot > 2U) {
            errorPayload(reply, "illegal DMR slot");
            return;
        }

        if (m_dmr != nullptr) {
            m_dmr->permittedTG(dstId, slot);
        }
        else {
            errorPayload(reply, "DMR mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        }
    }
#else
    {
        errorPayload(reply, "DMR operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
    }
#endif // defined(ENABLE_DMR)
    break;
    case STATE_P25:
#if defined(ENABLE_P25)
    {
        if (m_p25 != nullptr) {
            m_p25->permittedTG(dstId);
        }
        else {
          errorPayload(reply, "P25 mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        }
    }
#else
    {
        errorPayload(reply, "P25 operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
    }
#endif // defined(ENABLE_P25)
    break;
    case STATE_NXDN:
#if defined(ENABLE_NXDN)
    {
        if (m_nxdn != nullptr) {
            m_nxdn->permittedTG(dstId);
        }
        else {
            errorPayload(reply, "NXDN mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        }
    }
#else
    {
        errorPayload(reply, "NXDN operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
    }
#endif // defined(ENABLE_NXDN)
    break;
    default:
        errorPayload(reply, "invalid mode");
        break;
    }
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_PutGrantTG(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object req = json::object();
    if (!parseRequestBody(request, reply, req)) {
        return;
    }

    errorPayload(reply, "OK", HTTPPayload::OK);

    if (m_host->m_authoritative && (m_host->m_dmrCtrlChannel || m_host->m_p25CtrlChannel || m_host->m_nxdnCtrlChannel)) {
        errorPayload(reply, "Host is authoritative, cannot grant TG");
        return;
    }

    // validate state is a string within the JSON blob
    if (!req["state"].is<int>()) {
        errorPayload(reply, "state was not a valid integer");
        return;
    }

    DVM_STATE state = (DVM_STATE)req["state"].get<int>();

    // validate destination ID is a integer within the JSON blob
    if (!req["dstId"].is<int>()) {
        errorPayload(reply, "destination ID was not a valid integer");
        return;
    }

    uint32_t dstId = req["dstId"].get<uint32_t>();

    if (dstId == 0U) {
        errorPayload(reply, "destination ID is an illegal TGID");
        return;
    }

    // validate unit-to-unit is a integer within the JSON blob
    if (!req["unitToUnit"].is<bool>()) {
        errorPayload(reply, "unit-to-unit was not a valid boolean");
        return;
    }

    //bool unitToUnit = req["unitToUnit"].get<bool>();

    switch (state) {
    case STATE_DMR:
#if defined(ENABLE_DMR)
    {
        // validate slot is a integer within the JSON blob
        if (!req["slot"].is<int>()) {
            errorPayload(reply, "slot was not a valid integer");
            return;
        }

        uint8_t slot = (uint8_t)req["slot"].get<int>();

        if (slot == 0U || slot > 2U) {
            errorPayload(reply, "illegal DMR slot");
            return;
        }

        if (m_dmr != nullptr) {
            // TODO TODO
            //m_dmr->grantTG(dstId, slot, unitToUnit);
        }
        else {
            errorPayload(reply, "DMR mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        }
    }
#else
    {
        errorPayload(reply, "DMR operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
    }
#endif // defined(ENABLE_DMR)
    break;
    case STATE_P25:
#if defined(ENABLE_P25)
    {
        if (m_p25 != nullptr) {
            // TODO TODO
            //m_p25->grantTG(dstId, unitToUnit);
        }
        else {
            errorPayload(reply, "P25 mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        }
    }
#else
    {
        errorPayload(reply, "P25 operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
    }
#endif // defined(ENABLE_P25)
    break;
    case STATE_NXDN:
#if defined(ENABLE_NXDN)
    {
        if (m_nxdn != nullptr) {
            // TODO TODO
            //nxdn->grantTG(dstId, unitToUnit);
        }
        else {
            errorPayload(reply, "NXDN mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        }
    }
#else
    {
        errorPayload(reply, "NXDN operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
    }
#endif // defined(ENABLE_NXDN)
    break;
    default:
        errorPayload(reply, "invalid mode");
    }
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetReleaseGrants(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    errorPayload(reply, "OK", HTTPPayload::OK);
#if defined(ENABLE_DMR)
    if (m_dmr != nullptr) {
        m_dmr->affiliations().releaseGrant(0, true);
    }
#endif // defined(ENABLE_DMR)
#if defined(ENABLE_P25)
    if (m_p25 != nullptr) {
        m_p25->affiliations().releaseGrant(0, true);
    }
#endif // defined(ENABLE_P25)
#if defined(ENABLE_NXDN)
    if (m_nxdn != nullptr) {
        m_nxdn->affiliations().releaseGrant(0, true);
    }
#endif // defined(ENABLE_NXDN)
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetReleaseAffs(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    errorPayload(reply, "OK", HTTPPayload::OK);
#if defined(ENABLE_DMR)
    if (m_dmr != nullptr) {
        m_dmr->affiliations().clearGroupAff(0, true);
    }
#endif // defined(ENABLE_DMR)
#if defined(ENABLE_P25)
    if (m_p25 != nullptr) {
        m_p25->affiliations().clearGroupAff(0, true);
    }
#endif // defined(ENABLE_P25)
#if defined(ENABLE_NXDN)
    if (m_nxdn != nullptr) {
        m_nxdn->affiliations().clearGroupAff(0, true);
    }
#endif // defined(ENABLE_NXDN)
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetRIDWhitelist(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    if (match.size() < 2) {
        errorPayload(reply, "invalid API call arguments");
        return;
    }

    errorPayload(reply, "OK", HTTPPayload::OK);
    uint32_t srcId = (uint32_t)::strtoul(match.str(1).c_str(), NULL, 10);

    if (srcId != 0U) {
        m_ridLookup->toggleEntry(srcId, true);
    }
    else {
        errorPayload(reply, "tried to whitelist RID 0");
    }
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetRIDBlacklist(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    if (match.size() < 2) {
        errorPayload(reply, "invalid API call arguments");
        return;
    }

    errorPayload(reply, "OK", HTTPPayload::OK);
    uint32_t srcId = (uint32_t)::strtoul(match.str(1).c_str(), NULL, 10);

    if (srcId != 0U) {
        m_ridLookup->toggleEntry(srcId, false);
    }
    else {
        errorPayload(reply, "tried to blacklist RID 0");
    }
}

/*
** Digital Mobile Radio
*/

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetDMRBeacon(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

#if defined(ENABLE_DMR)
    errorPayload(reply, "OK", HTTPPayload::OK);
    if (m_dmr != nullptr) {
        if (m_host->m_dmrBeacons) {
            g_fireDMRBeacon = true;
        }
        else {
            errorPayload(reply, "DMR beacons are not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
            return;
        }
    }
    else {
        errorPayload(reply, "DMR mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        return;
    }
#else
    errorPayload(reply, "DMR operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
#endif // defined(ENABLE_DMR)
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetDMRDebug(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object response = json::object();
    setResponseDefaultStatus(response);
#if defined(ENABLE_DMR)
    errorPayload(reply, "OK", HTTPPayload::OK);
    if (m_dmr != nullptr) {
        if (match.size() <= 1) {
            bool debug = m_dmr->getDebug();
            bool verbose = m_dmr->getVerbose();

            response["debug"].set<bool>(debug);
            response["verbose"].set<bool>(verbose);

            reply.payload(response);
            return;
        }
        else {
            if (match.size() == 3) {
                uint8_t debug = (uint8_t)::strtoul(match.str(1).c_str(), NULL, 10);
                uint8_t verbose = (uint8_t)::strtoul(match.str(2).c_str(), NULL, 10);
                m_dmr->setDebugVerbose((debug == 1U) ? true : false, (verbose == 1U) ? true : false);
            }
        }
    }
    else {
        errorPayload(reply, "DMR mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        return;
    }
#else
    errorPayload(reply, "DMR operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
#endif // defined(ENABLE_DMR)
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetDMRDumpCSBK(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object response = json::object();
    setResponseDefaultStatus(response);
#if defined(ENABLE_DMR)
    errorPayload(reply, "OK", HTTPPayload::OK);
    if (m_dmr != nullptr) {
        if (match.size() <= 1) {
            bool csbkDump = m_dmr->getCSBKVerbose();

            response["verbose"].set<bool>(csbkDump);

            reply.payload(response);
            return;
        }
        else {
            if (match.size() == 2) {
                uint8_t enable = (uint8_t)::strtoul(match.str(1).c_str(), NULL, 10);
                m_dmr->setCSBKVerbose((enable == 1U) ? true : false);
            }
        }
    }
    else {
        errorPayload(reply, "DMR mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        return;
    }
#else
    errorPayload(reply, "DMR operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
#endif // defined(ENABLE_DMR)
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_PutDMRRID(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object req = json::object();
    if (!parseRequestBody(request, reply, req)) {
        return;
    }
#if defined(ENABLE_DMR)
    if (m_dmr == nullptr) {
        errorPayload(reply, "DMR mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        return;
    }

    // validate state is a string within the JSON blob
    if (!req["command"].is<std::string>()) {
        errorPayload(reply, "command was not valid");
        return;
    }

    // validate destination ID is a integer within the JSON blob
    if (!req["dstId"].is<uint32_t>()) {
        errorPayload(reply, "destination ID was not valid");
        return;
    }

    // validate destination ID is a integer within the JSON blob
    if (!req["slot"].is<uint8_t>()) {
        errorPayload(reply, "slot was not valid");
        return;
    }

    uint32_t dstId = req["dstId"].get<uint32_t>();
    uint8_t slot = req["slot"].get<uint8_t>();

    if (dstId == 0U) {
        errorPayload(reply, "destination ID was not valid");
        return;
    }

    if (slot == 0U && slot >= 3U) {
        errorPayload(reply, "invalid DMR slot number (slot == 0 or slot > 3)");
        return;
    }

    errorPayload(reply, "OK", HTTPPayload::OK);
    std::string command = req["command"].get<std::string>();
    if (::strtolower(command) == RID_CMD_PAGE) {
        m_dmr->writeRF_Call_Alrt(slot, p25::P25_WUID_FNE, dstId);
    }
    else if (::strtolower(command) == RID_CMD_CHECK) {
        m_dmr->writeRF_Ext_Func(slot, dmr::DMR_EXT_FNCT_CHECK, p25::P25_WUID_FNE, dstId);
    }
    else if (::strtolower(command) == RID_CMD_INHIBIT) {
        m_dmr->writeRF_Ext_Func(slot, dmr::DMR_EXT_FNCT_INHIBIT, p25::P25_WUID_FNE, dstId);
    }
    else if (::strtolower(command) == RID_CMD_UNINHIBIT) {
        m_dmr->writeRF_Ext_Func(slot, dmr::DMR_EXT_FNCT_UNINHIBIT, p25::P25_WUID_FNE, dstId);
    }
    else {
        errorPayload(reply, "invalid command");
        return;
    }
#else
    errorPayload(reply, "DMR operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
#endif // defined(ENABLE_DMR)
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetDMRCCEnable(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }
#if defined(ENABLE_DMR)
    errorPayload(reply, "OK", HTTPPayload::OK);
    if (m_dmr != nullptr) {
        if (m_host->m_dmrTSCCData) {
            if (m_p25 != nullptr) {
                errorPayload(reply, "Can't enable DMR control channel while P25 is enabled!");
            }

            if (m_nxdn != nullptr) {
                errorPayload(reply, "Can't enable DMR control channel while NXDN is enabled!");
                return;
            }

            m_host->m_dmrCtrlChannel = !m_host->m_dmrCtrlChannel;
            errorPayload(reply, string_format("DMR CC is %s", m_host->m_p25CtrlChannel ? "enabled" : "disabled"), HTTPPayload::OK);
        }
        else {
            errorPayload(reply, "DMR control data is not enabled!");
        }
    }
    else {
        errorPayload(reply, "DMR mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        return;
    }
#else
    errorPayload(reply, "DMR operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
#endif // defined(ENABLE_DMR)
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetDMRCCBroadcast(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }
#if defined(ENABLE_DMR)
    errorPayload(reply, "OK", HTTPPayload::OK);
    if (m_dmr != nullptr) {
        m_host->m_dmrTSCCData = !m_host->m_dmrTSCCData;
        errorPayload(reply, string_format("DMR CC broadcast is %s", m_host->m_dmrTSCCData ? "enabled" : "disabled"), HTTPPayload::OK);
    }
    else {
        errorPayload(reply, "DMR mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        return;
    }
#else
    errorPayload(reply, "DMR operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
#endif // defined(ENABLE_DMR)
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_PutTSCCPayloadActivate(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object req = json::object();
    if (!parseRequestBody(request, reply, req)) {
        return;
    }
#if defined(ENABLE_DMR)
    if (m_dmr == nullptr) {
        errorPayload(reply, "DMR mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        return;
    }

    // validate destination ID is a integer within the JSON blob
    if (!req["slot"].is<uint8_t>()) {
        errorPayload(reply, "slot was not valid");
        return;
    }

    // validate clear flag is a boolean within the JSON blob
    if (!req["clear"].is<bool>()) {
        errorPayload(reply, "clear flag was not valid");
        return;
    }

    uint8_t slot = req["slot"].get<uint8_t>();
    bool clear = req["clear"].get<bool>();

    if (slot == 0U && slot >= 3U) {
        errorPayload(reply, "invalid DMR slot number (slot == 0 or slot > 3)");
        return;
    }

    errorPayload(reply, "OK", HTTPPayload::OK);
    if (clear) {
        m_dmr->tsccClearActivatedSlot(slot);
    }
    else {
        // validate destination ID is a integer within the JSON blob
        if (!req["dstId"].is<uint32_t>()) {
            errorPayload(reply, "destination ID was not valid");
            return;
        }

        // validate destination ID is a integer within the JSON blob
        if (!req["srcId"].is<uint32_t>()) {
            errorPayload(reply, "source ID was not valid");
            return;
        }

        // validate group flag is a boolean within the JSON blob
        if (!req["group"].is<bool>()) {
            errorPayload(reply, "group flag was not valid");
            return;
        }

        // validate voice flag is a boolean within the JSON blob
        if (!req["voice"].is<bool>()) {
            errorPayload(reply, "voice flag was not valid");
            return;
        }

        uint32_t dstId = req["dstId"].get<uint32_t>();
        uint32_t srcId = req["srcId"].get<uint32_t>();
        bool group = req["group"].get<bool>();
        bool voice = req["voice"].get<bool>();

        if (dstId == 0U) {
            errorPayload(reply, "destination ID was not valid");
            return;
        }

        m_dmr->tsccActivateSlot(slot, dstId, srcId, group, voice);
    }
#else
    errorPayload(reply, "DMR operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
#endif // defined(ENABLE_DMR)
}

/*
** Project 25
*/

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetP25CC(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

#if defined(ENABLE_P25)
    errorPayload(reply, "OK", HTTPPayload::OK);
    if (m_p25 != nullptr) {
        if (m_host->m_p25CCData) {
            g_fireP25Control = true;
        }
        else {
            errorPayload(reply, "P25 control data is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
            return;
        }
    }
    else {
        errorPayload(reply, "P25 mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        return;
    }
#else
    errorPayload(reply, "P25 operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
#endif // defined(ENABLE_P25)
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetP25Debug(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object response = json::object();
    setResponseDefaultStatus(response);
#if defined(ENABLE_P25)
    errorPayload(reply, "OK", HTTPPayload::OK);
    if (m_dmr != nullptr) {
        if (match.size() <= 1) {
            bool debug = m_p25->getDebug();
            bool verbose = m_p25->getVerbose();

            response["debug"].set<bool>(debug);
            response["verbose"].set<bool>(verbose);

            reply.payload(response);
            return;
        }
        else {
            if (match.size() == 3) {
                uint8_t debug = (uint8_t)::strtoul(match.str(1).c_str(), NULL, 10);
                uint8_t verbose = (uint8_t)::strtoul(match.str(2).c_str(), NULL, 10);
                m_p25->setDebugVerbose((debug == 1U) ? true : false, (verbose == 1U) ? true : false);
            }
        }
    }
    else {
        errorPayload(reply, "P25 mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        return;
    }
#else
    errorPayload(reply, "P25 operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
#endif // defined(ENABLE_P25)
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetP25DumpTSBK(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object response = json::object();
    setResponseDefaultStatus(response);
#if defined(ENABLE_P25)
    errorPayload(reply, "OK", HTTPPayload::OK);
    if (m_p25 != nullptr) {
        if (match.size() <= 1) {
            bool tsbkDump = m_p25->trunk()->getTSBKVerbose();

            response["verbose"].set<bool>(tsbkDump);

            reply.payload(response);
            return;
        }
        else {
            if (match.size() == 2) {
                uint8_t enable = (uint8_t)::strtoul(match.str(1).c_str(), NULL, 10);
                m_p25->trunk()->setTSBKVerbose((enable == 1U) ? true : false);
            }
        }
    }
    else {
        errorPayload(reply, "P25 mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        return;
    }
#else
    errorPayload(reply, "P25 operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
#endif // defined(ENABLE_P25)
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_PutP25RID(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object req = json::object();
    if (!parseRequestBody(request, reply, req)) {
        return;
    }
#if defined(ENABLE_P25)
    if (m_p25 == nullptr) {
        errorPayload(reply, "P25 mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        return;
    }

    // validate state is a string within the JSON blob
    if (!req["command"].is<std::string>()) {
        errorPayload(reply, "command was not valid");
        return;
    }

    std::string command = req["command"].get<std::string>();
    uint32_t dstId = 0;

    if (::strtolower(command) != RID_CMD_P25_SET_MFID) {
        // validate destination ID is a integer within the JSON blob
        if (!req["dstId"].is<uint32_t>()) {
            errorPayload(reply, "destination ID was not valid");
            return;
        }

        dstId = req["dstId"].get<uint32_t>();

        if (dstId == 0U) {
            errorPayload(reply, "destination ID was not valid");
            return;
        }
    }


    errorPayload(reply, "OK", HTTPPayload::OK);
    if (::strtolower(command) == RID_CMD_P25_SET_MFID) {
        // validate destination ID is a integer within the JSON blob
        if (!req["mfId"].is<uint8_t>()) {
            errorPayload(reply, "MFID was not valid");
            return;
        }

        uint8_t mfId = req["mfId"].get<uint8_t>();
        m_p25->trunk()->setLastMFId(mfId);
    }
    else if (::strtolower(command) == RID_CMD_PAGE) {
        m_p25->trunk()->writeRF_TSDU_Call_Alrt(p25::P25_WUID_FNE, dstId);
    }
    else if (::strtolower(command) == RID_CMD_CHECK) {
        m_p25->trunk()->writeRF_TSDU_Ext_Func(p25::P25_EXT_FNCT_CHECK, p25::P25_WUID_FNE, dstId);
    }
    else if (::strtolower(command) == RID_CMD_INHIBIT) {
        m_p25->trunk()->writeRF_TSDU_Ext_Func(p25::P25_EXT_FNCT_INHIBIT, p25::P25_WUID_FNE, dstId);
    }
    else if (::strtolower(command) == RID_CMD_UNINHIBIT) {
        m_p25->trunk()->writeRF_TSDU_Ext_Func(p25::P25_EXT_FNCT_UNINHIBIT, p25::P25_WUID_FNE, dstId);
    }
    else if (::strtolower(command) == RID_CMD_GAQ) {
        m_p25->trunk()->writeRF_TSDU_Grp_Aff_Q(dstId);
    }
    else if (::strtolower(command) == RID_CMD_UREG) {
        m_p25->trunk()->writeRF_TSDU_U_Reg_Cmd(dstId);
    }
    else if (::strtolower(command) == RID_CMD_EMERG) {
        // validate source ID is a integer within the JSON blob
        if (!req["srcId"].is<uint32_t>()) {
            errorPayload(reply, "source ID was not valid");
            return;
        }

        uint32_t srcId = req["srcId"].get<uint32_t>();

        if (srcId == 0U) {
            errorPayload(reply, "source ID was not valid");
            return;
        }

        m_p25->trunk()->writeRF_TSDU_Emerg_Alrm(srcId, dstId);
    }
    else {
        errorPayload(reply, "invalid command");
        return;
    }
#else
    errorPayload(reply, "P25 operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
#endif // defined(ENABLE_P25)
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetP25CCEnable(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }
#if defined(ENABLE_P25)
    errorPayload(reply, "OK", HTTPPayload::OK);
    if (m_p25 != nullptr) {
        if (m_host->m_p25CCData) {
            if (m_dmr != nullptr) {
                errorPayload(reply, "Can't enable P25 control channel while DMR is enabled!");
                return;
            }

            if (m_nxdn != nullptr) {
                errorPayload(reply, "Can't enable P25 control channel while NXDN is enabled!");
                return;
            }

            m_host->m_p25CtrlChannel = !m_host->m_p25CtrlChannel;
            m_host->m_p25CtrlBroadcast = true;
            g_fireP25Control = true;
            m_p25->setCCHalted(false);

            errorPayload(reply, string_format("P25 CC is %s", m_host->m_p25CtrlChannel ? "enabled" : "disabled"), HTTPPayload::OK);
        }
        else {
            errorPayload(reply, "P25 control data is not enabled!");
        }
    }
    else {
        errorPayload(reply, "P25 mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        return;
    }
#else
    errorPayload(reply, "P25 operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
#endif // defined(ENABLE_P25)
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetP25CCBroadcast(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }
#if defined(ENABLE_P25)
    errorPayload(reply, "OK", HTTPPayload::OK);
    if (m_p25 != nullptr) {
        if (m_host->m_p25CCData) {
            m_host->m_p25CtrlBroadcast = !m_host->m_p25CtrlBroadcast;

            if (!m_host->m_p25CtrlBroadcast) {
                g_fireP25Control = false;
                m_p25->setCCHalted(true);
            }
            else {
                g_fireP25Control = true;
                m_p25->setCCHalted(false);
            }

            errorPayload(reply, string_format("P25 CC broadcast is %s", m_host->m_p25CtrlBroadcast ? "enabled" : "disabled"), HTTPPayload::OK);
        }
        else {
            errorPayload(reply, "P25 control data is not enabled!");
        }
    }
    else {
        errorPayload(reply, "P25 mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        return;
    }
#else
    errorPayload(reply, "P25 operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
#endif // defined(ENABLE_P25)
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_PutP25RawTSBK(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object req = json::object();
    if (!parseRequestBody(request, reply, req)) {
        return;
    }
#if defined(ENABLE_P25)
    if (m_p25 == nullptr) {
        errorPayload(reply, "P25 mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        return;
    }

    // validate state is a string within the JSON blob
    if (!req["tsbk"].is<std::string>()) {
        errorPayload(reply, "tsbk was not valid");
        return;
    }

    std::string tsbkBytes = req["tsbk"].get<std::string>();

    if (tsbkBytes.size() != 24) {
        errorPayload(reply, "TSBK must be 24 characters in length");
        return;
    }

    if (!(tsbkBytes.find_first_not_of("0123456789abcdefABCDEF", 2) == std::string::npos)) {
        errorPayload(reply, "TSBK contains invalid characters");
        return;
    }

    const char* tsbkPtr = tsbkBytes.c_str();
    uint8_t* tsbk = new uint8_t[p25::P25_TSBK_LENGTH_BYTES];
    ::memset(tsbk, 0x00U, p25::P25_TSBK_LENGTH_BYTES);

    for (uint8_t i = 0; i < p25::P25_TSBK_LENGTH_BYTES; i++) {
        char t[4] = {tsbkPtr[0], tsbkPtr[1], 0};
        tsbk[i] = (uint8_t)::strtoul(t, NULL, 16);
        tsbkPtr += 2 * sizeof(char);
    }

    if (m_debug) {
        Utils::dump("Raw TSBK", tsbk, p25::P25_TSBK_LENGTH_BYTES);
    }

    m_p25->trunk()->writeRF_TSDU_Raw(tsbk);
#else
    errorPayload(reply, "P25 operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
#endif // defined(ENABLE_P25)
}

/*
** Next Generation Digital Narrowband
*/

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetNXDNCC(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

#if defined(ENABLE_NXDN)
    errorPayload(reply, "OK", HTTPPayload::OK);
    if (m_nxdn != nullptr) {
        if (m_host->m_nxdnCCData) {
            g_fireNXDNControl = true;
        }
        else {
            errorPayload(reply, "NXDN control data is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
            return;
        }
    }
    else {
        errorPayload(reply, "NXDN mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        return;
    }
#else
    errorPayload(reply, "NXDN operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
#endif // defined(ENABLE_NXDN)
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetNXDNDebug(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object response = json::object();
    setResponseDefaultStatus(response);
#if defined(ENABLE_NXDN)
    errorPayload(reply, "OK", HTTPPayload::OK);
    if (m_dmr != nullptr) {
        if (match.size() <= 1) {
            bool debug = m_nxdn->getDebug();
            bool verbose = m_nxdn->getVerbose();

            response["debug"].set<bool>(debug);
            response["verbose"].set<bool>(verbose);

            reply.payload(response);
            return;
        }
        else {
            if (match.size() == 3) {
                uint8_t debug = (uint8_t)::strtoul(match.str(1).c_str(), NULL, 10);
                uint8_t verbose = (uint8_t)::strtoul(match.str(2).c_str(), NULL, 10);
                m_nxdn->setDebugVerbose((debug == 1U) ? true : false, (verbose == 1U) ? true : false);
            }
        }
    }
    else {
        errorPayload(reply, "NXDN mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        return;
    }
#else
    errorPayload(reply, "NXDN operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
#endif // defined(ENABLE_NXDN)
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetNXDNDumpRCCH(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }

    json::object response = json::object();
    setResponseDefaultStatus(response);
#if defined(ENABLE_NXDN)
    errorPayload(reply, "OK", HTTPPayload::OK);
    if (m_p25 != nullptr) {
        if (match.size() <= 1) {
            bool rcchDump = m_nxdn->getRCCHVerbose();

            response["verbose"].set<bool>(rcchDump);

            reply.payload(response);
            return;
        }
        else {
            if (match.size() == 2) {
                uint8_t enable = (uint8_t)::strtoul(match.str(1).c_str(), NULL, 10);
                m_nxdn->setRCCHVerbose((enable == 1U) ? true : false);
            }
        }
    }
    else {
        errorPayload(reply, "NXDN mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        return;
    }
#else
    errorPayload(reply, "NXDN operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
#endif // defined(ENABLE_NXDN)
}

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <param name="match"></param>
void RESTAPI::restAPI_GetNXDNCCEnable(const HTTPPayload& request, HTTPPayload& reply, const RequestMatch& match)
{
    if (!validateAuth(request, reply)) {
        return;
    }
#if defined(ENABLE_NXDN)
    errorPayload(reply, "OK", HTTPPayload::OK);
    if (m_nxdn != nullptr) {
        if (m_host->m_nxdnCCData) {
            if (m_dmr != nullptr) {
                errorPayload(reply, "Can't enable NXDN control channel while DMR is enabled!");
                return;
            }

            if (m_p25 != nullptr) {
                errorPayload(reply, "Can't enable NXDN control channel while P25 is enabled!");
                return;
            }

            m_host->m_nxdnCtrlChannel = !m_host->m_nxdnCtrlChannel;
            m_host->m_nxdnCtrlBroadcast = true;
            g_fireNXDNControl = true;
            m_nxdn->setCCHalted(false);

            errorPayload(reply, string_format("NXDN CC is %s", m_host->m_nxdnCtrlChannel ? "enabled" : "disabled"), HTTPPayload::OK);
        }
        else {
            errorPayload(reply, "NXDN control data is not enabled!");
        }
    }
    else {
        errorPayload(reply, "NXDN mode is not enabled", HTTPPayload::SERVICE_UNAVAILABLE);
        return;
    }
#else
    errorPayload(reply, "NXDN operations are unavailable", HTTPPayload::SERVICE_UNAVAILABLE);
#endif // defined(ENABLE_NXDN)
}
