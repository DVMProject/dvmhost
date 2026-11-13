// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */

#include <catch2/catch_test_macros.hpp>

#include "fne/PatchStatusRegistry.h"

#include <chrono>
#include <future>
#include <string>
#include <thread>

namespace {
    /**
     * @brief Creates a JSON object representing a patch member.
     * @param system The system name of the patch member.
     * @param mode The mode of the patch member.
     * @param tgid The talkgroup ID of the patch member.
     * @param slot The slot number of the patch member.
     * @return json::object The JSON representation of the patch member.
     */
    json::object member(const std::string& system, const std::string& mode,
        uint32_t tgid, uint32_t slot)
    {
        json::object value = json::object();
        value["system"].set<std::string>(system);
        value["mode"].set<std::string>(mode);
        value["tgid"].set<uint32_t>(tgid);
        value["slot"].set<uint32_t>(slot);
        return value;
    }

    /**
     * @brief Creates a JSON object representing a patch.
     * @param patchId The ID of the patch.
     * @param members The array of patch members.
     * @param active Whether the patch is active.
     * @param oneWay Whether the patch is one-way.
     * @return json::object The JSON representation of the patch.
     */
    json::object patch(const std::string& patchId, const json::array& members,
        bool active = true, bool oneWay = false)
    {
        json::object value = json::object();
        value["patchId"].set<std::string>(patchId);
        value["active"].set<bool>(active);
        value["oneWay"].set<bool>(oneWay);
        value["members"].set<json::array>(members);
        return value;
    }

    /**
     * @brief Creates a JSON object representing a publish request.
     * @param peerId The ID of the peer making the request.
     * @param sequence The sequence number of the request.
     * @param patches The array of patches being published.
     * @param ttlSeconds The time-to-live for the request.
     * @param originFnePeerId The originating FNE peer ID.
     * @return json::object The JSON representation of the publish request.
     */
    json::object publishRequest(uint32_t peerId, uint32_t sequence,
        const json::array& patches, uint32_t ttlSeconds = 15U,
        uint32_t originFnePeerId = 0U)
    {
        json::object request = json::object();
        request["peerId"].set<uint32_t>(peerId);
        request["peerName"].set<std::string>("Dispatch Console");
        request["originFnePeerId"].set<uint32_t>(originFnePeerId);
        request["sequence"].set<uint32_t>(sequence);
        request["ttlSeconds"].set<uint32_t>(ttlSeconds);
        request["patches"].set<json::array>(patches);
        return request;
    }

    /**
     * @brief Publishes a patch status update to the registry.
     * @param registry The patch status registry instance.
     * @param request The JSON publish request.
     * @param response The JSON response object to be populated.
     * @param error The error string to be populated in case of failure.
     * @param changed Optional pointer to a boolean indicating if the registry changed.
     * @return bool True if the publish was successful, false otherwise.
     */
    bool publish(PatchStatusRegistry& registry, json::object request,
        json::object& response, std::string& error, bool* changed = nullptr)
    {
        return registry.publish(request, response, error, changed);
    }
}

TEST_CASE("FNE patch status registry publishes normalized indexed snapshots",
    "[fne][patch-status]")
{
    PatchStatusRegistry registry;
    json::array members = json::array();
    members.push_back(json::value(member("SYS1", "TG3100", 3100U, 2U)));
    members.push_back(json::value(member("SYS2", "TG4100", 4100U, 0U)));

    json::array patches = json::array();
    patches.push_back(json::value(patch("patch-7", members, true, true)));
    json::object response;
    std::string error;
    bool changed = false;

    REQUIRE(publish(registry, publishRequest(1001U, 7U, patches, 30U, 9001U),
        response, error, &changed));
    REQUIRE(changed);
    REQUIRE(error.empty());
    REQUIRE(response["revision"].get<uint64_t>() == 1U);
    REQUIRE(response["acceptedPeerId"].get<uint32_t>() == 1001U);
    REQUIRE(response["ttlSeconds"].get<uint32_t>() == 30U);

    json::array peerSnapshots = response["peers"].get<json::array>();
    REQUIRE(peerSnapshots.size() == 1U);
    json::object peer = peerSnapshots[0U].get<json::object>();
    REQUIRE(peer["peerId"].get<uint32_t>() == 1001U);
    REQUIRE(peer["originFnePeerId"].get<uint32_t>() == 9001U);
    REQUIRE(peer["sequence"].get<uint32_t>() == 7U);

    json::object index = response["byTalkgroup"].get<json::object>();
    REQUIRE(index["tg3100:3100:2"].get<json::array>().size() == 1U);
    REQUIRE(index["tg4100:4100:0"].get<json::array>().size() == 1U);
    json::object indexed = index["tg3100:3100:2"].get<json::array>()[0U].get<json::object>();
    REQUIRE(indexed["patchId"].get<std::string>() == "patch-7");
    REQUIRE(indexed["oneWay"].get<bool>());
    json::object indexedMember = indexed["member"].get<json::object>();
    REQUIRE(indexedMember["mode"].get<std::string>() == "tg3100");
    REQUIRE(indexedMember["key"].get<std::string>() == "tg3100:3100:2");
}

TEST_CASE("FNE patch status registry applies sequence and replacement semantics",
    "[fne][patch-status]")
{
    PatchStatusRegistry registry;
    json::array firstMembers = json::array();
    firstMembers.push_back(json::value(member("A", "P25", 100U, 1U)));
    json::array firstPatches = json::array();
    firstPatches.push_back(json::value(patch("first", firstMembers)));

    json::object response;
    std::string error;
    bool changed = false;
    REQUIRE(publish(registry, publishRequest(42U, 10U, firstPatches, 15U, 7U),
        response, error, &changed));
    REQUIRE(changed);

    changed = true;
    REQUIRE(publish(registry, publishRequest(42U, 9U, json::array(), 15U, 7U),
        response, error, &changed));
    REQUIRE_FALSE(changed);
    REQUIRE(registry.revision() == 1U);
    REQUIRE(response["patches"].get<json::array>().size() == 1U);

    changed = false;
    REQUIRE(publish(registry, publishRequest(42U, 9U, json::array(), 15U, 8U),
        response, error, &changed));
    REQUIRE(changed);
    REQUIRE(registry.revision() == 2U);
    REQUIRE(response["patches"].get<json::array>().empty());

    REQUIRE_FALSE(registry.removePeer(42U));
    REQUIRE_FALSE(registry.removePeer(0U));
}

TEST_CASE("FNE patch status registry does not revise identical snapshots",
    "[fne][patch-status]")
{
    PatchStatusRegistry registry;
    json::array members = json::array();
    members.push_back(json::value(member("A", "P25", 1200U, 0U)));
    json::array patches = json::array();
    patches.push_back(json::value(patch("same", members)));

    json::object response;
    std::string error;
    bool changed = false;
    json::object request = publishRequest(77U, 0U, patches);
    REQUIRE(publish(registry, request, response, error, &changed));
    REQUIRE(changed);

    changed = true;
    REQUIRE(publish(registry, request, response, error, &changed));
    REQUIRE_FALSE(changed);
    REQUIRE(registry.revision() == 1U);

    REQUIRE(registry.removePeer(77U));
    REQUIRE(registry.revision() == 2U);
    REQUIRE(registry.snapshot()["peers"].get<json::array>().empty());
}

TEST_CASE("FNE patch status registry validates malformed and oversized publishes",
    "[fne][patch-status][validation]")
{
    PatchStatusRegistry registry;
    json::object response;
    std::string error;

    json::object missingPeer = json::object();
    missingPeer["patches"].set<json::array>(json::array());
    REQUIRE_FALSE(publish(registry, missingPeer, response, error));
    REQUIRE(error == "peerId was not a valid integer");

    json::array members = json::array();
    members.push_back(json::value(member("A", "P25", 0U, 1U)));
    json::array patches = json::array();
    patches.push_back(json::value(patch("bad-tgid", members)));
    error.clear();
    REQUIRE_FALSE(publish(registry, publishRequest(1U, 1U, patches), response, error));
    REQUIRE(error == "patch member tgid cannot be zero");

    members = json::array();
    members.push_back(json::value(member("A", "P25", 1U, 256U)));
    patches = json::array();
    patches.push_back(json::value(patch("bad-slot", members)));
    error.clear();
    REQUIRE_FALSE(publish(registry, publishRequest(1U, 1U, patches), response, error));
    REQUIRE(error == "patch member slot was out of range");

    patches = json::array();
    for (size_t i = 0U; i <= PatchStatusRegistry::MAX_PATCHES_PER_PEER; i++)
        patches.push_back(json::value(patch(std::to_string(i), json::array())));
    error.clear();
    REQUIRE_FALSE(publish(registry, publishRequest(1U, 1U, patches), response, error));
    REQUIRE(error == "patches exceeded the maximum allowed count");
    REQUIRE(registry.revision() == 0U);
}

TEST_CASE("FNE patch status registry clamps TTL and expires records",
    "[fne][patch-status][ttl]")
{
    PatchStatusRegistry registry;
    registry.configure(0U, 1U, 2U);
    REQUIRE(registry.defaultTtlSeconds() == 1U);
    REQUIRE(registry.minTtlSeconds() == 1U);
    REQUIRE(registry.maxTtlSeconds() == 2U);

    json::array members = json::array();
    members.push_back(json::value(member("A", "P25", 101U, 0U)));
    json::array patches = json::array();
    patches.push_back(json::value(patch("short-lived", members)));
    json::object response;
    std::string error;
    REQUIRE(publish(registry, publishRequest(55U, 1U, patches, 0U), response, error));
    REQUIRE(response["ttlSeconds"].get<uint32_t>() == 1U);

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    REQUIRE(registry.cleanupExpired() == 1U);
    REQUIRE(registry.revision() == 2U);
    REQUIRE(registry.snapshot()["peers"].get<json::array>().empty());
}

TEST_CASE("FNE patch status registry long poll wakes on revision change",
    "[fne][patch-status][long-poll]")
{
    PatchStatusRegistry registry;
    std::future<json::object> waiting = std::async(std::launch::async, [&registry]() {
        return registry.waitForChanges(0U, 2000U);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    json::array members = json::array();
    members.push_back(json::value(member("A", "P25", 101U, 1U)));
    json::array patches = json::array();
    patches.push_back(json::value(patch("wake", members)));
    json::object response;
    std::string error;
    REQUIRE(publish(registry, publishRequest(88U, 1U, patches), response, error));

    REQUIRE(waiting.wait_for(std::chrono::milliseconds(500)) == std::future_status::ready);
    REQUIRE(waiting.get()["revision"].get<uint64_t>() == 1U);
}
