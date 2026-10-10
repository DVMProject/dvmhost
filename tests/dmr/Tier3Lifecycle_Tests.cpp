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
