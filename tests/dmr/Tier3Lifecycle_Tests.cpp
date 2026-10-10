// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */

#include "host/Defines.h"
#include "common/dmr/DMRDefines.h"
#include "common/dmr/lc/csbk/CSBK_MAINT.h"
#include "common/dmr/lc/csbk/CSBK_P_CLEAR.h"
#include "common/lookups/ChannelLookup.h"
#include "host/dmr/lookups/DMRAffiliationLookup.h"

#include <catch2/catch_test_macros.hpp>

using namespace dmr::defines;
using namespace dmr::lc::csbk;

TEST_CASE("DMR Tier III maintenance preserves 24-bit addresses", "[dmr][tier3][csbk]")
{
    CSBK_MAINT encoded;
    encoded.setLastBlock(true);
    encoded.setMaintKind(0U);
    encoded.setDstId(WUID_TSI);
    encoded.setSrcId(0xABCDEFU);

    uint8_t burst[DMR_FRAME_LENGTH_BYTES] = { 0U };
    encoded.encode(burst);

    CSBK_MAINT decoded;
    decoded.setDataType(DataType::CSBK);
    REQUIRE(decoded.decode(burst));
    CHECK(decoded.getMaintKind() == 0U);
    CHECK(decoded.getDstId() == WUID_TSI);
    CHECK(decoded.getSrcId() == 0xABCDEFU);
}

TEST_CASE("DMR Tier III maintenance round trip preserves field boundaries", "[dmr][tier3][csbk]")
{
    CSBK_MAINT encoded;
    encoded.setLastBlock(true);
    encoded.setMaintKind(7U);
    encoded.setDstId(0xFFFFFFU);
    encoded.setSrcId(0x000001U);

    uint8_t burst[DMR_FRAME_LENGTH_BYTES] = { 0U };
    encoded.encode(burst);

    CSBK_MAINT decoded;
    decoded.setDataType(DataType::CSBK);
    REQUIRE(decoded.decode(burst));
    CHECK(decoded.getMaintKind() == 7U);
    CHECK(decoded.getDstId() == 0xFFFFFFU);
    CHECK(decoded.getSrcId() == 0x000001U);
}

TEST_CASE("DMR Tier III payload clear round trip carries TSCC return fields", "[dmr][tier3][csbk]")
{
    CSBK_P_CLEAR encoded;
    encoded.setLastBlock(true);
    encoded.setLogicalCh1(0U); // return to last-confirmed TSCC
    encoded.setGI(true);
    encoded.setDstId(0x123456U);
    encoded.setSrcId(WUID_TSI);

    uint8_t burst[DMR_FRAME_LENGTH_BYTES] = { 0U };
    encoded.encode(burst);

    CSBK_P_CLEAR decoded;
    decoded.setDataType(DataType::CSBK);
    REQUIRE(decoded.decode(burst));
    CHECK(decoded.getLogicalCh1() == 0U);
    CHECK(decoded.getGI());
    CHECK(decoded.getDstId() == 0x123456U);
    CHECK(decoded.getSrcId() == WUID_TSI);
}

TEST_CASE("DMR Tier III individual payload clear preserves maximum channel and addresses", "[dmr][tier3][csbk]")
{
    CSBK_P_CLEAR encoded;
    encoded.setLastBlock(true);
    encoded.setLogicalCh1(0xFFFU);
    encoded.setGI(false);
    encoded.setDstId(0xFFFFFFU);
    encoded.setSrcId(0x000001U);

    uint8_t burst[DMR_FRAME_LENGTH_BYTES] = { 0U };
    encoded.encode(burst);

    CSBK_P_CLEAR decoded;
    decoded.setDataType(DataType::CSBK);
    REQUIRE(decoded.decode(burst));
    CHECK(decoded.getLogicalCh1() == 0xFFFU);
    CHECK_FALSE(decoded.getGI());
    CHECK(decoded.getDstId() == 0xFFFFFFU);
    CHECK(decoded.getSrcId() == 0x000001U);
}

TEST_CASE("DMR Tier III grants account for both slots without changing ChannelLookup", "[dmr][tier3][grant]")
{
    ::lookups::ChannelLookup channels;
    REQUIRE(channels.initializeRFCh(101U));

    dmr::lookups::DMRAffiliationLookup affiliations(&channels, false);

    REQUIRE(affiliations.grantChSlot(2001U, 1001U, 1U, 15U, true, false));
    CHECK_FALSE(channels.isRFChAvailable());
    CHECK_FALSE(affiliations.isChBusy(101U));

    // The physical channel bit is already allocated, but DMR slot 2 remains usable.
    REQUIRE(affiliations.grantChSlot(2002U, 1002U, 2U, 15U, true, false));
    CHECK(affiliations.getGrantedCh(2002U) == 101U);
    CHECK(affiliations.isChBusy(101U));

    // Releasing one logical slot must not free the shared physical carrier.
    REQUIRE(affiliations.releaseGrant(2001U, false));
    CHECK_FALSE(channels.isRFChAvailable());
    CHECK_FALSE(affiliations.isChBusy(101U));

    REQUIRE(affiliations.releaseGrant(2002U, false));
    CHECK(channels.isRFChAvailable());
}

TEST_CASE("DMR Tier III does not double-grant one slot", "[dmr][tier3][grant]")
{
    ::lookups::ChannelLookup channels;
    REQUIRE(channels.initializeRFCh(101U));

    dmr::lookups::DMRAffiliationLookup affiliations(&channels, false);
    REQUIRE(affiliations.grantChSlot(2001U, 1001U, 1U, 15U, true, false));
    CHECK_FALSE(affiliations.grantChSlot(2002U, 1002U, 1U, 15U, true, false));
}

TEST_CASE("DMR Tier III rejects grants without a valid destination or RF resource", "[dmr][tier3][grant]")
{
    ::lookups::ChannelLookup channels;
    dmr::lookups::DMRAffiliationLookup affiliations(&channels, false);

    CHECK_FALSE(affiliations.grantChSlot(0U, 1001U, 1U, 15U, true, false));
    CHECK_FALSE(affiliations.grantChSlot(2001U, 1001U, 1U, 15U, true, false));
    CHECK(affiliations.getGrantedRFChCnt() == 0U);
}

TEST_CASE("DMR Tier III spills an occupied slot onto another physical channel", "[dmr][tier3][grant]")
{
    ::lookups::ChannelLookup channels;
    REQUIRE(channels.initializeRFCh(101U));
    REQUIRE(channels.initializeRFCh(102U));

    dmr::lookups::DMRAffiliationLookup affiliations(&channels, false);
    REQUIRE(affiliations.grantChSlot(2001U, 1001U, 1U, 15U, true, false));
    REQUIRE(affiliations.grantChSlot(2002U, 1002U, 1U, 15U, true, false));

    CHECK(affiliations.getGrantedCh(2001U) == 101U);
    CHECK(affiliations.getGrantedCh(2002U) == 102U);
    CHECK(affiliations.getGrantedSlot(2001U) == 1U);
    CHECK(affiliations.getGrantedSlot(2002U) == 1U);
    CHECK_FALSE(channels.isRFChAvailable());
}

TEST_CASE("DMR Tier III can recreate a grant on an exact remote payload channel", "[dmr][tier3][grant]")
{
    ::lookups::ChannelLookup channels;
    REQUIRE(channels.initializeRFCh(101U));
    REQUIRE(channels.initializeRFCh(102U));

    dmr::lookups::DMRAffiliationLookup affiliations(&channels, false);
    affiliations.setSlotForChannelTSCC(101U, 1U);

    REQUIRE(affiliations.grantChSlot(2001U, 1001U, 2U, 15U, true, true, 102U));
    CHECK(affiliations.getGrantedCh(2001U) == 102U);
    CHECK(affiliations.getGrantedSlot(2001U) == 2U);
    CHECK_FALSE(affiliations.grantChSlot(2002U, 1002U, 2U, 15U, true, true, 102U));
}

TEST_CASE("DMR Tier III reserves the TSCC slot and permits its opposing payload slot", "[dmr][tier3][grant]")
{
    ::lookups::ChannelLookup channels;
    REQUIRE(channels.initializeRFCh(101U));

    dmr::lookups::DMRAffiliationLookup affiliations(&channels, false);
    affiliations.setSlotForChannelTSCC(101U, 1U);

    CHECK_FALSE(channels.isRFChAvailable());
    CHECK(affiliations.getAvailableSlotForChannel(101U) == 2U);
    CHECK(affiliations.getAvailableChannelForSlot(1U) == 0U);
    CHECK(affiliations.getAvailableChannelForSlot(2U) == 101U);
    CHECK_FALSE(affiliations.isChBusy(101U));

    REQUIRE(affiliations.grantChSlot(2001U, 1001U, 2U, 15U, true, false));
    CHECK(affiliations.isChBusy(101U));
    CHECK(affiliations.getAvailableSlotForChannel(101U) == 0U);

    REQUIRE(affiliations.releaseGrant(2001U, false));
    CHECK_FALSE(affiliations.isChBusy(101U));
    CHECK_FALSE(channels.isRFChAvailable());
}

TEST_CASE("DMR Tier III release callback reports physical channel and slot", "[dmr][tier3][grant]")
{
    ::lookups::ChannelLookup channels;
    REQUIRE(channels.initializeRFCh(101U));

    dmr::lookups::DMRAffiliationLookup affiliations(&channels, false);
    uint32_t releasedCh = 0U;
    uint32_t releasedSrc = 0U;
    uint32_t releasedDst = 0U;
    uint8_t releasedSlot = 0U;
    affiliations.setReleaseGrantCallback(
        [&](uint32_t chNo, uint32_t srcId, uint32_t dstId, uint8_t slot) {
            releasedCh = chNo;
            releasedSrc = srcId;
            releasedDst = dstId;
            releasedSlot = slot;
        });

    REQUIRE(affiliations.grantChSlot(2001U, 1001U, 2U, 15U, false, true));
    CHECK_FALSE(affiliations.isGroup(2001U));
    CHECK(affiliations.isNetGranted(2001U));
    REQUIRE(affiliations.releaseGrant(2001U, false));

    CHECK(releasedCh == 101U);
    CHECK(releasedSrc == 1001U);
    CHECK(releasedDst == 2001U);
    CHECK(releasedSlot == 2U);
    CHECK(affiliations.getGrantedRFChCnt() == 0U);
}

TEST_CASE("DMR Tier III bulk release clears grants across carriers", "[dmr][tier3][grant]")
{
    ::lookups::ChannelLookup channels;
    REQUIRE(channels.initializeRFCh(101U));
    REQUIRE(channels.initializeRFCh(102U));

    dmr::lookups::DMRAffiliationLookup affiliations(&channels, false);
    REQUIRE(affiliations.grantChSlot(2001U, 1001U, 1U, 15U, true, false));
    REQUIRE(affiliations.grantChSlot(2002U, 1002U, 1U, 15U, true, false));
    CHECK(affiliations.getGrantedRFChCnt() == 2U);

    REQUIRE(affiliations.releaseGrant(0U, true));
    CHECK_FALSE(affiliations.isGranted(2001U));
    CHECK_FALSE(affiliations.isGranted(2002U));
    CHECK(affiliations.getGrantedSlot(2001U) == 0U);
    CHECK(affiliations.getGrantedSlot(2002U) == 0U);
    CHECK(affiliations.getGrantedRFChCnt() == 0U);
    CHECK(channels.isRFChAvailable());
}
