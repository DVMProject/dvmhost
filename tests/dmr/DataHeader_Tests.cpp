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

TEST_CASE("DataHeader encodes and decodes defined raw data", "[dmr][dataheader]") {
    uint8_t frame[DMR_FRAME_LENGTH_BYTES + 2U];
    ::memset(frame, 0x00U, sizeof(frame));

    DataHeader hdr;
    hdr.setDPF(DPF::DEFINED_RAW);
    hdr.setSAP(0x03U);
    hdr.setGI(true);
    hdr.setSrcId(1234U);
    hdr.setDstId(5678U);
    hdr.setBlocksToFollow(2U);
    hdr.setFullMesage(true);
    hdr.setSynchronize(false);
    hdr.setSrcPort(3U);
    hdr.setDstPort(5U);

    hdr.encode(frame + 2U);

    DataHeader decoded;
    REQUIRE(decoded.decode(frame + 2U));
    REQUIRE(decoded.getDPF() == DPF::DEFINED_RAW);
    REQUIRE(decoded.getSrcPort() == 3U);
    REQUIRE(decoded.getDstPort() == 5U);
    REQUIRE(decoded.getFullMesage() == true);
    REQUIRE(decoded.getSynchronize() == false);
}

TEST_CASE("DataHeader copy and assignment preserve fields", "[dmr][dataheader]") {
    DataHeader original;
    original.setDPF(DPF::CONFIRMED_DATA);
    original.setSAP(0x0AU);
    original.setGI(false);
    original.setSrcId(111U);
    original.setDstId(222U);
    original.setBlocksToFollow(4U);
    original.setFullMesage(true);
    original.setSynchronize(true);
    original.setNs(5U);
    original.setFSN(9U);

    DataHeader copied(original);
    REQUIRE(copied.getDPF() == original.getDPF());
    REQUIRE(copied.getSAP() == original.getSAP());
    REQUIRE(copied.getGI() == original.getGI());
    REQUIRE(copied.getSrcId() == original.getSrcId());
    REQUIRE(copied.getDstId() == original.getDstId());
    REQUIRE(copied.getBlocksToFollow() == original.getBlocksToFollow());
    REQUIRE(copied.getFullMesage() == original.getFullMesage());
    REQUIRE(copied.getSynchronize() == original.getSynchronize());
    REQUIRE(copied.getNs() == original.getNs());
    REQUIRE(copied.getFSN() == original.getFSN());

    DataHeader assigned;
    assigned = original;
    REQUIRE(assigned.getDPF() == original.getDPF());
    REQUIRE(assigned.getSAP() == original.getSAP());
    REQUIRE(assigned.getGI() == original.getGI());
    REQUIRE(assigned.getSrcId() == original.getSrcId());
    REQUIRE(assigned.getDstId() == original.getDstId());
}

TEST_CASE("DataHeader preserves all six defined-short data format bits", "[dmr][dataheader]") {
    const uint8_t formats[] = {0x01U, 0x15U, 0x2AU, 0x3FU};

    for (uint8_t format : formats) {
        uint8_t frame[DMR_FRAME_LENGTH_BYTES] = {};
        DataHeader header;
        header.setDPF(DPF::DEFINED_SHORT);
        header.setSAP(0x0AU);
        header.setSrcId(123U);
        header.setDstId(456U);
        header.setBlocksToFollow(1U);
        header.setDataFormat(format);
        header.encode(frame);

        DataHeader decoded;
        REQUIRE(decoded.decode(frame));
        REQUIRE(decoded.getDataFormat() == format);
    }
}
