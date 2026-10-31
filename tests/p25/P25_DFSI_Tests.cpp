// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "host/Defines.h"
#include "common/p25/P25Defines.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/p25/dfsi/frames/StartOfStream.h"
#include "common/p25/dfsi/frames/FullRateVoice.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>

using namespace p25;
using namespace p25::defines;
using namespace p25::dfsi;
using namespace p25::dfsi::defines;
using namespace p25::dfsi::frames;

TEST_CASE("DFSI StartOfStream and FullRateVoice encode/decode round-trips", "[p25][dfsi][frames]")
{
    SECTION("StartOfStream preserves NID and low nibble error count") {
        StartOfStream tx;
        tx.setNID(0xA55AU);
        tx.setErrorCount(0x1DU);

        uint8_t buffer[StartOfStream::LENGTH] = { 0U };
        tx.encode(buffer);

        StartOfStream rx;
        REQUIRE(rx.decode(buffer));

        REQUIRE(rx.getNID() == 0xA55AU);
        REQUIRE(rx.getErrorCount() == 0x0DU);
    }

    SECTION("FullRateVoice LDU1 voice3 includes four additional bytes") {
        FullRateVoice tx;
        tx.setFrameType(DFSIFrameType::LDU1_VOICE3);
        tx.setTotalErrors(5U);
        tx.setMuteFrame(true);
        tx.setLostFrame(false);
        tx.setSuperframeCnt(2U);
        tx.setBusy(DFSI_BUSY_BITS_BUSY);

        for (uint8_t i = 0U; i < FullRateVoice::IMBE_BUF_LEN; i++)
            tx.imbeData[i] = (uint8_t)(0x20U + i);

        tx.additionalData[0U] = 0x11U;
        tx.additionalData[1U] = 0x22U;
        tx.additionalData[2U] = 0x33U;
        tx.additionalData[3U] = 0x44U;

        uint8_t buffer[FullRateVoice::LENGTH] = { 0U };
        tx.encode(buffer);

        FullRateVoice rx;
        REQUIRE(rx.decode(buffer));

        REQUIRE(rx.getFrameType() == DFSIFrameType::LDU1_VOICE3);
        REQUIRE(rx.getLength() == FullRateVoice::LENGTH);
        REQUIRE(rx.getTotalErrors() == 5U);
        REQUIRE(rx.getMuteFrame() == true);
        REQUIRE(rx.getLostFrame() == false);
        REQUIRE(rx.getSuperframeCnt() == 2U);
        REQUIRE(rx.getBusy() == DFSI_BUSY_BITS_BUSY);
        REQUIRE(::memcmp(rx.imbeData, tx.imbeData, FullRateVoice::IMBE_BUF_LEN) == 0);
        REQUIRE(::memcmp(rx.additionalData, tx.additionalData, FullRateVoice::ADDITIONAL_LENGTH) == 0);
    }

    SECTION("FullRateVoice LDU1 voice9 uses three-byte additional payload") {
        FullRateVoice tx;
        tx.setFrameType(DFSIFrameType::LDU1_VOICE9);
        tx.setTotalErrors(1U);
        tx.setMuteFrame(false);
        tx.setLostFrame(true);
        tx.setSuperframeCnt(1U);
        tx.setBusy(DFSI_BUSY_BITS_INBOUND);

        for (uint8_t i = 0U; i < FullRateVoice::IMBE_BUF_LEN; i++)
            tx.imbeData[i] = (uint8_t)(0x40U + i);

        tx.additionalData[0U] = 0xAAU;
        tx.additionalData[1U] = 0xBBU;
        tx.additionalData[2U] = 0xCCU;
        tx.additionalData[3U] = 0xDDU;

        uint8_t buffer[FullRateVoice::LENGTH_918] = { 0U };
        tx.encode(buffer);

        FullRateVoice rx;
        REQUIRE(rx.decode(buffer));

        REQUIRE(rx.getFrameType() == DFSIFrameType::LDU1_VOICE9);
        REQUIRE(rx.getLength() == FullRateVoice::LENGTH_918);
        REQUIRE(rx.additionalData != nullptr);
        REQUIRE(rx.additionalData[0U] == 0xAAU);
        REQUIRE(rx.additionalData[1U] == 0xBBU);
        REQUIRE(rx.additionalData[2U] == 0xCCU);
        REQUIRE(rx.additionalData[3U] == 0x00U);
    }

    SECTION("FullRateVoice LDU1 voice2 has no additional payload") {
        FullRateVoice tx;
        tx.setFrameType(DFSIFrameType::LDU1_VOICE2);
        tx.setTotalErrors(7U);
        tx.setMuteFrame(false);
        tx.setLostFrame(false);
        tx.setSuperframeCnt(3U);
        tx.setBusy(DFSI_BUSY_BITS_IDLE);

        for (uint8_t i = 0U; i < FullRateVoice::IMBE_BUF_LEN; i++)
            tx.imbeData[i] = (uint8_t)(0x60U + i);

        uint8_t buffer[FullRateVoice::LENGTH_121011] = { 0U };
        tx.encode(buffer);

        FullRateVoice rx;
        REQUIRE(rx.decode(buffer));

        REQUIRE(rx.getFrameType() == DFSIFrameType::LDU1_VOICE2);
        REQUIRE(rx.getLength() == FullRateVoice::LENGTH_121011);
        REQUIRE(rx.additionalData == nullptr);
    }
}

TEST_CASE("DFSI PDU wire lengths follow known captures", "[p25][dfsi][pdu]")
{
    constexpr uint32_t headerPrefix = DFSI_MOT_START_LEN +
        P25_PDU_HEADER_LENGTH_BYTES + DFSI_PDU_RESERVED_LENGTH_BYTES;

    REQUIRE(DFSI_MAX_PDU_BLOCKS == 33U);
    REQUIRE(DFSI_PDU_UNCONFIRMED_BLOCK_LENGTH_BYTES == 13U);
    REQUIRE(DFSI_PDU_CONFIRMED_BLOCK_LENGTH_BYTES == 18U);

    // HDEND12/18: embedded start, header + SQ, three blocks.
    REQUIRE(headerPrefix + (3U * DFSI_PDU_UNCONFIRMED_BLOCK_LENGTH_BYTES) +
        DFSI_PDU_END_METADATA_LENGTH_BYTES == 63U);
    REQUIRE(headerPrefix + (3U * DFSI_PDU_CONFIRMED_BLOCK_LENGTH_BYTES) +
        DFSI_PDU_END_METADATA_LENGTH_BYTES == 78U);

    // HDR12/18: embedded start, header, three blocks.
    REQUIRE(headerPrefix + (3U * DFSI_PDU_UNCONFIRMED_BLOCK_LENGTH_BYTES) + 1U == 62U);
    REQUIRE(headerPrefix + (3U * DFSI_PDU_CONFIRMED_BLOCK_LENGTH_BYTES) + 1U == 77U);

    // Four-block middle and END packets.
    REQUIRE(1U + (4U * DFSI_PDU_UNCONFIRMED_BLOCK_LENGTH_BYTES) + 1U == 54U);
    REQUIRE(1U + (4U * DFSI_PDU_CONFIRMED_BLOCK_LENGTH_BYTES) + 1U == 74U);
    REQUIRE(1U + (4U * DFSI_PDU_UNCONFIRMED_BLOCK_LENGTH_BYTES) +
        DFSI_PDU_END_METADATA_LENGTH_BYTES == 55U);
    REQUIRE(1U + (4U * DFSI_PDU_CONFIRMED_BLOCK_LENGTH_BYTES) +
        DFSI_PDU_END_METADATA_LENGTH_BYTES == 75U);
}
