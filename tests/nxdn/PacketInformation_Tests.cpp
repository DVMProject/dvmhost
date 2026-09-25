// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 */

#include <catch2/catch_test_macros.hpp>

#include "common/nxdn/NXDNDefines.h"
#include "common/nxdn/lc/PacketInformation.h"

using namespace nxdn::defines;
using namespace nxdn::lc;

TEST_CASE("PacketInformation encodes DCALL_ACK response fields", "[nxdn][packet-information][golden]")
{
    PacketInformation packet;
    uint8_t data[2U] = {};

    SECTION("ACK receive success") {
        packet.setResponseClass(PDUResponseClass::ACK);
        packet.setResponseType(0x01U);
        packet.setFragmentCount(0x000U);

        packet.encode(MessageType::RTCH_DCALL_ACK, data);

        REQUIRE(data[0U] == 0x02U);
        REQUIRE(data[1U] == 0x00U);
    }

    SECTION("NACK abort with maximum fragment count") {
        packet.setResponseClass(PDUResponseClass::NACK);
        packet.setResponseType(0x03U);
        packet.setFragmentCount(0x1FFU);

        packet.encode(MessageType::RTCH_DCALL_ACK, data);

        REQUIRE(data[0U] == 0x37U);
        REQUIRE(data[1U] == 0xFFU);
    }
}
