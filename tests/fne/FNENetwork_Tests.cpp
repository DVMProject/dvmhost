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
#include "common/lookups/TalkgroupRulesLookup.h"

#include <algorithm>
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
        talkgroups("", 0U, true, false),
        traffic(&host, "127.0.0.1", 62031U, 999999U, "test-password", "test-fne",
            false, false, false, false, true, true, true, true, true,
            0U, false, true, true, 5U, 10U, 2U),
        metadata(&host, &traffic, "127.0.0.1", 62032U, 2U)
    {
        traffic.setLookups(nullptr, nullptr, &talkgroups, nullptr, nullptr, nullptr);
    }
    /**
     * @brief Finalizes a instance of the FNEHarness class.
     */
    ~FNEHarness()
    {
        FNETestHooks::clearPeerWriteCapture(traffic);
        FNETestHooks::clearPeers(traffic);
    }

    std::string configFile;
    HostFNE host;
    lookups::TalkgroupRulesLookup talkgroups;
    network::TrafficNetwork traffic;
    network::MetadataNetwork metadata;
};

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
    harness.talkgroups.addEntry(PARROT_TG, 0U, true, true);
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
