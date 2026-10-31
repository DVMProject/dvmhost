// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Charles Bricker, K3NOP
 *
 */
#include "host/Defines.h"
#include "common/p25/lc/tsbk/TSBKFactory.h"
#include "common/p25/P25Defines.h"
#include "common/edac/CRC.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <catch2/catch_test_macros.hpp>

TEST_CASE("TSBK Telephone Interconnect", "[p25][tsbk]") {
    SECTION("OSP_TELE_INT_VCH_GRANT_Layout") {
        OSP_TELE_INT_VCH_GRANT osp;
        osp.setPriority(4U);
        osp.setGrpVchId(1U);
        osp.setGrpVchNo(0x123U);
        osp.setCallTimer(600U);
        osp.setSrcId(0x123456U);

        uint8_t raw[P25_TSBK_LENGTH_BYTES];
        osp.encode(raw, true, true);

        REQUIRE((raw[0U] & 0x3FU) == TSBKO::IOSP_TELE_INT_DIAL);
        REQUIRE(raw[2U] == 0x04U);                  // Service Options
        REQUIRE(raw[3U] == 0x11U);                  // Channel ID + Channel Number (MSB)
        REQUIRE(raw[4U] == 0x23U);                  // Channel Number (LSB)
        REQUIRE(raw[5U] == 0x02U);                  // Call Timer (MSB)
        REQUIRE(raw[6U] == 0x58U);                  // Call Timer (LSB)
        REQUIRE(raw[7U] == 0x12U);                  // Address
        REQUIRE(raw[8U] == 0x34U);
        REQUIRE(raw[9U] == 0x56U);

        std::unique_ptr<TSBK> tsbk = TSBKFactory::createTSBK(raw, true);
        REQUIRE(tsbk != nullptr);

        OSP_TELE_INT_VCH_GRANT* decoded = dynamic_cast<OSP_TELE_INT_VCH_GRANT*>(tsbk.get());
        REQUIRE(decoded != nullptr);
        REQUIRE(decoded->getPriority() == 4U);
        REQUIRE(decoded->getGrpVchId() == 1U);
        REQUIRE(decoded->getGrpVchNo() == 0x123U);
        REQUIRE(decoded->getCallTimer() == 600U);
        REQUIRE(decoded->getSrcId() == 0x123456U);
        REQUIRE(decoded->getDstId() == 0x123456U);
    }

    SECTION("MBT_OSP_TELE_INT_VCH_GRANT_Layout") {
        MBT_OSP_TELE_INT_VCH_GRANT osp;
        osp.setMFId(MFG_STANDARD);
        osp.setPriority(4U);
        osp.setGrpVchId(1U);
        osp.setGrpVchNo(0x123U);
        osp.setRxGrpVchId(2U);
        osp.setRxGrpVchNo(0x456U);
        osp.setCallTimer(600U);
        osp.setSrcId(0x123456U);
        osp.setForceChannelId(true);

        data::DataHeader dataHeader = data::DataHeader();
        uint8_t pduUserData[P25_PDU_UNCONFIRMED_LENGTH_BYTES];
        ::memset(pduUserData, 0x00U, P25_PDU_UNCONFIRMED_LENGTH_BYTES);
        osp.encodeMBT(dataHeader, pduUserData);

        uint8_t header[P25_PDU_HEADER_LENGTH_BYTES];
        ::memset(header, 0x00U, P25_PDU_HEADER_LENGTH_BYTES);
        dataHeader.encode(header, true);

        REQUIRE((header[0U] & 0x1FU) == PDUFormatType::AMBT);
        REQUIRE((header[0U] & 0x20U) == 0x20U);     // Outbound
        REQUIRE((header[1U] & 0x3FU) == PDUSAP::TRUNK_CTRL);
        REQUIRE(header[2U] == MFG_STANDARD);
        REQUIRE(header[3U] == 0x12U);               // Address
        REQUIRE(header[4U] == 0x34U);
        REQUIRE(header[5U] == 0x56U);
        REQUIRE((header[6U] & 0x7FU) == 1U);        // Blocks to Follow
        REQUIRE((header[7U] & 0x3FU) == TSBKO::IOSP_TELE_INT_DIAL);
        REQUIRE(header[8U] == 0x04U);               // Service Options
        REQUIRE(header[9U] == 0x00U);               // Reserved

        REQUIRE(pduUserData[0U] == 0x11U);          // Channel (T)
        REQUIRE(pduUserData[1U] == 0x23U);
        REQUIRE(pduUserData[2U] == 0x24U);          // Channel (R)
        REQUIRE(pduUserData[3U] == 0x56U);
        REQUIRE(pduUserData[4U] == 0x02U);          // Call Timer
        REQUIRE(pduUserData[5U] == 0x58U);
        REQUIRE(pduUserData[6U] == 0x00U);          // Reserved
        REQUIRE(pduUserData[7U] == 0x00U);
        REQUIRE(edac::CRC::checkCRC32(pduUserData, P25_PDU_UNCONFIRMED_LENGTH_BYTES));
    }

    SECTION("ISP_TELE_INT_PSTN_REQ_Layout") {
        ISP_TELE_INT_PSTN_REQ isp;
        isp.setEmergency(true);
        isp.setPSTNAddress(0x07U);
        isp.setSrcId(0x00ABCDU);

        uint8_t raw[P25_TSBK_LENGTH_BYTES];
        isp.encode(raw, true, true);

        REQUIRE((raw[0U] & 0x3FU) == TSBKO::ISP_TELE_INT_PSTN_REQ);
        REQUIRE(raw[2U] == 0x80U);                  // Service Options
        REQUIRE(raw[3U] == 0x00U);                  // Reserved
        REQUIRE(raw[4U] == 0x00U);
        REQUIRE(raw[5U] == 0x00U);
        REQUIRE(raw[6U] == 0x07U);                  // PSTN Address
        REQUIRE(raw[7U] == 0x00U);                  // Source Address
        REQUIRE(raw[8U] == 0xABU);
        REQUIRE(raw[9U] == 0xCDU);

        std::unique_ptr<TSBK> tsbk = TSBKFactory::createTSBK(raw, true);
        REQUIRE(tsbk != nullptr);

        ISP_TELE_INT_PSTN_REQ* decoded = dynamic_cast<ISP_TELE_INT_PSTN_REQ*>(tsbk.get());
        REQUIRE(decoded != nullptr);
        REQUIRE(decoded->getEmergency() == true);
        REQUIRE(decoded->getPSTNAddress() == 0x07U);
        REQUIRE(decoded->getSrcId() == 0x00ABCDU);
    }

    SECTION("IOSP_TELE_INT_ANS_Request_Layout") {
        IOSP_TELE_INT_ANS osp;
        osp.setDigits("5551234");
        osp.setDstId(0x000064U);

        uint8_t raw[P25_TSBK_LENGTH_BYTES];
        osp.encode(raw, true, true);

        REQUIRE((raw[0U] & 0x3FU) == TSBKO::IOSP_TELE_INT_ANS);
        REQUIRE(raw[2U] == 0x55U);                  // Digits 1 - 2
        REQUIRE(raw[3U] == 0x51U);                  // Digits 3 - 4
        REQUIRE(raw[4U] == 0x23U);                  // Digits 5 - 6
        REQUIRE(raw[5U] == 0x4FU);                  // Digit 7 + Null Filler
        REQUIRE(raw[6U] == 0xFFU);                  // Null Filler
        REQUIRE(raw[7U] == 0x00U);                  // Target Address
        REQUIRE(raw[8U] == 0x00U);
        REQUIRE(raw[9U] == 0x64U);

        std::unique_ptr<TSBK> tsbk = TSBKFactory::createTSBK(raw, true);
        REQUIRE(tsbk != nullptr);

        IOSP_TELE_INT_ANS* decoded = dynamic_cast<IOSP_TELE_INT_ANS*>(tsbk.get());
        REQUIRE(decoded != nullptr);
        REQUIRE(decoded->getDigits() == "5551234");
        REQUIRE(decoded->getDstId() == 0x000064U);
    }

    SECTION("IOSP_TELE_INT_ANS_Request_ExtendedDigits") {
        IOSP_TELE_INT_ANS osp;
        osp.setDigits("12A*#");
        osp.setDstId(0x000001U);

        uint8_t raw[P25_TSBK_LENGTH_BYTES];
        osp.encode(raw, true, true);

        REQUIRE(raw[2U] == 0x12U);                  // 1, 2
        REQUIRE(raw[3U] == 0xF0U);                  // ESC, A
        REQUIRE(raw[4U] == 0xABU);                  // *, #

        IOSP_TELE_INT_ANS decoded;
        REQUIRE(decoded.decode(raw, true) == true);
        REQUIRE(decoded.getDigits() == "12A*#");
    }

    SECTION("IOSP_TELE_INT_ANS_Response_Decode") {
        uint8_t raw[P25_TSBK_LENGTH_BYTES];
        ::memset(raw, 0x00U, P25_TSBK_LENGTH_BYTES);

        raw[0U] = 0x80U | TSBKO::IOSP_TELE_INT_ANS; // Last Block + LCO
        raw[1U] = MFG_STANDARD;
        raw[2U] = 0x00U;                            // Service Options
        raw[3U] = ResponseCode::ANS_PROCEED;        // Answer Response
        raw[7U] = 0x00U;                            // Source Address
        raw[8U] = 0x12U;
        raw[9U] = 0x34U;
        edac::CRC::addCCITT162(raw, P25_TSBK_LENGTH_BYTES);

        std::unique_ptr<TSBK> tsbk = TSBKFactory::createTSBK(raw, true);
        REQUIRE(tsbk != nullptr);

        IOSP_TELE_INT_ANS* decoded = dynamic_cast<IOSP_TELE_INT_ANS*>(tsbk.get());
        REQUIRE(decoded != nullptr);
        REQUIRE(decoded->getResponse() == ResponseCode::ANS_PROCEED);
        REQUIRE(decoded->getSrcId() == 0x001234U);
    }
}
