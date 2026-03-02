// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */

#include <catch2/catch_test_macros.hpp>
#include <cstring>

#include "common/nxdn/channel/LICH.h"
#include "common/nxdn/NXDNDefines.h"

using namespace nxdn;
using namespace nxdn::defines;
using namespace nxdn::channel;

TEST_CASE("LICH encodes and decodes RCCH channel", "[nxdn][lich]") {
    uint8_t data[NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(data, 0x00U, sizeof(data));

    LICH lich;
    lich.setRFCT(RFChannelType::RCCH);
    lich.setFCT(FuncChannelType::CAC_OUTBOUND);
    lich.setOption(ChOption::DATA_COMMON);
    lich.setOutbound(true);

    lich.encode(data);

    // Decode and verify
    LICH decoded;
    REQUIRE(decoded.decode(data));
    REQUIRE(decoded.getRFCT() == RFChannelType::RCCH);
    REQUIRE(decoded.getFCT() == FuncChannelType::CAC_OUTBOUND);
    REQUIRE(decoded.getOption() == ChOption::DATA_COMMON);
    REQUIRE(decoded.getOutbound() == true);
}

TEST_CASE("LICH encodes and decodes RDCH voice channel", "[nxdn][lich]") {
    uint8_t data[NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(data, 0x00U, sizeof(data));

    LICH lich;
    lich.setRFCT(RFChannelType::RDCH);
    lich.setFCT(FuncChannelType::USC_SACCH_NS);
    lich.setOption(ChOption::STEAL_FACCH);
    lich.setOutbound(false);

    lich.encode(data);

    LICH decoded;
    REQUIRE(decoded.decode(data));
    REQUIRE(decoded.getRFCT() == RFChannelType::RDCH);
    REQUIRE(decoded.getFCT() == FuncChannelType::USC_SACCH_NS);
    REQUIRE(decoded.getOption() == ChOption::STEAL_FACCH);
    REQUIRE(decoded.getOutbound() == false);
}

TEST_CASE("LICH preserves all RFChannelType values", "[nxdn][lich]") {
    const RFChannelType::E rfctValues[] = {
        RFChannelType::RCCH,
        RFChannelType::RTCH,
        RFChannelType::RDCH
    };

    for (auto rfct : rfctValues) {
        uint8_t data[NXDN_FRAME_LENGTH_BYTES + 2U];
        ::memset(data, 0x00U, sizeof(data));

        LICH lich;
        lich.setRFCT(rfct);
        lich.setFCT(FuncChannelType::USC_SACCH_NS);
        lich.setOption(ChOption::DATA_NORMAL);
        lich.setOutbound(true);

        lich.encode(data);

        LICH decoded;
        REQUIRE(decoded.decode(data));
        REQUIRE(decoded.getRFCT() == rfct);
    }
}

TEST_CASE("LICH preserves all FuncChannelType values", "[nxdn][lich]") {
    const FuncChannelType::E fctValues[] = {
        FuncChannelType::CAC_OUTBOUND,
        FuncChannelType::CAC_INBOUND_LONG,
        FuncChannelType::CAC_INBOUND_SHORT,
        FuncChannelType::USC_SACCH_NS,
        FuncChannelType::USC_UDCH,
        FuncChannelType::USC_SACCH_SS,
        FuncChannelType::USC_SACCH_SS_IDLE
    };

    for (auto fct : fctValues) {
        uint8_t data[NXDN_FRAME_LENGTH_BYTES + 2U];
        ::memset(data, 0x00U, sizeof(data));

        LICH lich;
        lich.setRFCT(RFChannelType::RDCH);
        lich.setFCT(fct);
        lich.setOption(ChOption::DATA_NORMAL);
        lich.setOutbound(true);

        lich.encode(data);

        LICH decoded;
        REQUIRE(decoded.decode(data));
        REQUIRE(decoded.getFCT() == fct);
    }
}

TEST_CASE("LICH preserves all ChOption values", "[nxdn][lich]") {
    const ChOption::E optionValues[] = {
        ChOption::DATA_NORMAL,
        ChOption::DATA_COMMON,
        ChOption::STEAL_FACCH,
        ChOption::STEAL_FACCH1_1,
        ChOption::STEAL_FACCH1_2
    };

    for (auto option : optionValues) {
        uint8_t data[NXDN_FRAME_LENGTH_BYTES + 2U];
        ::memset(data, 0x00U, sizeof(data));

        LICH lich;
        lich.setRFCT(RFChannelType::RDCH);
        lich.setFCT(FuncChannelType::USC_SACCH_NS);
        lich.setOption(option);
        lich.setOutbound(true);

        lich.encode(data);

        LICH decoded;
        REQUIRE(decoded.decode(data));
        REQUIRE(decoded.getOption() == option);
    }
}

TEST_CASE("LICH preserves outbound flag", "[nxdn][lich]") {
    for (bool outbound : {true, false}) {
        uint8_t data[NXDN_FRAME_LENGTH_BYTES + 2U];
        ::memset(data, 0x00U, sizeof(data));

        LICH lich;
        lich.setRFCT(RFChannelType::RDCH);
        lich.setFCT(FuncChannelType::USC_SACCH_NS);
        lich.setOption(ChOption::DATA_NORMAL);
        lich.setOutbound(outbound);

        lich.encode(data);

        LICH decoded;
        REQUIRE(decoded.decode(data));
        REQUIRE(decoded.getOutbound() == outbound);
    }
}

TEST_CASE("LICH copy constructor preserves all fields", "[nxdn][lich]") {
    LICH original;
    original.setRFCT(RFChannelType::RDCH);
    original.setFCT(FuncChannelType::USC_SACCH_NS);
    original.setOption(ChOption::STEAL_FACCH);
    original.setOutbound(false);

    LICH copy(original);
    REQUIRE(copy.getRFCT() == original.getRFCT());
    REQUIRE(copy.getFCT() == original.getFCT());
    REQUIRE(copy.getOption() == original.getOption());
    REQUIRE(copy.getOutbound() == original.getOutbound());
}

TEST_CASE("LICH assignment operator preserves all fields", "[nxdn][lich]") {
    LICH original;
    original.setRFCT(RFChannelType::RCCH);
    original.setFCT(FuncChannelType::CAC_OUTBOUND);
    original.setOption(ChOption::DATA_COMMON);
    original.setOutbound(true);

    LICH assigned;
    assigned = original;
    REQUIRE(assigned.getRFCT() == original.getRFCT());
    REQUIRE(assigned.getFCT() == original.getFCT());
    REQUIRE(assigned.getOption() == original.getOption());
    REQUIRE(assigned.getOutbound() == original.getOutbound());
}

TEST_CASE("LICH golden test for voice call", "[nxdn][lich][golden]") {
    uint8_t data[NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(data, 0x00U, sizeof(data));

    LICH lich;
    lich.setRFCT(RFChannelType::RDCH);
    lich.setFCT(FuncChannelType::USC_SACCH_NS);
    lich.setOption(ChOption::STEAL_FACCH);
    lich.setOutbound(false);

    lich.encode(data);

    // Decode and verify round-trip
    LICH decoded;
    REQUIRE(decoded.decode(data));
    REQUIRE(decoded.getRFCT() == RFChannelType::RDCH);
    REQUIRE(decoded.getFCT() == FuncChannelType::USC_SACCH_NS);
    REQUIRE(decoded.getOption() == ChOption::STEAL_FACCH);
    REQUIRE(decoded.getOutbound() == false);
}
