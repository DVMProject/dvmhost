// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/FNETestHooks.h"
#include "fne/HostFNE.h"
#include "common/lookups/PeerListLookup.h"
#include "common/p25/lc/tsbk/ISP_CAN_SRV_REQ.h"
#include "common/p25/lc/tsbk/ISP_TELE_INT_PSTN_REQ.h"

using namespace network;
using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <catch2/catch_test_macros.hpp>

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Test harness for P25 telephone interconnect routing in the FNE.
 */
class P25TelephoneFNEHarness {
public:
    /** @brief Initializes the FNE traffic network and its peer lookup. */
    P25TelephoneFNEHarness() :
        host("p25-telephone-fne-test.yml"),
        traffic(&host, "127.0.0.1", 62041U, 999999U, "test-password", "test-fne",
            false, false, false, false, true, true, true, true, true,
            0U, false, true, true, 5U, 10U, 2U),
        peers("p25-telephone-peers.dat", 0U, true, false)
    {
        traffic.setLookups(nullptr, nullptr, nullptr, &peers, nullptr, nullptr);
    }

    /** @brief Releases peer connections retained by the traffic network. */
    ~P25TelephoneFNEHarness()
    {
        FNETestHooks::clearPeers(traffic);
    }

    /**
     * @brief Adds a peer to both the ACL and active connection registry.
     * @param peerId Peer ID.
     * @param gateway True when the peer is a telephone interconnect gateway.
     * @return Active peer connection.
     */
    FNEPeerConnection& addPeer(uint32_t peerId, bool gateway = false)
    {
        lookups::PeerId entry(peerId, "test-peer", "", false);
        entry.telephoneInterconnect(gateway);
        peers.addEntry(peerId, entry);
        return FNETestHooks::addPeer(traffic, peerId, NET_STAT_RUNNING, true);
    }

    HostFNE host;
    TrafficNetwork traffic;
    lookups::PeerListLookup peers;
};

TEST_CASE("FNE classifies P25 telephone interconnect traffic by DUID and manufacturer",
    "[fne][p25][telephone]")
{
    P25TelephoneFNEHarness harness;
    LC control;
    control.setLCO(LCO::TEL_INT_VCH_USER);
    control.setMFId(MFG_STANDARD);

    REQUIRE(FNETestHooks::isP25TelIntTraffic(harness.traffic, control, DUID::LDU1));
    REQUIRE(FNETestHooks::isP25TelIntTraffic(harness.traffic, control, DUID::TDULC));
    REQUIRE_FALSE(FNETestHooks::isP25TelIntTraffic(harness.traffic, control, DUID::PDU));

    control.setMFId(MFG_MOT);
    REQUIRE_FALSE(FNETestHooks::isP25TelIntTraffic(harness.traffic, control, DUID::LDU1));

    ISP_TELE_INT_PSTN_REQ request;
    request.setMFId(MFG_STANDARD);
    REQUIRE(FNETestHooks::isP25TelIntTraffic(harness.traffic, control, DUID::TSDU, &request));

    request.setMFId(MFG_MOT);
    REQUIRE(FNETestHooks::isP25TelIntTraffic(harness.traffic, control, DUID::TSDU, &request));

    request.setMFId(0x7FU);
    REQUIRE_FALSE(FNETestHooks::isP25TelIntTraffic(harness.traffic, control, DUID::TSDU, &request));

    ISP_CAN_SRV_REQ cancel;
    cancel.setMFId(MFG_STANDARD);
    cancel.setService(TSBKO::ISP_TELE_INT_PSTN_REQ);
    REQUIRE(FNETestHooks::isP25TelIntTraffic(harness.traffic, control, DUID::TSDU, &cancel));

    cancel.setService(TSBKO::IOSP_GRP_VCH);
    REQUIRE_FALSE(FNETestHooks::isP25TelIntTraffic(harness.traffic, control, DUID::TSDU, &cancel));
    REQUIRE_FALSE(FNETestHooks::isP25TelIntTraffic(harness.traffic, control, DUID::TSDU));
}

TEST_CASE("FNE routes P25 telephone interconnect only between gateways and the registered site",
    "[fne][p25][telephone][routing]")
{
    P25TelephoneFNEHarness harness;
    const uint32_t sitePeerId = 1001U;
    const uint32_t otherSitePeerId = 1002U;
    const uint32_t voicePeerId = 1101U;
    const uint32_t gatewayPeerId = 9001U;
    const uint32_t otherGatewayPeerId = 9002U;
    const uint32_t replicaPeerId = 7001U;
    const uint32_t unitId = 123456U;

    harness.addPeer(sitePeerId);
    harness.addPeer(otherSitePeerId);
    harness.addPeer(voicePeerId).ccPeerId(sitePeerId);
    harness.addPeer(gatewayPeerId, true);
    harness.addPeer(otherGatewayPeerId, true);
    harness.addPeer(replicaPeerId).isReplica(true);
    FNETestHooks::registerP25Unit(harness.traffic, sitePeerId, unitId);

    REQUIRE(FNETestHooks::isP25TelIntPeerPermitted(
        harness.traffic, sitePeerId, gatewayPeerId, unitId));
    REQUIRE_FALSE(FNETestHooks::isP25TelIntPeerPermitted(
        harness.traffic, sitePeerId, otherSitePeerId, unitId));

    REQUIRE(FNETestHooks::isP25TelIntPeerPermitted(
        harness.traffic, gatewayPeerId, sitePeerId, unitId));
    REQUIRE(FNETestHooks::isP25TelIntPeerPermitted(
        harness.traffic, gatewayPeerId, voicePeerId, unitId));
    REQUIRE_FALSE(FNETestHooks::isP25TelIntPeerPermitted(
        harness.traffic, gatewayPeerId, otherSitePeerId, unitId));
    REQUIRE_FALSE(FNETestHooks::isP25TelIntPeerPermitted(
        harness.traffic, gatewayPeerId, otherGatewayPeerId, unitId));

    REQUIRE(FNETestHooks::isP25TelIntPeerPermitted(
        harness.traffic, gatewayPeerId, otherSitePeerId, unitId + 1U));
    REQUIRE(FNETestHooks::isP25TelIntPeerPermitted(
        harness.traffic, gatewayPeerId, replicaPeerId, unitId));
}
