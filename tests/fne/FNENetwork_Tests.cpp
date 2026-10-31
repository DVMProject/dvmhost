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
#include "common/lookups/AffiliationLookup.h"

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
        traffic(&host, "127.0.0.1", 62031U, 999999U, "test-password", "test-fne",
            false, false, false, false, true, true, true, true, true,
            0U, false, true, true, 5U, 10U, 2U),
        metadata(&host, &traffic, "127.0.0.1", 62032U, 2U)
    {
        /* stub */
    }
    /**
     * @brief Finalizes a instance of the FNEHarness class.
     */
    ~FNEHarness()
    {
        FNETestHooks::clearPeers(traffic);
    }

    std::string configFile;
    HostFNE host;
    network::TrafficNetwork traffic;
    network::MetadataNetwork metadata;
};

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
