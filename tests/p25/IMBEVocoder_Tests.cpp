// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 */
#include "vocoder/imbe/imbe_vocoder.h"
#include "vocoder/MBEDecoder.h"
#include "vocoder/MBEEncoder.h"

#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include <cstring>

static void packFrameVector(const std::array<int16_t, 8U>& frame, std::array<uint8_t, 11U>& packed)
{
    static const std::array<uint32_t, 8U> WORD_WIDTHS = {12U, 12U, 12U, 12U, 11U, 11U, 11U, 7U};
    uint32_t offset = 0U;

    packed.fill(0U);
    for (uint32_t word = 0U; word < frame.size(); word++) {
        uint16_t mask = static_cast<uint16_t>(1U << (WORD_WIDTHS[word] - 1U));
        for (uint32_t bit = 0U; bit < WORD_WIDTHS[word]; bit++, offset++, mask >>= 1U) {
            if ((static_cast<uint16_t>(frame[word]) & mask) != 0U)
                packed[offset / 8U] |= static_cast<uint8_t>(1U << (7U - offset % 8U));
        }
    }
}

TEST_CASE("IMBE decoder pseudo-random state is isolated per stream", "[p25][vocoder][imbe]") {
    imbe_vocoder encoder;
    imbe_vocoder decoderA;
    imbe_vocoder decoderB;

    std::array<int16_t, 160U> pcm{};
    std::array<int16_t, 8U> frame{};
    std::array<int16_t, 160U> outputA{};
    std::array<int16_t, 160U> outputB{};

    for (uint32_t frameIndex = 0U; frameIndex < 8U; frameIndex++) {
        for (uint32_t sample = 0U; sample < pcm.size(); sample++)
            pcm[sample] = static_cast<int16_t>(static_cast<int32_t>((sample * 97U + frameIndex * 313U) & 0x3FFFU) - 0x2000);

        encoder.imbe_encode(frame.data(), pcm.data());
        decoderA.imbe_decode(frame.data(), outputA.data());
        decoderB.imbe_decode(frame.data(), outputB.data());

        REQUIRE(::memcmp(outputA.data(), outputB.data(), sizeof(outputA)) == 0);
    }
}

TEST_CASE("IMBE encoder selects the fundamental of stable harmonic signals", "[p25][vocoder][imbe]") {
    static const std::array<uint32_t, 9U> PERIODS = {20U, 21U, 25U, 30U, 40U, 50U, 60U, 80U, 100U};

    for (const uint32_t period : PERIODS) {
        imbe_vocoder encoder;
        std::array<int16_t, 160U> pcm{};
        std::array<int16_t, 8U> frame{};

        for (uint32_t frameIndex = 0U; frameIndex < 12U; frameIndex++) {
            for (uint32_t sample = 0U; sample < pcm.size(); sample++) {
                const uint32_t phase = (frameIndex * pcm.size() + sample) % period;
                pcm[sample] = static_cast<int16_t>(static_cast<int32_t>(phase * 12000U / period) - 6000);
            }

            encoder.imbe_encode(frame.data(), pcm.data());

            if (frameIndex >= 8U) {
                INFO("period=" << period << " frame=" << frameIndex);
                REQUIRE(encoder.param()->b_vec[0] == static_cast<int16_t>(2U * period - 39U));

                uint32_t voicedHarmonics = 0U;
                for (int16_t harmonic = 0; harmonic < encoder.param()->num_harms; harmonic++)
                    voicedHarmonics += encoder.param()->v_uv_dsn[harmonic] != 0;
                REQUIRE(voicedHarmonics == static_cast<uint32_t>(encoder.param()->num_harms));
            }
        }
    }
}

TEST_CASE("MBEDecoder uses the fixed-point decoder for 88-bit IMBE", "[p25][vocoder][imbe]") {
    imbe_vocoder encoder;
    imbe_vocoder fixedDecoder;
    vocoder::MBEDecoder decoder(vocoder::DECODE_88BIT_IMBE);
    std::array<int16_t, 160U> pcm{};
    std::array<int16_t, 8U> frame{};
    std::array<uint8_t, 11U> packed{};
    std::array<int16_t, 160U> fixedOutput{};
    std::array<int16_t, 160U> decoderOutput{};

    for (uint32_t frameIndex = 0U; frameIndex < 8U; frameIndex++) {
        for (uint32_t sample = 0U; sample < pcm.size(); sample++)
            pcm[sample] = static_cast<int16_t>(static_cast<int32_t>((sample * 83U + frameIndex * 271U) & 0x3FFFU) - 0x2000);

        encoder.imbe_encode(frame.data(), pcm.data());
        packFrameVector(frame, packed);
        fixedDecoder.imbe_decode(frame.data(), fixedOutput.data());
        REQUIRE(decoder.decode(packed.data(), decoderOutput.data()) == 0);
        REQUIRE(::memcmp(fixedOutput.data(), decoderOutput.data(), sizeof(fixedOutput)) == 0);
    }
}

TEST_CASE("IMBE analysis does not classify broadband noise as harmonic speech", "[p25][vocoder][imbe]") {
    imbe_vocoder encoder;
    std::array<int16_t, 160U> pcm{};
    std::array<int16_t, 8U> frame{};
    uint32_t noise = 0x12345678U;

    for (uint32_t frameIndex = 0U; frameIndex < 16U; frameIndex++) {
        for (uint32_t sample = 0U; sample < pcm.size(); sample++) {
            noise ^= noise << 13;
            noise ^= noise >> 17;
            noise ^= noise << 5;
            pcm[sample] = static_cast<int16_t>(noise >> 16);
        }

        encoder.imbe_encode(frame.data(), pcm.data());

        if (frameIndex >= 5U) {
            const IMBE_PARAM* parameters = encoder.param();
            uint32_t voicedHarmonics = 0U;
            for (int16_t harmonic = 0; harmonic < parameters->num_harms; harmonic++)
                voicedHarmonics += parameters->v_uv_dsn[harmonic] != 0;

            INFO("frame=" << frameIndex << " pitchError=" << parameters->e_p);
            REQUIRE(voicedHarmonics <= 3U);
        }
    }
}

TEST_CASE("IMBE encoder applies gain adjustment to the first frame", "[p25][vocoder][imbe]") {
    vocoder::MBEEncoder reference(vocoder::ENCODE_88BIT_IMBE);
    vocoder::MBEEncoder adjusted(vocoder::ENCODE_88BIT_IMBE);
    std::array<int16_t, 160U> pcm{};
    std::array<uint8_t, 11U> referenceBits{};
    std::array<uint8_t, 11U> adjustedBits{};

    for (uint32_t sample = 0U; sample < pcm.size(); sample++)
        pcm[sample] = static_cast<int16_t>(static_cast<int32_t>((sample * 211U) & 0x3FFFU) - 0x2000);

    adjusted.setGainAdjust(2.0f);
    reference.encode(pcm.data(), referenceBits.data());
    adjusted.encode(pcm.data(), adjustedBits.data());

    REQUIRE(referenceBits != adjustedBits);
}
