// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/Log.h"
#include "network/AdaptiveJitterBuffer.h"

using namespace network;

#include <algorithm>
#include <cassert>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define RTP_SEQ_MOD (1U << 16)                          // 65536

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the AdaptiveJitterBuffer class. */

AdaptiveJitterBuffer::AdaptiveJitterBuffer(uint16_t maxBufferSize, uint32_t maxWaitTime) :
    m_buffer(),
    m_mutex(),
    m_nextExpectedSeq(0U),
    m_maxBufferSize(maxBufferSize),
    m_maxWaitTime(maxWaitTime),
    m_totalFrames(0ULL),
    m_reorderedFrames(0ULL),
    m_droppedFrames(0ULL),
    m_timedOutFrames(0ULL),
    m_initialized(false)
{
    assert(maxBufferSize > 0U);
    assert(maxWaitTime > 0U);
}

/* Finalizes a instance of the AdaptiveJitterBuffer class. */

AdaptiveJitterBuffer::~AdaptiveJitterBuffer()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // clean up any buffered frames
    for (auto& pair : m_buffer) {
        if (pair.second != nullptr) {
            delete pair.second;
        }
    }
    m_buffer.clear();
}

/* Processes an incoming RTP frame. */

bool AdaptiveJitterBuffer::processFrame(uint16_t seq, const uint8_t* data, uint32_t length,
    std::vector<BufferedFrame*>& readyFrames)
{
    if (data == nullptr || length == 0U) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_totalFrames++;

    // initialize on first frame
    if (!m_initialized) {
        m_nextExpectedSeq = seq;
        m_initialized = true;
    }

    // zero-latency fast path: in-order packet
    if (seq == m_nextExpectedSeq) {
        // create frame and add to ready list
        BufferedFrame* frame = new BufferedFrame(seq, data, length);
        readyFrames.push_back(frame);

        // advance expected sequence
        m_nextExpectedSeq = (m_nextExpectedSeq + 1) & 0xFFFF;

        // flush any subsequent sequential frames from buffer
        flushSequentialFrames(readyFrames);

        return true;
    }

    int32_t diff = seqDiff(seq, m_nextExpectedSeq);

    // frame is in the past (duplicate or very late)
    if (diff < 0) {
        // check if it's severely out of order (> 1000 packets behind)
        if (diff < -1000) {
            // likely a sequence wraparound with new stream - reset
            m_nextExpectedSeq = seq;

            // cleanup any buffered frames, delete and clear list
            for (auto& pair : m_buffer) {
                if (pair.second != nullptr) {
                    delete pair.second;
                }
            }
            m_buffer.clear();

            BufferedFrame* frame = new BufferedFrame(seq, data, length);
            readyFrames.push_back(frame);
            m_nextExpectedSeq = (m_nextExpectedSeq + 1) & 0xFFFF;
            return true;
        }
        
        // drop duplicate/late frame
        m_droppedFrames++;
        return false;
    }

    // frame is in the future - buffer it
    m_reorderedFrames++;

    // check buffer capacity
    if (m_buffer.size() >= m_maxBufferSize) {
        // buffer is full - drop oldest frame to make room
        auto oldestIt = m_buffer.begin();
        delete oldestIt->second;
        m_buffer.erase(oldestIt);
        m_droppedFrames++;
    }

    // add frame to buffer
    BufferedFrame* frame = new BufferedFrame(seq, data, length);
    m_buffer[seq] = frame;

    // check if we now have the next expected frame
    flushSequentialFrames(readyFrames);

    return true;
}

/* Checks for timed-out buffered frames and forces their delivery. */

void AdaptiveJitterBuffer::checkTimeouts(std::vector<BufferedFrame*>& timedOutFrames,
    uint64_t currentTime)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_buffer.empty()) {
        return;
    }

    // get current time if not provided
    if (currentTime == 0ULL) {
        currentTime = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    // find frames that have exceeded the wait time
    std::vector<uint16_t> toRemove;
    for (auto& pair : m_buffer) {
        BufferedFrame* frame = pair.second;
        if (frame != nullptr) {
            uint64_t age = currentTime - frame->timestamp;

            if (age >= m_maxWaitTime) {
                toRemove.push_back(pair.first);
            }
        }
    }

    // remove and deliver timed-out frames in sequence order
    if (!toRemove.empty()) {
        // sort by sequence number
        std::sort(toRemove.begin(), toRemove.end(), [this](uint16_t a, uint16_t b) {
            return seqDiff(a, b) < 0;
        });

        for (uint16_t seq : toRemove) {
            auto it = m_buffer.find(seq);
            if (it != m_buffer.end() && it->second != nullptr) {
                timedOutFrames.push_back(it->second);
                m_buffer.erase(it);
                m_timedOutFrames++;

                // update next expected sequence to skip the gap
                int32_t diff = seqDiff(seq, m_nextExpectedSeq);
                if (diff >= 0) {
                    m_nextExpectedSeq = (seq + 1) & 0xFFFF;

                    // try to flush any sequential frames after this one
                    flushSequentialFrames(timedOutFrames);
                }
            }
        }
    }
}

/* Resets the jitter buffer state. */

void AdaptiveJitterBuffer::reset(bool clearStats)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // clean up buffered frames
    for (auto& pair : m_buffer) {
        if (pair.second != nullptr) {
            delete pair.second;
        }
    }
    m_buffer.clear();

    m_initialized = false;
    m_nextExpectedSeq = 0U;

    if (clearStats) {
        m_totalFrames = 0ULL;
        m_reorderedFrames = 0ULL;
        m_droppedFrames = 0ULL;
        m_timedOutFrames = 0ULL;
    }
}

/* Gets statistics about jitter buffer performance. */

void AdaptiveJitterBuffer::getStatistics(uint64_t& totalFrames, uint64_t& reorderedFrames,
    uint64_t& droppedFrames, uint64_t& timedOutFrames) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    totalFrames = m_totalFrames;
    reorderedFrames = m_reorderedFrames;
    droppedFrames = m_droppedFrames;
    timedOutFrames = m_timedOutFrames;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Delivers all sequential frames from the buffer. */

void AdaptiveJitterBuffer::flushSequentialFrames(std::vector<BufferedFrame*>& readyFrames)
{
    while (!m_buffer.empty()) {
        auto it = m_buffer.find(m_nextExpectedSeq);
        if (it == m_buffer.end()) {
            // gap in sequence - stop flushing
            break;
        }

        // found next sequential frame
        BufferedFrame* frame = it->second;
        readyFrames.push_back(frame);
        m_buffer.erase(it);
        
        // advance to next expected sequence
        m_nextExpectedSeq = (m_nextExpectedSeq + 1) & 0xFFFF;
    }
}

/* Calculates sequence number difference handling wraparound. */

int32_t AdaptiveJitterBuffer::seqDiff(uint16_t seq1, uint16_t seq2) const
{
    // handle RTP sequence number wraparound (RFC 3550)
    int32_t diff = (int32_t)seq1 - (int32_t)seq2;

    // adjust for wraparound
    if (diff > (int32_t)(RTP_SEQ_MOD / 2)) {
        diff -= (int32_t)RTP_SEQ_MOD;
    } else if (diff < -(int32_t)(RTP_SEQ_MOD / 2)) {
        diff += (int32_t)RTP_SEQ_MOD;
    }
    
    return diff;
}
