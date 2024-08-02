// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Bridge
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/dmr/DMRDefines.h"
#include "common/dmr/data/EMB.h"
#include "common/dmr/data/NetData.h"
#include "common/dmr/lc/FullLC.h"
#include "common/dmr/SlotType.h"
#include "common/p25/P25Defines.h"
#include "common/p25/data/LowSpeedData.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/p25/dfsi/LC.h"
#include "common/p25/lc/LC.h"
#include "common/p25/P25Utils.h"
#include "common/network/udp/Socket.h"
#include "common/Log.h"
#include "common/StopWatch.h"
#include "common/Thread.h"
#include "common/Utils.h"
#include "bridge/ActivityLog.h"
#include "HostBridge.h"
#include "BridgeMain.h"
#include "SampleTimeConversion.h"

using namespace network;
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
//  Constants
// ---------------------------------------------------------------------------

#define IDLE_WARMUP_MS 5U

const int SAMPLE_RATE = 8000;
const int BITS_PER_SECOND = 16;
const int NUMBER_OF_BUFFERS = 32;

#define LOCAL_CALL "Local Traffic"
#define UDP_CALL "UDP Traffic"

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::mutex HostBridge::m_audioMutex;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/* Helper callback, called when audio data is available. */

void audioCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount)
{
    HostBridge* bridge = (HostBridge*)device->pUserData;
    if (!bridge->m_running)
        return;

    ma_uint32 pcmBytes = frameCount * ma_get_bytes_per_frame(device->capture.format, device->capture.channels);

    // capture input audio
    if (frameCount > 0U) {
        std::lock_guard<std::mutex> lock(HostBridge::m_audioMutex);

        int smpIdx = 0;
        short samples[MBE_SAMPLES_LENGTH];
        const uint8_t* pcm = (const uint8_t*)input;
        for (uint32_t pcmIdx = 0; pcmIdx < pcmBytes; pcmIdx += 2) {
            samples[smpIdx] = (short)((pcm[pcmIdx + 1] << 8) + pcm[pcmIdx + 0]);
            smpIdx++;
        }

        bridge->m_inputAudio.addData(samples, MBE_SAMPLES_LENGTH);
    }

    // playback output audio
    if (bridge->m_outputAudio.dataSize() >= MBE_SAMPLES_LENGTH) {
        short samples[MBE_SAMPLES_LENGTH];
        bridge->m_outputAudio.get(samples, MBE_SAMPLES_LENGTH);
        uint8_t* pcm = (uint8_t*)output;
        int pcmIdx = 0;
        for (uint32_t smpIdx = 0; smpIdx < MBE_SAMPLES_LENGTH; smpIdx++) {
            pcm[pcmIdx + 0] = (uint8_t)(samples[smpIdx] & 0xFF);
            pcm[pcmIdx + 1] = (uint8_t)((samples[smpIdx] >> 8) & 0xFF);
            pcmIdx += 2;
        }
    }
}

/* Helper callback, called when MDC packets are detected. */

void mdcPacketDetected(int frameCount, mdc_u8_t op, mdc_u8_t arg, mdc_u16_t unitID,
    mdc_u8_t extra0, mdc_u8_t extra1, mdc_u8_t extra2, mdc_u8_t extra3, void* context)
{
    HostBridge* bridge = (HostBridge*)context;
    if (!bridge->m_running)
        return;

    if (op == OP_PTT_ID && bridge->m_overrideSrcIdFromMDC) {
        ::LogMessage(LOG_HOST, "Local Traffic, MDC Detect, unitId = $%04X", unitID);

        // HACK: nasty bullshit to convert MDC unitID to decimal
        char* pCharRes = new (char);
        ::sprintf(pCharRes, "0x%X", unitID);

        uint32_t res = 0U;
        std::string s = std::string(pCharRes + 2U);
        if (s.find_first_not_of("0123456789") == std::string::npos) {
            res = (uint32_t)std::stoi(pCharRes + 2U);
        }
        else {
            res = (uint32_t)std::stoi(pCharRes, 0, 16);
        }

        bridge->m_srcIdOverride = res;
        ::LogMessage(LOG_HOST, "Local Traffic, MDC Detect, converted srcId = %u", bridge->m_srcIdOverride);
    }
}

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the HostBridge class. */

HostBridge::HostBridge(const std::string& confFile) :
    m_confFile(confFile),
    m_conf(),
    m_network(nullptr),
    m_udpAudioSocket(nullptr),
    m_udpAudio(false),
    m_udpMetadata(false),
    m_udpSendPort(34001),
    m_udpSendAddress("127.0.0.1"),
    m_udpReceivePort(32001),
    m_udpReceiveAddress("127.0.0.1"),
    m_srcId(p25::defines::WUID_FNE),
    m_srcIdOverride(0U),
    m_overrideSrcIdFromMDC(false),
    m_overrideSrcIdFromUDP(false),
    m_dstId(1U),
    m_slot(1U),
    m_identity(),
    m_rxAudioGain(1.0f),
    m_vocoderDecoderAudioGain(3.0f),
    m_vocoderDecoderAutoGain(false),
    m_txAudioGain(1.0f),
    m_vocoderEncoderAudioGain(3.0),
    m_txMode(1U),
    m_voxSampleLevel(30.0f),
    m_dropTimeMS(180U),
    m_dropTime(1000U, 0U, 180U),
    m_detectAnalogMDC1200(false),
    m_preambleLeaderTone(false),
    m_preambleTone(2175),
    m_preambleLength(200U),
    m_grantDemand(false),
    m_localAudio(false),
    m_maContext(),
    m_maPlaybackDevices(nullptr),
    m_maCaptureDevices(nullptr),
    m_maDeviceConfig(),
    m_maDevice(),
    m_inputAudio(MBE_SAMPLES_LENGTH * NUMBER_OF_BUFFERS, "Input Audio Buffer"),
    m_outputAudio(MBE_SAMPLES_LENGTH * NUMBER_OF_BUFFERS, "Output Audio Buffer"),
    m_decoder(nullptr),
    m_encoder(nullptr),
    m_mdcDecoder(nullptr),
    m_dmrEmbeddedData(),
    m_rxDMRLC(),
    m_rxDMRPILC(),
    m_ambeBuffer(nullptr),
    m_ambeCount(0U),
    m_dmrSeqNo(0U),
    m_dmrN(0U),
    m_rxP25LC(),
    m_netLDU1(nullptr),
    m_netLDU2(nullptr),
    m_p25SeqNo(0U),
    m_p25N(0U),
    m_audioDetect(false),
    m_trafficFromUDP(false),
    m_udpSrcId(0U),
    m_udpDstId(0U),
    m_callInProgress(false),
    m_ignoreCall(false),
    m_callAlgoId(p25::defines::ALGO_UNENCRYPT),
    m_rxStartTime(0U),
    m_rxStreamId(0U),
    m_txStreamId(0U),
    m_detectedSampleCnt(0U),
    m_dumpSampleLevel(false),
    m_running(false),
    m_debug(false)
#if defined(_WIN32)
    ,
    m_encoderState(nullptr),
    m_dcMode(0U),
    m_decoderState(nullptr),
    m_ecMode(0U),
    m_ambeDLL(nullptr),
    m_useExternalVocoder(false),
    m_frameLengthInBits(0),
    m_frameLengthInBytes(0)
#endif // defined(_WIN32)
{
#if defined(_WIN32)
    ambe_init_dec = nullptr;
    ambe_get_dec_mode = nullptr;
    ambe_voice_dec = nullptr;

    ambe_init_enc = nullptr;
    ambe_get_enc_mode = nullptr;
    ambe_voice_enc = nullptr;
#endif // defined(_WIN32)
    m_ambeBuffer = new uint8_t[27U];
    ::memset(m_ambeBuffer, 0x00U, 27U);

    m_netLDU1 = new uint8_t[9U * 25U];
    m_netLDU2 = new uint8_t[9U * 25U];

    ::memset(m_netLDU1, 0x00U, 9U * 25U);
    ::memset(m_netLDU2, 0x00U, 9U * 25U);
}

/* Finalizes a instance of the HostBridge class. */

HostBridge::~HostBridge()
{
    delete[] m_ambeBuffer;
    delete[] m_netLDU1;
    delete[] m_netLDU2;
}

/* Executes the main FNE processing loop. */

int HostBridge::run()
{
    bool ret = false;
    try {
        ret = yaml::Parse(m_conf, m_confFile.c_str());
        if (!ret) {
            ::fatal("cannot read the configuration file, %s\n", m_confFile.c_str());
        }
    }
    catch (yaml::OperationException const& e) {
        ::fatal("cannot read the configuration file - %s (%s)", m_confFile.c_str(), e.message());
    }

    bool m_daemon = m_conf["daemon"].as<bool>(false);
    if (m_daemon && g_foreground)
        m_daemon = false;

    // initialize system logging
    yaml::Node logConf = m_conf["log"];
    ret = ::LogInitialise(logConf["filePath"].as<std::string>(), logConf["fileRoot"].as<std::string>(),
        logConf["fileLevel"].as<uint32_t>(0U), logConf["displayLevel"].as<uint32_t>(0U));
    if (!ret) {
        ::fatal("unable to open the log file\n");
    }

    ret = ::ActivityLogInitialise(logConf["activityFilePath"].as<std::string>(), logConf["fileRoot"].as<std::string>());
    if (!ret) {
        ::fatal("unable to open the activity log file\n");
    }

#if !defined(_WIN32)
    // handle POSIX process forking
    if (m_daemon) {
        // create new process
        pid_t pid = ::fork();
        if (pid == -1) {
            ::fprintf(stderr, "%s: Couldn't fork() , exiting\n", g_progExe.c_str());
            ::LogFinalise();
            return EXIT_FAILURE;
        }
        else if (pid != 0) {
            ::LogFinalise();
            exit(EXIT_SUCCESS);
        }

        // create new session and process group
        if (::setsid() == -1) {
            ::fprintf(stderr, "%s: Couldn't setsid(), exiting\n", g_progExe.c_str());
            ::LogFinalise();
            return EXIT_FAILURE;
        }

        // set the working directory to the root directory
        if (::chdir("/") == -1) {
            ::fprintf(stderr, "%s: Couldn't cd /, exiting\n", g_progExe.c_str());
            ::LogFinalise();
            return EXIT_FAILURE;
        }

        ::close(STDIN_FILENO);
        ::close(STDOUT_FILENO);
        ::close(STDERR_FILENO);
    }
#endif // !defined(_WIN32)

    ::LogInfo(__BANNER__ "\r\n" __PROG_NAME__ " " __VER__ " (built " __BUILD__ ")\r\n" \
        "Copyright (c) 2017-2024 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\r\n" \
        "Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others\r\n" \
        ">> Audio Bridge\r\n");

    // read base parameters from configuration
    ret = readParams();
    if (!ret)
        return EXIT_FAILURE;

    if (!m_localAudio && !m_udpAudio) {
        ::LogError(LOG_HOST, "Must at least local audio or UDP audio!");
        return EXIT_FAILURE;
    }

    if (m_localAudio) {
        if (g_inputDevice == -1) {
            ::LogError(LOG_HOST, "Cannot have local audio and no specified input audio device.");
            return EXIT_FAILURE;
        }

        if (g_outputDevice == -1) {
            ::LogError(LOG_HOST, "Cannot have local audio and no specified output audio device.");
            return EXIT_FAILURE;
        }
    }

    yaml::Node systemConf = m_conf["system"];

    // initialize peer networking
    ret = createNetwork();
    if (!ret)
        return EXIT_FAILURE;

    ma_result result;
    if (m_localAudio) {
        // initialize audio devices
        if (ma_context_init(NULL, 0, NULL, &m_maContext) != MA_SUCCESS) {
            ::LogError(LOG_HOST, "Failed to initialize audio context.");
            return EXIT_FAILURE;
        }

        ma_uint32 playbackDeviceCount, captureDeviceCount;
        result = ma_context_get_devices(&m_maContext, &m_maPlaybackDevices, &playbackDeviceCount, &m_maCaptureDevices, &captureDeviceCount);
        if (result != MA_SUCCESS) {
            ::LogError(LOG_HOST, "Failed to retrieve audio device information.");
            return EXIT_FAILURE;
        }

        LogInfo("Audio Parameters");
        LogInfo("    Input Device: %s", m_maCaptureDevices[g_inputDevice].name);
        LogInfo("    Output Device: %s", m_maPlaybackDevices[g_outputDevice].name);

        // configure audio devices
        m_maDeviceConfig = ma_device_config_init(ma_device_type_duplex);
        m_maDeviceConfig.sampleRate = SAMPLE_RATE;

        m_maDeviceConfig.capture.pDeviceID = &m_maCaptureDevices[g_inputDevice].id;
        m_maDeviceConfig.capture.format = ma_format_s16;
        m_maDeviceConfig.capture.channels = 1;
        m_maDeviceConfig.capture.shareMode = ma_share_mode_shared;

        m_maDeviceConfig.playback.pDeviceID = &m_maPlaybackDevices[g_outputDevice].id;
        m_maDeviceConfig.playback.format = ma_format_s16;
        m_maDeviceConfig.playback.channels = 1;

        m_maDeviceConfig.periodSizeInFrames = MBE_SAMPLES_LENGTH;
        m_maDeviceConfig.dataCallback = audioCallback;
        m_maDeviceConfig.pUserData = this;

        result = ma_device_init(NULL, &m_maDeviceConfig, &m_maDevice);
        if (result != MA_SUCCESS) {
            ma_context_uninit(&m_maContext);
            return EXIT_FAILURE;
        }

        // configure tone generator for preamble
        m_maSineWaveConfig = ma_waveform_config_init(m_maDevice.playback.format, m_maDevice.playback.channels, m_maDevice.sampleRate, ma_waveform_type_sine, 0.2, m_preambleTone);
        result = ma_waveform_init(&m_maSineWaveConfig, &m_maSineWaveform);
        if (result != MA_SUCCESS) {
            ma_context_uninit(&m_maContext);
            return EXIT_FAILURE;
        }
    }

    m_mdcDecoder = mdc_decoder_new(SAMPLE_RATE);
    mdc_decoder_set_callback(m_mdcDecoder, mdcPacketDetected, this);

    // initialize vocoders
    if (m_txMode == TX_MODE_DMR) {
        // initialize DMR vocoders
        m_decoder = new vocoder::MBEDecoder(vocoder::DECODE_DMR_AMBE);
        m_encoder = new vocoder::MBEEncoder(vocoder::ENCODE_DMR_AMBE);
    }
    else if (m_txMode == TX_MODE_P25) {
        // initialize P25 vocoders
        m_decoder = new vocoder::MBEDecoder(vocoder::DECODE_88BIT_IMBE);
        m_encoder = new vocoder::MBEEncoder(vocoder::ENCODE_88BIT_IMBE);
    }

    m_decoder->setGainAdjust(m_vocoderDecoderAudioGain);
    m_decoder->setAutoGain(m_vocoderDecoderAutoGain);
    m_encoder->setGainAdjust(m_vocoderEncoderAudioGain);

#if defined(_WIN32)
    initializeAMBEDLL();
    if (m_useExternalVocoder) {
        m_decoderState = ::malloc(DECSTATE_SIZE);
        ::memset(m_decoderState, 0x00U, DECSTATE_SIZE);
        m_encoderState = ::malloc(ENCSTATE_SIZE);
        ::memset(m_encoderState, 0x00U, ENCSTATE_SIZE);

        m_dcMode = 0U;
        m_ecMode = ECMODE_NOISE_SUPPRESS | ECMODE_AGC;

        if (m_txMode == TX_MODE_P25) {
            m_frameLengthInBits = 88;
            m_frameLengthInBytes = 11;

            ambe_init_dec(m_decoderState, FULL_RATE_MODE);
            ambe_init_enc(m_encoderState, FULL_RATE_MODE, 1);
        }
        else {
            m_frameLengthInBits = 49;
            m_frameLengthInBytes = 7;

            ambe_init_dec(m_decoderState, HALF_RATE_MODE);
            ambe_init_enc(m_encoderState, HALF_RATE_MODE, 1);
        }
    }
#endif // defined(_WIN32)

    /*
    ** Initialize Threads
    */

    if (!Thread::runAsThread(this, threadNetworkProcess))
        return EXIT_FAILURE;
    if (!Thread::runAsThread(this, threadCallLockup))
        return EXIT_FAILURE;

    if (m_localAudio) {
        if (!Thread::runAsThread(this, threadAudioProcess))
            return EXIT_FAILURE;

        // start audio device
        result = ma_device_start(&m_maDevice);
        if (result != MA_SUCCESS) {
            ma_device_uninit(&m_maDevice);
            ma_context_uninit(&m_maContext);
            return EXIT_FAILURE;
        }
    }

    ::LogInfoEx(LOG_HOST, "Bridge is up and running");

    m_running = true;

    StopWatch stopWatch;
    stopWatch.start();

    // main execution loop
    while (!g_killed) {
        uint32_t ms = stopWatch.elapsed();

        ms = stopWatch.elapsed();
        stopWatch.start();

        // ------------------------------------------------------
        //  -- Network Clocking                               --
        // ------------------------------------------------------

        if (m_network != nullptr)
            m_network->clock(ms);

        if (m_udpAudio && m_udpAudioSocket != nullptr)
            processUDPAudio();

        if (ms < 2U)
            Thread::sleep(1U);
    }

    ::LogSetNetwork(nullptr);
    if (m_network != nullptr) {
        m_network->close();
        delete m_network;
    }

    if (m_udpAudioSocket != nullptr)
    {
        m_udpAudioSocket->close();
        delete m_udpAudioSocket;
    }

    if (m_decoder != nullptr)
        delete m_decoder;
    if (m_encoder != nullptr)
        delete m_encoder;

    delete m_mdcDecoder;

#if defined(_WIN32)
    if (m_encoderState != nullptr)
        delete m_encoderState;
    if (m_decoderState != nullptr)
        delete m_decoderState;
    if (m_ambeDLL != nullptr)
        ::FreeLibrary(m_ambeDLL);
#endif // defined(_WIN32)

    ma_waveform_uninit(&m_maSineWaveform);
    ma_device_uninit(&m_maDevice);
    ma_context_uninit(&m_maContext);

    return EXIT_SUCCESS;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

#if defined(_WIN32)
/* Helper to initialize the use of the external AMBE.DLL binary for DVSI USB-3000. */

void HostBridge::initializeAMBEDLL()
{
    m_useExternalVocoder = false;

    // get a handle to the DLL module.
    m_ambeDLL = LoadLibrary(TEXT("AMBE.dll"));
    if (m_ambeDLL != nullptr)
    {
        ambe_init_dec = (Tambe_init_dec)GetProcAddress(m_ambeDLL, "ambe_init_dec");
        ambe_get_dec_mode = (Tambe_get_dec_mode)GetProcAddress(m_ambeDLL, "ambe_get_dec_mode");
        ambe_voice_dec = (Tambe_voice_dec)GetProcAddress(m_ambeDLL, "ambe_voice_dec");

        ambe_init_enc = (Tambe_init_enc)GetProcAddress(m_ambeDLL, "ambe_init_enc");
        ambe_get_enc_mode = (Tambe_get_enc_mode)GetProcAddress(m_ambeDLL, "ambe_get_enc_mode");
        ambe_voice_enc = (Tambe_voice_enc)GetProcAddress(m_ambeDLL, "ambe_voice_enc");
        
        ::LogInfoEx(LOG_HOST, "Using external USB vocoder.");
        m_useExternalVocoder = true;
    }
}

/* Helper to unpack the codeword bytes into codeword bits for use with the AMBE decoder. */

void HostBridge::unpackBytesToBits(short* codewordBits, const uint8_t* codeword, int lengthBytes, int lengthBits)
{
    assert(codewordBits != nullptr);
    assert(codeword != nullptr);

    //codewordBits = new short[m_frameLengthInBits * 2];

    int processed = 0, bitPtr = 0, bytePtr = 0;
    for (int i = 0; i < lengthBytes; i++) {
        for (int j = 7; -1 < j; j--) {
            if (processed < lengthBits) {
                codewordBits[bitPtr] = (short)((codeword[bytePtr] >> (j & 0x1F)) & 1);
                bitPtr++;
            }

            processed++;
        }

        bytePtr++;
    }
}

/* Helper to unpack the codeword bytes into codeword bits for use with the AMBE decoder. */

void HostBridge::unpackBytesToBits(uint8_t* codewordBits, const uint8_t* codeword, int lengthBytes, int lengthBits)
{
    assert(codewordBits != nullptr);
    assert(codeword != nullptr);

    //codewordBits = new byte[m_frameLengthInBits * 2];

    int processed = 0, bitPtr = 0, bytePtr = 0;
    for (int i = 0; i < lengthBytes; i++) {
        for (int j = 7; -1 < j; j--) {
            if (processed < lengthBits) {
                codewordBits[bitPtr] = ((codeword[bytePtr] >> (j & 0x1FU)) & 1);
                bitPtr++;
            }

            processed++;
        }

        bytePtr++;
    }
}

/* Decodes the given MBE codewords to PCM samples using the decoder mode. */

int HostBridge::ambeDecode(const uint8_t* codeword, uint32_t codewordLength, short* samples)
{
    assert(codeword != nullptr);
    assert(samples != nullptr);

    //samples = new short[MBE_SAMPLES_LENGTH];

    UInt8Array cw = std::make_unique<uint8_t[]>(codewordLength);
    ::memcpy(cw.get(), codeword, codewordLength);

    // is this a DMR codeword?
    if (codewordLength > m_frameLengthInBytes && m_txMode == TX_MODE_DMR &&
        codewordLength == 9)
    {
        // use the vocoder to retrieve the un-ECC'ed and uninterleaved AMBE bits
        UInt8Array bits = std::make_unique<uint8_t[]>(49U);
        m_decoder->decodeBits(cw.get(), (char*)bits.get());

        // repack bits into 7-byte array
        packBitsToBytes(bits.get(), cw.get(), m_frameLengthInBytes, m_frameLengthInBits);
        codewordLength = m_frameLengthInBytes;
    }

    if (codewordLength > m_frameLengthInBytes) {
        ::LogError(LOG_HOST, "Codeword length is > %u", m_frameLengthInBytes);
        return -1;
    }

    if (codewordLength < m_frameLengthInBytes) {
        ::LogError(LOG_HOST, "Codeword length is < %u", m_frameLengthInBytes);
        return -1;
    }

    // unpack codeword from bytes to bits for use with external library
    std::unique_ptr<short[]> codewordBits = std::make_unique<short[]>(m_frameLengthInBits * 2);
    unpackBytesToBits(codewordBits.get(), cw.get(), m_frameLengthInBytes, m_frameLengthInBits);

    std::unique_ptr<short[]> n0 = std::make_unique<short[]>(MBE_SAMPLES_LENGTH / 2);
    ambe_voice_dec(n0.get(), MBE_SAMPLES_LENGTH / 2, codewordBits.get(), NO_BIT_STEAL, m_dcMode, 0, m_decoderState);

    std::unique_ptr<short[]> n1 = std::make_unique<short[]>(MBE_SAMPLES_LENGTH / 2);
    ambe_voice_dec(n1.get(), MBE_SAMPLES_LENGTH / 2, codewordBits.get(), NO_BIT_STEAL, m_dcMode, 1, m_decoderState);

    // combine sample segments into contiguous samples
    for (int i = 0; i < MBE_SAMPLES_LENGTH / 2; i++)
        samples[i] = n0[i];
    for (int i = 0; i < MBE_SAMPLES_LENGTH / 2; i++)
        samples[i + (MBE_SAMPLES_LENGTH / 2)] = n1[i];

    return 0; // this always just returns no errors?
}

/* Helper to pack the codeword bits into codeword bytes for use with the AMBE encoder. */

void HostBridge::packBitsToBytes(const short* codewordBits, uint8_t* codeword, int lengthBytes, int lengthBits)
{
    assert(codewordBits != nullptr);
    assert(codeword != nullptr);

    //codeword = new byte[lengthBytes];

    int processed = 0, bitPtr = 0, bytePtr = 0;
    for (int i = 0; i < lengthBytes; i++) {
        codeword[i] = 0;
        for (int j = 7; -1 < j; j--) {
            if (processed < lengthBits) {
                codeword[bytePtr] = (codeword[bytePtr] | ((codewordBits[bitPtr] & 1) << (j & 0x1FU)));
                bitPtr++;
            }

            processed++;
        }

        bytePtr++;
    }
}

/* Helper to pack the codeword bits into codeword bytes for use with the AMBE encoder. */

void HostBridge::packBitsToBytes(const uint8_t* codewordBits, uint8_t* codeword, int lengthBytes, int lengthBits)
{
    assert(codewordBits != nullptr);
    assert(codeword != nullptr);

    //codeword = new byte[lengthBytes];

    int processed = 0, bitPtr = 0, bytePtr = 0;
    for (int i = 0; i < lengthBytes; i++) {
        codeword[i] = 0;
        for (int j = 7; -1 < j; j--) {
            if (processed < lengthBits) {
                codeword[bytePtr] = (codeword[bytePtr] | ((codewordBits[bitPtr] & 1) << (j & 0x1FU)));
                bitPtr++;
            }

            processed++;
        }

        bytePtr++;
    }
}

/* Encodes the given PCM samples using the encoder mode to MBE codewords. */

void HostBridge::ambeEncode(const short* samples, uint32_t sampleLength, uint8_t* codeword)
{
    assert(codeword != nullptr);
    assert(samples != nullptr);

    //codeword = new byte[this.frameLengthInBytes];

    if (sampleLength > MBE_SAMPLES_LENGTH) {
        ::LogError(LOG_HOST, "Samples length is > %u", MBE_SAMPLES_LENGTH);
        return;
    }
    if (sampleLength < MBE_SAMPLES_LENGTH) {
        ::LogError(LOG_HOST, "Samples length is < %u", MBE_SAMPLES_LENGTH);
        return;
    }

    std::unique_ptr<short[]> codewordBits = std::make_unique<short[]>(m_frameLengthInBits * 2);

    // split samples into 2 segments
    std::unique_ptr<short[]> n0 = std::make_unique<short[]>(MBE_SAMPLES_LENGTH / 2);
    for (int i = 0; i < MBE_SAMPLES_LENGTH / 2; i++)
        n0[i] = samples[i];

    ambe_voice_enc(codewordBits.get(), NO_BIT_STEAL, n0.get(), MBE_SAMPLES_LENGTH / 2, m_ecMode, 0, 8192, m_encoderState);

    std::unique_ptr<short[]> n1 = std::make_unique<short[]>(MBE_SAMPLES_LENGTH / 2);
    for (int i = 0; i < MBE_SAMPLES_LENGTH / 2; i++)
        n1[i] = samples[i + (MBE_SAMPLES_LENGTH / 2)];

    ambe_voice_enc(codewordBits.get(), NO_BIT_STEAL, n1.get(), MBE_SAMPLES_LENGTH / 2, m_ecMode, 1, 8192, m_encoderState);

    // is this to be a DMR codeword?
    if (m_txMode == TX_MODE_DMR) {
        UInt8Array bits = std::make_unique<uint8_t[]>(49);
        for (int i = 0; i < 49; i++)
            bits[i] = (uint8_t)codewordBits[i];

        // use the vocoder to create the ECC'ed and interleaved AMBE bits
        m_encoder->encodeBits(bits.get(), codeword);
    }
    else {
        // pack codeword from bits to bytes for use with external library
        packBitsToBytes(codewordBits.get(), codeword, m_frameLengthInBytes, m_frameLengthInBits);
    }
}
#endif // defined(_WIN32)

/* Reads basic configuration parameters from the YAML configuration file. */

bool HostBridge::readParams()
{
    yaml::Node systemConf = m_conf["system"];

    m_identity = systemConf["identity"].as<std::string>();

    m_rxAudioGain = systemConf["rxAudioGain"].as<float>(1.0f);
    m_vocoderDecoderAudioGain = systemConf["vocoderDecoderAudioGain"].as<float>(3.0f);
    m_vocoderDecoderAutoGain = systemConf["vocoderDecoderAutoGain"].as<bool>(false);
    m_txAudioGain = systemConf["txAudioGain"].as<float>(1.0f);
    m_vocoderEncoderAudioGain = systemConf["vocoderEncoderAudioGain"].as<float>(3.0f);
    
    m_txMode = (uint8_t)systemConf["txMode"].as<uint32_t>(1U);
    if (m_txMode < TX_MODE_DMR)
        m_txMode = TX_MODE_DMR;
    if (m_txMode > TX_MODE_P25)
        m_txMode = TX_MODE_P25;

    m_voxSampleLevel = systemConf["voxSampleLevel"].as<float>(30.0f);
    m_dropTimeMS = (uint16_t)systemConf["dropTimeMs"].as<uint32_t>(180);
    m_dropTime = Timer(1000U, 0U, m_dropTimeMS);

    m_detectAnalogMDC1200 = systemConf["detectAnalogMDC1200"].as<bool>(false);

    m_preambleLeaderTone = systemConf["preambleLeaderTone"].as<bool>(false);
    m_preambleTone = (uint16_t)systemConf["preambleTone"].as<uint32_t>(2175);
    m_preambleLength = (uint16_t)systemConf["preambleLength"].as<uint32_t>(200);

    m_dumpSampleLevel = systemConf["dumpSampleLevel"].as<bool>(false);

    m_grantDemand = systemConf["grantDemand"].as<bool>(false);

    m_localAudio = systemConf["localAudio"].as<bool>(true);

    LogInfo("General Parameters");
    LogInfo("    Rx Audio Gain: %.1f", m_rxAudioGain);
    LogInfo("    Vocoder Decoder Audio Gain: %.1f", m_vocoderDecoderAudioGain);
    LogInfo("    Vocoder Decoder Auto Gain: %s", m_vocoderDecoderAutoGain ? "yes" : "no");
    LogInfo("    Tx Audio Gain: %.1f", m_txAudioGain);
    LogInfo("    Vocoder Encoder Audio Gain: %.1f", m_vocoderEncoderAudioGain);
    LogInfo("    Transmit Mode: %s", m_txMode == TX_MODE_DMR ? "DMR" : "P25");
    LogInfo("    VOX Sample Level: %.1f", m_voxSampleLevel);
    LogInfo("    Drop Time: %ums", m_dropTimeMS);
    LogInfo("    Detect Analog MDC1200: %s", m_detectAnalogMDC1200 ? "yes" : "no");
    LogInfo("    Generate Preamble Tone: %s", m_preambleLeaderTone ? "yes" : "no");
    LogInfo("    Preamble Tone: %uhz", m_preambleTone);
    LogInfo("    Preamble Tone Length: %ums", m_preambleLength);
    LogInfo("    Dump Sample Levels: %s", m_dumpSampleLevel ? "yes" : "no");
    LogInfo("    Grant Demands: %s", m_grantDemand ? "yes" : "no");
    LogInfo("    Local Audio: %s", m_localAudio ? "yes" : "no");

    return true;
}

/* Initializes network connectivity. */

bool HostBridge::createNetwork()
{
    yaml::Node networkConf = m_conf["network"];

    std::string address = networkConf["address"].as<std::string>();
    uint16_t port = (uint16_t)networkConf["port"].as<uint32_t>(TRAFFIC_DEFAULT_PORT);
    uint16_t local = (uint16_t)networkConf["local"].as<uint32_t>(0U);
    uint32_t id = networkConf["id"].as<uint32_t>(1000U);
    std::string password = networkConf["password"].as<std::string>();
    bool debug = networkConf["debug"].as<bool>(false);

    m_udpAudio = networkConf["udpAudio"].as<bool>(false);
    m_udpMetadata = networkConf["udpMetadata"].as<bool>(false);
    m_udpSendPort = (uint16_t)networkConf["udpSendPort"].as<uint32_t>(34001);
    m_udpSendAddress = networkConf["udpSendAddress"].as<std::string>();
    m_udpReceivePort = (uint16_t)networkConf["udpReceivePort"].as<uint32_t>(34001);
    m_udpReceiveAddress = networkConf["udpReceiveAddress"].as<std::string>();

    m_srcId = (uint32_t)networkConf["sourceId"].as<uint32_t>(p25::defines::WUID_FNE);
    m_overrideSrcIdFromMDC = networkConf["overrideSourceIdFromMDC"].as<bool>(false);
    m_overrideSrcIdFromUDP = networkConf["overrideSourceIdFromUDP"].as<bool>(false);
    m_dstId = (uint32_t)networkConf["destinationId"].as<uint32_t>(1U);
    m_slot = (uint8_t)networkConf["slot"].as<uint32_t>(1U);

    bool encrypted = networkConf["encrypted"].as<bool>(false);
    std::string key = networkConf["presharedKey"].as<std::string>();
    uint8_t presharedKey[AES_WRAPPED_PCKT_KEY_LEN];
    if (!key.empty()) {
        if (key.size() == 32) {
            // bryanb: shhhhhhh....dirty nasty hacks
            key = key.append(key); // since the key is 32 characters (16 hex pairs), double it on itself for 64 characters (32 hex pairs)
            LogWarning(LOG_HOST, "Half-length network preshared encryption key detected, doubling key on itself.");
        }

        if (key.size() == 64) {
            if ((key.find_first_not_of("0123456789abcdefABCDEF", 2) == std::string::npos)) {
                const char* keyPtr = key.c_str();
                ::memset(presharedKey, 0x00U, AES_WRAPPED_PCKT_KEY_LEN);

                for (uint8_t i = 0; i < AES_WRAPPED_PCKT_KEY_LEN; i++) {
                    char t[4] = { keyPtr[0], keyPtr[1], 0 };
                    presharedKey[i] = (uint8_t)::strtoul(t, NULL, 16);
                    keyPtr += 2 * sizeof(char);
                }
            }
            else {
                LogWarning(LOG_HOST, "Invalid characters in the network preshared encryption key. Encryption disabled.");
                encrypted = false;
            }
        }
        else {
            LogWarning(LOG_HOST, "Invalid  network preshared encryption key length, key should be 32 hex pairs, or 64 characters. Encryption disabled.");
            encrypted = false;
        }
    }

    if (id > 999999999U) {
        ::LogError(LOG_HOST, "Network Peer ID cannot be greater then 999999999.");
        return false;
    }

    LogInfo("Network Parameters");
    LogInfo("    Peer ID: %u", id);
    LogInfo("    Address: %s", address.c_str());
    LogInfo("    Port: %u", port);
    if (local > 0U)
        LogInfo("    Local: %u", local);
    else
        LogInfo("    Local: random");

    LogInfo("    Encrypted: %s", encrypted ? "yes" : "no");

    LogInfo("    PCM over UDP Audio: %s", m_udpAudio ? "yes" : "no");
    if (m_udpAudio) {
        LogInfo("    UDP Audio Metadata: %s", m_udpMetadata ? "yes" : "no");
        LogInfo("    UDP Audio end Address: %s", m_udpSendAddress.c_str());
        LogInfo("    UDP Audio Send Port: %u", m_udpSendPort);
        LogInfo("    UDP Audio Receive Address: %s", m_udpReceiveAddress.c_str());
        LogInfo("    UDP Audio Receive Port: %u", m_udpReceivePort);
    }

    LogInfo("    Source ID: %u", m_srcId);
    LogInfo("    Destination ID: %u", m_dstId);
    LogInfo("    DMR Slot: %u", m_slot);
    LogInfo("    Override Source ID from MDC: %s", m_overrideSrcIdFromMDC ? "yes" : "no");
    LogInfo("    Override Source ID from UDP Audio: %s", m_overrideSrcIdFromUDP ? "yes" : "no");

    if (debug) {
        LogInfo("    Debug: yes");
    }

    bool dmr = false, p25 = false;
    switch (m_txMode) {
    case TX_MODE_DMR:
        dmr = true;
        break;
    case TX_MODE_P25:
        p25 = true;
        break;
    }

    // initialize networking
    m_network = new PeerNetwork(address, port, local, id, password, true, debug, dmr, p25, false, true, true, true, true, true, false);

    m_network->setMetadata(m_identity, 0U, 0U, 0.0F, 0.0F, 0, 0, 0, 0.0F, 0.0F, 0, "");
    m_network->setConventional(true);

    if (encrypted) {
        m_network->setPresharedKey(presharedKey);
    }

    m_network->enable(true);
    bool ret = m_network->open();
    if (!ret) {
        delete m_network;
        m_network = nullptr;
        LogError(LOG_HOST, "failed to initialize traffic networking!");
        return false;
    }

    ::LogSetNetwork(m_network);

    if (m_udpAudio) {
        m_udpAudioSocket = new Socket(m_udpSendAddress, m_udpSendPort);
    }

    return true;
}

/* Helper to process UDP audio. */

void HostBridge::processUDPAudio()
{
    if (!m_udpAudio)
        return;
    if (m_udpAudioSocket == nullptr)
        return;

    sockaddr_storage addr;
    uint32_t addrLen;

    // read message from socket
    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);
    int length = m_udpAudioSocket->read(buffer, DATA_PACKET_LENGTH, addr, addrLen);
    if (length < 0) {
        LogError(LOG_NET, "Failed reading data from the network");
        return;
    }

    if (length > 0) {
        if (m_debug)
            Utils::dump(1U, "UDP Audio Network Packet", buffer, length);
        
        uint32_t pcmLength = __GET_UINT32(buffer, 0U);

        UInt8Array __pcm = std::make_unique<uint8_t[]>(pcmLength);
        uint8_t* pcm = __pcm.get();

        ::memcpy(pcm, buffer + 4U, pcmLength);

        // Utils::dump(1U, "PCM RECV BYTE BUFFER", pcm, pcmLength);

        m_udpSrcId = m_srcId;
        if (m_udpMetadata) {
            if (m_overrideSrcIdFromUDP)
                m_udpSrcId = __GET_UINT32(buffer, pcmLength + 4U);
        }

        m_udpDstId = m_dstId;

        std::lock_guard<std::mutex> lock(m_audioMutex);

        int smpIdx = 0;
        short samples[MBE_SAMPLES_LENGTH];
        for (uint32_t pcmIdx = 0; pcmIdx < pcmLength; pcmIdx += 2) {
            samples[smpIdx] = (short)((pcm[pcmIdx + 1] << 8) + pcm[pcmIdx + 0]);
            smpIdx++;
        }

        m_inputAudio.addData(samples, MBE_SAMPLES_LENGTH);

        m_trafficFromUDP = true;

        // force start a call if one isn't already in progress
        if (!m_audioDetect && !m_callInProgress) {
            m_audioDetect = true;
            if (m_txStreamId == 0U) {
                m_txStreamId = 1U; // prevent further false starts -- this isn't the right way to handle this...
                LogMessage(LOG_HOST, "%s, call start, srcId = %u, dstId = %u", UDP_CALL, m_udpSrcId, m_udpDstId);
                if (m_grantDemand) {
                    switch (m_txMode)
                    {
                    case TX_MODE_P25:
                    {
                        p25::lc::LC lc = p25::lc::LC();
                        lc.setLCO(p25::defines::LCO::GROUP);
                        lc.setDstId(m_udpDstId);
                        lc.setSrcId(m_udpSrcId);

                        p25::data::LowSpeedData lsd = p25::data::LowSpeedData();

                        uint8_t controlByte = 0x80U;
                        m_network->writeP25TDU(lc, lsd, controlByte);
                    }
                    break;
                    }
                }
            }

            m_dropTime.stop();

            if (!m_dropTime.isRunning())
                m_dropTime.start();
        }

        // If audio detection is active and no call is in progress, encode and transmit the audio
        if (m_audioDetect && !m_callInProgress) {
            switch (m_txMode) {
            case TX_MODE_DMR:
                encodeDMRAudioFrame(pcm, m_udpSrcId);
                break;
            case TX_MODE_P25:
                encodeP25AudioFrame(pcm, m_udpSrcId);
                break;
            }
        }
    }
}

/* Helper to process DMR network traffic. */

void HostBridge::processDMRNetwork(uint8_t* buffer, uint32_t length)
{
    assert(buffer != nullptr);
    using namespace dmr;
    using namespace dmr::defines;

    if (m_txMode != TX_MODE_DMR)
        return;

    // process network message header
    uint8_t seqNo = buffer[4U];

    uint32_t srcId = __GET_UINT16(buffer, 5U);
    uint32_t dstId = __GET_UINT16(buffer, 8U);

    FLCO::E flco = (buffer[15U] & 0x40U) == 0x40U ? FLCO::PRIVATE : FLCO::GROUP;

    uint32_t slotNo = (buffer[15U] & 0x80U) == 0x80U ? 2U : 1U;

    if (slotNo > 3U) {
        LogError(LOG_DMR, "DMR, invalid slot, slotNo = %u", slotNo);
        return;
    }

    // DMO mode slot disabling
    if (slotNo == 1U && !m_network->getDuplex()) {
        LogError(LOG_DMR, "DMR/DMO, invalid slot, slotNo = %u", slotNo);
        return;
    }

    // Individual slot disabling
    if (slotNo == 1U && !m_network->getDMRSlot1()) {
        LogError(LOG_DMR, "DMR, invalid slot, slot 1 disabled, slotNo = %u", slotNo);
        return;
    }
    if (slotNo == 2U && !m_network->getDMRSlot2()) {
        LogError(LOG_DMR, "DMR, invalid slot, slot 2 disabled, slotNo = %u", slotNo);
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

    if (flco == FLCO::GROUP) {
        if (srcId == 0)
            return;

        // ensure destination ID matches and slot matches
        if (dstId != m_dstId && slotNo == m_slot)
            return;

        // is this a new call stream?
        if (m_network->getDMRStreamId(slotNo) != m_rxStreamId) {
            m_callInProgress = true;
            m_callAlgoId = 0U;

            uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            m_rxStartTime = now;

            LogMessage(LOG_HOST, "DMR, call start, srcId = %u, dstId = %u, slot = %u", srcId, dstId, slotNo);
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

        if (dataSync && (dataType == DataType::TERMINATOR_WITH_LC)) {
            m_callInProgress = false;
            m_ignoreCall = false;
            m_callAlgoId = 0U;

            uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            uint64_t diff = now - m_rxStartTime;

            LogMessage(LOG_HOST, "DMR, call end, srcId = %u, dstId = %u, dur = %us", srcId, dstId, diff / 1000U);
            
            m_rxDMRLC = lc::LC();
            m_rxDMRPILC = lc::PrivacyLC();
            m_rxStartTime = 0U;
            m_rxStreamId = 0U;
            return;
        }

        if (m_ignoreCall && m_callAlgoId == 0U)
            m_ignoreCall = false;

        if (m_ignoreCall)
            return;

        if (m_callAlgoId != 0U) {
            if (m_callInProgress) {
                m_callInProgress = false;

                uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                uint64_t diff = now - m_rxStartTime;

                LogMessage(LOG_HOST, "P25, call end (T), srcId = %u, dstId = %u, dur = %us", srcId, dstId, diff / 1000U);
            }

            m_ignoreCall = true;
            return;
        }

        if (dataType == DataType::VOICE_SYNC || dataType == DataType::VOICE) {
            uint8_t ambe[27U];
            ::memcpy(ambe, data.get(), 14U);
            ambe[13] &= 0xF0;
            ambe[13] |= (uint8_t)(data[19] & 0x0F);
            ::memcpy(ambe + 14U, data.get() + 20U, 13U);

            LogMessage(LOG_NET, DMR_DT_VOICE ", audio, slot = %u, srcId = %u, dstId = %u, seqNo = %u", slotNo, srcId, dstId, n);
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

        short samples[MBE_SAMPLES_LENGTH];
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
            LogMessage(LOG_HOST, DMR_DT_VOICE ", Frame, VC%u.%u, srcId = %u, dstId = %u, errs = %u", dmrN, n, srcId, dstId, errs);

        // post-process: apply gain to decoded audio frames
        if (m_rxAudioGain != 1.0f) {
            for (int n = 0; n < MBE_SAMPLES_LENGTH; n++) {
                short sample = samples[n];
                float newSample = sample * m_rxAudioGain;
                sample = (short)newSample;

                // clip if necessary
                if (m_rxAudioGain > 1.0f) {
                    if (newSample > 32767)
                        sample = 32767;
                    else if (newSample < -32767)
                        sample = -32767;
                }

                samples[n] = sample;
            }
        }

        if (m_localAudio) {
            m_outputAudio.addData(samples, MBE_SAMPLES_LENGTH);
        }

        if (m_udpAudio) {
            int pcmIdx = 0;
            uint8_t pcm[MBE_SAMPLES_LENGTH * 2U];
            for (uint32_t smpIdx = 0; smpIdx < MBE_SAMPLES_LENGTH; smpIdx++) {
                pcm[pcmIdx + 0] = (uint8_t)(samples[smpIdx] & 0xFF);
                pcm[pcmIdx + 1] = (uint8_t)((samples[smpIdx] >> 8) & 0xFF);
                pcmIdx += 2;
            }

            uint32_t length = (MBE_SAMPLES_LENGTH * 2U) + 4U;
            uint8_t* audioData = nullptr;
            if (!m_udpMetadata) {
                audioData = new uint8_t[(MBE_SAMPLES_LENGTH * 2U) + 4U]; // PCM + 4 bytes (PCM length)
                __SET_UINT32((MBE_SAMPLES_LENGTH * 2U), audioData, 0U);
                ::memcpy(audioData + 4U, pcm, MBE_SAMPLES_LENGTH * 2U);
            }
            else {
                length = (MBE_SAMPLES_LENGTH * 2U) + 12U;
                audioData = new uint8_t[(MBE_SAMPLES_LENGTH * 2U) + 12U]; // PCM + (4 bytes (PCM length) + 4 bytes (srcId) + 4 bytes (dstId))
                __SET_UINT32((MBE_SAMPLES_LENGTH * 2U), audioData, 0U);
                ::memcpy(audioData + 4U, pcm, MBE_SAMPLES_LENGTH * 2U);

                // embed destination and source IDs
                __SET_UINT32(dstId, audioData, ((MBE_SAMPLES_LENGTH * 2U) + 0U));
                __SET_UINT32(srcId, audioData, ((MBE_SAMPLES_LENGTH * 2U) + 4U));
            }

            sockaddr_storage addr;
            uint32_t addrLen;

            if (udp::Socket::lookup(m_udpSendAddress, m_udpSendPort, addr, addrLen) == 0) {
                m_udpAudioSocket->write(audioData, length, addr, addrLen);
            }

            delete[] audioData;
        }
    }
}

/* Helper to encode DMR network traffic audio frames. */

void HostBridge::encodeDMRAudioFrame(uint8_t* pcm, uint32_t forcedSrcId, uint32_t forcedDstId)
{
    assert(pcm != nullptr);
    using namespace dmr;
    using namespace dmr::defines;

    uint32_t srcId = m_srcId;
    if (m_srcIdOverride != 0 && (m_overrideSrcIdFromMDC || m_overrideSrcIdFromUDP))
        srcId = m_srcIdOverride;
    if (forcedSrcId > 0 && forcedSrcId != m_srcId)
        srcId = forcedSrcId;
    uint32_t dstId = m_dstId;
    if (forcedDstId > 0 && forcedDstId != m_dstId)
        dstId = forcedDstId;

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
            data::NetData dmrData;
            dmrData.setSlotNo(m_slot);
            dmrData.setDataType(DataType::VOICE_LC_HEADER);
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
            data::EMB emb = data::EMB();
            emb.setColorCode(0U);
            emb.setLCSS(lcss);
            emb.encode(data);
        }

        LogMessage(LOG_HOST, DMR_DT_VOICE ", srcId = %u, dstId = %u, slot = %u, seqNo = %u", srcId, dstId, m_slot, m_dmrN);

        // generate DMR network frame
        data::NetData dmrData;
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
    short samples[MBE_SAMPLES_LENGTH];
    for (uint32_t pcmIdx = 0; pcmIdx < (MBE_SAMPLES_LENGTH * 2U); pcmIdx += 2) {
        samples[smpIdx] = (short)((pcm[pcmIdx + 1] << 8) + pcm[pcmIdx + 0]);
        smpIdx++;
    }

    // pre-process: apply gain to PCM audio frames
    if (m_txAudioGain != 1.0f) {
        for (int n = 0; n < MBE_SAMPLES_LENGTH; n++) {
            short sample = samples[n];
            float newSample = sample * m_txAudioGain;
            sample = (short)newSample;

            // clip if necessary
            if (m_txAudioGain > 1.0f) {
                if (newSample > 32767)
                    sample = 32767;
                else if (newSample < -32767)
                    sample = -32767;
            }

            samples[n] = sample;
        }
    }

    // encode PCM samples into AMBE codewords
    uint8_t ambe[RAW_AMBE_LENGTH_BYTES];
    ::memset(ambe, 0x00U, RAW_AMBE_LENGTH_BYTES);
#if defined(_WIN32)
    if (m_useExternalVocoder) {
        ambeEncode(samples, MBE_SAMPLES_LENGTH, ambe);
    }
    else {
#endif // defined(_WIN32)
        m_encoder->encode(samples, ambe);
#if defined(_WIN32)
    }
#endif // defined(_WIN32)

    // Utils::dump(1U, "Encoded AMBE", ambe, RAW_AMBE_LENGTH_BYTES);

    ::memcpy(m_ambeBuffer + (m_ambeCount * 9U), ambe, RAW_AMBE_LENGTH_BYTES);
    m_ambeCount++;
}

/* Helper to process P25 network traffic. */

void HostBridge::processP25Network(uint8_t* buffer, uint32_t length)
{
    assert(buffer != nullptr);
    using namespace p25;
    using namespace p25::defines;
    using namespace p25::dfsi::defines;

    if (m_txMode != TX_MODE_P25)
        return;

    bool grantDemand = (buffer[14U] & 0x80U) == 0x80U;
    bool grantDenial = (buffer[14U] & 0x40U) == 0x40U;
    bool unitToUnit = (buffer[14U] & 0x01U) == 0x01U;

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

    uint32_t srcId = __GET_UINT16(buffer, 5U);
    uint32_t dstId = __GET_UINT16(buffer, 8U);

    uint8_t lsd1 = buffer[20U];
    uint8_t lsd2 = buffer[21U];

    lc::LC control;
    data::LowSpeedData lsd;

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

    if (control.getLCO() == LCO::GROUP) {
        if (srcId == 0)
            return;

        if ((duid == DUID::TDU) || (duid == DUID::TDULC)) {
            // ignore TDU's that are grant demands
            if (grantDemand)
                return;
        }

        // ensure destination ID matches
        if (dstId != m_dstId)
            return;

        // is this a new call stream?
        if (m_network->getP25StreamId() != m_rxStreamId && ((duid != DUID::TDU) && (duid != DUID::TDULC))) {
            m_callInProgress = true;
            m_callAlgoId = ALGO_UNENCRYPT;

            uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            m_rxStartTime = now;

            LogMessage(LOG_HOST, "P25, call start, srcId = %u, dstId = %u", srcId, dstId);
            if (m_preambleLeaderTone)
                generatePreambleTone();
        }

        if ((duid == DUID::TDU) || (duid == DUID::TDULC)) {
            m_callInProgress = false;
            m_ignoreCall = false;
            m_callAlgoId = ALGO_UNENCRYPT;

            uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            uint64_t diff = now - m_rxStartTime;

            LogMessage(LOG_HOST, "P25, call end, srcId = %u, dstId = %u, dur = %us", srcId, dstId, diff / 1000U);

            m_rxP25LC = lc::LC();
            m_rxStartTime = 0U;
            m_rxStreamId = 0U;
            return;
        }

        if (m_ignoreCall && m_callAlgoId == ALGO_UNENCRYPT)
            m_ignoreCall = false;

        // if this is an LDU1 see if this is the first LDU with HDU encryption data
        if (duid == DUID::LDU1 && !m_ignoreCall) {
            uint8_t frameType = buffer[180U];
            if (frameType == FrameType::HDU_VALID)
                m_callAlgoId = buffer[181U];
        }

        if (duid == DUID::LDU2 && !m_ignoreCall)
            m_callAlgoId = data[88];

        if (m_ignoreCall)
            return;

        if (m_callAlgoId != ALGO_UNENCRYPT) {
            if (m_callInProgress) {
                m_callInProgress = false;

                uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                uint64_t diff = now - m_rxStartTime;

                LogMessage(LOG_HOST, "P25, call end (T), srcId = %u, dstId = %u, dur = %us", srcId, dstId, diff / 1000U);
            }

            m_ignoreCall = true;
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

                LogMessage(LOG_NET, P25_LDU1_STR " audio, srcId = %u, dstId = %u", srcId, dstId);

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

                LogMessage(LOG_NET, P25_LDU2_STR " audio");

                // decode 9 IMBE codewords into PCM samples
                decodeP25AudioFrame(m_netLDU2, srcId, dstId, 2U);
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

        // Utils::dump(1U, "IMBE", imbe, RAW_IMBE_LENGTH_BYTES);

        short samples[MBE_SAMPLES_LENGTH];
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
        if (m_rxAudioGain != 1.0f) {
            for (int n = 0; n < MBE_SAMPLES_LENGTH; n++) {
                short sample = samples[n];
                float newSample = sample * m_rxAudioGain;
                sample = (short)newSample;

                // clip if necessary
                if (m_rxAudioGain > 1.0f) {
                    if (newSample > 32767)
                        sample = 32767;
                    else if (newSample < -32767)
                        sample = -32767;
                }

                samples[n] = sample;
            }
        }

        if (m_localAudio) {
            m_outputAudio.addData(samples, MBE_SAMPLES_LENGTH);
        }

        if (m_udpAudio) {
            int pcmIdx = 0;
            uint8_t pcm[MBE_SAMPLES_LENGTH * 2U];
            for (uint32_t smpIdx = 0; smpIdx < MBE_SAMPLES_LENGTH; smpIdx++) {
                pcm[pcmIdx + 0] = (uint8_t)(samples[smpIdx] & 0xFF);
                pcm[pcmIdx + 1] = (uint8_t)((samples[smpIdx] >> 8) & 0xFF);
                pcmIdx += 2;
            }

            uint32_t length = (MBE_SAMPLES_LENGTH * 2U) + 4U;
            uint8_t* audioData = nullptr;
            if (!m_udpMetadata) {
                audioData = new uint8_t[(MBE_SAMPLES_LENGTH * 2U) + 4U]; // PCM + 4 bytes (PCM length)
                __SET_UINT32((MBE_SAMPLES_LENGTH * 2U), audioData, 0U);
                ::memcpy(audioData + 4U, pcm, MBE_SAMPLES_LENGTH * 2U);
            }
            else {
                length = (MBE_SAMPLES_LENGTH * 2U) + 12U;
                audioData = new uint8_t[(MBE_SAMPLES_LENGTH * 2U) + 12U]; // PCM + (4 bytes (PCM length) + 4 bytes (srcId) + 4 bytes (dstId))
                __SET_UINT32((MBE_SAMPLES_LENGTH * 2U), audioData, 0U);
                ::memcpy(audioData + 4U, pcm, MBE_SAMPLES_LENGTH * 2U);

                // embed destination and source IDs
                __SET_UINT32(dstId, audioData, ((MBE_SAMPLES_LENGTH * 2U) + 0U));
                __SET_UINT32(srcId, audioData, ((MBE_SAMPLES_LENGTH * 2U) + 4U));
            }

            sockaddr_storage addr;
            uint32_t addrLen;

            if (udp::Socket::lookup(m_udpSendAddress, m_udpSendPort, addr, addrLen) == 0) {
                m_udpAudioSocket->write(audioData, length, addr, addrLen);
            }

            delete[] audioData;
        }
    }
}

/* Helper to encode P25 network traffic audio frames. */

void HostBridge::encodeP25AudioFrame(uint8_t* pcm, uint32_t forcedSrcId, uint32_t forcedDstId)
{
    assert(pcm != nullptr);
    using namespace p25;
    using namespace p25::defines;

    if (m_p25N > 17)
        m_p25N = 0;
    if (m_p25N == 0)
        ::memset(m_netLDU1, 0x00U, 9U * 25U);
    if (m_p25N == 9)
        ::memset(m_netLDU2, 0x00U, 9U * 25U);

    int smpIdx = 0;
    short samples[MBE_SAMPLES_LENGTH];
    for (uint32_t pcmIdx = 0; pcmIdx < (MBE_SAMPLES_LENGTH * 2U); pcmIdx += 2) {
        samples[smpIdx] = (short)((pcm[pcmIdx + 1] << 8) + pcm[pcmIdx + 0]);
        smpIdx++;
    }

    // pre-process: apply gain to PCM audio frames
    if (m_txAudioGain != 1.0f) {
        for (int n = 0; n < MBE_SAMPLES_LENGTH; n++) {
            short sample = samples[n];
            float newSample = sample * m_txAudioGain;
            sample = (short)newSample;

            // clip if necessary
            if (m_txAudioGain > 1.0f) {
                if (newSample > 32767)
                    sample = 32767;
                else if (newSample < -32767)
                    sample = -32767;
            }

            samples[n] = sample;
        }
    }

    // encode PCM samples into IMBE codewords
    uint8_t imbe[RAW_IMBE_LENGTH_BYTES];
    ::memset(imbe, 0x00U, RAW_IMBE_LENGTH_BYTES);
#if defined(_WIN32)
    if (m_useExternalVocoder) {
        ambeEncode(samples, MBE_SAMPLES_LENGTH, imbe);
    }
    else {
#endif // defined(_WIN32)
        m_encoder->encode(samples, imbe);
#if defined(_WIN32)
    }
#endif // defined(_WIN32)

    // Utils::dump(1U, "Encoded IMBE", imbe, RAW_IMBE_LENGTH_BYTES);

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
    if (m_srcIdOverride != 0 && (m_overrideSrcIdFromMDC || m_overrideSrcIdFromUDP))
        srcId = m_srcIdOverride;
    if (forcedSrcId > 0 && forcedSrcId != m_srcId)
        srcId = forcedSrcId;
    uint32_t dstId = m_dstId;
    if (forcedDstId > 0 && forcedDstId != m_dstId)
        dstId = forcedDstId;

    lc::LC lc = lc::LC();
    lc.setLCO(LCO::GROUP);
    lc.setGroup(true);
    lc.setPriority(4U);
    lc.setDstId(dstId);
    lc.setSrcId(srcId);

    data::LowSpeedData lsd = data::LowSpeedData();

    // send P25 LDU1
    if (m_p25N == 8U) {
        LogMessage(LOG_HOST, P25_LDU1_STR " audio, srcId = %u, dstId = %u", srcId, dstId);
        m_network->writeP25LDU1(lc, lsd, m_netLDU1, FrameType::HDU_VALID);
        m_txStreamId = m_network->getP25StreamId();
    }

    // send P25 LDU2
    if (m_p25N == 17U) {
        LogMessage(LOG_HOST, P25_LDU2_STR " audio");
        m_network->writeP25LDU2(lc, lsd, m_netLDU2);
    }

    m_p25SeqNo++;
    m_p25N++;
}

/* Helper to generate the preamble tone. */

void HostBridge::generatePreambleTone()
{
    std::lock_guard<std::mutex> lock(m_audioMutex);

    uint64_t frameCount = SampleTimeConvert::ToSamples(SAMPLE_RATE, 1, m_preambleLength);
    if (frameCount > m_outputAudio.freeSpace()) {
        ::LogError(LOG_HOST, "failed to generate preamble tone");
        return;
    }

    ma_uint32 pcmBytes = frameCount * ma_get_bytes_per_frame(m_maDevice.capture.format, m_maDevice.capture.channels);
    UInt8Array __sine = std::make_unique<uint8_t[]>(pcmBytes);
    uint8_t* sine = __sine.get();

    ma_waveform_read_pcm_frames(&m_maSineWaveform, sine, frameCount, NULL);

    int smpIdx = 0;
    std::unique_ptr<short[]> __UNIQUE_sineSamples = std::make_unique<short[]>(frameCount);
    short* sineSamples = __UNIQUE_sineSamples.get();
    const uint8_t* pcm = (const uint8_t*)sine;
    for (uint32_t pcmIdx = 0; pcmIdx < pcmBytes; pcmIdx += 2) {
        sineSamples[smpIdx] = (short)((pcm[pcmIdx + 1] << 8) + pcm[pcmIdx + 0]);
        smpIdx++;
    }

    m_outputAudio.addData(sineSamples, frameCount);
}

/* Entry point to audio processing thread. */

void* HostBridge::threadAudioProcess(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
#if defined(_WIN32)
        ::CloseHandle(th->thread);
#else
        ::pthread_detach(th->thread);
#endif // defined(_WIN32)

        std::string threadName("bridge:audio-process");
        HostBridge* bridge = static_cast<HostBridge*>(th->obj);
        if (bridge == nullptr) {
            g_killed = true;
            LogDebug(LOG_HOST, "[FAIL] %s", threadName.c_str());
        }

        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogDebug(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        StopWatch stopWatch;
        stopWatch.start();

        while (!g_killed) {
            if (!bridge->m_running) {
                Thread::sleep(1U);
                continue;
            }

            uint32_t ms = stopWatch.elapsed();
            stopWatch.start();

            if (bridge->m_dropTime.isRunning())
                bridge->m_dropTime.clock(ms);

            // scope is intentional
            {
                std::lock_guard<std::mutex> lock(m_audioMutex);

                if (bridge->m_inputAudio.dataSize() >= MBE_SAMPLES_LENGTH) {
                    short samples[MBE_SAMPLES_LENGTH];
                    bridge->m_inputAudio.get(samples, MBE_SAMPLES_LENGTH);

                    // process MDC, if necessary
                    if (bridge->m_overrideSrcIdFromMDC)
                        mdc_decoder_process_samples(bridge->m_mdcDecoder, samples, MBE_SAMPLES_LENGTH);

                    float sampleLevel = bridge->m_voxSampleLevel / 1000;

                    uint32_t srcId = bridge->m_srcId;
                    if (bridge->m_srcIdOverride != 0 && bridge->m_overrideSrcIdFromMDC)
                        srcId = bridge->m_srcIdOverride;

                    uint32_t dstId = bridge->m_dstId;

                    std::string trafficType = LOCAL_CALL;
                    if (bridge->m_trafficFromUDP) {
                        srcId = bridge->m_udpSrcId;
                        trafficType = UDP_CALL;
                    }

                    // perform maximum sample detection
                    float maxSample = 0.0f;
                    for (int i = 0; i < MBE_SAMPLES_LENGTH; i++) {
                        float sampleValue = fabs((float)samples[i]);
                        maxSample = fmax(maxSample, sampleValue);
                    }
                    maxSample = maxSample / 1000;

                    if (bridge->m_dumpSampleLevel && bridge->m_detectedSampleCnt > 50U) {
                        bridge->m_detectedSampleCnt = 0U;
                        ::LogInfoEx(LOG_HOST, "Detected Sample Level: %.2f", maxSample * 1000);
                    }

                    if (bridge->m_dumpSampleLevel) {
                        bridge->m_detectedSampleCnt++;
                    }

                    // handle Rx triggered by internal VOX
                    if (maxSample > sampleLevel) {
                        bridge->m_audioDetect = true;
                        if (bridge->m_txStreamId == 0U) {
                            bridge->m_txStreamId = 1U; // prevent further false starts -- this isn't the right way to handle this...
                            LogMessage(LOG_HOST, "%s, call start, srcId = %u, dstId = %u", trafficType.c_str(), srcId, dstId);

                            if (bridge->m_grantDemand) {
                                switch (bridge->m_txMode) {
                                case TX_MODE_P25:
                                {
                                    p25::lc::LC lc = p25::lc::LC();
                                    lc.setLCO(p25::defines::LCO::GROUP);
                                    lc.setDstId(dstId);
                                    lc.setSrcId(srcId);

                                    p25::data::LowSpeedData lsd = p25::data::LowSpeedData();

                                    uint8_t controlByte = 0x80U;
                                    bridge->m_network->writeP25TDU(lc, lsd, controlByte);
                                }
                                break;
                                }
                            }
                        }

                        bridge->m_dropTime.stop();
                    } else {
                        // if we've exceeded the audio drop timeout, then really drop the audio
                        if (bridge->m_dropTime.isRunning() && bridge->m_dropTime.hasExpired()) {
                            if (bridge->m_audioDetect) {
                                LogMessage(LOG_HOST, "%s, call end, srcId = %u, dstId = %u", trafficType.c_str(), srcId, dstId);

                                bridge->m_audioDetect = false;
                                bridge->m_dropTime.stop();

                                if (!bridge->m_callInProgress) {
                                    switch (bridge->m_txMode) {
                                    case TX_MODE_DMR:
                                    {
                                        dmr::data::NetData data = dmr::data::NetData();
                                        data.setDataType(dmr::defines::DataType::TERMINATOR_WITH_LC);
                                        data.setDstId(dstId);
                                        data.setSrcId(srcId);

                                        bridge->m_network->writeDMRTerminator(data, &bridge->m_dmrSeqNo, &bridge->m_dmrN, bridge->m_dmrEmbeddedData);
                                    }
                                    break;
                                    case TX_MODE_P25:
                                    {
                                        p25::lc::LC lc = p25::lc::LC();
                                        lc.setLCO(p25::defines::LCO::GROUP);
                                        lc.setDstId(dstId);
                                        lc.setSrcId(srcId);

                                        p25::data::LowSpeedData lsd = p25::data::LowSpeedData();

                                        uint8_t controlByte = 0x00U;
                                        bridge->m_network->writeP25TDU(lc, lsd, controlByte);
                                    }
                                    break;
                                    }
                                }

                                bridge->m_srcIdOverride = 0;
                                bridge->m_txStreamId = 0;

                                bridge->m_udpSrcId = 0;
                                bridge->m_udpDstId = 0;
                                bridge->m_trafficFromUDP = false;
                            }
                        }

                        if (!bridge->m_dropTime.isRunning())
                            bridge->m_dropTime.start();
                    }

                    if (bridge->m_audioDetect && !bridge->m_callInProgress) {
                        ma_uint32 pcmBytes = MBE_SAMPLES_LENGTH * ma_get_bytes_per_frame(bridge->m_maDevice.capture.format, bridge->m_maDevice.capture.channels);
                        UInt8Array __pcm = std::make_unique<uint8_t[]>(pcmBytes);
                        uint8_t* pcm = __pcm.get();

                        int pcmIdx = 0;
                        for (uint32_t smpIdx = 0; smpIdx < MBE_SAMPLES_LENGTH; smpIdx++) {
                            pcm[pcmIdx + 0] = (uint8_t)(samples[smpIdx] & 0xFF);
                            pcm[pcmIdx + 1] = (uint8_t)((samples[smpIdx] >> 8) & 0xFF);
                            pcmIdx += 2;
                        }

                        switch (bridge->m_txMode)
                        {
                        case TX_MODE_DMR:
                            bridge->encodeDMRAudioFrame(pcm);
                            break;
                        case TX_MODE_P25:
                            bridge->encodeP25AudioFrame(pcm);
                            break;
                        }
                    }
                }
            }

            Thread::sleep(1U);
        }

        LogDebug(LOG_HOST, "[STOP] %s", threadName.c_str());
        delete th;
    }

    return nullptr;
}

/* Entry point to network processing thread. */

void* HostBridge::threadNetworkProcess(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
#if defined(_WIN32)
        ::CloseHandle(th->thread);
#else
        ::pthread_detach(th->thread);
#endif // defined(_WIN32)

        std::string threadName("bridge:net-process");
        HostBridge* bridge = static_cast<HostBridge*>(th->obj);
        if (bridge == nullptr) {
            g_killed = true;
            LogDebug(LOG_HOST, "[FAIL] %s", threadName.c_str());
        }

        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogDebug(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        StopWatch stopWatch;
        stopWatch.start();

        while (!g_killed) {
            if (!bridge->m_running) {
                Thread::sleep(1U);
                continue;
            }

            uint32_t ms = stopWatch.elapsed();
            stopWatch.start();

            uint32_t length = 0U;
            bool netReadRet = false;
            if (bridge->m_txMode == TX_MODE_DMR) {
                UInt8Array dmrBuffer = bridge->m_network->readDMR(netReadRet, length);
                if (netReadRet) {
                    bridge->processDMRNetwork(dmrBuffer.get(), length);
                }
            }

            if (bridge->m_txMode == TX_MODE_P25) {
                UInt8Array p25Buffer = bridge->m_network->readP25(netReadRet, length);
                if (netReadRet) {
                    bridge->processP25Network(p25Buffer.get(), length);
                }
            }

            Thread::sleep(1U);
        }

        LogDebug(LOG_HOST, "[STOP] %s", threadName.c_str());
        delete th;
    }

    return nullptr;
}

/* Entry point to call lockup handler thread. */

void* HostBridge::threadCallLockup(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
#if defined(_WIN32)
        ::CloseHandle(th->thread);
#else
        ::pthread_detach(th->thread);
#endif // defined(_WIN32)

        std::string threadName("bridge:call-lockup");
        HostBridge* bridge = static_cast<HostBridge*>(th->obj);
        if (bridge == nullptr) {
            g_killed = true;
            LogDebug(LOG_HOST, "[FAIL] %s", threadName.c_str());
        }

        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogDebug(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        StopWatch stopWatch;
        stopWatch.start();

        while (!g_killed) {
            if (!bridge->m_running) {
                Thread::sleep(1U);
                continue;
            }

            uint32_t ms = stopWatch.elapsed();
            stopWatch.start();

            std::string trafficType = LOCAL_CALL;
            if (bridge->m_trafficFromUDP)
                trafficType = UDP_CALL;

            uint32_t srcId = bridge->m_srcId;
            if (bridge->m_srcIdOverride != 0 && bridge->m_overrideSrcIdFromMDC)
                srcId = bridge->m_srcIdOverride;

            uint32_t dstId = bridge->m_dstId;

            ulong64_t temp = (bridge->m_dropTimeMS) * 1000U;
            uint32_t dropTimeout = (uint32_t)((temp / 1000ULL + 1ULL) * 2U);

            // if we've exceeded the audio drop timeout, then really drop the audio
            if ((bridge->m_dropTime.isRunning() && (bridge->m_dropTime.getTimer() >= dropTimeout)) ||
                (!bridge->m_dropTime.isRunning() && !bridge->m_audioDetect && bridge->m_callInProgress)) {
                LogMessage(LOG_HOST, "%s, call end (S)", trafficType.c_str());

                bridge->m_audioDetect = false;
                bridge->m_dropTime.stop();

                if (!bridge->m_callInProgress) {
                    switch (bridge->m_txMode) {
                    case TX_MODE_DMR:
                    {
                        dmr::data::NetData data = dmr::data::NetData();
                        data.setDataType(dmr::defines::DataType::TERMINATOR_WITH_LC);
                        data.setDstId(dstId);
                        data.setSrcId(srcId);

                        bridge->m_network->writeDMRTerminator(data, &bridge->m_dmrSeqNo, &bridge->m_dmrN, bridge->m_dmrEmbeddedData);
                    }
                    break;
                    case TX_MODE_P25:
                    {
                        p25::lc::LC lc = p25::lc::LC();
                        lc.setLCO(p25::defines::LCO::GROUP);
                        lc.setDstId(dstId);
                        lc.setSrcId(srcId);

                        p25::data::LowSpeedData lsd = p25::data::LowSpeedData();

                        uint8_t controlByte = 0x00U;
                        bridge->m_network->writeP25TDU(lc, lsd, controlByte);
                    }
                    break;
                    }
                }

                bridge->m_srcIdOverride = 0;
                bridge->m_txStreamId = 0;

                bridge->m_udpSrcId = 0;
                bridge->m_udpDstId = 0;
                bridge->m_trafficFromUDP = false;
            }

            Thread::sleep(5U);
        }

        LogDebug(LOG_HOST, "[STOP] %s", threadName.c_str());
        delete th;
    }

    return nullptr;
}
