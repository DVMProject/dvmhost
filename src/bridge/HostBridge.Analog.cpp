// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Bridge
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024-2026 Bryan Biedenkapp, N2PLL
 *  Copyright (C) 2025 Caleb, K4PHP
 *  Copyright (C) 2025 Lorenzo L Romero, K2LLR
 *
 */
#include "Defines.h"
#include "common/analog/AnalogDefines.h"
#include "common/analog/AnalogAudio.h"
#include "common/analog/data/NetData.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "bridge/ActivityLog.h"
#include "HostBridge.h"
#include "BridgeMain.h"

using namespace analog;
using namespace analog::defines;
using namespace network;
using namespace network::frame;
using namespace network::udp;

#include <cstdio>
#include <algorithm>
#include <functional>
#include <random>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/*
** Analog
*/

/* Helper to process analog network traffic. */

void HostBridge::processAnalogNetwork(uint8_t* buffer, uint32_t length)
{
    assert(buffer != nullptr);
    using namespace analog;
    using namespace analog::defines;

    if (m_txMode != TX_MODE_ANALOG)
        return;

    // process network message header
    uint8_t seqNo = buffer[4U];

    uint32_t srcId = GET_UINT24(buffer, 5U);
    uint32_t dstId = GET_UINT24(buffer, 8U);

    bool individual = (buffer[15] & 0x40U) == 0x40U;

    AudioFrameType::E frameType = (AudioFrameType::E)(buffer[15U] & 0x0FU);

    data::NetData analogData;
    analogData.setSeqNo(seqNo);
    analogData.setSrcId(srcId);
    analogData.setDstId(dstId);
    analogData.setFrameType(frameType);

    analogData.setAudio(buffer + 20U);

    uint8_t frame[AUDIO_SAMPLES_LENGTH_BYTES];
    analogData.getAudio(frame);

    if (m_debug) {
        LogDebug(LOG_NET, "Analog, seqNo = %u, srcId = %u, dstId = %u, len = %u", seqNo, srcId, dstId, length);
    }

    // ignore network traffic entirely when local audio detect or
    //  traffic from UDP is running
    if (m_audioDetect || m_trafficFromUDP)
        return;

    if (!individual) {
        if (srcId == 0)
            return;

        // ensure destination ID matches and slot matches
        if (dstId != m_dstId)
            return;

        m_networkWatchdog.start();

        // is this a new call stream?
        if (m_network->getAnalogStreamId() != m_rxStreamId && !m_callInProgress) {
            m_callInProgress = true;
            m_callAlgoId = 0U;

            uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            m_rxStartTime = now;

            LogInfoEx(LOG_HOST, "Analog, call start, srcId = %u, dstId = %u", srcId, dstId);
            if (m_preambleLeaderTone)
                generatePreambleTone();
        }

        // process call termination
        if (frameType == AudioFrameType::TERMINATOR) {
            m_callInProgress = false;
            m_networkWatchdog.stop();
            m_ignoreCall = false;
            m_callAlgoId = 0U;

            if (m_rxStartTime > 0U) {
                uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                uint64_t diff = now - m_rxStartTime;

                LogInfoEx(LOG_HOST, "Analog, call end, srcId = %u, dstId = %u, dur = %us", srcId, dstId, diff / 1000U);
            }

            m_rxStartTime = 0U;
            m_rxStreamId = 0U;

            if (!m_udpRTPContinuousSeq) {
                m_rtpInitialFrame = false;
                m_rtpSeqNo = 0U;
            }
            m_rtpTimestamp = INVALID_TS;
            m_network->resetAnalog();
            return;
        }

        if (m_ignoreCall && m_callAlgoId == 0U)
            m_ignoreCall = false;

        if (m_ignoreCall)
            return;

        // decode audio frames
        if (frameType == AudioFrameType::VOICE_START || frameType == AudioFrameType::VOICE) {
            LogInfoEx(LOG_NET, ANO_VOICE ", audio, srcId = %u, dstId = %u, seqNo = %u", srcId, dstId, analogData.getSeqNo());

            short samples[AUDIO_SAMPLES_LENGTH];
            int smpIdx = 0;
            for (uint32_t pcmIdx = 0; pcmIdx < AUDIO_SAMPLES_LENGTH; pcmIdx++) {
                samples[smpIdx] = AnalogAudio::decodeMuLaw(frame[pcmIdx]);
                smpIdx++;
            }

            // post-process: apply gain to decoded audio frames
            AnalogAudio::gain(samples, AUDIO_SAMPLES_LENGTH, m_rxAudioGain);

            if (m_localAudio) {
                m_outputAudio.addData(samples, AUDIO_SAMPLES_LENGTH);
            }

            if (m_udpAudio) {
                int pcmIdx = 0;
                uint8_t pcm[AUDIO_SAMPLES_LENGTH * 2U];
                if (m_udpUseULaw) {
                    for (uint32_t smpIdx = 0; smpIdx < AUDIO_SAMPLES_LENGTH; smpIdx++) {
                        pcm[smpIdx] = AnalogAudio::encodeMuLaw(samples[smpIdx]);
                    }

                    if (m_trace)
                        Utils::dump(1U, "HostBridge()::processAnalogNetwork(), Encoded uLaw Audio", pcm, AUDIO_SAMPLES_LENGTH);

                    writeUDPAudio(srcId, dstId, pcm, AUDIO_SAMPLES_LENGTH_BYTES / 2U);
                } else {
                    for (uint32_t smpIdx = 0; smpIdx < AUDIO_SAMPLES_LENGTH; smpIdx++) {
                        pcm[pcmIdx + 0] = (uint8_t)(samples[smpIdx] & 0xFF);
                        pcm[pcmIdx + 1] = (uint8_t)((samples[smpIdx] >> 8) & 0xFF);
                        pcmIdx += 2;
                    }

                    writeUDPAudio(srcId, dstId, pcm, AUDIO_SAMPLES_LENGTH_BYTES);
                }
            }
        }

        m_rxStreamId = m_network->getAnalogStreamId();
    }
}

/* Helper to encode analog network traffic audio frames. */

void HostBridge::encodeAnalogAudioFrame(uint8_t* pcm, uint32_t forcedSrcId, uint32_t forcedDstId)
{
    assert(pcm != nullptr);
    using namespace analog;
    using namespace analog::defines;
    using namespace analog::data;

    if (m_analogN == 254U)
        m_analogN = 0;

    int smpIdx = 0;
    short samples[AUDIO_SAMPLES_LENGTH];
    for (uint32_t pcmIdx = 0; pcmIdx < (AUDIO_SAMPLES_LENGTH * 2U); pcmIdx += 2) {
        samples[smpIdx] = (short)((pcm[pcmIdx + 1] << 8) + pcm[pcmIdx + 0]);
        smpIdx++;
    }

    // pre-process: apply gain to PCM audio frames
    AnalogAudio::gain(samples, AUDIO_SAMPLES_LENGTH, m_txAudioGain);

    uint32_t srcId = m_srcId;
    if (m_srcIdOverride != 0 && (m_overrideSrcIdFromMDC))
        srcId = m_srcIdOverride;
    if (m_overrideSrcIdFromUDP)
        srcId = m_udpSrcId;
    if (forcedSrcId > 0 && forcedSrcId != m_srcId)
        srcId = forcedSrcId;
    uint32_t dstId = m_dstId;
    if (forcedDstId > 0 && forcedDstId != m_dstId)
        dstId = forcedDstId;

    // never allow a source ID of 0
    if (srcId == 0U)
        srcId = m_srcId;

    data::NetData analogData;
    analogData.setSeqNo(m_analogN);
    analogData.setSrcId(srcId);
    analogData.setDstId(dstId);
    analogData.setControl(0U);
    analogData.setFrameType(AudioFrameType::VOICE);
    if (m_txStreamId <= 1U) {
        analogData.setFrameType(AudioFrameType::VOICE_START);

        if (m_grantDemand) {
            analogData.setControl(0x80U); // analog remote grant demand flag
        }
    }

    int pcmIdx = 0;
    uint8_t outPcm[AUDIO_SAMPLES_LENGTH * 2U];
    for (uint32_t smpIdx = 0; smpIdx < AUDIO_SAMPLES_LENGTH; smpIdx++) {
        outPcm[smpIdx] = AnalogAudio::encodeMuLaw(samples[smpIdx]);
    }

    if (m_trace)
        Utils::dump(1U, "HostBridge()::encodeAnalogAudioFrame(), Encoded uLaw Audio", outPcm, AUDIO_SAMPLES_LENGTH);

    analogData.setAudio(outPcm);

    if (analogData.getFrameType() == AudioFrameType::VOICE) {
        LogInfoEx(LOG_HOST, ANO_VOICE ", audio, srcId = %u, dstId = %u, seqNo = %u", srcId, dstId, analogData.getSeqNo());
    }

    m_network->writeAnalog(analogData);
    m_txStreamId = m_network->getAnalogStreamId();
    m_analogN++;
}
