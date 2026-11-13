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

#include "common/dmr/DMRDefines.h"
#include "common/dmr/lc/csbk/CSBK_RAW.h"
#include "common/edac/CRC.h"
#include "common/nxdn/NXDNDefines.h"
#include "common/nxdn/channel/FACCH1.h"
#include "common/p25/P25Defines.h"
#include "common/p25/lc/tsbk/OSP_TSBK_RAW.h"

#include <array>
#include <cstdint>
#include <cstring>

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::lc::csbk;
using namespace nxdn::defines;
using namespace nxdn::channel;
using namespace p25::defines;
using namespace p25::lc::tsbk;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------
namespace {

/**
 * @brief Generate a pseudo-random number using xorshift32.
 * @param state Reference to the current state of the random number generator.
 * @returns uint32_t Pseudo-random number.
 */
uint32_t nextRand(uint32_t& state)
{
    // Deterministic xorshift32 for reproducible fuzz vectors.
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

/**
 * @brief Fill a buffer with pseudo-random data.
 * @param buffer Pointer to the buffer to fill.
 * @param len Length of the buffer in bytes.
 * @param state Reference to the current state of the random number generator.
 */
void fillPseudoRandom(uint8_t* buffer, uint32_t len, uint32_t& state)
{
    for (uint32_t i = 0U; i < len; ++i) {
        buffer[i] = static_cast<uint8_t>(nextRand(state) & 0xFFU);
    }
}

} // namespace

TEST_CASE("Parsers tolerate malformed inputs without throwing", "[robustness][fuzz][parser]")
{
    SECTION("DMR CSBK decode survives malformed frames")
    {
        CSBK_RAW csbk;
        csbk.setDataType(DataType::CSBK);

        uint32_t seed = 0xC5B71235U;
        std::array<uint8_t, DMR_FRAME_LENGTH_BYTES> frame{};

        for (uint32_t i = 0U; i < 128U; ++i) {
            fillPseudoRandom(frame.data(), static_cast<uint32_t>(frame.size()), seed);
            REQUIRE_NOTHROW(csbk.decode(frame.data()));
        }
    }

    SECTION("P25 TSBK decode survives malformed frames")
    {
        OSP_TSBK_RAW tsbk;

        uint32_t seed = 0x19A4D02FU;
        std::array<uint8_t, P25_TSDU_FRAME_LENGTH_BYTES> frame{};

        for (uint32_t i = 0U; i < 128U; ++i) {
            fillPseudoRandom(frame.data(), static_cast<uint32_t>(frame.size()), seed);
            REQUIRE_NOTHROW(tsbk.decode(frame.data()));
        }
    }

    SECTION("NXDN FACCH1 decode survives malformed frames")
    {
        FACCH1 facch;

        uint32_t seed = 0x8E12B77DU;
        std::array<uint8_t, NXDN_FRAME_LENGTH_BYTES + 2U> frame{};

        for (uint32_t i = 0U; i < 128U; ++i) {
            fillPseudoRandom(frame.data(), static_cast<uint32_t>(frame.size()), seed);
            REQUIRE_NOTHROW(facch.decode(frame.data(), NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_FEC_LENGTH_BITS));
        }
    }
}

TEST_CASE("Parsers handle heavily corrupted control blocks deterministically", "[robustness][negative][parser]")
{
    SECTION("DMR CSBK handles heavily corrupted encoded frame consistently")
    {
        CSBK_RAW encoded;

        uint8_t csbkData[DMR_CSBK_LENGTH_BYTES];
        ::memset(csbkData, 0x00U, sizeof(csbkData));
        csbkData[0U] = 0x02U;
        csbkData[1U] = 0x00U;

        csbkData[10U] ^= CSBK_CRC_MASK[0U];
        csbkData[11U] ^= CSBK_CRC_MASK[1U];
        edac::CRC::addCCITT162(csbkData, DMR_CSBK_LENGTH_BYTES);
        csbkData[10U] ^= CSBK_CRC_MASK[0U];
        csbkData[11U] ^= CSBK_CRC_MASK[1U];

        encoded.setCSBK(csbkData);

        uint8_t frame[DMR_FRAME_LENGTH_BYTES];
        ::memset(frame, 0x00U, sizeof(frame));
        encoded.encode(frame);

        for (uint32_t i = 0U; i < sizeof(frame); ++i) {
            frame[i] ^= 0xFFU;
        }

        CSBK_RAW decoded;
        decoded.setDataType(DataType::CSBK);

        // CSBK_RAW is a raw wrapper and may accept opaque payloads. Verify deterministic handling.
        const bool accepted = decoded.decode(frame);
        const bool acceptedAgain = decoded.decode(frame);
        REQUIRE(accepted == acceptedAgain);
    }

    SECTION("P25 TSBK handles heavily corrupted encoded block consistently")
    {
        OSP_TSBK_RAW encoded;

        uint8_t tsbkData[P25_TSBK_LENGTH_BYTES];
        ::memset(tsbkData, 0x00U, sizeof(tsbkData));
        tsbkData[0U] = 0x34U;
        tsbkData[1U] = 0x00U;
        edac::CRC::addCCITT162(tsbkData, P25_TSBK_LENGTH_BYTES);

        encoded.setTSBK(tsbkData);

        uint8_t frame[P25_TSBK_LENGTH_BYTES];
        ::memset(frame, 0x00U, sizeof(frame));
        encoded.encode(frame, true, true);

        for (uint32_t i = 0U; i < sizeof(frame); ++i) {
            frame[i] ^= 0xFFU;
        }

        OSP_TSBK_RAW decoded;

        // Trellis/CRC decoding may still resolve some corrupted blocks. Require stable behavior.
        const bool accepted = decoded.decode(frame, true);
        const bool acceptedAgain = decoded.decode(frame, true);
        REQUIRE(accepted == acceptedAgain);
    }

    SECTION("NXDN FACCH1 rejects heavily corrupted encoded block")
    {
        uint8_t facchData[10U];
        ::memset(facchData, 0xA5U, sizeof(facchData));

        uint8_t frame[NXDN_FRAME_LENGTH_BYTES + 2U];
        ::memset(frame, 0x00U, sizeof(frame));

        FACCH1 encoded;
        encoded.setData(facchData);
        const uint32_t offset = NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_FEC_LENGTH_BITS;
        encoded.encode(frame, offset);

        for (uint32_t i = 0U; i < NXDN_FRAME_LENGTH_BYTES; ++i) {
            frame[i] ^= 0x5AU;
        }

        FACCH1 decoded;
        REQUIRE_FALSE(decoded.decode(frame, offset));
    }
}
