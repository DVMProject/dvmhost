// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Bridge
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/analog/AnalogDefines.h"
#include "common/analog/AnalogAudio.h"
#include "common/dmr/DMRDefines.h"
#include "common/edac/AMBEFEC.h"
#include "common/nxdn/NXDNDefines.h"
#include "common/nxdn/NXDNUtils.h"
#include "common/nxdn/Audio.h"
#include "common/nxdn/Sync.h"
#include "common/nxdn/channel/FACCH1.h"
#include "common/nxdn/channel/LICH.h"
#include "common/nxdn/channel/SACCH.h"
#include "common/nxdn/lc/RTCH.h"
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
** NXDN
*/

/* Helper to process NXDN network traffic. */

void HostBridge::processNXDNNetwork(uint8_t* buffer, uint32_t length)
{
    assert(buffer != nullptr);
    using namespace nxdn;
    using namespace nxdn::defines;

    if (m_txMode != TX_MODE_NXDN) {
        m_network->resetNXDN();
        return;
    }

    uint8_t messageType = buffer[4U];
    uint32_t srcId = GET_UINT24(buffer, 5U);
    uint32_t dstId = GET_UINT24(buffer, 8U);

    if (m_debug) {
        LogDebug(LOG_NET, "NXDN, messageType = $%02X, srcId = %u, dstId = %u, len = %u", messageType, srcId, dstId, length);
    }

    if (m_audioDetect || m_trafficFromUDP)
        return;

    if (srcId == 0U) {
        m_network->resetNXDN();
        return;
    }

    if (dstId != m_dstId) {
        m_network->resetNXDN();
        return;
    }

    uint8_t frame[NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(frame, 0x00U, NXDN_FRAME_LENGTH_BYTES + 2U);
    ::memcpy(frame + 2U, buffer + 24U, NXDN_FRAME_LENGTH_BYTES);

    NXDNUtils::scrambler(frame + 2U);

    channel::LICH lich;
    if (!lich.decode(frame + 2U)) {
        m_network->resetNXDN();
        return;
    }

    FuncChannelType::E fct = lich.getFCT();

    // process non-superframe control signalling (VCALL/TX_REL) from FACCH
    if (fct == FuncChannelType::USC_SACCH_NS) {
        channel::FACCH1 facch;
        bool valid = facch.decode(frame + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_FEC_LENGTH_BITS);
        if (!valid) {
            valid = facch.decode(frame + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_FEC_LENGTH_BITS + NXDN_FACCH1_FEC_LENGTH_BITS);
        }

        if (valid) {
            uint8_t lcBuffer[10U];
            ::memset(lcBuffer, 0x00U, 10U);
            facch.getData(lcBuffer);

            lc::RTCH lc;
            lc.decode(lcBuffer, NXDN_FACCH1_LENGTH_BITS);

            if (lc.getMessageType() == MessageType::RTCH_VCALL) {
                m_rxNXDNLC = lc;
                m_callAlgoId = lc.getAlgId();

                if (m_callAlgoId != CIPHER_TYPE_NONE) {
                    if (m_callAlgoId != m_nxdnCipherType || lc.getKId() != m_tekKeyId ||
                        m_nxdnCrypto->getTEKKeyLength() == 0U) {
                        LogWarning(LOG_HOST, "NXDN, call ignored, unsupported encryption parameters, callAlgoId = $%02X, callKID = $%02X, tekAlgoId = $%02X, tekKID = $%02X",
                            m_callAlgoId, lc.getKId(), m_nxdnCipherType, m_tekKeyId);
                        m_ignoreCall = true;
                        m_network->resetNXDN();
                        return;
                    }

                    if (m_callAlgoId == CIPHER_TYPE_EHR) {
                        m_nxdnCrypto->generateKeystream();
                    }
                    else {
                        // DES/AES headers carry VCALL in FACCH1-1 and the current
                        // session's VCALL_IV in FACCH1-2.
                        channel::FACCH1 ivFacch;
                        if (!ivFacch.decode(frame + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS +
                            NXDN_SACCH_FEC_LENGTH_BITS + NXDN_FACCH1_FEC_LENGTH_BITS)) {
                            LogWarning(LOG_HOST, "NXDN, encrypted call ignored, missing initial VCALL_IV");
                            m_ignoreCall = true;
                            m_network->resetNXDN();
                            return;
                        }

                        uint8_t ivData[NXDN_RTCH_LC_LENGTH_BYTES];
                        ::memset(ivData, 0x00U, sizeof(ivData));
                        ivFacch.getData(ivData);

                        lc::RTCH ivLC;
                        ivLC.decode(ivData, NXDN_FACCH1_LENGTH_BITS);
                        if (ivLC.getMessageType() != MessageType::RTCH_VCALL_IV) {
                            LogWarning(LOG_HOST, "NXDN, encrypted call ignored, invalid initial VCALL_IV");
                            m_ignoreCall = true;
                            m_network->resetNXDN();
                            return;
                        }

                        uint8_t mi[MI_LENGTH_BYTES];
                        ivLC.getMI(mi);

                        m_nxdnCrypto->setMI(mi);
                        m_nxdnCrypto->generateKeystream();
                        if (!m_nxdnCrypto->hasValidKeystream()) {
                            m_ignoreCall = true;
                            m_network->resetNXDN();
                            return;
                        }
                    }
                }

                m_networkWatchdog.start();

                if (m_network->getNXDNStreamId() != m_rxStreamId && !m_callInProgress) {
                    m_callInProgress = true;
                    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                    m_rxStartTime = now;

                    LogInfoEx(LOG_HOST, "NXDN, call start, srcId = %u, dstId = %u", srcId, dstId);
                    if (m_preambleLeaderTone)
                        generatePreambleTone();
                }
            }
            else if (lc.getMessageType() == MessageType::RTCH_TX_REL || lc.getMessageType() == MessageType::RTCH_TX_REL_EX) {
                m_callInProgress = false;
                m_networkWatchdog.stop();
                m_ignoreCall = false;
                m_callAlgoId = 0U;

                if (m_rxStartTime > 0U) {
                    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                    uint64_t diff = now - m_rxStartTime;

                    LogInfoEx(LOG_HOST, "NXDN, call end, srcId = %u, dstId = %u, dur = %us", srcId, dstId, diff / 1000U);
                }

                m_rxNXDNLC = lc::RTCH();
                m_rxStartTime = 0U;
                m_rxStreamId = 0U;
                m_nxdnSeqNo = 0U;
                m_nxdnN = 0U;
                ::memset(m_nxdnAMBE, 0x00U, 36U);

                m_nxdnCrypto->resetKeystream();
                m_rxNXDNSACCHLC = lc::RTCH();
                m_rxNXDNSACCHMask = 0U;
                m_rxNXDNPendingMI = false;

                if (!m_udpRTPContinuousSeq) {
                    m_rtpInitialFrame = false;
                    m_rtpSeqNo = 0U;
                }
                m_rtpTimestamp = INVALID_TS;
                m_network->resetNXDN();
                return;
            }
        }
    }

    if (m_ignoreCall)
        return;

    // only superframe SACCH traffic frames contain voice payloads
    if (fct != FuncChannelType::USC_SACCH_SS)
        return;

    // reassemble the 72-bit RTCH message carried as four 18-bit SACCH
    // fragments -- DES/AES alternates VCALL and VCALL_IV superframes
    channel::SACCH voiceSACCH;
    if (voiceSACCH.decode(frame + 2U)) {
        uint8_t fragment[3U];
        ::memset(fragment, 0x00U, sizeof(fragment));
        voiceSACCH.getData(fragment);

        uint8_t quarter = 3U - (uint8_t)voiceSACCH.getStructure();
        if (quarter == 0U) {
            m_rxNXDNSACCHLC = lc::RTCH();
            m_rxNXDNSACCHMask = 0U;
        }

        m_rxNXDNSACCHLC.decode(fragment, 18U, quarter * 18U);
        m_rxNXDNSACCHMask |= (uint8_t)(1U << quarter);

        if (quarter == 3U && m_rxNXDNSACCHMask == 0x0FU) {
            uint8_t sacchType = m_rxNXDNSACCHLC.getMessageType();
            if (sacchType == MessageType::RTCH_VCALL) {
                m_rxNXDNLC = m_rxNXDNSACCHLC;
                m_callAlgoId = m_rxNXDNLC.getAlgId();
                m_nxdnSeqNo = 3U;

                if (m_callAlgoId == CIPHER_TYPE_EHR &&
                    m_callAlgoId == m_nxdnCipherType &&
                    m_rxNXDNLC.getKId() == m_tekKeyId &&
                    m_nxdnCrypto->getTEKKeyLength() > 0U) {
                    m_nxdnCrypto->generateKeystream();
                }
            }
            else if (sacchType == MessageType::RTCH_VCALL_IV &&
                m_callAlgoId >= CIPHER_TYPE_DES) {
                m_rxNXDNSACCHLC.getMI(m_rxNXDNNextMI);
                m_rxNXDNPendingMI = true;
                m_nxdnSeqNo = 7U;
            }
            m_rxNXDNSACCHMask = 0U;
        }
    }

    // bridge may join an already active stream after its VCALL header, treat
    // the first valid voice frame as the start of the receive call as well
    if (!m_callInProgress) {
        if (m_rxNXDNLC.getMessageType() != MessageType::RTCH_VCALL)
            return;
        if (m_callAlgoId != CIPHER_TYPE_NONE &&
            (m_callAlgoId != m_nxdnCipherType || m_rxNXDNLC.getKId() != m_tekKeyId ||
             m_nxdnCrypto->getTEKKeyLength() == 0U)) {
            m_ignoreCall = true;
            return;
        }

        // if late entry first observed the preceding VCALL_IV superframe, that
        // announced MI is already current by the time this VCALL completes
        if (m_rxNXDNPendingMI && m_callAlgoId >= CIPHER_TYPE_DES) {
            m_nxdnCrypto->setMI(m_rxNXDNNextMI);
            m_nxdnCrypto->generateKeystream();
            m_rxNXDNPendingMI = false;
        }

        m_callInProgress = true;
        m_networkWatchdog.start();
        m_rxStartTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        LogInfoEx(LOG_HOST, "NXDN, late entry call start, srcId = %u, dstId = %u", srcId, dstId);
        if (m_preambleLeaderTone)
            generatePreambleTone();
    }
    else {
        m_networkWatchdog.start();
    }

    LogInfoEx(LOG_NET, "NXDN, " NXDN_RTCH_MSG_TYPE_VCALL ", audio, srcId = %u, dstId = %u", srcId, dstId);
    decodeNXDNAudioFrame(frame, srcId, dstId, m_nxdnSeqNo);

    // a VCALL_IV completed in this frame announces the MI for the following
    // eight-frame DES/AES session, so activate it only after current audio
    if (m_rxNXDNPendingMI) {
        m_nxdnCrypto->setMI(m_rxNXDNNextMI);
        m_nxdnCrypto->generateKeystream();
        m_rxNXDNPendingMI = false;
    }

    m_nxdnSeqNo++;
    m_rxStreamId = m_network->getNXDNStreamId();
}

/* Helper to decode NXDN network traffic audio frames. */

void HostBridge::decodeNXDNAudioFrame(uint8_t* frame, uint32_t srcId, uint32_t dstId, uint8_t nxdnN)
{
    assert(frame != nullptr);
    using namespace nxdn;
    using namespace nxdn::defines;

    channel::LICH lich;
    if (!lich.decode(frame + 2U))
        return;

    ChOption::E option = lich.getOption();

    nxdn::Audio nxdnAudio;
    ::edac::AMBEFEC ambeFec;

    // ahhh yes fantastical C++ lambda functions...
    auto decodePair = [&](uint32_t pairOffset, uint8_t vcBase) {
        uint8_t nxdnAMBE[18U];
        ::memset(nxdnAMBE, 0x00U, 18U);
        ::memcpy(nxdnAMBE, frame + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES + pairOffset, 18U);

        const uint32_t fecErrors[] = {
            ambeFec.regenerateNXDN(nxdnAMBE + 0U),
            ambeFec.regenerateNXDN(nxdnAMBE + 9U)
        };

        // match the normal NXDN voice path's lost-audio policy -- a pair is
        // half of a full four-codeword frame, so use half the frame threshold
        if ((fecErrors[0U] + fecErrors[1U]) > (DEFAULT_SILENCE_THRESHOLD / 2U)) {
            LogWarning(LOG_HOST, "NXDN, AMBE errors exceeded threshold, substituting silence, errors = %u",
                fecErrors[0U] + fecErrors[1U]);
            if (m_callAlgoId != CIPHER_TYPE_NONE)
                return;

            ::memcpy(nxdnAMBE + 0U, NULL_AMBE, 9U);
            ::memcpy(nxdnAMBE + 9U, NULL_AMBE, 9U);
        }

        uint8_t packedBits[13U];
        ::memset(packedBits, 0x00U, 13U);
        nxdnAudio.decode(nxdnAMBE, packedBits);

        for (uint8_t half = 0U; half < 2U; half++) {
            uint8_t rawBits[49U];
            ::memset(rawBits, 0x00U, sizeof(rawBits));

            for (uint32_t b = 0U; b < 49U; b++) {
                rawBits[b] = READ_BIT(packedBits, (half * 49U) + b) ? 1U : 0U;
            }

            if (m_callAlgoId != CIPHER_TYPE_NONE) {
                if (!m_nxdnCrypto->hasValidKeystream())
                    continue;

                if (m_debug) {
                    uint8_t mi[MI_LENGTH_BYTES];
                    ::memset(mi, 0x00U, MI_LENGTH_BYTES);
                    m_nxdnCrypto->getMI(mi);

                    LogInfoEx(LOG_NET, "Crypto, Enc Sync, MI = %02X %02X %02X %02X %02X %02X %02X %02X", 
                        mi[0U], mi[1U], mi[2U], mi[3U], mi[4U], mi[5U], mi[6U], mi[7U]);
                }

                uint8_t framesPerSession = m_callAlgoId == CIPHER_TYPE_EHR ? 4U : 8U;
                uint8_t word = (uint8_t)((nxdnN % framesPerSession) * 4U + vcBase + half);
                switch (m_callAlgoId) {
                case CIPHER_TYPE_EHR:
                    m_nxdnCrypto->cryptEHR_AMBE(rawBits, word);
                    break;
                case CIPHER_TYPE_DES:
                    m_nxdnCrypto->cryptDES_AMBE(rawBits, word);
                    break;
                case CIPHER_TYPE_AES:
                    m_nxdnCrypto->cryptAES_AMBE(rawBits, word);
                    break;
                default:
                    continue;
                }
            }

            short samples[AUDIO_SAMPLES_LENGTH];
            int errs = 0;
#if defined(_WIN32)
            if (m_useExternalVocoder) {
                // reframe the NXDN 49-bit payload before handing it to the external vocoder
                uint8_t ambePartial[dmr::defines::RAW_AMBE_LENGTH_BYTES];
                ::memset(ambePartial, 0x00U, sizeof(ambePartial));
                m_encoder->encodeBits(rawBits, ambePartial);

                errs = ambeDecode(ambePartial, dmr::defines::RAW_AMBE_LENGTH_BYTES, samples);
            }
            else {
#endif // defined(_WIN32)
                errs = m_decoder->decodeBits(rawBits, samples);
#if defined(_WIN32)
            }
#endif // defined(_WIN32)

            errs += (int)fecErrors[half];

            if (m_debug) {
                LogDebug(LOG_HOST, "NXDN, Frame, VC%u.%u, srcId = %u, dstId = %u, errs = %u", nxdnN, vcBase + half, srcId, dstId, errs);
            }

            AnalogAudio::gain(samples, AUDIO_SAMPLES_LENGTH, m_rxAudioGain);

            if (m_localAudio) {
                m_outputAudio.addData(samples, AUDIO_SAMPLES_LENGTH);
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
                        Utils::dump(1U, "HostBridge()::decodeNXDNAudioFrame(), Encoded uLaw Audio", pcm, AUDIO_SAMPLES_LENGTH);

                    writeUDPAudio(srcId, dstId, pcm, AUDIO_SAMPLES_LENGTH_BYTES / 2U);
                }
                else {
                    for (uint32_t smpIdx = 0; smpIdx < AUDIO_SAMPLES_LENGTH; smpIdx++) {
                        pcm[pcmIdx + 0] = (uint8_t)(samples[smpIdx] & 0xFF);
                        pcm[pcmIdx + 1] = (uint8_t)((samples[smpIdx] >> 8) & 0xFF);
                        pcmIdx += 2;
                    }

                    writeUDPAudio(srcId, dstId, pcm, AUDIO_SAMPLES_LENGTH_BYTES);
                }
            }
        }
    };

    switch (option) {
    case ChOption::STEAL_NONE:
        decodePair(0U, 0U);
        decodePair(18U, 2U);
        break;
    case ChOption::STEAL_FACCH1_1:
        decodePair(18U, 2U);
        break;
    case ChOption::STEAL_FACCH1_2:
        decodePair(0U, 0U);
        break;
    case ChOption::STEAL_FACCH:
    default:
        break;
    }
}

/* Helper to encode NXDN network traffic audio frames. */

void HostBridge::encodeNXDNAudioFrame(uint8_t* pcm, uint32_t forcedSrcId, uint32_t forcedDstId)
{
    assert(pcm != nullptr);
    using namespace nxdn;
    using namespace nxdn::defines;

    bool callStart = false;

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

    if (srcId == 0U)
        srcId = m_srcId;

    int smpIdx = 0;
    short samples[AUDIO_SAMPLES_LENGTH];
    for (uint32_t pcmIdx = 0; pcmIdx < (AUDIO_SAMPLES_LENGTH * 2U); pcmIdx += 2) {
        samples[smpIdx] = (short)((pcm[pcmIdx + 1] << 8) + pcm[pcmIdx + 0]);
        smpIdx++;
    }

    AnalogAudio::gain(samples, AUDIO_SAMPLES_LENGTH, m_txAudioGain);

    uint8_t rawBits[49U];
    ::memset(rawBits, 0x00U, sizeof(rawBits));
#if defined(_WIN32)
    if (m_useExternalVocoder) {
        // reframe the NXDN 49-bit payload before handing it to the external vocoder
        uint8_t ambePartial[dmr::defines::RAW_AMBE_LENGTH_BYTES];
        ::memset(ambePartial, 0x00U, sizeof(ambePartial));
        ambeEncode(samples, AUDIO_SAMPLES_LENGTH, ambePartial);

        char mbeBits[49U];
        ::memset(mbeBits, 0x00U, sizeof(mbeBits));
        m_decoder->decodeBits(ambePartial, mbeBits);

        for (uint32_t i = 0U; i < 49U; i++)
            rawBits[i] = mbeBits[i] != 0;
    }
    else {
#endif // defined(_WIN32)
        m_encoder->encodeBits(samples, rawBits);
#if defined(_WIN32)
    }
#endif // defined(_WIN32)

    if (m_nxdnCipherType != CIPHER_TYPE_NONE) {
        if (m_nxdnCrypto->getTEKKeyLength() == 0U) {
            LogWarning(LOG_HOST, "NXDN, encrypted audio dropped while TEK is unavailable");
            return;
        }

        if (m_nxdnSeqNo == 0U && m_nxdnN == 0U && !m_nxdnCrypto->hasValidKeystream()) {
            if (m_nxdnCipherType >= CIPHER_TYPE_DES)
                m_nxdnCrypto->generateMI();
            m_nxdnCrypto->generateKeystream();
        }

        // during the second superframe the existing keystream remains active,
        // while the advanced MI is announced for the following session
        if (m_nxdnCipherType >= CIPHER_TYPE_DES &&
            (m_nxdnSeqNo % 8U) == 4U && m_nxdnN == 0U) {
            m_nxdnCrypto->generateNextMI();
        }

        uint8_t framesPerSession = m_nxdnCipherType == CIPHER_TYPE_EHR ? 4U : 8U;
        uint8_t word = (uint8_t)((m_nxdnSeqNo % framesPerSession) * 4U + m_nxdnN);
        switch (m_nxdnCipherType) {
        case CIPHER_TYPE_EHR:
            m_nxdnCrypto->cryptEHR_AMBE(rawBits, word);
            break;
        case CIPHER_TYPE_DES:
            m_nxdnCrypto->cryptDES_AMBE(rawBits, word);
            break;
        case CIPHER_TYPE_AES:
            m_nxdnCrypto->cryptAES_AMBE(rawBits, word);
            break;
        default:
            return;
        }
    }

    if (m_nxdnN >= 4U) {
        m_nxdnN = 0U;
    }

    uint8_t* packedAMBE = m_nxdnAMBE + (m_nxdnN * dmr::defines::DMR_AMBE_LENGTH_BYTES);
    ::memset(packedAMBE, 0x00U, dmr::defines::DMR_AMBE_LENGTH_BYTES);
    for (uint32_t b = 0U; b < 49U; b++)
        WRITE_BIT(packedAMBE, b, rawBits[b] != 0U);
    m_nxdnN++;

    if (m_nxdnN < 4U) {
        return;
    }

    nxdn::Audio nxdnAudio;

    uint8_t nxdnAudioPayload[36U];
    ::memset(nxdnAudioPayload, 0x00U, 36U);

    for (uint8_t pair = 0U; pair < 2U; pair++) {
        uint8_t packedBits[13U];
        ::memset(packedBits, 0x00U, 13U);

        for (uint8_t half = 0U; half < 2U; half++) {
            uint8_t* ambe = m_nxdnAMBE + ((pair * 2U + half) * dmr::defines::DMR_AMBE_LENGTH_BYTES);
            for (uint32_t b = 0U; b < 49U; b++) {
                WRITE_BIT(packedBits, (half * 49U) + b, READ_BIT(ambe, b));
            }
        }

        uint8_t encodedPair[18U];
        ::memset(encodedPair, 0x00U, 18U);
        nxdnAudio.encode(packedBits, encodedPair);
        ::memcpy(nxdnAudioPayload + (pair * 18U), encodedPair, 18U);
    }

    lc::RTCH lc = lc::RTCH();
    lc.setMessageType(MessageType::RTCH_VCALL);
    lc.setCallType(CallType::UNSPECIFIED);
    lc.setGroup(true);
    lc.setSrcId((uint16_t)srcId);
    lc.setDstId((uint16_t)dstId);
    lc.setTransmissionMode(TransmissionMode::MODE_4800);
    lc.setAlgId(m_nxdnCipherType);
    lc.setKId(m_nxdnCipherType != CIPHER_TYPE_NONE ? (uint8_t)m_tekKeyId : 0U);
    lc.setEncrypted(m_nxdnCipherType != CIPHER_TYPE_NONE);

    if (m_nxdnSeqNo == 0U) {
        uint8_t controlFrame[NXDN_FRAME_LENGTH_BYTES + 2U];
        ::memset(controlFrame, 0x00U, NXDN_FRAME_LENGTH_BYTES + 2U);

        Sync::addNXDNSync(controlFrame + 2U);

        channel::LICH lich;
        lich.setRFCT(RFChannelType::RDCH);
        lich.setFCT(FuncChannelType::USC_SACCH_NS);
        lich.setOption(ChOption::STEAL_FACCH);
        lich.setOutbound(true);
        lich.encode(controlFrame + 2U);

        channel::SACCH sacch;
        sacch.setData(SACCH_IDLE);
        sacch.setRAN(0U);
        sacch.setStructure(ChStructure::SR_SINGLE);
        sacch.encode(controlFrame + 2U);

        channel::FACCH1 facch;
        uint8_t lcData[NXDN_RTCH_LC_LENGTH_BYTES];
        ::memset(lcData, 0x00U, NXDN_RTCH_LC_LENGTH_BYTES);
        lc.encode(lcData, NXDN_RTCH_LC_LENGTH_BITS);
        facch.setData(lcData);
        facch.encode(controlFrame + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_FEC_LENGTH_BITS);

        if (m_nxdnCipherType >= CIPHER_TYPE_DES) {
            lc::RTCH ivLC;
            ivLC.setMessageType(MessageType::RTCH_VCALL_IV);

            uint8_t mi[MI_LENGTH_BYTES];
            m_nxdnCrypto->getMI(mi);
            ivLC.setMI(mi);
            ::memset(lcData, 0x00U, sizeof(lcData));
            ivLC.encode(lcData, NXDN_RTCH_LC_LENGTH_BITS);

            facch.setData(lcData);
        }

        facch.encode(controlFrame + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS +
            NXDN_SACCH_FEC_LENGTH_BITS + NXDN_FACCH1_FEC_LENGTH_BITS);

        NXDNUtils::scrambler(controlFrame + 2U);

        LogInfoEx(LOG_HOST, "NXDN, " NXDN_RTCH_MSG_TYPE_VCALL ", srcId = %u, dstId = %u", srcId, dstId);
        m_network->writeNXDN(lc, controlFrame + 2U, NXDN_FRAME_LENGTH_BYTES);

        if (m_txStreamId <= 1U) {
            callStart = true;
        }

        m_txStreamId = m_network->getNXDNStreamId();
    }

    uint8_t voiceFrame[NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(voiceFrame, 0x00U, NXDN_FRAME_LENGTH_BYTES + 2U);

    Sync::addNXDNSync(voiceFrame + 2U);

    channel::LICH lich;
    lich.setRFCT(RFChannelType::RDCH);
    lich.setFCT(FuncChannelType::USC_SACCH_SS);
    lich.setOption(ChOption::STEAL_NONE);
    lich.setOutbound(true);
    lich.encode(voiceFrame + 2U);

    channel::SACCH sacch;
    uint8_t lcData[NXDN_RTCH_LC_LENGTH_BYTES];
    ::memset(lcData, 0x00U, sizeof(lcData));
    lc::RTCH sacchLC = lc;

    if (m_nxdnCipherType >= CIPHER_TYPE_DES && (m_nxdnSeqNo % 8U) >= 4U) {
        sacchLC = lc::RTCH();
        sacchLC.setMessageType(MessageType::RTCH_VCALL_IV);

        uint8_t mi[MI_LENGTH_BYTES];
        m_nxdnCrypto->getMI(mi);
        sacchLC.setMI(mi);
    }

    sacchLC.encode(lcData, NXDN_RTCH_LC_LENGTH_BITS);

    const uint8_t superframeIndex = m_nxdnSeqNo % 4U;
    const ChStructure::E structures[] = {
        ChStructure::SR_1_4, ChStructure::SR_2_4,
        ChStructure::SR_3_4, ChStructure::SR_4_4
    };

    uint8_t sacchData[3U];
    ::memset(sacchData, 0x00U, sizeof(sacchData));
    for (uint32_t bit = 0U; bit < 18U; bit++) {
        WRITE_BIT(sacchData, bit, READ_BIT(lcData, superframeIndex * 18U + bit));
    }

    sacch.setData(sacchData);
    sacch.setRAN(0U);
    sacch.setStructure(structures[superframeIndex]);
    sacch.encode(voiceFrame + 2U);

    ::memcpy(voiceFrame + 2U + NXDN_FSW_LICH_SACCH_LENGTH_BYTES, nxdnAudioPayload, 36U);

    NXDNUtils::scrambler(voiceFrame + 2U);

    LogInfoEx(LOG_HOST, "NXDN, " NXDN_RTCH_MSG_TYPE_VCALL ", audio, srcId = %u, dstId = %u", srcId, dstId);
    m_network->writeNXDN(lc, voiceFrame + 2U, NXDN_FRAME_LENGTH_BYTES);
    m_txStreamId = m_network->getNXDNStreamId();

    m_nxdnSeqNo++;
    if (m_nxdnCipherType >= CIPHER_TYPE_DES && (m_nxdnSeqNo % 8U) == 0U)
        m_nxdnCrypto->generateKeystream();
    m_nxdnN = 0U;
    ::memset(m_nxdnAMBE, 0x00U, 36U);

    // is this traffic being duplicated to a raw analog TG?
    if (m_duplicateToAnalog) {
        analog::data::NetData analogData;
        analogData.setSeqNo(m_analogN);
        analogData.setSrcId(srcId);
        analogData.setDstId(dstId);
        analogData.setControl(0U);
        analogData.setFrameType(AudioFrameType::VOICE);
        if (callStart) {
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
            Utils::dump(1U, "HostBridge()::encodeNXDNAudioFrame(), Encoded uLaw Audio", outPcm, AUDIO_SAMPLES_LENGTH);

        analogData.setAudio(outPcm);

        if (analogData.getFrameType() == AudioFrameType::VOICE) {
            LogInfoEx(LOG_HOST, ANO_VOICE ", audio, srcId = %u, dstId = %u, seqNo = %u", srcId, dstId, analogData.getSeqNo());
        }

        m_network->writeAnalog(analogData);
        m_analogN++;
    }
}
