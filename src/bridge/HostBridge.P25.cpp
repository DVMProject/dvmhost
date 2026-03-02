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
#include "common/p25/P25Defines.h"
#include "common/p25/data/LowSpeedData.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/p25/dfsi/LC.h"
#include "common/p25/lc/LC.h"
#include "common/p25/P25Utils.h"
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
** Project 25
*/

/* Helper to process P25 network traffic. */

void HostBridge::processP25Network(uint8_t* buffer, uint32_t length)
{
    assert(buffer != nullptr);
    using namespace p25;
    using namespace p25::defines;
    using namespace p25::dfsi::defines;
    using namespace p25::data;

    if (m_txMode != TX_MODE_P25) {
        m_network->resetP25();
        return;
    }

    bool grantDemand = (buffer[14U] & network::NET_CTRL_GRANT_DEMAND) == network::NET_CTRL_GRANT_DEMAND;
    bool grantDenial = (buffer[14U] & network::NET_CTRL_GRANT_DENIAL) == network::NET_CTRL_GRANT_DENIAL;
    bool unitToUnit = (buffer[14U] & network::NET_CTRL_U2U) == network::NET_CTRL_U2U;

    // process network message header
    DUID::E duid = (DUID::E)buffer[22U];
    uint8_t MFId = buffer[15U];

    if (duid == DUID::HDU || duid == DUID::TSDU || duid == DUID::PDU)
        return;

    // process raw P25 data bytes
    UInt8Array data;
    uint8_t frameLength = buffer[23U];
    if (duid == DUID::PDU) {
        frameLength = length;
        data = std::unique_ptr<uint8_t[]>(new uint8_t[length]);
        ::memset(data.get(), 0x00U, length);
        ::memcpy(data.get(), buffer, length);
    }
    else {
        if (frameLength <= 24) {
            data = std::unique_ptr<uint8_t[]>(new uint8_t[frameLength]);
            ::memset(data.get(), 0x00U, frameLength);
        }
        else {
            data = std::unique_ptr<uint8_t[]>(new uint8_t[frameLength]);
            ::memset(data.get(), 0x00U, frameLength);
            ::memcpy(data.get(), buffer + 24U, frameLength);
        }
    }

    // handle LDU, TDU or TSDU frame
    uint8_t lco = buffer[4U];

    uint32_t srcId = GET_UINT24(buffer, 5U);
    uint32_t dstId = GET_UINT24(buffer, 8U);

    uint8_t lsd1 = buffer[20U];
    uint8_t lsd2 = buffer[21U];

    lc::LC control;
    LowSpeedData lsd;

    control.setLCO(lco);
    control.setSrcId(srcId);
    control.setDstId(dstId);
    control.setMFId(MFId);

    if (!control.isStandardMFId()) {
        control.setLCO(LCO::GROUP);
    }
    else {
        if (control.getLCO() == LCO::GROUP_UPDT || control.getLCO() == LCO::RFSS_STS_BCAST) {
            control.setLCO(LCO::GROUP);
        }
    }

    lsd.setLSD1(lsd1);
    lsd.setLSD2(lsd2);

    // ignore network traffic entirely when local audio detect or
    //  traffic from UDP is running
    if (m_audioDetect || m_trafficFromUDP)
        return;

    if (control.getLCO() == LCO::GROUP) {
        if ((duid == DUID::TDU) || (duid == DUID::TDULC)) {
            // ignore TDU's that are grant demands
            if (grantDemand) {
                m_network->resetP25();
                return;
            }
        }

        if (srcId == 0) {
            m_network->resetP25();
            return;
        }

        // ensure destination ID matches
        if (dstId != m_dstId) {
            m_network->resetP25();
            return;
        }

        m_networkWatchdog.start();

        // is this a new call stream?
        uint16_t callKID = 0U;
        if (m_network->getP25StreamId() != m_rxStreamId && ((duid != DUID::TDU) && (duid != DUID::TDULC)) && !m_callInProgress) {
            m_callInProgress = true;
            m_callAlgoId = ALGO_UNENCRYPT;

            // if this is the beginning of a call and we have a valid HDU frame, extract the algo ID
            uint8_t frameType = buffer[180U];
            if (frameType == FrameType::HDU_VALID) {
                m_callAlgoId = buffer[181U];
                if (m_callAlgoId != ALGO_UNENCRYPT) {
                    callKID = GET_UINT16(buffer, 182U);

                    if (m_callAlgoId != m_tekAlgoId && callKID != m_tekKeyId) {
                        m_callAlgoId = ALGO_UNENCRYPT;
                        m_callInProgress = false;
                        m_ignoreCall = true;

                        LogWarning(LOG_HOST, "P25, call ignored, using different encryption parameters, callAlgoId = $%02X, callKID = $%04X, tekAlgoId = $%02X, tekKID = $%04X", m_callAlgoId, callKID, m_tekAlgoId, m_tekKeyId);
                        m_network->resetP25();
                        return;
                    } else {
                        uint8_t mi[MI_LENGTH_BYTES];
                        ::memset(mi, 0x00U, MI_LENGTH_BYTES);
                        for (uint8_t i = 0; i < MI_LENGTH_BYTES; i++) {
                            mi[i] = buffer[184U + i];
                        }

                        m_p25Crypto->setMI(mi);
                        m_p25Crypto->generateKeystream();
                    }
                }
            }

            uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            m_rxStartTime = now;

            LogInfoEx(LOG_HOST, "P25, call start, srcId = %u, dstId = %u, callAlgoId = $%02X, callKID = $%04X", srcId, dstId, m_callAlgoId, callKID);
            if (m_preambleLeaderTone)
                generatePreambleTone();
        }

        // process call termination
        if ((duid == DUID::TDU) || (duid == DUID::TDULC)) {
            m_callInProgress = false;
            m_networkWatchdog.stop();
            m_ignoreCall = false;
            m_callAlgoId = ALGO_UNENCRYPT;

            if (m_rxStartTime > 0U) {
                uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                uint64_t diff = now - m_rxStartTime;

                // send USRP end of transmission
                if (m_udpUsrp) {
                    sendUsrpEot();
                }

                LogInfoEx(LOG_HOST, "P25, call end, srcId = %u, dstId = %u, dur = %us", srcId, dstId, diff / 1000U);
            }

            m_rxP25LC = lc::LC();
            m_rxStartTime = 0U;
            m_rxStreamId = 0U;

            if (!m_udpRTPContinuousSeq) {
                m_rtpInitialFrame = false;
                m_rtpSeqNo = 0U;
            }
            m_rtpTimestamp = INVALID_TS;
            m_network->resetP25();
            return;
        }

        if (m_ignoreCall && m_callAlgoId == ALGO_UNENCRYPT)
            m_ignoreCall = false;
        if (m_ignoreCall && m_callAlgoId == m_tekAlgoId)
            m_ignoreCall = false;

        if (duid == DUID::LDU2 && !m_ignoreCall) {
            m_callAlgoId = data[88U];
            callKID = GET_UINT16(buffer, 89U);
        }

        if (m_callAlgoId != ALGO_UNENCRYPT) {
            if (m_callAlgoId == m_tekAlgoId)
                m_ignoreCall = false;
            else
                m_ignoreCall = true;
        }

        if (m_ignoreCall) {
            m_network->resetP25();
            return;
        }

        // unsupported change of encryption parameters during call
        if (m_callAlgoId != ALGO_UNENCRYPT && m_callAlgoId != m_tekAlgoId && callKID != m_tekKeyId) {
            if (m_callInProgress) {
                m_callInProgress = false;

                if (m_callAlgoId != m_tekAlgoId && callKID != m_tekKeyId) {
                    LogWarning(LOG_HOST, "P25, unsupported change of encryption parameters during call, callAlgoId = $%02X, callKID = $%04X, tekAlgoId = $%02X, tekKID = $%04X", m_callAlgoId, callKID, m_tekAlgoId, m_tekKeyId);
                }

                uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                uint64_t diff = now - m_rxStartTime;

                LogInfoEx(LOG_HOST, "P25, call end (T), srcId = %u, dstId = %u, dur = %us", srcId, dstId, diff / 1000U);
            }

            m_ignoreCall = true;
            m_network->resetP25();
            return;
        }

        int count = 0;
        switch (duid)
        {
        case DUID::LDU1:
            if ((data[0U] == DFSIFrameType::LDU1_VOICE1) && (data[22U] == DFSIFrameType::LDU1_VOICE2) &&
                (data[36U] == DFSIFrameType::LDU1_VOICE3) && (data[53U] == DFSIFrameType::LDU1_VOICE4) &&
                (data[70U] == DFSIFrameType::LDU1_VOICE5) && (data[87U] == DFSIFrameType::LDU1_VOICE6) &&
                (data[104U] == DFSIFrameType::LDU1_VOICE7) && (data[121U] == DFSIFrameType::LDU1_VOICE8) &&
                (data[138U] == DFSIFrameType::LDU1_VOICE9)) {

                dfsi::LC dfsiLC = dfsi::LC(control, lsd);

                dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE1);
                dfsiLC.decodeLDU1(data.get() + count, m_netLDU1 + 10U);
                count += DFSI_LDU1_VOICE1_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE2);
                dfsiLC.decodeLDU1(data.get() + count, m_netLDU1 + 26U);
                count += DFSI_LDU1_VOICE2_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE3);
                dfsiLC.decodeLDU1(data.get() + count, m_netLDU1 + 55U);
                count += DFSI_LDU1_VOICE3_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE4);
                dfsiLC.decodeLDU1(data.get() + count, m_netLDU1 + 80U);
                count += DFSI_LDU1_VOICE4_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE5);
                dfsiLC.decodeLDU1(data.get() + count, m_netLDU1 + 105U);
                count += DFSI_LDU1_VOICE5_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE6);
                dfsiLC.decodeLDU1(data.get() + count, m_netLDU1 + 130U);
                count += DFSI_LDU1_VOICE6_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE7);
                dfsiLC.decodeLDU1(data.get() + count, m_netLDU1 + 155U);
                count += DFSI_LDU1_VOICE7_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE8);
                dfsiLC.decodeLDU1(data.get() + count, m_netLDU1 + 180U);
                count += DFSI_LDU1_VOICE8_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE9);
                dfsiLC.decodeLDU1(data.get() + count, m_netLDU1 + 204U);
                count += DFSI_LDU1_VOICE9_FRAME_LENGTH_BYTES;

                LogInfoEx(LOG_NET, P25_LDU1_STR " audio, srcId = %u, dstId = %u", srcId, dstId);

                // decode 9 IMBE codewords into PCM samples
                decodeP25AudioFrame(m_netLDU1, srcId, dstId, 1U);
            }
            break;
        case DUID::LDU2:
            if ((data[0U] == DFSIFrameType::LDU2_VOICE10) && (data[22U] == DFSIFrameType::LDU2_VOICE11) &&
                (data[36U] == DFSIFrameType::LDU2_VOICE12) && (data[53U] == DFSIFrameType::LDU2_VOICE13) &&
                (data[70U] == DFSIFrameType::LDU2_VOICE14) && (data[87U] == DFSIFrameType::LDU2_VOICE15) &&
                (data[104U] == DFSIFrameType::LDU2_VOICE16) && (data[121U] == DFSIFrameType::LDU2_VOICE17) &&
                (data[138U] == DFSIFrameType::LDU2_VOICE18)) {

                dfsi::LC dfsiLC = dfsi::LC(control, lsd);

                dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE10);
                dfsiLC.decodeLDU2(data.get() + count, m_netLDU2 + 10U);
                count += DFSI_LDU2_VOICE10_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE11);
                dfsiLC.decodeLDU2(data.get() + count, m_netLDU2 + 26U);
                count += DFSI_LDU2_VOICE11_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE12);
                dfsiLC.decodeLDU2(data.get() + count, m_netLDU2 + 55U);
                count += DFSI_LDU2_VOICE12_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE13);
                dfsiLC.decodeLDU2(data.get() + count, m_netLDU2 + 80U);
                count += DFSI_LDU2_VOICE13_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE14);
                dfsiLC.decodeLDU2(data.get() + count, m_netLDU2 + 105U);
                count += DFSI_LDU2_VOICE14_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE15);
                dfsiLC.decodeLDU2(data.get() + count, m_netLDU2 + 130U);
                count += DFSI_LDU2_VOICE15_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE16);
                dfsiLC.decodeLDU2(data.get() + count, m_netLDU2 + 155U);
                count += DFSI_LDU2_VOICE16_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE17);
                dfsiLC.decodeLDU2(data.get() + count, m_netLDU2 + 180U);
                count += DFSI_LDU2_VOICE17_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE18);
                dfsiLC.decodeLDU2(data.get() + count, m_netLDU2 + 204U);
                count += DFSI_LDU2_VOICE18_FRAME_LENGTH_BYTES;

                LogInfoEx(LOG_NET, P25_LDU2_STR " audio, algo = $%02X, kid = $%04X", dfsiLC.control()->getAlgId(), dfsiLC.control()->getKId());

                // decode 9 IMBE codewords into PCM samples
                decodeP25AudioFrame(m_netLDU2, srcId, dstId, 2U);

                // copy out the MI for the next super frame
                if (dfsiLC.control()->getAlgId() == m_tekAlgoId && dfsiLC.control()->getKId() == m_tekKeyId) {
                    uint8_t mi[MI_LENGTH_BYTES];
                    dfsiLC.control()->getMI(mi);
                    
                    m_p25Crypto->setMI(mi);
                    m_p25Crypto->generateKeystream();
                } else {
                    m_p25Crypto->clearMI();
                }
            }
            break;
        
        case DUID::HDU:
        case DUID::PDU:
        case DUID::TDU:
        case DUID::TDULC:
        case DUID::TSDU:
        case DUID::VSELP1:
        case DUID::VSELP2:
        default:
            // this makes GCC happy
            break;
        }

        m_rxStreamId = m_network->getP25StreamId();
    }
}

/* Helper to decode P25 network traffic audio frames. */

void HostBridge::decodeP25AudioFrame(uint8_t* ldu, uint32_t srcId, uint32_t dstId, uint8_t p25N)
{
    assert(ldu != nullptr);
    using namespace p25;
    using namespace p25::defines;

    if (m_debug) {
        uint8_t mi[MI_LENGTH_BYTES];
        ::memset(mi, 0x00U, MI_LENGTH_BYTES);
        m_p25Crypto->getMI(mi);

        LogInfoEx(LOG_NET, "Crypto, Enc Sync, MI = %02X %02X %02X %02X %02X %02X %02X %02X %02X", 
            mi[0U], mi[1U], mi[2U], mi[3U], mi[4U], mi[5U], mi[6U], mi[7U], mi[8U]);
    }

    // decode 9 IMBE codewords into PCM samples
    for (int n = 0; n < 9; n++) {
        uint8_t imbe[RAW_IMBE_LENGTH_BYTES];
        switch (n) {
        case 0:
            ::memcpy(imbe, ldu + 10U, RAW_IMBE_LENGTH_BYTES);
            break;
        case 1:
            ::memcpy(imbe, ldu + 26U, RAW_IMBE_LENGTH_BYTES);
            break;
        case 2:
            ::memcpy(imbe, ldu + 55U, RAW_IMBE_LENGTH_BYTES);
            break;
        case 3:
            ::memcpy(imbe, ldu + 80U, RAW_IMBE_LENGTH_BYTES);
            break;
        case 4:
            ::memcpy(imbe, ldu + 105U, RAW_IMBE_LENGTH_BYTES);
            break;
        case 5:
            ::memcpy(imbe, ldu + 130U, RAW_IMBE_LENGTH_BYTES);
            break;
        case 6:
            ::memcpy(imbe, ldu + 155U, RAW_IMBE_LENGTH_BYTES);
            break;
        case 7:
            ::memcpy(imbe, ldu + 180U, RAW_IMBE_LENGTH_BYTES);
            break;
        case 8:
            ::memcpy(imbe, ldu + 204U, RAW_IMBE_LENGTH_BYTES);
            break;
        }

        // Utils::dump(1U, "HostBridge::decodeP25AudioFrame(), IMBE", imbe, RAW_IMBE_LENGTH_BYTES);

        if (m_tekAlgoId != P25DEF::ALGO_UNENCRYPT && m_tekKeyId > 0U && m_p25Crypto->getTEKLength() > 0U) {
            switch (m_tekAlgoId) {
            case P25DEF::ALGO_AES_256:
                m_p25Crypto->cryptAES_IMBE(imbe, (p25N == 1U) ? DUID::LDU1 : DUID::LDU2);
                break;
            case P25DEF::ALGO_ARC4:
                m_p25Crypto->cryptARC4_IMBE(imbe, (p25N == 1U) ? DUID::LDU1 : DUID::LDU2);
                break;
            case P25DEF::ALGO_DES:
                m_p25Crypto->cryptDES_IMBE(imbe, (p25N == 1U) ? DUID::LDU1 : DUID::LDU2);
                break;
            default:
                LogError(LOG_HOST, "unsupported TEK algorithm, tekAlgoId = $%02X", m_tekAlgoId);
                break;
            }
        }

        // Utils::dump(1U, "HostBridge::decodeP25AudioFrame(), Decrypted IMBE", imbe, RAW_IMBE_LENGTH_BYTES);

        short samples[AUDIO_SAMPLES_LENGTH];
        int errs = 0;
#if defined(_WIN32)
        if (m_useExternalVocoder) {
            ambeDecode(imbe, RAW_IMBE_LENGTH_BYTES, samples);
        }
        else {
#endif // defined(_WIN32)
            m_decoder->decode(imbe, samples);
#if defined(_WIN32)
        }
#endif // defined(_WIN32)

        if (m_debug)
            LogDebug(LOG_HOST, "P25, LDU (Logical Link Data Unit), Frame, VC%u.%u, srcId = %u, dstId = %u, errs = %u", p25N, n, srcId, dstId, errs);

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
            if (m_udpUseULaw) {
                for (uint32_t smpIdx = 0; smpIdx < AUDIO_SAMPLES_LENGTH; smpIdx++) {
                    pcm[smpIdx] = AnalogAudio::encodeMuLaw(samples[smpIdx]);
                }

                if (m_trace)
                    Utils::dump(1U, "HostBridge()::decodeP25AudioFrame(), Encoded uLaw Audio", pcm, AUDIO_SAMPLES_LENGTH);

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
}

/* Helper to encode P25 network traffic audio frames. */

void HostBridge::encodeP25AudioFrame(uint8_t* pcm, uint32_t forcedSrcId, uint32_t forcedDstId)
{
    assert(pcm != nullptr);
    using namespace p25;
    using namespace p25::defines;
    using namespace p25::data;

    if (m_p25N > 17)
        m_p25N = 0;
    if (m_p25N == 0)
        ::memset(m_netLDU1, 0x00U, 9U * 25U);
    if (m_p25N == 9)
        ::memset(m_netLDU2, 0x00U, 9U * 25U);

    int smpIdx = 0;
    short samples[AUDIO_SAMPLES_LENGTH];
    for (uint32_t pcmIdx = 0; pcmIdx < (AUDIO_SAMPLES_LENGTH * 2U); pcmIdx += 2) {
        samples[smpIdx] = (short)((pcm[pcmIdx + 1] << 8) + pcm[pcmIdx + 0]);
        smpIdx++;
    }

    // pre-process: apply gain to PCM audio frames
    AnalogAudio::gain(samples, AUDIO_SAMPLES_LENGTH, m_txAudioGain);

    // encode PCM samples into IMBE codewords
    uint8_t imbe[RAW_IMBE_LENGTH_BYTES];
    ::memset(imbe, 0x00U, RAW_IMBE_LENGTH_BYTES);
#if defined(_WIN32)
    if (m_useExternalVocoder) {
        ambeEncode(samples, AUDIO_SAMPLES_LENGTH, imbe);
    }
    else {
#endif // defined(_WIN32)
        m_encoder->encode(samples, imbe);
#if defined(_WIN32)
    }
#endif // defined(_WIN32)

    // Utils::dump(1U, "HostBridge::encodeP25AudioFrame(), Encoded IMBE", imbe, RAW_IMBE_LENGTH_BYTES);

    if (m_tekAlgoId != P25DEF::ALGO_UNENCRYPT && m_tekKeyId > 0U && m_p25Crypto->getTEKLength() > 0U) {
        // generate initial MI for the HDU
        if (m_p25N == 0U && !m_p25Crypto->hasValidKeystream()) {
            if (!m_p25Crypto->hasValidMI()) {
                m_p25Crypto->generateMI();
                m_p25Crypto->generateKeystream();
            }
        }

        // perform crypto
        switch (m_tekAlgoId) {
        case P25DEF::ALGO_AES_256:
            m_p25Crypto->cryptAES_IMBE(imbe, (m_p25N < 9U) ? DUID::LDU1 : DUID::LDU2);
            break;
        case P25DEF::ALGO_ARC4:
            m_p25Crypto->cryptARC4_IMBE(imbe, (m_p25N < 9U) ? DUID::LDU1 : DUID::LDU2);
            break;
        case P25DEF::ALGO_DES:
            m_p25Crypto->cryptDES_IMBE(imbe, (m_p25N < 9U) ? DUID::LDU1 : DUID::LDU2);
            break;
        default:
            LogError(LOG_HOST, "unsupported TEK algorithm, tekAlgoId = $%02X", m_tekAlgoId);
            break;
        }

        // if we're on the last block of the LDU2 -- generate the next MI
        if (m_p25N == 17U) {
            m_p25Crypto->generateNextMI();

            // generate new keystream
            m_p25Crypto->generateKeystream();
        }
    }

    // fill the LDU buffers appropriately
    switch (m_p25N) {
    // LDU1
    case 0:
        ::memcpy(m_netLDU1 + 10U, imbe, RAW_IMBE_LENGTH_BYTES);
        break;
    case 1:
        ::memcpy(m_netLDU1 + 26U, imbe, RAW_IMBE_LENGTH_BYTES);
        break;
    case 2:
        ::memcpy(m_netLDU1 + 55U, imbe, RAW_IMBE_LENGTH_BYTES);
        break;
    case 3:
        ::memcpy(m_netLDU1 + 80U, imbe, RAW_IMBE_LENGTH_BYTES);
        break;
    case 4:
        ::memcpy(m_netLDU1 + 105U, imbe, RAW_IMBE_LENGTH_BYTES);
        break;
    case 5:
        ::memcpy(m_netLDU1 + 130U, imbe, RAW_IMBE_LENGTH_BYTES);
        break;
    case 6:
        ::memcpy(m_netLDU1 + 155U, imbe, RAW_IMBE_LENGTH_BYTES);
        break;
    case 7:
        ::memcpy(m_netLDU1 + 180U, imbe, RAW_IMBE_LENGTH_BYTES);
        break;
    case 8:
        ::memcpy(m_netLDU1 + 204U, imbe, RAW_IMBE_LENGTH_BYTES);
        break;

    // LDU2
    case 9:
        ::memcpy(m_netLDU2 + 10U, imbe, RAW_IMBE_LENGTH_BYTES);
        break;
    case 10:
        ::memcpy(m_netLDU2 + 26U, imbe, RAW_IMBE_LENGTH_BYTES);
        break;
    case 11:
        ::memcpy(m_netLDU2 + 55U, imbe, RAW_IMBE_LENGTH_BYTES);
        break;
    case 12:
        ::memcpy(m_netLDU2 + 80U, imbe, RAW_IMBE_LENGTH_BYTES);
        break;
    case 13:
        ::memcpy(m_netLDU2 + 105U, imbe, RAW_IMBE_LENGTH_BYTES);
        break;
    case 14:
        ::memcpy(m_netLDU2 + 130U, imbe, RAW_IMBE_LENGTH_BYTES);
        break;
    case 15:
        ::memcpy(m_netLDU2 + 155U, imbe, RAW_IMBE_LENGTH_BYTES);
        break;
    case 16:
        ::memcpy(m_netLDU2 + 180U, imbe, RAW_IMBE_LENGTH_BYTES);
        break;
    case 17:
        ::memcpy(m_netLDU2 + 204U, imbe, RAW_IMBE_LENGTH_BYTES);
        break;
    }

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

    lc::LC lc = lc::LC();
    lc.setLCO(LCO::GROUP);
    lc.setGroup(true);
    lc.setPriority(4U);
    lc.setDstId(dstId);
    lc.setSrcId(srcId);

    lc.setAlgId(m_tekAlgoId);
    lc.setKId(m_tekKeyId);

    uint8_t mi[MI_LENGTH_BYTES];
    m_p25Crypto->getMI(mi);
    lc.setMI(mi);

    LowSpeedData lsd = LowSpeedData();

    uint8_t controlByte = network::NET_CTRL_SWITCH_OVER;

    // send P25 LDU1
    if (m_p25N == 8U) {
        LogInfoEx(LOG_HOST, P25_LDU1_STR " audio, srcId = %u, dstId = %u", srcId, dstId);
        m_network->writeP25LDU1(lc, lsd, m_netLDU1, FrameType::HDU_VALID, controlByte);
        m_txStreamId = m_network->getP25StreamId();
    }

    // send P25 LDU2
    if (m_p25N == 17U) {
        LogInfoEx(LOG_HOST, P25_LDU2_STR " audio, algo = $%02X, kid = $%04X", m_tekAlgoId, m_tekKeyId);
        m_network->writeP25LDU2(lc, lsd, m_netLDU2, controlByte);
    }

    m_p25SeqNo++;
    m_p25N++;
    
    // if N is >17 reset sequence
    if (m_p25N > 17)
        m_p25N = 0;
}
