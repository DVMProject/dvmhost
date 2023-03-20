/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
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
#if !defined(__REST_API_H__)
#define __REST_API_H__

#include "Defines.h"
#include "network/UDPSocket.h"
#include "network/rest/RequestDispatcher.h"
#include "network/rest/http/HTTPServer.h"
#include "lookups/RadioIdLookup.h"
#include "lookups/TalkgroupIdLookup.h"
#include "Thread.h"

#include <vector>
#include <string>
#include <random>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define DVM_REST_RAND_MAX 0xfffffffffffffffe

#define PUT_AUTHENTICATE                "/auth"

#define GET_VERSION                     "/version"
#define GET_STATUS                      "/status"
#define GET_VOICE_CH                    "/voice-ch"

#define PUT_MDM_MODE                    "/mdm/mode"
#define MODE_OPT_IDLE                   "idle"
#define MODE_OPT_LCKOUT                 "lockout"
#define MODE_OPT_FDMR                   "dmr"
#define MODE_OPT_FP25                   "p25"
#define MODE_OPT_FNXDN                  "nxdn"

#define PUT_MDM_KILL                    "/mdm/kill"

#define PUT_PERMIT_TG                   "/permit-tg"
#define PUT_GRANT_TG                    "/grant-tg"
#define GET_RELEASE_GRNTS               "/release-grants"
#define GET_RELEASE_AFFS                "/release-affs"

#define GET_RID_WHITELIST               "/rid-whitelist/(\\d+)"
#define GET_RID_BLACKLIST               "/rid-blacklist/(\\d+)"

#define GET_DMR_BEACON                  "/dmr/beacon"
#define GET_DMR_DEBUG                   "/dmr/debug/(\\d+)/(\\d+)"
#define GET_DMR_DUMP_CSBK               "/dmr/dump-csbk/(\\d+)" 
#define PUT_DMR_RID                     "/dmr/rid"
#define GET_DMR_CC_DEDICATED            "/dmr/cc-enable/(\\d+)"
#define GET_DMR_CC_BCAST                "/dmr/cc-broadcast/(\\d+)"

#define GET_P25_CC                      "/p25/cc"
//#define GET_P25_CC_FALLBACK             "/p25/cc-fallback/(\\d+)"
#define GET_P25_DEBUG                   "/p25/debug/(\\d+)/(\\d+)"
#define GET_P25_DUMP_TSBK               "/p25/dump-tsbk/(\\d+)"
#define PUT_P25_RID                     "/p25/rid"
#define GET_P25_CC_DEDICATED            "/p25/cc-enable/(\\d+)"
#define GET_P25_CC_BCAST                "/p25/cc-broadcast/(\\d+)"

#define GET_NXDN_DEBUG                  "/nxdn/debug/(\\d+)/(\\d+)"
#define GET_NXDN_DUMP_RCCH              "/nxdn/dump-rcch/(\\d+)"

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------

class HOST_SW_API Host;
namespace dmr { class HOST_SW_API Control; }
namespace p25 { class HOST_SW_API Control; }
namespace nxdn { class HOST_SW_API Control; }

// ---------------------------------------------------------------------------
//  Class Declaration
//      Implements the REST API server logic.
// ---------------------------------------------------------------------------

class HOST_SW_API RESTAPI : private Thread {
public:
    /// <summary>Initializes a new instance of the RESTAPI class.</summary>
    RESTAPI(const std::string& address, uint16_t port, const std::string& password, Host* host, bool debug);
    /// <summary>Finalizes a instance of the RESTAPI class.</summary>
    ~RESTAPI();

    /// <summary>Sets the instances of the Radio ID and Talkgroup ID lookup tables.</summary>
    void setLookups(::lookups::RadioIdLookup* ridLookup, ::lookups::TalkgroupIdLookup* tidLookup);
    /// <summary>Sets the instances of the digital radio protocols.</summary>
    void setProtocols(dmr::Control* dmr, p25::Control* p25, nxdn::Control* nxdn);

    /// <summary>Opens connection to the network.</summary>
    bool open();

    /// <summary>Closes connection to the network.</summary>
    void close();

private:
    typedef network::rest::RequestDispatcher<network::rest::http::HTTPPayload, network::rest::http::HTTPPayload> RESTDispatcherType;
    typedef network::rest::http::HTTPPayload HTTPPayload;
    RESTDispatcherType m_dispatcher;
    network::rest::http::HTTPServer<RESTDispatcherType> m_restServer;

    std::mt19937 m_random;

    uint8_t m_p25MFId;

    std::string m_password;
    uint8_t* m_passwordHash;
    bool m_debug;

    Host* m_host;
    dmr::Control* m_dmr;
    p25::Control* m_p25;
    nxdn::Control* m_nxdn;

    ::lookups::RadioIdLookup* m_ridLookup;
    ::lookups::TalkgroupIdLookup* m_tidLookup;

    typedef std::unordered_map<std::string, uint64_t>::value_type AuthTokenValueType;
    std::unordered_map<std::string, uint64_t> m_authTokens;

    /// <summary></summary>
    virtual void entry();

    /// <summary>Helper to initialize REST API endpoints.</summary>
    void initializeEndpoints();

    /// <summary></summary>
    void invalidateHostToken(const std::string host);
    /// <summary></summary>
    bool validateAuth(const HTTPPayload& request, HTTPPayload& reply);

    /// <summary></summary>
    void restAPI_PutAuth(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /// <summary></summary>
    void restAPI_GetVersion(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /// <summary></summary>
    void restAPI_GetStatus(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /// <summary></summary>
    void restAPI_GetVoiceCh(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /// <summary></summary>
    void restAPI_PutModemMode(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /// <summary></summary>
    void restAPI_PutModemKill(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /// <summary></summary>
    void restAPI_PutPermitTG(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /// <summary></summary>
    void restAPI_PutGrantTG(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /// <summary></summary>
    void restAPI_GetReleaseGrants(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /// <summary></summary>
    void restAPI_GetReleaseAffs(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /// <summary></summary>
    void restAPI_GetRIDWhitelist(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /// <summary></summary>
    void restAPI_GetRIDBlacklist(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /*
    ** Digital Mobile Radio
    */

    /// <summary></summary>
    void restAPI_GetDMRBeacon(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /// <summary></summary>
    void restAPI_GetDMRDebug(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /// <summary></summary>
    void restAPI_GetDMRDumpCSBK(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /// <summary></summary>
    void restAPI_PutDMRRID(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /// <summary></summary>
    void restAPI_GetDMRCCEnable(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /// <summary></summary>
    void restAPI_GetDMRCCBroadcast(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /*
    ** Project 25
    */

    /// <summary></summary>
    void restAPI_GetP25CC(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /// <summary></summary>
    void restAPI_GetP25Debug(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /// <summary></summary>
    void restAPI_GetP25DumpTSBK(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /// <summary></summary>
    void restAPI_PutP25RID(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /// <summary></summary>
    void restAPI_GetP25CCEnable(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /// <summary></summary>
    void restAPI_GetP25CCBroadcast(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /*
    ** Next Generation Digital Narrowband
    */

    /// <summary></summary>
    void restAPI_GetNXDNDebug(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /// <summary></summary>
    void restAPI_GetNXDNDumpRCCH(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
};

#endif // __REST_API_H__
