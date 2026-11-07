// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 */
/**
 * @file OrderedFrameQueue.h
 * @ingroup fne_callhandler
 */
#if !defined(__CALLHANDLER__ORDERED_FRAME_QUEUE_H__)
#define __CALLHANDLER__ORDERED_FRAME_QUEUE_H__

#include "common/ThreadPool.h"
#include "common/Log.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace network
{
    namespace callhandler
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Serializes frames within one traffic stream while allowing independent streams to run concurrently.
         * @ingroup fne_callhandler
         */
        class OrderedFrameQueue {
        public:
            /**
             * @brief Function invoked to process one ordered frame.
             */
            using FrameHandler = std::function<void(const uint8_t*, uint32_t, uint32_t, uint32_t,
                uint16_t, uint32_t, bool)>;
            /**
             * @brief Function invoked when ordered work cannot be scheduled.
             */
            using FailureHandler = std::function<void()>;

            /**
             * @brief Initializes an ordered frame queue.
             * @param pool Shared worker pool used to process frames.
             * @param handler Function invoked for each frame.
             * @param failureHandler Function invoked when scheduling fails.
             */
            OrderedFrameQueue(ThreadPool& pool, FrameHandler handler, FailureHandler failureHandler) :
                m_pool(pool),
                m_handler(std::move(handler)),
                m_failureHandler(std::move(failureHandler)),
                m_streamsMutex(),
                m_streams()
#if defined(CATCH2_TEST_COMPILATION)
                , m_testFrameHandler(nullptr)
#endif
            {
                /* stub */
            }

            /**
             * @brief Copies and immediately schedules a frame in arrival order for its stream.
             * @param data Network data buffer copied before this method returns.
             * @param len Length of data in bytes.
             * @param peerId Peer ID identifying the packet origin.
             * @param ssrc RTP synchronization source ID.
             * @param pktSeq RTP packet sequence.
             * @param streamId Call stream ID.
             * @param fromUpstream Flag indicating traffic is from an upstream master.
             * @returns bool True if the frame was queued, otherwise false.
             */
            bool enqueue(const uint8_t* data, uint32_t len, uint32_t peerId, uint32_t ssrc, uint16_t pktSeq, uint32_t streamId, bool fromUpstream)
            {
                if (data == nullptr || len == 0U)
                    return false;

                const StreamKey key{ peerId, ssrc, streamId, fromUpstream };
                Frame frame{ std::vector<uint8_t>(data, data + len), peerId, ssrc, pktSeq, streamId, fromUpstream };
                bool schedule = false;
                
                // scope is intentional
                {
                    std::lock_guard<std::mutex> lock(m_streamsMutex);

                    // retrieve or create the stream state for this key
                    StreamState& state = m_streams[key];
                    state.frames.emplace_back(std::move(frame));
                    if (!state.scheduled) {
                        state.scheduled = true;
                        schedule = true;
                    }
                }

                // if we don't need to schedule, we're done
                if (!schedule)
                    return true;

                // attempt to schedule the task in the pool
                if (m_pool.enqueue(new_pooltask(taskDrain, this, key)))
                    return true;

                // scope is intentional
                {
                    std::lock_guard<std::mutex> lock(m_streamsMutex);
                    m_streams.erase(key);
                }

                LogError(LOG_NET, "Failed to schedule ordered frame, peerId = %u, ssrc = %u, streamId = %u, fromUpstream = %u", peerId, ssrc, streamId, fromUpstream);
                if (m_failureHandler != nullptr)
                    m_failureHandler();

                return false;
            }

#if defined(CATCH2_TEST_COMPILATION)
            /**
             * @brief Installs a test callback in place of normal frame processing.
             * @param handler Callback receiving the complete ordered frame metadata.
             */
            void setTestFrameHandler(FrameHandler handler)
            {
                m_testFrameHandler = std::move(handler);
            }
#endif

        private:
            /** 
             * @brief Identifies one independently ordered receive stream. 
             * @ingroup fne_callhandler 
             */
            struct StreamKey {
                uint32_t peerId;        //!< Peer ID identifying the packet origin.
                uint32_t ssrc;          //!< RTP synchronization source ID.
                uint32_t streamId;      //!< Call stream ID.
                bool fromUpstream;      //!< Flag indicating traffic is from an upstream master.

                /** 
                 * @brief Compares stream keys. 
                 * @param other Key to compare. 
                 * @returns bool True when equal. 
                 */
                bool operator==(const StreamKey& other) const
                {
                    return peerId == other.peerId && ssrc == other.ssrc && streamId == other.streamId && fromUpstream == other.fromUpstream;
                }
            };

            /** 
             * @brief Computes a stream-key hash. 
             * @ingroup fne_callhandler 
             */
            struct StreamKeyHash {
                /** 
                 * @brief Computes a hash. 
                 * @param key Key to hash. 
                 * @returns size_t Hash value. 
                 */
                size_t operator()(const StreamKey& key) const
                {
                    size_t hash = std::hash<uint32_t>()(key.peerId);
                    hash ^= std::hash<uint32_t>()(key.ssrc) + 0x9E3779B9U + (hash << 6U) + (hash >> 2U);
                    hash ^= std::hash<uint32_t>()(key.streamId) + 0x9E3779B9U + (hash << 6U) + (hash >> 2U);
                    hash ^= std::hash<bool>()(key.fromUpstream) + 0x9E3779B9U + (hash << 6U) + (hash >> 2U);
                    return hash;
                }
            };

            /** 
             * @brief Owns one frame awaiting ordered processing. 
             * @ingroup fne_callhandler 
             */
            struct Frame {
                std::vector<uint8_t> data; //!< Copied network frame bytes.
                uint32_t peerId;           //!< Peer ID identifying the packet origin.
                uint32_t ssrc;             //!< RTP synchronization source ID.
                uint16_t pktSeq;           //!< RTP packet sequence.
                uint32_t streamId;         //!< Call stream ID.
                bool fromUpstream;         //!< Flag indicating traffic is from an upstream master.
            };

            /** 
             * @brief Stores queued frames for one stream. 
             * @ingroup fne_callhandler 
             */
            struct StreamState {
                std::deque<Frame> frames;  //!< Frames in packet arrival order.
                bool scheduled = false;    //!< Whether a drain task is scheduled or running.
            };

            ThreadPool& m_pool;
            FrameHandler m_handler;
            FailureHandler m_failureHandler;
            std::mutex m_streamsMutex;
            std::unordered_map<StreamKey, StreamState, StreamKeyHash> m_streams;
#if defined(CATCH2_TEST_COMPILATION)
            FrameHandler m_testFrameHandler;
#endif

            /** 
             * @brief Processes one frame and reschedules remaining work. 
             * @param queue Queue to drain. 
             * @param key Stream to drain. 
             */
            static void taskDrain(OrderedFrameQueue* queue, StreamKey key)
            {
                if (queue == nullptr)
                    return;

                Frame frame;

                // scope is intentional
                {
                    std::lock_guard<std::mutex> lock(queue->m_streamsMutex);
                    auto it = queue->m_streams.find(key);
                    if (it == queue->m_streams.end() || it->second.frames.empty())
                        return;

                    frame = std::move(it->second.frames.front());
                    it->second.frames.pop_front();
                }

                // process the frame outside the lock to avoid blocking other streams
#if defined(CATCH2_TEST_COMPILATION)
                if (queue->m_testFrameHandler != nullptr)
                    queue->m_testFrameHandler(frame.data.data(), static_cast<uint32_t>(frame.data.size()),
                        frame.peerId, frame.ssrc, frame.pktSeq, frame.streamId, frame.fromUpstream);
                else
#endif
                    queue->m_handler(frame.data.data(), static_cast<uint32_t>(frame.data.size()),
                        frame.peerId, frame.ssrc, frame.pktSeq, frame.streamId, frame.fromUpstream);

                bool schedule = false;

                // scope is intentional
                {
                    std::lock_guard<std::mutex> lock(queue->m_streamsMutex);

                    // check if the stream still exists and has frames
                    auto it = queue->m_streams.find(key);
                    if (it == queue->m_streams.end())
                        return;

                    // if the stream has no more frames, remove it from the map
                    if (it->second.frames.empty())
                        queue->m_streams.erase(it);
                    else
                        schedule = true;
                }

                // if we need to schedule the next task and fail, handle the failure case
                if (schedule && !queue->m_pool.enqueue(new_pooltask(taskDrain, queue, key))) {
                    // scope is intentional
                    {
                        std::lock_guard<std::mutex> lock(queue->m_streamsMutex);
                        queue->m_streams.erase(key);
                    }

                    LogError(LOG_NET, "Failed to reschedule ordered stream, peerId = %u, ssrc = %u, streamId = %u, fromUpstream = %u", key.peerId, key.ssrc, key.streamId, key.fromUpstream);
                    if (queue->m_failureHandler != nullptr)
                        queue->m_failureHandler();
                }
            }
        };
    } // namespace callhandler
} // namespace network

#endif // __CALLHANDLER__ORDERED_FRAME_QUEUE_H__
