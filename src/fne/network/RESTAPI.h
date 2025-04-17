// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file RESTAPI.h
 * @ingroup fne_rest
 * @file RESTAPI.cpp
 * @ingroup fne_rest
 */
#if !defined(__REST_API_H__)
#define __REST_API_H__

#include "fne/Defines.h"
#include "common/network/rest/RequestDispatcher.h"
#include "common/network/rest/http/HTTPServer.h"
#include "common/network/rest/http/SecureHTTPServer.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "common/Thread.h"
#include "fne/network/RESTDefines.h"

#include <vector>
#include <string>
#include <random>

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------

class HOST_SW_API HostFNE;
namespace network { class HOST_SW_API FNENetwork; }

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Implements the REST API server logic.
 * @ingroup fne_rest
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
     * @param host Instance of the HostFNE class.
     * @param debug Flag indicating verbose logging should be enabled.
     */
    RESTAPI(const std::string& address, uint16_t port, const std::string& password, const std::string& keyFile, const std::string& certFile,
        bool enableSSL, HostFNE* host, bool debug);
    /**
     * @brief Finalizes a instance of the RESTAPI class.
     */
    ~RESTAPI() override;

    /**
     * @brief Sets the instances of the Radio ID, Talkgroup ID and Peer List lookup tables.
     * @param ridLookup Radio ID Lookup Table Instance
     * @param tidLookup Talkgroup Rules Lookup Table Instance
     * @param peerListLookup Peer List Lookup Table Instance
     */
    void setLookups(::lookups::RadioIdLookup* ridLookup, ::lookups::TalkgroupRulesLookup* tidLookup, ::lookups::PeerListLookup* peerListLookup);
    /**
     * @brief Sets the instance of the FNE network.
     * @param network Instance oft he FNENetwork class.
     */
    void setNetwork(::network::FNENetwork* network);

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

    std::string m_password;
    uint8_t* m_passwordHash;
    bool m_debug;

    HostFNE* m_host;
    network::FNENetwork* m_network;

    ::lookups::RadioIdLookup* m_ridLookup;
    ::lookups::TalkgroupRulesLookup* m_tidLookup;
    ::lookups::PeerListLookup* m_peerListLookup;

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
     * @brief REST API endpoint; implements get peer query request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetPeerQuery(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements get peer count request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetPeerCount(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements get peer reset request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_PutPeerReset(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /**
     * @brief REST API endpoint; implements get radio ID query request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetRIDQuery(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements put radio ID add request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_PutRIDAdd(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements put radio ID delete request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_PutRIDDelete(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements put radio ID commit request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetRIDCommit(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /**
     * @brief REST API endpoint; implements get talkgroup ID query request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetTGQuery(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements put talkgroup ID add request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_PutTGAdd(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements put talkgroup ID delete request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_PutTGDelete(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements put talkgroup ID commit request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetTGCommit(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /**
     * @brief REST API endpoint; implements get peer list query request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetPeerList(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements put peer add request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_PutPeerAdd(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements put peer delete request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_PutPeerDelete(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements put peer list commit request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetPeerCommit(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief 
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetPeerMode(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /**
     * @brief 
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetForceUpdate(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /**
     * @brief REST API endpoint; implements get reload talkgroup ID list request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetReloadTGs(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
    /**
     * @brief REST API endpoint; implements get reload radio ID list request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetReloadRIDs(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /**
     * @brief REST API endpoint; implements get affiliation list request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_GetAffList(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /*
    ** Digital Mobile Radio
    */

    /**
     * @brief REST API endpoint; implements DMR RID operations request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_PutDMRRID(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);

    /*
    ** Project 25
    */

    /**
     * @brief REST API endpoint; implements P25 RID operation request.
     * @param request HTTP request.
     * @param reply HTTP reply.
     * @param match HTTP request matcher.
     */
    void restAPI_PutP25RID(const HTTPPayload& request, HTTPPayload& reply, const network::rest::RequestMatch& match);
};

#endif // __REST_API_H__
