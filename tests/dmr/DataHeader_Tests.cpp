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

#include "common/dmr/DMRDefines.h"
#include "common/dmr/data/DataHeader.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::data;

TEST_CASE("DataHeader encodes and decodes UDT data", "[dmr][dataheader]") {
    uint8_t frame[DMR_FRAME_LENGTH_BYTES + 2U];
    ::memset(frame, 0x00U, sizeof(frame));

    DataHeader hdr;
    hdr.setDPF(DPF::UDT);
    hdr.setSAP(0x01U);  // UDT SAP is 4 bits (0x0-0xF)
    hdr.setGI(false);
    hdr.setSrcId(1001U);
    hdr.setDstId(2002U);
    hdr.setBlocksToFollow(3U);  // UDT blocks to follow is 2 bits + 1 (1-4 blocks)

    hdr.encode(frame + 2U);

    // Decode and verify
    DataHeader decoded;
    REQUIRE(decoded.decode(frame + 2U));
    REQUIRE(decoded.getDPF() == DPF::UDT);
    REQUIRE(decoded.getSAP() == 0x01U);
    REQUIRE(decoded.getGI() == false);
    REQUIRE(decoded.getSrcId() == 1001U);
    REQUIRE(decoded.getDstId() == 2002U);
    REQUIRE(decoded.getBlocksToFollow() == 3U);
}

TEST_CASE("DataHeader encodes and decodes unconfirmed data", "[dmr][dataheader]") {
    uint8_t frame[DMR_FRAME_LENGTH_BYTES + 2U];
    ::memset(frame, 0x00U, sizeof(frame));

    DataHeader hdr;
    hdr.setDPF(DPF::UNCONFIRMED_DATA);
    hdr.setSAP(0x00U);  // SAP is 4 bits (0x0-0xF)
    hdr.setGI(true);
    hdr.setSrcId(5000U);
    hdr.setDstId(9999U);
    hdr.setBlocksToFollow(1U);

    hdr.encode(frame + 2U);

    // Decode and verify
    DataHeader decoded;
    REQUIRE(decoded.decode(frame + 2U));
    REQUIRE(decoded.getDPF() == DPF::UNCONFIRMED_DATA);
    REQUIRE(decoded.getSAP() == 0x00U);
    REQUIRE(decoded.getGI() == true);
    REQUIRE(decoded.getSrcId() == 5000U);
    REQUIRE(decoded.getDstId() == 9999U);
    REQUIRE(decoded.getBlocksToFollow() == 1U);
}

TEST_CASE("DataHeader handles response headers", "[dmr][dataheader]") {
    uint8_t frame[DMR_FRAME_LENGTH_BYTES + 2U];
    ::memset(frame, 0x00U, sizeof(frame));

    DataHeader hdr;
    hdr.setDPF(DPF::RESPONSE);
    hdr.setSAP(0x05U);
    hdr.setGI(false);
    hdr.setSrcId(3333U);
    hdr.setDstId(4444U);
    hdr.setResponseClass(PDUResponseClass::ACK);
    hdr.setResponseType(PDUResponseType::ACK);
    hdr.setResponseStatus(0x00U);
    hdr.setBlocksToFollow(1U);

    hdr.encode(frame + 2U);

    // Decode and verify
    DataHeader decoded;
    REQUIRE(decoded.decode(frame + 2U));
    REQUIRE(decoded.getDPF() == DPF::RESPONSE);
    REQUIRE(decoded.getResponseClass() == PDUResponseClass::ACK);
    REQUIRE(decoded.getResponseType() == PDUResponseType::ACK);
}

TEST_CASE("DataHeader preserves all SAP values", "[dmr][dataheader]") {
    uint8_t frame[DMR_FRAME_LENGTH_BYTES + 2U];

    // SAP is 4 bits, valid values are 0x0-0xF
    const uint8_t sapValues[] = {0x00U, 0x02U, 0x0AU, 0x0DU, 0x0FU};

    for (auto sap : sapValues) {
        ::memset(frame, 0x00U, sizeof(frame));
        
        DataHeader hdr;
        hdr.setDPF(DPF::UDT);
        hdr.setSAP(sap);
        hdr.setGI(true);
        hdr.setSrcId(100U);
        hdr.setDstId(200U);
        hdr.setBlocksToFollow(2U);

        hdr.encode(frame + 2U);

        DataHeader decoded;
        REQUIRE(decoded.decode(frame + 2U));
        REQUIRE(decoded.getSAP() == sap);
    }
}

TEST_CASE("DataHeader handles maximum blocks to follow", "[dmr][dataheader]") {
    uint8_t frame[DMR_FRAME_LENGTH_BYTES + 2U];
    ::memset(frame, 0x00U, sizeof(frame));

    DataHeader hdr;
    hdr.setDPF(DPF::UNCONFIRMED_DATA);  // Use UNCONFIRMED_DATA which has 7-bit blocks to follow
    hdr.setSAP(0x00U);
    hdr.setGI(true);
    hdr.setSrcId(1U);
    hdr.setDstId(1U);
    hdr.setBlocksToFollow(127U); // Max value for 7-bit field

    hdr.encode(frame + 2U);

    DataHeader decoded;
    REQUIRE(decoded.decode(frame + 2U));
    REQUIRE(decoded.getBlocksToFollow() == 127U);
}
