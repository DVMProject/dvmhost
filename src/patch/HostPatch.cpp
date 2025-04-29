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
    m_identity(),
    m_digiMode(1U),
    m_dmrEmbeddedData(),
    m_grantDemand(false),
    m_callInProgress(false),
    m_rxStartTime(0U),
    m_rxStreamId(0U),
    m_running(false),
    m_trace(false),
    m_debug(false)
{
    /* stub */
}

/* Finalizes a instance of the HostPatch class. */

HostPatch::~HostPatch() = default;

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

    /*
    ** Initialize Threads
    */

    if (!Thread::runAsThread(this, threadNetworkProcess))
        return EXIT_FAILURE;

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

    m_trace = systemConf["trace"].as<bool>(false);
    m_debug = systemConf["debug"].as<bool>(false);

    LogInfo("General Parameters");
    LogInfo("    Digital Mode: %s", m_digiMode == TX_MODE_DMR ? "DMR" : "P25");
    LogInfo("    Grant Demands: %s", m_grantDemand ? "yes" : "no");

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
    LogInfo("    Destination TGID: %u", m_dstTGId);
    LogInfo("    Destination DMR Slot: %u", m_dstSlot);
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

    uint32_t srcId = __GET_UINT16(buffer, 5U);
    uint32_t dstId = __GET_UINT16(buffer, 8U);

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

    uint32_t srcId = __GET_UINT16(buffer, 5U);
    uint32_t dstId = __GET_UINT16(buffer, 8U);

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

        uint32_t actualDstId = m_dstTGId;
        if (m_twoWayPatch) {
            if (dstId == m_dstTGId)
                actualDstId = m_srcTGId;
        } else {
            if (dstId == m_dstTGId)
                return;
        }

        if (m_network->getP25StreamId() != m_rxStreamId && ((duid != DUID::TDU) && (duid != DUID::TDULC))) {
            m_callInProgress = true;

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

            uint8_t controlByte = 0x00U;
            m_network->writeP25TDU(lc, lsd, controlByte);

            if (m_rxStartTime > 0U) {
                uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                uint64_t diff = now - m_rxStartTime;

                LogMessage(LOG_HOST, "P25, call end, srcId = %u, dstId = %u, dur = %us", srcId, dstId, diff / 1000U);
            }

            m_rxStartTime = 0U;
            m_rxStreamId = 0U;

            m_callInProgress = false;
            m_rxStartTime = 0U;
            m_rxStreamId = 0U;
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

                control = lc::LC(*dfsiLC.control());

                control.setSrcId(srcId);
                control.setDstId(actualDstId);

                // if this is the beginning of a call and we have a valid HDU frame, extract the algo ID
                if (frameType == FrameType::HDU_VALID) {
                    uint8_t algoId = buffer[181U];
                    if (algoId != ALGO_UNENCRYPT) {
                        uint16_t kid = __GET_UINT16B(buffer, 182U);

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

                m_network->writeP25LDU1(control, lsd, netLDU, frameType);
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

                control = lc::LC(*dfsiLC.control());

                control.setSrcId(srcId);
                control.setDstId(actualDstId);

                m_network->writeP25LDU2(control, lsd, netLDU);
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
