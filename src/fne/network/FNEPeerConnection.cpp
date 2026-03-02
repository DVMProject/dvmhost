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

/* Cleans up jitter buffer for the specified stream. */

void FNEPeerConnection::cleanupJitterBuffer(uint64_t streamId)
{
    std::lock_guard<std::mutex> lock(m_jitterMutex);

    auto it = m_jitterBuffers.find(streamId);
    if (it != m_jitterBuffers.end()) {
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
    
    // check timeouts for all active jitter buffers
    for (auto& pair : m_jitterBuffers) {
        AdaptiveJitterBuffer* buffer = pair.second;
        if (buffer != nullptr) {
            std::vector<BufferedFrame*> timedOutFrames;
            buffer->checkTimeouts(timedOutFrames);
            
            // note: timed-out frames are handled by the calling context
            // this method just ensures the buffers are checked periodically
            // the frames themselves are cleaned up by the caller
            for (BufferedFrame* frame : timedOutFrames) {
                delete frame;
            }
        }
    }
}
