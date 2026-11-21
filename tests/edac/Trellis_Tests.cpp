// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */

#include "common/Log.h"
#include "common/Utils.h"

#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <random>

#include "common/edac/Trellis.h"

using namespace edac;

/**
 * @brief Shifts one dibit by one sliced 4FSK level, optionally skipping the DMR sync and embedded signaling fields.
 * @param data The burst data to modify.
 * @param symbol The dibit index to shift.
 * @param up Whether to shift the sliced level upward.
 * @param skipSymbols Whether to skip the DMR sync and embedded signaling fields.
 */
static void shiftSymbol(uint8_t* data, uint32_t symbol, bool up, bool skipSymbols = false)
{
    uint32_t firstBit = symbol * 2U;
    uint32_t secondBit = firstBit + 1U;
    if (skipSymbols && firstBit >= 98U)
        firstBit += 68U;
    if (skipSymbols && secondBit >= 98U)
        secondBit += 68U;

    const bool b1 = READ_BIT(data, firstBit) != 0U;
    const bool b2 = READ_BIT(data, secondBit) != 0U;
    int32_t level = (!b1 && b2) ? 3 : (!b1 && !b2) ? 1 : (b1 && !b2) ? -1 : -3;
    level += up ? 2 : -2;
    if (level > 3)
        level = 1;
    if (level < -3)
        level = -1;

    WRITE_BIT(data, firstBit, level < 0);
    WRITE_BIT(data, secondBit, level == 3 || level == -3);
}

TEST_CASE("Trellis 3/4 rate preserves all-zero payload", "[edac][trellis]") {
    uint8_t payload[18U];
    ::memset(payload, 0x00U, sizeof(payload));

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode34(payload, data, false);
    Utils::dump(2U, "Trellis::encode34() all zeros", data, 25U);

    uint8_t decoded[18U];
    bool result = trellis.decode34(data, decoded, false);

    REQUIRE(result);
    REQUIRE(::memcmp(decoded, payload, 18U) == 0);
}

TEST_CASE("Trellis 3/4 rate preserves all-ones payload", "[edac][trellis]") {
    uint8_t payload[18U];
    ::memset(payload, 0xFFU, sizeof(payload));

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode34(payload, data, false);
    Utils::dump(2U, "Trellis::encode34() all ones", data, 25U);

    uint8_t decoded[18U];
    bool result = trellis.decode34(data, decoded, false);

    REQUIRE(result);
    REQUIRE(::memcmp(decoded, payload, 18U) == 0);
}

TEST_CASE("Trellis 3/4 rate preserves alternating pattern", "[edac][trellis]") {
    uint8_t payload[18U];
    for (size_t i = 0; i < 18U; i++) {
        payload[i] = (i % 2 == 0) ? 0xAAU : 0x55U;
    }

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode34(payload, data, false);
    Utils::dump(2U, "Trellis::encode34() alternating", data, 25U);

    uint8_t decoded[18U];
    bool result = trellis.decode34(data, decoded, false);

    REQUIRE(result);
    REQUIRE(::memcmp(decoded, payload, 18U) == 0);
}

TEST_CASE("Trellis 3/4 rate preserves incrementing pattern", "[edac][trellis]") {
    uint8_t payload[18U];
    for (size_t i = 0; i < 18U; i++) {
        payload[i] = (uint8_t)(i * 13);
    }

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode34(payload, data, false);
    Utils::dump(2U, "Trellis::encode34() incrementing", data, 25U);

    uint8_t decoded[18U];
    bool result = trellis.decode34(data, decoded, false);

    REQUIRE(result);
    REQUIRE(::memcmp(decoded, payload, 18U) == 0);
}

TEST_CASE("Trellis 3/4 rate preserves specific pattern", "[edac][trellis]") {
    uint8_t payload[18U];
    for (size_t i = 0; i < 18U; i++) {
        payload[i] = (uint8_t)(i + 100);
    }

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode34(payload, data, false);

    uint8_t decoded[18U];
    bool result = trellis.decode34(data, decoded, false);

    REQUIRE(result);
    REQUIRE(::memcmp(decoded, payload, 18U) == 0);
}

TEST_CASE("Trellis 3/4 rate handles another pattern", "[edac][trellis]") {
    uint8_t payload[18U];
    for (size_t i = 0; i < 18U; i++) {
        payload[i] = (uint8_t)(i * 7);
    }

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode34(payload, data, false);
    Utils::dump(2U, "Trellis::encode34() pattern", data, 25U);

    uint8_t decoded[18U];
    bool result = trellis.decode34(data, decoded, false);

    REQUIRE(result);
    REQUIRE(::memcmp(decoded, payload, 18U) == 0);
}

TEST_CASE("Trellis 1/2 rate preserves all-zero payload", "[edac][trellis]") {
    uint8_t payload[12U];
    ::memset(payload, 0x00U, sizeof(payload));

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode12(payload, data);
    Utils::dump(2U, "Trellis::encode12() all zeros", data, 25U);

    uint8_t decoded[12U];
    bool result = trellis.decode12(data, decoded);

    REQUIRE(result);
    REQUIRE(::memcmp(decoded, payload, 12U) == 0);
}

TEST_CASE("Trellis 1/2 rate preserves all-ones payload", "[edac][trellis]") {
    uint8_t payload[12U];
    ::memset(payload, 0xFFU, sizeof(payload));

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode12(payload, data);
    Utils::dump(2U, "Trellis::encode12() all ones", data, 25U);

    uint8_t decoded[12U];
    bool result = trellis.decode12(data, decoded);

    REQUIRE(result);
    REQUIRE(::memcmp(decoded, payload, 12U) == 0);
}

TEST_CASE("Trellis 1/2 rate preserves alternating pattern", "[edac][trellis]") {
    uint8_t payload[12U];
    for (size_t i = 0; i < 12U; i++) {
        payload[i] = (i % 2 == 0) ? 0xAAU : 0x55U;
    }

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode12(payload, data);
    Utils::dump(2U, "Trellis::encode12() alternating", data, 25U);

    uint8_t decoded[12U];
    bool result = trellis.decode12(data, decoded);

    REQUIRE(result);
    REQUIRE(::memcmp(decoded, payload, 12U) == 0);
}

TEST_CASE("Trellis 1/2 rate preserves incrementing pattern", "[edac][trellis]") {
    uint8_t payload[12U];
    for (size_t i = 0; i < 12U; i++) {
        payload[i] = (uint8_t)(i * 17);
    }

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode12(payload, data);
    Utils::dump(2U, "Trellis::encode12() incrementing", data, 25U);

    uint8_t decoded[12U];
    bool result = trellis.decode12(data, decoded);

    REQUIRE(result);
    REQUIRE(::memcmp(decoded, payload, 12U) == 0);
}

TEST_CASE("Trellis 1/2 rate corrects errors", "[edac][trellis]") {
    uint8_t original[12U];
    for (size_t i = 0; i < 12U; i++) {
        original[i] = (uint8_t)(i + 75);
    }

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode12(original, data);

    // Save encoded data
    uint8_t encoded[25U];
    ::memcpy(encoded, data, 25U);

    const uint32_t errorSymbols[] = { 0U, 24U, 49U, 73U, 97U };
    
    for (uint32_t symbol : errorSymbols) {
        ::memcpy(data, encoded, 25U);
        shiftSymbol(data, symbol, (symbol & 1U) != 0U);

        uint8_t decoded[12U];
        REQUIRE(trellis.decode12(data, decoded));
        REQUIRE(::memcmp(decoded, original, 12U) == 0);
    }
}

TEST_CASE("Trellis 1/2 rate handles random payloads", "[edac][trellis]") {
    // Test with various random-like patterns
    for (uint32_t test = 0; test < 5; test++) {
        uint8_t payload[12U];
        for (size_t i = 0; i < 12U; i++) {
            payload[i] = (uint8_t)((i * 37 + test * 53) % 256);
        }

        uint8_t data[25U];
        ::memset(data, 0x00U, sizeof(data));

        Trellis trellis;
        trellis.encode12(payload, data);

        uint8_t decoded[12U];
        bool result = trellis.decode12(data, decoded);

        REQUIRE(result);
        REQUIRE(::memcmp(decoded, payload, 12U) == 0);
    }
}

TEST_CASE("Trellis 3/4 rate handles random payloads", "[edac][trellis]") {
    // Test with various random-like patterns
    for (uint32_t test = 0; test < 5; test++) {
        uint8_t payload[18U];
        for (size_t i = 0; i < 18U; i++) {
            payload[i] = (uint8_t)((i * 41 + test * 61) % 256);
        }

        uint8_t data[25U];
        ::memset(data, 0x00U, sizeof(data));

        Trellis trellis;
        trellis.encode34(payload, data, false);

        uint8_t decoded[18U];
        bool result = trellis.decode34(data, decoded, false);

        REQUIRE(result);
        REQUIRE(::memcmp(decoded, payload, 18U) == 0);
    }
}

TEST_CASE("Trellis Viterbi decoders correct every single-level symbol error", "[edac][trellis]") {
    Trellis trellis;

    uint8_t payload34[18U];
    for (uint32_t i = 0U; i < sizeof(payload34); i++)
        payload34[i] = (uint8_t)(i * 29U + 7U);

    uint8_t encoded34[25U] = { 0U };
    trellis.encode34(payload34, encoded34);

    uint8_t payload12[12U];
    for (uint32_t i = 0U; i < sizeof(payload12); i++)
        payload12[i] = (uint8_t)(i * 31U + 11U);

    uint8_t encoded12[25U] = { 0U };
    trellis.encode12(payload12, encoded12);

    for (uint32_t symbol = 0U; symbol < 98U; symbol++) {
        for (uint32_t direction = 0U; direction < 2U; direction++) {
            uint8_t corrupted34[25U];
            ::memcpy(corrupted34, encoded34, sizeof(corrupted34));
            shiftSymbol(corrupted34, symbol, direction == 0U);

            uint8_t decoded34[18U] = { 0U };
            REQUIRE(trellis.decode34(corrupted34, decoded34));
            REQUIRE(::memcmp(decoded34, payload34, sizeof(payload34)) == 0);

            uint8_t corrupted12[25U];
            ::memcpy(corrupted12, encoded12, sizeof(corrupted12));
            shiftSymbol(corrupted12, symbol, direction == 0U);

            uint8_t decoded12[12U] = { 0U };
            REQUIRE(trellis.decode12(corrupted12, decoded12));
            REQUIRE(::memcmp(decoded12, payload12, sizeof(payload12)) == 0);
        }
    }
}

TEST_CASE("Trellis 3/4 rate supports DMR symbol skipping with corruption", "[edac][trellis]") {
    uint8_t payload[18U];
    for (uint32_t i = 0U; i < sizeof(payload); i++)
        payload[i] = (uint8_t)(0xA5U ^ (i * 13U));

    uint8_t encoded[33U] = { 0U };
    Trellis trellis;
    trellis.encode34(payload, encoded, true);

    for (uint32_t symbol = 0U; symbol < 98U; symbol++) {
        uint8_t corrupted[33U];
        ::memcpy(corrupted, encoded, sizeof(corrupted));
        shiftSymbol(corrupted, symbol, (symbol & 1U) != 0U, true);

        uint8_t decoded[18U] = { 0U };
        REQUIRE(trellis.decode34(corrupted, decoded, true));
        REQUIRE(::memcmp(decoded, payload, sizeof(payload)) == 0);
    }
}

TEST_CASE("Trellis Viterbi decoders handle deterministic scattered errors", "[edac][trellis]") {
    std::mt19937 rng(0x251234U);
    Trellis trellis;

    uint32_t corrected34 = 0U;
    uint32_t corrected12 = 0U;
    const uint32_t trials = 500U;
    for (uint32_t trial = 0U; trial < trials; trial++) {
        uint8_t payload34[18U];
        uint8_t payload12[12U];
        for (uint32_t i = 0U; i < sizeof(payload34); i++)
            payload34[i] = (uint8_t)(rng() & 0xFFU);
        for (uint32_t i = 0U; i < sizeof(payload12); i++)
            payload12[i] = (uint8_t)(rng() & 0xFFU);

        uint8_t encoded34[25U] = { 0U };
        uint8_t encoded12[25U] = { 0U };
        trellis.encode34(payload34, encoded34);
        trellis.encode12(payload12, encoded12);

        for (uint32_t error = 0U; error < 2U; error++) {
            shiftSymbol(encoded34, rng() % 98U, (rng() & 1U) != 0U);
            shiftSymbol(encoded12, rng() % 98U, (rng() & 1U) != 0U);
        }

        uint8_t decoded34[18U] = { 0U };
        uint8_t decoded12[12U] = { 0U };
        REQUIRE(trellis.decode34(encoded34, decoded34));
        REQUIRE(trellis.decode12(encoded12, decoded12));
        if (::memcmp(decoded34, payload34, sizeof(payload34)) == 0)
            corrected34++;
        if (::memcmp(decoded12, payload12, sizeof(payload12)) == 0)
            corrected12++;
    }

    INFO("rate 3/4 corrected " << corrected34 << " of " << trials);
    INFO("rate 1/2 corrected " << corrected12 << " of " << trials);
    REQUIRE(corrected34 >= (trials * 90U) / 100U);
    REQUIRE(corrected12 >= (trials * 95U) / 100U);
}

TEST_CASE("Trellis 3/4 rate bounded list validates limits and ordering", "[edac][trellis]") {
    uint8_t payload[18U];
    for (uint32_t i = 0U; i < sizeof(payload); i++)
        payload[i] = (uint8_t)(i * 17U + 3U);

    uint8_t encoded[25U] = { 0U };
    Trellis trellis;
    trellis.encode34(payload, encoded);

    uint8_t candidates[TRELLIS_34_MAX_LIST_PATHS * TRELLIS_34_PAYLOAD_LENGTH_BYTES];
    bool truncated = true;
    REQUIRE(trellis.decode34List(encoded, candidates, TRELLIS_34_MAX_LIST_PATHS, truncated) == 1U);
    REQUIRE_FALSE(truncated);
    REQUIRE(::memcmp(candidates, payload, sizeof(payload)) == 0);

    truncated = true;
    REQUIRE(trellis.decode34List(encoded, candidates, 0U, truncated) == 0U);
    REQUIRE_FALSE(truncated);
    truncated = true;
    REQUIRE(trellis.decode34List(encoded, candidates, TRELLIS_34_MAX_LIST_PATHS + 1U, truncated) == 0U);
    REQUIRE_FALSE(truncated);

    uint8_t single[18U] = { 0U };
    REQUIRE(trellis.decode34(encoded, single));
    truncated = false;
    REQUIRE(trellis.decode34List(encoded, candidates, 1U, truncated) == 1U);
    REQUIRE(::memcmp(candidates, single, sizeof(single)) == 0);

    bool foundTruncation = false;
    for (uint32_t first = 0U; first < 98U && !foundTruncation; first++) {
        for (uint32_t second = first + 1U; second < 98U && !foundTruncation; second++) {
            uint8_t corrupted[25U];
            ::memcpy(corrupted, encoded, sizeof(corrupted));
            shiftSymbol(corrupted, first, false);
            shiftSymbol(corrupted, second, true);

            truncated = false;
            REQUIRE(trellis.decode34List(corrupted, candidates, 1U, truncated) == 1U);
            foundTruncation = truncated;
        }
    }
    REQUIRE(foundTruncation);
}
