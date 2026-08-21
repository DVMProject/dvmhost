// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 */

#include <catch2/catch_test_macros.hpp>

#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/AdjSiteMapLookup.h"
#include "common/lookups/PeerListLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "common/restapi/http/HTTPPayload.h"
#include "fne/restapi/RESTAPI.h"
#include "fne/restapi/RESTDefines.h"
#include "remote/RESTClient.h"

#include <asio.hpp>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace restapi::http;
using namespace fne_restapi;

namespace {
    // ---------------------------------------------------------------------------
    //  Global Variables
    // ---------------------------------------------------------------------------

    constexpr uint32_t TEST_RID = 123456U;
    const std::string TEST_PASSWORD = "fne-rest-test-password";

    // ---------------------------------------------------------------------------
    //  Global Functions
    // ---------------------------------------------------------------------------

    /**
     * @brief Reserves an available loopback port.
     * @returns uint16_t 
     */
    uint16_t reserveLoopbackPort()
    {
        asio::io_service ioService;
        asio::ip::tcp::acceptor acceptor(ioService,
            asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0U));
        return acceptor.local_endpoint().port();
    }

    /**
     * @brief Generates a temporary file path for the ACL file.
     * @returns std::string 
     */
    std::string temporaryACLPath()
    {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        return "/tmp/dvm-fne-rest-rid-" + std::to_string(stamp) + ".csv";
    }

    /** 
     * @brief Sends a REST API request to the specified endpoint with the given payload.
     * @param port The port on which the REST API server is running.
     * @param method The HTTP method to use for the request.
     * @param endpoint The REST API endpoint to send the request to.
     * @param payload The JSON payload to include in the request.
     * @return The JSON response from the server.
     */
    json::object sendRequest(uint16_t port, const std::string& method,
        const std::string& endpoint, json::object payload = json::object())
    {
        json::object response = json::object();
        REQUIRE(RESTClient::send("127.0.0.1", port, TEST_PASSWORD, method, endpoint,
            payload, response, false, 1000, false) == HTTPPayload::OK);
        REQUIRE(response["status"].get<int>() == HTTPPayload::OK);
        return response;
    }

    /**
     * @brief Creates a JSON payload for a talkgroup with the specified parameters.
     * @param tgid The talkgroup ID.
     * @param slot The slot number.
     * @param name The name of the talkgroup.
     * @return The JSON object representing the talkgroup.
     */
    json::object talkgroupPayload(uint32_t tgid, uint8_t slot, const std::string& name)
    {
        json::object source = json::object();
        source["tgid"].set<uint32_t>(tgid);
        source["slot"].set<uint8_t>(slot);

        json::object config = json::object();
        config["active"] = json::value(true);
        config["affiliated"] = json::value(false);
        config["parrot"] = json::value(false);
        config["strapping"] = json::value((double)lookups::TG_STRAPPING_SELECTABLE);
        config["inclusion"].set<json::array>(json::array());
        config["exclusion"].set<json::array>(json::array());
        config["rewrite"].set<json::array>(json::array());
        config["always"].set<json::array>(json::array());
        config["preferred"].set<json::array>(json::array());
        config["permittedRids"].set<json::array>(json::array());

        json::object tg = json::object();
        tg["name"].set<std::string>(name);
        tg["alias"].set<std::string>(name + " alias");
        tg["source"].set<json::object>(source);
        tg["config"].set<json::object>(config);
        return tg;
    }

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Scoped wrapper for a temporary ACL file.
     */
    class ScopedACLFile {
    public:
        /**
         * @brief Initializes a new instance of the ScopedACLFile class.
         */
        ScopedACLFile() : path(temporaryACLPath()) { }
        /**
         * @brief Finalizes a instance of the ScopedACLFile class.
         */
        ~ScopedACLFile() { std::remove(path.c_str()); }

        std::string path;
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Scoped wrapper for the RESTAPI instance used in tests.
     */
    class ScopedRESTAPI {
    public:
        /**
         * @brief Initializes a new instance of the ScopedRESTAPI class.
         * @param port The port on which the REST API server will listen.
         * @param ridLookup The Radio ID lookup instance.
         * @param tgLookup The Talkgroup Rules lookup instance.
         * @param peerLookup The Peer List lookup instance.
         * @param adjLookup The Adjacent Site Map lookup instance.
         */
        ScopedRESTAPI(uint16_t port, lookups::RadioIdLookup* ridLookup = nullptr,
            lookups::TalkgroupRulesLookup* tgLookup = nullptr,
            lookups::PeerListLookup* peerLookup = nullptr,
            lookups::AdjSiteMapLookup* adjLookup = nullptr) :
            api("127.0.0.1", port, TEST_PASSWORD, "", "", false, nullptr, false)
        {
            api.setLookups(ridLookup, tgLookup, peerLookup, adjLookup, nullptr);
            REQUIRE(api.open());
        }
        /**
         * @brief Finalizes a instance of the ScopedRESTAPI class.
         */
        ~ScopedRESTAPI() { api.close(); }

        fne_restapi::RESTAPI api;
    };
} // namespace


TEST_CASE("FNE REST RID add and commit updates the persisted ACL", "[fne][restapi][rid]")
{
    ScopedACLFile aclFile;
    lookups::RadioIdLookup lookup(aclFile.path, 0U, true);
    const uint16_t port = reserveLoopbackPort();
    ScopedRESTAPI server(port, &lookup);

    json::object add = json::object();
    add["rid"] = json::value((double)TEST_RID);
    add["enabled"] = json::value(true);
    add["alias"].set<std::string>("REST test radio");
    add["canRequestKeys"] = json::value(true);
    add["canRekey"] = json::value(true);
    json::array allowedKIds = json::array();
    allowedKIds.push_back(json::value((double)0x1234U));
    allowedKIds.push_back(json::value((double)0xABCDU));
    add["allowedKIds"].set<json::array>(allowedKIds);

    sendRequest(port, HTTP_PUT, FNE_PUT_RID_ADD, add);

    const lookups::RadioId added = lookup.find(TEST_RID);
    REQUIRE_FALSE(added.radioDefault());
    CHECK(added.radioEnabled());
    CHECK(added.radioAlias() == "REST test radio");
    CHECK(added.canRequestKeys());
    CHECK(added.canRekey());
    CHECK(added.allowedKIds() == std::vector<uint16_t> { 0x1234U, 0xABCDU });

    sendRequest(port, HTTP_GET, FNE_GET_RID_COMMIT);

    lookups::RadioIdLookup persisted(aclFile.path, 0U, true);
    REQUIRE(persisted.read());
    const lookups::RadioId saved = persisted.find(TEST_RID);
    REQUIRE_FALSE(saved.radioDefault());
    CHECK(saved.radioAlias() == "REST test radio");
    CHECK(saved.allowedKIds() == std::vector<uint16_t> { 0x1234U, 0xABCDU });
}

TEST_CASE("FNE REST RID delete and commit removes the RID from the ACL", "[fne][restapi][rid]")
{
    ScopedACLFile aclFile;
    lookups::RadioIdLookup lookup(aclFile.path, 0U, true);
    lookup.addEntry(TEST_RID, true, "radio to delete");
    lookup.commit(true);

    const uint16_t port = reserveLoopbackPort();
    ScopedRESTAPI server(port, &lookup);

    json::object remove = json::object();
    remove["rid"] = json::value((double)TEST_RID);
    sendRequest(port, HTTP_PUT, FNE_PUT_RID_DELETE, remove);
    CHECK(lookup.find(TEST_RID).radioDefault());

    sendRequest(port, HTTP_GET, FNE_GET_RID_COMMIT);

    std::ifstream savedACL(aclFile.path);
    REQUIRE(savedACL.good());
    const std::string contents((std::istreambuf_iterator<char>(savedACL)),
        std::istreambuf_iterator<char>());
    CHECK(contents.find(std::to_string(TEST_RID)) == std::string::npos);
}

TEST_CASE("FNE REST peer endpoints query, persist, and delete ACL entries", "[fne][restapi][peer]")
{
    ScopedACLFile aclFile;
    lookups::PeerListLookup lookup(aclFile.path, 0U, true, false);
    const uint16_t port = reserveLoopbackPort();
    ScopedRESTAPI server(port, nullptr, nullptr, &lookup);

    json::object add = json::object();
    add["peerId"] = json::value((double)9001U);
    add["peerAlias"].set<std::string>("REST peer");
    add["peerPassword"].set<std::string>("peer secret");
    add["peerReplica"] = json::value(true);
    add["canRequestKeys"] = json::value(true);
    add["canIssueInhibit"] = json::value(true);
    add["hasCallPriority"] = json::value(true);
    sendRequest(port, HTTP_PUT, FNE_PUT_PEER_ADD, add);

    const lookups::PeerId peer = lookup.find(9001U);
    CHECK(peer.peerAlias() == "REST peer");
    CHECK(peer.peerPassword() == "peer secret");
    CHECK(peer.peerReplica());
    CHECK(peer.canRequestKeys());
    CHECK(peer.canIssueInhibit());
    CHECK(peer.hasCallPriority());

    json::object query = sendRequest(port, HTTP_GET, FNE_GET_PEER_LIST);
    REQUIRE(query["peers"].get<json::array>().size() == 1U);
    CHECK(query["peers"].get<json::array>()[0U].get<json::object>()["peerPassword"].get<bool>());

    sendRequest(port, HTTP_GET, FNE_GET_PEER_COMMIT);
    lookups::PeerListLookup persisted(aclFile.path, 0U, true, false);
    REQUIRE(persisted.read());
    CHECK(persisted.find(9001U).peerAlias() == "REST peer");

    json::object remove = json::object();
    remove["peerId"] = json::value((double)9001U);
    sendRequest(port, HTTP_PUT, FNE_PUT_PEER_DELETE, remove);
    sendRequest(port, HTTP_GET, FNE_GET_PEER_COMMIT);
    CHECK(lookup.find(9001U).peerDefault());
}

TEST_CASE("FNE REST talkgroup endpoints query, persist, and delete rules", "[fne][restapi][tg]")
{
    ScopedACLFile rulesFile;
    lookups::TalkgroupRulesLookup lookup(rulesFile.path, 0U, true, false);
    const uint16_t port = reserveLoopbackPort();
    ScopedRESTAPI server(port, nullptr, &lookup);

    sendRequest(port, HTTP_PUT, FNE_PUT_TGID_ADD, talkgroupPayload(3100U, 1U, "REST TG"));
    const lookups::TalkgroupRuleGroupVoice added = lookup.find(3100U, 1U);
    REQUIRE_FALSE(added.isInvalid());
    CHECK(added.name() == "REST TG");
    CHECK(added.config().active());

    json::object query = sendRequest(port, HTTP_GET, FNE_GET_TGID_QUERY);
    REQUIRE(query["tgs"].get<json::array>().size() == 1U);
    sendRequest(port, HTTP_GET, FNE_GET_TGID_COMMIT);

    lookups::TalkgroupRulesLookup persisted(rulesFile.path, 0U, true, false);
    REQUIRE(persisted.read());
    CHECK_FALSE(persisted.find(3100U, 1U).isInvalid());

    json::object remove = json::object();
    remove["tgid"] = json::value((double)3100U);
    remove["slot"] = json::value((double)1U);
    sendRequest(port, HTTP_PUT, FNE_PUT_TGID_DELETE, remove);
    sendRequest(port, HTTP_GET, FNE_GET_TGID_COMMIT);
    CHECK(lookup.find(3100U, 1U).isInvalid());
}

TEST_CASE("FNE REST adjacent-map endpoints query, persist, and delete entries", "[fne][restapi][adjmap]")
{
    ScopedACLFile mapFile;
    lookups::AdjSiteMapLookup lookup(mapFile.path, 0U);
    const uint16_t port = reserveLoopbackPort();
    ScopedRESTAPI server(port, nullptr, nullptr, nullptr, &lookup);

    json::object add = json::object();
    add["peerId"] = json::value((double)100U);
    json::array neighbors = json::array();
    neighbors.push_back(json::value((double)101U));
    neighbors.push_back(json::value((double)102U));
    add["neighbors"].set<json::array>(neighbors);
    sendRequest(port, HTTP_PUT, FNE_PUT_ADJ_MAP_ADD, add);

    CHECK(lookup.find(100U).neighbors() == std::vector<uint32_t> { 101U, 102U });
    json::object query = sendRequest(port, HTTP_GET, FNE_GET_ADJ_MAP_LIST);
    REQUIRE(query["peers"].get<json::array>().size() == 1U);
    sendRequest(port, HTTP_GET, FNE_GET_ADJ_MAP_COMMIT);

    lookups::AdjSiteMapLookup persisted(mapFile.path, 0U);
    REQUIRE(persisted.read());
    CHECK(persisted.find(100U).neighbors() == std::vector<uint32_t> { 101U, 102U });

    json::object remove = json::object();
    remove["peerId"] = json::value((double)100U);
    sendRequest(port, HTTP_PUT, FNE_PUT_ADJ_MAP_DELETE, remove);
    sendRequest(port, HTTP_GET, FNE_GET_ADJ_MAP_COMMIT);
    CHECK(lookup.find(100U).neighbors().empty());
}

TEST_CASE("FNE REST reload endpoints refresh their configured lookup tables", "[fne][restapi][reload]")
{
    ScopedACLFile ridFile;
    ScopedACLFile peerFile;
    ScopedACLFile tgFile;
    lookups::RadioIdLookup rids(ridFile.path, 0U, true);
    lookups::PeerListLookup peers(peerFile.path, 0U, true, false);
    lookups::TalkgroupRulesLookup tgs(tgFile.path, 0U, true, false);

    rids.addEntry(1001U, true, "persisted RID");
    rids.commit(true);
    lookups::PeerId peer(2001U, "persisted peer", "secret", false);
    peers.addEntry(2001U, peer);
    peers.commit(true);

    const uint16_t port = reserveLoopbackPort();
    ScopedRESTAPI server(port, &rids, &tgs, &peers);
    sendRequest(port, HTTP_PUT, FNE_PUT_TGID_ADD, talkgroupPayload(3001U, 1U, "persisted TG"));
    sendRequest(port, HTTP_GET, FNE_GET_TGID_COMMIT);

    rids.addEntry(1002U, true, "memory only");
    peers.addEntry(2002U, lookups::PeerId(2002U, "memory only", "", false));
    sendRequest(port, HTTP_PUT, FNE_PUT_TGID_ADD, talkgroupPayload(3002U, 1U, "memory only"));

    sendRequest(port, HTTP_GET, FNE_GET_RELOAD_RIDS);
    sendRequest(port, HTTP_GET, FNE_GET_RELOAD_PEERLIST);
    sendRequest(port, HTTP_GET, FNE_GET_RELOAD_TGS);

    CHECK_FALSE(rids.find(1001U).radioDefault());
    CHECK(rids.find(1002U).radioDefault());
    CHECK_FALSE(peers.find(2001U).peerDefault());
    CHECK(peers.find(2002U).peerDefault());
    CHECK_FALSE(tgs.find(3001U, 1U).isInvalid());
    CHECK(tgs.find(3002U, 1U).isInvalid());
}
