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
#include "common/nxdn/lc/rcch/MESSAGE_TYPE_VCALL_ASSGN.h"
#include "common/nxdn/lc/rcch/MESSAGE_TYPE_VCALL_CONN.h"
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

TEST_CASE("RCCH voice response decodes its on-air cause", "[nxdn][rcch][u2u]")
{
    uint8_t data[NXDN_RCCH_LC_LENGTH_BYTES + 2U] = {};
    MESSAGE_TYPE_VCALL_CONN response;
    response.setMessageType(MessageType::RCCH_VCALL_REC);
    response.setCallType(CallType::INDIVIDUAL);
    response.setSrcId(1002U);
    response.setDstId(1001U);
    response.setCauseResponse(CauseResponse::VD_QUE_TGT_UNIT_BUSY);
    response.encode(data, NXDN_RCCH_LC_LENGTH_BITS);

    REQUIRE(data[7U] == CauseResponse::VD_QUE_TGT_UNIT_BUSY);
    std::unique_ptr<RCCH> decoded = RCCHFactory::createRCCH(
        data, NXDN_RCCH_LC_LENGTH_BITS);
    REQUIRE(decoded != nullptr);
    CHECK(decoded->getCauseResponse() == CauseResponse::VD_QUE_TGT_UNIT_BUSY);
}

TEST_CASE("RCCH voice assignment uses all ten channel bits", "[nxdn][rcch][grant]")
{
    for (const uint16_t channel : { 255U, 256U, 511U, 512U, 1023U }) {
        uint8_t data[NXDN_RCCH_LC_LENGTH_BYTES + 2U] = {};
        MESSAGE_TYPE_VCALL_ASSGN assignment;
        assignment.setCallType(CallType::INDIVIDUAL);
        assignment.setSrcId(1001U);
        assignment.setDstId(1002U);
        assignment.setGrpVchNo(channel);
        assignment.encode(data, NXDN_RCCH_LC_LENGTH_BITS);

        CHECK((data[7U] & 0x03U) == ((channel >> 8U) & 0x03U));
        CHECK(data[8U] == (channel & 0xFFU));

        MESSAGE_TYPE_VCALL_ASSGN decoded;
        decoded.decode(data, NXDN_RCCH_LC_LENGTH_BITS);
        CHECK(decoded.getGrpVchNo() == channel);
    }
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

TEST_CASE("RCCH voice call request preserves individual and group classification", "[nxdn][rcch][u2u]")
{
    for (const auto callType : { CallType::INDIVIDUAL, CallType::CONFERENCE }) {
        uint8_t data[NXDN_RCCH_LC_LENGTH_BYTES + 2U] = {};

        MESSAGE_TYPE_VCALL_CONN request;
        request.setMessageType(MessageType::RTCH_VCALL);
        request.setCallType(callType);
        request.setSrcId(1001U);
        request.setDstId(2001U);
        request.encode(data, NXDN_RCCH_LC_LENGTH_BITS);

        std::unique_ptr<RCCH> decoded = RCCHFactory::createRCCH(
            data, NXDN_RCCH_LC_LENGTH_BITS);
        REQUIRE(decoded != nullptr);
        CHECK(decoded->getCallType() == callType);
        CHECK(decoded->getGroup() == (callType != CallType::INDIVIDUAL));
        CHECK(decoded->getSrcId() == 1001U);
        CHECK(decoded->getDstId() == 2001U);
    }
}
