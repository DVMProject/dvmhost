// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES FROM THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/network/PacketBuffer.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace network;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/**
 * @brief Retrieves the fragments of a PacketBuffer in order based on their index. The fragments 
 * are returned as a vector of pointers to PacketBuffer::Fragment objects. The order is determined 
 * by the fragment index, starting from 0 and stopping when a missing or null fragment is encountered.
 * @param buffer The PacketBuffer instance whose fragments are to be retrieved in order.
 * @return std::vector<const PacketBuffer::Fragment*> 
 */
static std::vector<const PacketBuffer::Fragment*> fragmentsInOrder(PacketBuffer& buffer)
{
    std::vector<const PacketBuffer::Fragment*> fragments;
    for (uint32_t index = 0U; index < 32U; index++) {
        auto it = buffer.fragments.find((uint8_t)index);
        if (it == buffer.fragments.end() || it->second == nullptr)
            break;

        fragments.push_back(it->second);
    }

    return fragments;
}

/**
 * @brief Sets the header information for a fragment. This includes the total size of the fragment, 
 * the compressed size, the block ID, and the total number of blocks. The header is written directly 
 * into the provided fragment byte array.
 * @param fragment The byte array representing the fragment where the header will be written.
 * @param size The total size of the fragment.
 * @param compressedSize The size of the compressed data within the fragment.
 * @param blockId The ID of the block within the fragment sequence.
 * @param blockCount The total number of blocks in the fragment sequence.   
 */
static void setFragmentHeader(uint8_t* fragment, uint32_t size, uint32_t compressedSize, uint8_t blockId, uint8_t blockCount)
{
    SET_UINT32(size, fragment, 0U);
    SET_UINT32(compressedSize, fragment, 4U);
    fragment[8U] = blockId;
    fragment[9U] = blockCount;
}

TEST_CASE("PacketBuffer reassembles uncompressed fragments in order", "[network][packetbuffer]")
{
    PacketBuffer encoder(false, "packetbuffer-plain");
    std::vector<uint8_t> payload(800U);
    for (size_t i = 0U; i < payload.size(); i++)
        payload[i] = (uint8_t)(i & 0xFFU);

    encoder.encode(payload.data(), (uint32_t)payload.size());
    REQUIRE(encoder.fragments.size() == 2U);

    PacketBuffer decoder(false, "packetbuffer-plain");
    uint8_t* message = nullptr;
    uint32_t messageLength = 0U;

    auto encodedFragments = fragmentsInOrder(encoder);
    REQUIRE(encodedFragments.size() == 2U);

    REQUIRE_FALSE(decoder.decode(encodedFragments[1U]->data, &message, &messageLength));
    REQUIRE(message == nullptr);
    REQUIRE(messageLength == 0U);

    REQUIRE(decoder.decode(encodedFragments[0U]->data, &message, &messageLength));
    REQUIRE(message != nullptr);
    REQUIRE(messageLength == payload.size());
    REQUIRE(std::memcmp(message, payload.data(), payload.size()) == 0);
    delete[] message;

    REQUIRE(decoder.fragments.size() == 0U);
    encoder.clear();
    REQUIRE(encoder.fragments.size() == 0U);
}

TEST_CASE("PacketBuffer reassembles compressed fragments", "[network][packetbuffer]")
{
    PacketBuffer encoder(true, "packetbuffer-compressed");
    std::vector<uint8_t> payload;
    for (uint32_t i = 0U; i < 1024U; i++)
        payload.push_back('A');

    encoder.encode(payload.data(), (uint32_t)payload.size());
    REQUIRE(encoder.fragments.size() >= 1U);

    PacketBuffer decoder(true, "packetbuffer-compressed");
    uint8_t* message = nullptr;
    uint32_t messageLength = 0U;

    auto encodedFragments = fragmentsInOrder(encoder);
    REQUIRE_FALSE(encodedFragments.empty());
    for (size_t i = 0U; i < encodedFragments.size(); i++) {
        bool complete = decoder.decode(encodedFragments[i]->data, &message, &messageLength);
        if (i + 1U < encodedFragments.size()) {
            REQUIRE_FALSE(complete);
            REQUIRE(message == nullptr);
        } else {
            REQUIRE(complete);
            REQUIRE(message != nullptr);
            REQUIRE(messageLength == payload.size());
            REQUIRE(std::memcmp(message, payload.data(), payload.size()) == 0);
            delete[] message;
        }
    }
}

TEST_CASE("PacketBuffer rejects oversized packet metadata", "[network][packetbuffer]")
{
    PacketBuffer buffer(false, "packetbuffer-invalid");

    std::array<uint8_t, FRAG_SIZE> fragment = {};
    setFragmentHeader(fragment.data(), 8192U * 1024U + 1U, 8192U * 1024U + 1U, 0U, 0U);

    uint8_t* message = nullptr;
    uint32_t messageLength = 0U;
    REQUIRE_FALSE(buffer.decode(fragment.data(), &message, &messageLength));
    REQUIRE(message == nullptr);
    REQUIRE(messageLength == 0U);
    REQUIRE(buffer.fragments.size() == 0U);
}

TEST_CASE("PacketBuffer rejects invalid ZLIB data", "[network][packetbuffer]")
{
    PacketBuffer buffer(true, "packetbuffer-invalid-zlib");

    std::array<uint8_t, FRAG_SIZE> fragment = {};
    constexpr uint32_t invalidCompressedSize = 8U;
    setFragmentHeader(fragment.data(), 64U, invalidCompressedSize, 0U, 0U);
    std::fill_n(fragment.data() + FRAG_HDR_SIZE, invalidCompressedSize, 0xFFU);

    uint8_t* message = nullptr;
    uint32_t messageLength = 0U;
    REQUIRE_FALSE(buffer.decode(fragment.data(), &message, &messageLength));
    REQUIRE(message == nullptr);
    REQUIRE(messageLength == 0U);
    REQUIRE(buffer.fragments.empty());
}

TEST_CASE("PacketBuffer enforces the maximum block count", "[network][packetbuffer]")
{
    constexpr uint32_t maxBlockCount = 254U;

    SECTION("accepts exactly the maximum number of blocks") {
        PacketBuffer buffer(false, "packetbuffer-max-blocks");
        std::vector<uint8_t> payload(FRAG_BLOCK_SIZE * maxBlockCount, 0x5AU);

        REQUIRE(buffer.encode(payload.data(), (uint32_t)payload.size()));
        REQUIRE(buffer.fragments.size() == maxBlockCount);

        auto last = buffer.fragments.find((uint8_t)(maxBlockCount - 1U));
        REQUIRE(last != buffer.fragments.end());
        REQUIRE(last->second != nullptr);
        REQUIRE(last->second->data[8U] == maxBlockCount - 1U);
        REQUIRE(last->second->data[9U] == maxBlockCount - 1U);
    }

    SECTION("rejects one block over the maximum") {
        PacketBuffer buffer(false, "packetbuffer-too-many-blocks");
        std::vector<uint8_t> payload(FRAG_BLOCK_SIZE * maxBlockCount + 1U, 0xA5U);

        REQUIRE_FALSE(buffer.encode(payload.data(), (uint32_t)payload.size()));
        REQUIRE(buffer.fragments.empty());
    }
}
