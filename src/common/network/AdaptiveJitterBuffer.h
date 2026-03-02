// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file AdaptiveJitterBuffer.h
 * @ingroup network_core
 * @file AdaptiveJitterBuffer.cpp
 * @ingroup network_core
 */
#if !defined(__ADAPTIVE_JITTER_BUFFER_H__)
#define __ADAPTIVE_JITTER_BUFFER_H__

#include "common/Defines.h"

#include <cstdint>
#include <cstring>
#include <map>
#include <vector>
#include <mutex>
#include <chrono>

namespace network
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    #define DEFAULT_JITTER_MAX_SIZE 4U
    #define DEFAULT_JITTER_MAX_WAIT 40000U

    #define MIN_JITTER_MAX_SIZE 2U
    #define MAX_JITTER_MAX_SIZE 8U

    #define MIN_JITTER_MAX_WAIT 10000U
    #define MAX_JITTER_MAX_WAIT 200000U

    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents a buffered frame in the jitter buffer.
     * @ingroup network_core
     */
    struct BufferedFrame {
        uint16_t seq;                       //<! RTP sequence number
        uint8_t* data;                      //<! Frame data
        uint32_t length;                    //<! Frame length
        uint64_t timestamp;                 //<! Reception timestamp (microseconds)

        /**
         * @brief Initializes a new instance of the BufferedFrame struct.
         */
        BufferedFrame() : seq(0U), data(nullptr), length(0U), timestamp(0ULL) { /* stub */ }

        /**
         * @brief Initializes a new instance of the BufferedFrame struct.
         * @param sequence RTP sequence number.
         * @param buffer Frame data buffer.
         * @param len Frame length.
         */
        BufferedFrame(uint16_t sequence, const uint8_t* buffer, uint32_t len) :
            seq(sequence),
            data(nullptr),
            length(len),
            timestamp(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count())
        {
            if (len > 0U && buffer != nullptr) {
                data = new uint8_t[len];
                ::memcpy(data, buffer, len);
            }
        }
        
        /**
         * @brief Finalizes a instance of the BufferedFrame struct.
         */
        ~BufferedFrame()
        {
            if (data != nullptr) {
                delete[] data;
                data = nullptr;
            }
        }
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements an adaptive jitter buffer for RTP streams.
     * @ingroup network_core
     * 
     * This class provides minimal-latency jitter buffering with a zero-latency
     * fast path for in-order packets. Out-of-order packets are buffered briefly
     * to allow reordering, with adaptive timeout based on observed jitter.
     */
    class HOST_SW_API AdaptiveJitterBuffer {
    public:
        /**
         * @brief Initializes a new instance of the AdaptiveJitterBuffer class.
         * @param maxBufferSize Maximum number of frames to buffer (default: 4).
         * @param maxWaitTime Maximum time to wait for out-of-order frames in microseconds (default: 40000 = 40ms).
         */
        AdaptiveJitterBuffer(uint16_t maxBufferSize = 4U, uint32_t maxWaitTime = 40000U);
        
        /**
         * @brief Finalizes a instance of the AdaptiveJitterBuffer class.
         */
        ~AdaptiveJitterBuffer();

        /**
         * @brief Processes an incoming RTP frame.
         * @param seq RTP sequence number.
         * @param data Frame data.
         * @param length Frame length.
         * @param[out] readyFrames Vector of frames ready for delivery (in sequence order).
         * @returns bool True if frame was processed successfully, otherwise false.
         * 
         * This method implements a zero-latency fast path for in-order packets.
         * Out-of-order packets are buffered and returned when they become sequential.
         */
        bool processFrame(uint16_t seq, const uint8_t* data, uint32_t length, 
            std::vector<BufferedFrame*>& readyFrames);

        /**
         * @brief Checks for timed-out buffered frames and forces their delivery.
         * @param[out] timedOutFrames Vector of frames that have exceeded the wait time.
         * @param currentTime Current time in microseconds (0 = use system clock).
         * 
         * This should be called periodically (e.g., every 10-20ms) to ensure
         * buffered frames are delivered even if missing packets never arrive.
         */
        void checkTimeouts(std::vector<BufferedFrame*>& timedOutFrames, 
            uint64_t currentTime = 0ULL);

        /**
         * @brief Resets the jitter buffer state.
         * @param clearStats If true, also resets statistics (default: false).
         * 
         * This should be called when a stream ends or restarts.
         */
        void reset(bool clearStats = false);

        /**
         * @brief Gets the current buffer occupancy.
         * @returns size_t Number of frames currently buffered.
         */
        size_t getBufferSize() const { return m_buffer.size(); }

        /**
         * @brief Gets the next expected sequence number.
         * @returns uint16_t Next expected sequence number.
         */
        uint16_t getNextExpectedSeq() const { return m_nextExpectedSeq; }

        /**
         * @brief Gets statistics about jitter buffer performance.
         * @param[out] totalFrames Total frames processed.
         * @param[out] reorderedFrames Frames that were out-of-order but successfully reordered.
         * @param[out] droppedFrames Frames dropped due to buffer overflow or severe reordering.
         * @param[out] timedOutFrames Frames delivered due to timeout (missing packets).
         */
        void getStatistics(uint64_t& totalFrames, uint64_t& reorderedFrames, 
            uint64_t& droppedFrames, uint64_t& timedOutFrames) const;

        /**
         * @brief Sets the maximum buffer size.
         * @param maxBufferSize Maximum number of frames to buffer.
         */
        void setMaxBufferSize(uint16_t maxBufferSize) { m_maxBufferSize = maxBufferSize; }

        /**
         * @brief Sets the maximum wait time for out-of-order frames.
         * @param maxWaitTime Maximum wait time in microseconds.
         */
        void setMaxWaitTime(uint32_t maxWaitTime) { m_maxWaitTime = maxWaitTime; }

    private:
        std::map<uint16_t, BufferedFrame*> m_buffer;
        mutable std::mutex m_mutex;
        
        uint16_t m_nextExpectedSeq;
        uint16_t m_maxBufferSize;
        uint32_t m_maxWaitTime;
        
        uint64_t m_totalFrames;
        uint64_t m_reorderedFrames;
        uint64_t m_droppedFrames;
        uint64_t m_timedOutFrames;
        
        bool m_initialized;
        
        /**
         * @brief Delivers all sequential frames from the buffer.
         * @param[out] readyFrames Vector to append ready frames to.
         * 
         * Internal helper that flushes all frames starting from m_nextExpectedSeq
         * until a gap is encountered.
         */
        void flushSequentialFrames(std::vector<BufferedFrame*>& readyFrames);

        /**
         * @brief Calculates sequence number difference handling wraparound.
         * @param seq1 First sequence number.
         * @param seq2 Second sequence number.
         * @returns int32_t Signed difference (seq1 - seq2).
         */
        int32_t seqDiff(uint16_t seq1, uint16_t seq2) const;
    };
} // namespace network

#endif // __ADAPTIVE_JITTER_BUFFER_H__
