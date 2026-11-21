// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/Log.h"
#include "network/FNEPeerConnection.h"

using namespace network;

#include <chrono>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Gets or creates a jitter buffer for the specified stream. */

AdaptiveJitterBuffer* FNEPeerConnection::getOrCreateJitterBuffer(uint64_t streamId)
{
    std::lock_guard<std::mutex> lock(m_jitterMutex);

    if (m_jitterBuffers.find(streamId) == m_jitterBuffers.end()) {
        m_jitterBuffers[streamId] = new AdaptiveJitterBuffer(m_jitterMaxSize, m_jitterMaxWait);
    }

    return m_jitterBuffers[streamId];
}

/* Processes a frame through the jitter buffer for the specified stream. */

bool FNEPeerConnection::processJitterFrame(uint64_t streamId, uint16_t seq, const uint8_t* data, uint32_t length,
    std::vector<BufferedFrame*>& readyFrames)
{
    std::lock_guard<std::mutex> lock(m_jitterMutex);

    auto it = m_jitterBuffers.find(streamId);
    if (it == m_jitterBuffers.end()) {
        m_jitterBuffers[streamId] = new AdaptiveJitterBuffer(m_jitterMaxSize, m_jitterMaxWait);
        it = m_jitterBuffers.find(streamId);
    }

    AdaptiveJitterBuffer* buffer = (it != m_jitterBuffers.end()) ? it->second : nullptr;
    if (buffer == nullptr) {
        return false;
    }

    return buffer->processFrame(seq, data, length, readyFrames);
}

/* Cleans up jitter buffer for the specified stream. */

void FNEPeerConnection::cleanupJitterBuffer(uint64_t streamId)
{
    std::lock_guard<std::mutex> lock(m_jitterMutex);

    auto it = m_jitterBuffers.find(streamId);
    if (it != m_jitterBuffers.end()) {
        uint64_t totalFrames = 0ULL, reorderedFrames = 0ULL, droppedFrames = 0ULL, timedOutFrames = 0ULL;
        AdaptiveJitterBuffer* buffer = it->second;
        buffer->getStatistics(totalFrames, reorderedFrames, droppedFrames, timedOutFrames);

        if (reorderedFrames != 0ULL || droppedFrames != 0ULL || timedOutFrames != 0ULL) {
            LogInfoEx(LOG_MASTER, "PEER %u (%s) jitter stream %u stats, total = %llu, reordered = %llu, timedOut = %llu, dropped = %llu",
                id(), identWithQualifier().c_str(), (uint32_t)streamId,
                (uint64_t)totalFrames, (uint64_t)reorderedFrames, (uint64_t)timedOutFrames, (uint64_t)droppedFrames);
        }

        delete it->second;
        m_jitterBuffers.erase(it);
    }
}

/* Checks for timed-out buffered frames across all streams. */

void FNEPeerConnection::checkJitterTimeouts()
{
    if (!m_jitterBufferEnabled) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_jitterMutex);
    uint64_t currentTime = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // check timeouts for all active jitter buffers
    for (auto& pair : m_jitterBuffers) {
        AdaptiveJitterBuffer* buffer = pair.second;
        if (buffer != nullptr) {
            std::vector<BufferedFrame*> timedOutFrames;
            buffer->checkTimeouts(timedOutFrames, currentTime);
            
            for (BufferedFrame* frame : timedOutFrames) {
                if (frame != nullptr) {
                    LogWarning(LOG_MASTER, "PEER %u (%s) jitter stream %u timeout; delivered seq %u after %lluus",
                        id(), identWithQualifier().c_str(), (uint32_t)pair.first, (uint32_t)frame->seq,
                        (uint64_t)(currentTime - frame->timestamp));
                }
            }

            for (BufferedFrame* frame : timedOutFrames) {
                delete frame;
            }
        }
    }
}
