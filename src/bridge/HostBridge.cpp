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
#include "common/network/RTPHeader.h"
#include "common/network/udp/Socket.h"
#include "common/dmr/DMRDefines.h"
#include "common/dmr/data/EMB.h"
#include "common/p25/P25Defines.h"
#include "common/p25/data/LowSpeedData.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/p25/dfsi/LC.h"
#include "common/p25/lc/LC.h"
#include "common/p25/P25Utils.h"
#include "common/Clock.h"
#include "common/StopWatch.h"
#include "common/Thread.h"
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
//  Constants
// ---------------------------------------------------------------------------

#define IDLE_WARMUP_MS 5U

const int SAMPLE_RATE = 8000;
const int BITS_PER_SECOND = 16;
const int NUMBER_OF_BUFFERS = 32;

#define LOCAL_CALL "Local Traffic"
#define UDP_CALL "UDP Traffic"

#define TEK_DES "des"
#define TEK_AES "aes"
#define TEK_ARC4 "arc4"

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::mutex HostBridge::s_audioMutex;
std::mutex HostBridge::s_networkMutex;

bool HostBridge::s_running = false;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/* Helper callback, called when audio data is available. */

void audioCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount)
{
    HostBridge* bridge = (HostBridge*)device->pUserData;
    if (!HostBridge::s_running)
        return;

    ma_uint32 pcmBytes = frameCount * ma_get_bytes_per_frame(device->capture.format, device->capture.channels);

    // capture input audio
    if (frameCount > 0U) {
        std::lock_guard<std::mutex> lock(HostBridge::s_audioMutex);

        int smpIdx = 0;
        short samples[AUDIO_SAMPLES_LENGTH];
        const uint8_t* pcm = (const uint8_t*)input;
        for (uint32_t pcmIdx = 0; pcmIdx < pcmBytes; pcmIdx += 2) {
            samples[smpIdx] = (short)((pcm[pcmIdx + 1] << 8) + pcm[pcmIdx + 0]);
            smpIdx++;
        }

        bridge->m_inputAudio.addData(samples, AUDIO_SAMPLES_LENGTH);
    }

    // playback output audio
    if (bridge->m_outputAudio.dataSize() >= AUDIO_SAMPLES_LENGTH) {
        short samples[AUDIO_SAMPLES_LENGTH];
        bridge->m_outputAudio.get(samples, AUDIO_SAMPLES_LENGTH);

        uint8_t* pcm = (uint8_t*)output;
        int pcmIdx = 0;
        for (uint32_t smpIdx = 0; smpIdx < AUDIO_SAMPLES_LENGTH; smpIdx++) {
            pcm[pcmIdx + 0] = (uint8_t)(samples[smpIdx] & 0xFF);
            pcm[pcmIdx + 1] = (uint8_t)((samples[smpIdx] >> 8) & 0xFF);
            pcmIdx += 2;
        }

        // Assert RTS PTT when audio is being sent to output and record last output time
        bridge->assertRtsPtt();
        bridge->m_lastAudioOut = system_clock::hrc::now();
    }
}

/* Helper callback, called when MDC packets are detected. */

void mdcPacketDetected(int frameCount, mdc_u8_t op, mdc_u8_t arg, mdc_u16_t unitID,
    mdc_u8_t extra0, mdc_u8_t extra1, mdc_u8_t extra2, mdc_u8_t extra3, void* context)
{
    HostBridge* bridge = (HostBridge*)context;
    if (!HostBridge::s_running)
        return;

    if (op == OP_PTT_ID && bridge->m_overrideSrcIdFromMDC) {
        ::LogInfoEx(LOG_HOST, "Local Traffic, MDC Detect, unitId = $%04X", unitID);

        // HACK: nasty bullshit to convert MDC unitID to decimal
        char* pCharRes = new char[16]; // enough space for "0xFFFFFFFF"
        ::sprintf(pCharRes, "0x%X", unitID);

        uint32_t res = 0U;
        std::string s = std::string(pCharRes + 2U);
        if (s.find_first_not_of("0123456789") == std::string::npos) {
            res = (uint32_t)std::stoi(pCharRes + 2U);
        }
        else {
            res = (uint32_t)std::stoi(pCharRes, 0, 16);
        }

        delete[] pCharRes;
        bridge->m_srcIdOverride = res;
        ::LogInfoEx(LOG_HOST, "Local Traffic, MDC Detect, converted srcId = %u", bridge->m_srcIdOverride);
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
    m_srcId(P25DEF::WUID_FNE),
    m_srcIdOverride(0U),
    m_overrideSrcIdFromMDC(false),
    m_overrideSrcIdFromUDP(false),
    m_resetCallForSourceIdChange(false),
    m_dstId(1U),
    m_slot(1U),
    m_identity(),
    m_netId(P25DEF::WACN_STD_DEFAULT),
    m_sysId(P25DEF::SID_STD_DEFAULT),
    m_grantDemand(false),
    m_txMode(1U),
    m_rxAudioGain(1.0f),
    m_vocoderDecoderAudioGain(3.0f),
    m_vocoderDecoderAutoGain(false),
    m_txAudioGain(1.0f),
    m_vocoderEncoderAudioGain(3.0),
    m_tekAlgoId(P25DEF::ALGO_UNENCRYPT),
    m_tekKeyId(0U),
    m_requestedTek(false),
    m_p25Crypto(nullptr),
    m_localAudio(false),
    m_voxSampleLevel(30.0f),
    m_dropTimeMS(180U),
    m_localDropTime(1000U, 0U, 180U),
    m_udpDropTime(1000U, 0U, 180U),
    m_detectAnalogMDC1200(false),
    m_preambleLeaderTone(false),
    m_preambleTone(2175),
    m_preambleLength(200U),
    m_maContext(),
    m_maPlaybackDevices(nullptr),
    m_maCaptureDevices(nullptr),
    m_maDeviceConfig(),
    m_maDevice(),
    m_inputAudio(AUDIO_SAMPLES_LENGTH * NUMBER_OF_BUFFERS, "Input Audio Buffer"),
    m_outputAudio(AUDIO_SAMPLES_LENGTH * NUMBER_OF_BUFFERS, "Output Audio Buffer"),
    m_udpPackets(),
    m_decoder(nullptr),
    m_encoder(nullptr),
    m_mdcDecoder(nullptr),
    m_udpAudioSocket(nullptr),
    m_udpAudio(false),
    m_udpMetadata(false),
    m_udpSendPort(34001),
    m_udpSendAddress("127.0.0.1"),
    m_udpReceivePort(32001),
    m_udpReceiveAddress("127.0.0.1"),
    m_udpRTPFrames(false),
    m_udpIgnoreRTPTiming(false),
    m_udpRTPContinuousSeq(false),
    m_udpUseULaw(false),
    m_udpUsrp(false),
    m_udpFrameTiming(false),
    m_udpFrameCnt(0U),
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
    m_analogN(0U),
    m_rtsPttEnable(false),
    m_rtsPttPort(),
    m_rtsPttController(nullptr),
    m_rtsPttActive(false),
    m_lastAudioOut(),
    m_rtsPttHoldoffMs(250U),
    m_ctsCorEnable(false),
    m_ctsCorPort(),
    m_ctsCorController(nullptr),
    m_ctsCorActive(false),
    m_ctsCorInvert(false),
    m_ctsPadTimeout(1000U, 0U, 22U),
    m_ctsCorHoldoffMs(250U),
    m_audioDetect(false),
    m_trafficFromUDP(false),
    m_udpSrcId(0U),
    m_udpDstId(0U),
    m_callInProgress(false),
    m_ignoreCall(false),
    m_callAlgoId(P25DEF::ALGO_UNENCRYPT),
    m_rxStartTime(0U),
    m_rxStreamId(0U),
    m_txStreamId(0U),
    m_detectedSampleCnt(0U),
    m_networkWatchdog(1000U, 0U, 1500U),
    m_trace(false),
    m_debug(false),
    m_rtpInitialFrame(false),
    m_rtpSeqNo(0U),
    m_rtpTimestamp(INVALID_TS),
    m_udpNetPktSeq(0U),
    m_udpNetLastPktSeq(0U),
    m_usrpSeqNo(0U)
#if defined(_WIN32)
    ,
    m_decoderState(nullptr),
    m_dcMode(0U),
    m_encoderState(nullptr),
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

    m_p25Crypto = new p25::crypto::P25Crypto();

    // initialize RTS PTT timing
    m_lastAudioOut = system_clock::hrc::now();
    m_rtsPttHoldoffMs = 250U;
}

/* Finalizes a instance of the HostBridge class. */

HostBridge::~HostBridge()
{
    if (m_rtsPttController != nullptr) {
        m_rtsPttController->close();
        delete m_rtsPttController;
        m_rtsPttController = nullptr;
    }

    if (m_ctsCorController != nullptr) {
        m_ctsCorController->close();
        delete m_ctsCorController;
        m_ctsCorController = nullptr;
    }

    delete[] m_ambeBuffer;
    delete[] m_netLDU1;
    delete[] m_netLDU2;
    delete m_p25Crypto;
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
        "Copyright (c) 2017-2026 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\r\n" \
        "Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others\r\n" \
        HIGHLY_UNNECESSARY_DISCLAIMER_FOR_THE_MENTAL "\r\n" \
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

    // initialize RTS PTT control
    ret = initializeRtsPtt();
    if (!ret)
        return EXIT_FAILURE;

    // initialize CTS COR detection
    ret = initializeCtsCor();
    if (!ret)
        return EXIT_FAILURE;

    ma_result result;
    if (m_localAudio) {
        // initialize audio devices
        if (ma_context_init(g_backends, g_backendCnt, NULL, &m_maContext) != MA_SUCCESS) {
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
        LogInfo("    Audio Backend: %s", ma_get_backend_name(m_maContext.backend));
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
        m_maDeviceConfig.playback.shareMode = ma_share_mode_shared;

        m_maDeviceConfig.periodSizeInFrames = AUDIO_SAMPLES_LENGTH;
        m_maDeviceConfig.dataCallback = audioCallback;
        m_maDeviceConfig.pUserData = this;

        result = ma_device_init(&m_maContext, &m_maDeviceConfig, &m_maDevice);
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

    if (m_txMode != TX_MODE_ANALOG) {
        m_decoder->setGainAdjust(m_vocoderDecoderAudioGain);
        m_decoder->setAutoGain(m_vocoderDecoderAutoGain);
        m_encoder->setGainAdjust(m_vocoderEncoderAudioGain);
    }

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

    // set the In-Call Control function callback
    if (m_network != nullptr) {
        if (m_txMode == TX_MODE_DMR) {
            m_network->setDMRICCCallback([=](network::NET_ICC::ENUM command, uint32_t dstId, 
                uint8_t slotNo, uint32_t peerId, uint32_t ssrc, uint32_t streamId) { processInCallCtrl(command, dstId, slotNo); });
        }

        if (m_txMode == TX_MODE_P25) {
            m_network->setP25ICCCallback([=](network::NET_ICC::ENUM command, uint32_t dstId, 
                uint32_t peerId, uint32_t ssrc, uint32_t streamId) { processInCallCtrl(command, dstId, 0U); });
        }

        if (m_txMode == TX_MODE_ANALOG) {
            m_network->setAnalogICCCallback([=](network::NET_ICC::ENUM command, uint32_t dstId, 
                uint32_t peerId, uint32_t ssrc, uint32_t streamId) { processInCallCtrl(command, dstId, 0U); });
        }
    }

    /*
    ** Initialize Threads
    */

    if (!Thread::runAsThread(this, threadNetworkProcess))
        return EXIT_FAILURE;
    if (!Thread::runAsThread(this, threadCallWatchdog))
        return EXIT_FAILURE;

    if (m_localAudio) {
        if (!Thread::runAsThread(this, threadAudioProcess))
            return EXIT_FAILURE;
    }

    if (m_localAudio) {
        // start audio device
        result = ma_device_start(&m_maDevice);
        if (result != MA_SUCCESS) {
            ma_device_uninit(&m_maDevice);
            ma_context_uninit(&m_maContext);
            return EXIT_FAILURE;
        }
    }

    if (m_udpAudio) {
        if (!Thread::runAsThread(this, threadUDPAudioProcess))
            return EXIT_FAILURE;
    }

    ::LogInfoEx(LOG_HOST, "Bridge is up and running");

    s_running = true;

    StopWatch stopWatch;
    stopWatch.start();

    // main execution loop
    while (!g_killed) {
        uint32_t ms = stopWatch.elapsed();

        ms = stopWatch.elapsed();
        stopWatch.start();

        // ------------------------------------------------------
        //  -- Audio Device Checking                          --
        // ------------------------------------------------------

        if (m_localAudio) {
            ma_device_state state = ma_device_get_state(&m_maDevice);
            if (state != ma_device_state_started) {
                LogError(LOG_HOST, "audio device state invalid, state = %u", state);

                // restart audio device
                result = ma_device_start(&m_maDevice);
                if (result != MA_SUCCESS) {
                    ma_device_uninit(&m_maDevice);
                    ma_context_uninit(&m_maContext);
                    ::fatal("failed to reinitialize audio device! panic.");
                }
            }
        }

        // ------------------------------------------------------
        //  -- Network Clocking                               --
        // ------------------------------------------------------

        if (m_network != nullptr) {
            std::lock_guard<std::mutex> lock(HostBridge::s_networkMutex);
            m_network->clock(ms);

            if (m_callInProgress) {
                m_networkWatchdog.clock(ms);

                if (m_networkWatchdog.isRunning() && m_networkWatchdog.hasExpired()) {
                    if (m_rxStartTime > 0U) {
                        uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                        uint64_t diff = now - m_rxStartTime;

                        // send USRP end of transmission
                        if (m_udpUsrp) {
                            sendUsrpEot();
                        }

                        LogInfoEx(LOG_HOST, "Network watchdog, call end, dur = %us", diff / 1000U);
                    }

                    m_networkWatchdog.stop();

                    m_callInProgress = false;
                    m_ignoreCall = false;
                    m_callAlgoId = P25DEF::ALGO_UNENCRYPT;

                    m_rxDMRLC = dmr::lc::LC();
                    m_rxDMRPILC = dmr::lc::PrivacyLC();
                    m_rxP25LC = p25::lc::LC();
                    m_rxStartTime = 0U;
                    m_rxStreamId = 0U;

                    m_srcIdOverride = 0;
                    m_txStreamId = 0;

                    m_udpSrcId = 0;
                    m_udpDstId = 0;
                    m_trafficFromUDP = false;
                    m_udpFrameCnt = 0U;

                    // ensure PTT is dropped at call end
                    if (m_rtsPttEnable) {
                        deassertRtsPtt();
                    }

                    m_dmrSeqNo = 0U;
                    m_dmrN = 0U;
                    m_p25SeqNo = 0U;
                    m_p25N = 0U;
                    m_analogN = 0U;

                    if (!m_udpRTPContinuousSeq) {
                        m_rtpInitialFrame = false;
                        m_rtpSeqNo = 0U;
                    }
                    m_rtpTimestamp = INVALID_TS;

                    m_p25Crypto->clearMI();
                    m_p25Crypto->resetKeystream();

                    m_network->resetDMR(1U);
                    m_network->resetDMR(2U);

                    m_network->resetP25();

                    m_network->resetAnalog();
                }
            }
        }

        if (m_udpAudio && m_udpAudioSocket != nullptr)
            processUDPAudio();

        if (ms < 2U)
            Thread::sleep(1U);
    }

    s_running = false;

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

    if (m_localAudio) {
        ma_waveform_uninit(&m_maSineWaveform);
        ma_device_uninit(&m_maDevice);
        ma_context_uninit(&m_maContext);
    }

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

    //samples = new short[AUDIO_SAMPLES_LENGTH];

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

    std::unique_ptr<short[]> n0 = std::make_unique<short[]>(AUDIO_SAMPLES_LENGTH / 2);
    ambe_voice_dec(n0.get(), AUDIO_SAMPLES_LENGTH / 2, codewordBits.get(), NO_BIT_STEAL, m_dcMode, 0, m_decoderState);

    std::unique_ptr<short[]> n1 = std::make_unique<short[]>(AUDIO_SAMPLES_LENGTH / 2);
    ambe_voice_dec(n1.get(), AUDIO_SAMPLES_LENGTH / 2, codewordBits.get(), NO_BIT_STEAL, m_dcMode, 1, m_decoderState);

    // combine sample segments into contiguous samples
    for (int i = 0; i < AUDIO_SAMPLES_LENGTH / 2; i++)
        samples[i] = n0[i];
    for (int i = 0; i < AUDIO_SAMPLES_LENGTH / 2; i++)
        samples[i + (AUDIO_SAMPLES_LENGTH / 2)] = n1[i];

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

    if (sampleLength > AUDIO_SAMPLES_LENGTH) {
        ::LogError(LOG_HOST, "Samples length is > %u", AUDIO_SAMPLES_LENGTH);
        return;
    }
    if (sampleLength < AUDIO_SAMPLES_LENGTH) {
        ::LogError(LOG_HOST, "Samples length is < %u", AUDIO_SAMPLES_LENGTH);
        return;
    }

    std::unique_ptr<short[]> codewordBits = std::make_unique<short[]>(m_frameLengthInBits * 2);

    // split samples into 2 segments
    std::unique_ptr<short[]> n0 = std::make_unique<short[]>(AUDIO_SAMPLES_LENGTH / 2);
    for (int i = 0; i < AUDIO_SAMPLES_LENGTH / 2; i++)
        n0[i] = samples[i];

    ambe_voice_enc(codewordBits.get(), NO_BIT_STEAL, n0.get(), AUDIO_SAMPLES_LENGTH / 2, m_ecMode, 0, 8192, m_encoderState);

    std::unique_ptr<short[]> n1 = std::make_unique<short[]>(AUDIO_SAMPLES_LENGTH / 2);
    for (int i = 0; i < AUDIO_SAMPLES_LENGTH / 2; i++)
        n1[i] = samples[i + (AUDIO_SAMPLES_LENGTH / 2)];

    ambe_voice_enc(codewordBits.get(), NO_BIT_STEAL, n1.get(), AUDIO_SAMPLES_LENGTH / 2, m_ecMode, 1, 8192, m_encoderState);

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
    m_trace = systemConf["trace"].as<bool>(false);
    m_debug = systemConf["debug"].as<bool>(false);

    m_netId = (uint32_t)::strtoul(systemConf["netId"].as<std::string>("BB800").c_str(), NULL, 16);
    m_netId = p25::P25Utils::netId(m_netId);
    if (m_netId == 0xBEE00) {
        ::fatal("error 4\n");
    }

    m_sysId = (uint32_t)::strtoul(systemConf["sysId"].as<std::string>("001").c_str(), NULL, 16);
    m_sysId = p25::P25Utils::sysId(m_sysId);

    /*
    ** Site Data
    */
    int8_t lto = (int8_t)systemConf["localTimeOffset"].as<int32_t>(0);
    p25::SiteData siteData = p25::SiteData(m_netId, m_sysId, 1U, 1U, 0U, 0U, 1U, P25DEF::ServiceClass::VOICE, lto);
    siteData.setNetActive(true);

    p25::lc::LC::setSiteData(siteData);

    m_rxAudioGain = systemConf["rxAudioGain"].as<float>(1.0f);
    m_vocoderDecoderAudioGain = systemConf["vocoderDecoderAudioGain"].as<float>(3.0f);
    m_vocoderDecoderAutoGain = systemConf["vocoderDecoderAutoGain"].as<bool>(false);
    m_txAudioGain = systemConf["txAudioGain"].as<float>(1.0f);
    m_vocoderEncoderAudioGain = systemConf["vocoderEncoderAudioGain"].as<float>(3.0f);
    
    m_txMode = (uint8_t)systemConf["txMode"].as<uint32_t>(1U);
    if (m_txMode < TX_MODE_DMR)
        m_txMode = TX_MODE_DMR;
    if (m_txMode > TX_MODE_ANALOG)
        m_txMode = TX_MODE_ANALOG;

    m_voxSampleLevel = systemConf["voxSampleLevel"].as<float>(30.0f);
    m_dropTimeMS = (uint16_t)systemConf["dropTimeMs"].as<uint32_t>(180U);

    yaml::Node networkConf = m_conf["network"];
    m_udpAudio = networkConf["udpAudio"].as<bool>(false);

    switch (m_txMode) {
    case TX_MODE_DMR:
        break;
    case TX_MODE_P25:
        {
            if (m_udpAudio) {
                ::LogWarning(LOG_HOST, "When using UDP audio, the drop time is fixed to 360ms. (1 P25 audio superframe.)");
                m_dropTimeMS = 360U;
            }
        }
        break;
    case TX_MODE_ANALOG:
        break;
    }

    m_localDropTime = Timer(1000U, 0U, m_dropTimeMS);
    m_udpDropTime = Timer(1000U, 0U, m_dropTimeMS);

    m_detectAnalogMDC1200 = systemConf["detectAnalogMDC1200"].as<bool>(false);

    m_preambleLeaderTone = systemConf["preambleLeaderTone"].as<bool>(false);
    m_preambleTone = (uint16_t)systemConf["preambleTone"].as<uint32_t>(2175);
    m_preambleLength = (uint16_t)systemConf["preambleLength"].as<uint32_t>(200);

    m_grantDemand = systemConf["grantDemand"].as<bool>(false);

    m_localAudio = systemConf["localAudio"].as<bool>(true);

    m_udpAudio = systemConf["udpAudio"].as<bool>(false);
    m_udpMetadata = systemConf["udpMetadata"].as<bool>(false);
    m_udpSendPort = (uint16_t)systemConf["udpSendPort"].as<uint32_t>(34001);
    m_udpSendAddress = systemConf["udpSendAddress"].as<std::string>();
    m_udpReceivePort = (uint16_t)systemConf["udpReceivePort"].as<uint32_t>(34001);
    m_udpReceiveAddress = systemConf["udpReceiveAddress"].as<std::string>();
    m_udpUsrp = systemConf["udpUsrp"].as<bool>(false);
    m_udpFrameTiming = systemConf["udpFrameTiming"].as<bool>(false);

    if (m_udpUsrp) {
        m_udpMetadata = false;          // USRP disables metadata due to USRP always having metadata
        m_udpRTPFrames = false;         // USRP disables RTP
        m_udpUseULaw = false;           // USRP disables ULaw
    }

    m_udpRTPFrames = systemConf["udpRTPFrames"].as<bool>(false);
    m_udpIgnoreRTPTiming = systemConf["udpIgnoreRTPTiming"].as<bool>(false);
    m_udpRTPContinuousSeq = systemConf["udpRTPContinuousSeq"].as<bool>(false);
    m_udpUseULaw = systemConf["udpUseULaw"].as<bool>(false);
    if (m_udpRTPFrames) {
        m_udpUsrp = false;              // RTP disabled USRP
        m_udpFrameTiming = false;
    }
    else {
        if (m_udpUseULaw) {
            ::LogWarning(LOG_HOST, "uLaw encoding can only be used with RTP frames, disabling.");
            m_udpUseULaw = false;
        }
    }

    if (m_udpIgnoreRTPTiming)
        ::LogWarning(LOG_HOST, "Ignoring RTP timing, audio frames will be processed as they arrive.");
    if (m_udpRTPContinuousSeq)
        ::LogWarning(LOG_HOST, "Using continuous RTP sequence numbers, sequence numbers will not reset at the start of a new call.");

    yaml::Node tekConf = systemConf["tek"];
    bool tekEnable = tekConf["enable"].as<bool>(false);
    std::string tekAlgo = tekConf["tekAlgo"].as<std::string>();
    std::transform(tekAlgo.begin(), tekAlgo.end(), tekAlgo.begin(), ::tolower);
    m_tekKeyId = (uint32_t)::strtoul(tekConf["tekKeyId"].as<std::string>("0").c_str(), NULL, 16);
    if (tekEnable && m_tekKeyId > 0U) {
        if (tekAlgo == TEK_AES)
            m_tekAlgoId = P25DEF::ALGO_AES_256;
        else if (tekAlgo == TEK_ARC4)
            m_tekAlgoId = P25DEF::ALGO_ARC4;
        else if (tekAlgo == TEK_DES)
            m_tekAlgoId = P25DEF::ALGO_DES;
        else {
            ::LogError(LOG_HOST, "Invalid TEK algorithm specified, must be \"aes\" or \"adp\".");
            m_tekAlgoId = P25DEF::ALGO_UNENCRYPT;
            m_tekKeyId = 0U;
        }
    }

    if (!tekEnable)
        m_tekAlgoId = P25DEF::ALGO_UNENCRYPT;
    if (m_tekAlgoId == P25DEF::ALGO_UNENCRYPT)
        m_tekKeyId = 0U;

    // ensure encryption is currently disabled for DMR (its not supported)
    if (m_txMode == TX_MODE_DMR && m_tekAlgoId != P25DEF::ALGO_UNENCRYPT && m_tekKeyId > 0U) {
        ::LogError(LOG_HOST, "Encryption is not supported for DMR. Disabling.");
        m_tekAlgoId = P25DEF::ALGO_UNENCRYPT;
        m_tekKeyId = 0U;
    }

    // ensure encryption is currently disabled for analog (its not supported)
    if (m_txMode == TX_MODE_ANALOG && m_tekAlgoId != P25DEF::ALGO_UNENCRYPT && m_tekKeyId > 0U) {
        ::LogError(LOG_HOST, "Encryption is not supported for Analog. Disabling.");
        m_tekAlgoId = P25DEF::ALGO_UNENCRYPT;
        m_tekKeyId = 0U;
    }

    // RTS PTT Configuration
    m_rtsPttEnable = systemConf["rtsPttEnable"].as<bool>(false);
    m_rtsPttPort = systemConf["rtsPttPort"].as<std::string>("/dev/ttyUSB0");
    m_rtsPttHoldoffMs = (uint32_t)systemConf["rtsPttHoldoffMs"].as<uint32_t>(m_rtsPttHoldoffMs);

    // CTS COR Configuration
    m_ctsCorEnable = systemConf["ctsCorEnable"].as<bool>(false);
    m_ctsCorPort = systemConf["ctsCorPort"].as<std::string>("/dev/ttyUSB0");
    m_ctsCorInvert = systemConf["ctsCorInvert"].as<bool>(false);
    m_ctsCorHoldoffMs = (uint32_t)systemConf["ctsCorHoldoffMs"].as<uint32_t>(m_ctsCorHoldoffMs);

    std::string txModeStr = "DMR";
    if (m_txMode == TX_MODE_P25)
        txModeStr = "P25";
    if (m_txMode == TX_MODE_ANALOG)
        txModeStr = "Analog";

    LogInfo("General Parameters");
    LogInfo("    System Id: $%03X", m_sysId);
    LogInfo("    P25 Network Id: $%05X", m_netId);
    LogInfo("    Rx Audio Gain: %.1f", m_rxAudioGain);
    LogInfo("    Vocoder Decoder Audio Gain: %.1f", m_vocoderDecoderAudioGain);
    LogInfo("    Vocoder Decoder Auto Gain: %s", m_vocoderDecoderAutoGain ? "yes" : "no");
    LogInfo("    Tx Audio Gain: %.1f", m_txAudioGain);
    LogInfo("    Vocoder Encoder Audio Gain: %.1f", m_vocoderEncoderAudioGain);
    LogInfo("    Transmit Mode: %s", txModeStr.c_str());
    LogInfo("    VOX Sample Level: %.1f", m_voxSampleLevel);
    LogInfo("    Drop Time: %ums", m_dropTimeMS);
    LogInfo("    Detect Analog MDC1200: %s", m_detectAnalogMDC1200 ? "yes" : "no");
    LogInfo("    Generate Preamble Tone: %s", m_preambleLeaderTone ? "yes" : "no");
    LogInfo("    Preamble Tone: %uhz", m_preambleTone);
    LogInfo("    Preamble Tone Length: %ums", m_preambleLength);
    LogInfo("    Grant Demands: %s", m_grantDemand ? "yes" : "no");
    LogInfo("    Local Audio: %s", m_localAudio ? "yes" : "no");
    LogInfo("    PCM over UDP Audio: %s", m_udpAudio ? "yes" : "no");
    if (m_udpAudio) {
        LogInfo("    UDP Audio Metadata: %s", m_udpMetadata ? "yes" : "no");
        LogInfo("    UDP Audio Send Address: %s", m_udpSendAddress.c_str());
        LogInfo("    UDP Audio Send Port: %u", m_udpSendPort);
        LogInfo("    UDP Audio Receive Address: %s", m_udpReceiveAddress.c_str());
        LogInfo("    UDP Audio Receive Port: %u", m_udpReceivePort);
        LogInfo("    UDP Audio RTP Framed: %s", m_udpRTPFrames ? "yes" : "no");
        if (m_udpRTPFrames) {
            LogInfo("    UDP Audio Use uLaw Encoding: %s", m_udpUseULaw ? "yes" : "no");
            LogInfo("    UDP Audio Ignore RTP Timing: %s", m_udpIgnoreRTPTiming ? "yes" : "no");
            LogInfo("    UDP Audio Use Continuous RTP Sequence Numbers: %s", m_udpRTPContinuousSeq ? "yes" : "no");
        }
        LogInfo("    UDP Audio USRP: %s", m_udpUsrp ? "yes" : "no");
        LogInfo("    UDP Frame Timing: %s", m_udpFrameTiming ? "yes" : "no");
    }

    LogInfo("    Traffic Encrypted: %s", tekEnable ? "yes" : "no");
    if (tekEnable) {
        LogInfo("    TEK Algorithm: %s", tekAlgo.c_str());
        LogInfo("    TEK Key ID: $%04X", m_tekKeyId);
    }
    LogInfo("    RTS PTT Enable: %s", m_rtsPttEnable ? "yes" : "no");
    if (m_rtsPttEnable) {
        LogInfo("    RTS PTT Port: %s", m_rtsPttPort.c_str());
        LogInfo("    RTS PTT Hold-off: %ums", m_rtsPttHoldoffMs);
    }
    LogInfo("    CTS COR Enable: %s", m_ctsCorEnable ? "yes" : "no");
    if (m_ctsCorEnable) {
        LogInfo("    CTS COR Port: %s", m_ctsCorPort.c_str());
        LogInfo("    CTS COR Invert: %s (%s triggers)", m_ctsCorInvert ? "yes" : "no", m_ctsCorInvert ? "LOW" : "HIGH");
        LogInfo("    CTS COR Holdoff: %u ms", m_ctsCorHoldoffMs);
    }

    if (m_debug) {
        LogInfo("    Debug: yes");
    }

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
    bool allowDiagnosticTransfer = networkConf["allowDiagnosticTransfer"].as<bool>(false);
    bool packetDump = networkConf["packetDump"].as<bool>(false);
    bool debug = networkConf["debug"].as<bool>(false);

    m_srcId = (uint32_t)networkConf["sourceId"].as<uint32_t>(P25DEF::WUID_FNE);
    m_overrideSrcIdFromMDC = networkConf["overrideSourceIdFromMDC"].as<bool>(false);
    m_overrideSrcIdFromUDP = networkConf["overrideSourceIdFromUDP"].as<bool>(false);
    m_resetCallForSourceIdChange = networkConf["resetCallForSourceIdChange"].as<bool>(false);
    m_dstId = (uint32_t)networkConf["destinationId"].as<uint32_t>(1U);
    m_slot = (uint8_t)networkConf["slot"].as<uint32_t>(1U);

    // make sure our source ID is sane
    if (m_srcId == 0U) {
        ::LogError(LOG_HOST, "Bridge source ID cannot be set to 0.");
        return false;
    }

    // make sure our destination ID is sane
    if (m_dstId == 0U) {
        ::LogError(LOG_HOST, "Bridge destination ID cannot be set to 0.");
        return false;
    }

    // make sure we're range checked
    switch (m_txMode) {
    case TX_MODE_DMR:
        {
            if (m_dstId > 16777215) {
                ::LogError(LOG_HOST, "Bridge destination ID cannot be greater than 16777215.");
                return false;
            }
        }
        break;
    case TX_MODE_P25:
    case TX_MODE_ANALOG:
        {
            if (m_dstId > 65535) {
                ::LogError(LOG_HOST, "Bridge destination ID cannot be greater than 65535.");
                return false;
            }
        }
        break;
    }

    if (!m_udpMetadata && m_resetCallForSourceIdChange)
        m_resetCallForSourceIdChange = false; // only applies to UDP audio with metadata
    if (!m_overrideSrcIdFromUDP && m_resetCallForSourceIdChange)
        m_resetCallForSourceIdChange = false; // only applies to UDP audio when overriding source ID

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
    LogInfo("    Source ID: %u", m_srcId);
    LogInfo("    Destination ID: %u", m_dstId);
    LogInfo("    DMR Slot: %u", m_slot);
    LogInfo("    Override Source ID from MDC: %s", m_overrideSrcIdFromMDC ? "yes" : "no");
    LogInfo("    Override Source ID from UDP Audio: %s", m_overrideSrcIdFromUDP ? "yes" : "no");
    if (m_resetCallForSourceIdChange) {
        LogInfo("    Reset Call if Source ID Changes from UDP Audio: %s", m_resetCallForSourceIdChange ? "yes" : "no");
    }

    if (packetDump) {
        LogInfo("    Packet Dump: yes");
    }

    if (debug) {
        LogInfo("    Debug: yes");
    }

    bool dmr = false, p25 = false, analog = false;
    switch (m_txMode) {
    case TX_MODE_DMR:
        dmr = true;
        break;
    case TX_MODE_P25:
        p25 = true;
        break;
    case TX_MODE_ANALOG:
        analog = true;
        break;
    }

    // initialize networking
    m_network = new PeerNetwork(address, port, local, id, password, true, debug, dmr, p25, false, analog, true, true, true, allowDiagnosticTransfer, true, false);

    m_network->setPacketDump(packetDump);
    m_network->setMetadata(m_identity, 0U, 0U, 0.0F, 0.0F, 0, 0, 0, 0.0F, 0.0F, 0, "");
    m_network->setConventional(true);
    m_network->setKeyResponseCallback([=](p25::kmm::KeyItem ki, uint8_t algId, uint8_t keyLength) {
        processTEKResponse(&ki, algId, keyLength);
    });

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
        m_udpAudioSocket = new Socket(m_udpReceiveAddress, m_udpReceivePort);
        m_udpAudioSocket->open();

        /*
        ** bryanb: resize the system UDP socket buffer used for receiving audio frames to 2M, this should hold
        *   ~6300 raw audio frames before filling
        */
        if (!m_udpAudioSocket->recvBufSize(2097152U)) // 2M recv buffer
            LogWarning(LOG_HOST, "failed to resize UDP audio socket buffer size to 2M");
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
        return;
    }

    if (length > AUDIO_SAMPLES_LENGTH_BYTES * 2U) {
        LogWarning(LOG_NET, "UDP audio packet too large (%d bytes), dropping", length);
        return;
    }

    // is the recieved audio frame *at least* raw PCM length of 320 bytes?
    if (!m_udpUseULaw && length < AUDIO_SAMPLES_LENGTH_BYTES)
        return;

    // is the recieved audio frame *at least* uLaw length of 160 bytes?
    if (m_udpUseULaw && length < AUDIO_SAMPLES_LENGTH_BYTES / 2U)
        return;

    if (length > 0) {
        if (m_debug && m_trace)
            Utils::dump(1U, "HostBridge()::processUDPAudio(), Audio Receive Packet", buffer, length);

        uint32_t pcmLength = 0U;
        pcmLength = GET_UINT32(buffer, 0U);

        if (m_udpRTPFrames || m_udpUsrp)
            pcmLength = AUDIO_SAMPLES_LENGTH_BYTES;
        if (m_udpRTPFrames && m_udpUseULaw)
            pcmLength = AUDIO_SAMPLES_LENGTH_BYTES / 2U;

        DECLARE_UINT8_ARRAY(pcm, pcmLength + 1U);
        RTPHeader rtpHeader = RTPHeader();

        // are we setup for receiving RTP frames?
        if (m_udpRTPFrames) {
            rtpHeader.decode(buffer);

            if (rtpHeader.getPayloadType() != RTP_G711_PAYLOAD_TYPE) {
                LogError(LOG_HOST, "Invalid RTP payload type %u", rtpHeader.getPayloadType());
                return;
            }

            m_udpNetPktSeq = rtpHeader.getSequence();

            if (m_udpNetPktSeq == RTP_END_OF_CALL_SEQ) {
                // reset the received sequence back to 0
                m_udpNetLastPktSeq = 0U;
            }
            else {
                uint16_t lastRxSeq = m_udpNetLastPktSeq;

                if ((m_udpNetPktSeq >= m_udpNetLastPktSeq) || (m_udpNetPktSeq == 0U)) {
                    // if the sequence isn't 0, and is greater then the last received sequence + 1 frame
                    // assume a packet was lost
                    if ((m_udpNetPktSeq != 0U) && m_udpNetPktSeq > m_udpNetLastPktSeq + 1U) {
                        LogWarning(LOG_NET, "audio possible lost frames; got %u, expected %u", 
                            m_udpNetPktSeq, lastRxSeq);
                    }

                    m_udpNetPktSeq = m_udpNetPktSeq;
                }
                else {
                    if (m_udpNetPktSeq < m_udpNetPktSeq) {
                        LogWarning(LOG_NET, "audio out-of-order; got %u, expected %u", 
                            m_udpNetPktSeq, lastRxSeq);
                    }
                }
            }

            m_udpNetLastPktSeq = m_udpNetPktSeq;

            ::memcpy(pcm, buffer + RTP_HEADER_LENGTH_BYTES, pcmLength);
        } else {
            if (m_udpUsrp) {
                uint8_t* usrpHeader = new uint8_t[USRP_HEADER_LENGTH];
                ::memcpy(usrpHeader, buffer, USRP_HEADER_LENGTH);

                if (usrpHeader[15U] == 1U && length > USRP_HEADER_LENGTH) // PTT state true and ensure we did not just receive a USRP header
                    ::memcpy(pcm, buffer + USRP_HEADER_LENGTH, pcmLength);

                delete[] usrpHeader;
            } else {
                ::memcpy(pcm, buffer + 4U, pcmLength);
            }
        }

        // Utils::dump(1U, "HostBridge::processUDPAudio(), PCM RECV BYTE BUFFER", pcm, pcmLength);

        // create a new UDP packet request and queue it for processing
        NetPacketRequest* req = new NetPacketRequest();
        req->pcm = new uint8_t[pcmLength];
        ::memset(req->pcm, 0x00U, pcmLength);
        ::memcpy(req->pcm, pcm, pcmLength);

        req->rtpHeader = rtpHeader;
        req->pcmLength = pcmLength;

        if (m_udpMetadata) {
            if (m_udpRTPFrames) {
                req->srcId = GET_UINT32(buffer, RTP_HEADER_LENGTH_BYTES + pcmLength + 8U);
            } else {
                req->srcId = GET_UINT32(buffer, pcmLength + 8U);
            }
        } else {
            req->srcId = m_srcId;
        }

        req->dstId = m_dstId;

        m_udpPackets.push_back(req);
    }
}

/* Helper to write UDP audio to the UDP audio socket. */

void HostBridge::writeUDPAudio(uint32_t srcId, uint32_t dstId, uint8_t* pcm, uint32_t pcmLength)
{
    if (!m_udpAudio)
        return;

    uint32_t length = pcmLength + 4U;
    uint8_t* audioData = nullptr;

    // are we sending RTP audio frames?
    if (m_udpRTPFrames) {
        //LogDebug(LOG_HOST, "Generating RTP frame for UDP audio, srcId = %u, dstId = %u, pcmLength = %u, prevRtpSeq = %u", srcId, dstId, pcmLength, m_rtpSeqNo);

        uint8_t* rtpFrame = generateRTPHeaders(pcmLength, m_rtpSeqNo);
        if (rtpFrame != nullptr) {
            // are we sending metadata with the RTP frames?
            if (!m_udpMetadata) {
                length = RTP_HEADER_LENGTH_BYTES + pcmLength;
                audioData = new uint8_t[length];
                ::memcpy(audioData, rtpFrame, RTP_HEADER_LENGTH_BYTES);
                ::memcpy(audioData + RTP_HEADER_LENGTH_BYTES, pcm, pcmLength);
            } else {
                length = RTP_HEADER_LENGTH_BYTES + pcmLength + 8U; // RTP Header Length + trailing 4 bytes (srcId) + 4 bytes (dstId))
                audioData = new uint8_t[length];
                ::memcpy(audioData, rtpFrame, RTP_HEADER_LENGTH_BYTES);
                ::memcpy(audioData + RTP_HEADER_LENGTH_BYTES, pcm, pcmLength);

                // embed destination and source IDs
                SET_UINT32(dstId, audioData, RTP_HEADER_LENGTH_BYTES + pcmLength + 4U);
                SET_UINT32(srcId, audioData, RTP_HEADER_LENGTH_BYTES + pcmLength + 8U);
            }
        }

        m_rtpSeqNo++;
        if (m_rtpSeqNo == RTP_END_OF_CALL_SEQ)
            m_rtpSeqNo = 0U;
    }
    else {
        // are we sending USRP formatted audio frames?
        if (m_udpUsrp) {
            uint8_t* usrpHeader = new uint8_t[USRP_HEADER_LENGTH];

            length = USRP_HEADER_LENGTH + pcmLength;
            audioData = new uint8_t[length]; // PCM + 32 bytes (USRP Header)

            m_usrpSeqNo++;
            usrpHeader[15U] = 1; // set PTT state to true
            SET_UINT32(m_usrpSeqNo, usrpHeader, 4U);

            ::memcpy(usrpHeader, "USRP", 4);
            ::memcpy(audioData, usrpHeader, USRP_HEADER_LENGTH); // copy USRP header into the UDP payload
            ::memcpy(audioData + USRP_HEADER_LENGTH, pcm, pcmLength);
        } else {
            // untimed raw audio frames
            length = pcmLength + 12U;
            audioData = new uint8_t[pcmLength + 12U]; // PCM + (4 bytes (PCM length) + 4 bytes (srcId) + 4 bytes (dstId))
            SET_UINT32(pcmLength, audioData, 0U);
            ::memcpy(audioData + 4U, pcm, AUDIO_SAMPLES_LENGTH * 2U);

            // embed destination and source IDs
            SET_UINT32(dstId, audioData, (pcmLength + 4U));
            SET_UINT32(srcId, audioData, (pcmLength + 8U));
        }
    }
 
    if (m_debug && m_trace)
        Utils::dump(1U, "HostBridge()::writeUDPAudio(), Audio Send Packet", audioData, length);

    sockaddr_storage addr;
    uint32_t addrLen;

    if (udp::Socket::lookup(m_udpSendAddress, m_udpSendPort, addr, addrLen) == 0) {
        m_udpAudioSocket->write(audioData, length, addr, addrLen);
    }

    delete[] audioData;
}

/* Helper to process an In-Call Control message. */

void HostBridge::processInCallCtrl(network::NET_ICC::ENUM command, uint32_t dstId, uint8_t slotNo)
{
    std::string trafficType = LOCAL_CALL;
    if (m_trafficFromUDP) {
        trafficType = UDP_CALL;
    }

    switch (command) {
    case network::NET_ICC::REJECT_TRAFFIC:
        {
            /*
            ** bryanb: this is a naive implementation, it will likely cause start/stop, start/stop type cycling
            */
            if (dstId == m_dstId) {
                LogWarning(LOG_HOST, "network requested in-call traffic reject, dstId = %u", dstId);

                m_ignoreCall = true;
                callEnd(m_srcId, m_dstId);
            }
        }
        break;

    default:
        break;
    }
}

/* Helper to send USRP end of transmission */

void HostBridge::sendUsrpEot()
{
    sockaddr_storage addr;
    uint32_t addrLen;

    uint8_t usrpHeader[USRP_HEADER_LENGTH];

    m_usrpSeqNo = 0U;
    ::memcpy(usrpHeader, "USRP", 4);

    if (udp::Socket::lookup(m_udpSendAddress, m_udpSendPort, addr, addrLen) == 0) {
        m_udpAudioSocket->write(usrpHeader, USRP_HEADER_LENGTH, addr, addrLen);
    }
}

/* Helper to generate the single-tone preamble tone. */

void HostBridge::generatePreambleTone()
{
    if (!m_localAudio)
        return;

    std::lock_guard<std::mutex> lock(s_audioMutex);

    uint64_t frameCount = AnalogAudio::toSamples(SAMPLE_RATE, 1, m_preambleLength);
    if (frameCount > m_outputAudio.freeSpace()) {
        ::LogError(LOG_HOST, "failed to generate preamble tone");
        return;
    }

    ma_waveform_set_frequency(&m_maSineWaveform, m_preambleTone);

    ma_uint32 pcmBytes = frameCount * ma_get_bytes_per_frame(m_maDevice.capture.format, m_maDevice.capture.channels);
    DECLARE_UINT8_ARRAY(sine, pcmBytes);

    ma_waveform_read_pcm_frames(&m_maSineWaveform, sine, frameCount, NULL);

    int smpIdx = 0;
    DECLARE_SHORT_ARRAY(sineSamples, frameCount);
    const uint8_t* pcm = (const uint8_t*)sine;
    for (uint32_t pcmIdx = 0; pcmIdx < pcmBytes; pcmIdx += 2) {
        sineSamples[smpIdx] = (short)((pcm[pcmIdx + 1] << 8) + pcm[pcmIdx + 0]);
        smpIdx++;
    }

    m_outputAudio.addData(sineSamples, frameCount);
}

/* Helper to generate outgoing RTP headers. */

uint8_t* HostBridge::generateRTPHeaders(uint8_t msgLen, uint16_t& rtpSeq)
{
    uint32_t timestamp = m_rtpTimestamp;
    if (timestamp != INVALID_TS) {
        timestamp += (RTP_GENERIC_CLOCK_RATE / 50);
        if (m_debug)
            LogDebugEx(LOG_NET, "HostBridge::generateRTPHeaders()", "RTP, previous TS = %u, TS = %u, rtpSeq = %u", m_rtpTimestamp, timestamp, rtpSeq);
        m_rtpTimestamp = timestamp;
    }

    // generate RTP header
    RTPHeader header = RTPHeader();

    header.setPayloadType(RTP_G711_PAYLOAD_TYPE);
    header.setTimestamp(timestamp);
    header.setSequence(rtpSeq);
    header.setSSRC(m_network->getPeerId());

    // set the marker for the start of a stream
    if (rtpSeq == 0U && !m_rtpInitialFrame) {
        m_rtpInitialFrame = true;
        header.setMarker(true);
    }

    uint8_t* buffer = new uint8_t[RTP_HEADER_LENGTH_BYTES + msgLen];
    ::memset(buffer, 0x00U, RTP_HEADER_LENGTH_BYTES + msgLen);

    if (timestamp == INVALID_TS) {
        if (m_debug)
            LogDebugEx(LOG_NET, "HostBridge::generateRTPHeaders()", "RTP, initial TS = %u, rtpSeq = %u", header.getTimestamp(), rtpSeq);

        timestamp = (uint32_t)system_clock::ntp::now();
        header.setTimestamp(timestamp);

        m_rtpTimestamp = header.getTimestamp();
    }

    header.encode(buffer);

    return buffer;
}

/* Helper to end a local or UDP call. */

void HostBridge::callEnd(uint32_t srcId, uint32_t dstId)
{
    std::string trafficType = LOCAL_CALL;
    if (m_trafficFromUDP) {
        srcId = m_udpSrcId;
        trafficType = UDP_CALL;
    }

    if (srcId == 0U && !m_audioDetect && (!m_localDropTime.isRunning() || !m_udpDropTime.isRunning())) {
        LogError(LOG_HOST, "%s, call end, ignoring invalid call end, srcId = %u, dstId = %u", trafficType.c_str(), srcId, dstId);
        return;
    }

    m_audioDetect = false;
    m_localDropTime.stop();
    m_udpDropTime.stop();

    if (!m_callInProgress) {
        switch (m_txMode) {
        case TX_MODE_DMR:
            {
                padSilenceAudio(srcId, dstId);

                DMRDEF::DataType::E dataType = DMRDEF::DataType::VOICE_SYNC;
                if (m_dmrN == 0)
                    dataType = DMRDEF::DataType::VOICE_SYNC;
                else {
                    dataType = DMRDEF::DataType::VOICE;
                }

                dmr::data::NetData data = dmr::data::NetData();
                data.setSlotNo(m_slot);
                data.setDataType(dataType);
                data.setSrcId(srcId);
                data.setDstId(dstId);
                data.setFLCO(DMRDEF::FLCO::GROUP);
                data.setN(m_dmrN);
                data.setSeqNo(m_dmrSeqNo);
                data.setBER(0U);
                data.setRSSI(0U);

                LogInfoEx(LOG_HOST, DMR_DT_TERMINATOR_WITH_LC ", slot = %u, dstId = %u", m_slot, dstId);

                m_network->writeDMRTerminator(data, &m_dmrSeqNo, &m_dmrN, m_dmrEmbeddedData);
            }
            break;
        case TX_MODE_P25:
            {
                // insert 2 silence LDUs at call end for clean transition
                padSilenceAudio(srcId, dstId);
                padSilenceAudio(srcId, dstId);

                p25::lc::LC lc = p25::lc::LC();
                lc.setLCO(P25DEF::LCO::GROUP);
                lc.setDstId(dstId);
                lc.setSrcId(srcId);

                p25::data::LowSpeedData lsd = p25::data::LowSpeedData();

                LogInfoEx(LOG_HOST, P25_TDU_STR);

                uint8_t controlByte = 0x00U;
                m_network->writeP25TDU(lc, lsd, controlByte);
            }
            break;
        case TX_MODE_ANALOG:
            {
                LogInfoEx(LOG_HOST, ANO_TERMINATOR);

                uint8_t controlByte = 0x00U;
                
                data::NetData analogData;
                analogData.setSeqNo(m_analogN);
                analogData.setSrcId(srcId);
                analogData.setDstId(dstId);
                analogData.setFrameType(AudioFrameType::TERMINATOR);

                uint8_t pcm[AUDIO_SAMPLES_LENGTH * 2U];
                ::memset(pcm, 0x00U, AUDIO_SAMPLES_LENGTH * 2U);
                analogData.setAudio(pcm);

                m_network->writeAnalog(analogData, true);
            }
            break;
        }
    }

    LogInfoEx(LOG_HOST, "%s, call end, srcId = %u, dstId = %u", trafficType.c_str(), srcId, dstId);

    m_srcIdOverride = 0;
    m_txStreamId = 0;

    m_udpSrcId = 0;
    m_udpDstId = 0;
    m_trafficFromUDP = false;
    m_udpFrameCnt = 0U;

    // ensure PTT is dropped at call end
    if (m_rtsPttEnable) {
        deassertRtsPtt();
    }

    m_dmrSeqNo = 0U;
    m_dmrN = 0U;
    m_p25SeqNo = 0U;
    m_p25N = 0U;
    m_analogN = 0U;

    if (!m_udpRTPContinuousSeq) {
        m_rtpInitialFrame = false;
        m_rtpSeqNo = 0U;
    }
    m_rtpTimestamp = INVALID_TS;

    m_p25Crypto->clearMI();
    m_p25Crypto->resetKeystream();

    m_network->resetDMR(m_slot);
    m_network->resetP25();
    m_network->resetAnalog();
}

/* Helper to process a FNE KMM TEK response. */

void HostBridge::processTEKResponse(p25::kmm::KeyItem* ki, uint8_t algId, uint8_t keyLength)
{
    if (ki == nullptr)
        return;

    if (algId == m_tekAlgoId && ki->kId() == m_tekKeyId) {
        LogInfoEx(LOG_HOST, "TEK loaded, algId = $%02X, kId = $%04X, sln = $%04X", algId, ki->kId(), ki->sln());
        UInt8Array tek = std::make_unique<uint8_t[]>(keyLength);
        ki->getKey(tek.get());

        m_p25Crypto->setTEKAlgoId(algId);
        m_p25Crypto->setTEKKeyId(ki->kId());
        m_p25Crypto->setKey(tek.get(), keyLength);
    }
    else {
        m_p25Crypto->setTEKAlgoId(P25DEF::ALGO_UNENCRYPT);
        m_p25Crypto->setTEKKeyId(0U);
        m_p25Crypto->clearKey();
    }
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

        std::string threadName("bridge:local-audio");
        HostBridge* bridge = static_cast<HostBridge*>(th->obj);
        if (bridge == nullptr) {
            g_killed = true;
            LogError(LOG_HOST, "[FAIL] %s", threadName.c_str());
        }

        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogInfoEx(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        while (!g_killed) {
            if (!HostBridge::s_running) {
                LogError(LOG_HOST, "HostBridge::threadAudioProcess(), thread not running");
                Thread::sleep(1000U);
                continue;
            }

            // scope is intentional
            {
                std::lock_guard<std::mutex> lock(s_audioMutex);

                // When COR is active, we need to send frames continuously when audio data is available
                // The audio callback should be continuously feeding data, so we should always have data available
                bool hasAudioData = bridge->m_inputAudio.dataSize() >= AUDIO_SAMPLES_LENGTH;
                bool shouldProcess = false;

                if (!bridge->m_ctsCorEnable)
                    shouldProcess = true;
                else {
                    // When COR is active: process whenever we have data (which should be continuous)
                    // When COR is not active: only process when VOX detects audio (normal mode)
                    if (bridge->m_ctsCorActive && bridge->m_audioDetect) {
                        // COR is active: process whenever we have audio data (continuous transmission)
                        shouldProcess = hasAudioData;

                    }
                    else if (!bridge->m_ctsCorActive && bridge->m_audioDetect) {
                        // Normal VOX mode: process when we have audio data
                        shouldProcess = hasAudioData;
                    }
                }

                if (shouldProcess && hasAudioData) {
                    short samples[AUDIO_SAMPLES_LENGTH];
                    bridge->m_inputAudio.get(samples, AUDIO_SAMPLES_LENGTH);

                    // process MDC, if necessary
                    if (bridge->m_overrideSrcIdFromMDC)
                        mdc_decoder_process_samples(bridge->m_mdcDecoder, samples, AUDIO_SAMPLES_LENGTH);

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
                    for (int i = 0; i < (int)AUDIO_SAMPLES_LENGTH; i++) {
                        float sampleValue = fabs((float)samples[i]);
                        maxSample = fmax(maxSample, sampleValue);
                    }
                    maxSample = maxSample / 1000;

                    if (g_dumpSampleLevels && bridge->m_detectedSampleCnt > 50U) {
                        bridge->m_detectedSampleCnt = 0U;
                        ::LogInfoEx(LOG_HOST, "Detected Sample Level: %.2f", maxSample * 1000);
                    }

                    if (g_dumpSampleLevels) {
                        bridge->m_detectedSampleCnt++;
                    }

                    // handle Rx triggered by internal VOX (unless COR is active, which takes precedence)
                    if (!bridge->m_ctsCorActive) {
                        if (maxSample > sampleLevel) {
                            bridge->m_audioDetect = true;
                            if (bridge->m_txStreamId == 0U) {
                                bridge->m_txStreamId = 1U;
                                LogInfoEx(LOG_HOST, "%s, call start, srcId = %u, dstId = %u", trafficType.c_str(), srcId, dstId);

                                if (bridge->m_grantDemand) {
                                    switch (bridge->m_txMode) {
                                        case TX_MODE_P25:
                                        {
                                            p25::lc::LC lc = p25::lc::LC();
                                            lc.setLCO(P25DEF::LCO::GROUP);
                                            lc.setDstId(dstId);
                                            lc.setSrcId(srcId);

                                            p25::data::LowSpeedData lsd = p25::data::LowSpeedData();

                                            uint8_t controlByte = network::NET_CTRL_GRANT_DEMAND;
                                            if (bridge->m_tekAlgoId != P25DEF::ALGO_UNENCRYPT)
                                                controlByte |= network::NET_CTRL_GRANT_ENCRYPT;
                                            bridge->m_network->writeP25TDU(lc, lsd, controlByte);
                                        }
                                        break;
                                    }
                                }
                            }

                            bridge->m_localDropTime.stop();
                        } else {
                            // if we've exceeded the audio drop timeout, then really drop the audio
                            if (bridge->m_localDropTime.isRunning() && bridge->m_localDropTime.hasExpired()) {
                                if (bridge->m_audioDetect) {
                                    bridge->callEnd(srcId, dstId);
                                }
                            }

                            if (!bridge->m_localDropTime.isRunning())
                                bridge->m_localDropTime.start();
                        }
                    }

                    // Send audio frames: either from actual audio input OR silence frames when COR is active
                    if (bridge->m_audioDetect && !bridge->m_callInProgress) {
                        // If COR is active, always send the actual audio from the buffer (even if quiet)
                        // If COR is not active, only send when VOX detects audio
                        if (bridge->m_ctsCorActive) {
                            // COR is active: always encode actual audio from buffer
                            // The buffer should always have data from audio callback, even if it's quiet
                            ma_uint32 pcmBytes = AUDIO_SAMPLES_LENGTH * ma_get_bytes_per_frame(bridge->m_maDevice.capture.format, bridge->m_maDevice.capture.channels);
                            DECLARE_UINT8_ARRAY(pcm, pcmBytes);

                            // Always encode the actual samples, even if they're quiet
                            // This ensures real audio is passed through, not just silence
                            int pcmIdx = 0;
                            for (uint32_t smpIdx = 0; smpIdx < AUDIO_SAMPLES_LENGTH; smpIdx++) {
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
                            case TX_MODE_ANALOG:
                                bridge->encodeAnalogAudioFrame(pcm);
                                break;
                            }
                        } else {
                            // COR is not active: normal VOX-based audio processing
                            if (maxSample > sampleLevel) {
                                ma_uint32 pcmBytes = AUDIO_SAMPLES_LENGTH * ma_get_bytes_per_frame(bridge->m_maDevice.capture.format, bridge->m_maDevice.capture.channels);
                                DECLARE_UINT8_ARRAY(pcm, pcmBytes);

                                int pcmIdx = 0;
                                for (uint32_t smpIdx = 0; smpIdx < AUDIO_SAMPLES_LENGTH; smpIdx++) {
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
                                case TX_MODE_ANALOG:
                                    bridge->encodeAnalogAudioFrame(pcm);
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            // When COR is active, we need to process frames continuously
            // The audio callback should be feeding data, but if buffer is empty and COR is active,
            // we'll send silence frames. Keep minimal sleep to ensure responsive processing.
            Thread::sleep(1U);
        }

        LogInfoEx(LOG_HOST, "[STOP] %s", threadName.c_str());
        delete th;
    }

    return nullptr;
}

/* Entry point to CTS COR monitor thread. */

void* HostBridge::threadCtsCorMonitor(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
#if defined(_WIN32)
        ::CloseHandle(th->thread);
#else
        ::pthread_detach(th->thread);
#endif // defined(_WIN32)

        std::string threadName("bridge:cts-cor-monitor");
        HostBridge* bridge = static_cast<HostBridge*>(th->obj);
        if (bridge == nullptr) {
            g_killed = true;
            LogError(LOG_HOST, "[FAIL] %s", threadName.c_str());
        }

        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogInfoEx(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        // Initialize lastCts to current state to avoid false trigger on startup
        bool lastCts = false;
        if (bridge->m_ctsCorEnable && bridge->m_ctsCorController != nullptr) {
            bool ctsRawInit = bridge->m_ctsCorController->isCtsAsserted();
            lastCts = bridge->m_ctsCorInvert ? !ctsRawInit : ctsRawInit;
            bridge->m_ctsCorActive = lastCts;
            LogInfoEx(LOG_HOST, "CTS COR monitor initialized: initial state = %s (raw: %s)", 
                lastCts ? "TRIGGER" : "IDLE", ctsRawInit ? "HIGH" : "LOW");
        }
        uint32_t pollCount = 0U;

        while (!g_killed) {
            if (!HostBridge::s_running) {
                LogError(LOG_HOST, "HostBridge::threadCtsCorMonitor(), thread not running");
                Thread::sleep(1000U);
                continue;
            }

            if (!bridge->m_ctsCorEnable) {
                LogDebug(LOG_HOST, "CTS COR is disabled, waiting...");
                Thread::sleep(1000U);
                continue;
            }

            if (bridge->m_ctsCorController == nullptr) {
                LogError(LOG_HOST, "CTS COR Controller is null!");
                Thread::sleep(1000U);
                continue;
            }

            bool ctsRaw = bridge->m_ctsCorController->isCtsAsserted();
            // Apply inversion: if invert is true, LOW triggers (so we invert the raw signal)
            bool cts = bridge->m_ctsCorInvert ? !ctsRaw : ctsRaw;
            pollCount++;

            if (cts != lastCts) {
                LogInfoEx(LOG_HOST, "CTS COR state changed: %s -> %s (raw: %s)", 
                    lastCts ? "TRIGGER" : "IDLE", cts ? "TRIGGER" : "IDLE", ctsRaw ? "HIGH" : "LOW");
                lastCts = cts;
                bridge->m_ctsCorActive = cts;
                if (cts) {
                    // Rising edge: force call start, immediately send silence frame, and start padding timer
                    uint32_t srcId = bridge->m_srcId;
                    uint32_t dstId = bridge->m_dstId;
                    if (!bridge->m_audioDetect) {
                        bridge->m_audioDetect = true;
                        if (bridge->m_txStreamId == 0U) {
                            bridge->m_txStreamId = 1U;
                            LogInfoEx(LOG_HOST, "%s, call start (CTS COR), srcId = %u, dstId = %u", LOCAL_CALL, srcId, dstId);
                            if (bridge->m_grantDemand) {
                                switch (bridge->m_txMode) {
                                case TX_MODE_P25:
                                {
                                    p25::lc::LC lc = p25::lc::LC();
                                    lc.setLCO(P25DEF::LCO::GROUP);
                                    lc.setDstId(dstId);
                                    lc.setSrcId(srcId);

                                    p25::data::LowSpeedData lsd = p25::data::LowSpeedData();

                                    uint8_t controlByte = network::NET_CTRL_GRANT_DEMAND;
                                    if (bridge->m_tekAlgoId != P25DEF::ALGO_UNENCRYPT)
                                        controlByte |= network::NET_CTRL_GRANT_ENCRYPT;
                                    bridge->m_network->writeP25TDU(lc, lsd, controlByte);
                                }
                                break;
                                }
                            }
                        }
                    }
                    // Stop drop timer while COR is activem audio is processing
                    bridge->m_localDropTime.stop();
                    // Don't start padding timer
                }
                else {
                    // Falling edge: start hold-off timer before allowing call to end
                    bridge->m_ctsPadTimeout.stop();
                    // Start drop timer with hold-off delay
                    bridge->m_localDropTime = Timer(1000U, 0U, bridge->m_ctsCorHoldoffMs);
                    bridge->m_localDropTime.start();
                }
            }

            Thread::sleep(5U);
        }

        LogInfoEx(LOG_HOST, "[STOP] %s", threadName.c_str());
        delete th;
    }

    return nullptr;
}

/* Entry point to UDP audio processing thread. */

void* HostBridge::threadUDPAudioProcess(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
#if defined(_WIN32)
        ::CloseHandle(th->thread);
#else
        ::pthread_detach(th->thread);
#endif // defined(_WIN32)

        std::string threadName("bridge:udp-audio");
        HostBridge* bridge = static_cast<HostBridge*>(th->obj);
        if (bridge == nullptr) {
            g_killed = true;
            LogError(LOG_HOST, "[FAIL] %s", threadName.c_str());
        }

        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogInfoEx(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        StopWatch stopWatch;
        stopWatch.start();

        ulong64_t lastFrameTime = 0U;

        while (!g_killed) {
            if (!HostBridge::s_running) {
                LogError(LOG_HOST, "HostBridge::threadUDPAudioProcess(), thread not running");
                Thread::sleep(1000U);
                continue;
            }

            uint32_t ms = stopWatch.elapsed();
            stopWatch.start();

            if (bridge->m_udpPackets.empty())
                Thread::sleep(1U);
            else {
                NetPacketRequest* req = bridge->m_udpPackets.front();
                if (req == nullptr) {
                    bridge->m_udpPackets.pop_front();
                    continue;
                }

                bool shouldProcess = true;
                uint16_t pktSeq = 0U;

                // are we using RTP frames?
                if (bridge->m_udpRTPFrames) {
                    pktSeq = req->rtpHeader.getSequence();

                    // are we timing based on RTP timestamps?
                    if (!bridge->m_udpIgnoreRTPTiming) {
                        // RTP timing takes precedence - use RTP timestamps exclusively
                        uint32_t rtpTimestamp = req->rtpHeader.getTimestamp();
                        if (lastFrameTime == 0U) {
                            lastFrameTime = rtpTimestamp;
                        }
                        else {
                            // RTP timestamps increment by samples per frame
                            uint32_t expectedTimestamp = (uint32_t)lastFrameTime + (RTP_GENERIC_CLOCK_RATE / 50);
                            if (rtpTimestamp < expectedTimestamp) {
                                // frame is stale (already processed a more recent frame) - discard it
                                // rather than spinning on it forever at the head of the queue
                                LogWarning(LOG_NET, "RTP frame stale/out-of-order, discarding; rtpTs = %u, expected >= %u, pktSeq = %u",
                                    rtpTimestamp, expectedTimestamp, pktSeq);
                                bridge->m_udpPackets.pop_front();
                                if (req->pcm != nullptr)
                                    delete[] req->pcm;
                                delete req;
                                req = nullptr;
                                shouldProcess = false;
                            } else {
                                // frame is ready to process - update RTP timestamp marker
                                lastFrameTime = rtpTimestamp;
                            }
                        }
                    }
                } else if (bridge->m_udpFrameTiming) {
                    // raw PCM with frame timing - pace at 10ms intervals using system time
                    if (lastFrameTime != 0U) {
                        // get current time right before the timing check for accuracy
                        uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                        
                        // check if enough time has passed since last frame (10ms for P25 LDUs)
                        if (now < lastFrameTime + 10U) {
                            // too early, don't process yet - keep frame in queue
                            shouldProcess = false;
                        }
                    }
                    
                    // lastFrameTime is updated AFTER we pop and commit to processing
                }

                // if timing checks say we shouldn't process yet, skip this iteration
                if (!shouldProcess) {
                    Thread::sleep(1U);
                    continue;
                }

                uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

                if (bridge->m_debug)
                    LogDebugEx(LOG_HOST, "HostBridge::threadUDPAudioProcess()", "now = %llu, lastUdpFrameTime = %llu, audioDetect = %u, callInProgress = %u, p25N = %u, dmrN = %u, analogN = %u, frameCnt = %u, pktSeq = %u",
                        now, lastFrameTime, bridge->m_audioDetect, bridge->m_callInProgress, bridge->m_p25N, bridge->m_dmrN, bridge->m_analogN, bridge->m_udpFrameCnt, pktSeq);

                // validate frame before popping
                if (req->pcm == nullptr || req->pcmLength == 0U) {
                    LogWarning(LOG_HOST, "UDP audio frame has null or zero-length PCM data, discarding (pcm=%p, len=%u)", 
                        req->pcm, req->pcmLength);
                    bridge->m_udpPackets.pop_front();
                    if (req->pcm != nullptr)
                        delete[] req->pcm;
                    delete req;
                    continue;
                }

                uint32_t framePcmLength = req->pcmLength;
                uint32_t frameSrcId = req->srcId;
                uint32_t frameDstId = req->dstId;
                
                uint32_t copyLength = (framePcmLength <= AUDIO_SAMPLES_LENGTH_BYTES) ? framePcmLength : AUDIO_SAMPLES_LENGTH_BYTES;
                uint8_t* framePcmData = new uint8_t[copyLength];
                ::memcpy(framePcmData, req->pcm, copyLength);

                // now pop the frame from the queue and free it
                bridge->m_udpPackets.pop_front();
                delete[] req->pcm;
                delete req;
                req = nullptr; // prevent use-after-free
                
                // update frame timing marker after committing to process this frame
                // (only for raw PCM timing mode - RTP timing updates within the RTP block above)
                if (!bridge->m_udpRTPFrames && bridge->m_udpFrameTiming) {
                    lastFrameTime = now;
                }
                
                bridge->m_udpDropTime.start();

                // handle source ID management
                bool forceCallStart = false;
                uint32_t txStreamId = bridge->m_txStreamId;

                // determine source ID to use for this UDP audio frame
                if (bridge->m_udpMetadata) {
                    // use source ID from UDP metadata if available and override is enabled
                    if (bridge->m_overrideSrcIdFromUDP) {
                        if (frameSrcId != 0U && bridge->m_udpSrcId != 0U) {
                            // if the UDP source ID now doesn't match the current call ID, reset call states
                            if (bridge->m_resetCallForSourceIdChange && (frameSrcId != bridge->m_udpSrcId)) {
                                LogInfoEx(LOG_HOST, "%s, call switch over, old srcId = %u, new srcId = %u", UDP_CALL, bridge->m_udpSrcId, frameSrcId);
                                bridge->callEnd(bridge->m_udpSrcId, bridge->m_dstId);

                                if (bridge->m_udpDropTime.isRunning())
                                    bridge->m_udpDropTime.start();

                                forceCallStart = true;
                            }

                            bridge->m_udpSrcId = frameSrcId;
                        }
                        else {
                            if (bridge->m_udpSrcId == 0U) {
                                bridge->m_udpSrcId = frameSrcId;
                            }

                            if (bridge->m_udpSrcId == 0U) {
                                bridge->m_udpSrcId = bridge->m_srcId;
                            }
                        }
                    }
                    else {
                        bridge->m_udpSrcId = bridge->m_srcId;
                    }
                }
                else {
                    bridge->m_udpSrcId = bridge->m_srcId;
                }

                bridge->m_udpDstId = bridge->m_dstId;

                // force start a call if one isn't already in progress
                if (!bridge->m_callInProgress || forceCallStart) {
                    if (bridge->m_txStreamId == 0U) {
                        bridge->m_txStreamId = 1U;
                        if (forceCallStart)
                            bridge->m_txStreamId = txStreamId;

                        LogInfoEx(LOG_HOST, "%s, call start, srcId = %u, dstId = %u", UDP_CALL, bridge->m_udpSrcId, bridge->m_udpDstId);
                        if (bridge->m_grantDemand) {
                            switch (bridge->m_txMode) {
                            case TX_MODE_P25:
                            {
                                p25::lc::LC lc = p25::lc::LC();
                                lc.setLCO(P25DEF::LCO::GROUP);
                                lc.setDstId(bridge->m_udpDstId);
                                lc.setSrcId(bridge->m_udpSrcId);

                                p25::data::LowSpeedData lsd = p25::data::LowSpeedData();

                                uint8_t controlByte = network::NET_CTRL_GRANT_DEMAND;
                                if (bridge->m_tekAlgoId != P25DEF::ALGO_UNENCRYPT)
                                    controlByte |= network::NET_CTRL_GRANT_ENCRYPT;
                                controlByte |= network::NET_CTRL_SWITCH_OVER;
                                bridge->m_network->writeP25TDU(lc, lsd, controlByte);
                                
                                // insert 2 silence LDUs at call start for clean transition
                                bridge->padSilenceAudio(bridge->m_udpSrcId, bridge->m_udpDstId);
                                bridge->padSilenceAudio(bridge->m_udpSrcId, bridge->m_udpDstId);
                            }
                            break;
                            }
                        }
                    }

                    bridge->m_udpDropTime.stop();
                    if (!bridge->m_udpDropTime.isRunning())
                        bridge->m_udpDropTime.start();
                }

                // process the received audio frame
                std::lock_guard<std::mutex> lock(s_audioMutex);
                uint8_t pcm[AUDIO_SAMPLES_LENGTH_BYTES];
                ::memset(pcm, 0x00U, AUDIO_SAMPLES_LENGTH_BYTES);

                // copy the frame data we saved earlier
                ::memcpy(pcm, framePcmData, copyLength);
                
                // free the temporary copy
                delete[] framePcmData;

                if (bridge->m_udpUseULaw) {
                    if (bridge->m_trace)
                        Utils::dump(1U, "HostBridge()::threadUDPAudioProcess(), uLaw Audio", pcm, AUDIO_SAMPLES_LENGTH * 2U);

                    int smpIdx = 0;
                    short samples[AUDIO_SAMPLES_LENGTH];
                    for (uint32_t pcmIdx = 0; pcmIdx < AUDIO_SAMPLES_LENGTH; pcmIdx++) {
                        samples[smpIdx] = AnalogAudio::decodeMuLaw(pcm[pcmIdx]);
                        smpIdx++;
                    }

                    int pcmIdx = 0;
                    for (uint32_t smpIdx = 0; smpIdx < AUDIO_SAMPLES_LENGTH; smpIdx++) {
                        pcm[pcmIdx + 0] = (uint8_t)(samples[smpIdx] & 0xFF);
                        pcm[pcmIdx + 1] = (uint8_t)((samples[smpIdx] >> 8) & 0xFF);
                        pcmIdx += 2;
                    }
                }

                bridge->m_trafficFromUDP = true;

                // check if PCM buffer is all zeros (silence detection for diagnostics)
                bool isSilence = true;
                for (uint32_t i = 0; i < copyLength && isSilence; i++) {
                    if (pcm[i] != 0x00U) {
                        isSilence = false;
                    }
                }
                
                if (isSilence && bridge->m_debug) {
                    LogWarning(LOG_HOST, "UDP audio frame contains all zeros (silence), pcmLength=%u", copyLength);
                }

                // encode and transmit UDP audio if audio detection is active
                // Note: We encode even if a network call is in progress, since UDP audio takes priority
                bridge->m_udpDropTime.start();

                switch (bridge->m_txMode) {
                case TX_MODE_DMR:
                    bridge->encodeDMRAudioFrame(pcm, bridge->m_udpSrcId);
                    break;
                case TX_MODE_P25:
                    bridge->encodeP25AudioFrame(pcm, bridge->m_udpSrcId);
                    break;
                case TX_MODE_ANALOG:
                    bridge->encodeAnalogAudioFrame(pcm, bridge->m_udpSrcId);
                    break;
                }

                bridge->m_udpFrameCnt++;

                if (!bridge->m_callInProgress)
                    Thread::sleep(1U);
            }
        }

        LogInfoEx(LOG_HOST, "[STOP] %s", threadName.c_str());
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
            LogError(LOG_HOST, "[FAIL] %s", threadName.c_str());
        }

        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogInfoEx(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        while (!g_killed) {
            if (!HostBridge::s_running) {
                LogError(LOG_HOST, "HostBridge::threadNetworkProcess(), thread not running");
                Thread::sleep(1000U);
                continue;
            }

            if (bridge->m_network->getStatus() == NET_STAT_RUNNING) {
                if (bridge->m_tekAlgoId != P25DEF::ALGO_UNENCRYPT && bridge->m_tekKeyId > 0U) {
                    if (bridge->m_p25Crypto->getTEKLength() == 0U && !bridge->m_requestedTek) {
                        bridge->m_requestedTek = true;
                        LogInfoEx(LOG_HOST, "Bridge encryption enabled, requesting TEK from network.");
                        bridge->m_network->writeKeyReq(bridge->m_tekKeyId, bridge->m_tekAlgoId);
                    }
                }
            }

            uint32_t length = 0U;
            bool netReadRet = false;
            // is the bridge in DMR mode?
            if (bridge->m_txMode == TX_MODE_DMR) {
                UInt8Array dmrBuffer = nullptr;

                // scope is intentional to limit lock duration
                {
                    std::lock_guard<std::mutex> lock(HostBridge::s_networkMutex);
                    dmrBuffer = bridge->m_network->readDMR(netReadRet, length);
                }

                if (netReadRet && dmrBuffer != nullptr) {
                    bridge->processDMRNetwork(dmrBuffer.get(), length);
                }
            }

            // is the bridge in P25 mode?
            if (bridge->m_txMode == TX_MODE_P25) {
                UInt8Array p25Buffer = nullptr;

                // scope is intentional to limit lock duration
                {
                    std::lock_guard<std::mutex> lock(HostBridge::s_networkMutex);
                    p25Buffer = bridge->m_network->readP25(netReadRet, length);
                }

                if (netReadRet && p25Buffer != nullptr) {
                    bridge->processP25Network(p25Buffer.get(), length);
                }
            }

            // is the bridge in analog mode?
            if (bridge->m_txMode == TX_MODE_ANALOG) {
                UInt8Array analogBuffer = nullptr;

                // scope is intentional to limit lock duration
                {
                    std::lock_guard<std::mutex> lock(HostBridge::s_networkMutex);
                    analogBuffer = bridge->m_network->readAnalog(netReadRet, length);
                }

                if (netReadRet && analogBuffer != nullptr) {
                    bridge->processAnalogNetwork(analogBuffer.get(), length);
                }
            }

            Thread::sleep(1U);
        }

        LogInfoEx(LOG_HOST, "[STOP] %s", threadName.c_str());
        delete th;
    }

    return nullptr;
}

/* Helper to reset IMBE buffer with null frames. */

void HostBridge::resetWithNullAudio(uint8_t* data, bool encrypted)
{
    if (data == nullptr)
        return;

    // clear buffer for next sequence
    ::memset(data, 0x00U, 9U * 25U);

    // fill with null
    if (!encrypted) {
        ::memcpy(data + 10U, P25DEF::NULL_IMBE, 11U);
        ::memcpy(data + 26U, P25DEF::NULL_IMBE, 11U);
        ::memcpy(data + 55U, P25DEF::NULL_IMBE, 11U);

        ::memcpy(data + 80U, P25DEF::NULL_IMBE, 11U);
        ::memcpy(data + 105U, P25DEF::NULL_IMBE, 11U);
        ::memcpy(data + 130U, P25DEF::NULL_IMBE, 11U);

        ::memcpy(data + 155U, P25DEF::NULL_IMBE, 11U);
        ::memcpy(data + 180U, P25DEF::NULL_IMBE, 11U);
        ::memcpy(data + 204U, P25DEF::NULL_IMBE, 11U);
    }
    else {
        ::memcpy(data + 10U, P25DEF::ENCRYPTED_NULL_IMBE, 11U);
        ::memcpy(data + 26U, P25DEF::ENCRYPTED_NULL_IMBE, 11U);
        ::memcpy(data + 55U, P25DEF::ENCRYPTED_NULL_IMBE, 11U);

        ::memcpy(data + 80U, P25DEF::ENCRYPTED_NULL_IMBE, 11U);
        ::memcpy(data + 105U, P25DEF::ENCRYPTED_NULL_IMBE, 11U);
        ::memcpy(data + 130U, P25DEF::ENCRYPTED_NULL_IMBE, 11U);

        ::memcpy(data + 155U, P25DEF::ENCRYPTED_NULL_IMBE, 11U);
        ::memcpy(data + 180U, P25DEF::ENCRYPTED_NULL_IMBE, 11U);
        ::memcpy(data + 204U, P25DEF::ENCRYPTED_NULL_IMBE, 11U);
    }
}

/* */

void HostBridge::padSilenceAudio(uint32_t srcId, uint32_t dstId)
{
    switch (m_txMode) {
    case TX_MODE_DMR:
        {
            using namespace dmr;
            using namespace dmr::defines;
            using namespace dmr::data;

            m_dmrN = (uint8_t)(m_dmrSeqNo % 6);

            // send DMR voice
            uint8_t data[DMR_FRAME_LENGTH_BYTES];

            m_ambeCount = 0U;

            ::memcpy(m_ambeBuffer + (m_ambeCount * 9U), NULL_AMBE, RAW_AMBE_LENGTH_BYTES);
            m_ambeCount++;
            ::memcpy(m_ambeBuffer + (m_ambeCount * 9U), NULL_AMBE, RAW_AMBE_LENGTH_BYTES);
            m_ambeCount++;
            ::memcpy(m_ambeBuffer + (m_ambeCount * 9U), NULL_AMBE, RAW_AMBE_LENGTH_BYTES);
            m_ambeCount++;

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

            LogInfoEx(LOG_HOST, DMR_DT_VOICE ", audio (silence), srcId = %u, dstId = %u, slot = %u, seqNo = %u", srcId, dstId, m_slot, m_dmrN);

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
        }
        break;
    case TX_MODE_P25:
        {
            using namespace p25;
            using namespace p25::defines;
            using namespace p25::data;

            uint8_t n = m_p25N;

            // fill the LDU buffers appropriately
            if (m_p25N > 0U) {
                // LDU1
                if (m_p25N >= 0U && m_p25N < 9U) {
                    for (uint8_t n = m_p25N; n < 9U; n++) {
                        switch (n) {
                        case 0:
                            ::memcpy(m_netLDU1 + 10U, P25DEF::NULL_IMBE, RAW_IMBE_LENGTH_BYTES);
                            break;
                        case 1:
                            ::memcpy(m_netLDU1 + 26U, P25DEF::NULL_IMBE, RAW_IMBE_LENGTH_BYTES);
                            break;
                        case 2:
                            ::memcpy(m_netLDU1 + 55U, P25DEF::NULL_IMBE, RAW_IMBE_LENGTH_BYTES);
                            break;
                        case 3:
                            ::memcpy(m_netLDU1 + 80U, P25DEF::NULL_IMBE, RAW_IMBE_LENGTH_BYTES);
                            break;
                        case 4:
                            ::memcpy(m_netLDU1 + 105U, P25DEF::NULL_IMBE, RAW_IMBE_LENGTH_BYTES);
                            break;
                        case 5:
                            ::memcpy(m_netLDU1 + 130U, P25DEF::NULL_IMBE, RAW_IMBE_LENGTH_BYTES);
                            break;
                        case 6:
                            ::memcpy(m_netLDU1 + 155U, P25DEF::NULL_IMBE, RAW_IMBE_LENGTH_BYTES);
                            break;
                        case 7:
                            ::memcpy(m_netLDU1 + 180U, P25DEF::NULL_IMBE, RAW_IMBE_LENGTH_BYTES);
                            break;
                        case 8:
                            ::memcpy(m_netLDU1 + 204U, P25DEF::NULL_IMBE, RAW_IMBE_LENGTH_BYTES);
                            break;
                        }
                    }

                    m_p25N = 8U;
                }

                // LDU2
                if (m_p25N >= 9U && m_p25N < 17U) {
                    for (uint8_t n = m_p25N; n < 18U; n++) {
                        switch (n) {
                        case 9:
                            ::memcpy(m_netLDU2 + 10U, P25DEF::NULL_IMBE, RAW_IMBE_LENGTH_BYTES);
                            break;
                        case 10:
                            ::memcpy(m_netLDU2 + 26U, P25DEF::NULL_IMBE, RAW_IMBE_LENGTH_BYTES);
                            break;
                        case 11:
                            ::memcpy(m_netLDU2 + 55U, P25DEF::NULL_IMBE, RAW_IMBE_LENGTH_BYTES);
                            break;
                        case 12:
                            ::memcpy(m_netLDU2 + 80U, P25DEF::NULL_IMBE, RAW_IMBE_LENGTH_BYTES);
                            break;
                        case 13:
                            ::memcpy(m_netLDU2 + 105U, P25DEF::NULL_IMBE, RAW_IMBE_LENGTH_BYTES);
                            break;
                        case 14:
                            ::memcpy(m_netLDU2 + 130U, P25DEF::NULL_IMBE, RAW_IMBE_LENGTH_BYTES);
                            break;
                        case 15:
                            ::memcpy(m_netLDU2 + 155U, P25DEF::NULL_IMBE, RAW_IMBE_LENGTH_BYTES);
                            break;
                        case 16:
                            ::memcpy(m_netLDU2 + 180U, P25DEF::NULL_IMBE, RAW_IMBE_LENGTH_BYTES);
                            break;
                        case 17:
                            ::memcpy(m_netLDU2 + 204U, P25DEF::NULL_IMBE, RAW_IMBE_LENGTH_BYTES);
                            break;
                        }
                    }

                    m_p25N = 17U;
                }
            }
            else {
                // LDU1
                if (m_p25N >= 0U && m_p25N < 9U) {
                    resetWithNullAudio(m_netLDU1, false);
                    m_p25N = 8U;
                }

                // LDU2
                if (m_p25N >= 9U && m_p25N < 17U) {
                    resetWithNullAudio(m_netLDU2, false);
                    m_p25N = 17U;
                }
            }

            switch (m_p25N) {
                // LDU1
            case 0:
                resetWithNullAudio(m_netLDU1, false);
                break;

                // LDU2
            case 1:
                resetWithNullAudio(m_netLDU2, false);
                break;
            }

            lc::LC lc = lc::LC();
            lc.setLCO(LCO::GROUP);
            lc.setGroup(true);
            lc.setPriority(4U);
            lc.setDstId(dstId);
            lc.setSrcId(srcId);

            lc.setAlgId(ALGO_UNENCRYPT);
            lc.setKId(0);

            LowSpeedData lsd = LowSpeedData();

            // send P25 LDU1
            if (m_p25N == 8U) {
                LogInfoEx(LOG_HOST, P25_LDU1_STR " audio (silence padded %u), srcId = %u, dstId = %u", 8U - n, srcId, dstId);
                m_network->writeP25LDU1(lc, lsd, m_netLDU1, FrameType::DATA_UNIT);
                m_p25N = 9U;
                break;
            }

            // send P25 LDU2
            if (m_p25N == 17U) {
                LogInfoEx(LOG_HOST, P25_LDU2_STR " audio (silence padded %u), algo = $%02X, kid = $%04X", 17U - n, ALGO_UNENCRYPT, 0U);
                m_network->writeP25LDU2(lc, lsd, m_netLDU2);
                m_p25N = 0U;
                break;
            }
        }
        break;
    }
}

/* Entry point to call watchdog handler thread. */

void* HostBridge::threadCallWatchdog(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
#if defined(_WIN32)
        ::CloseHandle(th->thread);
#else
        ::pthread_detach(th->thread);
#endif // defined(_WIN32)

        std::string threadName("bridge:call-watchdog");
        HostBridge* bridge = static_cast<HostBridge*>(th->obj);
        if (bridge == nullptr) {
            g_killed = true;
            LogError(LOG_HOST, "[FAIL] %s", threadName.c_str());
        }

        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogInfoEx(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        StopWatch stopWatch;
        stopWatch.start();

        while (!g_killed) {
            if (!HostBridge::s_running) {
                LogError(LOG_HOST, "HostBridge::threadCallWatchdog(), thread not running");
                Thread::sleep(1000U);
                continue;
            }

            uint32_t ms = stopWatch.elapsed();
            stopWatch.start();

            if (!bridge->m_trafficFromUDP) {
                if (bridge->m_localDropTime.isRunning())
                    bridge->m_localDropTime.clock(ms);
            }
            else {
                if (bridge->m_udpDropTime.isRunning())
                    bridge->m_udpDropTime.clock(ms);
            }

            // Debounce RTS PTT clear using hold-off after last audio output
            if (bridge->m_rtsPttEnable && bridge->m_rtsPttActive) {
                uint64_t sinceLastOut = system_clock::hrc::diffNow(bridge->m_lastAudioOut);
                if (sinceLastOut >= bridge->m_rtsPttHoldoffMs) {
                    bridge->deassertRtsPtt();
                }
            }

            // When CTS COR is active, the audio processing thread handles frame transmission
            // We don't use the watchdog thread for padding to avoid conflicts with actual audio frames

            std::string trafficType = LOCAL_CALL;
            if (bridge->m_trafficFromUDP)
                trafficType = UDP_CALL;

            uint32_t srcId = bridge->m_srcId;
            if (bridge->m_srcIdOverride != 0 && bridge->m_overrideSrcIdFromMDC)
                srcId = bridge->m_srcIdOverride;

            uint32_t dstId = bridge->m_dstId;

            ulong64_t temp = (bridge->m_dropTimeMS) * 1000U;
            uint32_t dropTimeout = (uint32_t)((temp / 1000ULL + 1ULL) * 2U);

            if (bridge->m_trafficFromUDP) {
                srcId = bridge->m_udpSrcId;
                dstId = bridge->m_udpDstId;

                if (bridge->m_udpDropTime.isRunning() && bridge->m_udpDropTime.hasExpired()) {
                    bridge->callEnd(srcId, dstId);
                }
            }
            else {
                // Don't end call due to drop timeout if COR is still active
                if (!bridge->m_ctsCorActive) {
                    // if we've exceeded the drop timeout, then really drop the audio
                    if (bridge->m_localDropTime.isRunning() && (bridge->m_localDropTime.getTimer() >= dropTimeout)) {
                        LogInfoEx(LOG_HOST, "%s, terminating stuck call", trafficType.c_str());
                        bridge->callEnd(srcId, dstId);
                    }
                }
            }

            Thread::sleep(5U);
        }

        LogInfoEx(LOG_HOST, "[STOP] %s", threadName.c_str());
        delete th;
    }

    return nullptr;
}

/* Helper to initialize RTS PTT control. */

bool HostBridge::initializeRtsPtt()
{
    if (!m_rtsPttEnable)
        return true;

    if (m_rtsPttPort.empty()) {
        ::LogError(LOG_HOST, "RTS PTT port is not specified");
        return false;
    }

    m_rtsPttController = new RtsPttController(m_rtsPttPort);
    if (!m_rtsPttController->open()) {
        ::LogError(LOG_HOST, "Failed to open RTS PTT port %s", m_rtsPttPort.c_str());
        delete m_rtsPttController;
        m_rtsPttController = nullptr;
        return false;
    }

    ::LogInfo(LOG_HOST, "RTS PTT Controller initialized on %s", m_rtsPttPort.c_str());
    return true;
}

/* Helper to initialize CTS COR detection. */

bool HostBridge::initializeCtsCor()
{
    if (!m_ctsCorEnable)
        return true;

    if (m_ctsCorPort.empty()) {
        ::LogError(LOG_HOST, "CTS COR port is not specified");
        return false;
    }

    m_ctsCorController = new CtsCorController(m_ctsCorPort);
    
    // If RTS PTT and CTS COR are on the same port, reuse the file descriptor
    // to avoid opening the port twice (which would affect RTS)
    int reuseFd = -1;
    if (m_rtsPttEnable && m_rtsPttController != nullptr && 
        m_rtsPttPort == m_ctsCorPort && m_rtsPttController->getFd() >= 0) {
        reuseFd = m_rtsPttController->getFd();
        ::LogInfo(LOG_HOST, "CTS COR reusing RTS PTT file descriptor for %s (same port)", m_ctsCorPort.c_str());
    }
    
    if (!m_ctsCorController->open(reuseFd)) {
        ::LogError(LOG_HOST, "Failed to open CTS COR port %s", m_ctsCorPort.c_str());
        delete m_ctsCorController;
        m_ctsCorController = nullptr;
        return false;
    }

    // Start monitor thread
    thread_t* th = new thread_t();
    th->obj = this;
    if (!Thread::runAsThread(this, &HostBridge::threadCtsCorMonitor, th)) {
        ::LogError(LOG_HOST, "Failed to start CTS COR monitor thread");
        return false;
    }

    ::LogInfo(LOG_HOST, "CTS COR initialized on %s", m_ctsCorPort.c_str());
    
    // Test read CTS state to verify it's working
    bool ctsRaw = m_ctsCorController->isCtsAsserted();
    bool ctsEffective = m_ctsCorInvert ? !ctsRaw : ctsRaw;
    ::LogInfo(LOG_HOST, "CTS COR initial state: raw=%s, effective=%s (%s)", 
        ctsRaw ? "HIGH" : "LOW", ctsEffective ? "TRIGGER" : "IDLE", 
        m_ctsCorInvert ? "inverted" : "normal");
    
    return true;
}

/* Helper to assert RTS PTT (start transmission). */

void HostBridge::assertRtsPtt()
{
    if (!m_rtsPttEnable || m_rtsPttController == nullptr || m_rtsPttActive)
        return;

    if (m_rtsPttController->setPTT()) {
        m_rtsPttActive = true;
        ::LogDebug(LOG_HOST, "RTS PTT asserted");
    }
}

/* Helper to deassert RTS PTT (stop transmission). */

void HostBridge::deassertRtsPtt()
{
    if (!m_rtsPttEnable || m_rtsPttController == nullptr || !m_rtsPttActive)
        return;

    if (m_rtsPttController->clearPTT()) {
        m_rtsPttActive = false;
        ::LogDebug(LOG_HOST, "RTS PTT deasserted");
    }
}
