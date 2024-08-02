// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Bridge
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file HostBridge.h
 * @ingroup bridge
 * @file HostBridge.cpp
 * @ingroup bridge
 */
#if !defined(__HOST_BRIDGE_H__)
#define __HOST_BRIDGE_H__

#include "Defines.h"
#include "common/dmr/data/EmbeddedData.h"
#include "common/dmr/lc/LC.h"
#include "common/dmr/lc/PrivacyLC.h"
#include "common/network/udp/Socket.h"
#include "common/yaml/Yaml.h"
#include "common/RingBuffer.h"
#include "common/Timer.h"
#include "vocoder/MBEDecoder.h"
#include "vocoder/MBEEncoder.h"
#define MINIAUDIO_IMPLEMENTATION
#include "audio/miniaudio.h"
#include "mdc/mdc_decode.h"
#include "network/PeerNetwork.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <pthread.h>
#endif // defined(_WIN32)

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define MBE_SAMPLES_LENGTH 160
#define NO_BIT_STEAL 0

#define ECMODE_NOISE_SUPPRESS 0x40
#define ECMODE_AGC 0x2000

#define DECSTATE_SIZE 2048
#define ENCSTATE_SIZE 6144

const uint8_t FULL_RATE_MODE = 0x00U;
const uint8_t HALF_RATE_MODE = 0x01U;

const uint8_t TX_MODE_DMR = 1U;
const uint8_t TX_MODE_P25 = 2U;


// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/**
 * @brief Helper callback, called when audio data is available.
 * @param device
 * @param output
 * @param input
 * @param frameCount
 */
void audioCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount);

/**
 * @brief Helper callback, called when MDC packets are detected.
 * @param frameCount 
 * @param op MDC Opcode.
 * @param arg MDC Argument.
 * @param unitID Unit ID.
 * @param extra0 1st extra byte.
 * @param extra1 2nd extra byte.
 * @param extra2 3rd extra byte.
 * @param extra3 4th extra byte.
 * @param context 
 */
void mdcPacketDetected(int frameCount, mdc_u8_t op, mdc_u8_t arg, mdc_u16_t unitID,
    mdc_u8_t extra0, mdc_u8_t extra1, mdc_u8_t extra2, mdc_u8_t extra3, void* context);

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the core service logic.
 * @ingroup bridge
 */
class HOST_SW_API HostBridge {
public:
    /**
     * @brief Initializes a new instance of the HostBridge class.
     * @param confFile Full-path to the configuration file.
     */
    HostBridge(const std::string& confFile);
    /**
     * @brief Finalizes a instance of the HostBridge class.
     */
    ~HostBridge();

    /**
     * @brief Executes the main host processing loop.
     * @returns int Zero if successful, otherwise error occurred.
     */
    int run();

private:
    friend void ::audioCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount);
    friend void ::mdcPacketDetected(int frameCount, mdc_u8_t op, mdc_u8_t arg, mdc_u16_t unitID,
        mdc_u8_t extra0, mdc_u8_t extra1, mdc_u8_t extra2, mdc_u8_t extra3, void* context);

    const std::string& m_confFile;
    yaml::Node m_conf;

    network::PeerNetwork* m_network;
    network::udp::Socket* m_udpAudioSocket;

    bool m_udpAudio;
    bool m_udpMetadata;
    uint16_t m_udpSendPort;
    std::string m_udpSendAddress;
    uint16_t m_udpReceivePort;
    std::string m_udpReceiveAddress;

    uint32_t m_srcId;
    uint32_t m_srcIdOverride;
    bool m_overrideSrcIdFromMDC;
    bool m_overrideSrcIdFromUDP;
    uint32_t m_dstId;
    uint8_t m_slot;

    std::string m_identity;
    float m_rxAudioGain;
    float m_vocoderDecoderAudioGain;
    bool m_vocoderDecoderAutoGain;
    float m_txAudioGain;
    float m_vocoderEncoderAudioGain;

    uint8_t m_txMode;

    float m_voxSampleLevel;
    uint16_t m_dropTimeMS;
    Timer m_dropTime;

    bool m_detectAnalogMDC1200;

    bool m_preambleLeaderTone;
    uint16_t m_preambleTone;
    uint16_t m_preambleLength;

    bool m_grantDemand;

    bool m_localAudio;

    ma_context m_maContext;
    ma_device_info* m_maPlaybackDevices;
    ma_device_info* m_maCaptureDevices;
    ma_device_config m_maDeviceConfig;
    ma_device m_maDevice;

    ma_waveform m_maSineWaveform;
    ma_waveform_config m_maSineWaveConfig;

    RingBuffer<short> m_inputAudio;
    RingBuffer<short> m_outputAudio;

    vocoder::MBEDecoder* m_decoder;
    vocoder::MBEEncoder* m_encoder;

    mdc_decoder_t* m_mdcDecoder;

    dmr::data::EmbeddedData m_dmrEmbeddedData;
    dmr::lc::LC m_rxDMRLC;
    dmr::lc::PrivacyLC m_rxDMRPILC;
    uint8_t* m_ambeBuffer;
    uint32_t m_ambeCount;
    uint32_t m_dmrSeqNo;
    uint8_t m_dmrN;

    p25::lc::LC m_rxP25LC;
    uint8_t* m_netLDU1;
    uint8_t* m_netLDU2;
    uint32_t m_p25SeqNo;
    uint8_t m_p25N;

    bool m_audioDetect;
    bool m_trafficFromUDP;
    uint32_t m_udpSrcId;
    uint32_t m_udpDstId;
    bool m_callInProgress;
    bool m_ignoreCall;
    uint8_t m_callAlgoId;
    uint64_t m_rxStartTime;
    uint32_t m_rxStreamId;
    uint32_t m_txStreamId;

    uint8_t m_detectedSampleCnt;
    bool m_dumpSampleLevel;

    bool m_running;
    bool m_debug;

    static std::mutex m_audioMutex;

#if defined(_WIN32)
    void* m_decoderState;
    uint16_t m_dcMode;
    void* m_encoderState;
    uint16_t m_ecMode;

    HINSTANCE m_ambeDLL;
    bool m_useExternalVocoder;

    int m_frameLengthInBits;
    int m_frameLengthInBytes;

    typedef void(__cdecl* Tambe_init_dec)(void* state, short mode);
    /**
     * @brief Initialize the AMBE decoder.
     * @param[out] state Buffer containing the decoder state to initialize.
     * @param mode AMBE mode; FULL (0) or HALF (1).
     */
    Tambe_init_dec ambe_init_dec;

    typedef short(__cdecl* Tambe_get_dec_mode)(void* state);
    /**
     * @brief Get the current operating state of the AMBE decoder.
     * @param[out] state Buffer containing the decoder state.
     * @returns short Operational mode.
     */
    Tambe_get_dec_mode ambe_get_dec_mode;

    typedef uint32_t(__cdecl* Tambe_voice_dec)(short* samples, short sampleLength, short* packedCodeword, short bitSteal, uint16_t cmode, short n, void* state);
    /**
     * @brief Decode AMBE codeword into PCM samples.
     * @param[out] samples Audio Output (in short samples)
     * @param[in] sampleLength Length of sample buffer.
     * @param[in] packedCodeword AMBE codewords.
     * @param[in] bitSteal
     * @param[in] cmode
     * @param[in] n
     * @param[in] state Buffer containing the decoder state.
     * @returns uint32_t
     */
    Tambe_voice_dec ambe_voice_dec;

    typedef void(__cdecl* Tambe_init_enc)(void* state, short mode, short initialize);
    /**
     * @brief Initialize the AMBE decoder.
     * @param[out] state Buffer containing the ebncoder state to initialize.
     * @param mode AMBE mode; FULL (0) or HALF (1).
     * @param initialize Flag to initialize encoder state fully, 1 to initialize, 0 to not.
     */
    Tambe_init_enc ambe_init_enc;

    typedef short(__cdecl* Tambe_get_enc_mode)(void* state);
    /**
     * @brief Get the current operating state of the AMBE encoder.
     * @param[out] state Buffer containing the decoder state.
     * @returns short Operational mode.
     */
    Tambe_get_enc_mode ambe_get_enc_mode;

    typedef uint32_t(__cdecl* Tambe_voice_enc)(short* packedCodeword, short bitSteal, short* samples, short sampleLength, uint16_t cmode, short n, short, void* state);
    /**
     * @brief Decode AMBE codeword into PCM samples.
     * @param[out] packedCodeword AMBE codewords.
     * @param[in] bitSteal
     * @param[in] samples Audio Output (in short samples)
     * @param[in] sampleLength Length of sample buffer.
     * @param[in] cmode
     * @param[in] n
     * @param[in]
     * @param[in] state Buffer containing the decoder state.
     * @returns uint32_t
     */
    Tambe_voice_enc ambe_voice_enc;

    /**
     * @brief Helper to initialize the use of the external AMBE.DLL binary for DVSI USB-3000.
     */
    void initializeAMBEDLL();

    /**
     * @brief Helper to unpack the codeword bytes into codeword bits for use with the AMBE decoder.
     * @param[out] codewordBits Codeword bits.
     * @param[in] codeword Codeword bytes.
     * @param lengthInBytes Length of codeword in bytes.
     * @param lengthBits Length of codeword in bits.
     */
    void unpackBytesToBits(short* codewordBits, const uint8_t* codeword, int lengthBytes, int lengthBits);
    /**
     * @brief Helper to unpack the codeword bytes into codeword bits for use with the AMBE decoder.
     * @param[out] codewordBits Codeword bits.
     * @param[in] codeword Codeword bytes.
     * @param lengthInBytes Length of codeword in bytes.
     * @param lengthBits Length of codeword in bits.
     */
    void unpackBytesToBits(uint8_t* codewordBits, const uint8_t* codeword, int lengthBytes, int lengthBits);
    /**
     * @brief Decodes the given MBE codewords to PCM samples using the decoder mode.
     * @param[in] codeword 
     * @param codewordLength 
     * @param[out] samples 
     * @returns int 
     */
    int ambeDecode(const uint8_t* codeword, uint32_t codewordLength, short* samples);

    /**
     * @brief Helper to pack the codeword bits into codeword bytes for use with the AMBE encoder.
     * @param[in] codewordBits Codeword bits.
     * @param[out] codeword Codeword bytes.
     * @param lengthInBytes Length of codeword in bytes.
     * @param lengthBits Length of codeword in bits.
     */
    void packBitsToBytes(const short* codewordBits, uint8_t* codeword, int lengthBytes, int lengthBits);
    /**
     * @brief Helper to pack the codeword bits into codeword bytes for use with the AMBE encoder.
     * @param[in] codewordBits Codeword bits.
     * @param[out] codeword Codeword bytes.
     * @param lengthInBytes Length of codeword in bytes.
     * @param lengthBits Length of codeword in bits.
     */
    void packBitsToBytes(const uint8_t* codewordBits, uint8_t* codeword, int lengthBytes, int lengthBits);
    /**
     * @brief Encodes the given PCM samples using the encoder mode to MBE codewords.
     * @param[in] samples 
     * @param sampleLength 
     * @param[out] codeword 
     * @returns int
     */
    void ambeEncode(const short* samples, uint32_t sampleLength, uint8_t* codeword);
#endif // defined(_WIN32)

    /**
     * @brief Reads basic configuration parameters from the INI.
     * @returns bool True, if configuration was read successfully, otherwise false.
     */
    bool readParams();
    /**
     * @brief Initializes network connectivity.
     * @returns bool True, if network connectivity was initialized, otherwise false.
     */
    bool createNetwork();

    /**
     * @brief Helper to process UDP audio.
     */
    void processUDPAudio();

    /**
     * @brief Helper to process DMR network traffic.
     * @param buffer 
     * @param length 
     */
    void processDMRNetwork(uint8_t* buffer, uint32_t length);
    /**
     * @brief Helper to decode DMR network traffic audio frames.
     * @param ambe 
     * @param srcId 
     * @param dstId 
     * @param dmrN 
     */
    void decodeDMRAudioFrame(uint8_t* ambe, uint32_t srcId, uint32_t dstId, uint8_t dmrN);
    /**
     * @brief Helper to encode DMR network traffic audio frames.
     * @param pcm 
     * @param forcedSrcId 
     * @param forcedDstId 
     */
    void encodeDMRAudioFrame(uint8_t* pcm, uint32_t forcedSrcId = 0U, uint32_t forcedDstId = 0U);

    /**
     * @brief Helper to process P25 network traffic.
     * @param buffer 
     * @param length 
     */
    void processP25Network(uint8_t* buffer, uint32_t length);
    /**
     * @brief Helper to decode P25 network traffic audio frames.
     * @param ldu 
     * @param srcId 
     * @param dstId 
     * @param p25N 
     */
    void decodeP25AudioFrame(uint8_t* ldu, uint32_t srcId, uint32_t dstId, uint8_t p25N);
    /**
     * @brief Helper to encode P25 network traffic audio frames.
     * @param pcm 
     * @param forcedSrcId 
     * @param forcedDstId 
     */
    void encodeP25AudioFrame(uint8_t* pcm, uint32_t forcedSrcId = 0U, uint32_t forcedDstId = 0U);

    /**
     * @brief Helper to generate the preamble tone.
     */
    void generatePreambleTone();

    /**
     * @brief Entry point to audio processing thread.
     * @param arg Instance of the thread_t structure.
     * @returns void* (Ignore)
     */
    static void* threadAudioProcess(void* arg);

    /**
     * @brief Entry point to network processing thread.
     * @param arg Instance of the thread_t structure.
     * @returns void* (Ignore)
     */
    static void* threadNetworkProcess(void* arg);

    /**
     * @brief Entry point to call lockup handler thread.
     * @param arg Instance of the thread_t structure.
     * @returns void* (Ignore)
     */
    static void* threadCallLockup(void* arg);
};

#endif // __HOST_BRIDGE_H__
