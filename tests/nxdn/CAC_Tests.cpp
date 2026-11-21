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
#include <cstring>

#include "common/nxdn/channel/CAC.h"
#include "common/nxdn/NXDNDefines.h"

using namespace nxdn;
using namespace nxdn::defines;
using namespace nxdn::channel;

TEST_CASE("CAC preserves short inbound fields and payload", "[nxdn][cac]") {
    uint8_t rawData[NXDN_CAC_CRC_LENGTH_BYTES];
    ::memset(rawData, 0x00U, sizeof(rawData));
    for (uint32_t i = 0U; i < sizeof(rawData); i++) {
        rawData[i] = static_cast<uint8_t>(i * 11U + 7U);
    }

    CAC cac;
    cac.setRAN(23U);
    cac.setStructure(ChStructure::SR_RCCH_SINGLE);
    cac.setIdleBusy(false);
    cac.setTxContinuous(true);
    cac.setReceive(false);
    cac.setData(rawData);

    REQUIRE(cac.getRAN() == 23U);
    REQUIRE(cac.getStructure() == ChStructure::SR_RCCH_SINGLE);

    uint8_t decodedData[12U];
    ::memset(decodedData, 0x00U, sizeof(decodedData));
    cac.getData(decodedData);
    REQUIRE(::memcmp(decodedData, rawData, sizeof(decodedData)) == 0);
}

TEST_CASE("CAC copy and assignment preserve key fields", "[nxdn][cac]") {
    CAC original;
    original.setRAN(9U);
    original.setStructure(ChStructure::SR_3_4);

    CAC copy(original);
    REQUIRE(copy.getRAN() == original.getRAN());
    REQUIRE(copy.getStructure() == original.getStructure());

    CAC assigned;
    assigned = original;
    REQUIRE(assigned.getRAN() == original.getRAN());
    REQUIRE(assigned.getStructure() == original.getStructure());
}
