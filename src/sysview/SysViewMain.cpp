// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - FNE System View
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/Clock.h"
#include "common/dmr/DMRDefines.h"
#include "common/dmr/lc/csbk/CSBKFactory.h"
#include "common/dmr/lc/LC.h"
#include "common/dmr/lc/FullLC.h"
#include "common/dmr/SlotType.h"
#include "common/dmr/Sync.h"
#include "common/p25/P25Defines.h"
#include "common/p25/lc/tdulc/TDULCFactory.h"
#include "common/p25/lc/tsbk/TSBKFactory.h"
#include "common/nxdn/NXDNDefines.h"
#include "common/nxdn/lc/RTCH.h"
#include "common/yaml/Yaml.h"
#include "common/Log.h"
#include "common/StopWatch.h"
#include "common/Thread.h"
#include "p25/tsbk/OSP_GRP_AFF.h"
#include "network/PeerNetwork.h"
#include "SysViewMain.h"
#include "SysViewApplication.h"
#include "SysViewMainWnd.h"
#if !defined(NO_WEBSOCKETS)
#include "HostWS.h"
#endif // !defined(NO_WEBSOCKETS)

using namespace system_clock;
using namespace network;
using namespace lookups;

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <vector>

// ---------------------------------------------------------------------------
//	Macros
// ---------------------------------------------------------------------------

#define IS(s) (::strcmp(argv[i], s) == 0)

// ---------------------------------------------------------------------------
//  Global Variables
// ---------------------------------------------------------------------------

int g_signal = 0;

std::string g_progExe = std::string(__EXE_NAME__);
std::string g_iniFile = std::string(DEFAULT_CONF_FILE);
yaml::Node g_conf;
bool g_debug = false;

bool g_foreground = false;
bool g_killed = false;
bool g_hideLoggingWnd = false;

bool g_webSocketMode = false;

lookups::RadioIdLookup* g_ridLookup = nullptr;
lookups::TalkgroupRulesLookup* g_tidLookup = nullptr;
lookups::IdenTableLookup* g_idenTable = nullptr;

std::unordered_map<uint32_t, std::string> g_peerIdentityNameMap = std::unordered_map<uint32_t, std::string>();

std::function<void(json::object)> g_netDataEvent = nullptr;

SysViewApplication* g_app = nullptr;

network::PeerNetwork* g_network = nullptr;

/**
 * @brief Represents the receive status of a call.
 */
class RxStatus {
public:
    ::system_clock::hrc::hrc_t callStartTime;
    ::system_clock::hrc::hrc_t lastPacket;
    uint32_t srcId;
    uint32_t dstId;
    uint8_t slotNo;
    uint32_t streamId;
};
typedef std::pair<const uint32_t, RxStatus> StatusMapPair;
std::unordered_map<uint32_t, RxStatus> g_dmrStatus;
std::unordered_map<uint32_t, RxStatus> g_p25Status;
std::unordered_map<uint32_t, RxStatus> g_nxdnStatus;

// ---------------------------------------------------------------------------
//	Global Functions
// ---------------------------------------------------------------------------

/* Internal signal handler. */

static void sigHandler(int signum)
{
    g_signal = signum;
    g_killed = true;
}

/* Helper to print a fatal error message and exit. */

void fatal(const char* msg, ...)
{
    char buffer[400U];
    ::memset(buffer, 0x20U, 400U);

    va_list vl;
    va_start(vl, msg);

    ::vsprintf(buffer, msg, vl);

    va_end(vl);

    ::fprintf(stderr, "%s: FATAL PANIC; %s\n", g_progExe.c_str(), buffer);
    exit(EXIT_FAILURE);
}

/* Helper to set the network data event callback. */

void setNetDataEventCallback(std::function<void(json::object)>&& callback) 
{ 
    g_netDataEvent = callback; 
}

/* Helper to resolve a TGID to a textual name. */

std::string resolveRID(uint32_t id)
{
    switch (id) {
    case DMRDEF::WUID_REGI:
        return std::string("DMR REG SVCS");
    case DMRDEF::WUID_STUNI:
        return std::string("DMR MS STUN");
    case DMRDEF::WUID_AUTHI:
        return std::string("DMR AUTH SVCS");
    case DMRDEF::WUID_KILLI:
        return std::string("DMR MS KILL");
    case DMRDEF::WUID_ALLL:
        return std::string("ALL CALL SW");

    case P25DEF::WUID_REG:
        return std::string("REG SVCS");
    case P25DEF::WUID_FNE:
        return std::string("SYS/FNE");
    case P25DEF::WUID_ALL:
        return std::string("ALL CALL");

    case 0:
        return std::string("EXTERNAL/PATCH");
    }

    auto entry = g_ridLookup->find(id);
    if (!entry.radioDefault()) {
        return entry.radioAlias();
    }

    return std::string("UNK");
}

/* Helper to resolve a TGID to a textual name. */

std::string resolveTGID(uint32_t id)
{
    auto entry = g_tidLookup->find(id);
    if (!entry.isInvalid()) {
        return entry.name();
    }

    return std::string("UNK");
}

/* Initializes peer network connectivity. */

bool createPeerNetwork()
{
    yaml::Node fne = g_conf["fne"];

    std::string password = fne["password"].as<std::string>();

    std::string address = fne["masterAddress"].as<std::string>();
    uint16_t port = (uint16_t)fne["masterPort"].as<uint32_t>();
    uint32_t id = fne["peerId"].as<uint32_t>();

    bool encrypted = fne["encrypted"].as<bool>(false);
    std::string key = fne["presharedKey"].as<std::string>();
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
                    char t[4] = {keyPtr[0], keyPtr[1], 0};
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

    std::string identity = fne["identity"].as<std::string>();

    LogInfo("Network Parameters");
    LogInfo("    Peer ID: %u", id);
    LogInfo("    Address: %s", address.c_str());
    LogInfo("    Port: %u", port);

    LogInfo("    Encrypted: %s", encrypted ? "yes" : "no");

    if (id > 999999999U) {
        ::LogError(LOG_HOST, "Network Peer ID cannot be greater then 999999999.");
        return false;
    }

    // initialize networking
    g_network = new PeerNetwork(address, port, 0U, id, password, true, g_debug, true, true, true, true, true, true, false, true, false);
    g_network->setMetadata(identity, 0U, 0U, 0.0F, 0.0F, 0, 0, 0, 0.0F, 0.0F, 0, "");
    g_network->setLookups(g_ridLookup, g_tidLookup);

    ::LogSetNetwork(g_network);

    if (encrypted) {
        g_network->setPresharedKey(presharedKey);
    }

    g_network->enable(true);
    bool ret = g_network->open();
    if (!ret) {
        delete g_network;
        g_network = nullptr;
        LogError(LOG_HOST, "failed to initialize traffic networking for PEER %u", id);
        return false;
    }

    ::LogSetNetwork(g_network);

    return true;
}

/* */

network::PeerNetwork* getNetwork()
{
    return g_network;
}

/* Entry point to network pump update thread. */

void* threadNetworkPump(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
#if defined(_WIN32)
        ::CloseHandle(th->thread);
#else
        ::pthread_detach(th->thread);
#endif // defined(_WIN32)

        std::string threadName("sysview:net-pump");
        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogMessage(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        Timer networkPeerStatusNotify(1000U, 5U);
        networkPeerStatusNotify.start();

        StopWatch stopWatch;
        stopWatch.start();

        while (!g_killed) {
            uint32_t ms = stopWatch.elapsed();
            stopWatch.start();

            // ------------------------------------------------------
            //  -- Network Clocking                               --
            // ------------------------------------------------------

            if (g_network != nullptr) {
                g_network->clock(ms);

                hrc::hrc_t pktTime = hrc::now();

                uint32_t length = 0U;
                bool netReadRet = false;
                UInt8Array dmrBuffer = g_network->readDMR(netReadRet, length);
                if (netReadRet) {
                    using namespace dmr;

                    uint8_t seqNo = dmrBuffer[4U];

                    uint32_t srcId = __GET_UINT16(dmrBuffer, 5U);
                    uint32_t dstId = __GET_UINT16(dmrBuffer, 8U);

                    DMRDEF::FLCO::E flco = (dmrBuffer[15U] & 0x40U) == 0x40U ? DMRDEF::FLCO::PRIVATE : DMRDEF::FLCO::GROUP;

                    uint32_t slotNo = (dmrBuffer[15U] & 0x80U) == 0x80U ? 2U : 1U;

                    DMRDEF::DataType::E dataType = (DMRDEF::DataType::E)(dmrBuffer[15U] & 0x0FU);

                    data::NetData dmrData;
                    dmrData.setSeqNo(seqNo);
                    dmrData.setSlotNo(slotNo);
                    dmrData.setSrcId(srcId);
                    dmrData.setDstId(dstId);
                    dmrData.setFLCO(flco);

                    bool dataSync = (dmrBuffer[15U] & 0x20U) == 0x20U;
                    bool voiceSync = (dmrBuffer[15U] & 0x10U) == 0x10U;

                    if (dataSync) {
                        dmrData.setData(dmrBuffer.get() + 20U);
                        dmrData.setDataType(dataType);
                        dmrData.setN(0U);
                    }
                    else if (voiceSync) {
                        dmrData.setData(dmrBuffer.get() + 20U);
                        dmrData.setDataType(DMRDEF::DataType::VOICE_SYNC);
                        dmrData.setN(0U);
                    }
                    else {
                        uint8_t n = dmrBuffer[15U] & 0x0FU;
                        dmrData.setData(dmrBuffer.get() + 20U);
                        dmrData.setDataType(DMRDEF::DataType::VOICE);
                        dmrData.setN(n);
                    }

                    // is this the end of the call stream?
                    if (dataSync && (dataType == DMRDEF::DataType::TERMINATOR_WITH_LC)) {
                        if (srcId == 0U && dstId == 0U) {
                            LogWarning(LOG_NET, "DMR, invalid TERMINATOR, srcId = %u (%s), dstId = %u (%s)", 
                                srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                        }

                        RxStatus status;
                        auto it = std::find_if(g_dmrStatus.begin(), g_dmrStatus.end(), [&](StatusMapPair x) { return (x.second.dstId == dstId && x.second.slotNo == slotNo); });
                        if (it == g_dmrStatus.end()) {
                            LogError(LOG_NET, "DMR, tried to end call for non-existent call in progress?, srcId = %u (%s), dstId = %u (%s)",
                                srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                        }
                        else {
                            status = it->second;
                        }

                        uint64_t duration = hrc::diff(pktTime, status.callStartTime);

                        if (std::find_if(g_dmrStatus.begin(), g_dmrStatus.end(), [&](StatusMapPair x) { return (x.second.dstId == dstId && x.second.slotNo == slotNo); }) != g_dmrStatus.end()) {
                            g_dmrStatus.erase(dstId);

                            LogMessage(LOG_NET, "DMR, Call End, srcId = %u (%s), dstId = %u (%s), duration = %u",
                                        srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str(), duration / 1000);
                        }
                    }

                    // is this a new call stream?
                    if (dataSync && (dataType == DMRDEF::DataType::VOICE_LC_HEADER)) {
                        if (srcId == 0U && dstId == 0U) {
                            LogWarning(LOG_NET, "DMR, invalid call, srcId = %u (%s), dstId = %u (%s)", 
                                srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                        }

                        auto it = std::find_if(g_dmrStatus.begin(), g_dmrStatus.end(), [&](StatusMapPair x) { return (x.second.dstId == dstId && x.second.slotNo == slotNo); });
                        if (it == g_dmrStatus.end()) {
                            // this is a new call stream
                            RxStatus status = RxStatus();
                            status.callStartTime = pktTime;
                            status.srcId = srcId;
                            status.dstId = dstId;
                            status.slotNo = slotNo;
                            g_dmrStatus[dstId] = status; // this *could* be an issue if a dstId appears on both slots somehow...

                            LogMessage(LOG_NET, "DMR, Call Start, srcId = %u (%s), dstId = %u (%s)", 
                                srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                        }
                    }

                    // are we receiving a CSBK?
                    if (dmrData.getDataType() == DMRDEF::DataType::CSBK) {
                        uint8_t data[DMRDEF::DMR_FRAME_LENGTH_BYTES + 2U];
                        dmrData.getData(data + 2U);

                        std::unique_ptr<lc::CSBK> csbk = lc::csbk::CSBKFactory::createCSBK(data + 2U, DMRDEF::DataType::CSBK);
                        if (csbk != nullptr) {
                            json::object netEvent = json::object();
                            std::string type = "csbk";
                            netEvent["type"].set<std::string>(type);

                            uint8_t slot = dmrData.getSlotNo();
                            netEvent["slot"].set<uint8_t>(slot);
                            uint8_t csbko = csbk->getCSBKO();
                            netEvent["opcode"].set<uint8_t>(csbko);
                            std::string desc = csbk->toString();
                            netEvent["desc"].set<std::string>(desc);

                            switch (csbk->getCSBKO()) {
                            case DMRDEF::CSBKO::BROADCAST:
                                {
                                    lc::csbk::CSBK_BROADCAST* osp = static_cast<lc::csbk::CSBK_BROADCAST*>(csbk.get());
                                    if (osp->getAnncType() == DMRDEF::BroadcastAnncType::ANN_WD_TSCC) {
                                        LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, %s, sysId = $%03X, chNo = %u", dmrData.getSlotNo(), csbk->toString().c_str(),
                                            osp->getSystemId(), osp->getLogicalCh1());

                                        // generate a net event for this
                                        if (g_netDataEvent != nullptr) {
                                            uint32_t sysId = osp->getSystemId();
                                            uint16_t logicalCh1 = osp->getLogicalCh1();
                                            netEvent["sysId"].set<uint32_t>(sysId);
                                            netEvent["chNo"].set<uint16_t>(logicalCh1);

                                            g_netDataEvent(netEvent);
                                        }
                                    }
                                }
                                break;
                            default:
                                {
                                    LogMessage(LOG_NET, "DMR Slot %u, DT_CSBK, %s, srcId = %u (%s), dstId = %u (%s)", dmrData.getSlotNo(), csbk->toString().c_str(), 
                                        srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());

                                    // generate a net event for this
                                    if (g_netDataEvent != nullptr) {
                                        std::string resolvedSrc = resolveRID(srcId);
                                        std::string resolvedDst = resolveTGID(dstId);
                                        netEvent["srcId"].set<uint32_t>(srcId);
                                        netEvent["srcStr"].set<std::string>(resolvedSrc);
                                        netEvent["dstId"].set<uint32_t>(dstId);
                                        netEvent["dstStr"].set<std::string>(resolvedDst);

                                        g_netDataEvent(netEvent);
                                    }
                                }
                                break;
                            }
                        }
                    }

                    if (g_debug)
                        LogMessage(LOG_NET, "DMR, slotNo = %u, seqNo = %u, flco = $%02X, srcId = %u, dstId = %u, len = %u", slotNo, seqNo, flco, srcId, dstId, length);
                }

                UInt8Array p25Buffer = g_network->readP25(netReadRet, length);
                if (netReadRet) {
                    using namespace p25;

                    uint8_t duid = p25Buffer[22U];
                    uint8_t MFId = p25Buffer[15U];

                    // process raw P25 data bytes
                    UInt8Array data;
                    uint8_t frameLength = p25Buffer[23U];
                    if (duid == P25DEF::DUID::PDU) {
                        frameLength = length;
                        data = std::unique_ptr<uint8_t[]>(new uint8_t[length]);
                        ::memset(data.get(), 0x00U, length);
                        ::memcpy(data.get(), p25Buffer.get(), length);
                    }
                    else {
                        if (frameLength <= 24) {
                            data = std::unique_ptr<uint8_t[]>(new uint8_t[frameLength]);
                            ::memset(data.get(), 0x00U, frameLength);
                        }
                        else {
                            data = std::unique_ptr<uint8_t[]>(new uint8_t[frameLength]);
                            ::memset(data.get(), 0x00U, frameLength);
                            ::memcpy(data.get(), p25Buffer.get() + 24U, frameLength);
                        }
                    }

                    uint8_t lco = p25Buffer[4U];

                    uint32_t srcId = __GET_UINT16(p25Buffer, 5U);
                    uint32_t dstId = __GET_UINT16(p25Buffer, 8U);

                    uint32_t sysId = (p25Buffer[11U] << 8) | (p25Buffer[12U] << 0);
                    uint32_t netId = __GET_UINT16(p25Buffer, 16U);

                    // log call status
                    if (duid != P25DEF::DUID::TSDU && duid != P25DEF::DUID::PDU) {
                        // is this the end of the call stream?
                        if ((duid == P25DEF::DUID::TDU) || (duid == P25DEF::DUID::TDULC)) {
                            if (srcId == 0U && dstId == 0U) {
                                LogWarning(LOG_NET, "P25, invalid TDU, srcId = %u (%s), dstId = %u (%s)", 
                                    srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                            }

                            RxStatus status = g_p25Status[dstId];
                            uint64_t duration = hrc::diff(pktTime, status.callStartTime);

                            if (std::find_if(g_p25Status.begin(), g_p25Status.end(), [&](StatusMapPair x) { return x.second.dstId == dstId; }) != g_p25Status.end()) {
                                g_p25Status.erase(dstId);

                                LogMessage(LOG_NET, "P25, Call End, srcId = %u (%s), dstId = %u (%s), duration = %u",
                                    srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str(), duration / 1000);
                            }
                        }

                        // is this a new call stream?
                        if ((duid != P25DEF::DUID::TDU) && (duid != P25DEF::DUID::TDULC)) {
                            if (srcId == 0U && dstId == 0U) {
                                LogWarning(LOG_NET, "P25, invalid call, srcId = %u (%s), dstId = %u (%s)", 
                                    srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                            }

                            auto it = std::find_if(g_p25Status.begin(), g_p25Status.end(), [&](StatusMapPair x) { return x.second.dstId == dstId; });
                            if (it == g_p25Status.end()) {
                                // this is a new call stream
                                RxStatus status = RxStatus();
                                status.callStartTime = pktTime;
                                status.srcId = srcId;
                                status.dstId = dstId;
                                g_p25Status[dstId] = status;

                                LogMessage(LOG_NET, "P25, Call Start, srcId = %u (%s), dstId = %u (%s)", 
                                    srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                            }
                        }
                    }

                    switch (duid) {
                    case P25DEF::DUID::TDU:
                    case P25DEF::DUID::TDULC:
                        if (duid == P25DEF::DUID::TDU) {
                            LogMessage(LOG_NET, P25_TDU_STR ", srcId = %u (%s), dstId = %u (%s)", 
                                    srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                        }
                        else {
                            std::unique_ptr<lc::TDULC> tdulc = lc::tdulc::TDULCFactory::createTDULC(data.get());
                            if (tdulc == nullptr) {
                                LogWarning(LOG_NET, P25_TDULC_STR ", undecodable TDULC");
                            }
                            else {
                                LogMessage(LOG_NET, P25_TDULC_STR ", srcId = %u (%s), dstId = %u (%s)", 
                                    srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                            }
                        }
                        break;

                    case P25DEF::DUID::TSDU:
                        std::unique_ptr<lc::TSBK> tsbk = lc::tsbk::TSBKFactory::createTSBK(data.get());
                        if (tsbk == nullptr) {
                            LogWarning(LOG_NET, P25_TSDU_STR ", undecodable TSBK");
                        }
                        else {
                            json::object netEvent = json::object();
                            std::string type = "tsbk";
                            netEvent["type"].set<std::string>(type);

                            uint8_t lco = tsbk->getLCO();
                            netEvent["opcode"].set<uint8_t>(lco);
                            std::string desc = tsbk->toString();
                            netEvent["desc"].set<std::string>(desc);

                            switch (tsbk->getLCO()) {
                                case P25DEF::TSBKO::IOSP_GRP_VCH:
                                case P25DEF::TSBKO::IOSP_UU_VCH:
                                {
                                    LogMessage(LOG_NET, P25_TSDU_STR ", %s, emerg = %u, encrypt = %u, prio = %u, chNo = %u, srcId = %u (%s), dstId = %u (%s)",
                                        tsbk->toString(true).c_str(), tsbk->getEmergency(), tsbk->getEncrypted(), tsbk->getPriority(), tsbk->getGrpVchNo(), 
                                        srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());

                                    // generate a net event for this
                                    if (g_netDataEvent != nullptr) {
                                        bool emerg = tsbk->getEmergency();
                                        netEvent["emerg"].set<bool>(emerg);
                                        bool encry = tsbk->getEncrypted();
                                        netEvent["encry"].set<bool>(encry);
                                        uint8_t prio = tsbk->getPriority();
                                        netEvent["prio"].set<uint8_t>(prio);
                                        uint32_t chNo = tsbk->getGrpVchNo();
                                        netEvent["chNo"].set<uint32_t>(chNo);

                                        std::string resolvedSrc = resolveRID(srcId);
                                        std::string resolvedDst = resolveTGID(dstId);
                                        netEvent["srcId"].set<uint32_t>(srcId);
                                        netEvent["srcStr"].set<std::string>(resolvedSrc);
                                        netEvent["dstId"].set<uint32_t>(dstId);
                                        netEvent["dstStr"].set<std::string>(resolvedDst);

                                        g_netDataEvent(netEvent);
                                    }
                                }
                                break;
                                case P25DEF::TSBKO::IOSP_UU_ANS:
                                {
                                    lc::tsbk::IOSP_UU_ANS* iosp = static_cast<lc::tsbk::IOSP_UU_ANS*>(tsbk.get());
                                    if (iosp->getResponse() > 0U) {
                                        LogMessage(LOG_NET, P25_TSDU_STR ", %s, response = $%02X, srcId = %u (%s), dstId = %u (%s)",
                                            tsbk->toString(true).c_str(), iosp->getResponse(),
                                            srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());

                                        // generate a net event for this
                                        if (g_netDataEvent != nullptr) {
                                            uint8_t response = iosp->getResponse();
                                            netEvent["response"].set<uint8_t>(response);

                                            std::string resolvedSrc = resolveRID(srcId);
                                            std::string resolvedDst = resolveTGID(dstId);
                                            netEvent["srcId"].set<uint32_t>(srcId);
                                            netEvent["srcStr"].set<std::string>(resolvedSrc);
                                            netEvent["dstId"].set<uint32_t>(dstId);
                                            netEvent["dstStr"].set<std::string>(resolvedDst);

                                            g_netDataEvent(netEvent);
                                        }
                                    }
                                }
                                break;
                                case P25DEF::TSBKO::IOSP_STS_UPDT:
                                {
                                    lc::tsbk::IOSP_STS_UPDT* iosp = static_cast<lc::tsbk::IOSP_STS_UPDT*>(tsbk.get());
                                    LogMessage(LOG_NET, P25_TSDU_STR ", %s, status = $%02X, srcId = %u (%s)",
                                        tsbk->toString(true).c_str(), iosp->getStatus(), srcId, resolveRID(srcId).c_str());

                                    // generate a net event for this
                                    if (g_netDataEvent != nullptr) {
                                        uint8_t status = iosp->getStatus();
                                        netEvent["status"].set<uint8_t>(status);

                                        std::string resolvedSrc = resolveRID(srcId);
                                        std::string resolvedDst = resolveTGID(dstId);
                                        netEvent["srcId"].set<uint32_t>(srcId);
                                        netEvent["srcStr"].set<std::string>(resolvedSrc);
                                        netEvent["dstId"].set<uint32_t>(dstId);
                                        netEvent["dstStr"].set<std::string>(resolvedDst);

                                        g_netDataEvent(netEvent);
                                    }
                                }
                                break;
                                case P25DEF::TSBKO::IOSP_MSG_UPDT:
                                {
                                    lc::tsbk::IOSP_MSG_UPDT* iosp = static_cast<lc::tsbk::IOSP_MSG_UPDT*>(tsbk.get());
                                    LogMessage(LOG_NET, P25_TSDU_STR ", %s, message = $%02X, srcId = %u (%s), dstId = %u (%s)",
                                        tsbk->toString(true).c_str(), iosp->getMessage(), 
                                        srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());

                                    // generate a net event for this
                                    if (g_netDataEvent != nullptr) {
                                        uint8_t message = iosp->getMessage();
                                        netEvent["message"].set<uint8_t>(message);

                                        std::string resolvedSrc = resolveRID(srcId);
                                        std::string resolvedDst = resolveTGID(dstId);
                                        netEvent["srcId"].set<uint32_t>(srcId);
                                        netEvent["srcStr"].set<std::string>(resolvedSrc);
                                        netEvent["dstId"].set<uint32_t>(dstId);
                                        netEvent["dstStr"].set<std::string>(resolvedDst);

                                        g_netDataEvent(netEvent);
                                    }
                                }
                                break;
                                case P25DEF::TSBKO::IOSP_RAD_MON:
                                {
                                    //lc::tsbk::IOSP_RAD_MON* iosp = static_cast<lc::tsbk::IOSP_RAD_MON*>(tsbk.get());
                                    LogMessage(LOG_NET, P25_TSDU_STR ", %s, srcId = %u (%s), dstId = %u (%s)", tsbk->toString(true).c_str(), 
                                        srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());

                                    // generate a net event for this
                                    if (g_netDataEvent != nullptr) {
                                        std::string resolvedSrc = resolveRID(srcId);
                                        std::string resolvedDst = resolveTGID(dstId);
                                        netEvent["srcId"].set<uint32_t>(srcId);
                                        netEvent["srcStr"].set<std::string>(resolvedSrc);
                                        netEvent["dstId"].set<uint32_t>(dstId);
                                        netEvent["dstStr"].set<std::string>(resolvedDst);

                                        g_netDataEvent(netEvent);
                                    }
                                }
                                break;
                                case P25DEF::TSBKO::IOSP_CALL_ALRT:
                                {
                                    LogMessage(LOG_NET, P25_TSDU_STR ", %s, srcId = %u (%s), dstId = %u (%s)", tsbk->toString(true).c_str(), 
                                        srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                                
                                    // generate a net event for this
                                    if (g_netDataEvent != nullptr) {
                                        std::string resolvedSrc = resolveRID(srcId);
                                        std::string resolvedDst = resolveTGID(dstId);
                                        netEvent["srcId"].set<uint32_t>(srcId);
                                        netEvent["srcStr"].set<std::string>(resolvedSrc);
                                        netEvent["dstId"].set<uint32_t>(dstId);
                                        netEvent["dstStr"].set<std::string>(resolvedDst);

                                        g_netDataEvent(netEvent);
                                    }
                                }
                                break;
                                case P25DEF::TSBKO::IOSP_ACK_RSP:
                                {
                                    lc::tsbk::IOSP_ACK_RSP* iosp = static_cast<lc::tsbk::IOSP_ACK_RSP*>(tsbk.get());
                                    LogMessage(LOG_NET, P25_TSDU_STR ", %s, AIV = %u, serviceType = $%02X, srcId = %u (%s), dstId = %u (%s)",
                                        tsbk->toString(true).c_str(), iosp->getAIV(), iosp->getService(), 
                                        srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());

                                    // generate a net event for this
                                    if (g_netDataEvent != nullptr) {
                                        uint8_t service = iosp->getService();
                                        netEvent["service"].set<uint8_t>(service);

                                        std::string resolvedSrc = resolveRID(srcId);
                                        std::string resolvedDst = resolveTGID(dstId);
                                        netEvent["srcId"].set<uint32_t>(srcId);
                                        netEvent["srcStr"].set<std::string>(resolvedSrc);
                                        netEvent["dstId"].set<uint32_t>(dstId);
                                        netEvent["dstStr"].set<std::string>(resolvedDst);

                                        g_netDataEvent(netEvent);
                                    }
                                }
                                break;
                                case P25DEF::TSBKO::IOSP_EXT_FNCT:
                                {
                                    lc::tsbk::IOSP_EXT_FNCT* iosp = static_cast<lc::tsbk::IOSP_EXT_FNCT*>(tsbk.get());
                                    LogMessage(LOG_NET, P25_TSDU_STR ", %s, serviceType = $%02X, arg = %u, tgt = %u",
                                        tsbk->toString(true).c_str(), iosp->getService(), srcId, dstId);

                                    // generate a net event for this
                                    if (g_netDataEvent != nullptr) {
                                        uint8_t service = iosp->getService();
                                        netEvent["service"].set<uint8_t>(service);

                                        netEvent["arg"].set<uint32_t>(srcId);

                                        std::string resolvedDst = resolveRID(dstId);
                                        netEvent["dstId"].set<uint32_t>(dstId);
                                        netEvent["dstStr"].set<std::string>(resolvedDst);

                                        g_netDataEvent(netEvent);
                                    }
                                }
                                break;
                                case P25DEF::TSBKO::ISP_EMERG_ALRM_REQ:
                                {
                                    // non-emergency mode is a TSBKO::OSP_DENY_RSP
                                    if (!tsbk->getEmergency()) {
                                        lc::tsbk::OSP_DENY_RSP* osp = static_cast<lc::tsbk::OSP_DENY_RSP*>(tsbk.get());
                                        LogMessage(LOG_NET, P25_TSDU_STR ", %s, AIV = %u, reason = $%02X, srcId = %u (%s), dstId = %u (%s)",
                                            osp->toString().c_str(), osp->getAIV(), osp->getResponse(), 
                                            osp->getSrcId(), resolveRID(osp->getSrcId()).c_str(), osp->getDstId(), resolveTGID(osp->getDstId()).c_str());

                                        // generate a net event for this
                                        if (g_netDataEvent != nullptr) {
                                            uint8_t reason = osp->getResponse();
                                            netEvent["reason"].set<uint8_t>(reason);

                                            std::string resolvedSrc = resolveRID(srcId);
                                            std::string resolvedDst = resolveTGID(dstId);
                                            netEvent["srcId"].set<uint32_t>(srcId);
                                            netEvent["srcStr"].set<std::string>(resolvedSrc);
                                            netEvent["dstId"].set<uint32_t>(dstId);
                                            netEvent["dstStr"].set<std::string>(resolvedDst);

                                            g_netDataEvent(netEvent);
                                        }
                                    } else {
                                        LogMessage(LOG_NET, P25_TSDU_STR ", %s, srcId = %u (%s), dstId = %u (%s)", tsbk->toString().c_str(), 
                                            srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());

                                        // generate a net event for this
                                        if (g_netDataEvent != nullptr) {
                                            std::string resolvedSrc = resolveRID(srcId);
                                            std::string resolvedDst = resolveTGID(dstId);
                                            netEvent["srcId"].set<uint32_t>(srcId);
                                            netEvent["srcStr"].set<std::string>(resolvedSrc);
                                            netEvent["dstId"].set<uint32_t>(dstId);
                                            netEvent["dstStr"].set<std::string>(resolvedDst);

                                            g_netDataEvent(netEvent);
                                        }
                                    }
                                }
                                break;
                                case P25DEF::TSBKO::IOSP_GRP_AFF:
                                {
                                    lc::tsbk::OSP_GRP_AFF* iosp = static_cast<lc::tsbk::OSP_GRP_AFF*>(tsbk.get());
                                    LogMessage(LOG_NET, P25_TSDU_STR ", %s, anncId = %u (%s), srcId = %u (%s), dstId = %u (%s), response = $%02X", tsbk->toString().c_str(),
                                            iosp->getAnnounceGroup(), resolveTGID(iosp->getAnnounceGroup()).c_str(),
                                            srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str(),
                                            iosp->getResponse());

                                    // generate a net event for this
                                    if (g_netDataEvent != nullptr) {
                                        uint32_t anncId = iosp->getAnnounceGroup();
                                        netEvent["anncId"].set<uint32_t>(anncId);
                                        uint8_t resp = iosp->getResponse();
                                        netEvent["response"].set<uint8_t>(resp);

                                        std::string resolvedSrc = resolveRID(srcId);
                                        std::string resolvedDst = resolveTGID(dstId);
                                        netEvent["srcId"].set<uint32_t>(srcId);
                                        netEvent["srcStr"].set<std::string>(resolvedSrc);
                                        netEvent["dstId"].set<uint32_t>(dstId);
                                        netEvent["dstStr"].set<std::string>(resolvedDst);

                                        g_netDataEvent(netEvent);
                                    }
                                }
                                break;
                                case P25DEF::TSBKO::OSP_U_DEREG_ACK:
                                {
                                    //lc::tsbk::OSP_U_DEREG_ACK* iosp = static_cast<lc::tsbk::OSP_U_DEREG_ACK*>(tsbk.get());
                                    LogMessage(LOG_NET, P25_TSDU_STR ", %s, srcId = %u (%s)",
                                        tsbk->toString(true).c_str(), srcId, resolveRID(srcId).c_str());

                                    // generate a net event for this
                                    if (g_netDataEvent != nullptr) {
                                        std::string resolvedSrc = resolveRID(srcId);
                                        netEvent["srcId"].set<uint32_t>(srcId);
                                        netEvent["srcStr"].set<std::string>(resolvedSrc);

                                        g_netDataEvent(netEvent);
                                    }
                                }
                                break;
                                case P25DEF::TSBKO::OSP_LOC_REG_RSP:
                                {
                                    lc::tsbk::OSP_LOC_REG_RSP* osp = static_cast<lc::tsbk::OSP_LOC_REG_RSP*>(tsbk.get());
                                    LogMessage(LOG_NET, P25_TSDU_STR ", %s, srcId = %u (%s), dstId = %u (%s)", osp->toString().c_str(), 
                                        srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());

                                    // generate a net event for this
                                    if (g_netDataEvent != nullptr) {
                                        std::string resolvedSrc = resolveRID(srcId);
                                        std::string resolvedDst = resolveTGID(dstId);
                                        netEvent["srcId"].set<uint32_t>(srcId);
                                        netEvent["srcStr"].set<std::string>(resolvedSrc);
                                        netEvent["dstId"].set<uint32_t>(dstId);
                                        netEvent["dstStr"].set<std::string>(resolvedDst);

                                        g_netDataEvent(netEvent);
                                    }
                                }
                                break;
                                case P25DEF::TSBKO::OSP_ADJ_STS_BCAST:
                                {
                                    lc::tsbk::OSP_ADJ_STS_BCAST* osp = static_cast<lc::tsbk::OSP_ADJ_STS_BCAST*>(tsbk.get());
                                    LogMessage(LOG_NET, P25_TSDU_STR ", %s, sysId = $%03X, rfss = $%02X, site = $%02X, chId = %u, chNo = %u, svcClass = $%02X", tsbk->toString().c_str(),
                                        osp->getAdjSiteSysId(), osp->getAdjSiteRFSSId(), osp->getAdjSiteId(), osp->getAdjSiteChnId(), osp->getAdjSiteChnNo(), osp->getAdjSiteSvcClass());

                                    // generate a net event for this
                                    if (g_netDataEvent != nullptr) {
                                        uint32_t sysId = osp->getAdjSiteSysId();
                                        netEvent["sysId"].set<uint32_t>(sysId);
                                        uint8_t rfssId = osp->getAdjSiteRFSSId();
                                        netEvent["rfssId"].set<uint8_t>(rfssId);
                                        uint8_t siteId = osp->getAdjSiteId();
                                        netEvent["siteId"].set<uint8_t>(siteId);
                                        uint8_t chId = osp->getAdjSiteChnId();
                                        netEvent["chId"].set<uint8_t>(chId);
                                        uint32_t chNo = osp->getAdjSiteChnNo();
                                        netEvent["chNo"].set<uint32_t>(chNo);
                                        uint8_t svcClass = osp->getAdjSiteSvcClass();
                                        netEvent["svcClass"].set<uint8_t>(svcClass);

                                        g_netDataEvent(netEvent);
                                    }
                                }
                                break;
                                default:
                                    {
                                        LogMessage(LOG_NET, P25_TSDU_STR ", %s, srcId = %u (%s), dstId = %u (%s)", tsbk->toString().c_str(), 
                                            srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());

                                        // generate a net event for this
                                        if (g_netDataEvent != nullptr) {
                                            std::string resolvedSrc = resolveRID(srcId);
                                            std::string resolvedDst = resolveTGID(dstId);
                                            netEvent["srcId"].set<uint32_t>(srcId);
                                            netEvent["srcStr"].set<std::string>(resolvedSrc);
                                            netEvent["dstId"].set<uint32_t>(dstId);
                                            netEvent["dstStr"].set<std::string>(resolvedDst);

                                            g_netDataEvent(netEvent);
                                        }
                                    }
                                    break;
                            }
                        }
                        break;
                    }

                    if (g_debug)
                        LogMessage(LOG_NET, "P25, duid = $%02X, lco = $%02X, MFId = $%02X, srcId = %u, dstId = %u, len = %u", duid, lco, MFId, srcId, dstId, length);
                }

                UInt8Array nxdnBuffer = g_network->readNXDN(netReadRet, length);
                if (netReadRet) {
                    using namespace nxdn;

                    uint8_t messageType = nxdnBuffer[4U];

                    uint32_t srcId = __GET_UINT16(nxdnBuffer, 5U);
                    uint32_t dstId = __GET_UINT16(nxdnBuffer, 8U);

                    lc::RTCH lc;

                    lc.setMessageType(messageType);
                    lc.setSrcId((uint16_t)srcId & 0xFFFFU);
                    lc.setDstId((uint16_t)dstId & 0xFFFFU);

                    bool group = (nxdnBuffer[15U] & 0x40U) == 0x40U ? false : true;
                    lc.setGroup(group);

                    // specifically only check the following logic for end of call, voice or data frames
                    if ((messageType == NXDDEF::MessageType::RTCH_TX_REL || messageType == NXDDEF::MessageType::RTCH_TX_REL_EX) ||
                        (messageType == NXDDEF::MessageType::RTCH_VCALL || messageType == NXDDEF::MessageType::RTCH_DCALL_HDR ||
                        messageType == NXDDEF::MessageType::RTCH_DCALL_DATA)) {
                        // is this the end of the call stream?
                        if (messageType == NXDDEF::MessageType::RTCH_TX_REL || messageType == NXDDEF::MessageType::RTCH_TX_REL_EX) {
                            if (srcId == 0U && dstId == 0U) {
                                LogWarning(LOG_NET, "NXDN, invalid TX_REL, srcId = %u (%s), dstId = %u (%s)", 
                                    srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                            }

                            RxStatus status = g_nxdnStatus[dstId];
                            uint64_t duration = hrc::diff(pktTime, status.callStartTime);

                            if (std::find_if(g_nxdnStatus.begin(), g_nxdnStatus.end(), [&](StatusMapPair x) { return x.second.dstId == dstId; }) != g_nxdnStatus.end()) {
                                g_nxdnStatus.erase(dstId);

                                LogMessage(LOG_NET, "NXDN, Call End, srcId = %u (%s), dstId = %u (%s), duration = %u",
                                    srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str(), duration / 1000);
                            }
                        }

                        // is this a new call stream?
                        if ((messageType != NXDDEF::MessageType::RTCH_TX_REL && messageType != NXDDEF::MessageType::RTCH_TX_REL_EX)) {
                            if (srcId == 0U && dstId == 0U) {
                                LogWarning(LOG_NET, "NXDN, invalid call, srcId = %u (%s), dstId = %u (%s)", 
                                    srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                            }

                            auto it = std::find_if(g_nxdnStatus.begin(), g_nxdnStatus.end(), [&](StatusMapPair x) { return x.second.dstId == dstId; });
                            if (it == g_nxdnStatus.end()) {
                                // this is a new call stream
                                RxStatus status = RxStatus();
                                status.callStartTime = pktTime;
                                status.srcId = srcId;
                                status.dstId = dstId;
                                g_nxdnStatus[dstId] = status;

                                LogMessage(LOG_NET, "NXDN, Call Start, srcId = %u (%s), dstId = %u (%s)", 
                                    srcId, resolveRID(srcId).c_str(), dstId, resolveTGID(dstId).c_str());
                            }
                        }
                    }

                    if (g_debug)
                        LogMessage(LOG_NET, "NXDN, messageType = $%02X, srcId = %u, dstId = %u, len = %u", messageType, srcId, dstId, length);
                }
            }

            if (ms < 2U)
                Thread::sleep(1U);
        }

        LogMessage(LOG_HOST, "[STOP] %s", threadName.c_str());
        delete th;
    }

    return nullptr;
}

/* Helper to pring usage the command line arguments. (And optionally an error.) */

void usage(const char* message, const char* arg)
{
    ::fprintf(stdout, __PROG_NAME__ " %s (built %s)\r\n", __VER__, __BUILD__);
    ::fprintf(stdout, "Copyright (c) 2017-2025 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\n");
    ::fprintf(stdout, "Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others\n\n");
    if (message != nullptr) {
        ::fprintf(stderr, "%s: ", g_progExe.c_str());
        ::fprintf(stderr, message, arg);
        ::fprintf(stderr, "\n\n");
    }

    ::fprintf(stdout, 
        "usage: %s [-dvh]"
        "[--hide-log]"
        "[-c <configuration file>]"
#if !defined(NO_WEBSOCKETS)
        "[-f][-ws]"
#endif // !defined(NO_WEBSOCKETS)
        "\n\n"
        "  -d                          enable debug\n"
        "  -v                          show version information\n"
        "  -h                          show this screen\n"
        "\n"
        "  --hide-log                  hide interactive logging window on startup\n"
        "\n"
        "  -c <file>                   specifies the system view configuration file to use\n"
        "\n"
#if !defined(NO_WEBSOCKETS)
        "  -f                          foreground mode\n"
        "  -ws                         websocket mode\n"
        "\n"
#endif // !defined(NO_WEBSOCKETS)
        "  --                          stop handling options\n",
        g_progExe.c_str());

    exit(EXIT_FAILURE);
}

/* Helper to validate the command line arguments. */

int checkArgs(int argc, char* argv[])
{
    int i, p = 0;

    // iterate through arguments
    for (i = 1; i <= argc; i++)
    {
        if (argv[i] == nullptr) {
            break;
        }

        if (*argv[i] != '-') {
            continue;
        }
        else if (IS("--")) {
            ++p;
            break;
        }
        else if (IS("-c")) {
            if (argc-- <= 0)
                usage("error: %s", "must specify the monitor configuration file to use");
            g_iniFile = std::string(argv[++i]);

            if (g_iniFile.empty())
                usage("error: %s", "monitor configuration file cannot be blank!");

            p += 2;
        }
        else if (IS("--hide-log")) {
            ++p;
            g_hideLoggingWnd = true;
        }
#if !defined(NO_WEBSOCKETS)
        else if (IS("-f")) {
            ++p;
            g_foreground = true;
        }
        else if (IS("-ws")) {
            ++p;
            g_webSocketMode = true;
        }
#endif // !defined(NO_WEBSOCKETS)
        else if (IS("-d")) {
            ++p;
            g_debug = true;
        }
        else if (IS("-v")) {
            ::fprintf(stdout, __PROG_NAME__ " %s (built %s)\r\n", __VER__, __BUILD__);
            ::fprintf(stdout, "Copyright (c) 2017-2025 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\n");
            ::fprintf(stdout, "Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others\n\n");
            if (argc == 2)
                exit(EXIT_SUCCESS);
        }
        else if (IS("-h")) {
            usage(nullptr, nullptr);
            if (argc == 2)
                exit(EXIT_SUCCESS);
        }
        else {
            usage("unrecognized option `%s'", argv[i]);
        }
    }

    if (p < 0 || p > argc) {
        p = 0;
    }

    return ++p;
}

// ---------------------------------------------------------------------------
//  Program Entry Point
// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
    if (argv[0] != nullptr && *argv[0] != 0)
        g_progExe = std::string(argv[0]);

    if (argc > 1) {
        // check arguments
        int i = checkArgs(argc, argv);
        if (i < argc) {
            argc -= i;
            argv += i;
        }
        else {
            argc--;
            argv++;
        }
    }

    // initialize system logging
    bool ret = ::LogInitialise("", "", 0U, 1U);
    if (!ret) {
        ::fprintf(stderr, "unable to open the log file\n");
        return 1;
    }

    ::LogInfo(__PROG_NAME__ " " __VER__ " (built " __BUILD__ ")\r\n" \
        "Copyright (c) 2017-2025 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\r\n" \
        "Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others\r\n" \
        ">> FNE System View\r\n");

    try {
        ret = yaml::Parse(g_conf, g_iniFile.c_str());
        if (!ret) {
            ::fatal("cannot read the configuration file, %s\n", g_iniFile.c_str());
        }
    }
    catch (yaml::OperationException const& e) {
        ::fatal("cannot read the configuration file - %s (%s)", g_iniFile.c_str(), e.message());
    }

    /** Network Thread */
    if (!Thread::runAsThread(nullptr, threadNetworkPump))
        return EXIT_FAILURE;

    // setup the finalcut tui
    SysViewApplication* app = nullptr;
    SysViewMainWnd* wnd = nullptr;
    if (!g_webSocketMode) {
        app = new SysViewApplication(argc, argv);
        g_app = app;

        wnd = new SysViewMainWnd(app);
        finalcut::FWidget::setMainWidget(wnd);
    } else {
        // in WebSocket mode install signal handlers
        ::signal(SIGINT, sigHandler);
        ::signal(SIGTERM, sigHandler);
        ::signal(SIGHUP, sigHandler);
    }

    // try to load bandplan identity table
    std::string idenLookupFile = g_conf["iden_table"]["file"].as<std::string>();
    uint32_t idenReloadTime = g_conf["iden_table"]["time"].as<uint32_t>(0U);

    if (idenLookupFile.length() <= 0U) {
        ::LogError(LOG_HOST, "No bandplan identity table? This must be defined!");
        return 1;
    }

    if (!g_webSocketMode)
        g_logDisplayLevel = 0U;

    // try to load radio IDs table
    std::string ridLookupFile = g_conf["radio_id"]["file"].as<std::string>();
    uint32_t ridReloadTime = g_conf["radio_id"]["time"].as<uint32_t>(0U);

    LogInfo("Radio Id Lookups");
    LogInfo("    File: %s", ridLookupFile.length() > 0U ? ridLookupFile.c_str() : "None");
    if (ridReloadTime > 0U)
        LogInfo("    Reload: %u mins", ridReloadTime);

    g_ridLookup = new RadioIdLookup(ridLookupFile, ridReloadTime, false);
    g_ridLookup->read();

    // try to load talkgroup IDs table
    std::string tidLookupFile = g_conf["talkgroup_rules"]["file"].as<std::string>();
    uint32_t tidReloadTime = g_conf["talkgroup_rules"]["time"].as<uint32_t>(0U);

    LogInfo("Talkgroup Rule Lookups");
    LogInfo("    File: %s", tidLookupFile.length() > 0U ? tidLookupFile.c_str() : "None");
    if (tidReloadTime > 0U)
        LogInfo("    Reload: %u mins", tidReloadTime);

    g_tidLookup = new TalkgroupRulesLookup(tidLookupFile, tidReloadTime, false);
    g_tidLookup->read();

    LogInfo("Iden Table Lookups");
    LogInfo("    File: %s", idenLookupFile.length() > 0U ? idenLookupFile.c_str() : "None");
    if (idenReloadTime > 0U)
        LogInfo("    Reload: %u mins", idenReloadTime);

    g_idenTable = new IdenTableLookup(idenLookupFile, idenReloadTime);
    g_idenTable->read();

    int _errno = 0U;
    if (!g_webSocketMode) {
        // show and start the application
        wnd->show();

        finalcut::FApplication::setColorTheme<dvmColorTheme>();
        app->resetColors();
        app->redraw();
        
        _errno = app->exec();

        if (wnd != nullptr)
            delete wnd;
        if (app != nullptr)
            delete app;
    } else {
#if !defined(NO_WEBSOCKETS)
        ::LogFinalise(); // HostWS will reinitialize logging after this point...

        HostWS* host = new HostWS(g_iniFile);
        _errno = host->run();
        delete host;
#endif // !defined(NO_WEBSOCKETS)
    }

    g_logDisplayLevel = 1U;

    g_killed = true;
    if (g_network != nullptr) {
        g_network->close();
        delete g_network;
    }

    ::LogFinalise();
    return _errno;
}
