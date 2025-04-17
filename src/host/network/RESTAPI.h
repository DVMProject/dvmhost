// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023-2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file RESTAPI.h
 * @ingroup host_rest
 * @file RESTAPI.cpp
 * @ingroup host_rest
 */
#if !defined(__REST_API_H__)
#define __REST_API_H__

#include "Defines.h"
#include "common/network/rest/RequestDispatcher.h"
#include "common/network/rest/http/HTTPServer.h"
#include "common/network/rest/http/SecureHTTPServer.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "common/Thread.h"
#include "network/RESTDefines.h"

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
// ---------------------------------------------------------------------------

/**
 * @brief Implements the REST API server logic.
 * @ingroup host_rest
 */
class HOST_SW_API RESTAPI : private Thread {
public:
    /**
     * @brief Initializes a new instance of the RESTAPI class.
     * @param address Network Hostname/IP address to connect to.
     * @param port Network port number.
     * @param password Authentication password.
     * @param keyFile SSL certificate private key.
     * @param certFile SSL certificate.
     * @param enableSSL Flag indicating SSL should be used for HTTPS support.
     * @param host Instance of the Host class.
     * @param debug Flag indicating verbose logging should be enabled.
     */
    RESTAPI(const std::string& address, uint16_t port, const std::string& password, const std::string& keyFile, const std::string& certFile,
        bool enableSSL, Host* host, bool debug);
    /**
     * @brief Finalizes a instance of the RESTAPI class.
     */
    ~RESTAPI() override;

    /**
     * @brief Sets the instances of the Radio ID and Talkgroup ID lookup tables.
     * @param ridLookup Radio ID Lookup Table Instance
     * @param tidLookup Talkgroup Rules Lookup Table Instance
     */
    void setLookups(::lookups::RadioIdLookup* ridLookup, ::lookups::TalkgroupRulesLookup* tidLookup);
    /**
     * @brief Sets the instances of the digital radio protocols.
     * @param dmr Instance of the DMR Control class.
     * @param p25 Instance of the P25 Control class.
     * @param nxdn Instance of the NXDN Control class.
     */
    void setProtocols(dmr::Control* dmr, p25::Control* p25, nxdn::Control* nxdn);

    /**
     * @brief Opens connection to the network.
     * @returns bool True, if REST API services are started, otherwise false. 
     */
    bool open();

    /**
     * @brief Closes connection to the network.
     */
    void close();

private:
    typedef network::rest::RequestDispatcher<network::rest::http::HTTPPayload, network::rest::http::HTTPPayload> RESTDispatcherType;
    typedef network::rest::http::HTTPPayload HTTPPayload;
    RESTDispatcherType m_dispatcher;
    network::rest::http::HTTPServer<RESTDispatcherType> m_restServer;
#if defined(ENABLE_SSL)
    network::rest::http::SecureHTTPServer<RESTDispatcherType> m_restSecureServer;
    bool m_enableSSL;
#endif // ENABLE_SSL

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
    ::lookups::TalkgroupRulesLookup* m_tidLookup;

    typedef std::unordered_map<std::string, uint64_t>::value_type AuthTokenValueType;
    std::unordered_map<std::string, uint64_t> m_authTokens;

    /**
     * @brief Thread entry point. This function is provided to run the thread
     *  for the REST API services.
     */
    void entry() override;

    /**
     * @brief Helper to initialize REST API endpoints.
     */
    void initializeEndpoints();

    /**
     * @brief Helper to invalidate a host token.
     * @param host Host.
     */
    void invalidateHostToken(const std::string host);
    /**
     * @brief Helper to validate authentication for REST API.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @returns bool True, if authentication token is valid, otherwise false.
     */
    bool validateAuth(const HTTPPayload& request, HTTPPayload& reply);

    /**
     * @brief REST API endpoint; implements authentication request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_PutAuth(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /**
     * @brief REST API endpoint; implements get version request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetVersion(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements get status request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetStatus(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements get voice channels request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetVoiceCh(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /**
     * @brief REST API endpoint; implements put/set modem mode request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_PutModemMode(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements put/request modem kill request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_PutModemKill(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /**
     * @brief REST API endpoint; implements set supervisory mode request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_PutSetSupervisor(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements permit TG request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_PutPermitTG(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements grant TG request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_PutGrantTG(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements release grants request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetReleaseGrants(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements release affiliations request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetReleaseAffs(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /**
     * @brief REST API endpoint; implements get RID whitelist request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetRIDWhitelist(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements get RID blacklist request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetRIDBlacklist(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /*
    ** Digital Mobile Radio
    */

    /**
     * @brief REST API endpoint; implements fire DMR beacon request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetDMRBeacon(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements get DMR debug state request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetDMRDebug(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements get DMR dump CSBK state request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetDMRDumpCSBK(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements DMR RID operations request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_PutDMRRID(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements toggle DMR CC enable request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetDMRCCEnable(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements toggle DMR CC broadcast request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetDMRCCBroadcast(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements get DMR affiliations request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetDMRAffList(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /*
    ** Project 25
    */

    /**
     * @brief REST API endpoint; implements fire P25 CC request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetP25CC(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements P25 debug state request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetP25Debug(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements P25 dump TSBK state request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetP25DumpTSBK(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements P25 RID operation request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_PutP25RID(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements toggle P25 CC request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetP25CCEnable(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements toggle P25 broadcast request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetP25CCBroadcast(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements transmitting raw TSBK request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_PutP25RawTSBK(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements get P25 affiliations request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetP25AffList(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /*
    ** Next Generation Digital Narrowband
    */

    /**
     * @brief REST API endpoint; implements fire NXDN CC request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetNXDNCC(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements NXDN debug state request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetNXDNDebug(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements NXDN dump RCCH state request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetNXDNDumpRCCH(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements toggle NXDN CC request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetNXDNCCEnable(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements get NXDN affiliations request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetNXDNAffList(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
};

#endif // __REST_API_H__
