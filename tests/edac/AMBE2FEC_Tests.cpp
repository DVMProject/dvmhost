// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */

#include "common/edac/AMBE2FEC.h"
#include "common/edac/Golay24128.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>

using namespace edac;

TEST_CASE("AMBE2FEC Annex S interleave round trip", "[edac][ambe2][p25][p2]")
{
    AMBE2FEC fec;
    uint8_t codeword[9U] = {0xA5U, 0x5AU, 0x13U, 0xC7U, 0xE1U, 0x42U, 0x89U, 0x3CU, 0x80U};
    uint8_t frame[9U] = {0U};
    uint8_t decoded[9U] = {0U};

    fec.interleave(frame, codeword);
    fec.deinterleave(decoded, frame);

    REQUIRE(std::memcmp(decoded, codeword, sizeof(codeword)) == 0);
}

TEST_CASE("AMBE2FEC corrects the half-rate Golay codewords", "[edac][ambe2][p25][p2]")
{
    AMBE2FEC fec;
    uint8_t codeword[9U] = {0U};
    uint8_t frame[9U] = {0U};
    uint8_t decoded[9U] = {0U};

    fec.encode(codeword, 0xA5BU, 0x5A3U, 0x3A5U, 0x2D37U);
    fec.interleave(frame, codeword);
    fec.deinterleave(decoded, frame);

    decoded[0U] ^= 0x01U;
    decoded[3U] ^= 0x04U;

    REQUIRE(fec.regenerate(decoded) > 0U);
    for (uint32_t i = 0U; i < sizeof(codeword); i++)
        REQUIRE(decoded[i] == codeword[i]);
}

TEST_CASE("AMBE2FEC extracts and repairs 4V/2V burst voice fields", "[edac][ambe2][p25][p2]")
{
    AMBE2FEC fec;
    uint8_t burst[40U] = {0U};
    uint8_t frame[9U] = {0U};
    uint8_t codeword[9U] = {0U};

    fec.encode(codeword, 0xA5BU, 0x5A3U, 0x3A5U, 0x2D37U);
    fec.interleave(frame, codeword);
    for (uint32_t bit = 0U; bit < 72U; bit++)
        burst[(2U + bit) >> 3U] |= static_cast<uint8_t>(((frame[bit >> 3U] >> (7U - (bit & 7U))) & 1U) << (7U - ((2U + bit) & 7U)));

    fec.encode(codeword, 0x321U, 0x654U, 0x2AAU, 0x1555U);
    fec.interleave(frame, codeword);
    for (uint32_t bit = 0U; bit < 72U; bit++)
        burst[(76U + bit) >> 3U] |= static_cast<uint8_t>(((frame[bit >> 3U] >> (7U - (bit & 7U))) & 1U) << (7U - ((76U + bit) & 7U)));

    uint8_t cleanBurst[40U] = {0U};
    ::memcpy(cleanBurst, burst, sizeof(cleanBurst));
    uint8_t roundTrip[40U] = {0U};
    ::memcpy(roundTrip, burst, sizeof(roundTrip));
    fec.regenerateBurst(roundTrip, false, false);
    REQUIRE(std::memcmp(roundTrip, cleanBurst, sizeof(roundTrip)) == 0);
    burst[1U] ^= 0x20U;
    burst[10U] ^= 0x08U;
    fec.regenerateBurst(burst, false, false);
    for (uint32_t i = 0U; i < sizeof(burst); i++) {
        INFO("byte " << i);
        REQUIRE(burst[i] == cleanBurst[i]);
    }
}

TEST_CASE("AMBE2FEC extracts inbound 4V/2V voice fields", "[edac][ambe2][p25][p2]")
{
    AMBE2FEC fec;
    uint8_t burst[40U] = {0U};
    uint8_t frame[9U] = {0U};
    uint8_t codeword[9U] = {0U};
    fec.encode(codeword, 0x6A5U, 0x321U, 0x155U, 0x2AAAU);
    fec.interleave(frame, codeword);

    for (uint32_t base : {0U, 74U}) {
        for (uint32_t bit = 0U; bit < 72U; bit++) {
            uint32_t source = (frame[bit >> 3U] >> (7U - (bit & 7U))) & 1U;
            uint32_t target = base + bit;
            burst[target >> 3U] |= static_cast<uint8_t>(source << (7U - (target & 7U)));
        }
    }

    uint8_t cleanBurst[40U] = {0U};
    ::memcpy(cleanBurst, burst, sizeof(cleanBurst));
    burst[2U] ^= 0x04U;
    fec.regenerateBurst(burst, true, false);
    REQUIRE(std::memcmp(burst, cleanBurst, sizeof(burst)) == 0);
}

TEST_CASE("AMBE2FEC repairs all four 4V voice codewords", "[edac][ambe2][p25][p2]")
{
    AMBE2FEC fec;
    uint8_t burst[40U] = {0U};
    const uint32_t offsets[4U] = {2U, 76U, 176U, 250U};
    const uint16_t values[4U][4U] = {
        {0xA5BU, 0x5A3U, 0x3A5U, 0x2D37U},
        {0x321U, 0x654U, 0x2AAU, 0x1555U},
        {0x777U, 0x111U, 0x155U, 0x2AAAU},
        {0x12AU, 0x345U, 0x3CCU, 0x1B6DU}
    };

    for (uint32_t frameNo = 0U; frameNo < 4U; frameNo++) {
        uint8_t codeword[9U] = {0U};
        uint8_t frame[9U] = {0U};
        fec.encode(codeword, values[frameNo][0U], values[frameNo][1U], values[frameNo][2U], values[frameNo][3U]);
        fec.interleave(frame, codeword);
        for (uint32_t bit = 0U; bit < 72U; bit++) {
            uint32_t value = (frame[bit >> 3U] >> (7U - (bit & 7U))) & 1U;
            uint32_t target = offsets[frameNo] + bit;
            burst[target >> 3U] |= static_cast<uint8_t>(value << (7U - (target & 7U)));
        }
    }

    uint8_t clean[40U] = {0U};
    ::memcpy(clean, burst, sizeof(clean));
    burst[3U] ^= 0x10U;
    burst[22U] ^= 0x04U;
    burst[34U] ^= 0x20U;
    fec.regenerateBurst(burst, false, true);
    REQUIRE(std::memcmp(burst, clean, sizeof(burst)) == 0);
}