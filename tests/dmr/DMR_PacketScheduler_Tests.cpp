// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "network/callhandler/packetdata/DMRPacketScheduler.h"
#include "network/callhandler/packetdata/DataRouting.h"

using namespace network::callhandler::packetdata;

#include <catch2/catch_test_macros.hpp>

#include <utility>

TEST_CASE("DMR packet scheduler bounds queued downlinks",
    "[dmr][packet-data][scheduler]")
{
    DMRPacketScheduler scheduler(2U, 8U);

    ScheduledDMRDataPacket first;
    first.dstId = 1U;
    first.userData.assign(4U, 0x11U);
    CHECK(scheduler.enqueue(std::move(first)) == 0U);

    ScheduledDMRDataPacket second;
    second.dstId = 2U;
    second.userData.assign(4U, 0x22U);
    CHECK(scheduler.enqueue(std::move(second)) == 0U);
    REQUIRE(scheduler.front() != nullptr);
    CHECK(scheduler.front()->dstId == 1U);

    ScheduledDMRDataPacket third;
    third.dstId = 3U;
    third.userData.assign(4U, 0x33U);
    CHECK(scheduler.enqueue(std::move(third)) == 1U);
    REQUIRE(scheduler.front() != nullptr);
    CHECK(scheduler.front()->dstId == 2U);
    CHECK(scheduler.size() == 2U);
    CHECK(scheduler.byteCount() == 8U);
}

TEST_CASE("DMR scheduled packet owns its header and payload",
    "[dmr][packet-data][scheduler][memory]")
{
    DMRPacketScheduler scheduler(1U, 64U);

    {
        dmr::data::DataHeader source;
        source.setSrcId(1001U);
        source.setDstId(2002U);

        ScheduledDMRDataPacket packet;
        packet.header = source;
        packet.userData.assign(20U, 0x55U);
        REQUIRE(scheduler.enqueue(std::move(packet)) == 0U);
    }

    REQUIRE(scheduler.front() != nullptr);
    CHECK(scheduler.front()->header.getSrcId() == 1001U);
    CHECK(scheduler.front()->header.getDstId() == 2002U);
    CHECK(scheduler.front()->userData.size() == 20U);

    scheduler.clear();
    CHECK(scheduler.empty());
}

TEST_CASE("DMR subscriber locations preserve peer and timeslot",
    "[dmr][packet-data][routing]")
{
    DataLocationRegistry locations(100U);
    ConventionalLocation location;
    location.peerId = 20U;
    location.slotNo = 2U;
    location.lastSeen = 1000U;
    REQUIRE(locations.updateConventional(1001U, location));

    DataRoute route = locations.resolve(1001U, AccessMode::CONVENTIONAL, 1050U);
    REQUIRE(route.valid);
    CHECK(route.peerId == 20U);
    CHECK(route.slotNo == 2U);

    location.peerId = 30U;
    location.slotNo = 1U;
    location.lastSeen = 1060U;
    REQUIRE(locations.updateConventional(1001U, location));
    route = locations.resolve(1001U, AccessMode::CONVENTIONAL, 1070U);
    CHECK(route.peerId == 30U);
    CHECK(route.slotNo == 1U);

    locations.expire(1160U);
    CHECK_FALSE(locations.resolve(1001U, AccessMode::CONVENTIONAL, 1160U).valid);
}
