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

#include "common/nxdn/lc/rcch/MESSAGE_TYPE_IDLE.h"
#include "common/nxdn/lc/rcch/RCCHFactory.h"
#include "common/nxdn/NXDNDefines.h"

using namespace nxdn;
using namespace nxdn::defines;
using namespace nxdn::lc;
using namespace nxdn::lc::rcch;

TEST_CASE("RCCH IDLE encode/decode preserves message type", "[nxdn][rcch]") {
    uint8_t data[NXDN_RCCH_LC_LENGTH_BYTES + 2U];
    ::memset(data, 0x00U, sizeof(data));

    MESSAGE_TYPE_IDLE idle;
    idle.encode(data, NXDN_RCCH_LC_LENGTH_BITS);

    REQUIRE((data[0U] & 0x3FU) == MessageType::IDLE);

    MESSAGE_TYPE_IDLE decoded;
    decoded.decode(data, NXDN_RCCH_LC_LENGTH_BITS);
    REQUIRE(decoded.getMessageType() == MessageType::IDLE);
    REQUIRE(decoded.toString() == "IDLE (Idle)");
}

TEST_CASE("RCCHFactory creates IDLE instance from encoded buffer", "[nxdn][rcch]") {
    uint8_t data[NXDN_RCCH_LC_LENGTH_BYTES + 2U];
    ::memset(data, 0x00U, sizeof(data));

    MESSAGE_TYPE_IDLE idle;
    idle.encode(data, NXDN_RCCH_LC_LENGTH_BITS);

    auto rcch = RCCHFactory::createRCCH(data, NXDN_RCCH_LC_LENGTH_BITS);
    REQUIRE(rcch != nullptr);
    REQUIRE(rcch->getMessageType() == MessageType::IDLE);
    REQUIRE(rcch->toString() == "IDLE (Idle)");
}
