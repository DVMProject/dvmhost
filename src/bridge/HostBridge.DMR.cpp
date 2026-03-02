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
#include "common/dmr/DMRDefines.h"
#include "common/dmr/data/EMB.h"
#include "common/dmr/data/NetData.h"
#include "common/dmr/lc/FullLC.h"
#include "common/dmr/SlotType.h"
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

#if !defined(_WIN32)
#include <unistd.h>
#include <pwd.h>
#endif // !defined(_WIN32)

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/*
** Digital Mobile Radio
*/

/* Helper to process DMR network traffic. */

void HostBridge::processDMRNetwork(uint8_t* buffer, uint32_t length)
{
    assert(buffer != nullptr);
    using namespace dmr;
    using namespace dmr::defines;

    if (m_txMode != TX_MODE_DMR) {
        m_network->resetDMR(1U);
        m_network->resetDMR(2U);
        return;
    }

    // process network message header
    uint8_t seqNo = buffer[4U];

    uint32_t srcId = GET_UINT24(buffer, 5U);
    uint32_t dstId = GET_UINT24(buffer, 8U);

    FLCO::E flco = (buffer[15U] & 0x40U) == 0x40U ? FLCO::PRIVATE : FLCO::GROUP;

    uint32_t slotNo = (buffer[15U] & 0x80U) == 0x80U ? 2U : 1U;

    if (slotNo > 3U) {
        LogError(LOG_DMR, "DMR, invalid slot, slotNo = %u", slotNo);
        m_network->resetDMR(1U);
        m_network->resetDMR(2U);
        return;
    }

    // DMO mode slot disabling
    if (slotNo == 1U && !m_network->getDuplex()) {
        LogError(LOG_DMR, "DMR/DMO, invalid slot, slotNo = %u", slotNo);
        m_network->resetDMR(1U);
        return;
    }

    // Individual slot disabling
    if (slotNo == 1U && !m_network->getSlot1()) {
        LogError(LOG_DMR, "DMR, invalid slot, slot 1 disabled, slotNo = %u", slotNo);
        m_network->resetDMR(1U);
        return;
    }
    if (slotNo == 2U && !m_network->getSlot2()) {
        LogError(LOG_DMR, "DMR, invalid slot, slot 2 disabled, slotNo = %u", slotNo);
        m_network->resetDMR(2U);
        return;
    }

    bool dataSync = (buffer[15U] & 0x20U) == 0x20U;
    bool voiceSync = (buffer[15U] & 0x10U) == 0x10U;

    if (m_debug) {
        LogDebug(LOG_NET, "DMR, seqNo = %u, srcId = %u, dstId = %u, flco = $%02X, slotNo = %u, len = %u", seqNo, srcId, dstId, flco, slotNo, length);
    }

    // process raw DMR data bytes
    UInt8Array data = std::unique_ptr<uint8_t[]>(new uint8_t[DMR_FRAME_LENGTH_BYTES]);
    ::memset(data.get(), 0x00U, DMR_FRAME_LENGTH_BYTES);
    DataType::E dataType = DataType::VOICE_SYNC;
    uint8_t n = 0U;
    if (dataSync) {
        dataType = (DataType::E)(buffer[15U] & 0x0FU);
        ::memcpy(data.get(), buffer + 20U, DMR_FRAME_LENGTH_BYTES);
    }
    else if (voiceSync) {
        ::memcpy(data.get(), buffer + 20U, DMR_FRAME_LENGTH_BYTES);
    }
    else {
        n = buffer[15U] & 0x0FU;
        dataType = DataType::VOICE;
        ::memcpy(data.get(), buffer + 20U, DMR_FRAME_LENGTH_BYTES);
    }

    // ignore network traffic entirely when local audio detect or
    //  traffic from UDP is running
    if (m_audioDetect || m_trafficFromUDP)
        return;

    if (flco == FLCO::GROUP) {
        if (srcId == 0) {
            m_network->resetDMR(slotNo);
            return;
        }

        // ensure destination ID matches and slot matches
        if (dstId != m_dstId) {
            m_network->resetDMR(slotNo);
            return;
        }
        if (slotNo != m_slot) {
            m_network->resetDMR(slotNo);
            return;
        }

        m_networkWatchdog.start();

        // is this a new call stream?
        if (m_network->getDMRStreamId(slotNo) != m_rxStreamId && !m_callInProgress) {
            m_callInProgress = true;
            m_callAlgoId = 0U;

            uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            m_rxStartTime = now;

            LogInfoEx(LOG_HOST, "DMR, call start, srcId = %u, dstId = %u, slot = %u", srcId, dstId, slotNo);
            if (m_preambleLeaderTone)
                generatePreambleTone();

            // if we can, use the LC from the voice header as to keep all options intact
            if (dataSync && (dataType == DataType::VOICE_LC_HEADER)) {
                lc::LC lc = lc::LC();
                lc::FullLC fullLC = lc::FullLC();
                lc = *fullLC.decode(data.get(), DataType::VOICE_LC_HEADER);

                m_rxDMRLC = lc;
            }
            else {
                // if we don't have a voice header; don't wait to decode it, just make a dummy header
                m_rxDMRLC = lc::LC();
                m_rxDMRLC.setDstId(dstId);
                m_rxDMRLC.setSrcId(srcId);
            }

            m_rxDMRPILC = lc::PrivacyLC();
        }

        // if we can, use the PI LC from the PI voice header as to keep all options intact
        if (dataSync && (dataType == DataType::VOICE_PI_HEADER)) {
            lc::PrivacyLC lc = lc::PrivacyLC();
            lc::FullLC fullLC = lc::FullLC();
            lc = *fullLC.decodePI(data.get());

            m_rxDMRPILC = lc;
            m_callAlgoId = lc.getAlgId();
        }

        // process call termination
        if (dataSync && (dataType == DataType::TERMINATOR_WITH_LC)) {
            m_callInProgress = false;
            m_networkWatchdog.stop();
            m_ignoreCall = false;
            m_callAlgoId = 0U;

            if (m_rxStartTime > 0U) {
                uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                uint64_t diff = now - m_rxStartTime;

                LogInfoEx(LOG_HOST, "DMR, call end, srcId = %u, dstId = %u, dur = %us", srcId, dstId, diff / 1000U);
            }
            
            m_rxDMRLC = lc::LC();
            m_rxDMRPILC = lc::PrivacyLC();
            m_rxStartTime = 0U;
            m_rxStreamId = 0U;

            if (!m_udpRTPContinuousSeq) {
                m_rtpInitialFrame = false;
                m_rtpSeqNo = 0U;
            }
            m_rtpTimestamp = INVALID_TS;
            m_network->resetDMR(slotNo);
            return;
        }

        if (m_ignoreCall && m_callAlgoId == 0U)
            m_ignoreCall = false;

        if (m_ignoreCall) {
            m_network->resetDMR(slotNo);
            return;
        }

        if (m_callAlgoId != 0U) {
            if (m_callInProgress) {
                m_callInProgress = false;

                uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                uint64_t diff = now - m_rxStartTime;

                // send USRP end of transmission
                if (m_udpUsrp)
                    sendUsrpEot();

                LogInfoEx(LOG_HOST, "P25, call end (T), srcId = %u, dstId = %u, dur = %us", srcId, dstId, diff / 1000U);
            }

            m_ignoreCall = true;
            m_network->resetDMR(slotNo);
            return;
        }

        // process audio frames
        if (dataType == DataType::VOICE_SYNC || dataType == DataType::VOICE) {
            uint8_t ambe[27U];
            ::memcpy(ambe, data.get(), 14U);
            ambe[13] &= 0xF0;
            ambe[13] |= (uint8_t)(data[19] & 0x0F);
            ::memcpy(ambe + 14U, data.get() + 20U, 13U);

            LogInfoEx(LOG_NET, DMR_DT_VOICE ", audio, slot = %u, srcId = %u, dstId = %u, seqNo = %u", slotNo, srcId, dstId, n);
            decodeDMRAudioFrame(ambe, srcId, dstId, n);
        }

        m_rxStreamId = m_network->getDMRStreamId(slotNo);
    }
}

/* Helper to decode DMR network traffic audio frames. */

void HostBridge::decodeDMRAudioFrame(uint8_t* ambe, uint32_t srcId, uint32_t dstId, uint8_t dmrN)
{
    assert(ambe != nullptr);
    using namespace dmr;
    using namespace dmr::defines;

    for (uint32_t n = 0; n < AMBE_PER_SLOT; n++) {
        uint8_t ambePartial[RAW_AMBE_LENGTH_BYTES];
        for (uint32_t i = 0; i < RAW_AMBE_LENGTH_BYTES; i++)
            ambePartial[i] = ambe[i + (n * 9)];

        short samples[AUDIO_SAMPLES_LENGTH];
        int errs = 0;
#if defined(_WIN32)
        if (m_useExternalVocoder) {
            ambeDecode(ambePartial, RAW_AMBE_LENGTH_BYTES, samples);
        }
        else {
#endif // defined(_WIN32)
            m_decoder->decode(ambePartial, samples);
#if defined(_WIN32)
        }
#endif // defined(_WIN32)

        if (m_debug)
            LogInfoEx(LOG_HOST, DMR_DT_VOICE ", Frame, VC%u.%u, srcId = %u, dstId = %u, errs = %u", dmrN, n, srcId, dstId, errs);

        // post-process: apply gain to decoded audio frames
        AnalogAudio::gain(samples, AUDIO_SAMPLES_LENGTH, m_rxAudioGain);

        if (m_localAudio) {
            m_outputAudio.addData(samples, AUDIO_SAMPLES_LENGTH);
            // Assert RTS PTT when audio is being sent to output
            assertRtsPtt();
        }

        if (m_udpAudio) {
            int pcmIdx = 0;
            uint8_t pcm[AUDIO_SAMPLES_LENGTH * 2U];
            // are we sending uLaw encoded audio?
            if (m_udpUseULaw) {
                for (uint32_t smpIdx = 0; smpIdx < AUDIO_SAMPLES_LENGTH; smpIdx++) {
                    pcm[smpIdx] = AnalogAudio::encodeMuLaw(samples[smpIdx]);
                }

                if (m_trace)
                    Utils::dump(1U, "HostBridge()::decodeDMRAudioFrame(), Encoded uLaw Audio", pcm, AUDIO_SAMPLES_LENGTH);

                writeUDPAudio(srcId, dstId, pcm, AUDIO_SAMPLES_LENGTH_BYTES / 2U);
            } else {
                // raw PCM audio
                for (uint32_t smpIdx = 0; smpIdx < AUDIO_SAMPLES_LENGTH; smpIdx++) {
                    pcm[pcmIdx + 0] = (uint8_t)(samples[smpIdx] & 0xFF);
                    pcm[pcmIdx + 1] = (uint8_t)((samples[smpIdx] >> 8) & 0xFF);
                    pcmIdx += 2;
                }

                writeUDPAudio(srcId, dstId, pcm, AUDIO_SAMPLES_LENGTH_BYTES);
            }
        }
    }
}

/* Helper to encode DMR network traffic audio frames. */

void HostBridge::encodeDMRAudioFrame(uint8_t* pcm, uint32_t forcedSrcId, uint32_t forcedDstId)
{
    assert(pcm != nullptr);
    using namespace dmr;
    using namespace dmr::defines;
    using namespace dmr::data;

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

    uint8_t* data = nullptr;
    m_dmrN = (uint8_t)(m_dmrSeqNo % 6);
    if (m_ambeCount == AMBE_PER_SLOT) {
        // is this the intitial sequence?
        if (m_dmrSeqNo == 0) {
            // send DMR voice header
            data = new uint8_t[DMR_FRAME_LENGTH_BYTES];

            // generate DMR LC
            lc::LC dmrLC = lc::LC();
            dmrLC.setFLCO(FLCO::GROUP);
            dmrLC.setSrcId(srcId);
            dmrLC.setDstId(dstId);
            m_dmrEmbeddedData.setLC(dmrLC);

            // generate the Slot TYpe
            SlotType slotType = SlotType();
            slotType.setDataType(DataType::VOICE_LC_HEADER);
            slotType.encode(data);

            lc::FullLC fullLC = lc::FullLC();
            fullLC.encode(dmrLC, data, DataType::VOICE_LC_HEADER);

            // generate DMR network frame
            NetData dmrData;
            dmrData.setSlotNo(m_slot);
            dmrData.setDataType(DataType::VOICE_LC_HEADER);
            dmrData.setSrcId(srcId);
            dmrData.setDstId(dstId);
            dmrData.setFLCO(FLCO::GROUP);

            uint8_t controlByte = 0U;
            if (m_grantDemand)
                controlByte = network::NET_CTRL_GRANT_DEMAND;                       // Grant Demand Flag
            controlByte |= network::NET_CTRL_SWITCH_OVER;
            dmrData.setControl(controlByte);

            dmrData.setN(m_dmrN);
            dmrData.setSeqNo(m_dmrSeqNo);
            dmrData.setBER(0U);
            dmrData.setRSSI(0U);

            dmrData.setData(data);

            LogInfoEx(LOG_HOST, DMR_DT_VOICE_LC_HEADER ", slot = %u, srcId = %u, dstId = %u, FLCO = $%02X", m_slot,
                dmrLC.getSrcId(), dmrLC.getDstId(), dmrData.getFLCO());

            m_network->writeDMR(dmrData, false);
            m_txStreamId = m_network->getDMRStreamId(m_slot);

            m_dmrSeqNo++;
            delete[] data;
        }

        // send DMR voice
        data = new uint8_t[DMR_FRAME_LENGTH_BYTES];

        ::memcpy(data, m_ambeBuffer, 13U);
        data[13U] = (uint8_t)(m_ambeBuffer[13U] & 0xF0);
        data[19U] = (uint8_t)(m_ambeBuffer[13U] & 0x0F);
        ::memcpy(data + 20U, m_ambeBuffer + 14U, 13U);

        DataType::E dataType = DataType::VOICE_SYNC;
        if (m_dmrN == 0)
            dataType = DataType::VOICE_SYNC;
        else {
            dataType = DataType::VOICE;

            uint8_t lcss = m_dmrEmbeddedData.getData(data, m_dmrN);

            // generated embedded signalling
            EMB emb = EMB();
            emb.setColorCode(0U);
            emb.setLCSS(lcss);
            emb.encode(data);
        }

        LogInfoEx(LOG_HOST, DMR_DT_VOICE ", srcId = %u, dstId = %u, slot = %u, seqNo = %u", srcId, dstId, m_slot, m_dmrN);

        // generate DMR network frame
        NetData dmrData;
        dmrData.setSlotNo(m_slot);
        dmrData.setDataType(dataType);
        dmrData.setSrcId(srcId);
        dmrData.setDstId(dstId);
        dmrData.setFLCO(FLCO::GROUP);
        dmrData.setN(m_dmrN);
        dmrData.setSeqNo(m_dmrSeqNo);
        dmrData.setBER(0U);
        dmrData.setRSSI(0U);

        dmrData.setData(data);

        m_network->writeDMR(dmrData, false);
        m_txStreamId = m_network->getDMRStreamId(m_slot);

        m_dmrSeqNo++;
        ::memset(m_ambeBuffer, 0x00U, 27U);
        m_ambeCount = 0U;
    }

    int smpIdx = 0;
    short samples[AUDIO_SAMPLES_LENGTH];
    for (uint32_t pcmIdx = 0; pcmIdx < (AUDIO_SAMPLES_LENGTH * 2U); pcmIdx += 2) {
        samples[smpIdx] = (short)((pcm[pcmIdx + 1] << 8) + pcm[pcmIdx + 0]);
        smpIdx++;
    }

    // pre-process: apply gain to PCM audio frames
    AnalogAudio::gain(samples, AUDIO_SAMPLES_LENGTH, m_txAudioGain);

    // encode PCM samples into AMBE codewords
    uint8_t ambe[RAW_AMBE_LENGTH_BYTES];
    ::memset(ambe, 0x00U, RAW_AMBE_LENGTH_BYTES);
#if defined(_WIN32)
    if (m_useExternalVocoder) {
        ambeEncode(samples, AUDIO_SAMPLES_LENGTH, ambe);
    }
    else {
#endif // defined(_WIN32)
        m_encoder->encode(samples, ambe);
#if defined(_WIN32)
    }
#endif // defined(_WIN32)

    // Utils::dump(1U, "HostBridge::encodeDMRAudioFrame(), Encoded AMBE", ambe, RAW_AMBE_LENGTH_BYTES);

    ::memcpy(m_ambeBuffer + (m_ambeCount * 9U), ambe, RAW_AMBE_LENGTH_BYTES);
    m_ambeCount++;
}
