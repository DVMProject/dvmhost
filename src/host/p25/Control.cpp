// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016,2017,2018 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/p25/P25Defines.h"
#include "common/p25/acl/AccessControl.h"
#include "common/p25/P25Utils.h"
#include "common/p25/Sync.h"
#include "common/AESCrypto.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "p25/Control.h"
#include "modem/ModemV24.h"
#include "ActivityLog.h"
#include "HostMain.h"
#include "Host.h"

using namespace p25::packet;
using namespace p25::defines;
using namespace p25;

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint8_t MAX_SYNC_BYTES_ERRS = 4U;
const uint32_t TSBK_PCH_CCH_CNT = 6U;
const uint32_t MAX_PREAMBLE_TDU_CNT = 64U;

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::mutex Control::m_queueLock;
std::mutex Control::m_activeTGLock;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the Control class. */

Control::Control(bool authoritative, uint32_t nac, uint32_t callHang, uint32_t queueSize, modem::Modem* modem, network::Network* network,
    uint32_t timeout, uint32_t tgHang, bool duplex, ::lookups::ChannelLookup* chLookup, ::lookups::RadioIdLookup* ridLookup,
    ::lookups::TalkgroupRulesLookup* tidLookup, ::lookups::IdenTableLookup* idenTable, ::lookups::RSSIInterpolator* rssiMapper,
    bool dumpPDUData, bool repeatPDU, bool dumpTSBKData, bool debug, bool verbose) :
    m_voice(nullptr),
    m_data(nullptr),
    m_control(nullptr),
    m_authoritative(authoritative),
    m_supervisor(false),
    m_nac(nac),
    m_txNAC(nac),
    m_timeout(timeout),
    m_modem(modem),
    m_isModemDFSI(false),
    m_network(network),
    m_inhibitUnauth(false),
    m_legacyGroupGrnt(true),
    m_legacyGroupReg(false),
    m_duplex(duplex),
    m_enableControl(false),
    m_dedicatedControl(false),
    m_voiceOnControl(false),
    m_ackTSBKRequests(true),
    m_disableNetworkGrant(false),
    m_disableNetworkHDU(false),
    m_allowExplicitSourceId(true),
    m_convNetGrantDemand(false),
    m_sndcpSupport(false),
    m_ignoreAffiliationCheck(false),
    m_demandUnitRegForRefusedAff(true),
    m_idenTable(idenTable),
    m_ridLookup(ridLookup),
    m_tidLookup(tidLookup),
    m_affiliations(nullptr),
    m_controlChData(),
    m_idenEntry(),
    m_activeTG(),
    m_txImmQueue(queueSize, "P25 Imm Frame"),
    m_txQueue(queueSize, "P25 Frame"),
    m_rfState(RS_RF_LISTENING),
    m_rfLastDstId(0U),
    m_rfLastSrcId(0U),
    m_netState(RS_NET_IDLE),
    m_netLastDstId(0U),
    m_netLastSrcId(0U),
    m_permittedDstId(0U),
    m_tailOnIdle(false),
    m_ccRunning(false),
    m_ccPrevRunning(false),
    m_ccHalted(false),
    m_rfTimeout(1000U, timeout),
    m_rfTGHang(1000U, tgHang),
    m_rfLossWatchdog(1000U, 0U, 1500U),
    m_netTimeout(1000U, timeout),
    m_netTGHang(1000U, 2U),
    m_networkWatchdog(1000U, 0U, 1500U),
    m_adjSiteUpdate(1000U, 75U),
    m_activeTGUpdate(1000U, 5U),
    m_ccPacketInterval(1000U, 0U, 10U),
    m_interval(),
    m_hangCount(3U * 8U),
    m_tduPreambleCount(8U),
    m_frameLossCnt(0U),
    m_frameLossThreshold(DEFAULT_FRAME_LOSS_THRESHOLD),
    m_ccFrameCnt(0U),
    m_ccSeq(0U),
    m_random(),
    m_llaK(nullptr),
    m_llaRS(nullptr),
    m_llaCRS(nullptr),
    m_llaKS(nullptr),
    m_nid(nac),
    m_siteData(),
    m_rssiMapper(rssiMapper),
    m_rssi(0U),
    m_maxRSSI(0U),
    m_minRSSI(0U),
    m_aveRSSI(0U),
    m_rssiCount(0U),
    m_ccNotifyActiveTG(false),
    m_disableAdjSiteBroadcast(false),
    m_notifyCC(true),
    m_ccDebug(debug),
    m_verbose(verbose),
    m_debug(debug)
{
    assert(chLookup != nullptr);
    assert(ridLookup != nullptr);
    assert(tidLookup != nullptr);
    assert(idenTable != nullptr);
    assert(rssiMapper != nullptr);

    m_affiliations = new lookups::P25AffiliationLookup(this, chLookup, verbose);

    // bryanb: this is a hacky check to see if the modem is a ModemV24 or not...
    modem::ModemV24* modemV24 = dynamic_cast<modem::ModemV24*>(modem);
    if (modemV24 != nullptr)
        m_isModemDFSI = true;

    m_interval.start();

    acl::AccessControl::init(m_ridLookup, m_tidLookup);

    m_hangCount = callHang * 4U;

    m_voice = new Voice(this, debug, verbose);
    m_control = new ControlSignaling(this, dumpTSBKData, debug, verbose);
    m_data = new Data(this, dumpPDUData, repeatPDU, debug, verbose);

    std::random_device rd;
    std::mt19937 mt(rd());
    m_random = mt;

    m_llaK = nullptr;
    m_llaRS = new uint8_t[AUTH_KEY_LENGTH_BYTES];
    m_llaCRS = new uint8_t[AUTH_KEY_LENGTH_BYTES];
    m_llaKS = new uint8_t[AUTH_KEY_LENGTH_BYTES];

    // register RPC handlers
    g_RPC->registerHandler(RPC_PERMIT_P25_TG, RPC_FUNC_BIND(Control::RPC_permittedTG, this));
    g_RPC->registerHandler(RPC_ACTIVE_P25_TG, RPC_FUNC_BIND(Control::RPC_activeTG, this));
    g_RPC->registerHandler(RPC_CLEAR_ACTIVE_P25_TG, RPC_FUNC_BIND(Control::RPC_clearActiveTG, this));
    g_RPC->registerHandler(RPC_RELEASE_P25_TG, RPC_FUNC_BIND(Control::RPC_releaseGrantTG, this));
    g_RPC->registerHandler(RPC_TOUCH_P25_TG, RPC_FUNC_BIND(Control::RPC_touchGrantTG, this));
}

/* Finalizes a instance of the Control class. */

Control::~Control()
{
    if (m_affiliations != nullptr) {
        delete m_affiliations;
    }

    if (m_voice != nullptr) {
        delete m_voice;
    }

    if (m_control != nullptr) {
        delete m_control;
    }

    if (m_data != nullptr) {
        delete m_data;
    }

    if (m_llaK != nullptr) {
        delete[] m_llaK;
    }
    delete[] m_llaRS;
    delete[] m_llaCRS;
    delete[] m_llaKS;
}

/* Resets the data states for the RF interface. */

void Control::reset()
{
    m_rfState = RS_RF_LISTENING;
    m_ccHalted = false;

    if (m_voice != nullptr) {
        m_voice->resetRF();
    }

    if (m_data != nullptr) {
        m_data->resetRF();
    }

    m_txImmQueue.clear();
    m_txQueue.clear();
}

/* Helper to set P25 configuration options. */

void Control::setOptions(yaml::Node& conf, bool supervisor, const std::string cwCallsign, const ::lookups::VoiceChData controlChData,
    uint32_t netId, uint32_t sysId, uint8_t rfssId, uint8_t siteId, uint8_t channelId, uint32_t channelNo, bool printOptions)
{
    yaml::Node systemConf = conf["system"];
    yaml::Node p25Protocol = conf["protocols"]["p25"];

    m_supervisor = supervisor;

    m_tduPreambleCount = p25Protocol["tduPreambleCount"].as<uint32_t>(8U);

    yaml::Node rfssConfig = systemConf["config"];
    m_control->m_patchSuperGroup = (uint32_t)::strtoul(rfssConfig["pSuperGroup"].as<std::string>("FFFE").c_str(), NULL, 16);
    m_control->m_announcementGroup = (uint32_t)::strtoul(rfssConfig["announcementGroup"].as<std::string>("FFFE").c_str(), NULL, 16);

    yaml::Node secureConfig = rfssConfig["secure"];
    std::string key = secureConfig["key"].as<std::string>();
    if (!key.empty()) {
        if (key.size() == 32) {
            if ((key.find_first_not_of("0123456789abcdefABCDEF", 2) == std::string::npos)) {
                const char* keyPtr = key.c_str();
                m_llaK = new uint8_t[AUTH_KEY_LENGTH_BYTES];
                ::memset(m_llaK, 0x00U, AUTH_KEY_LENGTH_BYTES);

                for (uint8_t i = 0; i < AUTH_KEY_LENGTH_BYTES; i++) {
                    char t[4] = {keyPtr[0], keyPtr[1], 0};
                    m_llaK[i] = (uint8_t)::strtoul(t, NULL, 16);
                    keyPtr += 2 * sizeof(char);
                }
            }
            else {
                LogWarning(LOG_P25, "Invalid characters in the secure key. LLA disabled.");
            }
        }
        else {
            LogWarning(LOG_P25, "Invalid secure key length, key should be 16 hex pairs, or 32 characters. LLA disabled.");
        }
    }

    if (m_llaK != nullptr) {
        generateLLA_AM1_Parameters();
    }

    m_inhibitUnauth = p25Protocol["inhibitUnauthorized"].as<bool>(false);
    m_legacyGroupGrnt = p25Protocol["legacyGroupGrnt"].as<bool>(true);
    m_legacyGroupReg = p25Protocol["legacyGroupReg"].as<bool>(false);

    m_control->m_verifyAff = p25Protocol["verifyAff"].as<bool>(false);
    m_control->m_verifyReg = p25Protocol["verifyReg"].as<bool>(false);

    m_control->m_requireLLAForReg = p25Protocol["requireLLAForReg"].as<bool>(false);

    if (m_llaK == nullptr) {
        m_control->m_requireLLAForReg = false;
    }

    m_control->m_noStatusAck = p25Protocol["noStatusAck"].as<bool>(false);
    m_control->m_noMessageAck = p25Protocol["noMessageAck"].as<bool>(true);
    m_control->m_unitToUnitAvailCheck = p25Protocol["unitToUnitAvailCheck"].as<bool>(true);

    m_sndcpSupport = p25Protocol["sndcpSupport"].as<bool>(false);

    m_ignoreAffiliationCheck = p25Protocol["ignoreAffiliationCheck"].as<bool>(false);
    m_demandUnitRegForRefusedAff = p25Protocol["demandUnitRegForRefusedAff"].as<bool>(true);

    yaml::Node control = p25Protocol["control"];
    m_enableControl = control["enable"].as<bool>(false);
    if (m_enableControl) {
        m_dedicatedControl = control["dedicated"].as<bool>(false);
        m_convNetGrantDemand = false;
    }
    else {
        m_dedicatedControl = false;
    }

    m_voiceOnControl = control["voiceOnControl"].as<bool>(false);

    if (m_voiceOnControl) {
        LogWarning(LOG_P25, "HERE BE DRAGONS! Voice on Control is an unsupported/undocumented configuration; are you sure you should be doing this?");
    }

    // if control channel is off for voice on control off
    if (!m_enableControl) {
        m_voiceOnControl = false;
    }

    m_controlOnly = p25Protocol["controlOnly"].as<bool>(false);

    // if we're a dedicated control channel don't set the control only flag
    if (m_dedicatedControl) {
        m_controlOnly = false;
    }

    m_ackTSBKRequests = control["ackRequests"].as<bool>(true);
    m_control->m_ctrlTSDUMBF = !control["disableTSDUMBF"].as<bool>(false);
    if (m_isModemDFSI)
        m_control->m_ctrlTSDUMBF = false; // force single block TSDUs for DFSI mode
    m_control->m_ctrlTimeDateAnn = control["enableTimeDateAnn"].as<bool>(false);
    m_control->m_redundantImmediate = control["redundantImmediate"].as<bool>(true);
    m_control->m_redundantGrant = control["redundantGrantTransmit"].as<bool>(false);
    m_ccDebug = control["debug"].as<bool>(false);

    m_ccNotifyActiveTG = control["notifyActiveTG"].as<bool>(true);

    m_allowExplicitSourceId = p25Protocol["allowExplicitSourceId"].as<bool>(true);
    m_convNetGrantDemand = p25Protocol["convNetGrantDemand"].as<bool>(false);

    uint32_t ccBcstInterval = p25Protocol["control"]["interval"].as<uint32_t>(300U);
    m_control->m_adjSiteUpdateInterval += ccBcstInterval;

    m_control->m_disableGrantSrcIdCheck = p25Protocol["control"]["disableGrantSourceIdCheck"].as<bool>(false);
    m_disableAdjSiteBroadcast = p25Protocol["disableAdjSiteBroadcast"].as<bool>(false);

    yaml::Node controlCh = rfssConfig["controlCh"];
    m_notifyCC = controlCh["notifyEnable"].as<bool>(false);

    // voice on control forcibly disables CC notification; and force enable redundant grant
    if (m_voiceOnControl) {
        m_control->m_redundantGrant = true;
        m_notifyCC = false;
    }

    /*
    ** Voice Silence and Frame Loss Thresholds
    */
    m_voice->m_silenceThreshold = p25Protocol["silenceThreshold"].as<uint32_t>(DEFAULT_SILENCE_THRESHOLD);
    if (m_voice->m_silenceThreshold > MAX_P25_VOICE_ERRORS) {
        LogWarning(LOG_P25, "Silence threshold > %u, defaulting to %u", MAX_P25_VOICE_ERRORS, DEFAULT_SILENCE_THRESHOLD);
        m_voice->m_silenceThreshold = DEFAULT_SILENCE_THRESHOLD;
    }

    // either MAX_P25_VOICE_ERRORS or 0 will disable the threshold logic
    if (m_voice->m_silenceThreshold == 0) {
        LogWarning(LOG_P25, "Silence threshold set to zero, defaulting to %u", MAX_P25_VOICE_ERRORS);
        m_voice->m_silenceThreshold = MAX_P25_VOICE_ERRORS;
    }
    m_frameLossThreshold = (uint8_t)p25Protocol["frameLossThreshold"].as<uint32_t>(DEFAULT_FRAME_LOSS_THRESHOLD);
    if (m_frameLossThreshold == 0U) {
        m_frameLossThreshold = 1U;
    }

    if (m_frameLossThreshold > DEFAULT_FRAME_LOSS_THRESHOLD * 2U) {
        LogWarning(LOG_P25, "Frame loss threshold may be excessive, default is %u, configured is %u", DEFAULT_FRAME_LOSS_THRESHOLD, m_frameLossThreshold);
    }

    /*
    ** Network HDU and Grant Disables
    */
    m_disableNetworkGrant = p25Protocol["disableNetworkGrant"].as<bool>(false);
    m_disableNetworkHDU = p25Protocol["disableNetworkHDU"].as<bool>(false);

    if (m_disableNetworkGrant && m_enableControl && m_dedicatedControl) {
        LogWarning(LOG_P25, "Cannot disable network traffic grants for dedicated control configuration.");
        m_disableNetworkGrant = false;
    }

    if (m_disableNetworkGrant && m_controlOnly) {
        LogWarning(LOG_P25, "Cannot disable network traffic grants for control-only configuration.");
        m_disableNetworkGrant = false;
    }

    if (m_disableNetworkGrant && m_voiceOnControl) {
        LogWarning(LOG_P25, "Cannot disable network traffic grants for voice on control configuration.");
        m_disableNetworkGrant = false;
    }

    /*
    ** CC Service Class
    */
    uint8_t serviceClass = ServiceClass::VOICE | ServiceClass::DATA;
    if (m_enableControl) {
        if (!m_sndcpSupport)
            serviceClass = ServiceClass::VOICE;
        serviceClass |= ServiceClass::REG;
    }

    /*
    ** Site Data
    */
    int8_t lto = (int8_t)systemConf["localTimeOffset"].as<int32_t>(0);
    m_siteData = SiteData(netId, sysId, rfssId, siteId, 0U, channelId, channelNo, serviceClass, lto);
    uint32_t valueTest = (netId >> 8);
    const uint32_t constValue = 0x17DC0U;
    if (valueTest == (constValue >> 5)) {
        ::fatal("error 8\n");
    }
    m_siteData.setCallsign(cwCallsign);

    lc::LC::setSiteData(m_siteData);
    lc::TDULC::setSiteData(m_siteData);

    lc::TSBK::setCallsign(cwCallsign);
    lc::TSBK::setSiteData(m_siteData);

    std::vector<::lookups::IdenTable> entries = m_idenTable->list();
    for (auto entry : entries) {
        if (entry.channelId() == channelId) {
            m_idenEntry = entry;
            break;
        }
    }

    m_siteData.setChCnt((uint8_t)m_affiliations->rfCh()->rfChSize());

    m_controlChData = controlChData;

    bool disableUnitRegTimeout = p25Protocol["disableUnitRegTimeout"].as<bool>(false);
    m_affiliations->setDisableUnitRegTimeout(disableUnitRegTimeout);

    // set the grant release callback
    m_affiliations->setReleaseGrantCallback([=](uint32_t chNo, uint32_t dstId, uint8_t slot) {
        // callback REST API to clear TG permit for the granted TG on the specified voice channel
        if (m_authoritative && m_supervisor) {
            ::lookups::VoiceChData voiceChData = m_affiliations->rfCh()->getRFChData(chNo);
            if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0 &&
                chNo != m_siteData.channelNo()) {
                json::object req = json::object();
                dstId = 0U; // clear TG value
                req["dstId"].set<uint32_t>(dstId);

                g_RPC->req(RPC_PERMIT_P25_TG, req, nullptr, voiceChData.address(), voiceChData.port());
            }
            else {
                ::LogError(LOG_P25, P25_TSDU_STR ", TSBKO, IOSP_GRP_VCH (Group Voice Channel Grant), failed to clear TG permit, chNo = %u", chNo);
            }
        }
    });

    // set the unit deregistration callback
    m_affiliations->setUnitDeregCallback([=](uint32_t srcId, bool automatic) {
        if (m_network != nullptr)
            m_network->announceUnitDeregistration(srcId);

        // fire off a U_REG_CMD to get the SU to re-affiliate
        if (m_enableControl && automatic) {
            m_control->writeRF_TSDU_U_Reg_Cmd(srcId);
        }
    });

    // set the In-Call Control function callback
    if (m_network != nullptr) {
        m_network->setP25ICCCallback([=](network::NET_ICC::ENUM command, uint32_t dstId) { processInCallCtrl(command, dstId); });
    }

    // throw a warning if we are notifying a CC of our presence (this indicates we're a VC) *AND* we have the control
    // enable flag set
    if (m_enableControl && m_notifyCC) {
        LogWarning(LOG_P25, "We are configured as a dedicated trunked voice channel but, have control data enabled. Don't do this, control data for a dedicated trunked voice channel should be disabled. This can cause unintended behavior.");
    }

    if (printOptions) {
        LogInfo("    Silence Threshold: %u (%.1f%%)", m_voice->m_silenceThreshold, float(m_voice->m_silenceThreshold) / 12.33F);
        LogInfo("    Frame Loss Threshold: %u", m_frameLossThreshold);

        if (m_enableControl) {
            LogInfo("    Ack Requests: %s", m_ackTSBKRequests ? "yes" : "no");
            if (m_control->m_disableGrantSrcIdCheck) {
                LogInfo("    Disable Grant Source ID Check: yes");
            }
            if (m_supervisor)
                LogMessage(LOG_P25, "Host is configured to operate as a P25 control channel, site controller mode.");
        }

        if (m_controlOnly) {
            LogInfo("    Control Data Only: yes");
        }

        LogInfo("    Patch Super Group: $%04X", m_control->m_patchSuperGroup);
        LogInfo("    Announcement Group: $%04X", m_control->m_announcementGroup);

        LogInfo("    Notify Control: %s", m_notifyCC ? "yes" : "no");
        if (m_disableNetworkHDU) {
            LogInfo("    Disable Network HDUs: yes");
        }
        if (m_disableNetworkGrant) {
            LogInfo("    Disable Network Grants: yes");
        }
        if (!m_control->m_ctrlTSDUMBF) {
            LogInfo("    Disable Multi-Block TSDUs: yes");
        }
        LogInfo("    Time/Date Announcement TSBK: %s", m_control->m_ctrlTimeDateAnn ? "yes" : "no");

        if (m_llaK != nullptr) {
            LogInfo("    Link Layer Authentication: yes");
        }

        LogInfo("    Inhibit Unauthorized: %s", m_inhibitUnauth ? "yes" : "no");
        LogInfo("    Legacy Group Grant: %s", m_legacyGroupGrnt ? "yes" : "no");
        LogInfo("    Legacy Group Registration: %s", m_legacyGroupReg ? "yes" : "no");
        LogInfo("    Verify Affiliation: %s", m_control->m_verifyAff ? "yes" : "no");
        LogInfo("    Verify Registration: %s", m_control->m_verifyReg ? "yes" : "no");
        LogInfo("    Require LLA for Registration: %s", m_control->m_requireLLAForReg ? "yes" : "no");

        LogInfo("    SNDCP Support: %s", m_sndcpSupport ? "yes" : "no");

        LogInfo("    Ignore Affiliation Check: %s", m_ignoreAffiliationCheck ? "yes" : "no");
        LogInfo("    No Status ACK: %s", m_control->m_noStatusAck ? "yes" : "no");
        LogInfo("    No Message ACK: %s", m_control->m_noMessageAck ? "yes" : "no");
        LogInfo("    Unit-to-Unit Availability Check: %s", m_control->m_unitToUnitAvailCheck ? "yes" : "no");
        LogInfo("    Explicit Source ID Support: %s", m_allowExplicitSourceId ? "yes" : "no");
        LogInfo("    Conventional Network Grant Demand: %s", m_convNetGrantDemand ? "yes" : "no");
        LogInfo("    Demand Unit Registration for Refused Affiliation: %s", m_demandUnitRegForRefusedAff ? "yes" : "no");

        LogInfo("    Notify VCs of Active TGs: %s", m_ccNotifyActiveTG ? "yes" : "no");

        if (disableUnitRegTimeout) {
            LogInfo("    Disable Unit Registration Timeout: yes");
        }

        if (m_disableAdjSiteBroadcast) {
            LogInfo("    Disable Adjacent Site Broadcast: yes");
        }

        LogInfo("    Redundant Immediate: %s", m_control->m_redundantImmediate ? "yes" : "no");
        if (m_control->m_redundantGrant) {
            LogInfo("    Redundant Grant Transmit: yes");
        }

        if (m_ccDebug) {
            LogInfo("    Control Message Debug: yes");
        }
    }

    // are we overriding the NAC for split NAC operations?
    uint32_t txNAC = (uint32_t)::strtoul(systemConf["config"]["txNAC"].as<std::string>("F7E").c_str(), NULL, 16);
    if (txNAC != NAC_DIGITAL_SQ && txNAC != m_nac) {
        LogMessage(LOG_P25, "Split NAC operations, setting Tx NAC to $%03X", txNAC);
        m_txNAC = txNAC;
        m_nid.setTxNAC(m_txNAC);
    }

    if (m_voice != nullptr) {
        m_voice->resetRF();
        m_voice->resetNet();
    }

    if (m_data != nullptr) {
        m_data->resetRF();
    }
}

/* Process a data frame from the RF interface. */

bool Control::processFrame(uint8_t* data, uint32_t len)
{
    assert(data != nullptr);

    bool sync = data[1U] == 0x01U;

    if (data[0U] == modem::TAG_LOST) {
        if (m_frameLossCnt > m_frameLossThreshold) {
            m_frameLossCnt = 0U;

            processFrameLoss();

            return false;
        }
        else {
            // increment the frame loss count by one for audio or data; otherwise drop
            // packets
            if (m_rfState == RS_RF_AUDIO || m_rfState == RS_RF_DATA) {
                m_rfLossWatchdog.start();
                ++m_frameLossCnt;
            }
            else {
                m_frameLossCnt = 0U;
                m_rfState = RS_RF_LISTENING;

                m_voice->resetRF();
                m_data->resetRF();

                return false;
            }
        }
    }

    if (m_rfState == RS_RF_AUDIO || m_rfState == RS_RF_DATA) {
        if (m_rfLossWatchdog.isRunning()) {
            m_rfLossWatchdog.start();
        }
    }

    if (!sync && m_rfState == RS_RF_LISTENING) {
        uint8_t syncBytes[P25_SYNC_LENGTH_BYTES];
        ::memcpy(syncBytes, data + 2U, P25_SYNC_LENGTH_BYTES);

        uint8_t errs = 0U;
        for (uint8_t i = 0U; i < P25_SYNC_LENGTH_BYTES; i++)
            errs += Utils::countBits8(syncBytes[i] ^ P25_SYNC_BYTES[i]);

        if (errs >= MAX_SYNC_BYTES_ERRS) {
/*
            LogWarning(LOG_RF, "P25, possible sync word rejected, errs = %u, sync word = %02X %02X %02X %02X %02X %02X", errs,
                syncBytes[0U], syncBytes[1U], syncBytes[2U], syncBytes[3U], syncBytes[4U], syncBytes[5U]);
*/
            return false;
        }
        else {
            LogWarning(LOG_RF, "P25, possible sync word, errs = %u, sync word = %02X %02X %02X %02X %02X %02X", errs,
                syncBytes[0U], syncBytes[1U], syncBytes[2U], syncBytes[3U], syncBytes[4U], syncBytes[5U]);

            sync = true; // we found a completly valid sync with no errors...
        }
    }

    if (sync && m_debug) {
        Utils::symbols("!!! *Rx P25", data + 2U, len - 2U);
    }

    // Decode the NID
    bool valid = m_nid.decode(data + 2U);

    if (!valid && m_rfState == RS_RF_LISTENING)
        return false;

    // if the controller is currently in an reject state; block any RF traffic
    if (valid && m_rfState == RS_RF_REJECTED)
        return false;

    DUID::E duid = m_nid.getDUID();

    // Have we got RSSI bytes on the end of a P25 LDU?
    if (len == (P25_LDU_FRAME_LENGTH_BYTES + 4U)) {
        uint16_t raw = 0U;
        raw |= (data[218U] << 8) & 0xFF00U;
        raw |= (data[219U] << 0) & 0x00FFU;

        // Convert the raw RSSI to dBm
        int rssi = m_rssiMapper->interpolate(raw);
        if (m_verbose) {
            LogMessage(LOG_RF, "P25, raw RSSI = %u, reported RSSI = %d dBm", raw, rssi);
        }

        // RSSI is always reported as positive
        m_rssi = (rssi >= 0) ? rssi : -rssi;

        if (m_rssi > m_minRSSI)
            m_minRSSI = m_rssi;
        if (m_rssi < m_maxRSSI)
            m_maxRSSI = m_rssi;

        m_aveRSSI += m_rssi;
        m_rssiCount++;
    }

    if (m_debug) {
        LogDebug(LOG_RF, "P25, rfState = %u, netState = %u, DUID = %u, lastDUID = %u", m_rfState, m_netState, duid, m_voice->m_lastDUID);
    }

    // are we interrupting a running CC?
    if (m_ccRunning) {
        if (duid != DUID::TSDU) {
            m_ccHalted = true;
        }
    }

    bool ret = false;

    // handle individual DUIDs
    switch (duid) {
        case DUID::HDU:
        case DUID::LDU1:
        case DUID::VSELP1:
        case DUID::VSELP2:
        case DUID::LDU2:
            if (m_controlOnly) {
                if (m_debug) {
                    LogDebug(LOG_RF, "CC only mode, ignoring HDU/LDU from RF");
                }
                break;
            }

            if (!m_dedicatedControl || m_control->m_convFallback)
                ret = m_voice->process(data, len);
            else {
                if (m_voiceOnControl && m_affiliations->isChBusy(m_siteData.channelNo()))
                    ret = m_voice->process(data, len);
            }
            break;

        case DUID::TDU:
        case DUID::TDULC:
            m_frameLossCnt = 0U;
            ret = m_voice->process(data, len);
            break;

        case DUID::PDU:
            ret = m_data->process(data, len);
            break;

        case DUID::TSDU:
            ret = m_control->process(data, len);
            break;

        default:
            LogError(LOG_RF, "P25 unhandled DUID, duid = $%02X", duid);
            return false;
    }

    return ret;
}

/* Get the frame data length for the next frame in the data ring buffer. */

uint32_t Control::peekFrameLength()
{
    std::lock_guard<std::mutex> lock(m_queueLock);

    if (m_txQueue.isEmpty() && m_txImmQueue.isEmpty())
        return 0U;

    uint8_t length[2U];
    ::memset(length, 0x00U, 2U);

    uint16_t len = 0U;

    // tx immediate queue takes priority
    if (!m_txImmQueue.isEmpty()) {
        m_txImmQueue.peek(length, 2U);
        len = (length[0U] << 8) + length[1U];
    }
    else {
        m_txQueue.peek(length, 2U);
        len = (length[0U] << 8) + length[1U];
    }

    return len;
}

/* Helper to determine whether or not the internal frame queue is full. */

bool Control::isQueueFull()
{
    if (m_txQueue.isEmpty() && m_txImmQueue.isEmpty())
        return false;

    // tx immediate queue takes priority
    if (!m_txImmQueue.isEmpty()) {
        uint32_t space = m_txImmQueue.freeSpace();
        if (space < (P25_LDU_FRAME_LENGTH_BYTES + 1U))
            return true;
    }
    else {
        uint32_t space = m_txQueue.freeSpace();
        if (space < (P25_LDU_FRAME_LENGTH_BYTES + 1U))
            return true;
    }

    return false;
}

/* Get frame data from data ring buffer. */

uint32_t Control::getFrame(uint8_t* data)
{
    assert(data != nullptr);

    std::lock_guard<std::mutex> lock(m_queueLock);

    if (m_txQueue.isEmpty() && m_txImmQueue.isEmpty())
        return 0U;

    uint8_t length[2U];
    ::memset(length, 0x00U, 2U);

    uint16_t len = 0U;

    // tx immediate queue takes priority
    if (!m_txImmQueue.isEmpty()) {
        m_txImmQueue.get(length, 2U);
        len = (length[0U] << 8) + length[1U];

        m_txImmQueue.get(data, len);
    }
    else {
        m_txQueue.get(length, 2U);
        len = (length[0U] << 8) + length[1U];

        m_txQueue.get(data, len);
    }

    return len;
}

/* Helper to write end of voice call frame data. */

bool Control::writeRF_VoiceEnd()
{
    if (m_netState == RS_NET_IDLE && m_rfState == RS_RF_LISTENING) {
        if (m_tailOnIdle) {
            bool ret = false;
            if (m_netState == RS_NET_IDLE && m_rfState == RS_RF_LISTENING) {
                m_voice->writeRF_EndOfVoice();

                // this should have been cleared by writeRF_EndOfVoice; but if it hasn't clear it
                // to prevent badness
                if (m_voice->m_hadVoice) {
                    m_voice->m_hadVoice = false;
                }

                m_tailOnIdle = false;
                if (m_network != nullptr)
                    m_network->resetP25();

                ret = true;
            }

            if (!m_enableControl && m_duplex) {
                writeRF_Nulls();
            }

            return ret;
        }
    }

    return false;
}

/* Updates the processor. */

void Control::clock()
{
    uint32_t ms = m_interval.elapsed();
    m_interval.start();

    if (m_network != nullptr) {
        processNetwork();

        if (m_network->getStatus() == network::NET_STAT_RUNNING) {
            m_siteData.setNetActive(true);
        }
        else {
            m_siteData.setNetActive(false);
        }

        lc::TDULC::setSiteData(m_siteData);
        lc::TSBK::setSiteData(m_siteData);
    }

    // if we have control enabled; do clocking to generate a CC data stream
    if (m_enableControl) {
        if (m_ccRunning && !m_ccPacketInterval.isRunning()) {
            m_ccPacketInterval.start();
        }

        if (m_ccHalted) {
            if (!m_ccRunning) {
                m_ccHalted = false;
                m_ccPrevRunning = m_ccRunning;
            }
        }
        else {
            m_ccPacketInterval.clock(ms);
            if (!m_ccPacketInterval.isRunning()) {
                m_ccPacketInterval.start();
            }

            if (m_ccPacketInterval.isRunning() && m_ccPacketInterval.hasExpired()) {
                if (m_ccRunning) {
                    writeRF_ControlData();
                }

                m_ccPacketInterval.start();
            }
        }

        if (m_ccPrevRunning && !m_ccRunning) {
            writeRF_ControlEnd();
            m_ccPrevRunning = m_ccRunning;
        }
    }

    // handle timeouts and hang timers
    m_rfTimeout.clock(ms);
    m_netTimeout.clock(ms);

    if (m_rfTGHang.isRunning()) {
        m_rfTGHang.clock(ms);

        if (m_rfTGHang.hasExpired()) {
            m_rfTGHang.stop();
            if (m_verbose) {
                LogMessage(LOG_RF, "talkgroup hang has expired, lastDstId = %u", m_rfLastDstId);
            }
            m_rfLastDstId = 0U;
            m_rfLastSrcId = 0U;

            // reset permitted ID and clear permission state
            if (!m_authoritative && m_permittedDstId != 0U) {
                m_permittedDstId = 0U;
            }

            // has the talkgroup hang timer expired while the modem is in a non-listening state?
            if (m_rfState != RS_RF_LISTENING) {
                processFrameLoss();
            }
        }
    }

    if (m_rfState == RS_RF_AUDIO || m_rfState == RS_RF_DATA) {
        if (m_rfLossWatchdog.isRunning()) {
            m_rfLossWatchdog.clock(ms);

            if (m_rfLossWatchdog.hasExpired()) {
                m_rfLossWatchdog.stop();

                processFrameLoss();
            }
        }
    }

    if (m_authoritative) {
        if (m_netTGHang.isRunning()) {
            m_netTGHang.clock(ms);

            if (m_netTGHang.hasExpired()) {
                m_netTGHang.stop();
                if (m_verbose) {
                    LogMessage(LOG_NET, "talkgroup hang has expired, lastDstId = %u", m_netLastDstId);
                }
                m_netLastDstId = 0U;
                m_netLastSrcId = 0U;
            }
        }
    }
    else {
        m_netTGHang.stop();
    }

    if (m_netState == RS_NET_AUDIO || m_netState == RS_NET_DATA) {
        m_networkWatchdog.clock(ms);

        if (m_networkWatchdog.isRunning() && m_networkWatchdog.hasExpired()) {
            if (m_netState == RS_NET_AUDIO) {
                if (m_voice->m_netFrames > 0.0F) {
                    ::ActivityLog("P25", false, "network watchdog has expired, %.1f seconds, %u%% packet loss",
                        float(m_voice->m_netFrames) / 50.0F, (m_voice->m_netLost * 100U) / m_voice->m_netFrames);
                }
            }
            else {
                ::ActivityLog("P25", false, "network watchdog has expired");
            }

            m_networkWatchdog.stop();
            m_affiliations->releaseGrant(m_voice->m_netLC.getDstId(), false);

            if (m_dedicatedControl) {
                if (m_network != nullptr)
                    m_network->resetP25();
            }

            m_netState = RS_NET_IDLE;
            m_tailOnIdle = true;

            m_voice->resetNet();

            m_netTimeout.stop();
        }
    }

    // reset states if we're in a rejected state and we're a control channel
    if (m_rfState == RS_RF_REJECTED && m_enableControl && m_dedicatedControl) {
        clearRFReject();
    }

    if (m_frameLossCnt > 0U && m_rfState == RS_RF_LISTENING)
        m_frameLossCnt = 0U;
    if (m_frameLossCnt >= m_frameLossThreshold && (m_rfState == RS_RF_AUDIO || m_rfState == RS_RF_DATA)) {
        processFrameLoss();
    }

    // clock data and trunking
    if (m_data != nullptr) {
        m_data->clock(ms);
    }
}

/* Updates the adj. site tables and affiliations. */

void Control::clockSiteData(uint32_t ms)
{
    if (m_enableControl) {
        // clock all the grant timers
        m_affiliations->clock(ms);
    }

    if (m_control != nullptr) {
        // do we need to network announce ourselves?
        if (!m_adjSiteUpdate.isRunning()) {
            m_control->writeAdjSSNetwork();
            m_adjSiteUpdate.start();
        }

        m_adjSiteUpdate.clock(ms);
        if (m_adjSiteUpdate.isRunning() && m_adjSiteUpdate.hasExpired()) {
            if (m_rfState == RS_RF_LISTENING && m_netState == RS_NET_IDLE) {
                m_control->writeAdjSSNetwork();
                if (m_network != nullptr) {
                    if (m_affiliations->grpAffSize() > 0) {
                        auto affs = m_affiliations->grpAffTable();
                        m_network->announceAffiliationUpdate(affs);
                    }
                }
                m_adjSiteUpdate.start();
            }
        }

        if (m_enableControl) {
            // clock adjacent site and SCCB update timers
            m_control->m_adjSiteUpdateTimer.clock(ms);
            if (m_control->m_adjSiteUpdateTimer.isRunning() && m_control->m_adjSiteUpdateTimer.hasExpired()) {
                if (m_control->m_adjSiteUpdateCnt.size() > 0) {
                    // update adjacent site data
                    for (auto &entry : m_control->m_adjSiteUpdateCnt) {
                        uint8_t siteId = entry.first;
                        uint8_t updateCnt = entry.second;
                        if (updateCnt > 0U) {
                            updateCnt--;
                        }

                        if (updateCnt == 0U) {
                            SiteData siteData = m_control->m_adjSiteTable[siteId];
                            LogWarning(LOG_NET, "P25, Adjacent Site Status Expired, no data [FAILED], sysId = $%03X, rfss = $%02X, site = $%02X, chNo = %u-%u, svcClass = $%02X",
                                siteData.sysId(), siteData.rfssId(), siteData.siteId(), siteData.channelId(), siteData.channelNo(), siteData.serviceClass());
                        }

                        entry.second = updateCnt;
                    }
                }

                if (m_control->m_sccbUpdateCnt.size() > 0) {
                    // update SCCB data
                    for (auto& entry : m_control->m_sccbUpdateCnt) {
                        uint8_t rfssId = entry.first;
                        uint8_t updateCnt = entry.second;
                        if (updateCnt > 0U) {
                            updateCnt--;
                        }

                        if (updateCnt == 0U) {
                            SiteData siteData = m_control->m_sccbTable[rfssId];
                            LogWarning(LOG_NET, "P25, Secondary Control Channel Expired, no data [FAILED], sysId = $%03X, rfss = $%02X, site = $%02X, chNo = %u-%u, svcClass = $%02X",
                                siteData.sysId(), siteData.rfssId(), siteData.siteId(), siteData.channelId(), siteData.channelNo(), siteData.serviceClass());
                        }

                        entry.second = updateCnt;
                    }
                }

                m_control->m_adjSiteUpdateTimer.setTimeout(m_control->m_adjSiteUpdateInterval);
                m_control->m_adjSiteUpdateTimer.start();
            }

            if (m_ccNotifyActiveTG) {
                if (!m_activeTGUpdate.isRunning()) {
                    m_activeTGUpdate.start();
                }

                m_activeTGUpdate.clock(ms);
                if (m_activeTGUpdate.isRunning() && m_activeTGUpdate.hasExpired()) {
                    m_activeTGUpdate.start();

                    // do we have any granted channels?
                    if (m_affiliations->getGrantedRFChCnt() > 0U) {
                        uint8_t activeCnt = m_affiliations->getGrantedRFChCnt();
                        std::unordered_map<uint32_t, uint32_t> grantTable = m_affiliations->grantTable();

                        // iterate dynamic channel grant table entries
                        json::array active = json::array();
                        for (auto entry : grantTable) {
                            uint32_t dstId = entry.first;
                            active.push_back(json::value((double)dstId));
                        }

                        std::unordered_map<uint32_t, ::lookups::VoiceChData> voiceChs = m_affiliations->rfCh()->rfChDataTable();
                        for (auto entry : voiceChs) {
                            ::lookups::VoiceChData voiceChData = entry.second;

                            // callback RPC to transmit active TG list to the voice channels
                            if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0) {
                                if (voiceChData.address() != "0.0.0.0") {
                                    json::object req = json::object();
                                    req["active"].set<json::array>(active);

                                    g_RPC->req(RPC_ACTIVE_P25_TG, req, [=](json::object& req, json::object& reply) {
                                        if (!req["status"].is<int>()) {
                                            ::LogError(LOG_P25, "failed to send active TG list to VC %s:%u, invalid RPC response", voiceChData.address().c_str(), voiceChData.port());
                                            return;
                                        }

                                        int status = req["status"].get<int>();
                                        if (status != network::NetRPC::OK) {
                                            ::LogError(LOG_P25, "failed to send active TG list to VC %s:%u", voiceChData.address().c_str(), voiceChData.port());
                                            if (req["message"].is<std::string>()) {
                                                std::string retMsg = req["message"].get<std::string>();
                                                ::LogError(LOG_P25, "RPC failed, %s", retMsg.c_str());
                                            }
                                        } 
                                        else
                                            ::LogMessage(LOG_P25, "VC %s:%u, active TG update, activeCnt = %u", voiceChData.address().c_str(), voiceChData.port(), activeCnt);
                                    }, voiceChData.address(), voiceChData.port());
                                }
                            }
                        }
                    } else {
                        std::unordered_map<uint32_t, ::lookups::VoiceChData> voiceChs = m_affiliations->rfCh()->rfChDataTable();
                        for (auto entry : voiceChs) {
                            ::lookups::VoiceChData voiceChData = entry.second;

                            // callback RPC to transmit active TG list to the voice channels
                            if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0) {
                                if (voiceChData.address() != "0.0.0.0") {
                                    json::object req = json::object();

                                    g_RPC->req(RPC_CLEAR_ACTIVE_P25_TG, req, [=](json::object& req, json::object& reply) {
                                        if (!req["status"].is<int>()) {
                                            ::LogError(LOG_P25, "failed to send clear active TG list to VC %s:%u, invalid RPC response", voiceChData.address().c_str(), voiceChData.port());
                                            return;
                                        }

                                        int status = req["status"].get<int>();
                                        if (status != network::NetRPC::OK) {
                                            ::LogError(LOG_P25, "failed to send clear active TG list to VC %s:%u", voiceChData.address().c_str(), voiceChData.port());
                                            if (req["message"].is<std::string>()) {
                                                std::string retMsg = req["message"].get<std::string>();
                                                ::LogError(LOG_P25, "RPC failed, %s", retMsg.c_str());
                                            }
                                        } 
                                        else
                                            ::LogMessage(LOG_P25, "VC %s:%u, clear active TG update", voiceChData.address().c_str(), voiceChData.port());
                                    }, voiceChData.address(), voiceChData.port());
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

/* Permits a TGID on a non-authoritative host. */

void Control::permittedTG(uint32_t dstId, bool dataPermit)
{
    if (m_authoritative) {
        return;
    }

    if (m_verbose) {
        if (dstId == 0U)
            LogMessage(LOG_P25, "non-authoritative TG unpermit");
        else
            LogMessage(LOG_P25, "non-authoritative TG permit, dstId = %u", dstId);
    }

    m_permittedDstId = dstId;

    // is this a data permit?
    if (dataPermit && m_sndcpSupport) {
        m_data->sndcpInitialize(dstId);
    }
}

/* Grants a TGID on a non-authoritative host. */

void Control::grantTG(uint32_t srcId, uint32_t dstId, bool grp)
{
    if (!m_enableControl) {
        return;
    }

    if (m_verbose) {
        LogMessage(LOG_P25, "network TG grant demand, srcId = %u, dstId = %u", srcId, dstId);
    }

    m_control->writeRF_TSDU_Grant(srcId, dstId, 4U, grp);
}

/* Clears the current operating RF state back to idle. */

void Control::clearRFReject()
{
    if (m_rfState == RS_RF_REJECTED) {
        m_txQueue.clear();

        m_voice->resetRF();
        m_voice->resetNet();

        m_data->resetRF();

        if (m_network != nullptr)
            m_network->resetP25();

        m_rfState = RS_RF_LISTENING;
    }
}

/* Flag indicating whether the processor or is busy or not. */

bool Control::isBusy() const
{
    return m_rfState != RS_RF_LISTENING || m_netState != RS_NET_IDLE;
}

/* Helper to change the debug and verbose state. */

void Control::setDebugVerbose(bool debug, bool verbose)
{
    m_debug = m_voice->m_debug = m_data->m_debug = m_control->m_debug = debug;
    m_verbose = m_voice->m_verbose = m_data->m_verbose = m_control->m_verbose = verbose;
}

/* Helper to get the last transmitted destination ID. */

uint32_t Control::getLastDstId() const
{
    if (m_rfLastDstId != 0U) {
        return m_rfLastDstId;
    }

    if (m_netLastDstId != 0U) {
        return m_netLastDstId;
    }

    return 0U;
}

/* Helper to get the last transmitted source ID. */

uint32_t Control::getLastSrcId() const
{
    if (m_rfLastSrcId != 0U) {
        return m_rfLastSrcId;
    }

    if (m_netLastSrcId != 0U) {
        return m_netLastSrcId;
    }

    return 0U;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Add data frame to the data ring buffer. */

void Control::addFrame(const uint8_t* data, uint32_t length, bool net, bool imm)
{
    assert(data != nullptr);

    std::lock_guard<std::mutex> lock(m_queueLock);

    if (!net) {
        if (m_rfTimeout.isRunning() && m_rfTimeout.hasExpired())
            return;
    } else {
        if (m_netTimeout.isRunning() && m_netTimeout.hasExpired())
            return;
    }

    if (m_debug) {
        Utils::symbols("!!! *Tx P25", data + 2U, length - 2U);
    }

    uint32_t fifoSpace = m_modem->getP25Space();

    // LogDebugEx(LOG_P25, "Control::addFrame()", "fifoSpace = %u", fifoSpace);

    // is this immediate data?
    if (imm) {
        // resize immediate queue if necessary (this shouldn't really ever happen)
        uint32_t space = m_txImmQueue.freeSpace();
        if (space < (length + 1U)) {
            if (!net) {
                uint32_t queueLen = m_txImmQueue.length();
                m_txImmQueue.resize(queueLen + P25_LDU_FRAME_LENGTH_BYTES);
                LogError(LOG_P25, "overflow in the P25 queue while writing imm data; queue free is %u, needed %u; resized was %u is %u, fifoSpace = %u", space, length, queueLen, m_txImmQueue.length(), fifoSpace);
                return;
            }
            else {
                LogError(LOG_P25, "overflow in the P25 queue while writing imm network data; queue free is %u, needed %u, fifoSpace = %u", space, length, fifoSpace);
                return;
            }
        }

        uint8_t lenBuffer[2U];
        if (length > 255U)
            lenBuffer[0U] = (length >> 8U) & 0xFFU;
        else
            lenBuffer[0U] = 0x00U;
        lenBuffer[1U] = length & 0xFFU;
        m_txImmQueue.addData(lenBuffer, 2U);

        m_txImmQueue.addData(data, length);
        return;
    }

    // resize queue if necessary (this shouldn't really ever happen)
    uint32_t space = m_txQueue.freeSpace();
    if (space < (length + 1U)) {
        if (!net) {
            uint32_t queueLen = m_txQueue.length();
            m_txQueue.resize(queueLen + P25_LDU_FRAME_LENGTH_BYTES);
            LogError(LOG_P25, "overflow in the P25 queue while writing data; queue free is %u, needed %u; resized was %u is %u, fifoSpace = %u", space, length, queueLen, m_txQueue.length(), fifoSpace);
            return;
        }
        else {
            LogError(LOG_P25, "overflow in the P25 queue while writing network data; queue free is %u, needed %u, fifoSpace = %u", space, length, fifoSpace);
            return;
        }
    }

    uint8_t lenBuffer[2U];
    if (length > 255U)
        lenBuffer[0U] = (length >> 8U) & 0xFFU;
    else
        lenBuffer[0U] = 0x00U;
    lenBuffer[1U] = length & 0xFFU;
    m_txQueue.addData(lenBuffer, 2U);

    m_txQueue.addData(data, length);
}

/* Process a data frames from the network. */

void Control::processNetwork()
{
    if (m_rfState != RS_RF_LISTENING && m_netState == RS_NET_IDLE)
        return;

    uint32_t length = 0U;
    bool ret = false;
    UInt8Array buffer = m_network->readP25(ret, length);
    if (!ret)
        return;
    if (length == 0U)
        return;
    if (buffer == nullptr) {
        m_network->resetP25();
        return;
    }

    bool grantDemand = (buffer[14U] & 0x80U) == 0x80U;
    bool grantDenial = (buffer[14U] & 0x40U) == 0x40U;
    bool grantEncrypt = (buffer[14U] & 0x08U) == 0x08U;
    bool unitToUnit = (buffer[14U] & 0x01U) == 0x01U;

    // process network message header
    DUID::E duid = (DUID::E)buffer[22U];
    uint8_t MFId = buffer[15U];

    // process raw P25 data bytes
    UInt8Array data;
    uint8_t frameLength = buffer[23U];
    if (duid == DUID::PDU) {
        frameLength = length;
        data = std::unique_ptr<uint8_t[]>(new uint8_t[length]);
        ::memset(data.get(), 0x00U, length);
        ::memcpy(data.get(), buffer.get(), length);
    }
    else {
        if (frameLength <= 24) {
            data = std::unique_ptr<uint8_t[]>(new uint8_t[frameLength]);
            ::memset(data.get(), 0x00U, frameLength);
        }
        else {
            data = std::unique_ptr<uint8_t[]>(new uint8_t[frameLength]);
            ::memset(data.get(), 0x00U, frameLength);
            ::memcpy(data.get(), buffer.get() + 24U, frameLength);
        }
    }

    // is this a PDU?
    if (duid == DUID::PDU) {
        if (m_controlOnly) {
            if (m_debug) {
                LogDebug(LOG_NET, "CC only mode, ignoring PDU from network");
            }

            return;
        }

        uint32_t blockLength = __GET_UINT16(buffer, 8U);

        if (m_debug) {
            LogDebug(LOG_NET, "P25, duid = $%02X, MFId = $%02X, blockLength = %u, len = %u", duid, MFId, blockLength, length);
        }

        if (!m_dedicatedControl)
            m_data->processNetwork(data.get(), frameLength, blockLength);

        return;
    }

    // handle LDU, TDU or TSDU frame
    uint8_t lco = buffer[4U];

    uint32_t srcId = __GET_UINT16(buffer, 5U);
    uint32_t dstId = __GET_UINT16(buffer, 8U);

    uint32_t sysId = (buffer[11U] << 8) | (buffer[12U] << 0);
    uint32_t netId = __GET_UINT16(buffer, 16U);

    uint8_t lsd1 = buffer[20U];
    uint8_t lsd2 = buffer[21U];

    FrameType::E frameType = FrameType::DATA_UNIT;

    // if the netId or sysId is missing; default to our netId and sysId
    if (netId == 0U) {
        netId = lc::LC::getSiteData().netId();
    }

    if (sysId == 0U) {
        sysId = lc::LC::getSiteData().sysId();
    }

    if (m_debug) {
        LogDebug(LOG_NET, "P25, duid = $%02X, lco = $%02X, MFId = $%02X, srcId = %u, dstId = %u, len = %u", duid, lco, MFId, srcId, dstId, length);
    }

    lc::LC control;
    data::LowSpeedData lsd;

    // is this a LDU1, is this the first of a call?
    if (duid == DUID::LDU1) {
        frameType = (FrameType::E)buffer[180U];

        if (m_debug) {
            LogDebug(LOG_NET, "P25, frameType = $%02X", frameType);
        }

        if (frameType == FrameType::HDU_VALID) {
            uint8_t algId = buffer[181U];
            uint32_t kid = (buffer[182U] << 8) | (buffer[183U] << 0);

            // copy MI data
            uint8_t mi[MI_LENGTH_BYTES];
            ::memset(mi, 0x00U, MI_LENGTH_BYTES);

            for (uint8_t i = 0; i < MI_LENGTH_BYTES; i++) {
                mi[i] = buffer[184U + i];
            }

            if (m_debug) {
                LogDebug(LOG_NET, "P25, HDU algId = $%02X, kId = $%02X", algId, kid);
                Utils::dump(1U, "P25 HDU Network MI", mi, MI_LENGTH_BYTES);
            }

            control.setAlgId(algId);
            control.setKId(kid);
            control.setMI(mi);
        }
    }

    control.setLCO(lco);
    control.setSrcId(srcId);
    control.setDstId(dstId);
    control.setMFId(MFId);

    control.setNetId(netId);
    control.setSysId(sysId);

    lsd.setLSD1(lsd1);
    lsd.setLSD2(lsd2);

    m_networkWatchdog.start();

    if (m_debug) {
        Utils::dump(2U, "* !!! P25 Network Frame", data.get(), frameLength);
    }

    // forward onto the specific processor for final processing and delivery
    switch (duid) {
        case DUID::HDU:
        case DUID::LDU1:
        case DUID::VSELP1:
        case DUID::VSELP2:
        case DUID::LDU2:
            if (m_controlOnly) {
                if (m_debug) {
                    LogDebug(LOG_NET, "CC only mode, ignoring HDU/LDU from network");
                }
                break;
            }

            m_voice->processNetwork(data.get(), frameLength, control, lsd, duid, frameType);
            break;

        case DUID::TDU:
        case DUID::TDULC:
            // is this an TDU with a grant demand?
            if (duid == DUID::TDU && m_enableControl && grantDemand) {
                if (m_disableNetworkGrant) {
                    return;
                }

                // if we're non-dedicated control, and if we're not in a listening or idle state, ignore any grant
                // demands
                if (!m_dedicatedControl && (m_rfState != RS_RF_LISTENING || m_netState != RS_NET_IDLE)) {
                    return;
                }

                // validate source RID
                if (!acl::AccessControl::validateSrcId(srcId)) {
                    return;
                }

                // validate the target ID, if the target is a talkgroup
                if (!acl::AccessControl::validateTGId(dstId)) {
                    return;
                }

                if (grantEncrypt)
                    control.setEncrypted(true); // make sure encrypted flag is set

                uint8_t serviceOptions = (control.getEmergency() ? 0x80U : 0x00U) +     // Emergency Flag
                    (control.getEncrypted() ? 0x40U : 0x00U) +                          // Encrypted Flag
                    (control.getPriority() & 0x07U);                                    // Priority

                if (m_verbose) {
                    LogMessage(LOG_NET, P25_TSDU_STR " remote grant demand, srcId = %u, dstId = %u, unitToUnit = %u, encrypted = %u", srcId, dstId, unitToUnit, grantEncrypt);
                }

                // are we denying the grant?
                if (grantDenial) {
                    m_control->writeRF_TSDU_Deny(srcId, dstId, ReasonCode::DENY_PTT_COLLIDE, (!unitToUnit) ? TSBKO::IOSP_GRP_VCH : TSBKO::IOSP_UU_VCH);
                    return;
                }

                // perform grant response logic
                m_control->writeRF_TSDU_Grant(srcId, dstId, serviceOptions, !unitToUnit, true);
/*
                if (!m_control->writeRF_TSDU_Grant(srcId, dstId, serviceOptions, !unitToUnit, true))
                {
                    LogError(LOG_NET, P25_TSDU_STR " call rejected, network call not granted, dstId = %u", dstId);
                    return;
                }
*/
                return;
            }

            m_voice->processNetwork(data.get(), frameLength, control, lsd, duid, frameType);
            break;

        case DUID::TSDU:
            m_control->processNetwork(data.get(), frameLength, control, lsd, duid);
            break;

        default:
            break;
        }
}

/* Helper to process loss of frame stream from modem. */

void Control::processFrameLoss()
{
    if (m_rfState == RS_RF_AUDIO) {
        if (m_rssi != 0U) {
            ::ActivityLog("P25", true, "transmission lost, %.1f seconds, BER: %.1f%%, RSSI: -%u/-%u/-%u dBm, loss count: %u",
                float(m_voice->m_rfFrames) / 5.56F, float(m_voice->m_rfErrs * 100U) / float(m_voice->m_rfBits), m_minRSSI, m_maxRSSI, m_aveRSSI / m_rssiCount, m_frameLossCnt);
        }
        else {
            ::ActivityLog("P25", true, "transmission lost, %.1f seconds, BER: %.1f%%, loss count: %u",
                float(m_voice->m_rfFrames) / 5.56F, float(m_voice->m_rfErrs * 100U) / float(m_voice->m_rfBits), m_frameLossCnt);
        }

        LogMessage(LOG_RF, P25_TDU_STR ", total frames: %d, bits: %d, undecodable LC: %d, errors: %d, BER: %.4f%%",
            m_voice->m_rfFrames, m_voice->m_rfBits, m_voice->m_rfUndecodableLC, m_voice->m_rfErrs, float(m_voice->m_rfErrs * 100U) / float(m_voice->m_rfBits));

        m_affiliations->releaseGrant(m_voice->m_rfLC.getDstId(), false);
        if (!m_enableControl) {
            notifyCC_ReleaseGrant(m_voice->m_rfLC.getDstId());
        }
        m_control->writeNet_TSDU_Call_Term(m_voice->m_rfLC.getSrcId(), m_voice->m_rfLC.getDstId());

        writeRF_TDU(false);
        m_voice->m_lastDUID = DUID::TDU;
        m_voice->writeNet_TDU();

        m_rfState = RS_RF_LISTENING;
        m_rfLastDstId = 0U;
        m_rfLastSrcId = 0U;
        m_rfTGHang.stop();

        m_tailOnIdle = true;

        m_rfTimeout.stop();
        m_txQueue.clear();

        if (m_network != nullptr)
            m_network->resetP25();
    }

    if (m_rfState == RS_RF_DATA) {
        m_rfState = RS_RF_LISTENING;
        m_rfLastDstId = 0U;
        m_rfLastSrcId = 0U;
        m_rfTGHang.stop();

        m_tailOnIdle = true;

        m_data->resetRF();

        m_rfTimeout.stop();
        m_txQueue.clear();
    }

    m_voice->resetRF();
    m_data->resetRF();

    // if voice on control; and CC is halted restart CC
    if (m_voiceOnControl && m_ccHalted) {
        m_ccHalted = false;
        writeRF_ControlData();
    }
}

/* Helper to process an In-Call Control message. */

void Control::processInCallCtrl(network::NET_ICC::ENUM command, uint32_t dstId)
{
    switch (command) {
    case network::NET_ICC::REJECT_TRAFFIC:
        {
            if (m_rfState == RS_RF_AUDIO && m_voice->m_rfLC.getDstId() == dstId) {
                LogWarning(LOG_P25, "network requested in-call traffic reject, dstId = %u", dstId);
                if (m_affiliations->isGranted(dstId)) {
                    uint32_t srcId = m_affiliations->getGrantedSrcId(dstId);

                    m_affiliations->releaseGrant(dstId, false);
                    if (!m_enableControl) {
                        notifyCC_ReleaseGrant(dstId);
                    }
                    m_control->writeNet_TSDU_Call_Term(srcId, dstId);

                    m_voice->m_rfLC.setSrcId(srcId);
                    m_voice->m_rfLC.setDstId(dstId);
                }

                processFrameLoss();

                m_rfLastDstId = 0U;
                m_rfLastSrcId = 0U;
                m_rfTGHang.stop();
                m_rfState = RS_RF_REJECTED;
            }
        }
        break;

    default:
        break;
    }
}

/* Helper to send a REST API request to the CC to release a channel grant at the end of a call. */

void Control::notifyCC_ReleaseGrant(uint32_t dstId)
{
    if (m_controlChData.address().empty()) {
        return;
    }

    if (m_controlChData.port() == 0) {
        return;
    }

    if (!m_notifyCC) {
        return;
    }

    if (m_verbose) {
        LogMessage(LOG_P25, "CC %s:%u, notifying CC of call termination, dstId = %u", m_controlChData.address().c_str(), m_controlChData.port(), dstId);
    }

    // callback REST API to release the granted TG on the specified control channel
    json::object req = json::object();
    req["dstId"].set<uint32_t>(dstId);

    g_RPC->req(RPC_RELEASE_P25_TG, req, [=](json::object& req, json::object& reply) {
        if (!req["status"].is<int>()) {
            ::LogError(LOG_P25, "failed to notify the CC %s:%u of the release of, dstId = %u, invalid RPC response", m_controlChData.address().c_str(), m_controlChData.port(), dstId);
            return;
        }

        int status = req["status"].get<int>();
        if (status != network::NetRPC::OK) {
            ::LogError(LOG_P25, "failed to notify the CC %s:%u of the release of, dstId = %u", m_controlChData.address().c_str(), m_controlChData.port(), dstId);
            if (req["message"].is<std::string>()) {
                std::string retMsg = req["message"].get<std::string>();
                ::LogError(LOG_P25, "RPC failed, %s", retMsg.c_str());
            }
        } 
        else
            ::LogMessage(LOG_P25, "CC %s:%u, released grant, dstId = %u", m_controlChData.address().c_str(), m_controlChData.port(), dstId);
    }, m_controlChData.address(), m_controlChData.port());

    m_rfLastDstId = 0U;
    m_rfLastSrcId = 0U;
    m_netLastDstId = 0U;
    m_netLastSrcId = 0U;
}

/* Helper to send a REST API request to the CC to "touch" a channel grant to refresh grant timers. */

void Control::notifyCC_TouchGrant(uint32_t dstId)
{
    if (m_controlChData.address().empty()) {
        return;
    }

    if (m_controlChData.port() == 0) {
        return;
    }

    if (!m_notifyCC) {
        return;
    }

    // callback REST API to touch the granted TG on the specified control channel
    json::object req = json::object();
    req["dstId"].set<uint32_t>(dstId);

    g_RPC->req(RPC_TOUCH_P25_TG, req, [=](json::object& req, json::object& reply) {
        if (!req["status"].is<int>()) {
            ::LogError(LOG_P25, "failed to notify the CC %s:%u of the touch of, dstId = %u, invalid RPC response", m_controlChData.address().c_str(), m_controlChData.port(), dstId);
            return;
        }

        int status = req["status"].get<int>();
        if (status != network::NetRPC::OK) {
            ::LogError(LOG_P25, "failed to notify the CC %s:%u of the touch of, dstId = %u", m_controlChData.address().c_str(), m_controlChData.port(), dstId);
            if (req["message"].is<std::string>()) {
                std::string retMsg = req["message"].get<std::string>();
                ::LogError(LOG_P25, "RPC failed, %s", retMsg.c_str());
            }
        }
        else
            ::LogMessage(LOG_P25, "CC %s:%u, touched grant, dstId = %u", m_controlChData.address().c_str(), m_controlChData.port(), dstId);
    }, m_controlChData.address(), m_controlChData.port());
}

/* Helper to write control channel frame data. */

bool Control::writeRF_ControlData()
{
    if (!m_enableControl)
        return false;

    if (m_ccFrameCnt == 254U) {
        m_ccFrameCnt = 0U;
    }

    // don't add any frames if the queue is full
    uint8_t len = (P25_TSDU_TRIPLE_FRAME_LENGTH_BYTES * 2U) + 2U;
    uint32_t space = m_txQueue.freeSpace();
    if (space < (len + 1U)) {
        return false;
    }

    const uint8_t maxSeq = 9U;
    if (m_ccSeq == maxSeq) {
        m_ccSeq = 0U;
    }

    if (!m_dedicatedControl || m_voiceOnControl) {
        if (m_netState != RS_NET_IDLE && m_rfState != RS_RF_LISTENING) {
            return false;
        }
    }

    m_control->writeRF_ControlData(m_ccFrameCnt, m_ccSeq, true);

    m_ccSeq++;
    if (m_ccSeq == maxSeq) {
        m_ccFrameCnt++;
    }

    return true;
}

/* Helper to write end of control channel frame data. */

bool Control::writeRF_ControlEnd()
{
    if (!m_enableControl)
        return false;

    m_txQueue.clear();
    m_ccPacketInterval.stop();
    m_adjSiteUpdate.stop();

    if (m_netState == RS_NET_IDLE && m_rfState == RS_RF_LISTENING) {
        for (uint32_t i = 0; i < TSBK_PCH_CCH_CNT; i++) {
            m_control->queueRF_TSBK_Ctrl(TSBKO::OSP_MOT_PSH_CCH);
        }

        writeRF_Nulls();
        return true;
    }

    return false;
}

/* Helper to write data nulls. */

void Control::writeRF_Nulls()
{
    const uint8_t NULLS_LENGTH_BYTES = 25U;

    // write null bits (0x00)
    uint8_t data[NULLS_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, NULLS_LENGTH_BYTES);

    data[0U] = modem::TAG_EOT;
    data[1U] = 0x00U;

    addFrame(data, NULLS_LENGTH_BYTES + 2U);
}

/* Helper to write TDU preamble packet burst. */

void Control::writeRF_Preamble(uint32_t preambleCount, bool force)
{
    if (!m_duplex && !force)
        return;

    if (m_isModemDFSI)
        return;

    if (preambleCount == 0) {
        preambleCount = m_tduPreambleCount;
    }

    if (!force) {
        if (m_modem->hasTX() || m_tduPreambleCount == 0U) {
            return;
        }

        if (m_ccRunning) {
            return;
        }
    }

    if (m_tduPreambleCount > MAX_PREAMBLE_TDU_CNT) {
        LogWarning(LOG_P25, "oversized TDU preamble count, reducing to maximum %u", MAX_PREAMBLE_TDU_CNT);
        preambleCount = m_tduPreambleCount = MAX_PREAMBLE_TDU_CNT;
    }

    // write TDUs if requested
    for (uint8_t i = 0U; i < preambleCount; i++) {
        writeRF_TDU(true);
    }
}

/* Helper to write a P25 TDU packet. */

void Control::writeRF_TDU(bool noNetwork, bool imm)
{
    uint8_t data[P25_TDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, P25_TDU_FRAME_LENGTH_BYTES);

    // generate Sync
    Sync::addP25Sync(data + 2U);

    // generate NID
    m_nid.encode(data + 2U, DUID::TDU);

    // add status bits
    P25Utils::setStatusBitsAllIdle(data + 2U, P25_TDU_FRAME_LENGTH_BITS);

    if (!noNetwork)
        m_voice->writeNetwork(data + 2U, DUID::TDU);

    if (m_duplex) {
        data[0U] = modem::TAG_EOT;
        data[1U] = 0x00U;

        addFrame(data, P25_TDU_FRAME_LENGTH_BYTES + 2U, false, imm);
    }
}

/* (RPC Handler) Permits a TGID on a non-authoritative host. */

void Control::RPC_permittedTG(json::object& req, json::object& reply)
{
    g_RPC->defaultResponse(reply, "OK", network::NetRPC::OK);

    // validate destination ID is a integer within the JSON blob
    if (!req["dstId"].is<int>()) {
        g_RPC->defaultResponse(reply, "destination ID was not a valid integer", network::NetRPC::INVALID_ARGS);
        return;
    }

    uint32_t dstId = req["dstId"].get<uint32_t>();
    bool dataPermit = false;

    // validate destination ID is a integer within the JSON blob
    if (req["dataPermit"].is<bool>()) {
        dataPermit = (bool)req["dataPermit"].get<bool>();
    }

    // LogDebugEx(LOG_P25, "Control::RPC_permittedTG()", "callback, dstId = %u, dataPermit = %u", dstId, dataPermit);

    permittedTG(dstId, dataPermit);
}

/* (RPC Handler) Active TGID list from the authoritative CC host. */

void Control::RPC_activeTG(json::object& req, json::object& reply)
{
    g_RPC->defaultResponse(reply, "OK", network::NetRPC::OK);

    if (!req["active"].is<json::array>()) {
        g_RPC->defaultResponse(reply, "\"active\" was not a valid JSON array", network::NetRPC::INVALID_ARGS);
        return;
    }
    json::array active = req["active"].get<json::array>();

    std::lock_guard<std::mutex> lock(m_activeTGLock);
    m_activeTG.clear();

    if (active.size() > 0) {
        for (auto entry : active) {
            if (!entry.is<uint32_t>()) {
                g_RPC->defaultResponse(reply, "active TG was not a valid number", network::NetRPC::INVALID_ARGS);
                continue;
            }

            m_activeTG.push_back(entry.get<uint32_t>());
        }
    }

    ::LogMessage(LOG_P25, "active TG update, activeCnt = %u", m_activeTG.size());
}

/* (RPC Handler) Clear active TGID list from the authoritative CC host. */

void Control::RPC_clearActiveTG(json::object& req, json::object& reply)
{
    g_RPC->defaultResponse(reply, "OK", network::NetRPC::OK);

    if (m_activeTG.size() > 0) {
        std::lock_guard<std::mutex> lock(m_activeTGLock);
        m_activeTG.clear();
    }
}

/* (RPC Handler) Releases a granted TG. */

void Control::RPC_releaseGrantTG(json::object& req, json::object& reply)
{
    if (!m_enableControl) {
        g_RPC->defaultResponse(reply, "not P25 control channel", network::NetRPC::BAD_REQUEST);
        return;
    }

    g_RPC->defaultResponse(reply, "OK", network::NetRPC::OK);

    // validate destination ID is a integer within the JSON blob
    if (!req["dstId"].is<int>()) {
        g_RPC->defaultResponse(reply, "destination ID was not a valid integer", network::NetRPC::INVALID_ARGS);
        return;
    }

    uint32_t dstId = req["dstId"].get<uint32_t>();

    if (dstId == 0U) {
        g_RPC->defaultResponse(reply, "destination ID is an illegal TGID");
        return;
    }

    // LogDebugEx(LOG_P25, "Control::RPC_releaseGrantTG()", "callback, dstId = %u", dstId);

    if (m_verbose) {
        LogMessage(LOG_P25, "VC request, release TG grant, dstId = %u", dstId);
    }

    if (m_affiliations->isGranted(dstId)) {
        uint32_t chNo = m_affiliations->getGrantedCh(dstId);
        uint32_t srcId = m_affiliations->getGrantedSrcId(dstId);
        ::lookups::VoiceChData voiceCh = m_affiliations->rfCh()->getRFChData(chNo);

        if (m_verbose) {
            LogMessage(LOG_P25, "VC %s:%u, TG grant released, srcId = %u, dstId = %u, chNo = %u-%u", voiceCh.address().c_str(), voiceCh.port(), srcId, dstId, voiceCh.chId(), chNo);
        }

        m_affiliations->releaseGrant(dstId, false);
    }
}

/* (RPC Handler) Touches a granted TG to keep a channel grant alive. */

void Control::RPC_touchGrantTG(json::object& req, json::object& reply)
{
    if (!m_enableControl) {
        g_RPC->defaultResponse(reply, "not P25 control channel");
        return;
    }

    g_RPC->defaultResponse(reply, "OK", network::NetRPC::OK);

    // validate destination ID is a integer within the JSON blob
    if (!req["dstId"].is<int>()) {
        g_RPC->defaultResponse(reply, "destination ID was not a valid integer", network::NetRPC::INVALID_ARGS);
        return;
    }

    uint32_t dstId = req["dstId"].get<uint32_t>();

    if (dstId == 0U) {
        g_RPC->defaultResponse(reply, "destination ID is an illegal TGID");
        return;
    }

    // LogDebugEx(LOG_P25, "Control::RPC_touchGrantTG()", "callback, dstId = %u", dstId);

    if (m_affiliations->isGranted(dstId)) {
        uint32_t chNo = m_affiliations->getGrantedCh(dstId);
        uint32_t srcId = m_affiliations->getGrantedSrcId(dstId);
        ::lookups::VoiceChData voiceCh = m_affiliations->rfCh()->getRFChData(chNo);

        if (m_verbose) {
            LogMessage(LOG_P25, "VC %s:%u, call in progress, srcId = %u, dstId = %u, chNo = %u-%u", voiceCh.address().c_str(), voiceCh.port(), srcId, dstId, voiceCh.chId(), chNo);
        }

        m_affiliations->touchGrant(dstId);
    }
}

/* Helper to setup and generate LLA AM1 parameters. */

void Control::generateLLA_AM1_Parameters()
{
    ::memset(m_llaRS, 0x00U, AUTH_KEY_LENGTH_BYTES);
    ::memset(m_llaCRS, 0x00U, AUTH_KEY_LENGTH_BYTES);
    ::memset(m_llaKS, 0x00U, AUTH_KEY_LENGTH_BYTES);

    if (m_llaK == nullptr) {
        return;
    }

    crypto::AES* aes = new crypto::AES(crypto::AESKeyLength::AES_128);

    // generate new RS
    uint8_t RS[AUTH_RAND_SEED_LENGTH_BYTES];
    std::uniform_int_distribution<uint32_t> dist(DVM_RAND_MIN, DVM_RAND_MAX);
    uint32_t rnd = dist(m_random);
    __SET_UINT32(rnd, RS, 0U);

    rnd = dist(m_random);
    __SET_UINT32(rnd, RS, 4U);

    rnd = dist(m_random);
    RS[9U] = (uint8_t)(rnd & 0xFFU);

    // expand RS to 16 bytes
    for (uint32_t i = 0; i < AUTH_RAND_SEED_LENGTH_BYTES; i++)
        m_llaRS[i] = RS[i];

    // complement RS
    for (uint32_t i = 0; i < AUTH_KEY_LENGTH_BYTES; i++)
        m_llaCRS[i] = ~m_llaRS[i];

    // perform crypto
    uint8_t* KS = aes->encryptECB(m_llaRS, AUTH_KEY_LENGTH_BYTES * sizeof(uint8_t), m_llaK);
    ::memcpy(m_llaKS, KS, AUTH_KEY_LENGTH_BYTES);

    if (m_verbose) {
        LogMessage(LOG_P25, "P25, generated LLA AM1 parameters");
    }

    // cleanup
    delete[] KS;
    delete aes;
}
