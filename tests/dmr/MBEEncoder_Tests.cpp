// SPDX-License-Identifier: GPL-2.0-only
#include "vocoder/MBEEncoder.h"
#include "vocoder/MBEDecoder.h"

#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <memory>

TEST_CASE("Half-rate FEC consumes exactly 49 source bits", "[dmr][vocoder][bounds]") {
    vocoder::MBEEncoder encoder(vocoder::ENCODE_DMR_AMBE);
    vocoder::MBEDecoder decoder(vocoder::DECODE_DMR_AMBE);

    // Independent vectors from Console's existing native FEC runtime, with a zero-padded input.
    const uint8_t known[2U][9U] = {
        {0x22U, 0x00U, 0x02U, 0x04U, 0x02U, 0x20U, 0x40U, 0x44U, 0x00U},
        {0xDDU, 0xDDU, 0xFFU, 0xFFU, 0xDFU, 0xBFU, 0xBBU, 0xFFU, 0xBBU}
    };
    for (uint32_t pattern = 0U; pattern < 51U; pattern++) {
        // An exact heap allocation makes a one-past-end read visible to ASAN.
        auto bits = std::make_unique<uint8_t[]>(49U);
        for (uint32_t i = 0U; i < 49U; i++)
            bits[i] = pattern == 1U || (pattern >= 2U && i == pattern - 2U) ? 1U : 0U;
        uint8_t output[11U] = {};
        output[0U] = output[10U] = 0xA5U;
        encoder.encodeBits(bits.get(), output + 1U);
        REQUIRE(output[0U] == 0xA5U);
        REQUIRE(output[10U] == 0xA5U);
        if (pattern < 2U)
            REQUIRE(::memcmp(output + 1U, known[pattern], 9U) == 0);
        char decoded[49U] = {};
        REQUIRE(decoder.decodeBits(output + 1U, decoded) == 0);
        for (uint32_t i = 0U; i < 49U; i++)
            REQUIRE(decoded[i] == bits[i]);

        // Data beyond the API's 49 source entries must not change the last valid bit.
        uint8_t padded[72U];
        ::memset(padded, 0xFFU, sizeof(padded));
        ::memcpy(padded, bits.get(), 49U);
        uint8_t repeated[9U];
        encoder.encodeBits(padded, repeated);
        REQUIRE(::memcmp(output + 1U, repeated, 9U) == 0);
    }
}
