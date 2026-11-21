// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025,2026 Bryan Biedenkapp, N2PLL
 *
 */

#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstring>
#include <random>

#include "common/Defines.h"
#include "common/dmr/DMRDefines.h"
#include "common/dmr/data/DataBlock.h"
#include "common/dmr/data/DataHeader.h"
#include "common/edac/BPTC19696.h"
#include "common/edac/CRC.h"
#include "common/edac/Trellis.h"

using namespace dmr::defines;
using namespace dmr::data;

namespace {
    /**
     * @brief Returns the CRC mask for the given data type.
     * @param type The data type for which to retrieve the CRC mask.
     * @return constexpr uint16_t 
     */
    constexpr uint16_t crcMask(DataType::E type)
    {
        return type == DataType::RATE_12_DATA ? 0x0F0U :
            type == DataType::RATE_34_DATA ? 0x1FFU : 0x10FU;
    }

    /**
     * @brief Returns the payload length for the given data type and confirmation status.
     * @param type The data type for which to retrieve the payload length.
     * @param confirmed Whether the data is confirmed.
     * @return constexpr uint32_t The payload length in bytes.
     */
    constexpr uint32_t payloadLength(DataType::E type, bool confirmed)
    {
        if (type == DataType::RATE_12_DATA)
            return confirmed ? DMR_PDU_CONFIRMED_HR_DATA_LENGTH_BYTES : DMR_PDU_HALFRATE_LENGTH_BYTES;
        if (type == DataType::RATE_34_DATA)
            return confirmed ? DMR_PDU_CONFIRMED_TQ_DATA_LENGTH_BYTES : DMR_PDU_THREEQUARTER_LENGTH_BYTES;
        return confirmed ? DMR_PDU_CONFIRMED_UNCODED_DATA_LENGTH_BYTES : DMR_PDU_UNCODED_LENGTH_BYTES;
    }

    /**
     * @brief Extracts a data block from the given frame based on the data type.
     * @param type The data type of the block to extract.
     * @param frame The input frame containing the encoded data.
     * @param block The output buffer to store the extracted data block.
     */
    void extractBlock(DataType::E type, const uint8_t* frame, uint8_t* block)
    {
        if (type == DataType::RATE_12_DATA) {
            edac::BPTC19696().decode(frame, block);
        } else if (type == DataType::RATE_34_DATA) {
            REQUIRE(edac::Trellis().decode34(frame, block, true));
        } else {
            ::memcpy(block, frame, 12U);
            ::memcpy(block + 12U, frame + 21U, 12U);
        }
    }

    /**
     * @brief Injects a data block into the given frame based on the data type.
     * @param type The data type of the block to inject.
     * @param block The input buffer containing the data block to inject.
     * @param frame The output frame where the data block will be injected.
     */
    void injectBlock(DataType::E type, const uint8_t* block, uint8_t* frame)
    {
        ::memset(frame, 0x00U, DMR_FRAME_LENGTH_BYTES);
        if (type == DataType::RATE_12_DATA) {
            edac::BPTC19696().encode(block, frame);
        } else if (type == DataType::RATE_34_DATA) {
            edac::Trellis().encode34(block, frame, true);
        } else {
            ::memcpy(frame, block, 12U);
            ::memcpy(frame + 21U, block + 12U, 12U);
        }
    }

    /** Shifts one rate-3/4 symbol by one sliced 4FSK level in a DMR burst. */
    void shiftRate34Symbol(uint8_t* frame, uint32_t symbol, bool up)
    {
        uint32_t firstBit = symbol * 2U;
        uint32_t secondBit = firstBit + 1U;
        if (firstBit >= 98U)
            firstBit += 68U;
        if (secondBit >= 98U)
            secondBit += 68U;

        const bool b1 = READ_BIT(frame, firstBit) != 0U;
        const bool b2 = READ_BIT(frame, secondBit) != 0U;
        int32_t level = (!b1 && b2) ? 3 : (!b1 && !b2) ? 1 : (b1 && !b2) ? -1 : -3;
        level += up ? 2 : -2;
        if (level > 3)
            level = 1;
        if (level < -3)
            level = -1;

        WRITE_BIT(frame, firstBit, level < 0);
        WRITE_BIT(frame, secondBit, level == 3 || level == -3);
    }
}

TEST_CASE("DMR confirmed data blocks preserve DBSN, payload, and masked CRC-9", "[dmr][datablock]")
{
    const DataType::E rates[] = {
        DataType::RATE_12_DATA, DataType::RATE_34_DATA, DataType::RATE_1_DATA
    };

    for (DataType::E rate : rates) {
        CAPTURE(rate);
        const uint32_t length = payloadLength(rate, true);
        std::array<uint8_t, DMR_PDU_UNCODED_LENGTH_BYTES> payload{};
        for (uint32_t i = 0U; i < length; ++i)
            payload[i] = uint8_t(0x31U + i);

        DataBlock encoded;
        encoded.setFormat(DPF::CONFIRMED_DATA);
        encoded.setDataType(rate);
        encoded.setSerialNo(0x35U);
        encoded.setData(payload.data());

        std::array<uint8_t, DMR_FRAME_LENGTH_BYTES> frame{};
        encoded.encode(frame.data());

        std::array<uint8_t, DMR_PDU_UNCODED_LENGTH_BYTES> raw{};
        extractBlock(rate, frame.data(), raw.data());
        REQUIRE((raw[0U] >> 1) == 0x35U);
        REQUIRE(::memcmp(raw.data() + 2U, payload.data(), length) == 0);

        std::array<uint8_t, DMR_PDU_UNCODED_LENGTH_BYTES> crcInput{};
        ::memcpy(crcInput.data(), payload.data(), length);
        for (uint32_t i = 0U; i < 7U; ++i)
            WRITE_BIT(crcInput.data(), length * 8U + i, READ_BIT(raw.data(), i));
        uint16_t expected = ~edac::CRC::createCRC9(crcInput.data(), length * 8U + 7U) & 0x1FFU;
        expected ^= crcMask(rate);
        const uint16_t transmitted = ((raw[0U] & 0x01U) << 8) | raw[1U];
        REQUIRE(transmitted == expected);

        DataHeader header;
        header.setDPF(DPF::CONFIRMED_DATA);
        DataBlock decoded;
        decoded.setDataType(rate);
        REQUIRE(decoded.decode(frame.data(), header));
        REQUIRE(decoded.getSerialNo() == 0x35U);
        std::array<uint8_t, DMR_PDU_UNCODED_LENGTH_BYTES> output{};
        REQUIRE(decoded.getData(output.data()) == length);
        REQUIRE(::memcmp(output.data(), payload.data(), length) == 0);
    }
}

TEST_CASE("DMR confirmed data blocks reject CRC-9 failures", "[dmr][datablock]")
{
    const DataType::E rates[] = {
        DataType::RATE_12_DATA, DataType::RATE_34_DATA, DataType::RATE_1_DATA
    };
    DataHeader header;
    header.setDPF(DPF::CONFIRMED_DATA);

    for (DataType::E rate : rates) {
        CAPTURE(rate);
        std::array<uint8_t, DMR_PDU_UNCODED_LENGTH_BYTES> payload{};
        payload.fill(0xA5U);
        DataBlock encoded;
        encoded.setFormat(header);
        encoded.setDataType(rate);
        encoded.setSerialNo(7U);
        encoded.setData(payload.data());
        std::array<uint8_t, DMR_FRAME_LENGTH_BYTES> frame{};
        encoded.encode(frame.data());

        std::array<uint8_t, DMR_PDU_UNCODED_LENGTH_BYTES> raw{};
        extractBlock(rate, frame.data(), raw.data());
        raw[2U] ^= 0x80U;
        injectBlock(rate, raw.data(), frame.data());

        DataBlock decoded;
        decoded.setDataType(rate);
        REQUIRE_FALSE(decoded.decode(frame.data(), header));
    }
}

TEST_CASE("DMR confirmed rate-3/4 resolves tied Trellis candidates by CRC-9", "[dmr][datablock][trellis]")
{
    std::array<uint8_t, DMR_PDU_UNCODED_LENGTH_BYTES> payload{};
    for (uint32_t i = 0U; i < DMR_PDU_CONFIRMED_TQ_DATA_LENGTH_BYTES; i++)
        payload[i] = (uint8_t)(i * 23U + 5U);

    DataBlock encoded;
    encoded.setFormat(DPF::CONFIRMED_DATA);
    encoded.setDataType(DataType::RATE_34_DATA);
    encoded.setSerialNo(0x2AU);
    encoded.setData(payload.data());

    std::array<uint8_t, DMR_FRAME_LENGTH_BYTES> cleanFrame{};
    encoded.encode(cleanFrame.data());
    std::array<uint8_t, edac::TRELLIS_34_PAYLOAD_LENGTH_BYTES> expectedRaw{};
    extractBlock(DataType::RATE_34_DATA, cleanFrame.data(), expectedRaw.data());

    std::mt19937 rng(0x34D4U);
    std::array<uint8_t, DMR_FRAME_LENGTH_BYTES> tiedFrame{};
    bool found = false;
    for (uint32_t trial = 0U; trial < 10000U && !found; trial++) {
        tiedFrame = cleanFrame;
        for (uint32_t error = 0U; error < 5U; error++)
            shiftRate34Symbol(tiedFrame.data(), rng() % 98U, (rng() & 1U) != 0U);

        uint8_t candidates[edac::TRELLIS_34_MAX_LIST_PATHS * edac::TRELLIS_34_PAYLOAD_LENGTH_BYTES];
        bool truncated = false;
        const uint32_t count = edac::Trellis().decode34List(tiedFrame.data(), candidates,
            edac::TRELLIS_34_MAX_LIST_PATHS, truncated, true);
        for (uint32_t candidate = 1U; candidate < count; candidate++) {
            if (::memcmp(candidates + candidate * edac::TRELLIS_34_PAYLOAD_LENGTH_BYTES,
                expectedRaw.data(), expectedRaw.size()) == 0) {
                found = true;
                break;
            }
        }
    }

    REQUIRE(found);

    DataHeader header;
    header.setDPF(DPF::CONFIRMED_DATA);
    DataBlock decoded;
    decoded.setDataType(DataType::RATE_34_DATA);
    REQUIRE(decoded.decode(tiedFrame.data(), header));
    REQUIRE(decoded.getSerialNo() == 0x2AU);

    std::array<uint8_t, DMR_PDU_UNCODED_LENGTH_BYTES> output{};
    REQUIRE(decoded.getData(output.data()) == DMR_PDU_CONFIRMED_TQ_DATA_LENGTH_BYTES);
    REQUIRE(::memcmp(output.data(), payload.data(), DMR_PDU_CONFIRMED_TQ_DATA_LENGTH_BYTES) == 0);
}

TEST_CASE("DMR unconfirmed Rate 1 maps all 24 payload octets around the burst centre", "[dmr][datablock]")
{
    std::array<uint8_t, DMR_PDU_UNCODED_LENGTH_BYTES> payload{};
    for (uint32_t i = 0U; i < payload.size(); ++i)
        payload[i] = uint8_t(i + 1U);

    DataBlock encoded;
    encoded.setFormat(DPF::UNCONFIRMED_DATA);
    encoded.setDataType(DataType::RATE_1_DATA);
    encoded.setData(payload.data());
    std::array<uint8_t, DMR_FRAME_LENGTH_BYTES> frame{};
    frame.fill(0xFFU);
    encoded.encode(frame.data());
    REQUIRE((frame[12U] & 0xC0U) == 0U);
    REQUIRE((frame[20U] & 0x03U) == 0U);

    DataHeader header;
    header.setDPF(DPF::UNCONFIRMED_DATA);
    DataBlock decoded;
    decoded.setDataType(DataType::RATE_1_DATA);
    REQUIRE(decoded.decode(frame.data(), header));
    std::array<uint8_t, DMR_PDU_UNCODED_LENGTH_BYTES> output{};
    REQUIRE(decoded.getData(output.data()) == payload.size());
    REQUIRE(output == payload);
}
