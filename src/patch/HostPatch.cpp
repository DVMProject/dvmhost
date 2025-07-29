// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - TG Patch
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
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
#include "common/p25/Sync.h"
#include "common/p25/P25Utils.h"
#include "common/network/RTPHeader.h"
#include "common/network/udp/Socket.h"
#include "common/Clock.h"
#include "common/StopWatch.h"
#include "common/Thread.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "patch/ActivityLog.h"
#include "HostPatch.h"
#include "PatchMain.h"

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

#define TEK_AES "aes"
#define TEK_ARC4 "arc4"

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::mutex HostPatch::m_networkMutex;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the HostPatch class. */

HostPatch::HostPatch(const std::string& confFile) :
    m_confFile(confFile),
    m_conf(),
    m_network(nullptr),
    m_srcTGId(0U),
    m_srcSlot(1U),
    m_dstTGId(0U),
    m_dstSlot(1U),
    m_twoWayPatch(false),
    m_mmdvmP25Reflector(false),
    m_mmdvmP25Net(nullptr),
    m_netState(RS_NET_IDLE),
    m_netLC(),
    m_gotNetLDU1(false),
    m_netLDU1(nullptr),
    m_gotNetLDU2(false),
    m_netLDU2(nullptr),
    m_identity(),
    m_digiMode(1U),
    m_dmrEmbeddedData(),
    m_grantDemand(false),
    m_callInProgress(false),
    m_callAlgoId(p25::defines::ALGO_UNENCRYPT),
    m_rxStartTime(0U),
    m_rxStreamId(0U),
    m_tekSrcAlgoId(p25::defines::ALGO_UNENCRYPT),
    m_tekSrcKeyId(0U),
    m_tekDstAlgoId(p25::defines::ALGO_UNENCRYPT),
    m_tekDstKeyId(0U),
    m_requestedSrcTek(false),
    m_requestedDstTek(false),
    m_p25SrcCrypto(nullptr),
    m_p25DstCrypto(nullptr),
    m_running(false),
    m_trace(false),
    m_debug(false)
{
    m_netLDU1 = new uint8_t[9U * 25U];
    m_netLDU2 = new uint8_t[9U * 25U];

    ::memset(m_netLDU1, 0x00U, 9U * 25U);
    resetWithNullAudio(m_netLDU1, false);
    ::memset(m_netLDU2, 0x00U, 9U * 25U);
    resetWithNullAudio(m_netLDU2, false);

    m_p25SrcCrypto = new p25::crypto::P25Crypto();
    m_p25DstCrypto = new p25::crypto::P25Crypto();
}

/* Finalizes a instance of the HostPatch class. */

HostPatch::~HostPatch()
{
    delete[] m_netLDU1;
    delete[] m_netLDU2;
    delete m_p25SrcCrypto;
    delete m_p25DstCrypto;
}

/* Executes the main FNE processing loop. */

int HostPatch::run()
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
        "Copyright (c) 2025 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\r\n" \
        "Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others\r\n" \
        ">> Talkgroup Patch\r\n");

    // read base parameters from configuration
    ret = readParams();
    if (!ret)
        return EXIT_FAILURE;

    yaml::Node systemConf = m_conf["system"];

    // initialize peer networking
    ret = createNetwork();
    if (!ret)
        return EXIT_FAILURE;

    // initialize MMDVM P25 reflector networking
    if (m_mmdvmP25Reflector) {
        ret = createMMDVMP25Network();
        if (!ret)
            return EXIT_FAILURE;
    }

    /*
    ** Initialize Threads
    */

    if (!Thread::runAsThread(this, threadNetworkProcess))
        return EXIT_FAILURE;

    if (m_mmdvmP25Reflector) {
        if (!Thread::runAsThread(this, threadMMDVMProcess))
            return EXIT_FAILURE;
    }

    ::LogInfoEx(LOG_HOST, "Patch is up and running");

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

        if (m_network != nullptr) {
            std::lock_guard<std::mutex> lock(HostPatch::m_networkMutex);
            m_network->clock(ms);
        }

        if (m_mmdvmP25Reflector) {
            std::lock_guard<std::mutex> lock(HostPatch::m_networkMutex);
            m_mmdvmP25Net->clock(ms);
        }

        if (ms < 2U)
            Thread::sleep(1U);
    }

    ::LogSetNetwork(nullptr);
    if (m_network != nullptr) {
        m_network->close();
        delete m_network;
    }

    return EXIT_SUCCESS;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Reads basic configuration parameters from the YAML configuration file. */

bool HostPatch::readParams()
{
    yaml::Node systemConf = m_conf["system"];

    m_identity = systemConf["identity"].as<std::string>();
    
    m_digiMode = (uint8_t)systemConf["digiMode"].as<uint32_t>(1U);
    if (m_digiMode < TX_MODE_DMR)
        m_digiMode = TX_MODE_DMR;
    if (m_digiMode > TX_MODE_P25)
        m_digiMode = TX_MODE_P25;

    m_grantDemand = systemConf["grantDemand"].as<bool>(false);

    m_mmdvmP25Reflector = systemConf["mmdvmP25Reflector"].as<bool>(false);

    if (m_mmdvmP25Reflector && m_digiMode != TX_MODE_P25) {
        LogError(LOG_HOST, "Patch does not currently support MMDVM patching in any mode other then P25.");
        return false;
    }

    m_trace = systemConf["trace"].as<bool>(false);
    m_debug = systemConf["debug"].as<bool>(false);

    LogInfo("General Parameters");
    LogInfo("    Digital Mode: %s", m_digiMode == TX_MODE_DMR ? "DMR" : "P25");
    LogInfo("    Grant Demands: %s", m_grantDemand ? "yes" : "no");
    LogInfo("    MMDVM P25 Reflector Patch: %s", m_mmdvmP25Reflector ? "yes" : "no");

    if (m_debug) {
        LogInfo("    Debug: yes");
    }

    return true;
}

/* Initializes network connectivity. */

bool HostPatch::createNetwork()
{
    yaml::Node networkConf = m_conf["network"];

    std::string address = networkConf["address"].as<std::string>();
    uint16_t port = (uint16_t)networkConf["port"].as<uint32_t>(TRAFFIC_DEFAULT_PORT);
    uint16_t local = (uint16_t)networkConf["local"].as<uint32_t>(0U);
    uint32_t id = networkConf["id"].as<uint32_t>(1000U);
    std::string password = networkConf["password"].as<std::string>();
    bool allowDiagnosticTransfer = networkConf["allowDiagnosticTransfer"].as<bool>(false);
    bool debug = networkConf["debug"].as<bool>(false);

    m_srcTGId = (uint32_t)networkConf["sourceTGID"].as<uint32_t>(1U);
    m_srcSlot = (uint8_t)networkConf["sourceSlot"].as<uint32_t>(1U);
    m_dstTGId = (uint32_t)networkConf["destinationTGID"].as<uint32_t>(1U);
    m_dstSlot = (uint8_t)networkConf["destinationSlot"].as<uint32_t>(1U);

    // source TEK parameters
    yaml::Node srcTekConf = networkConf["srcTek"];
    bool tekSrcEnable = srcTekConf["enable"].as<bool>(false);
    std::string tekSrcAlgo = srcTekConf["tekAlgo"].as<std::string>();
    std::transform(tekSrcAlgo.begin(), tekSrcAlgo.end(), tekSrcAlgo.begin(), ::tolower);
    m_tekSrcKeyId = (uint32_t)::strtoul(srcTekConf["tekKeyId"].as<std::string>("0").c_str(), NULL, 16);
    if (tekSrcEnable && m_tekSrcKeyId > 0U) {
        if (tekSrcAlgo == TEK_AES)
            m_tekSrcAlgoId = p25::defines::ALGO_AES_256;
        else if (tekSrcAlgo == TEK_ARC4)
            m_tekSrcAlgoId = p25::defines::ALGO_ARC4;
        else {
            ::LogError(LOG_HOST, "Invalid TEK algorithm specified, must be \"aes\" or \"adp\".");
            m_tekSrcAlgoId = p25::defines::ALGO_UNENCRYPT;
            m_tekSrcKeyId = 0U;
        }
    }

    // destination TEK parameters
    yaml::Node dstTekConf = networkConf["srcTek"];
    bool tekDstEnable = dstTekConf["enable"].as<bool>(false);
    std::string tekDstAlgo = dstTekConf["tekAlgo"].as<std::string>();
    std::transform(tekDstAlgo.begin(), tekDstAlgo.end(), tekDstAlgo.begin(), ::tolower);
    m_tekDstKeyId = (uint32_t)::strtoul(dstTekConf["tekKeyId"].as<std::string>("0").c_str(), NULL, 16);
    if (tekDstEnable && m_tekDstKeyId > 0U) {
        if (tekDstAlgo == TEK_AES)
            m_tekDstAlgoId = p25::defines::ALGO_AES_256;
        else if (tekDstAlgo == TEK_ARC4)
            m_tekDstAlgoId = p25::defines::ALGO_ARC4;
        else {
            ::LogError(LOG_HOST, "Invalid TEK algorithm specified, must be \"aes\" or \"adp\".");
            m_tekDstAlgoId = p25::defines::ALGO_UNENCRYPT;
            m_tekDstKeyId = 0U;
        }
    }

    m_twoWayPatch = networkConf["twoWay"].as<bool>(false);

    // make sure our destination ID is sane
    if (m_srcTGId == 0U) {
        ::LogError(LOG_HOST, "Patch source TGID cannot be set to 0.");
        return false;
    }

    if (m_dstTGId == 0U) {
        ::LogError(LOG_HOST, "Patch destination TGID cannot be set to 0.");
        return false;
    }

    if (m_srcTGId == m_dstTGId) {
        ::LogError(LOG_HOST, "Patch source TGID and destination TGID cannot be the same.");
        return false;
    }

    // make sure we're range checked
    switch (m_digiMode) {
    case TX_MODE_DMR:
        {
            if (m_srcTGId > 16777215) {
                ::LogError(LOG_HOST, "Patch source TGID cannot be greater than 16777215.");
                return false;
            }

            if (m_dstTGId > 16777215) {
                ::LogError(LOG_HOST, "Patch source TGID cannot be greater than 16777215.");
                return false;
            }
        }
        break;
    case TX_MODE_P25:
        {
            if (m_srcTGId > 65535) {
                ::LogError(LOG_HOST, "Patch source TGID cannot be greater than 65535.");
                return false;
            }

            if (m_dstTGId > 65535) {
                ::LogError(LOG_HOST, "Patch destination TGID cannot be greater than 65535.");
                return false;
            }
        }
        break;
    }

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

    LogInfo("    Source TGID: %u", m_srcTGId);
    LogInfo("    Source DMR Slot: %u", m_srcSlot);

    LogInfo("    Source Traffic Encrypted: %s", tekSrcEnable ? "yes" : "no");
    if (tekSrcEnable) {
        LogInfo("    Source TEK Algorithm: %s", tekSrcAlgo.c_str());
        LogInfo("    Source TEK Key ID: $%04X", m_tekSrcKeyId);
    }

    LogInfo("    Destination TGID: %u", m_dstTGId);
    LogInfo("    Destination DMR Slot: %u", m_dstSlot);

    LogInfo("    Destination Traffic Encrypted: %s", tekDstEnable ? "yes" : "no");
    if (tekDstEnable) {
        LogInfo("    Destination TEK Algorithm: %s", tekDstAlgo.c_str());
        LogInfo("    Destination TEK Key ID: $%04X", m_tekDstKeyId);
    }

    LogInfo("    Two-Way Patch: %s", m_twoWayPatch ? "yes" : "no");

    if (debug) {
        LogInfo("    Debug: yes");
    }

    bool dmr = false, p25 = false;
    switch (m_digiMode) {
    case TX_MODE_DMR:
        dmr = true;
        break;
    case TX_MODE_P25:
        p25 = true;
        break;
    }

    // initialize networking
    m_network = new PeerNetwork(address, port, local, id, password, true, debug, dmr, p25, false, true, true, true, allowDiagnosticTransfer, true, false);

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

    return true;
}

/* Initializes MMDVM network connectivity. */

bool HostPatch::createMMDVMP25Network()
{
    yaml::Node networkConf = m_conf["network"];

    std::string address = networkConf["mmdvmGatewayAddress"].as<std::string>();
    uint16_t port = (uint16_t)networkConf["mmdvmGatewayPort"].as<uint32_t>(42020U);
    uint16_t localPort = (uint16_t)networkConf["localGatewayPort"].as<uint32_t>(32010U);
    bool debug = networkConf["debug"].as<bool>(false);

    LogInfo("MMDVM Network Parameters");
    LogInfo("    Address: %s", address.c_str());
    LogInfo("    Port: %u", port);
    LogInfo("    Local Port: %u", localPort);

    if (debug) {
        LogInfo("    Debug: yes");
    }

    // initialize networking
    m_mmdvmP25Net = new mmdvm::P25Network(address, port, localPort, debug);

    bool ret = m_mmdvmP25Net->open();
    if (!ret) {
        delete m_mmdvmP25Net;
        m_mmdvmP25Net = nullptr;
        LogError(LOG_HOST, "failed to initialize MMDVM networking!");
        return false;
    }

    return true;
}

/* Helper to process DMR network traffic. */

void HostPatch::processDMRNetwork(uint8_t* buffer, uint32_t length)
{
    assert(buffer != nullptr);
    using namespace dmr;
    using namespace dmr::defines;

    if (m_digiMode != TX_MODE_DMR)
        return;

    // process network message header
    uint32_t seqNo = buffer[4U];

    uint32_t srcId = GET_UINT24(buffer, 5U);
    uint32_t dstId = GET_UINT24(buffer, 8U);

    uint8_t controlByte = buffer[14U];

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

    if (dataSync) {
        dataType = (DataType::E)(buffer[15U] & 0x0FU);
        ::memcpy(data.get(), buffer + 20U, DMR_FRAME_LENGTH_BYTES);
    }
    else if (voiceSync) {
        ::memcpy(data.get(), buffer + 20U, DMR_FRAME_LENGTH_BYTES);
    }
    else {
        dataType = DataType::VOICE;
        ::memcpy(data.get(), buffer + 20U, DMR_FRAME_LENGTH_BYTES);
    }

    if (flco == FLCO::GROUP) {
        if (srcId == 0)
            return;

        // ensure destination ID matches and slot matches
        if (dstId != m_srcTGId && dstId != m_dstTGId)
            return;
        if (slotNo != m_srcSlot && slotNo != m_dstSlot)
            return;

        uint32_t actualDstId = m_dstTGId;
        if (m_twoWayPatch) {
            if (dstId == m_dstTGId)
                actualDstId = m_srcTGId;
        } else {
            if (dstId == m_dstTGId)
                return;
        }

        // is this a new call stream?
        if (m_network->getDMRStreamId(slotNo) != m_rxStreamId) {
            m_callInProgress = true;

            uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            m_rxStartTime = now;

            LogMessage(LOG_HOST, "DMR, call start, srcId = %u, dstId = %u, slot = %u", srcId, dstId, slotNo);
        }

        if (dataSync && (dataType == DataType::TERMINATOR_WITH_LC)) {
            // generate DMR network frame
            data::NetData dmrData;
            dmrData.setSlotNo(m_dstSlot);
            dmrData.setDataType(DataType::TERMINATOR_WITH_LC);
            dmrData.setSrcId(srcId);
            dmrData.setDstId(actualDstId);
            dmrData.setFLCO(flco);
            dmrData.setControl(controlByte);

            uint8_t n = data[15U] & 0x0FU;

            dmrData.setN(n);
            dmrData.setSeqNo(seqNo);
            dmrData.setBER(0U);
            dmrData.setRSSI(0U);

            dmrData.setData(data.get());

            m_network->writeDMRTerminator(dmrData, &seqNo, &n, m_dmrEmbeddedData);
            m_network->resetDMR(dmrData.getSlotNo());

            if (m_rxStartTime > 0U) {
                uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                uint64_t diff = now - m_rxStartTime;

                LogMessage(LOG_HOST, "DMR, call end, srcId = %u, dstId = %u, dur = %us", srcId, dstId, diff / 1000U);
            }

            m_callInProgress = false;
            m_rxStartTime = 0U;
            m_rxStreamId = 0U;
            return;
        }

        m_rxStreamId = m_network->getDMRStreamId(slotNo);

        uint8_t* buffer = nullptr;

        // if we can, use the LC from the voice header as to keep all options intact
        if (dataSync && (dataType == DataType::VOICE_LC_HEADER)) {
            lc::LC lc = lc::LC();
            lc::FullLC fullLC = lc::FullLC();
            lc = *fullLC.decode(data.get(), DataType::VOICE_LC_HEADER);

            LogMessage(LOG_HOST, DMR_DT_VOICE_LC_HEADER ", slot = %u, srcId = %u, dstId = %u, FLCO = $%02X", m_srcSlot,
                lc.getSrcId(), lc.getDstId(), flco);

            // send DMR voice header
            buffer = new uint8_t[DMR_FRAME_LENGTH_BYTES];

            lc.setDstId(actualDstId);
            m_dmrEmbeddedData.setLC(lc);

            // generate the Slot TYpe
            SlotType slotType = SlotType();
            slotType.setDataType(DataType::VOICE_LC_HEADER);
            slotType.encode(buffer);

            fullLC.encode(lc, buffer, DataType::VOICE_LC_HEADER);

            // generate DMR network frame
            data::NetData dmrData;
            dmrData.setSlotNo(m_dstSlot);
            dmrData.setDataType(DataType::VOICE_LC_HEADER);
            dmrData.setSrcId(srcId);
            dmrData.setDstId(actualDstId);
            dmrData.setFLCO(flco);
            dmrData.setControl(controlByte);
            if (m_grantDemand) {
                dmrData.setControl(0x80U); // DMR remote grant demand flag
            } else {
                dmrData.setControl(0U);
            }

            uint8_t n = data[15U] & 0x0FU;

            dmrData.setN(n);
            dmrData.setSeqNo(seqNo);
            dmrData.setBER(0U);
            dmrData.setRSSI(0U);

            dmrData.setData(buffer);

            m_network->writeDMR(dmrData, false);
            delete[] buffer;
        }

        // if we can, use the PI LC from the PI voice header as to keep all options intact
        if (dataSync && (dataType == DataType::VOICE_PI_HEADER)) {
            lc::PrivacyLC lc = lc::PrivacyLC();
            lc::FullLC fullLC = lc::FullLC();
            lc = *fullLC.decodePI(data.get());

            LogMessage(LOG_HOST, DMR_DT_VOICE_PI_HEADER ", slot = %u, algId = %u, kId = %u, dstId = %u", m_srcSlot,
                lc.getAlgId(), lc.getKId(), lc.getDstId());

            // send DMR voice header
            buffer = new uint8_t[DMR_FRAME_LENGTH_BYTES];

            lc.setDstId(actualDstId);

            // generate the Slot TYpe
            SlotType slotType = SlotType();
            slotType.setDataType(DataType::VOICE_PI_HEADER);
            slotType.encode(buffer);

            fullLC.encodePI(lc, buffer);

            // generate DMR network frame
            data::NetData dmrData;
            dmrData.setSlotNo(m_dstSlot);
            dmrData.setDataType(DataType::VOICE_PI_HEADER);
            dmrData.setSrcId(srcId);
            dmrData.setDstId(actualDstId);
            dmrData.setFLCO(flco);
            dmrData.setControl(controlByte);
            if (m_grantDemand) {
                dmrData.setControl(0x80U); // DMR remote grant demand flag
            } else {
                dmrData.setControl(0U);
            }

            uint8_t n = data[15U] & 0x0FU;

            dmrData.setN(n);
            dmrData.setSeqNo(seqNo);
            dmrData.setBER(0U);
            dmrData.setRSSI(0U);

            dmrData.setData(buffer);

            m_network->writeDMR(dmrData, false);
            delete[] buffer;
        }

        if (dataType == DataType::VOICE_SYNC || dataType == DataType::VOICE) {
            // send DMR voice
            buffer = new uint8_t[DMR_FRAME_LENGTH_BYTES];
            ::memcpy(buffer, data.get(), DMR_FRAME_LENGTH_BYTES);

            uint8_t n = data[15U] & 0x0FU;

            DataType::E dataType = DataType::VOICE_SYNC;
            if (n == 0)
                dataType = DataType::VOICE_SYNC;
            else {
                dataType = DataType::VOICE;

                uint8_t lcss = m_dmrEmbeddedData.getData(buffer, n);

                // generated embedded signalling
                data::EMB emb = data::EMB();
                emb.setColorCode(0U);
                emb.setLCSS(lcss);
                emb.encode(buffer);
            }

            LogMessage(LOG_HOST, DMR_DT_VOICE ", srcId = %u, dstId = %u, slot = %u, seqNo = %u", srcId, dstId, m_srcSlot, seqNo);

            // generate DMR network frame
            data::NetData dmrData;
            dmrData.setSlotNo(m_dstSlot);
            dmrData.setDataType(dataType);
            dmrData.setSrcId(srcId);
            dmrData.setDstId(actualDstId);
            dmrData.setFLCO(flco);
            dmrData.setN(n);
            dmrData.setSeqNo(seqNo);
            dmrData.setBER(0U);
            dmrData.setRSSI(0U);

            dmrData.setData(buffer);

            m_network->writeDMR(dmrData, false);
        }
    }
}

/* Helper to process P25 network traffic. */

void HostPatch::processP25Network(uint8_t* buffer, uint32_t length)
{
    assert(buffer != nullptr);
    using namespace p25;
    using namespace p25::defines;
    using namespace p25::dfsi::defines;

    if (m_digiMode != TX_MODE_P25)
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

    uint32_t srcId = GET_UINT24(buffer, 5U);
    uint32_t dstId = GET_UINT24(buffer, 8U);

    uint8_t lsd1 = buffer[20U];
    uint8_t lsd2 = buffer[21U];

    FrameType::E frameType = (FrameType::E)buffer[180U];

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

        // ensure destination ID matches
        if (dstId != m_srcTGId && dstId != m_dstTGId)
            return;

        bool reverseEncrypt = false;
        uint32_t actualDstId = m_srcTGId;
        uint8_t tekAlgoId = m_tekSrcAlgoId;
        uint16_t tekKeyId = m_tekSrcKeyId;

        if (!m_mmdvmP25Reflector) {
            actualDstId = m_dstTGId;
            if (m_twoWayPatch) {
                if (dstId == m_dstTGId) {
                    actualDstId = m_srcTGId;
                    tekAlgoId = m_tekDstAlgoId;
                    tekKeyId = m_tekDstKeyId;
                    reverseEncrypt = true;
                }
            } else {
                if (dstId == m_dstTGId)
                    return;
            }
        }

        // is this a new call stream?
        uint16_t callKID = 0U;
        if (m_network->getP25StreamId() != m_rxStreamId && ((duid != DUID::TDU) && (duid != DUID::TDULC))) {
            m_callInProgress = true;

            // if this is the beginning of a call and we have a valid HDU frame, extract the algo ID
            uint8_t frameType = buffer[180U];
            if (frameType == FrameType::HDU_VALID) {
                m_callAlgoId = buffer[181U];
                if (m_callAlgoId != ALGO_UNENCRYPT) {
                    callKID = GET_UINT16(buffer, 182U);

                    if (m_callAlgoId != tekAlgoId && callKID != tekKeyId) {
                        m_callAlgoId = ALGO_UNENCRYPT;
                        m_callInProgress = false;

                        LogWarning(LOG_HOST, "P25, call ignored, using different encryption parameters, callAlgoId = $%02X, callKID = $%04X, tekAlgoId = $%02X, tekKID = $%04X", m_callAlgoId, callKID, tekAlgoId, tekKeyId);
                        return;
                    } else {
                        uint8_t mi[MI_LENGTH_BYTES];
                        ::memset(mi, 0x00U, MI_LENGTH_BYTES);
                        for (uint8_t i = 0; i < MI_LENGTH_BYTES; i++) {
                            mi[i] = buffer[184U + i];
                        }

                        if (reverseEncrypt) {
                            m_p25DstCrypto->setMI(mi);
                            m_p25DstCrypto->generateKeystream();
                        } else {
                            m_p25SrcCrypto->setMI(mi);
                            m_p25SrcCrypto->generateKeystream();
                        }
                    }
                }
            }

            uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            m_rxStartTime = now;

            LogMessage(LOG_HOST, "P25, call start, srcId = %u, dstId = %u", srcId, dstId);

            if (m_grantDemand) {
                p25::lc::LC lc = p25::lc::LC();
                lc.setLCO(p25::defines::LCO::GROUP);
                lc.setDstId(dstId);
                lc.setSrcId(srcId);

                p25::data::LowSpeedData lsd = p25::data::LowSpeedData();

                uint8_t controlByte = 0x80U;
                m_network->writeP25TDU(lc, lsd, controlByte);
            }
        }

        if ((duid == DUID::TDU) || (duid == DUID::TDULC)) {
            // ignore TDU's that are grant demands
            if (grantDemand)
                return;

            p25::lc::LC lc = p25::lc::LC();
            lc.setLCO(p25::defines::LCO::GROUP);
            lc.setDstId(actualDstId);
            lc.setSrcId(srcId);

            p25::data::LowSpeedData lsd = p25::data::LowSpeedData();

            LogMessage(LOG_HOST, P25_TDU_STR);

            if (m_mmdvmP25Reflector) {
                m_mmdvmP25Net->writeTDU();
            }
            else {
                uint8_t controlByte = 0x00U;
                m_network->writeP25TDU(lc, lsd, controlByte);
            }

            if (m_rxStartTime > 0U) {
                uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                uint64_t diff = now - m_rxStartTime;

                LogMessage(LOG_HOST, "P25, call end, srcId = %u, dstId = %u, dur = %us", srcId, dstId, diff / 1000U);
            }

            m_rxStartTime = 0U;
            m_rxStreamId = 0U;

            m_callInProgress = false;
            m_callAlgoId = ALGO_UNENCRYPT;
            m_rxStartTime = 0U;
            m_rxStreamId = 0U;

            m_p25SrcCrypto->clearMI();
            m_p25SrcCrypto->resetKeystream();
            m_p25DstCrypto->clearMI();
            m_p25DstCrypto->resetKeystream();
            return;
        }

        m_rxStreamId = m_network->getP25StreamId();

        uint8_t* netLDU = new uint8_t[9U * 25U];
        ::memset(netLDU, 0x00U, 9U * 25U);

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
                dfsiLC.decodeLDU1(data.get() + count, netLDU + 10U);
                count += DFSI_LDU1_VOICE1_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE2);
                dfsiLC.decodeLDU1(data.get() + count, netLDU + 26U);
                count += DFSI_LDU1_VOICE2_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE3);
                dfsiLC.decodeLDU1(data.get() + count, netLDU + 55U);
                count += DFSI_LDU1_VOICE3_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE4);
                dfsiLC.decodeLDU1(data.get() + count, netLDU + 80U);
                count += DFSI_LDU1_VOICE4_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE5);
                dfsiLC.decodeLDU1(data.get() + count, netLDU + 105U);
                count += DFSI_LDU1_VOICE5_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE6);
                dfsiLC.decodeLDU1(data.get() + count, netLDU + 130U);
                count += DFSI_LDU1_VOICE6_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE7);
                dfsiLC.decodeLDU1(data.get() + count, netLDU + 155U);
                count += DFSI_LDU1_VOICE7_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE8);
                dfsiLC.decodeLDU1(data.get() + count, netLDU + 180U);
                count += DFSI_LDU1_VOICE8_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE9);
                dfsiLC.decodeLDU1(data.get() + count, netLDU + 204U);
                count += DFSI_LDU1_VOICE9_FRAME_LENGTH_BYTES;

                LogMessage(LOG_NET, P25_LDU1_STR " audio, srcId = %u, dstId = %u", srcId, dstId);

                if (tekAlgoId != ALGO_UNENCRYPT && tekKeyId != 0U) {
                    cryptP25AudioFrame(netLDU, reverseEncrypt, 1U);
                }

                control = lc::LC(*dfsiLC.control());

                control.setSrcId(srcId);
                control.setDstId(actualDstId);

                // if this is the beginning of a call and we have a valid HDU frame, extract the algo ID
                if (frameType == FrameType::HDU_VALID) {
                    uint8_t algoId = buffer[181U];
                    if (algoId != ALGO_UNENCRYPT) {
                        uint16_t kid = GET_UINT16(buffer, 182U);

                        uint8_t mi[MI_LENGTH_BYTES];
                        ::memset(mi, 0x00U, MI_LENGTH_BYTES);
                        for (uint8_t i = 0; i < MI_LENGTH_BYTES; i++) {
                            mi[i] = buffer[184U + i];
                        }

                        control.setAlgId(algoId);
                        control.setKId(kid);
                        control.setMI(mi);
                    }
                }

                // the previous is nice and all -- but if we're cross-encrypting, we need to use the TEK
                if (tekAlgoId != ALGO_UNENCRYPT && tekKeyId != 0U) {
                    control.setAlgId(tekAlgoId);
                    control.setKId(tekKeyId);

                    uint8_t mi[MI_LENGTH_BYTES];
                    ::memset(mi, 0x00U, MI_LENGTH_BYTES);
                    if (!reverseEncrypt)
                        m_p25SrcCrypto->getMI(mi);
                    else
                        m_p25DstCrypto->getMI(mi);

                    control.setMI(mi);
                }

                if (m_mmdvmP25Reflector) {
                    ::memcpy(m_netLDU1, netLDU, 9U * 25U);
                    m_gotNetLDU1 = true;
                    m_netLC = control;

                    writeNet_LDU1(false);
                } else {
                    m_network->writeP25LDU1(control, lsd, netLDU, frameType);
                }
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
                dfsiLC.decodeLDU2(data.get() + count, netLDU + 10U);
                count += DFSI_LDU2_VOICE10_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE11);
                dfsiLC.decodeLDU2(data.get() + count, netLDU + 26U);
                count += DFSI_LDU2_VOICE11_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE12);
                dfsiLC.decodeLDU2(data.get() + count, netLDU + 55U);
                count += DFSI_LDU2_VOICE12_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE13);
                dfsiLC.decodeLDU2(data.get() + count, netLDU + 80U);
                count += DFSI_LDU2_VOICE13_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE14);
                dfsiLC.decodeLDU2(data.get() + count, netLDU + 105U);
                count += DFSI_LDU2_VOICE14_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE15);
                dfsiLC.decodeLDU2(data.get() + count, netLDU + 130U);
                count += DFSI_LDU2_VOICE15_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE16);
                dfsiLC.decodeLDU2(data.get() + count, netLDU + 155U);
                count += DFSI_LDU2_VOICE16_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE17);
                dfsiLC.decodeLDU2(data.get() + count, netLDU + 180U);
                count += DFSI_LDU2_VOICE17_FRAME_LENGTH_BYTES;

                dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE18);
                dfsiLC.decodeLDU2(data.get() + count, netLDU + 204U);
                count += DFSI_LDU2_VOICE18_FRAME_LENGTH_BYTES;

                LogMessage(LOG_NET, P25_LDU2_STR " audio, algo = $%02X, kid = $%04X", dfsiLC.control()->getAlgId(), dfsiLC.control()->getKId());

                if (tekAlgoId != ALGO_UNENCRYPT && tekKeyId != 0U) {
                    cryptP25AudioFrame(netLDU, reverseEncrypt, 2U);
                }

                control = lc::LC(*dfsiLC.control());

                control.setSrcId(srcId);
                control.setDstId(actualDstId);

                // set the algo ID and key ID
                if (tekAlgoId != ALGO_UNENCRYPT && tekKeyId != 0U) {
                    control.setAlgId(tekAlgoId);
                    control.setKId(tekKeyId);

                    uint8_t mi[MI_LENGTH_BYTES];
                    ::memset(mi, 0x00U, MI_LENGTH_BYTES);
                    if (!reverseEncrypt)
                        m_p25SrcCrypto->getMI(mi);
                    else
                        m_p25DstCrypto->getMI(mi);

                    control.setMI(mi);
                }

                if (m_mmdvmP25Reflector) {
                    ::memcpy(m_netLDU2, netLDU, 9U * 25U);
                    m_gotNetLDU2 = true;
                    m_netLC = control;

                    writeNet_LDU2(false);
                } else {
                    m_network->writeP25LDU2(control, lsd, netLDU);
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
    }
}

/* Helper to cross encrypt P25 network traffic audio frames. */

void HostPatch::cryptP25AudioFrame(uint8_t* ldu, bool reverseEncrypt, uint8_t p25N)
{
    assert(ldu != nullptr);
    using namespace p25;
    using namespace p25::defines;

    uint8_t tekSrcAlgoId = m_tekSrcAlgoId;
    uint16_t tekSrcKeyId = m_tekSrcKeyId;
    uint8_t tekDstAlgoId = m_tekDstAlgoId;
    uint16_t tekDstKeyId = m_tekDstKeyId;

    if (reverseEncrypt) {
        tekSrcAlgoId = m_tekDstAlgoId;
        tekSrcKeyId = m_tekDstKeyId;
        tekDstAlgoId = m_tekSrcAlgoId;
        tekDstKeyId = m_tekSrcKeyId;
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

        // Utils::dump(1U, "IMBE", imbe, RAW_IMBE_LENGTH_BYTES);

        // first -- decrypt the IMBE codeword
        if (tekSrcAlgoId != p25::defines::ALGO_UNENCRYPT && tekSrcKeyId > 0U) {
            if (!reverseEncrypt && m_p25SrcCrypto->getTEKLength() > 0U) {
                switch (tekSrcAlgoId) {
                case p25::defines::ALGO_AES_256:
                    m_p25SrcCrypto->cryptAES_IMBE(imbe, (p25N == 1U) ? DUID::LDU1 : DUID::LDU2);
                    break;
                case p25::defines::ALGO_ARC4:
                    m_p25SrcCrypto->cryptARC4_IMBE(imbe, (p25N == 1U) ? DUID::LDU1 : DUID::LDU2);
                    break;
                default:
                    LogError(LOG_HOST, "unsupported TEK algorithm, tekAlgoId = $%02X", tekSrcAlgoId);
                    break;
                }
            } else {
                if (m_p25DstCrypto->getTEKLength() > 0U) {
                    switch (tekDstAlgoId) {
                    case p25::defines::ALGO_AES_256:
                        m_p25DstCrypto->cryptAES_IMBE(imbe, (p25N == 1U) ? DUID::LDU1 : DUID::LDU2);
                        break;
                    case p25::defines::ALGO_ARC4:
                        m_p25DstCrypto->cryptARC4_IMBE(imbe, (p25N == 1U) ? DUID::LDU1 : DUID::LDU2);
                        break;
                    default:
                        LogError(LOG_HOST, "unsupported TEK algorithm, tekAlgoId = $%02X", tekDstAlgoId);
                        break;
                    }
                }
            }
        }

        // second -- reencrypt the IMBE codeword
        if (tekDstAlgoId != p25::defines::ALGO_UNENCRYPT && tekDstKeyId > 0U) {
            if (!reverseEncrypt && m_p25DstCrypto->getTEKLength() > 0U) {
                switch (tekDstAlgoId) {
                case p25::defines::ALGO_AES_256:
                    m_p25DstCrypto->cryptAES_IMBE(imbe, (p25N == 1U) ? DUID::LDU1 : DUID::LDU2);
                    break;
                case p25::defines::ALGO_ARC4:
                    m_p25DstCrypto->cryptARC4_IMBE(imbe, (p25N == 1U) ? DUID::LDU1 : DUID::LDU2);
                    break;
                default:
                    LogError(LOG_HOST, "unsupported TEK algorithm, tekAlgoId = $%02X", tekDstAlgoId);
                    break;
                }
            } else {
                if (m_p25SrcCrypto->getTEKLength() > 0U) {
                    switch (tekSrcAlgoId) {
                    case p25::defines::ALGO_AES_256:
                        m_p25SrcCrypto->cryptAES_IMBE(imbe, (p25N == 1U) ? DUID::LDU1 : DUID::LDU2);
                        break;
                    case p25::defines::ALGO_ARC4:
                        m_p25SrcCrypto->cryptARC4_IMBE(imbe, (p25N == 1U) ? DUID::LDU1 : DUID::LDU2);
                        break;
                    default:
                        LogError(LOG_HOST, "unsupported TEK algorithm, tekAlgoId = $%02X", tekSrcAlgoId);
                        break;
                    }
                }
            }
        }
    }
}

/* Helper to process a FNE KMM TEK response. */

void HostPatch::processTEKResponse(p25::kmm::KeyItem* ki, uint8_t algId, uint8_t keyLength)
{
    if (ki == nullptr)
        return;

    if (algId == m_tekSrcAlgoId && ki->kId() == m_tekSrcKeyId) {
        LogMessage(LOG_HOST, "Source TEK loaded, algId = $%02X, kId = $%04X, sln = $%04X", algId, ki->kId(), ki->sln());
        UInt8Array tek = std::make_unique<uint8_t[]>(keyLength);
        ki->getKey(tek.get());

        m_p25SrcCrypto->setTEKAlgoId(algId);
        m_p25SrcCrypto->setTEKKeyId(ki->kId());
        m_p25SrcCrypto->setKey(tek.get(), keyLength);
    }

    if (algId == m_tekDstAlgoId && ki->kId() == m_tekDstKeyId) {
        LogMessage(LOG_HOST, "Destination TEK loaded, algId = $%02X, kId = $%04X, sln = $%04X", algId, ki->kId(), ki->sln());
        UInt8Array tek = std::make_unique<uint8_t[]>(keyLength);
        ki->getKey(tek.get());

        m_p25DstCrypto->setTEKAlgoId(algId);
        m_p25DstCrypto->setTEKKeyId(ki->kId());
        m_p25DstCrypto->setKey(tek.get(), keyLength);
    }
}

/* Helper to check for an unflushed LDU1 packet. */

void HostPatch::checkNet_LDU1()
{
    if (m_netState == RS_NET_IDLE)
        return;

    // check for an unflushed LDU1
    if ((m_netLDU1[10U] != 0x00U || m_netLDU1[26U] != 0x00U || m_netLDU1[55U] != 0x00U ||
        m_netLDU1[80U] != 0x00U || m_netLDU1[105U] != 0x00U || m_netLDU1[130U] != 0x00U ||
        m_netLDU1[155U] != 0x00U || m_netLDU1[180U] != 0x00U || m_netLDU1[204U] != 0x00U) &&
        m_gotNetLDU1)
        writeNet_LDU1(false);
}

/* Helper to write a network P25 LDU1 packet. */

void HostPatch::writeNet_LDU1(bool toFNE)
{
    using namespace p25;
    using namespace p25::defines;
    using namespace p25::dfsi::defines;

    if (toFNE) {
        if (m_netState == RS_NET_IDLE) {
            m_callInProgress = true;

            uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            m_rxStartTime = now;

            uint8_t lco = m_netLDU1[51U];
            uint8_t mfId = m_netLDU1[52U];
            uint32_t dstId = GET_UINT24(m_netLDU1, 76U);
            uint32_t srcId = GET_UINT24(m_netLDU1, 101U);

            LogMessage(LOG_HOST, "MMDVM P25, call start, srcId = %u, dstId = %u", srcId, dstId);

            lc::LC lc = lc::LC();
            m_netLC = lc;
            m_netLC.setLCO(lco);
            m_netLC.setMFId(mfId);
            m_netLC.setDstId(dstId);
            m_netLC.setSrcId(srcId);

            if (m_grantDemand) {
                p25::lc::LC lc = p25::lc::LC();
                lc.setLCO(p25::defines::LCO::GROUP);
                lc.setDstId(dstId);
                lc.setSrcId(srcId);

                p25::data::LowSpeedData lsd = p25::data::LowSpeedData();

                uint8_t controlByte = 0x80U;
                m_network->writeP25TDU(lc, lsd, controlByte);
            }
        }

        p25::data::LowSpeedData lsd = p25::data::LowSpeedData();
        lsd.setLSD1(m_netLDU1[201U]);
        lsd.setLSD2(m_netLDU1[202U]);

        LogMessage(LOG_NET, "MMDVM " P25_LDU1_STR " audio, srcId = %u, dstId = %u", m_netLC.getSrcId(), m_netLC.getDstId());

        if (m_debug)
            Utils::dump(1U, "MMDVM -> DVM LDU1", m_netLDU1, 9U * 25U);

        m_network->writeP25LDU1(m_netLC, lsd, m_netLDU1, FrameType::DATA_UNIT);

        m_netState = RS_NET_AUDIO;
        resetWithNullAudio(m_netLDU1, false);
        m_gotNetLDU1 = false;
    }
    else {
        if (m_debug)
            Utils::dump(1U, "DVM -> MMDVM LDU1", m_netLDU1, 9U * 25U);

        // add the Low Speed Data
        p25::data::LowSpeedData lsd = p25::data::LowSpeedData();
        lsd.setLSD1(m_netLDU1[201U]);
        lsd.setLSD2(m_netLDU1[202U]);

        m_mmdvmP25Net->writeLDU1(m_netLDU1, m_netLC, lsd, false);

        resetWithNullAudio(m_netLDU1, false);
        m_gotNetLDU1 = false;
    }
}

/* Helper to check for an unflushed LDU2 packet. */

void HostPatch::checkNet_LDU2()
{
    if (m_netState == RS_NET_IDLE)
        return;

    // check for an unflushed LDU2
    if ((m_netLDU2[10U] != 0x00U || m_netLDU2[26U] != 0x00U || m_netLDU2[55U] != 0x00U ||
        m_netLDU2[80U] != 0x00U || m_netLDU2[105U] != 0x00U || m_netLDU2[130U] != 0x00U ||
        m_netLDU2[155U] != 0x00U || m_netLDU2[180U] != 0x00U || m_netLDU2[204U] != 0x00U) &&
        m_gotNetLDU2) {
        writeNet_LDU2(false);
    }
}

/* Helper to write a network P25 LDU2 packet. */

void HostPatch::writeNet_LDU2(bool toFNE)
{
    using namespace p25;
    using namespace p25::defines;
    using namespace p25::dfsi::defines;

    if (toFNE) {
        p25::data::LowSpeedData lsd = p25::data::LowSpeedData();
        lsd.setLSD1(m_netLDU2[201U]);
        lsd.setLSD2(m_netLDU2[202U]);

        LogMessage(LOG_NET, "MMDVM " P25_LDU2_STR " audio");

        if (m_debug)
            Utils::dump(1U, "MMDVM -> DVM LDU2", m_netLDU2, 9U * 25U);

        m_network->writeP25LDU2(m_netLC, lsd, m_netLDU2);

        resetWithNullAudio(m_netLDU2, false);
        m_gotNetLDU2 = false;
    }
    else {
        if (m_debug)
            Utils::dump(1U, "DVM -> MMDVM LDU2", m_netLDU2, 9U * 25U);

        // add the Low Speed Data
        p25::data::LowSpeedData lsd = p25::data::LowSpeedData();
        lsd.setLSD1(m_netLDU2[201U]);
        lsd.setLSD2(m_netLDU2[202U]);

        m_mmdvmP25Net->writeLDU2(m_netLDU2, m_netLC, lsd, false);

        resetWithNullAudio(m_netLDU2, false);
        m_gotNetLDU2 = false;
    }
}

/* Entry point to network processing thread. */

void* HostPatch::threadNetworkProcess(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
#if defined(_WIN32)
        ::CloseHandle(th->thread);
#else
        ::pthread_detach(th->thread);
#endif // defined(_WIN32)

        std::string threadName("patch:net-process");
        HostPatch* patch = static_cast<HostPatch*>(th->obj);
        if (patch == nullptr) {
            g_killed = true;
            LogError(LOG_HOST, "[FAIL] %s", threadName.c_str());
        }

        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogMessage(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        while (!g_killed) {
            if (!patch->m_running) {
                Thread::sleep(1U);
                continue;
            }

            if (patch->m_network->getStatus() == NET_STAT_RUNNING) {
                // check if we need to request a TEK for the source TGID
                if (patch->m_tekSrcAlgoId != p25::defines::ALGO_UNENCRYPT && patch->m_tekSrcKeyId > 0U) {
                    if (patch->m_p25SrcCrypto->getTEKLength() == 0U && !patch->m_requestedSrcTek) {
                        patch->m_requestedSrcTek = true;
                        LogMessage(LOG_HOST, "Patch source TGID encryption enabled, requesting TEK from network.");
                        patch->m_network->writeKeyReq(patch->m_tekSrcKeyId, patch->m_tekSrcAlgoId);
                    }
                }

                // check if we need to request a TEK for the destination TGID
                if (patch->m_tekDstAlgoId != p25::defines::ALGO_UNENCRYPT && patch->m_tekDstKeyId > 0U) {
                    if (patch->m_p25DstCrypto->getTEKLength() == 0U && !patch->m_requestedDstTek) {
                        patch->m_requestedDstTek = true;
                        LogMessage(LOG_HOST, "Patch destination TGID encryption enabled, requesting TEK from network.");
                        patch->m_network->writeKeyReq(patch->m_tekDstKeyId, patch->m_tekDstAlgoId);
                    }
                }
            }

            uint32_t length = 0U;
            bool netReadRet = false;
            if (patch->m_digiMode == TX_MODE_DMR) {
                std::lock_guard<std::mutex> lock(HostPatch::m_networkMutex);
                UInt8Array dmrBuffer = patch->m_network->readDMR(netReadRet, length);
                if (netReadRet) {
                    patch->processDMRNetwork(dmrBuffer.get(), length);
                }
            }

            if (patch->m_digiMode == TX_MODE_P25) {
                std::lock_guard<std::mutex> lock(HostPatch::m_networkMutex);
                UInt8Array p25Buffer = patch->m_network->readP25(netReadRet, length);
                if (netReadRet) {
                    patch->processP25Network(p25Buffer.get(), length);
                }
            }

            Thread::sleep(1U);
        }

        LogMessage(LOG_HOST, "[STOP] %s", threadName.c_str());
        delete th;
    }

    return nullptr;
}

/* Entry point to MMDVM network processing thread. */

void* HostPatch::threadMMDVMProcess(void* arg)
{
    using namespace p25;
    using namespace p25::defines;
    using namespace p25::dfsi::defines;

    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
#if defined(_WIN32)
        ::CloseHandle(th->thread);
#else
        ::pthread_detach(th->thread);
#endif // defined(_WIN32)

        std::string threadName("patch:mmdvm-net-process");
        HostPatch* patch = static_cast<HostPatch*>(th->obj);
        if (patch == nullptr) {
            g_killed = true;
            LogError(LOG_HOST, "[FAIL] %s", threadName.c_str());
        }

        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogMessage(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        StopWatch stopWatch;
        stopWatch.start();

        while (!g_killed) {
            if (!patch->m_running) {
                Thread::sleep(1U);
                continue;
            }

            uint32_t ms = stopWatch.elapsed();

            ms = stopWatch.elapsed();
            stopWatch.start();

            if (patch->m_digiMode == TX_MODE_P25) {
                std::lock_guard<std::mutex> lock(HostPatch::m_networkMutex);

                DECLARE_UINT8_ARRAY(buffer, 100U);
                uint32_t len = patch->m_mmdvmP25Net->read(buffer, 100U);
                if (len != 0U) {
                    switch (buffer[0U]) {
                    // LDU1
                    case DFSIFrameType::LDU1_VOICE1:
                        ::memcpy(patch->m_netLDU1 + 0U, buffer, 22U);
                        break;
                    case DFSIFrameType::LDU1_VOICE2:
                        ::memcpy(patch->m_netLDU1 + 25U, buffer, 14U);
                        break;
                    case DFSIFrameType::LDU1_VOICE3:
                        ::memcpy(patch->m_netLDU1 + 50U, buffer, 17U);
                        break;
                    case DFSIFrameType::LDU1_VOICE4:
                        ::memcpy(patch->m_netLDU1 + 75U, buffer, 17U);
                        break;
                    case DFSIFrameType::LDU1_VOICE5:
                        ::memcpy(patch->m_netLDU1 + 100U, buffer, 17U);
                        break;
                    case DFSIFrameType::LDU1_VOICE6:
                        ::memcpy(patch->m_netLDU1 + 125U, buffer, 17U);
                        break;
                    case DFSIFrameType::LDU1_VOICE7:
                        ::memcpy(patch->m_netLDU1 + 150U, buffer, 17U);
                        break;
                    case DFSIFrameType::LDU1_VOICE8:
                        ::memcpy(patch->m_netLDU1 + 175U, buffer, 17U);
                        break;
                    case DFSIFrameType::LDU1_VOICE9:
                        ::memcpy(patch->m_netLDU1 + 200U, buffer, 16U);
                        patch->checkNet_LDU2();

                        if (patch->m_netState != RS_NET_IDLE) {
                            patch->m_gotNetLDU1 = true;
                            patch->writeNet_LDU1(true);
                        }
                        break;

                    // LDU2
                    case DFSIFrameType::LDU2_VOICE10:
                        ::memcpy(patch->m_netLDU2 + 0U, buffer, 22U);
                        break;
                    case DFSIFrameType::LDU2_VOICE11:
                        ::memcpy(patch->m_netLDU2 + 25U, buffer, 14U);
                        break;
                    case DFSIFrameType::LDU2_VOICE12:
                        ::memcpy(patch->m_netLDU2 + 50U, buffer, 17U);
                        break;
                    case DFSIFrameType::LDU2_VOICE13:
                        ::memcpy(patch->m_netLDU2 + 75U, buffer, 17U);
                        break;
                    case DFSIFrameType::LDU2_VOICE14:
                        ::memcpy(patch->m_netLDU2 + 100U, buffer, 17U);
                        break;
                    case DFSIFrameType::LDU2_VOICE15:
                        ::memcpy(patch->m_netLDU2 + 125U, buffer, 17U);
                        break;
                    case DFSIFrameType::LDU2_VOICE16:
                        ::memcpy(patch->m_netLDU2 + 150U, buffer, 17U);
                        break;
                    case DFSIFrameType::LDU2_VOICE17:
                        ::memcpy(patch->m_netLDU2 + 175U, buffer, 17U);
                        break;
                    case DFSIFrameType::LDU2_VOICE18:
                        ::memcpy(patch->m_netLDU2 + 200U, buffer, 16U);
                        if (patch->m_netState == RS_NET_IDLE) {
                            patch->writeNet_LDU1(true);
                        } else {
                            patch->checkNet_LDU1();
                        }
                        
                        patch->writeNet_LDU2(true);
                        break;

                    case 0x80U:
                        {
                            patch->m_netState = RS_NET_IDLE;

                            p25::data::LowSpeedData lsd = p25::data::LowSpeedData();

                            LogMessage(LOG_HOST, "MMDVM " P25_TDU_STR);

                            uint8_t controlByte = 0x00U;
                            patch->m_network->writeP25TDU(patch->m_netLC, lsd, controlByte);

                            if (patch->m_rxStartTime > 0U) {
                                uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                                uint64_t diff = now - patch->m_rxStartTime;

                                LogMessage(LOG_HOST, "MMDVM P25, call end, srcId = %u, dstId = %u, dur = %us", patch->m_netLC.getSrcId(), patch->m_netLC.getDstId(), diff / 1000U);
                            }

                            patch->m_rxStartTime = 0U;
                            patch->m_rxStreamId = 0U;

                            patch->m_callInProgress = false;
                            patch->m_rxStartTime = 0U;
                            patch->m_rxStreamId = 0U;
                        }
                        break;

                    case 0xF0U:
                    case 0xF1U:
                        // these are MMDVM control bytes -- we ignore these
                        break;

                    default:
                        LogError(LOG_NET, "unknown opcode from MMDVM gateway $%02X", buffer[0U]);
                        break;
                    }
                }
            }

            if (ms < 5U)
                Thread::sleep(5U);
        }

        LogMessage(LOG_HOST, "[STOP] %s", threadName.c_str());
        delete th;
    }

    return nullptr;
}

/* Helper to reset IMBE buffer with null frames. */

void HostPatch::resetWithNullAudio(uint8_t* data, bool encrypted)
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
