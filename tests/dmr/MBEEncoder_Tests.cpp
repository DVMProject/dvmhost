// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 C. Lovell, Dev_Ranger
 */
#include "vocoder/MBEEncoder.h"
#include "vocoder/MBEDecoder.h"

#include <catch2/catch_test_macros.hpp>
#include <array>
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

TEST_CASE("AMBE decoder pseudo-random state is isolated per stream", "[dmr][vocoder][ambe]") {
    vocoder::MBEEncoder encoder(vocoder::ENCODE_DMR_AMBE);
    vocoder::MBEDecoder decoderA(vocoder::DECODE_DMR_AMBE);
    vocoder::MBEDecoder decoderB(vocoder::DECODE_DMR_AMBE);
    std::array<int16_t, 160U> pcm{};
    std::array<uint8_t, 49U> bits{};
    std::array<int16_t, 160U> outputA{};
    std::array<int16_t, 160U> outputB{};
    uint32_t noise = 0x1A2B3C4DU;

    for (uint32_t frame = 0U; frame < 8U; frame++) {
        for (uint32_t sample = 0U; sample < pcm.size(); sample++) {
            noise ^= noise << 13;
            noise ^= noise >> 17;
            noise ^= noise << 5;
            pcm[sample] = static_cast<int16_t>(noise >> 16);
        }

        encoder.encodeBits(pcm.data(), bits.data());
        REQUIRE(decoderA.decodeBits(bits.data(), outputA.data()) == 0);
        REQUIRE(decoderB.decodeBits(bits.data(), outputB.data()) == 0);
        REQUIRE(::memcmp(outputA.data(), outputB.data(), sizeof(outputA)) == 0);
    }
}

TEST_CASE("AMBE encoder applies gain adjustment to analyzed speech", "[dmr][vocoder][ambe]") {
    vocoder::MBEEncoder reference(vocoder::ENCODE_DMR_AMBE);
    vocoder::MBEEncoder adjusted(vocoder::ENCODE_DMR_AMBE);
    std::array<int16_t, 160U> pcm{};
    std::array<uint8_t, 49U> referenceBits{};
    std::array<uint8_t, 49U> adjustedBits{};

    for (uint32_t sample = 0U; sample < pcm.size(); sample++)
        pcm[sample] = static_cast<int16_t>(static_cast<int32_t>((sample * 211U) & 0x3FFFU) - 0x2000);

    adjusted.setGainAdjust(2.0f);
    bool differs = false;
    for (uint32_t frame = 0U; frame < 8U; frame++) {
        reference.encodeBits(pcm.data(), referenceBits.data());
        adjusted.encodeBits(pcm.data(), adjustedBits.data());
        differs |= referenceBits != adjustedBits;
    }

    REQUIRE(differs);
}

TEST_CASE("AMBE encoder uses minimum gain for zero spectral energy", "[dmr][vocoder][ambe]") {
    vocoder::MBEEncoder encoder(vocoder::ENCODE_DMR_AMBE);
    std::array<int16_t, 160U> pcm{};
    std::array<uint8_t, 49U> bits{};

    for (uint32_t frame = 0U; frame < 4U; frame++) {
        encoder.encodeBits(pcm.data(), bits.data());

        const uint32_t gainIndex = (bits[8U] << 4U) | (bits[9U] << 3U) |
            (bits[10U] << 2U) | (bits[11U] << 1U) | bits[36U];
        REQUIRE(gainIndex == 0U);
    }
}

TEST_CASE("AMBE encoder resolves a period-20 fundamental at its lower pitch boundary", "[dmr][vocoder][ambe]") {
    vocoder::MBEEncoder encoder(vocoder::ENCODE_DMR_AMBE);
    std::array<int16_t, 160U> pcm{};
    std::array<uint8_t, 49U> bits{};

    for (uint32_t frame = 0U; frame < 12U; frame++) {
        for (uint32_t sample = 0U; sample < pcm.size(); sample++) {
            const uint32_t phase = (frame * pcm.size() + sample) % 20U;
            pcm[sample] = static_cast<int16_t>(static_cast<int32_t>(phase * 12000U / 20U) - 6000);
        }

        encoder.encodeBits(pcm.data(), bits.data());
        const uint32_t pitchIndex = (bits[0U] << 6U) | (bits[1U] << 5U) |
            (bits[2U] << 4U) | (bits[3U] << 3U) | (bits[37U] << 2U) |
            (bits[38U] << 1U) | bits[39U];
        REQUIRE(pitchIndex < 120U);
        if (frame >= 8U)
            REQUIRE(pitchIndex == 0U);
    }
}

TEST_CASE("Raw AMBE decoding uses the float decoder output scale", "[dmr][vocoder][ambe]") {
    vocoder::MBEEncoder encoder(vocoder::ENCODE_DMR_AMBE);
    vocoder::MBEDecoder decoder(vocoder::DECODE_DMR_AMBE);
    std::array<int16_t, 160U> pcm{};
    std::array<int16_t, 160U> decoded{};
    std::array<uint8_t, 49U> bits{};

    for (uint32_t frame = 0U; frame < 12U; frame++) {
        for (uint32_t sample = 0U; sample < pcm.size(); sample++) {
            const uint32_t phase = (frame * pcm.size() + sample) % 40U;
            pcm[sample] = static_cast<int16_t>(static_cast<int32_t>(phase * 12000U / 40U) - 6000);
        }

        encoder.encodeBits(pcm.data(), bits.data());
        REQUIRE(decoder.decodeBits(bits.data(), decoded.data()) == 0);

        if (frame >= 8U) {
            uint32_t clippedSamples = 0U;
            for (const int16_t output : decoded)
                clippedSamples += output == 32760 || output == -32760;
            REQUIRE(clippedSamples == 0U);
        }
    }
}
