// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */

#include "common/Log.h"
#include "common/Utils.h"

#include <catch2/catch_test_macros.hpp>
#include <cstring>

#include "common/nxdn/lc/RTCH.h"
#include "common/nxdn/NXDNDefines.h"

using namespace nxdn;
using namespace nxdn::defines;
using namespace nxdn::lc;

TEST_CASE("RTCH encodes and decodes voice call", "[nxdn][rtch]") {
    uint8_t data[NXDN_RTCH_LC_LENGTH_BYTES];
    ::memset(data, 0x00U, sizeof(data));

    RTCH rtch;
    rtch.setMessageType(MessageType::RTCH_VCALL);
    rtch.setSrcId(12345U);
    rtch.setDstId(54321U);
    rtch.setEmergency(false);
    rtch.setPriority(false);
    rtch.setDuplex(true);
    rtch.setTransmissionMode(TransmissionMode::MODE_4800);

    rtch.encode(data, NXDN_RTCH_LC_LENGTH_BITS);

    // Decode and verify
    RTCH decoded;
    decoded.decode(data, NXDN_RTCH_LC_LENGTH_BITS);
    REQUIRE(decoded.getMessageType() == MessageType::RTCH_VCALL);
    REQUIRE(decoded.getSrcId() == 12345U);
    REQUIRE(decoded.getDstId() == 54321U);
    REQUIRE(decoded.getEmergency() == false);
}

TEST_CASE("RTCH preserves all MessageType values", "[nxdn][rtch]") {
    const uint8_t messageTypes[] = {
        MessageType::RTCH_VCALL,
        MessageType::RTCH_VCALL_IV,
        MessageType::RTCH_TX_REL,
        MessageType::RTCH_TX_REL_EX,
        MessageType::RTCH_DCALL_HDR,
        MessageType::RTCH_DCALL_DATA
    };

    for (auto messageType : messageTypes) {
        uint8_t data[NXDN_RTCH_LC_LENGTH_BYTES];
        ::memset(data, 0x00U, sizeof(data));

        RTCH rtch;
        rtch.setMessageType(messageType);
        rtch.setSrcId(1234U);
        rtch.setDstId(5678U);

        rtch.encode(data, NXDN_RTCH_LC_LENGTH_BITS);

        RTCH decoded;
        decoded.decode(data, NXDN_RTCH_LC_LENGTH_BITS);
        REQUIRE(decoded.getMessageType() == messageType);
    }
}

TEST_CASE("RTCH preserves source and destination IDs", "[nxdn][rtch]") {
    const uint32_t testIds[] = {0U, 1U, 255U, 1000U, 32767U, 65535U};

    for (auto srcId : testIds) {
        for (auto dstId : testIds) {
            uint8_t data[NXDN_RTCH_LC_LENGTH_BYTES];
            ::memset(data, 0x00U, sizeof(data));

            RTCH rtch;
            rtch.setMessageType(MessageType::RTCH_VCALL);
            rtch.setSrcId(srcId);
            rtch.setDstId(dstId);

            rtch.encode(data, NXDN_RTCH_LC_LENGTH_BITS);

            RTCH decoded;
            decoded.decode(data, NXDN_RTCH_LC_LENGTH_BITS);
            REQUIRE(decoded.getSrcId() == srcId);
            REQUIRE(decoded.getDstId() == dstId);
        }
    }
}

TEST_CASE("RTCH preserves emergency flag", "[nxdn][rtch]") {
    for (bool isEmergency : {true, false}) {
        uint8_t data[NXDN_RTCH_LC_LENGTH_BYTES];
        ::memset(data, 0x00U, sizeof(data));

        RTCH rtch;
        rtch.setMessageType(MessageType::RTCH_VCALL);
        rtch.setSrcId(100U);
        rtch.setDstId(200U);
        rtch.setEmergency(isEmergency);

        rtch.encode(data, NXDN_RTCH_LC_LENGTH_BITS);

        RTCH decoded;
        decoded.decode(data, NXDN_RTCH_LC_LENGTH_BITS);
        REQUIRE(decoded.getEmergency() == isEmergency);
    }
}

TEST_CASE("RTCH preserves duplex flag", "[nxdn][rtch]") {
    for (bool isDuplex : {true, false}) {
        uint8_t data[NXDN_RTCH_LC_LENGTH_BYTES];
        ::memset(data, 0x00U, sizeof(data));

        RTCH rtch;
        rtch.setMessageType(MessageType::RTCH_VCALL);
        rtch.setSrcId(100U);
        rtch.setDstId(200U);
        rtch.setDuplex(isDuplex);

        rtch.encode(data, NXDN_RTCH_LC_LENGTH_BITS);

        RTCH decoded;
        decoded.decode(data, NXDN_RTCH_LC_LENGTH_BITS);
        REQUIRE(decoded.getDuplex() == isDuplex);
    }
}

TEST_CASE("RTCH preserves transmission mode", "[nxdn][rtch]") {
    const uint8_t transmissionModes[] = {
        TransmissionMode::MODE_4800,
        TransmissionMode::MODE_9600
    };

    for (auto mode : transmissionModes) {
        uint8_t data[NXDN_RTCH_LC_LENGTH_BYTES];
        ::memset(data, 0x00U, sizeof(data));

        RTCH rtch;
        rtch.setMessageType(MessageType::RTCH_VCALL);
        rtch.setSrcId(100U);
        rtch.setDstId(200U);
        rtch.setTransmissionMode(mode);

        rtch.encode(data, NXDN_RTCH_LC_LENGTH_BITS);

        RTCH decoded;
        decoded.decode(data, NXDN_RTCH_LC_LENGTH_BITS);
        REQUIRE(decoded.getTransmissionMode() == mode);
    }
}

TEST_CASE("RTCH copy constructor preserves all fields", "[nxdn][rtch]") {
    RTCH original;
    original.setMessageType(MessageType::RTCH_VCALL);
    original.setSrcId(11111U);
    original.setDstId(22222U);
    original.setGroup(true);
    original.setEmergency(true);
    original.setEncrypted(false);
    original.setPriority(true);

    RTCH copy(original);
    REQUIRE(copy.getMessageType() == original.getMessageType());
    REQUIRE(copy.getSrcId() == original.getSrcId());
    REQUIRE(copy.getDstId() == original.getDstId());
    REQUIRE(copy.getGroup() == original.getGroup());
    REQUIRE(copy.getEmergency() == original.getEmergency());
    REQUIRE(copy.getEncrypted() == original.getEncrypted());
}

TEST_CASE("RTCH assignment operator preserves all fields", "[nxdn][rtch]") {
    RTCH original;
    original.setMessageType(MessageType::RTCH_TX_REL);
    original.setSrcId(9999U);
    original.setDstId(8888U);
    original.setGroup(false);
    original.setEmergency(false);
    original.setEncrypted(true);

    RTCH assigned;
    assigned = original;
    REQUIRE(assigned.getMessageType() == original.getMessageType());
    REQUIRE(assigned.getSrcId() == original.getSrcId());
    REQUIRE(assigned.getDstId() == original.getDstId());
    REQUIRE(assigned.getGroup() == original.getGroup());
    REQUIRE(assigned.getEmergency() == original.getEmergency());
    REQUIRE(assigned.getEncrypted() == original.getEncrypted());
}
