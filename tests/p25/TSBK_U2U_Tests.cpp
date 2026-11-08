// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 */
#include "host/Defines.h"
#include "common/edac/CRC.h"
#include "common/p25/P25Defines.h"
#include "common/p25/lc/tsbk/TSBKFactory.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <catch2/catch_test_macros.hpp>

TEST_CASE("TSBK unit-to-unit answer request and response layouts", "[p25][tsbk][u2u]")
{
    const uint32_t callingUnit = 0x543210U;
    const uint32_t calledUnit = 0x123456U;

    SECTION("OSP answer request omits the ISP answer-response octet") {
        IOSP_UU_ANS request;
        request.setPriority(4U);
        request.setSrcId(callingUnit);
        request.setDstId(calledUnit);

        uint8_t raw[P25_TSBK_LENGTH_BYTES];
        request.encode(raw, true, true);

        REQUIRE((raw[0U] & 0x3FU) == TSBKO::IOSP_UU_ANS);
        REQUIRE(raw[1U] == MFG_STANDARD);
        REQUIRE(raw[2U] == 0x04U);                 // Service options
        REQUIRE(raw[3U] == 0x00U);                 // Reserved in the OSP request
        REQUIRE(raw[4U] == 0x12U);                 // Target address
        REQUIRE(raw[5U] == 0x34U);
        REQUIRE(raw[6U] == 0x56U);
        REQUIRE(raw[7U] == 0x54U);                 // Source address
        REQUIRE(raw[8U] == 0x32U);
        REQUIRE(raw[9U] == 0x10U);
    }

    SECTION("ISP answer response decodes responder as source and caller as target") {
        uint8_t raw[P25_TSBK_LENGTH_BYTES] = { 0U };
        raw[0U] = 0x80U | TSBKO::IOSP_UU_ANS;
        raw[1U] = MFG_STANDARD;
        raw[2U] = 0x04U;                           // Service options
        raw[3U] = ResponseCode::ANS_PROCEED;
        raw[4U] = 0x54U;                           // Target: original caller
        raw[5U] = 0x32U;
        raw[6U] = 0x10U;
        raw[7U] = 0x12U;                           // Source: responding called unit
        raw[8U] = 0x34U;
        raw[9U] = 0x56U;
        edac::CRC::addCCITT162(raw, P25_TSBK_LENGTH_BYTES);

        std::unique_ptr<TSBK> tsbk = TSBKFactory::createTSBK(raw, true);
        REQUIRE(tsbk != nullptr);

        IOSP_UU_ANS* response = dynamic_cast<IOSP_UU_ANS*>(tsbk.get());
        REQUIRE(response != nullptr);
        REQUIRE(response->getResponse() == ResponseCode::ANS_PROCEED);
        REQUIRE(response->getPriority() == 4U);
        REQUIRE(response->getSrcId() == calledUnit);
        REQUIRE(response->getDstId() == callingUnit);
    }
}
