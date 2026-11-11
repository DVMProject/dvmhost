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

#include "fne/FNETestHooks.h"
#include "fne/HostFNE.h"
#include "fne/network/callhandler/TagAnalogData.h"
#include "fne/network/UnitRouteRegistry.h"
#include "common/analog/AnalogDefines.h"
#include "common/BitManipulation.h"
#include "common/lookups/AffiliationLookup.h"
#include "common/lookups/PeerListLookup.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Test harness for FNE network tests.
 */
class FNEHarness {
public:
    /**
     * @brief Initializes a new instance of the FNEHarness class.
     */
    FNEHarness() :
        configFile("fne-test.yml"),
        host(configFile),
        radios("", 0U, true, false),
        talkgroups("", 0U, true, false),
        traffic(&host, "127.0.0.1", 62031U, 999999U, "test-password", "test-fne",
            false, false, false, false, true, true, true, true, true,
            0U, false, true, true, 5U, 10U, 2U),
        metadata(&host, &traffic, "127.0.0.1", 62032U, 2U)
    {
        traffic.setLookups(&radios, nullptr, &talkgroups, nullptr, nullptr, nullptr);
    }
    /**
     * @brief Finalizes a instance of the FNEHarness class.
     */
    ~FNEHarness()
    {
        FNETestHooks::clearPeerWriteCapture(traffic);
        FNETestHooks::clearMetadataWriteCapture(metadata);
        FNETestHooks::clearPeers(traffic);
    }

    std::string configFile;
    HostFNE host;
    lookups::RadioIdLookup radios;
    lookups::TalkgroupRulesLookup talkgroups;
    network::TrafficNetwork traffic;
    network::MetadataNetwork metadata;
};

TEST_CASE("FNE metadata returns subscriber operation and P25 status results",
    "[fne][metadata][subscriber-operations]")
{
    constexpr uint32_t CONSOLE_PEER_ID = 1001U;
    constexpr uint32_t RADIO_PEER_ID = 2001U;
    constexpr uint32_t SUBSCRIBER_ID = 123456U;

    FNEHarness harness;
    network::FNEPeerConnection& console = FNETestHooks::addPeer(
        harness.traffic, CONSOLE_PEER_ID, network::NET_STAT_RUNNING, true);
    console.peerClass(network::PEER_CONN_CLASS_CONSOLE);

    const uint64_t operationId = harness.traffic.subscriberOperations().start(
        network::SubscriberOperationStore::Protocol::P25,
        network::SubscriberOperationStore::Kind::STATUS_QUERY, SUBSCRIBER_ID, RADIO_PEER_ID);
    REQUIRE(operationId != 0U);
    REQUIRE(harness.traffic.subscriberOperations().addP25Status(
        SUBSCRIBER_ID, RADIO_PEER_ID, 0x1234U));

    std::vector<uint8_t> responsePayload;
    uint8_t responseFunction = network::NET_FUNC::ILLEGAL;
    uint8_t responseSubFunction = network::NET_SUBFUNC::NOP;
    FNETestHooks::captureMetadataWrite(harness.metadata, responsePayload,
        responseFunction, responseSubFunction);

    json::object operationRequest = json::object();
    operationRequest["operationId"].set<uint64_t>(operationId);
    const std::string operationJson = json::value(operationRequest).serialize();
    std::vector<uint8_t> requestPayload(11U + operationJson.length(), 0U);
    ::memcpy(requestPayload.data() + 11U, operationJson.data(), operationJson.length());

    FNETestHooks::dispatchMetadata(harness.traffic, harness.metadata,
        network::NET_FUNC::SUBSCRIBER_OP, network::NET_SUBFUNC::SUBSCRIBER_OP_OPERATION,
        CONSOLE_PEER_ID, requestPayload);

    REQUIRE(responsePayload.size() > 11U);
    CHECK(responseFunction == network::NET_FUNC::SUBSCRIBER_OP);
    CHECK(responseSubFunction == network::NET_SUBFUNC::SUBSCRIBER_OP_OPERATION);
    json::value operationResponseValue;
    REQUIRE(json::parse(operationResponseValue,
        std::string(reinterpret_cast<const char*>(responsePayload.data() + 11U),
            responsePayload.size() - 11U))
                .empty());
    REQUIRE(operationResponseValue.is<json::object>());
    json::object& operationResponse = operationResponseValue.get<json::object>();
    CHECK(operationResponse["found"].get<bool>());
    CHECK(operationResponse["operationId"].get<uint64_t>() == operationId);
    CHECK(operationResponse["state"].get<std::string>() == "succeeded");
    CHECK(operationResponse["resultValue"].get<uint16_t>() == 0x1234U);

    json::object statusRequest = json::object();
    statusRequest["subscriberId"].set<uint32_t>(SUBSCRIBER_ID);
    const std::string statusJson = json::value(statusRequest).serialize();
    requestPayload.assign(11U + statusJson.length(), 0U);
    ::memcpy(requestPayload.data() + 11U, statusJson.data(), statusJson.length());
    responsePayload.clear();

    FNETestHooks::dispatchMetadata(harness.traffic, harness.metadata,
        network::NET_FUNC::SUBSCRIBER_OP, network::NET_SUBFUNC::SUBSCRIBER_OP_P25_STATUS,
        CONSOLE_PEER_ID, requestPayload);

    REQUIRE(responsePayload.size() > 11U);
    CHECK(responseSubFunction == network::NET_SUBFUNC::SUBSCRIBER_OP_P25_STATUS);
    json::value statusResponseValue;
    REQUIRE(json::parse(statusResponseValue,
        std::string(reinterpret_cast<const char*>(responsePayload.data() + 11U),
            responsePayload.size() - 11U))
                .empty());
    REQUIRE(statusResponseValue.is<json::object>());
    json::object& statusResponse = statusResponseValue.get<json::object>();
    CHECK(statusResponse["found"].get<bool>());
    CHECK(statusResponse["subscriberId"].get<uint32_t>() == SUBSCRIBER_ID);
    CHECK(statusResponse["status"].get<uint16_t>() == 0x1234U);
    CHECK(statusResponse["solicited"].get<bool>());
}

TEST_CASE("FNE metadata starts subscriber operations before transmission",
    "[fne][metadata][subscriber-operations]")
{
    constexpr uint32_t CONSOLE_PEER_ID = 1001U;
    constexpr uint32_t RADIO_PEER_ID = 2001U;
    constexpr uint32_t SUBSCRIBER_ID = 123456U;
    const uint8_t dmrSlot = 1U;

    FNEHarness harness;
    network::FNEPeerConnection& console = FNETestHooks::addPeer(
        harness.traffic, CONSOLE_PEER_ID, network::NET_STAT_RUNNING, true);
    console.peerClass(network::PEER_CONN_CLASS_CONSOLE);
    FNETestHooks::addPeer(harness.traffic, RADIO_PEER_ID, network::NET_STAT_RUNNING, true);

    std::vector<uint8_t> responsePayload;
    uint8_t responseFunction = network::NET_FUNC::ILLEGAL;
    uint8_t responseSubFunction = network::NET_SUBFUNC::NOP;
    FNETestHooks::captureMetadataWrite(harness.metadata, responsePayload,
        responseFunction, responseSubFunction);
    std::vector<uint32_t> peerWrites;
    FNETestHooks::capturePeerWrites(harness.traffic, peerWrites);

    json::object request = json::object();
    request["protocol"].set<std::string>("p25");
    request["operation"].set<std::string>("radio-check");
    request["targetId"].set<uint32_t>(SUBSCRIBER_ID);
    request["peerId"].set<uint32_t>(RADIO_PEER_ID);
    const std::string requestJson = json::value(request).serialize();
    std::vector<uint8_t> requestPayload(11U + requestJson.length(), 0U);
    ::memcpy(requestPayload.data() + 11U, requestJson.data(), requestJson.length());

    FNETestHooks::dispatchMetadata(harness.traffic, harness.metadata,
        network::NET_FUNC::SUBSCRIBER_OP, network::NET_SUBFUNC::SUBSCRIBER_OP_START,
        CONSOLE_PEER_ID, requestPayload);

    REQUIRE(responsePayload.size() > 11U);
    CHECK(responseFunction == network::NET_FUNC::SUBSCRIBER_OP);
    CHECK(responseSubFunction == network::NET_SUBFUNC::SUBSCRIBER_OP_START);
    REQUIRE(peerWrites.size() == 1U);
    CHECK(peerWrites.front() == RADIO_PEER_ID);

    json::value responseValue;
    REQUIRE(json::parse(responseValue,
        std::string(reinterpret_cast<const char*>(responsePayload.data() + 11U),
            responsePayload.size() - 11U))
                .empty());
    REQUIRE(responseValue.is<json::object>());
    json::object& response = responseValue.get<json::object>();
    CHECK(response["found"].get<bool>());
    CHECK(response["state"].get<std::string>() == "pending");
    CHECK(response["targetId"].get<uint32_t>() == SUBSCRIBER_ID);
    CHECK(response["peerId"].get<uint32_t>() == RADIO_PEER_ID);

    const uint64_t operationId = response["operationId"].get<uint64_t>();
    network::SubscriberOperationStore::Operation operation;
    REQUIRE(harness.traffic.subscriberOperations().get(operationId, operation));
    CHECK(operation.kind == network::SubscriberOperationStore::Kind::RADIO_CHECK);
    CHECK(operation.state == network::SubscriberOperationStore::State::PENDING);

    request["protocol"].set<std::string>("dmr");
    request["slot"].set<uint8_t>(dmrSlot);
    const std::string dmrRequestJson = json::value(request).serialize();
    requestPayload.assign(11U + dmrRequestJson.length(), 0U);
    ::memcpy(requestPayload.data() + 11U, dmrRequestJson.data(), dmrRequestJson.length());
    responsePayload.clear();
    peerWrites.clear();

    FNETestHooks::dispatchMetadata(harness.traffic, harness.metadata,
        network::NET_FUNC::SUBSCRIBER_OP, network::NET_SUBFUNC::SUBSCRIBER_OP_START,
        CONSOLE_PEER_ID, requestPayload);

    REQUIRE(responsePayload.size() > 11U);
    REQUIRE(peerWrites.size() == 1U);
    CHECK(peerWrites.front() == RADIO_PEER_ID);
    REQUIRE(json::parse(responseValue,
        std::string(reinterpret_cast<const char*>(responsePayload.data() + 11U),
            responsePayload.size() - 11U))
                .empty());
    REQUIRE(responseValue.is<json::object>());
    json::object& dmrResponse = responseValue.get<json::object>();
    CHECK(dmrResponse["found"].get<bool>());
    const uint64_t dmrOperationId = dmrResponse["operationId"].get<uint64_t>();
    REQUIRE(harness.traffic.subscriberOperations().get(dmrOperationId, operation));
    CHECK(operation.protocol == network::SubscriberOperationStore::Protocol::DMR);
    CHECK(operation.kind == network::SubscriberOperationStore::Kind::RADIO_CHECK);
    CHECK(operation.state == network::SubscriberOperationStore::State::PENDING);
}

TEST_CASE("FNE consumes radio-check responses without peer fanout",
    "[fne][subscriber-operations][routing]")
{
    constexpr uint32_t SOURCE_PEER_ID = 1001U;
    constexpr uint32_t OTHER_PEER_ID = 1002U;
    constexpr uint32_t SUBSCRIBER_ID = 123456U;

    const std::vector<FNETestHooks::ParrotProtocol> protocols = {
        FNETestHooks::ParrotProtocol::DMR,
        FNETestHooks::ParrotProtocol::P25
    };

    for (FNETestHooks::ParrotProtocol protocol : protocols) {
        CAPTURE(static_cast<int>(protocol));
        FNEHarness harness;
        FNETestHooks::addPeer(harness.traffic, SOURCE_PEER_ID, network::NET_STAT_RUNNING, true);
        FNETestHooks::addPeer(harness.traffic, OTHER_PEER_ID, network::NET_STAT_RUNNING, true);

        const network::SubscriberOperationStore::Protocol storeProtocol =
            protocol == FNETestHooks::ParrotProtocol::DMR ? network::SubscriberOperationStore::Protocol::DMR : network::SubscriberOperationStore::Protocol::P25;
        const uint64_t operationId = harness.traffic.subscriberOperations().start(storeProtocol,
            network::SubscriberOperationStore::Kind::RADIO_CHECK, SUBSCRIBER_ID, SOURCE_PEER_ID);
        REQUIRE(operationId != 0U);

        std::vector<uint32_t> writes;
        FNETestHooks::capturePeerWrites(harness.traffic, writes);
        CHECK_FALSE(FNETestHooks::processRadioCheckResponse(
            harness.traffic, protocol, SOURCE_PEER_ID, SUBSCRIBER_ID));
        CHECK(writes.empty());

        network::SubscriberOperationStore::Operation operation;
        REQUIRE(harness.traffic.subscriberOperations().get(operationId, operation));
        CHECK(operation.state == network::SubscriberOperationStore::State::SUCCEEDED);
        CHECK(operation.peerId == SOURCE_PEER_ID);
    }
}

TEST_CASE("FNE unit route registry timestamps learned routes", "[fne][routing][u2u][registry]")
{
    network::UnitRouteRegistry registry(1000U);

    REQUIRE(registry.update(2001U, 1001U, 1001U, true, 100U));
    CHECK(registry.resolve(2001U, 1099U).valid);
    CHECK_FALSE(registry.resolve(2001U, 1100U).valid);

    const std::vector<network::UnitRoute> expired = registry.expire(1100U);
    REQUIRE(expired.size() == 1U);
    CHECK(expired[0U].unitId == 2001U);
    CHECK_FALSE(registry.resolve(2001U, 1100U).valid);
}

TEST_CASE("FNE explicit unit registrations do not expire", "[fne][routing][u2u][registry]")
{
    network::UnitRouteRegistry registry(1000U);

    REQUIRE(registry.update(2001U, 1001U, 1001U, false, 100U));
    CHECK(registry.expire(100000U).empty());
    const network::UnitRoute route = registry.resolve(2001U, 100000U);
    REQUIRE(route.valid);
    CHECK(route.peerId == 1001U);
    CHECK_FALSE(route.learned);
}

TEST_CASE("FNE unit route registry validates, refreshes, and moves learned routes",
    "[fne][routing][u2u][registry]")
{
    network::UnitRouteRegistry registry(1000U);

    CHECK_FALSE(registry.update(0U, 1001U, 1001U, true, 100U));
    CHECK_FALSE(registry.update(2001U, 0U, 1001U, true, 100U));
    REQUIRE(registry.update(2001U, 1001U, 0U, true, 100U));
    CHECK(registry.resolve(2001U, 100U).ssrc == 1001U);

    REQUIRE(registry.update(2001U, 1001U, 3001U, true, 900U));
    CHECK(registry.resolve(2001U, 1899U).valid);
    CHECK_FALSE(registry.resolve(2001U, 1900U).valid);

    REQUIRE(registry.update(2001U, 1002U, 3002U, true, 2000U));
    const network::UnitRoute moved = registry.resolve(2001U, 2000U);
    REQUIRE(moved.valid);
    CHECK(moved.peerId == 1002U);
    CHECK(moved.ssrc == 3002U);
}

TEST_CASE("FNE explicit unit routes resist traffic learning and support removal",
    "[fne][routing][u2u][registry]")
{
    network::UnitRouteRegistry registry(1000U);

    REQUIRE(registry.update(2001U, 1001U, 3001U, false, 100U));
    CHECK_FALSE(registry.update(2001U, 1002U, 3002U, true, 200U));
    REQUIRE(registry.update(2001U, 1001U, 3003U, true, 300U));

    const network::UnitRoute route = registry.resolve(2001U, 5000U);
    REQUIRE(route.valid);
    CHECK(route.peerId == 1001U);
    CHECK(route.ssrc == 3001U);
    CHECK_FALSE(route.learned);
    CHECK_FALSE(registry.erase(2001U, 1002U));
    CHECK(registry.erase(2001U, 1001U));
    CHECK_FALSE(registry.resolve(2001U, 5000U).valid);
}

TEST_CASE("FNE unit route snapshots replace only the selected peer",
    "[fne][routing][u2u][registry]")
{
    network::UnitRouteRegistry registry(1000U);
    REQUIRE(registry.update(2001U, 1001U, 1001U, false, 100U));
    REQUIRE(registry.update(2002U, 1001U, 1001U, false, 100U));
    REQUIRE(registry.update(2003U, 1002U, 1002U, false, 100U));

    registry.replacePeer(1001U, std::vector<uint32_t>{ 2002U, 2004U, 0U }, 3001U, 200U);
    CHECK_FALSE(registry.resolve(2001U, 200U).valid);
    CHECK(registry.resolve(2002U, 200U).peerId == 1001U);
    CHECK(registry.resolve(2003U, 200U).peerId == 1002U);
    CHECK(registry.resolve(2004U, 200U).ssrc == 3001U);

    std::vector<uint32_t> removed = registry.erasePeer(1001U);
    std::sort(removed.begin(), removed.end());
    CHECK(removed == std::vector<uint32_t>{ 2002U, 2004U });
    CHECK(registry.resolve(2003U, 200U).valid);
}

TEST_CASE("FNE orders each digital protocol stream without blocking independent calls", "[fne][ordering]")
{
    FNEHarness harness;
    FNETestHooks::startTrafficWorkerPool(harness.traffic);

    const std::vector<FNETestHooks::ParrotProtocol> protocols{
        FNETestHooks::ParrotProtocol::DMR,
        FNETestHooks::ParrotProtocol::P25,
        FNETestHooks::ParrotProtocol::P25P2,
        FNETestHooks::ParrotProtocol::NXDN
    };

    for (const FNETestHooks::ParrotProtocol protocol : protocols) {
        std::mutex mutex;
        std::condition_variable condition;
        bool firstStarted = false;
        bool releaseFirst = false;
        bool secondStartedEarly = false;
        bool independentCompleted = false;
        uint32_t completed = 0U;
        std::vector<uint32_t> streamOrder;

        FNETestHooks::setOrderedFrameHandler(harness.traffic, protocol,
            [&](const uint8_t*, uint32_t, uint32_t peerId, uint32_t, uint16_t,
                uint32_t streamId, bool) {
                std::unique_lock<std::mutex> lock(mutex);
                if (peerId == 1002U && streamId == 3002U) {
                    independentCompleted = true;
                    completed++;
                    condition.notify_all();
                    return;
                }

                if (streamOrder.empty()) {
                    firstStarted = true;
                    condition.notify_all();
                    condition.wait(lock, [&]() { return releaseFirst; });
                    streamOrder.push_back(1U);
                }
                else {
                    secondStartedEarly = !releaseFirst;
                    streamOrder.push_back(2U);
                }
                completed++;
                condition.notify_all();
            });

        const auto enqueueTime = std::chrono::steady_clock::now();
        const bool firstQueued = FNETestHooks::enqueueOrderedFrame(harness.traffic, protocol,
            1001U, 2001U, 3001U);

        bool firstRanPromptly = false;
        {
            std::unique_lock<std::mutex> lock(mutex);
            firstRanPromptly = condition.wait_for(lock, std::chrono::milliseconds(250),
                [&]() { return firstStarted; });
        }
        const auto firstDispatchDelay = std::chrono::steady_clock::now() - enqueueTime;

        const bool secondQueued = FNETestHooks::enqueueOrderedFrame(harness.traffic, protocol,
            1001U, 2001U, 3001U);
        const bool independentQueued = FNETestHooks::enqueueOrderedFrame(harness.traffic, protocol,
            1002U, 2002U, 3002U);

        bool independentRanWhileBlocked = false;
        {
            std::unique_lock<std::mutex> lock(mutex);
            independentRanWhileBlocked = condition.wait_for(lock, std::chrono::seconds(2),
                [&]() { return independentCompleted; });
            releaseFirst = true;
            condition.notify_all();
            condition.wait_for(lock, std::chrono::seconds(2), [&]() { return completed == 3U; });
        }

        FNETestHooks::clearOrderedFrameHandler(harness.traffic, protocol);

        CHECK(firstQueued);
        CHECK(secondQueued);
        CHECK(independentQueued);
        CHECK(firstRanPromptly);
        CHECK(firstDispatchDelay < std::chrono::milliseconds(250));
        CHECK(independentRanWhileBlocked);
        CHECK_FALSE(secondStartedEarly);
        CHECK(completed == 3U);
        CHECK(streamOrder == std::vector<uint32_t>{ 1U, 2U });
    }

    FNETestHooks::stopTrafficWorkerPool(harness.traffic);
}

TEST_CASE("FNE ordered protocol queues preserve burst FIFO and allow stream reuse", "[fne][ordering][fifo]")
{
    constexpr uint16_t BURST_LENGTH = 32U;
    FNEHarness harness;
    FNETestHooks::startTrafficWorkerPool(harness.traffic);

    const std::vector<FNETestHooks::ParrotProtocol> protocols{
        FNETestHooks::ParrotProtocol::DMR,
        FNETestHooks::ParrotProtocol::P25,
        FNETestHooks::ParrotProtocol::P25P2,
        FNETestHooks::ParrotProtocol::NXDN
    };

    for (const FNETestHooks::ParrotProtocol protocol : protocols) {
        std::mutex mutex;
        std::condition_variable condition;
        std::vector<uint16_t> sequences;

        FNETestHooks::setOrderedFrameHandler(harness.traffic, protocol,
            [&](const uint8_t*, uint32_t, uint32_t, uint32_t, uint16_t pktSeq,
                uint32_t, bool) {
                std::lock_guard<std::mutex> lock(mutex);
                sequences.push_back(pktSeq);
                condition.notify_all();
            });

        for (uint16_t sequence = 1U; sequence <= BURST_LENGTH; sequence++) {
            REQUIRE(FNETestHooks::enqueueOrderedFrame(harness.traffic, protocol,
                1101U, 2101U, 3101U, sequence));
        }

        bool burstCompleted = false;
        {
            std::unique_lock<std::mutex> lock(mutex);
            burstCompleted = condition.wait_for(lock, std::chrono::seconds(2),
                [&]() { return sequences.size() == BURST_LENGTH; });
        }

        REQUIRE(burstCompleted);
        std::vector<uint16_t> expected;
        for (uint16_t sequence = 1U; sequence <= BURST_LENGTH; sequence++)
            expected.push_back(sequence);
        CHECK(sequences == expected);

        // The first burst drains and removes the stream state. Reusing the exact
        // key must schedule a fresh task instead of leaving the stream stranded.
        REQUIRE(FNETestHooks::enqueueOrderedFrame(harness.traffic, protocol,
            1101U, 2101U, 3101U, BURST_LENGTH + 1U));

        bool reusedStreamCompleted = false;
        {
            std::unique_lock<std::mutex> lock(mutex);
            reusedStreamCompleted = condition.wait_for(lock, std::chrono::milliseconds(250),
                [&]() { return sequences.size() == BURST_LENGTH + 1U; });
        }

        CHECK(reusedStreamCompleted);
        REQUIRE(sequences.size() == BURST_LENGTH + 1U);
        CHECK(sequences.back() == BURST_LENGTH + 1U);
        FNETestHooks::clearOrderedFrameHandler(harness.traffic, protocol);
    }

    FNETestHooks::stopTrafficWorkerPool(harness.traffic);
}

TEST_CASE("FNE ordered protocol queues do not wait for or normalize packet sequences", "[fne][ordering][late-entry]")
{
    FNEHarness harness;
    FNETestHooks::startTrafficWorkerPool(harness.traffic);

    const std::vector<FNETestHooks::ParrotProtocol> protocols{
        FNETestHooks::ParrotProtocol::DMR,
        FNETestHooks::ParrotProtocol::P25,
        FNETestHooks::ParrotProtocol::P25P2,
        FNETestHooks::ParrotProtocol::NXDN
    };
    const std::vector<uint16_t> arrivals{ 25U, 23U, 23U, 24U };

    for (const FNETestHooks::ParrotProtocol protocol : protocols) {
        std::mutex mutex;
        std::condition_variable condition;
        std::vector<uint16_t> delivered;

        FNETestHooks::setOrderedFrameHandler(harness.traffic, protocol,
            [&](const uint8_t*, uint32_t, uint32_t, uint32_t, uint16_t pktSeq,
                uint32_t, bool) {
                std::lock_guard<std::mutex> lock(mutex);
                delivered.push_back(pktSeq);
                condition.notify_all();
            });

        for (const uint16_t sequence : arrivals) {
            REQUIRE(FNETestHooks::enqueueOrderedFrame(harness.traffic, protocol,
                1151U, 2151U, 3151U, sequence));
        }

        bool allDelivered = false;
        {
            std::unique_lock<std::mutex> lock(mutex);
            allDelivered = condition.wait_for(lock, std::chrono::milliseconds(250),
                [&]() { return delivered.size() == arrivals.size(); });
        }

        CHECK(allDelivered);
        CHECK(delivered == arrivals);
        FNETestHooks::clearOrderedFrameHandler(harness.traffic, protocol);
    }

    FNETestHooks::stopTrafficWorkerPool(harness.traffic);
}

TEST_CASE("FNE ordered protocol keys isolate every traffic origin dimension", "[fne][ordering][keys]")
{
    FNEHarness harness;
    FNETestHooks::startTrafficWorkerPool(harness.traffic);

    const std::vector<FNETestHooks::ParrotProtocol> protocols{
        FNETestHooks::ParrotProtocol::DMR,
        FNETestHooks::ParrotProtocol::P25,
        FNETestHooks::ParrotProtocol::P25P2,
        FNETestHooks::ParrotProtocol::NXDN
    };

    for (const FNETestHooks::ParrotProtocol protocol : protocols) {
        std::mutex mutex;
        std::condition_variable condition;
        bool blockedFrameStarted = false;
        bool releaseBlockedFrame = false;
        uint32_t independentCompleted = 0U;
        uint32_t totalCompleted = 0U;

        FNETestHooks::setOrderedFrameHandler(harness.traffic, protocol,
            [&](const uint8_t*, uint32_t, uint32_t, uint32_t, uint16_t pktSeq,
                uint32_t, bool) {
                std::unique_lock<std::mutex> lock(mutex);
                if (pktSeq == 1U) {
                    blockedFrameStarted = true;
                    condition.notify_all();
                    condition.wait(lock, [&]() { return releaseBlockedFrame; });
                }
                else {
                    independentCompleted++;
                }
                totalCompleted++;
                condition.notify_all();
            });

        const bool blockedQueued = FNETestHooks::enqueueOrderedFrame(harness.traffic, protocol,
            1201U, 2201U, 3201U, 1U, false);
        bool blockedRanPromptly = false;
        {
            std::unique_lock<std::mutex> lock(mutex);
            blockedRanPromptly = condition.wait_for(lock, std::chrono::milliseconds(250),
                [&]() { return blockedFrameStarted; });
        }

        // Each frame differs from the blocked key in exactly one field.
        const bool peerVariantQueued = FNETestHooks::enqueueOrderedFrame(harness.traffic, protocol,
            1202U, 2201U, 3201U, 2U, false);
        const bool ssrcVariantQueued = FNETestHooks::enqueueOrderedFrame(harness.traffic, protocol,
            1201U, 2202U, 3201U, 3U, false);
        const bool streamVariantQueued = FNETestHooks::enqueueOrderedFrame(harness.traffic, protocol,
            1201U, 2201U, 3202U, 4U, false);
        const bool directionVariantQueued = FNETestHooks::enqueueOrderedFrame(harness.traffic, protocol,
            1201U, 2201U, 3201U, 5U, true);

        bool keysRanIndependently = false;
        {
            std::unique_lock<std::mutex> lock(mutex);
            keysRanIndependently = condition.wait_for(lock, std::chrono::seconds(2),
                [&]() { return independentCompleted == 4U; });
            releaseBlockedFrame = true;
            condition.notify_all();
            condition.wait_for(lock, std::chrono::seconds(2),
                [&]() { return totalCompleted == 5U; });
        }

        CHECK(blockedQueued);
        CHECK(blockedRanPromptly);
        CHECK(peerVariantQueued);
        CHECK(ssrcVariantQueued);
        CHECK(streamVariantQueued);
        CHECK(directionVariantQueued);
        CHECK(keysRanIndependently);
        CHECK(totalCompleted == 5U);
        FNETestHooks::clearOrderedFrameHandler(harness.traffic, protocol);
    }

    FNETestHooks::stopTrafficWorkerPool(harness.traffic);
}

TEST_CASE("FNE protocol dispatch validates peers before ordered delivery", "[fne][ordering][dispatch]")
{
    constexpr uint32_t AUTHORIZED_PEER = 1301U;
    constexpr uint32_t UNAUTHORIZED_PEER = 1302U;
    FNEHarness harness;
    FNETestHooks::addPeer(harness.traffic, AUTHORIZED_PEER, network::NET_STAT_RUNNING, true);
    FNETestHooks::startTrafficWorkerPool(harness.traffic);

    struct ProtocolCase {
        FNETestHooks::ParrotProtocol protocol;
        network::NET_SUBFUNC::ENUM subFunction;
    };
    const std::vector<ProtocolCase> protocols{
        { FNETestHooks::ParrotProtocol::DMR, network::NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR },
        { FNETestHooks::ParrotProtocol::P25, network::NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 },
        { FNETestHooks::ParrotProtocol::P25P2, network::NET_SUBFUNC::PROTOCOL_SUBFUNC_P25_P2 },
        { FNETestHooks::ParrotProtocol::NXDN, network::NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN }
    };

    for (const ProtocolCase& protocolCase : protocols) {
        std::mutex mutex;
        std::condition_variable condition;
        uint32_t deliveries = 0U;
        uint32_t deliveredPeerId = 0U;

        FNETestHooks::setOrderedFrameHandler(harness.traffic, protocolCase.protocol,
            [&](const uint8_t*, uint32_t, uint32_t peerId, uint32_t, uint16_t,
                uint32_t, bool) {
                std::lock_guard<std::mutex> lock(mutex);
                deliveries++;
                deliveredPeerId = peerId;
                condition.notify_all();
            });

        const std::vector<uint8_t> packet{ 0U };
        FNETestHooks::dispatchTraffic(harness.traffic, network::NET_FUNC::PROTOCOL,
            protocolCase.subFunction, UNAUTHORIZED_PEER, packet);

        {
            std::unique_lock<std::mutex> lock(mutex);
            CHECK_FALSE(condition.wait_for(lock, std::chrono::milliseconds(50),
                [&]() { return deliveries != 0U; }));
        }

        FNETestHooks::dispatchTraffic(harness.traffic, network::NET_FUNC::PROTOCOL,
            protocolCase.subFunction, AUTHORIZED_PEER, packet);

        bool authorizedDelivered = false;
        {
            std::unique_lock<std::mutex> lock(mutex);
            authorizedDelivered = condition.wait_for(lock, std::chrono::milliseconds(250),
                [&]() { return deliveries == 1U; });
        }

        CHECK(authorizedDelivered);
        CHECK(deliveries == 1U);
        CHECK(deliveredPeerId == AUTHORIZED_PEER);
        FNETestHooks::clearOrderedFrameHandler(harness.traffic, protocolCase.protocol);
    }

    FNETestHooks::stopTrafficWorkerPool(harness.traffic);
}

TEST_CASE("FNE parrot playback honors affiliated-required talkgroups", "[fne][parrot][affiliations]")
{
    constexpr uint32_t PARROT_TG = 3100U;
    constexpr uint32_t OTHER_TG = 3200U;
    constexpr uint32_t AFFILIATED_PEER = 1001U;
    constexpr uint32_t UNAFFILIATED_PEER = 1002U;
    constexpr uint32_t OTHER_TG_PEER = 1003U;

    FNEHarness harness;
    harness.talkgroups.addEntry(PARROT_TG, 0U, true, true);

    FNETestHooks::addPeer(harness.traffic, AFFILIATED_PEER, network::NET_STAT_RUNNING, true);
    FNETestHooks::addPeer(harness.traffic, UNAFFILIATED_PEER, network::NET_STAT_RUNNING, true);
    FNETestHooks::addPeer(harness.traffic, OTHER_TG_PEER, network::NET_STAT_RUNNING, true);
    FNETestHooks::addGroupAffiliation(harness.traffic, AFFILIATED_PEER, 2001U, PARROT_TG);
    FNETestHooks::addGroupAffiliation(harness.traffic, OTHER_TG_PEER, 2003U, OTHER_TG);

    const std::vector<FNETestHooks::ParrotProtocol> protocols = {
        FNETestHooks::ParrotProtocol::DMR,
        FNETestHooks::ParrotProtocol::P25,
        FNETestHooks::ParrotProtocol::P25P2,
        FNETestHooks::ParrotProtocol::NXDN,
        FNETestHooks::ParrotProtocol::ANALOG
    };

    for (FNETestHooks::ParrotProtocol protocol : protocols) {
        CAPTURE(static_cast<int>(protocol));
        std::vector<uint32_t> destinations;
        FNETestHooks::capturePeerWrites(harness.traffic, destinations);

        FNETestHooks::playbackParrotFrame(harness.traffic, protocol,
            UNAFFILIATED_PEER, 2002U, PARROT_TG);

        REQUIRE(destinations == std::vector<uint32_t>{ AFFILIATED_PEER });
    }
}

TEST_CASE("FNE call handlers enforce group peer-routing precedence", "[fne][callhandler][routing]")
{
    constexpr uint32_t TALK_GROUP = 3100U;
    constexpr uint32_t INCLUDED_PEER = 1001U;
    constexpr uint32_t OTHER_PEER = 1002U;
    constexpr uint32_t REPLICA_PEER = 1003U;
    constexpr uint32_t SYSVIEW_PEER = 1004U;

    FNEHarness harness;
    harness.talkgroups.addEntry(TALK_GROUP, 1U, true, true);
    lookups::TalkgroupRuleGroupVoice talkgroup = harness.talkgroups.find(TALK_GROUP, 1U);
    lookups::TalkgroupRuleConfig config = talkgroup.config();
    config.inclusion({ INCLUDED_PEER });
    config.exclusion({ INCLUDED_PEER });
    talkgroup.config(config);
    harness.talkgroups.addEntry(talkgroup);

    FNETestHooks::addPeer(harness.traffic, INCLUDED_PEER, network::NET_STAT_RUNNING, true);
    FNETestHooks::addGroupAffiliation(harness.traffic, INCLUDED_PEER, 2001U, TALK_GROUP);
    FNETestHooks::addPeer(harness.traffic, OTHER_PEER, network::NET_STAT_RUNNING, true);
    network::FNEPeerConnection& replica = FNETestHooks::addPeer(
        harness.traffic, REPLICA_PEER, network::NET_STAT_RUNNING, true);
    replica.isReplica(true);
    network::FNEPeerConnection& sysview = FNETestHooks::addPeer(
        harness.traffic, SYSVIEW_PEER, network::NET_STAT_RUNNING, true);
    sysview.peerClass(network::PEER_CONN_CLASS_SYSVIEW);

    const std::vector<FNETestHooks::ParrotProtocol> protocols = {
        FNETestHooks::ParrotProtocol::DMR,
        FNETestHooks::ParrotProtocol::P25,
        FNETestHooks::ParrotProtocol::P25P2,
        FNETestHooks::ParrotProtocol::NXDN,
        FNETestHooks::ParrotProtocol::ANALOG
    };

    for (FNETestHooks::ParrotProtocol protocol : protocols) {
        CAPTURE(static_cast<int>(protocol));
        CHECK(FNETestHooks::isPeerPermitted(
            harness.traffic, protocol, INCLUDED_PEER, TALK_GROUP));
        CHECK_FALSE(FNETestHooks::isPeerPermitted(
            harness.traffic, protocol, OTHER_PEER, TALK_GROUP));
        CHECK(FNETestHooks::isPeerPermitted(
            harness.traffic, protocol, REPLICA_PEER, TALK_GROUP));
        CHECK(FNETestHooks::isPeerPermitted(
            harness.traffic, protocol, SYSVIEW_PEER, TALK_GROUP));
    }
}

TEST_CASE("FNE call handlers apply affiliation routing exceptions", "[fne][callhandler][routing]")
{
    constexpr uint32_t TALK_GROUP = 3100U;
    constexpr uint32_t NORMAL_PEER = 1001U;
    constexpr uint32_t CONSOLE_PEER = 1002U;
    constexpr uint32_t CONVENTIONAL_PEER = 1003U;
    constexpr uint32_t VOICE_CHANNEL_PEER = 1004U;
    constexpr uint32_t CONTROL_CHANNEL_PEER = 2004U;

    FNEHarness harness;
    harness.talkgroups.addEntry(TALK_GROUP, 1U, true, true);
    FNETestHooks::addPeer(harness.traffic, NORMAL_PEER, network::NET_STAT_RUNNING, true);
    network::FNEPeerConnection& console = FNETestHooks::addPeer(
        harness.traffic, CONSOLE_PEER, network::NET_STAT_RUNNING, true);
    console.peerClass(network::PEER_CONN_CLASS_CONSOLE);
    network::FNEPeerConnection& conventional = FNETestHooks::addPeer(
        harness.traffic, CONVENTIONAL_PEER, network::NET_STAT_RUNNING, true);
    conventional.peerClass(network::PEER_CONN_CLASS_STANDARD);
    conventional.isConventional(true);
    network::FNEPeerConnection& voiceChannel = FNETestHooks::addPeer(
        harness.traffic, VOICE_CHANNEL_PEER, network::NET_STAT_RUNNING, true);
    voiceChannel.ccPeerId(CONTROL_CHANNEL_PEER);
    FNETestHooks::addGroupAffiliation(
        harness.traffic, CONTROL_CHANNEL_PEER, 2001U, TALK_GROUP);
    FNETestHooks::allowConventionalAffiliationOverride(harness.traffic, true);

    const std::vector<FNETestHooks::ParrotProtocol> protocols = {
        FNETestHooks::ParrotProtocol::DMR,
        FNETestHooks::ParrotProtocol::P25,
        FNETestHooks::ParrotProtocol::P25P2,
        FNETestHooks::ParrotProtocol::NXDN,
        FNETestHooks::ParrotProtocol::ANALOG
    };

    for (FNETestHooks::ParrotProtocol protocol : protocols) {
        CAPTURE(static_cast<int>(protocol));
        CHECK_FALSE(FNETestHooks::isPeerPermitted(
            harness.traffic, protocol, NORMAL_PEER, TALK_GROUP));
        CHECK(FNETestHooks::isPeerPermitted(
            harness.traffic, protocol, NORMAL_PEER, TALK_GROUP, true, true));
        CHECK(FNETestHooks::isPeerPermitted(
            harness.traffic, protocol, CONSOLE_PEER, TALK_GROUP));
        CHECK(FNETestHooks::isPeerPermitted(
            harness.traffic, protocol, CONVENTIONAL_PEER, TALK_GROUP));
        CHECK(FNETestHooks::isPeerPermitted(
            harness.traffic, protocol, VOICE_CHANNEL_PEER, TALK_GROUP));
    }
}

TEST_CASE("FNE call handlers reject globally disabled unit-to-unit traffic", "[fne][callhandler][routing][u2u]")
{
    constexpr uint32_t PEER_ID = 1001U;
    constexpr uint32_t DESTINATION_ID = 2001U;

    FNEHarness harness;
    FNETestHooks::addPeer(harness.traffic, PEER_ID, network::NET_STAT_RUNNING, true);
    FNETestHooks::disallowU2U(harness.traffic, true);

    const std::vector<FNETestHooks::ParrotProtocol> protocols = {
        FNETestHooks::ParrotProtocol::DMR,
        FNETestHooks::ParrotProtocol::P25,
        FNETestHooks::ParrotProtocol::P25P2,
        FNETestHooks::ParrotProtocol::NXDN,
        FNETestHooks::ParrotProtocol::ANALOG
    };

    for (FNETestHooks::ParrotProtocol protocol : protocols) {
        CAPTURE(static_cast<int>(protocol));
        CHECK_FALSE(FNETestHooks::isPeerPermitted(
            harness.traffic, protocol, PEER_ID, DESTINATION_ID, false));
    }
}

TEST_CASE("FNE call handlers enforce per-peer unit-to-unit authorization", "[fne][callhandler][routing][u2u]")
{
    constexpr uint32_t SOURCE_PEER = 1001U;
    constexpr uint32_t TARGET_PEER = 1002U;
    constexpr uint32_t DESTINATION_ID = 2001U;

    FNEHarness harness;
    lookups::PeerListLookup peers("", 0U, true, false);
    lookups::PeerId sourceEntry(SOURCE_PEER, "source", "", false);
    sourceEntry.allowUnitToUnit(false);
    peers.addEntry(SOURCE_PEER, sourceEntry);
    harness.traffic.setLookups(&harness.radios, nullptr, &harness.talkgroups, &peers, nullptr, nullptr);

    FNETestHooks::addPeer(harness.traffic, SOURCE_PEER, network::NET_STAT_RUNNING, true);
    FNETestHooks::addPeer(harness.traffic, TARGET_PEER, network::NET_STAT_RUNNING, true);
    FNETestHooks::registerP25Unit(harness.traffic, TARGET_PEER, DESTINATION_ID);

    const std::vector<FNETestHooks::ParrotProtocol> protocols = {
        FNETestHooks::ParrotProtocol::DMR,
        FNETestHooks::ParrotProtocol::P25,
        FNETestHooks::ParrotProtocol::P25P2,
        FNETestHooks::ParrotProtocol::NXDN
    };

    for (FNETestHooks::ParrotProtocol protocol : protocols) {
        CAPTURE(static_cast<int>(protocol));
        CHECK_FALSE(FNETestHooks::isPeerPermitted(
            harness.traffic, protocol, SOURCE_PEER, DESTINATION_ID, false));
    }
}

TEST_CASE("FNE private-call unknown destinations reject or flood by policy",
    "[fne][routing][u2u][policy]")
{
    FNEHarness harness;

    FNETestHooks::configurePrivateCallRouting(harness.traffic, true, "reject", true, 300000U);
    CHECK_FALSE(FNETestHooks::isUnitRouteAvailable(harness.traffic, 2001U));
    CHECK_FALSE(FNETestHooks::isUnitRouteAvailable(harness.traffic, 0U));

    FNETestHooks::configurePrivateCallRouting(harness.traffic, true, "flood", true, 300000U);
    CHECK(FNETestHooks::isUnitRouteAvailable(harness.traffic, 2001U));
    CHECK_FALSE(FNETestHooks::isUnitRouteAvailable(harness.traffic, 0U));

    FNETestHooks::configurePrivateCallRouting(harness.traffic, false, "flood", true, 300000U);
    CHECK_FALSE(FNETestHooks::isUnitRouteAvailable(harness.traffic, 2001U));
}

TEST_CASE("FNE private-call traffic learning honors policy and peer authorization",
    "[fne][routing][u2u][learning]")
{
    constexpr uint32_t ALLOWED_PEER = 1001U;
    constexpr uint32_t DENIED_PEER = 1002U;
    constexpr uint32_t UNIT_ID = 2001U;

    FNEHarness harness;
    lookups::PeerListLookup peers("", 0U, true, false);
    lookups::PeerId allowed(ALLOWED_PEER, "allowed", "", false);
    allowed.allowUnitToUnit(true);
    peers.addEntry(ALLOWED_PEER, allowed);
    lookups::PeerId denied(DENIED_PEER, "denied", "", false);
    denied.allowUnitToUnit(false);
    peers.addEntry(DENIED_PEER, denied);
    harness.traffic.setLookups(&harness.radios, nullptr, &harness.talkgroups, &peers, nullptr, nullptr);
    FNETestHooks::registerP25Unit(harness.traffic, ALLOWED_PEER, 2999U);
    FNETestHooks::registerP25Unit(harness.traffic, DENIED_PEER, 2998U);

    FNETestHooks::configurePrivateCallRouting(harness.traffic, true, "reject", false, 300000U);
    FNETestHooks::learnUnitRoute(harness.traffic, ALLOWED_PEER, UNIT_ID, 3001U);
    CHECK(FNETestHooks::findUnitRoute(harness.traffic, UNIT_ID) == 0U);

    FNETestHooks::configurePrivateCallRouting(harness.traffic, true, "reject", true, 300000U);
    FNETestHooks::learnUnitRoute(harness.traffic, DENIED_PEER, UNIT_ID, 3002U);
    CHECK(FNETestHooks::findUnitRoute(harness.traffic, UNIT_ID) == 0U);

    FNETestHooks::learnUnitRoute(harness.traffic, ALLOWED_PEER, UNIT_ID, 3001U);
    CHECK(FNETestHooks::findUnitRoute(harness.traffic, UNIT_ID) == ALLOWED_PEER);
    CHECK(FNETestHooks::isUnitRouteAvailable(harness.traffic, UNIT_ID));
}

TEST_CASE("FNE private-call registration snapshots and peer teardown clean routes",
    "[fne][routing][u2u][lifecycle]")
{
    constexpr uint32_t PEER_ID = 1001U;

    FNEHarness harness;
    FNETestHooks::addPeer(harness.traffic, PEER_ID, network::NET_STAT_RUNNING, true);
    FNETestHooks::registerP25Unit(harness.traffic, PEER_ID, 2001U);
    REQUIRE(FNETestHooks::replaceUnitRegistrations(
        harness.traffic, PEER_ID, std::vector<uint32_t>{ 2002U, 2003U }, 3001U));
    CHECK(FNETestHooks::findUnitRoute(harness.traffic, 2001U) == 0U);
    CHECK(FNETestHooks::findUnitRoute(harness.traffic, 2002U) == PEER_ID);
    CHECK(FNETestHooks::findUnitRoute(harness.traffic, 2003U) == PEER_ID);

    FNETestHooks::teardownPeer(harness.traffic, PEER_ID);
    CHECK(FNETestHooks::findUnitRoute(harness.traffic, 2002U) == 0U);
    CHECK(FNETestHooks::findUnitRoute(harness.traffic, 2003U) == 0U);
}

TEST_CASE("FNE private-call grants target only the registered next hop",
    "[fne][callhandler][routing][u2u][grants]")
{
    constexpr uint32_t SOURCE_PEER = 1001U;
    constexpr uint32_t TARGET_PEER = 1002U;
    constexpr uint32_t OTHER_PEER = 1003U;
    constexpr uint32_t SOURCE_ID = 2001U;
    constexpr uint32_t DESTINATION_ID = 2002U;

    const std::vector<FNETestHooks::ParrotProtocol> protocols = {
        FNETestHooks::ParrotProtocol::DMR,
        FNETestHooks::ParrotProtocol::P25,
        FNETestHooks::ParrotProtocol::NXDN
    };

    for (FNETestHooks::ParrotProtocol protocol : protocols) {
        CAPTURE(static_cast<int>(protocol));
        FNEHarness harness;
        FNETestHooks::addPeer(harness.traffic, SOURCE_PEER, network::NET_STAT_RUNNING, true);
        FNETestHooks::addPeer(harness.traffic, TARGET_PEER, network::NET_STAT_RUNNING, true);
        FNETestHooks::addPeer(harness.traffic, OTHER_PEER, network::NET_STAT_RUNNING, true);
        harness.radios.addEntry(SOURCE_ID, true, "test source");
        harness.radios.addEntry(DESTINATION_ID, true, "test destination");
        FNETestHooks::registerP25Unit(harness.traffic, TARGET_PEER, DESTINATION_ID);

        std::vector<uint32_t> writes;
        FNETestHooks::capturePeerWrites(harness.traffic, writes);
        bool accepted = false;
        if (protocol == FNETestHooks::ParrotProtocol::DMR) {
            accepted = FNETestHooks::processDMRGrantRequest(
                harness.traffic, SOURCE_PEER, SOURCE_ID, DESTINATION_ID, true);
        }
        else if (protocol == FNETestHooks::ParrotProtocol::P25) {
            accepted = FNETestHooks::processP25GrantRequest(
                harness.traffic, SOURCE_PEER, SOURCE_ID, DESTINATION_ID, true);
        }
        else {
            accepted = FNETestHooks::processNXDNGrantRequest(
                harness.traffic, SOURCE_PEER, SOURCE_ID, DESTINATION_ID, true);
        }
        REQUIRE(accepted);
        std::sort(writes.begin(), writes.end());
        CHECK(writes == std::vector<uint32_t>{ SOURCE_PEER, TARGET_PEER });
    }
}

TEST_CASE("FNE private-call grants support retries and reverse talker direction",
    "[fne][callhandler][routing][u2u][grants][continuation]")
{
    constexpr uint32_t SOURCE_PEER = 1001U;
    constexpr uint32_t TARGET_PEER = 1002U;
    constexpr uint32_t SOURCE_ID = 2001U;
    constexpr uint32_t DESTINATION_ID = 2002U;

    const std::vector<FNETestHooks::ParrotProtocol> protocols = {
        FNETestHooks::ParrotProtocol::DMR,
        FNETestHooks::ParrotProtocol::P25,
        FNETestHooks::ParrotProtocol::NXDN
    };

    for (FNETestHooks::ParrotProtocol protocol : protocols) {
        CAPTURE(static_cast<int>(protocol));
        FNEHarness harness;
        FNETestHooks::addPeer(harness.traffic, SOURCE_PEER, network::NET_STAT_RUNNING, true);
        FNETestHooks::addPeer(harness.traffic, TARGET_PEER, network::NET_STAT_RUNNING, true);
        harness.radios.addEntry(SOURCE_ID, true, "test source");
        harness.radios.addEntry(DESTINATION_ID, true, "test destination");
        FNETestHooks::registerP25Unit(harness.traffic, SOURCE_PEER, SOURCE_ID);
        FNETestHooks::registerP25Unit(harness.traffic, TARGET_PEER, DESTINATION_ID);

        auto request = [&](uint32_t peerId, uint32_t sourceId, uint32_t destinationId) {
            if (protocol == FNETestHooks::ParrotProtocol::DMR) {
                return FNETestHooks::processDMRGrantRequest(
                    harness.traffic, peerId, sourceId, destinationId, true);
            }
            if (protocol == FNETestHooks::ParrotProtocol::P25) {
                return FNETestHooks::processP25GrantRequest(
                    harness.traffic, peerId, sourceId, destinationId, true);
            }
            return FNETestHooks::processNXDNGrantRequest(
                harness.traffic, peerId, sourceId, destinationId, true);
        };

        std::vector<uint32_t> writes;
        FNETestHooks::capturePeerWrites(harness.traffic, writes);
        REQUIRE(request(SOURCE_PEER, SOURCE_ID, DESTINATION_ID));

        writes.clear();
        REQUIRE(request(SOURCE_PEER, SOURCE_ID, DESTINATION_ID));
        std::sort(writes.begin(), writes.end());
        CHECK(writes == std::vector<uint32_t>{ SOURCE_PEER, TARGET_PEER });

        writes.clear();
        REQUIRE(request(TARGET_PEER, DESTINATION_ID, SOURCE_ID));
        std::sort(writes.begin(), writes.end());
        CHECK(writes == std::vector<uint32_t>{ SOURCE_PEER, TARGET_PEER });
    }
}

TEST_CASE("FNE private-call flood reaches every authorized transit peer",
    "[fne][callhandler][routing][u2u][flood]")
{
    constexpr uint32_t SOURCE_PEER = 1001U;
    constexpr uint32_t ALLOWED_PEER_A = 1002U;
    constexpr uint32_t ALLOWED_PEER_B = 1003U;
    constexpr uint32_t DENIED_PEER = 1004U;
    constexpr uint32_t SOURCE_ID = 2001U;
    constexpr uint32_t DESTINATION_ID = 2002U;

    const std::vector<FNETestHooks::ParrotProtocol> protocols = {
        FNETestHooks::ParrotProtocol::DMR,
        FNETestHooks::ParrotProtocol::P25,
        FNETestHooks::ParrotProtocol::NXDN
    };

    for (FNETestHooks::ParrotProtocol protocol : protocols) {
        CAPTURE(static_cast<int>(protocol));
        FNEHarness harness;
        lookups::PeerListLookup peers("", 0U, true, false);
        for (uint32_t peerId : { SOURCE_PEER, ALLOWED_PEER_A, ALLOWED_PEER_B }) {
            lookups::PeerId peer(peerId, "allowed", "", false);
            peer.allowUnitToUnit(true);
            peers.addEntry(peerId, peer);
        }
        lookups::PeerId denied(DENIED_PEER, "denied", "", false);
        denied.allowUnitToUnit(false);
        peers.addEntry(DENIED_PEER, denied);
        harness.traffic.setLookups(&harness.radios, nullptr, &harness.talkgroups, &peers, nullptr, nullptr);
        FNETestHooks::configurePrivateCallRouting(harness.traffic, true, "flood", true, 300000U);

        FNETestHooks::addPeer(harness.traffic, SOURCE_PEER, network::NET_STAT_RUNNING, true);
        FNETestHooks::addPeer(harness.traffic, ALLOWED_PEER_A, network::NET_STAT_RUNNING, true);
        FNETestHooks::addPeer(harness.traffic, ALLOWED_PEER_B, network::NET_STAT_RUNNING, true);
        FNETestHooks::addPeer(harness.traffic, DENIED_PEER, network::NET_STAT_RUNNING, true);
        FNETestHooks::registerP25Unit(harness.traffic, ALLOWED_PEER_A, 2901U);
        FNETestHooks::registerP25Unit(harness.traffic, ALLOWED_PEER_B, 2902U);
        harness.radios.addEntry(SOURCE_ID, true, "test source");
        harness.radios.addEntry(DESTINATION_ID, true, "test destination");

        std::vector<uint32_t> writes;
        FNETestHooks::capturePeerWrites(harness.traffic, writes);
        bool accepted = false;
        if (protocol == FNETestHooks::ParrotProtocol::DMR) {
            accepted = FNETestHooks::processDMRGrantRequest(
                harness.traffic, SOURCE_PEER, SOURCE_ID, DESTINATION_ID, true);
        }
        else if (protocol == FNETestHooks::ParrotProtocol::P25) {
            accepted = FNETestHooks::processP25GrantRequest(
                harness.traffic, SOURCE_PEER, SOURCE_ID, DESTINATION_ID, true);
        }
        else {
            accepted = FNETestHooks::processNXDNGrantRequest(
                harness.traffic, SOURCE_PEER, SOURCE_ID, DESTINATION_ID, true);
        }

        REQUIRE(accepted);
        std::sort(writes.begin(), writes.end());
        CHECK(writes == std::vector<uint32_t>{ SOURCE_PEER, ALLOWED_PEER_A, ALLOWED_PEER_B });
    }
}

TEST_CASE("FNE P25 Phase 2 private voice honors known routes and flood policy",
    "[fne][callhandler][routing][u2u][flood][p25p2]")
{
    constexpr uint32_t SOURCE_PEER = 1001U;
    constexpr uint32_t TARGET_PEER = 1002U;
    constexpr uint32_t OTHER_PEER = 1003U;
    constexpr uint32_t SOURCE_ID = 2001U;
    constexpr uint32_t DESTINATION_ID = 2002U;

    SECTION("known destination selects only its registered peer") {
        FNEHarness harness;
        FNETestHooks::addPeer(harness.traffic, SOURCE_PEER, network::NET_STAT_RUNNING, true);
        FNETestHooks::addPeer(harness.traffic, TARGET_PEER, network::NET_STAT_RUNNING, true);
        FNETestHooks::addPeer(harness.traffic, OTHER_PEER, network::NET_STAT_RUNNING, true);
        harness.radios.addEntry(SOURCE_ID, true, "test source");
        harness.radios.addEntry(DESTINATION_ID, true, "test destination");
        FNETestHooks::registerP25Unit(harness.traffic, TARGET_PEER, DESTINATION_ID);

        std::vector<uint32_t> writes;
        FNETestHooks::capturePeerWrites(harness.traffic, writes);
        REQUIRE(FNETestHooks::processP25P2PrivateFrame(
            harness.traffic, SOURCE_PEER, SOURCE_ID, DESTINATION_ID, 201U));
        CHECK(writes == std::vector<uint32_t>{ TARGET_PEER });
    }

    SECTION("unknown destination floods to authorized peers") {
        FNEHarness harness;
        FNETestHooks::configurePrivateCallRouting(harness.traffic, true, "flood", true, 300000U);
        FNETestHooks::addPeer(harness.traffic, SOURCE_PEER, network::NET_STAT_RUNNING, true);
        FNETestHooks::addPeer(harness.traffic, TARGET_PEER, network::NET_STAT_RUNNING, true);
        FNETestHooks::addPeer(harness.traffic, OTHER_PEER, network::NET_STAT_RUNNING, true);
        harness.radios.addEntry(SOURCE_ID, true, "test source");
        harness.radios.addEntry(DESTINATION_ID, true, "test destination");

        std::vector<uint32_t> writes;
        FNETestHooks::capturePeerWrites(harness.traffic, writes);
        REQUIRE(FNETestHooks::processP25P2PrivateFrame(
            harness.traffic, SOURCE_PEER, SOURCE_ID, DESTINATION_ID, 202U));
        std::sort(writes.begin(), writes.end());
        CHECK(writes == std::vector<uint32_t>{ TARGET_PEER, OTHER_PEER });
    }
}

TEST_CASE("FNE private-call routing denies an unauthorized destination peer",
    "[fne][callhandler][routing][u2u][egress]")
{
    constexpr uint32_t SOURCE_PEER = 1001U;
    constexpr uint32_t TARGET_PEER = 1002U;
    constexpr uint32_t SOURCE_ID = 2001U;
    constexpr uint32_t DESTINATION_ID = 2002U;

    FNEHarness harness;
    lookups::PeerListLookup peers("", 0U, true, false);
    lookups::PeerId source(SOURCE_PEER, "source", "", false);
    source.allowUnitToUnit(true);
    peers.addEntry(SOURCE_PEER, source);
    lookups::PeerId target(TARGET_PEER, "target", "", false);
    target.allowUnitToUnit(false);
    peers.addEntry(TARGET_PEER, target);
    harness.traffic.setLookups(&harness.radios, nullptr, &harness.talkgroups, &peers, nullptr, nullptr);
    FNETestHooks::addPeer(harness.traffic, SOURCE_PEER, network::NET_STAT_RUNNING, true);
    FNETestHooks::addPeer(harness.traffic, TARGET_PEER, network::NET_STAT_RUNNING, true);
    harness.radios.addEntry(SOURCE_ID, true, "test source");
    harness.radios.addEntry(DESTINATION_ID, true, "test destination");
    FNETestHooks::registerP25Unit(harness.traffic, TARGET_PEER, DESTINATION_ID);

    CHECK_FALSE(FNETestHooks::isUnitRouteAvailable(harness.traffic, DESTINATION_ID));
    CHECK_FALSE(FNETestHooks::processDMRGrantRequest(
        harness.traffic, SOURCE_PEER, SOURCE_ID, DESTINATION_ID, true));
    CHECK_FALSE(FNETestHooks::processP25GrantRequest(
        harness.traffic, SOURCE_PEER, SOURCE_ID, DESTINATION_ID, true));
    CHECK_FALSE(FNETestHooks::processNXDNGrantRequest(
        harness.traffic, SOURCE_PEER, SOURCE_ID, DESTINATION_ID, true));
}

TEST_CASE("FNE P25 uses the explicit U2U flag for late-entry voice", "[fne][callhandler][routing][u2u]")
{
    constexpr uint32_t PEER_ID = 1001U;
    constexpr uint32_t SOURCE_ID = 5519007U;
    constexpr uint32_t DESTINATION_ID = 5519001U;

    FNEHarness harness;
    FNETestHooks::addPeer(harness.traffic, PEER_ID, network::NET_STAT_RUNNING, true);
    harness.radios.addEntry(SOURCE_ID, true, "test source");
    harness.radios.addEntry(DESTINATION_ID, true, "test destination");
    FNETestHooks::registerP25Unit(harness.traffic, PEER_ID, DESTINATION_ID);

    CHECK(FNETestHooks::validateP25IndividualCall(
        harness.traffic, PEER_ID, SOURCE_ID, DESTINATION_ID));

    FNETestHooks::disallowU2U(harness.traffic, true);
    CHECK_FALSE(FNETestHooks::validateP25IndividualCall(
        harness.traffic, PEER_ID, SOURCE_ID, DESTINATION_ID));
}

TEST_CASE("FNE DMR individual grant demand targets the called unit site",
    "[fne][callhandler][routing][u2u][dmr]")
{
    constexpr uint32_t SOURCE_PEER = 1001U;
    constexpr uint32_t SOURCE_SITE = 1002U;
    constexpr uint32_t TARGET_SITE = 1003U;
    constexpr uint32_t OTHER_SITE = 1004U;
    constexpr uint32_t SOURCE_ID = 2001U;
    constexpr uint32_t DESTINATION_ID = 2002U;

    FNEHarness harness;
    FNETestHooks::addPeer(harness.traffic, SOURCE_PEER, network::NET_STAT_RUNNING, true);
    FNETestHooks::addPeer(harness.traffic, SOURCE_SITE, network::NET_STAT_RUNNING, true);
    FNETestHooks::addPeer(harness.traffic, TARGET_SITE, network::NET_STAT_RUNNING, true);
    FNETestHooks::addPeer(harness.traffic, OTHER_SITE, network::NET_STAT_RUNNING, true);
    harness.radios.addEntry(SOURCE_ID, true, "test source");
    harness.radios.addEntry(DESTINATION_ID, true, "test destination");
    FNETestHooks::registerP25Unit(harness.traffic, SOURCE_SITE, SOURCE_ID);
    FNETestHooks::registerP25Unit(harness.traffic, TARGET_SITE, DESTINATION_ID);

    std::vector<uint32_t> writes;
    FNETestHooks::capturePeerWrites(harness.traffic, writes);
    REQUIRE(FNETestHooks::processDMRGrantRequest(
        harness.traffic, SOURCE_PEER, SOURCE_ID, DESTINATION_ID, true));
    std::sort(writes.begin(), writes.end());
    CHECK(writes == std::vector<uint32_t>{ SOURCE_PEER, TARGET_SITE });
}

TEST_CASE("FNE grant release requires the originating peer and matching call",
    "[fne][callhandler][grant][release]")
{
    constexpr uint32_t OWNER_PEER = 1001U;
    constexpr uint32_t OTHER_PEER = 1002U;
    constexpr uint32_t SOURCE_ID = 2001U;
    constexpr uint32_t TALK_GROUP = 3100U;

    FNEHarness harness;
    FNETestHooks::addPeer(harness.traffic, OWNER_PEER, network::NET_STAT_RUNNING, true);
    FNETestHooks::addPeer(harness.traffic, OTHER_PEER, network::NET_STAT_RUNNING, true);
    harness.radios.addEntry(SOURCE_ID, true, "test source");
    harness.talkgroups.addEntry(TALK_GROUP, 1U, true, false);

    REQUIRE(FNETestHooks::processP25GrantRequest(
        harness.traffic, OWNER_PEER, SOURCE_ID, TALK_GROUP, false));
    CHECK_FALSE(FNETestHooks::processGrantRelease(
        harness.traffic, OTHER_PEER, SOURCE_ID, TALK_GROUP, false));
    CHECK_FALSE(FNETestHooks::processGrantRelease(
        harness.traffic, OWNER_PEER, SOURCE_ID + 1U, TALK_GROUP, false));
    CHECK_FALSE(FNETestHooks::processGrantRelease(
        harness.traffic, OWNER_PEER, SOURCE_ID, TALK_GROUP, true));
    CHECK(FNETestHooks::processGrantRelease(
        harness.traffic, OWNER_PEER, SOURCE_ID, TALK_GROUP, false));
    CHECK_FALSE(FNETestHooks::processGrantRelease(
        harness.traffic, OWNER_PEER, SOURCE_ID, TALK_GROUP, false));
}

TEST_CASE("FNE grant release resolves reverse U2U direction",
    "[fne][callhandler][grant][release][u2u]")
{
    constexpr uint32_t OWNER_PEER = 1001U;
    constexpr uint32_t SOURCE_ID = 2001U;
    constexpr uint32_t DESTINATION_ID = 2002U;

    FNEHarness harness;
    FNETestHooks::addPeer(harness.traffic, OWNER_PEER, network::NET_STAT_RUNNING, true);
    harness.radios.addEntry(SOURCE_ID, true, "test source");
    harness.radios.addEntry(DESTINATION_ID, true, "test destination");
    FNETestHooks::registerP25Unit(harness.traffic, OWNER_PEER, DESTINATION_ID);

    REQUIRE(FNETestHooks::processP25GrantRequest(
        harness.traffic, OWNER_PEER, SOURCE_ID, DESTINATION_ID, true));
    CHECK(FNETestHooks::processGrantRelease(
        harness.traffic, OWNER_PEER, DESTINATION_ID, SOURCE_ID, true));
}

TEST_CASE("FNE failed grant commit releases the reserved grant",
    "[fne][callhandler][grant][release][failed]")
{
    constexpr uint32_t OWNER_PEER = 1001U;
    constexpr uint32_t SOURCE_ID = 2001U;
    constexpr uint32_t TALK_GROUP = 3100U;

    FNEHarness harness;
    FNETestHooks::addPeer(harness.traffic, OWNER_PEER, network::NET_STAT_RUNNING, true);
    harness.radios.addEntry(SOURCE_ID, true, "test source");
    harness.talkgroups.addEntry(TALK_GROUP, 1U, true, false);

    REQUIRE(FNETestHooks::processP25GrantRequest(
        harness.traffic, OWNER_PEER, SOURCE_ID, TALK_GROUP, false));
    CHECK(FNETestHooks::processGrantRelease(
        harness.traffic, OWNER_PEER, SOURCE_ID, TALK_GROUP, false, true));
    CHECK_FALSE(FNETestHooks::processGrantRelease(
        harness.traffic, OWNER_PEER, SOURCE_ID, TALK_GROUP, false, true));
}

TEST_CASE("FNE DMR group grant demand retains group peer routing",
    "[fne][callhandler][routing][group][dmr]")
{
    constexpr uint32_t SOURCE_PEER = 1001U;
    constexpr uint32_t SITE_A = 1002U;
    constexpr uint32_t SITE_B = 1003U;
    constexpr uint32_t SOURCE_ID = 2001U;
    constexpr uint32_t TALK_GROUP = 3100U;

    FNEHarness harness;
    FNETestHooks::addPeer(harness.traffic, SOURCE_PEER, network::NET_STAT_RUNNING, true);
    FNETestHooks::addPeer(harness.traffic, SITE_A, network::NET_STAT_RUNNING, true);
    FNETestHooks::addPeer(harness.traffic, SITE_B, network::NET_STAT_RUNNING, true);
    harness.radios.addEntry(SOURCE_ID, true, "test source");
    harness.talkgroups.addEntry(TALK_GROUP, 1U, true, false);
    FNETestHooks::addGroupAffiliation(harness.traffic, SITE_A, SOURCE_ID, TALK_GROUP);
    FNETestHooks::addGroupAffiliation(harness.traffic, SITE_B, SOURCE_ID, TALK_GROUP);

    std::vector<uint32_t> writes;
    FNETestHooks::capturePeerWrites(harness.traffic, writes);
    REQUIRE(FNETestHooks::processDMRGrantRequest(
        harness.traffic, SOURCE_PEER, SOURCE_ID, TALK_GROUP, false));
    std::sort(writes.begin(), writes.end());
    CHECK(writes == std::vector<uint32_t>{ SOURCE_PEER, SITE_A, SITE_B });
}

TEST_CASE("FNE NXDN individual grant demand targets the called unit site",
    "[fne][callhandler][routing][u2u][nxdn]")
{
    constexpr uint32_t SOURCE_PEER = 1001U;
    constexpr uint32_t SOURCE_SITE = 1002U;
    constexpr uint32_t TARGET_SITE = 1003U;
    constexpr uint32_t OTHER_SITE = 1004U;
    constexpr uint32_t SOURCE_ID = 2001U;
    constexpr uint32_t DESTINATION_ID = 2002U;

    FNEHarness harness;
    FNETestHooks::addPeer(harness.traffic, SOURCE_PEER, network::NET_STAT_RUNNING, true);
    FNETestHooks::addPeer(harness.traffic, SOURCE_SITE, network::NET_STAT_RUNNING, true);
    FNETestHooks::addPeer(harness.traffic, TARGET_SITE, network::NET_STAT_RUNNING, true);
    FNETestHooks::addPeer(harness.traffic, OTHER_SITE, network::NET_STAT_RUNNING, true);
    harness.radios.addEntry(SOURCE_ID, true, "test source");
    harness.radios.addEntry(DESTINATION_ID, true, "test destination");
    FNETestHooks::registerP25Unit(harness.traffic, SOURCE_SITE, SOURCE_ID);
    FNETestHooks::registerP25Unit(harness.traffic, TARGET_SITE, DESTINATION_ID);

    std::vector<uint32_t> writes;
    FNETestHooks::capturePeerWrites(harness.traffic, writes);
    REQUIRE(FNETestHooks::processNXDNGrantRequest(
        harness.traffic, SOURCE_PEER, SOURCE_ID, DESTINATION_ID, true));
    std::sort(writes.begin(), writes.end());
    CHECK(writes == std::vector<uint32_t>{ SOURCE_PEER, TARGET_SITE });
}

TEST_CASE("FNE NXDN group grant demand retains group peer routing",
    "[fne][callhandler][routing][group][nxdn]")
{
    constexpr uint32_t SOURCE_PEER = 1001U;
    constexpr uint32_t SITE_A = 1002U;
    constexpr uint32_t SITE_B = 1003U;
    constexpr uint32_t SOURCE_ID = 2001U;
    constexpr uint32_t TALK_GROUP = 3100U;

    FNEHarness harness;
    FNETestHooks::addPeer(harness.traffic, SOURCE_PEER, network::NET_STAT_RUNNING, true);
    FNETestHooks::addPeer(harness.traffic, SITE_A, network::NET_STAT_RUNNING, true);
    FNETestHooks::addPeer(harness.traffic, SITE_B, network::NET_STAT_RUNNING, true);
    harness.radios.addEntry(SOURCE_ID, true, "test source");
    harness.talkgroups.addEntry(TALK_GROUP, 1U, true, false);
    FNETestHooks::addGroupAffiliation(harness.traffic, SITE_A, SOURCE_ID, TALK_GROUP);
    FNETestHooks::addGroupAffiliation(harness.traffic, SITE_B, SOURCE_ID, TALK_GROUP);

    std::vector<uint32_t> writes;
    FNETestHooks::capturePeerWrites(harness.traffic, writes);
    REQUIRE(FNETestHooks::processNXDNGrantRequest(
        harness.traffic, SOURCE_PEER, SOURCE_ID, TALK_GROUP, false));
    std::sort(writes.begin(), writes.end());
    CHECK(writes == std::vector<uint32_t>{ SOURCE_PEER, SITE_A, SITE_B });
}

TEST_CASE("FNE call handlers validate source radios and group state", "[fne][callhandler][validation]")
{
    constexpr uint32_t PEER_ID = 1001U;
    constexpr uint32_t SOURCE_ID = 2001U;
    constexpr uint32_t TALK_GROUP = 3100U;
    constexpr uint32_t INVALID_TALK_GROUP = 3200U;

    FNEHarness harness;
    FNETestHooks::addPeer(harness.traffic, PEER_ID, network::NET_STAT_RUNNING, true);
    std::vector<uint32_t> rejectedWrites;
    FNETestHooks::capturePeerWrites(harness.traffic, rejectedWrites);
    harness.radios.addEntry(SOURCE_ID, true, "test source");
    harness.talkgroups.addEntry(TALK_GROUP, 1U, true, false);

    const std::vector<FNETestHooks::ParrotProtocol> protocols = {
        FNETestHooks::ParrotProtocol::DMR,
        FNETestHooks::ParrotProtocol::P25,
        FNETestHooks::ParrotProtocol::P25P2,
        FNETestHooks::ParrotProtocol::NXDN,
        FNETestHooks::ParrotProtocol::ANALOG
    };

    for (FNETestHooks::ParrotProtocol protocol : protocols) {
        CAPTURE(static_cast<int>(protocol));
        CHECK(FNETestHooks::validateGroupCall(
            harness.traffic, protocol, PEER_ID, SOURCE_ID, TALK_GROUP));
        CHECK_FALSE(FNETestHooks::validateGroupCall(
            harness.traffic, protocol, PEER_ID, SOURCE_ID, INVALID_TALK_GROUP));
    }

    harness.talkgroups.addEntry(TALK_GROUP, 1U, false, false);
    harness.radios.addEntry(SOURCE_ID, false, "disabled source");
    for (FNETestHooks::ParrotProtocol protocol : protocols) {
        CAPTURE(static_cast<int>(protocol));
        CHECK_FALSE(FNETestHooks::validateGroupCall(
            harness.traffic, protocol, PEER_ID, SOURCE_ID, TALK_GROUP));
    }
}

TEST_CASE("FNE analog handler preserves the individual-call flag", "[fne][callhandler][analog][u2u]")
{
    constexpr uint32_t PEER_ID = 1001U;
    constexpr uint32_t SOURCE_ID = 2001U;
    constexpr uint32_t DESTINATION_ID = 2002U;
    constexpr uint32_t STREAM_ID = 100U;

    FNEHarness harness;
    FNETestHooks::addPeer(harness.traffic, PEER_ID, network::NET_STAT_RUNNING, true);
    FNETestHooks::disallowU2U(harness.traffic, true);

    std::vector<uint8_t> frame(network::ANALOG_PACKET_LENGTH, 0U);
    SET_UINT24(SOURCE_ID, frame.data(), 5U);
    SET_UINT24(DESTINATION_ID, frame.data(), 8U);
    frame[15U] = 0x40U | analog::defines::AudioFrameType::VOICE;

    CHECK_FALSE(harness.traffic.analogTrafficHandler()->processFrame(
        frame.data(), (uint32_t)frame.size(), PEER_ID, PEER_ID, 1U, STREAM_ID));
}

TEST_CASE("FNE call handlers reject truncated protocol frames", "[fne][callhandler][validation][security]")
{
    constexpr uint32_t PEER_ID = 1001U;

    FNEHarness harness;
    FNETestHooks::addPeer(harness.traffic, PEER_ID, network::NET_STAT_RUNNING, true);

    const std::vector<FNETestHooks::ParrotProtocol> protocols = {
        FNETestHooks::ParrotProtocol::DMR,
        FNETestHooks::ParrotProtocol::P25,
        FNETestHooks::ParrotProtocol::P25P2,
        FNETestHooks::ParrotProtocol::NXDN,
        FNETestHooks::ParrotProtocol::ANALOG
    };

    for (FNETestHooks::ParrotProtocol protocol : protocols) {
        CAPTURE(static_cast<int>(protocol));
        CHECK_FALSE(FNETestHooks::processTruncatedFrame(harness.traffic, protocol, PEER_ID));
    }
}

TEST_CASE("FNE parrot playback permits always-send and console peers", "[fne][parrot][routing]")
{
    constexpr uint32_t PARROT_TG = 3100U;
    constexpr uint32_t ALWAYS_SEND_PEER = 1001U;
    constexpr uint32_t CONSOLE_PEER = 1002U;
    constexpr uint32_t UNAFFILIATED_PEER = 1003U;

    FNEHarness harness;
    harness.talkgroups.addEntry(PARROT_TG, 0U, true, true);
    lookups::TalkgroupRuleGroupVoice talkgroup = harness.talkgroups.find(PARROT_TG);
    lookups::TalkgroupRuleConfig config = talkgroup.config();
    config.alwaysSend({ ALWAYS_SEND_PEER });
    talkgroup.config(config);
    harness.talkgroups.addEntry(talkgroup);

    FNETestHooks::addPeer(harness.traffic, ALWAYS_SEND_PEER, network::NET_STAT_RUNNING, true);
    network::FNEPeerConnection& console = FNETestHooks::addPeer(
        harness.traffic, CONSOLE_PEER, network::NET_STAT_RUNNING, true);
    console.peerClass(network::PEER_CONN_CLASS_CONSOLE);
    FNETestHooks::addPeer(harness.traffic, UNAFFILIATED_PEER, network::NET_STAT_RUNNING, true);

    const std::vector<FNETestHooks::ParrotProtocol> protocols = {
        FNETestHooks::ParrotProtocol::DMR,
        FNETestHooks::ParrotProtocol::P25,
        FNETestHooks::ParrotProtocol::P25P2,
        FNETestHooks::ParrotProtocol::NXDN,
        FNETestHooks::ParrotProtocol::ANALOG
    };

    for (FNETestHooks::ParrotProtocol protocol : protocols) {
        CAPTURE(static_cast<int>(protocol));
        std::vector<uint32_t> destinations;
        FNETestHooks::capturePeerWrites(harness.traffic, destinations);

        FNETestHooks::playbackParrotFrame(harness.traffic, protocol,
            UNAFFILIATED_PEER, 2003U, PARROT_TG);

        std::sort(destinations.begin(), destinations.end());
        REQUIRE(destinations == std::vector<uint32_t>{ ALWAYS_SEND_PEER, CONSOLE_PEER });
    }
}

TEST_CASE("FNE parrot playback blocks excluded peers", "[fne][parrot][routing]")
{
    constexpr uint32_t PARROT_TG = 3100U;
    constexpr uint32_t PERMITTED_PEER = 1001U;
    constexpr uint32_t EXCLUDED_PEER = 1002U;

    FNEHarness harness;
    harness.talkgroups.addEntry(PARROT_TG, 0U, true, false);
    lookups::TalkgroupRuleGroupVoice talkgroup = harness.talkgroups.find(PARROT_TG);
    lookups::TalkgroupRuleConfig config = talkgroup.config();
    config.exclusion({ EXCLUDED_PEER });
    config.alwaysSend({ EXCLUDED_PEER });
    talkgroup.config(config);
    harness.talkgroups.addEntry(talkgroup);

    FNETestHooks::addPeer(harness.traffic, PERMITTED_PEER, network::NET_STAT_RUNNING, true);
    network::FNEPeerConnection& excluded = FNETestHooks::addPeer(
        harness.traffic, EXCLUDED_PEER, network::NET_STAT_RUNNING, true);
    excluded.peerClass(network::PEER_CONN_CLASS_CONSOLE);
    FNETestHooks::addGroupAffiliation(harness.traffic, PERMITTED_PEER, 2001U, PARROT_TG);
    FNETestHooks::addGroupAffiliation(harness.traffic, EXCLUDED_PEER, 2002U, PARROT_TG);

    const std::vector<FNETestHooks::ParrotProtocol> protocols = {
        FNETestHooks::ParrotProtocol::DMR,
        FNETestHooks::ParrotProtocol::P25,
        FNETestHooks::ParrotProtocol::P25P2,
        FNETestHooks::ParrotProtocol::NXDN,
        FNETestHooks::ParrotProtocol::ANALOG
    };

    for (FNETestHooks::ParrotProtocol protocol : protocols) {
        CAPTURE(static_cast<int>(protocol));
        std::vector<uint32_t> destinations;
        FNETestHooks::capturePeerWrites(harness.traffic, destinations);

        FNETestHooks::playbackParrotFrame(harness.traffic, protocol,
            PERMITTED_PEER, 2001U, PARROT_TG);

        REQUIRE(destinations == std::vector<uint32_t>{ PERMITTED_PEER });
    }
}

TEST_CASE("FNE parrot playback honors peer inclusion lists", "[fne][parrot][routing]")
{
    constexpr uint32_t PARROT_TG = 3100U;
    constexpr uint32_t INCLUDED_PEER = 1001U;
    constexpr uint32_t OTHER_PEER = 1002U;

    FNEHarness harness;
    harness.talkgroups.addEntry(PARROT_TG, 0U, true, false);
    lookups::TalkgroupRuleGroupVoice talkgroup = harness.talkgroups.find(PARROT_TG);
    lookups::TalkgroupRuleConfig config = talkgroup.config();
    config.inclusion({ INCLUDED_PEER });
    config.alwaysSend({ OTHER_PEER });
    talkgroup.config(config);
    harness.talkgroups.addEntry(talkgroup);

    FNETestHooks::addPeer(harness.traffic, INCLUDED_PEER, network::NET_STAT_RUNNING, true);
    FNETestHooks::addPeer(harness.traffic, OTHER_PEER, network::NET_STAT_RUNNING, true);

    const std::vector<FNETestHooks::ParrotProtocol> protocols = {
        FNETestHooks::ParrotProtocol::DMR,
        FNETestHooks::ParrotProtocol::P25,
        FNETestHooks::ParrotProtocol::P25P2,
        FNETestHooks::ParrotProtocol::NXDN,
        FNETestHooks::ParrotProtocol::ANALOG
    };

    for (FNETestHooks::ParrotProtocol protocol : protocols) {
        CAPTURE(static_cast<int>(protocol));
        std::vector<uint32_t> destinations;
        FNETestHooks::capturePeerWrites(harness.traffic, destinations);

        FNETestHooks::playbackParrotFrame(harness.traffic, protocol,
            OTHER_PEER, 2002U, PARROT_TG);

        REQUIRE(destinations == std::vector<uint32_t>{ INCLUDED_PEER });
    }
}

TEST_CASE("FNE P25 filtered terminators honor peer routing lists", "[fne][callhandler][p25][routing]")
{
    constexpr uint32_t TALK_GROUP = 3100U;
    constexpr uint32_t INCLUDED_PEER = 1001U;
    constexpr uint32_t EXCLUDED_PEER = 1002U;
    constexpr uint32_t OTHER_PEER = 1003U;

    FNEHarness harness;
    harness.talkgroups.addEntry(TALK_GROUP, 0U, true, false);
    lookups::TalkgroupRuleGroupVoice talkgroup = harness.talkgroups.find(TALK_GROUP);
    lookups::TalkgroupRuleConfig config = talkgroup.config();
    config.inclusion({ INCLUDED_PEER });
    config.exclusion({ EXCLUDED_PEER });
    talkgroup.config(config);
    harness.talkgroups.addEntry(talkgroup);
    FNETestHooks::filterTerminators(harness.traffic, true);

    CHECK(FNETestHooks::isP25TerminatorPeerPermitted(
        harness.traffic, INCLUDED_PEER, TALK_GROUP));
    CHECK_FALSE(FNETestHooks::isP25TerminatorPeerPermitted(
        harness.traffic, EXCLUDED_PEER, TALK_GROUP));
    CHECK_FALSE(FNETestHooks::isP25TerminatorPeerPermitted(
        harness.traffic, OTHER_PEER, TALK_GROUP));
}

TEST_CASE("Affiliation snapshots replace prior state", "[fne][affiliations]")
{
    lookups::AffiliationLookup affiliations("test-affiliations", nullptr, false);
    affiliations.groupAff(1001U, 3100U);
    affiliations.groupAff(1002U, 3200U);

    std::unordered_map<uint32_t, uint32_t> replacement = {
        { 1002U, 3300U },
        { 1003U, 3400U }
    };
    affiliations.replaceGroupAffiliations(replacement);

    REQUIRE(affiliations.grpAffTable() == replacement);

    affiliations.replaceGroupAffiliations({});
    REQUIRE(affiliations.grpAffTable().empty());
}

TEST_CASE("Unit registration snapshots replace prior state", "[fne][registrations]")
{
    lookups::AffiliationLookup registrations("test-registrations", nullptr, false);
    registrations.unitReg(1001U);
    registrations.unitReg(1002U);

    registrations.replaceUnitRegistrations({ 1002U, 1003U, 1003U });

    REQUIRE(registrations.unitRegTable() == std::vector<uint32_t>{ 1002U, 1003U });

    registrations.replaceUnitRegistrations({});
    REQUIRE(registrations.unitRegTable().empty());
}

TEST_CASE("FNE test hooks expose peer connection state", "[fne][hooks]")
{
    FNEHarness harness;
    FNETestHooks::addPeer(harness.traffic, 1001U, network::NET_STAT_WAITING_AUTHORISATION);

    REQUIRE(FNETestHooks::hasPeer(harness.traffic, 1001U));
    REQUIRE(FNETestHooks::peerCount(harness.traffic) == 1U);
    REQUIRE(FNETestHooks::peerState(harness.traffic, 1001U) == network::NET_STAT_WAITING_AUTHORISATION);
}

TEST_CASE("FNE peer references remain valid after registry removal", "[fne][hooks][concurrency]")
{
    FNEHarness harness;
    FNETestHooks::addPeer(harness.traffic, 1001U, network::NET_STAT_RUNNING, true);

    std::shared_ptr<network::FNEPeerConnection> retained =
        FNETestHooks::retainPeer(harness.traffic, 1001U);
    REQUIRE(retained != nullptr);

    FNETestHooks::removePeer(harness.traffic, 1001U);

    REQUIRE_FALSE(FNETestHooks::hasPeer(harness.traffic, 1001U));
    REQUIRE(FNETestHooks::peerCount(harness.traffic) == 0U);
    REQUIRE(retained->id() == 1001U);
    REQUIRE(retained->connectionState() == network::NET_STAT_RUNNING);
}

TEST_CASE("FNE replicated peer snapshots are stable copies", "[fne][replication]")
{
    FNEHarness harness;
    json::array peers;
    json::object peer;
    uint32_t replicatedPeerId = 2001U;
    peer["peerId"].set<uint32_t>(replicatedPeerId);
    peers.push_back(json::value(peer));

    FNETestHooks::updateReplicatedPeers(harness.traffic, 1001U, peers);
    std::unordered_map<uint32_t, json::array> snapshot = FNETestHooks::replicatedPeersSnapshot(harness.traffic);
    REQUIRE(snapshot.size() == 1U);
    REQUIRE(snapshot.at(1001U).size() == 1U);

    FNETestHooks::eraseReplicatedPeers(harness.traffic, 1001U);
    REQUIRE(FNETestHooks::replicatedPeersSnapshot(harness.traffic).empty());
    REQUIRE(snapshot.at(1001U).size() == 1U);
}

TEST_CASE("FNE traffic dispatcher rejects truncated pre-authentication packets", "[fne][hooks][security]")
{
    FNEHarness harness;
    FNETestHooks::addPeer(harness.traffic, 1001U, network::NET_STAT_WAITING_AUTHORISATION);

    REQUIRE_NOTHROW(FNETestHooks::dispatchTraffic(harness.traffic, network::NET_FUNC::RPTK,
        network::NET_SUBFUNC::NOP, 1001U, std::vector<uint8_t>(8U)));
    REQUIRE(FNETestHooks::peerState(harness.traffic, 1001U) == network::NET_STAT_WAITING_AUTHORISATION);

    REQUIRE_NOTHROW(FNETestHooks::dispatchTraffic(harness.traffic, network::NET_FUNC::RPTC,
        network::NET_SUBFUNC::NOP, 1001U, std::vector<uint8_t>(8U)));
    REQUIRE(FNETestHooks::peerState(harness.traffic, 1001U) == network::NET_STAT_WAITING_AUTHORISATION);
}

TEST_CASE("FNE metadata dispatcher rejects truncated log transfers", "[fne][hooks][security]")
{
    FNEHarness harness;
    FNETestHooks::addPeer(harness.traffic, 1001U, network::NET_STAT_RUNNING, true);

    REQUIRE_NOTHROW(FNETestHooks::dispatchMetadata(harness.traffic, harness.metadata,
        network::NET_FUNC::TRANSFER, network::NET_SUBFUNC::TRANSFER_SUBFUNC_ACTIVITY,
        1001U, std::vector<uint8_t>(11U)));
    REQUIRE_NOTHROW(FNETestHooks::dispatchMetadata(harness.traffic, harness.metadata,
        network::NET_FUNC::TRANSFER, network::NET_SUBFUNC::TRANSFER_SUBFUNC_DIAG,
        1001U, std::vector<uint8_t>(10U)));
    REQUIRE(FNETestHooks::peerState(harness.traffic, 1001U) == network::NET_STAT_RUNNING);
}
