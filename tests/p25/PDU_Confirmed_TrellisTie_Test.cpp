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
#include "common/Log.h"
#include "common/edac/CRC.h"
#include "common/p25/P25Defines.h"
#include "common/p25/data/DataBlock.h"
#include "common/p25/data/DataHeader.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::data;

#include <catch2/catch_test_macros.hpp>
#include <cstring>

/**
 * @brief Builds a confirmed user-data PDU header for decoding test blocks.
 * @return A confirmed user-data PDU header.
 */
static DataHeader confirmedHeader()
{
    DataHeader header;
    header.setFormat(PDUFormatType::CONFIRMED);
    header.setSAP(PDUSAP::USER_DATA);
    return header;
}

TEST_CASE("P25 confirmed PDU resolves an over-the-air Trellis tie", "[p25][pdu][trellis]") {
    g_logDisplayLevel = 1U;

    const uint8_t raw[4U][P25_PDU_FEC_LENGTH_BYTES] = {
        { 0x2C, 0xE3, 0x23, 0xE2, 0xEE, 0x58, 0xE2, 0x92, 0xF2, 0x93, 0x2A, 0xB4, 0xA2, 0x72, 0xF2, 0x32, 0x22, 0x7F, 0x67, 0x57, 0x2E, 0x2F, 0x2D, 0xC7, 0xF0 },
        { 0x2C, 0x73, 0xF0, 0xE4, 0xEB, 0x38, 0xB2, 0xE6, 0x72, 0x53, 0x72, 0x6E, 0x6B, 0x25, 0x12, 0xE7, 0x22, 0x26, 0x01, 0xD7, 0x31, 0xC2, 0x0D, 0x5A, 0xD0 },
        { 0x2B, 0xA9, 0x05, 0x37, 0xE2, 0xF3, 0x2D, 0x7D, 0xCB, 0x61, 0x27, 0x23, 0xF8, 0xA1, 0x80, 0x81, 0xD3, 0x7E, 0x25, 0x34, 0x34, 0xAF, 0x1A, 0xC4, 0x20 },
        { 0x26, 0x22, 0x22, 0x22, 0x22, 0xD2, 0xAD, 0xF2, 0x22, 0x22, 0x22, 0xEB, 0xE2, 0xF2, 0x22, 0x22, 0x22, 0x56, 0x9B, 0x22, 0x22, 0x22, 0x22, 0xE7, 0xF0 } };

    const uint8_t expectedBlock1[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES] = {
        0x01, 0x48, 0xAC, 0x1C, 0x00, 0x37, 0xE3, 0x58, 0x0F, 0xA1, 0x00, 0x19, 0xA5, 0x09, 0x0B, 0x0F };

    uint8_t userData[4U * P25_PDU_CONFIRMED_DATA_LENGTH_BYTES] = { 0U };
    const DataHeader header = confirmedHeader();
    for (uint32_t i = 0U; i < 4U; i++) {
        DataBlock block;
        INFO("block " << i);
        REQUIRE(block.decode(raw[i], header));
        REQUIRE(block.getSerialNo() == i);
        block.getData(userData + i * P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
    }

    REQUIRE(::memcmp(userData + P25_PDU_CONFIRMED_DATA_LENGTH_BYTES, expectedBlock1,
        P25_PDU_CONFIRMED_DATA_LENGTH_BYTES) == 0);
    REQUIRE(edac::CRC::checkCRC32(userData, sizeof(userData)));
}

TEST_CASE("P25 confirmed PDU rejects corruption without a CRC-valid best candidate", "[p25][pdu][trellis]") {
    uint8_t payload[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES] = { 0U };
    DataBlock encodedBlock;
    encodedBlock.setFormat(PDUFormatType::CONFIRMED);
    encodedBlock.setSerialNo(3U);
    encodedBlock.setData(payload);

    uint8_t encoded[P25_PDU_FEC_LENGTH_BYTES] = { 0U };
    encodedBlock.encode(encoded);
    for (uint32_t i = 0U; i < sizeof(encoded); i++)
        encoded[i] ^= (uint8_t)(0x5AU + i * 17U);

    DataBlock decodedBlock;
    REQUIRE_FALSE(decodedBlock.decode(encoded, confirmedHeader()));
}
