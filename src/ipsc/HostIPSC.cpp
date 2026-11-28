// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - IPSC Gateway
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "HostIPSC.h"
#include "IPSCMain.h"
#include "common/Log.h"
#include "common/StopWatch.h"
#include "common/Thread.h"
#include "common/dmr/DMRDefines.h"
#include "common/dmr/data/NetData.h"
#include "common/network/Network.h"
#include "common/yaml/Yaml.h"
#include "dmr/BurstConverter.h"
#include "protocol/IPSCClient.h"

#include <chrono>
#include <cstdlib>
#include <cstring>

#if !defined(_WIN32)
#include <unistd.h>
#endif

using namespace dmr::defines;
using namespace ipsc;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the HostIPSC class. */

HostIPSC::HostIPSC(const std::string& configurationFile) :
    m_confFile(configurationFile),
    m_conf(),
    m_network(nullptr),
    m_ipsc(nullptr),
    m_calls(),
    m_embeddedData(),
    m_recent(),
    m_colorCode(1U),
    m_callTimeout(defines::DEFAULT_CALL_TIMEOUT_SECONDS)
{
    m_calls[0U] = {false, false, 0U, 0U, 0U, 0U, true, 0U, std::chrono::steady_clock::time_point()};
    m_calls[1U] = {false, false, 0U, 0U, 0U, 0U, true, 0U, std::chrono::steady_clock::time_point()};
}

/* Finalizes an instance of the HostIPSC class. */

HostIPSC::~HostIPSC()
{
    if (m_ipsc != nullptr) {
        m_ipsc->close();
        delete m_ipsc;
    }

    if (m_network != nullptr) {
        m_network->close();
        delete m_network;
    }
}

/* Runs the gateway application. */

int HostIPSC::run()
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

#if !defined(_WIN32)
    // handle POSIX process forking
    if (m_daemon) {
        // create new process
        const pid_t pid = ::fork();
        if (pid == -1) {
            ::fprintf(stderr, "%s: Couldn't fork(), exiting\n", g_progExe.c_str());
            ::LogFinalise();
            return EXIT_FAILURE;
        } else if (pid != 0) {
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
        ">> IPSC Gateway\r\n");

    if (!createNetwork())
        return EXIT_FAILURE;

    if (!createIPSC())
        return EXIT_FAILURE;

    LogInfoEx(LOG_HOST, "IPSC gateway started");

    StopWatch stopWatch;
    stopWatch.start();

    // main execution loop
    while (!g_killed) {
        const uint32_t ms = stopWatch.elapsed();
        stopWatch.start();

        const auto now = std::chrono::steady_clock::now();
        m_network->clock(ms);

        m_ipsc->clock();

        processNetwork();

        expireRecent(now);
        expireCalls(now);

        if (ms < 2U)
            Thread::sleep(1U);
    }

    if (m_ipsc != nullptr) {
        m_ipsc->close();
        delete m_ipsc;
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

/* Creates and opens the DVM network connection. */

bool HostIPSC::createNetwork()
{
    yaml::Node networkConf = m_conf["network"];
    const uint32_t networkId = networkConf["id"].as<uint32_t>(0U);
    const std::string networkAddress = networkConf["address"].as<std::string>();
    const uint16_t networkPort = uint16_t(networkConf["port"].as<uint32_t>(TRAFFIC_DEFAULT_PORT));
    const uint16_t networkLocalPort = uint16_t(networkConf["localPort"].as<uint32_t>(0U));
    const std::string password = networkConf["password"].as<std::string>();
    const bool networkDebug = networkConf["debug"].as<bool>(false);

    bool encrypted = networkConf["encrypted"].as<bool>(false);
    std::string key = networkConf["presharedKey"].as<std::string>();
    uint8_t presharedKey[AES_WRAPPED_PCKT_KEY_LEN];
    ::memset(presharedKey, 0x00U, sizeof(presharedKey));
    if (!key.empty()) {
        if (key.size() == 32U) {
            key.append(key);
            LogWarning(LOG_HOST, "Half-length network preshared encryption key detected, doubling key on itself.");
        }

        if (key.size() == 64U) {
            if (key.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos) {
                const char* keyPtr = key.c_str();
                for (uint8_t i = 0U; i < AES_WRAPPED_PCKT_KEY_LEN; ++i) {
                    char value[3U] = {keyPtr[0U], keyPtr[1U], 0};
                    presharedKey[i] = uint8_t(::strtoul(value, nullptr, 16));
                    keyPtr += 2U;
                }
            } else {
                LogWarning(LOG_HOST, "Invalid characters in the network preshared encryption key. Encryption disabled.");
                encrypted = false;
            }
        } else {
            LogWarning(LOG_HOST, "Invalid network preshared encryption key length, key should be 32 hex pairs, or 64 characters. Encryption disabled.");
            encrypted = false;
        }
    } else if (encrypted) {
        LogWarning(LOG_HOST, "Network encryption enabled without a preshared encryption key. Encryption disabled.");
        encrypted = false;
    }

    if (networkId == 0U || networkAddress.empty() || password.empty()) {
        LogError(LOG_HOST, "Invalid DVM network configuration");
        return false;
    }

    m_network = new network::Network(networkAddress, networkPort, networkLocalPort, networkId, password, true, networkDebug, true, false, false, false, true,
        true, false, false, false, false);
    m_network->setPacketDump(networkConf["packetDump"].as<bool>(false));
    if (encrypted)
        m_network->setPresharedKey(presharedKey);

    m_network->enable(true);
    return m_network->open();
}

/* Creates and opens the Motorola IPSC connection. */

bool HostIPSC::createIPSC()
{
    yaml::Node ipscConf = m_conf["ipsc"];
    m_colorCode = uint8_t(ipscConf["colorCode"].as<uint32_t>(1U));
    m_callTimeout = ipscConf["callTimeout"].as<uint32_t>(defines::DEFAULT_CALL_TIMEOUT_SECONDS);
    if (m_colorCode > defines::MAX_COLOR_CODE || m_callTimeout == 0U) {
        LogError(LOG_HOST, "Invalid IPSC DMR configuration, colorCode = %u, callTimeout = %u", m_colorCode, m_callTimeout);
        return false;
    }

    m_ipsc = new IPSCClient(ipscConf["masterAddress"].as<std::string>(), uint16_t(ipscConf["masterPort"].as<uint32_t>(defines::DEFAULT_PORT)),
        uint16_t(ipscConf["localPort"].as<uint32_t>(defines::DEFAULT_PORT)), ipscConf["peerId"].as<uint32_t>(0U),
        ipscConf["authenticationKey"].as<std::string>(""), ipscConf["slot1"].as<bool>(true), ipscConf["slot2"].as<bool>(true),
        ipscConf["debug"].as<bool>(false));
    m_ipsc->setVoiceCallback([this](const VoicePacket& packet) { processIPSC(packet); });
    return m_ipsc->open();
}

/* Generates a fingerprint used to suppress reflected voice packets. */

uint64_t HostIPSC::fingerprint(uint32_t sourceId, uint32_t destinationId, uint8_t slotNo, const uint8_t* data, uint32_t length)
{
    uint64_t hash = 1469598103934665603ULL;
    const uint64_t values[] = {sourceId, destinationId, slotNo};
    for (uint64_t value : values) {
        hash ^= value;
        hash *= 1099511628211ULL;
    }

    for (uint32_t i = 0U; i < length; ++i) {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }

    return hash;
}

/* Adds a packet fingerprint to the loop-suppression cache. */

void HostIPSC::remember(uint64_t value)
{
    m_recent[value] = std::chrono::steady_clock::now();
}

/* Determines whether a packet fingerprint is present in the loop-suppression cache. */

bool HostIPSC::remembered(uint64_t value) const
{
    return m_recent.find(value) != m_recent.end();
}

/* Removes expired packet fingerprints from the loop-suppression cache. */

void HostIPSC::expireRecent(std::chrono::steady_clock::time_point now)
{
    for (auto it = m_recent.begin(); it != m_recent.end();) {
        if (now - it->second > std::chrono::seconds(2))
            it = m_recent.erase(it);
        else
            ++it;
    }
}

/* Releases calls which have exceeded the configured inactivity timeout. */

void HostIPSC::expireCalls(std::chrono::steady_clock::time_point now)
{
    for (uint8_t slot = 0U; slot < 2U; ++slot) {
        CallState& call = m_calls[slot];
        if (call.active && now - call.lastActivity > std::chrono::seconds(m_callTimeout)) {
            LogWarning(LOG_HOST, "DMR slot %u call timed out, srcId = %u, dstId = %u", slot + 1U, call.sourceId, call.destinationId);
            call.active = false;
            m_embeddedData[slot].reset();
        }
    }
}

/* Converts and forwards a received IPSC voice packet to the DVM network. */

void HostIPSC::processIPSC(const VoicePacket& packet)
{
    if (packet.slotNo < 1U || packet.slotNo > 2U || packet.burst == nullptr || packet.burstLength == 0U)
        return;
    const uint64_t key = fingerprint(packet.sourceId, packet.destinationId, packet.slotNo, packet.burst, packet.burstLength);
    if (remembered(key))
        return;
    remember(key);

    CallState& call = m_calls[packet.slotNo - 1U];
    const uint8_t type = packet.burst[0U] & defines::BURST_TYPE_MASK;
    if (type == uint8_t(defines::BurstType::VOICE_HEADER)) {
        if (call.active && !call.fromIPSC)
            return;
        const uint8_t colorCode = packet.hasColorCode ? packet.colorCode : m_colorCode;
        call = {true, true, 0U, packet.sequence, packet.sourceId, packet.destinationId, packet.group, colorCode, std::chrono::steady_clock::now()};
        m_embeddedData[packet.slotNo - 1U].reset();
    } else if (!call.active || !call.fromIPSC) {
        return;
    }

    call.lastActivity = std::chrono::steady_clock::now();
    if (packet.hasColorCode)
        call.colorCode = packet.colorCode;
    ::dmr::data::NetData data;
    if (!::ipsc::dmr::BurstConverter::toDMR(packet.burst, packet.burstLength, packet.sourceId, packet.destinationId, packet.group, packet.slotNo,
        packet.sequence, call.n, call.colorCode, m_embeddedData[packet.slotNo - 1U], data))
        return;

    m_network->writeDMR(data);

    if (type == uint8_t(defines::BurstType::VOICE))
        call.n = uint8_t((call.n + 1U) % 6U);

    if (type == uint8_t(defines::BurstType::VOICE_TERMINATOR) || packet.last) {
        call.active = false;
        m_embeddedData[packet.slotNo - 1U].reset();
    }
}

/* Converts and forwards available DVM DMR traffic to the IPSC network. */

void HostIPSC::processNetwork()
{
    bool available = false;
    uint32_t length = 0U;
    UInt8Array packet = m_network->readDMR(available, length);
    if (!available || packet == nullptr || length < 55U)
        return;

    const uint32_t sourceId = GET_UINT24(packet.get(), 5U);
    const uint32_t destinationId = GET_UINT24(packet.get(), 8U);
    const uint8_t slotNo = (packet[15U] & 0x80U) != 0U ? 2U : 1U;
    const bool group = (packet[15U] & 0x40U) == 0U;
    const uint64_t key = fingerprint(sourceId, destinationId, slotNo, packet.get() + 20U, DMR_FRAME_LENGTH_BYTES);
    if (remembered(key))
        return;

    ::dmr::data::NetData data;
    data.setSeqNo(packet[4U]);
    data.setSrcId(sourceId);
    data.setDstId(destinationId);
    data.setSlotNo(slotNo);
    data.setFLCO(group ? FLCO::GROUP : FLCO::PRIVATE);
    data.setControl(packet[14U]);
    if ((packet[15U] & 0x20U) != 0U) {
        data.setDataType(DataType::E(packet[15U] & 0x0FU));
        data.setN(0U);
    } else if ((packet[15U] & 0x10U) != 0U) {
        data.setDataType(DataType::VOICE_SYNC);
        data.setN(0U);
    } else {
        data.setDataType(DataType::VOICE);
        data.setN(packet[15U] & 0x0FU);
    }
    data.setBER(packet[53U]);
    data.setRSSI(packet[54U]);
    data.setData(packet.get() + 20U);

    CallState& call = m_calls[slotNo - 1U];
    if (data.getDataType() == DataType::VOICE_LC_HEADER) {
        if (call.active && call.fromIPSC)
            return;
        call = {false, true, 0U, packet[4U], sourceId, destinationId, group, m_colorCode, std::chrono::steady_clock::now()};
    } else if (!call.active || call.fromIPSC) {
        return;
    }

    call.lastActivity = std::chrono::steady_clock::now();
    uint8_t burst[defines::VOICE_BURST_BASE_LENGTH];
    uint32_t burstLength = 0U;
    if (!::ipsc::dmr::BurstConverter::fromDMR(data, m_colorCode, burst, sizeof(burst), burstLength))
        return;

    const bool last = data.getDataType() == DataType::TERMINATOR_WITH_LC;
    if (m_ipsc->writeVoice(sourceId, destinationId, group, slotNo, call.sequence++, last, burst, burstLength))
        remember(key);

    if (last) {
        call.active = false;
        m_embeddedData[slotNo - 1U].reset();
    }
}
