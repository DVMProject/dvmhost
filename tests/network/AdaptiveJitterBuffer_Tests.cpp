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
#include "common/network/AdaptiveJitterBuffer.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace network;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/**
 * @brief Deletes all BufferedFrame pointers in the given vector and clears the vector.
 * @param frames A vector of BufferedFrame pointers to be deleted and cleared.
 */
static void cleanupFrames(std::vector<BufferedFrame*>& frames)
{
    for (BufferedFrame* frame : frames)
        delete frame;
    frames.clear();
}

/**
 * @brief Converts the data of a BufferedFrame into a std::string. If the frame is nullptr, or its data is nullptr, 
 * or its length is 0, an empty string is returned.
 * @param frame A pointer to the BufferedFrame whose data is to be converted to a std::string.
 * @return std::string 
 */
static std::string frameToString(const BufferedFrame* frame)
{
    if (frame == nullptr || frame->data == nullptr || frame->length == 0U)
        return std::string();

    return std::string(reinterpret_cast<const char*>(frame->data), frame->length);
}

TEST_CASE("AdaptiveJitterBuffer delivers in-order frames immediately", "[network][jitter]")
{
    AdaptiveJitterBuffer buffer(4U, 40000U);
    std::vector<BufferedFrame*> readyFrames;

    const std::string one = "one";
    const std::string two = "two";
    const std::string three = "three";

    REQUIRE(buffer.processFrame(1000U, reinterpret_cast<const uint8_t*>(one.data()), (uint32_t)one.size(), readyFrames));
    REQUIRE(readyFrames.size() == 1U);
    REQUIRE(readyFrames[0U]->seq == 1000U);
    REQUIRE(frameToString(readyFrames[0U]) == one);
    cleanupFrames(readyFrames);

    REQUIRE(buffer.processFrame(1001U, reinterpret_cast<const uint8_t*>(two.data()), (uint32_t)two.size(), readyFrames));
    REQUIRE(readyFrames.size() == 1U);
    REQUIRE(readyFrames[0U]->seq == 1001U);
    REQUIRE(frameToString(readyFrames[0U]) == two);
    cleanupFrames(readyFrames);

    REQUIRE(buffer.processFrame(1002U, reinterpret_cast<const uint8_t*>(three.data()), (uint32_t)three.size(), readyFrames));
    REQUIRE(readyFrames.size() == 1U);
    REQUIRE(readyFrames[0U]->seq == 1002U);
    REQUIRE(frameToString(readyFrames[0U]) == three);
    cleanupFrames(readyFrames);

    uint64_t totalFrames = 0ULL;
    uint64_t reorderedFrames = 0ULL;
    uint64_t droppedFrames = 0ULL;
    uint64_t timedOutFrames = 0ULL;
    buffer.getStatistics(totalFrames, reorderedFrames, droppedFrames, timedOutFrames);
    REQUIRE(totalFrames == 3U);
    REQUIRE(reorderedFrames == 0U);
    REQUIRE(droppedFrames == 0U);
    REQUIRE(timedOutFrames == 0U);
    REQUIRE(buffer.getNextExpectedSeq() == 1003U);
}

TEST_CASE("AdaptiveJitterBuffer reorders and drops duplicates", "[network][jitter]")
{
    AdaptiveJitterBuffer buffer(4U, 40000U);
    std::vector<BufferedFrame*> readyFrames;

    const std::string first = "first";
    const std::string second = "second";
    const std::string third = "third";

    REQUIRE(buffer.processFrame(2000U, reinterpret_cast<const uint8_t*>(first.data()), (uint32_t)first.size(), readyFrames));
    cleanupFrames(readyFrames);

    REQUIRE(buffer.processFrame(2002U, reinterpret_cast<const uint8_t*>(third.data()), (uint32_t)third.size(), readyFrames));
    REQUIRE(readyFrames.empty());

    REQUIRE(buffer.processFrame(2001U, reinterpret_cast<const uint8_t*>(second.data()), (uint32_t)second.size(), readyFrames));
    REQUIRE(readyFrames.size() == 2U);
    REQUIRE(readyFrames[0U]->seq == 2001U);
    REQUIRE(readyFrames[1U]->seq == 2002U);
    REQUIRE(frameToString(readyFrames[0U]) == second);
    REQUIRE(frameToString(readyFrames[1U]) == third);
    cleanupFrames(readyFrames);

    REQUIRE_FALSE(buffer.processFrame(2002U, reinterpret_cast<const uint8_t*>(third.data()), (uint32_t)third.size(), readyFrames));
    REQUIRE(readyFrames.empty());

    uint64_t totalFrames = 0ULL;
    uint64_t reorderedFrames = 0ULL;
    uint64_t droppedFrames = 0ULL;
    uint64_t timedOutFrames = 0ULL;
    buffer.getStatistics(totalFrames, reorderedFrames, droppedFrames, timedOutFrames);
    REQUIRE(totalFrames == 4U);
    REQUIRE(reorderedFrames == 1U);
    REQUIRE(droppedFrames == 1U);
    REQUIRE(timedOutFrames == 0U);
}

TEST_CASE("AdaptiveJitterBuffer drops the oldest buffered frame on overflow", "[network][jitter]")
{
    AdaptiveJitterBuffer buffer(1U, 1000U);
    std::vector<BufferedFrame*> readyFrames;
    std::vector<BufferedFrame*> timedOutFrames;

    const std::string base = "base";
    const std::string older = "older";
    const std::string newer = "newer";

    REQUIRE(buffer.processFrame(3000U, reinterpret_cast<const uint8_t*>(base.data()), (uint32_t)base.size(), readyFrames));
    cleanupFrames(readyFrames);

    REQUIRE(buffer.processFrame(3002U, reinterpret_cast<const uint8_t*>(older.data()), (uint32_t)older.size(), readyFrames));
    REQUIRE(readyFrames.empty());

    REQUIRE(buffer.processFrame(3003U, reinterpret_cast<const uint8_t*>(newer.data()), (uint32_t)newer.size(), readyFrames));
    REQUIRE(readyFrames.empty());

    buffer.checkTimeouts(timedOutFrames, UINT64_MAX);
    REQUIRE(timedOutFrames.size() == 1U);
    REQUIRE(timedOutFrames[0U]->seq == 3003U);
    REQUIRE(frameToString(timedOutFrames[0U]) == newer);
    cleanupFrames(timedOutFrames);

    uint64_t totalFrames = 0ULL;
    uint64_t reorderedFrames = 0ULL;
    uint64_t droppedFrames = 0ULL;
    uint64_t timedOutFramesCount = 0ULL;
    buffer.getStatistics(totalFrames, reorderedFrames, droppedFrames, timedOutFramesCount);
    REQUIRE(totalFrames == 3U);
    REQUIRE(reorderedFrames == 2U);
    REQUIRE(droppedFrames == 1U);
    REQUIRE(timedOutFramesCount == 1U);
}

TEST_CASE("AdaptiveJitterBuffer handles wraparound and timeout delivery", "[network][jitter]")
{
    AdaptiveJitterBuffer buffer(4U, 1000U);
    std::vector<BufferedFrame*> readyFrames;
    std::vector<BufferedFrame*> timedOutFrames;

    const std::string tail = "tail";
    const std::string head = "head";

    REQUIRE(buffer.processFrame(65535U, reinterpret_cast<const uint8_t*>(tail.data()), (uint32_t)tail.size(), readyFrames));
    REQUIRE(readyFrames.size() == 1U);
    REQUIRE(readyFrames[0U]->seq == 65535U);
    cleanupFrames(readyFrames);

    REQUIRE(buffer.processFrame(0U, reinterpret_cast<const uint8_t*>(head.data()), (uint32_t)head.size(), readyFrames));
    REQUIRE(readyFrames.size() == 1U);
    REQUIRE(readyFrames[0U]->seq == 0U);
    cleanupFrames(readyFrames);

    REQUIRE(buffer.processFrame(2U, reinterpret_cast<const uint8_t*>(head.data()), (uint32_t)head.size(), readyFrames));
    REQUIRE(readyFrames.empty());
    buffer.checkTimeouts(timedOutFrames, UINT64_MAX);
    REQUIRE(timedOutFrames.size() == 1U);
    REQUIRE(timedOutFrames[0U]->seq == 2U);
    cleanupFrames(timedOutFrames);

    REQUIRE(buffer.getNextExpectedSeq() == 3U);
}

TEST_CASE("AdaptiveJitterBuffer reset clears buffered frames and stats", "[network][jitter]")
{
    AdaptiveJitterBuffer buffer(4U, 1000U);
    std::vector<BufferedFrame*> readyFrames;

    const std::string payload = "payload";
    REQUIRE(buffer.processFrame(10U, reinterpret_cast<const uint8_t*>(payload.data()), (uint32_t)payload.size(), readyFrames));
    cleanupFrames(readyFrames);
    REQUIRE(buffer.processFrame(12U, reinterpret_cast<const uint8_t*>(payload.data()), (uint32_t)payload.size(), readyFrames));
    REQUIRE(buffer.getBufferSize() == 1U);

    buffer.reset(true);
    REQUIRE(buffer.getBufferSize() == 0U);
    REQUIRE(buffer.getNextExpectedSeq() == 0U);

    uint64_t totalFrames = 0ULL;
    uint64_t reorderedFrames = 0ULL;
    uint64_t droppedFrames = 0ULL;
    uint64_t timedOutFrames = 0ULL;
    buffer.getStatistics(totalFrames, reorderedFrames, droppedFrames, timedOutFrames);
    REQUIRE(totalFrames == 0U);
    REQUIRE(reorderedFrames == 0U);
    REQUIRE(droppedFrames == 0U);
    REQUIRE(timedOutFrames == 0U);
}