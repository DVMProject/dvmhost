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
#include "network/RESTDefines.h"
#include "network/rest/RequestDispatcher.h"
#include "network/rest/http/HTTPServer.h"
#include "lookups/RadioIdLookup.h"
#include "lookups/TalkgroupIdLookup.h"
#include "Thread.h"

#include <vector>
#include <string>
#include <random>

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
    /// <summary></summary>
    void restAPI_PutTSCCPayloadActivate(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

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
    /// <summary></summary>
    void restAPI_PutP25RawTSBK(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /*
    ** Next Generation Digital Narrowband
    */

     /// <summary></summary>
    void restAPI_GetNXDNCC(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
   /// <summary></summary>
    void restAPI_GetNXDNDebug(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /// <summary></summary>
    void restAPI_GetNXDNDumpRCCH(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /// <summary></summary>
    void restAPI_GetNXDNCCEnable(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
};

#endif // __REST_API_H__
