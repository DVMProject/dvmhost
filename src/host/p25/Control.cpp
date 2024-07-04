// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016,2017,2018 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
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
#include "remote/RESTClient.h"
#include "ActivityLog.h"
#include "HostMain.h"

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
    m_idenTable(idenTable),
    m_ridLookup(ridLookup),
    m_tidLookup(tidLookup),
    m_affiliations(this, chLookup, verbose),
    m_controlChData(),
    m_idenEntry(),
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
    m_notifyCC(true),
    m_verbose(verbose),
    m_debug(debug)
{
    assert(chLookup != nullptr);
    assert(ridLookup != nullptr);
    assert(tidLookup != nullptr);
    assert(idenTable != nullptr);
    assert(rssiMapper != nullptr);

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
}

/* Finalizes a instance of the Control class. */

Control::~Control()
{
    if (m_voice != nullptr) {
        delete m_voice;
    }

    if (m_control != nullptr) {
        delete m_control;
    }

    if (m_data != nullptr) {
        delete m_data;
    }
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
    m_control->m_ctrlTimeDateAnn = control["enableTimeDateAnn"].as<bool>(false);
    m_control->m_redundantImmediate = control["redundantImmediate"].as<bool>(true);
    m_control->m_redundantGrant = control["redundantGrantTransmit"].as<bool>(false);

    m_allowExplicitSourceId = p25Protocol["allowExplicitSourceId"].as<bool>(true);
    m_convNetGrantDemand = p25Protocol["convNetGrantDemand"].as<bool>(false);

    uint32_t ccBcstInterval = p25Protocol["control"]["interval"].as<uint32_t>(300U);
    m_control->m_adjSiteUpdateInterval += ccBcstInterval;

    m_control->m_disableGrantSrcIdCheck = p25Protocol["control"]["disableGrantSourceIdCheck"].as<bool>(false);

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

    m_siteData.setChCnt((uint8_t)m_affiliations.rfCh()->rfChSize());

    m_controlChData = controlChData;

    bool disableUnitRegTimeout = p25Protocol["disableUnitRegTimeout"].as<bool>(false);
    m_affiliations.setDisableUnitRegTimeout(disableUnitRegTimeout);

    // set the grant release callback
    m_affiliations.setReleaseGrantCallback([=](uint32_t chNo, uint32_t dstId, uint8_t slot) {
        // callback REST API to clear TG permit for the granted TG on the specified voice channel
        if (m_authoritative && m_supervisor) {
            ::lookups::VoiceChData voiceChData = m_affiliations.rfCh()->getRFChData(chNo);
            if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0 &&
                chNo != m_siteData.channelNo()) {
                json::object req = json::object();
                int state = modem::DVM_STATE::STATE_P25;
                req["state"].set<int>(state);
                dstId = 0U; // clear TG value
                req["dstId"].set<uint32_t>(dstId);

                RESTClient::send(voiceChData.address(), voiceChData.port(), voiceChData.password(),
                    HTTP_PUT, PUT_PERMIT_TG, req, voiceChData.ssl(), REST_DEFAULT_WAIT, m_debug);
            }
            else {
                ::LogError(LOG_P25, P25_TSDU_STR ", TSBKO, IOSP_GRP_VCH (Group Voice Channel Grant), failed to clear TG permit, chNo = %u", chNo);
            }
        }
    });

    // set the unit deregistration callback
    m_affiliations.setUnitDeregCallback([=](uint32_t srcId, bool automatic) {
        if (m_network != nullptr)
            m_network->announceUnitDeregistration(srcId);

        // fire off a U_REG_CMD to get the SU to re-affiliate
        if (m_enableControl && automatic) {
            m_control->writeRF_TSDU_U_Reg_Cmd(srcId);
        }
    });

    if (printOptions) {
        LogInfo("    Silence Threshold: %u (%.1f%%)", m_voice->m_silenceThreshold, float(m_voice->m_silenceThreshold) / 12.33F);
        LogInfo("    Frame Loss Threshold: %u", m_frameLossThreshold);

        if (m_enableControl) {
            LogInfo("    Ack Requests: %s", m_ackTSBKRequests ? "yes" : "no");
            if (m_control->m_disableGrantSrcIdCheck) {
                LogInfo("    Disable Grant Source ID Check: yes");
            }
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

        LogInfo("    No Status ACK: %s", m_control->m_noStatusAck ? "yes" : "no");
        LogInfo("    No Message ACK: %s", m_control->m_noMessageAck ? "yes" : "no");
        LogInfo("    Unit-to-Unit Availability Check: %s", m_control->m_unitToUnitAvailCheck ? "yes" : "no");
        LogInfo("    Explicit Source ID Support: %s", m_allowExplicitSourceId ? "yes" : "no");
        LogInfo("    Conventional Network Grant Demand: %s", m_convNetGrantDemand ? "yes" : "no");

        if (disableUnitRegTimeout) {
            LogInfo("    Disable Unit Registration Timeout: yes");
        }

        LogInfo("    Redundant Immediate: %s", m_control->m_redundantImmediate ? "yes" : "no");
        if (m_control->m_redundantGrant) {
            LogInfo("    Redundant Grant Transmit: yes");
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
            LogWarning(LOG_RF, "P25, possible sync word rejected, errs = %u, sync word = %02X %02X %02X %02X %02X %02X", errs,
                syncBytes[0U], syncBytes[1U], syncBytes[2U], syncBytes[3U], syncBytes[4U], syncBytes[5U]);

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
                if (m_voiceOnControl && m_affiliations.isChBusy(m_siteData.channelNo()))
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
    if (m_txQueue.isEmpty() && m_txImmQueue.isEmpty())
        return 0U;

    uint8_t len = 0U;

    // tx immediate queue takes priority
    if (!m_txImmQueue.isEmpty()) {
        m_txImmQueue.peek(&len, 1U);
    }
    else {
        m_txQueue.peek(&len, 1U);
    }

    return len;
}

/* Get frame data from data ring buffer. */

uint32_t Control::getFrame(uint8_t* data)
{
    assert(data != nullptr);

    if (m_txQueue.isEmpty() && m_txImmQueue.isEmpty())
        return 0U;

    uint8_t len = 0U;

    // tx immediate queue takes priority
    if (!m_txImmQueue.isEmpty()) {
        m_txImmQueue.get(&len, 1U);
        m_txImmQueue.get(data, len);
    }
    else {
        m_txQueue.get(&len, 1U);
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
            m_affiliations.releaseGrant(m_voice->m_netLC.getDstId(), false);

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

    // reset states if we're in a rejected state
    if (m_rfState == RS_RF_REJECTED) {
        m_txQueue.clear();

        m_voice->resetRF();
        m_voice->resetNet();

        m_data->resetRF();

        if (m_network != nullptr)
            m_network->resetP25();

        m_rfState = RS_RF_LISTENING;
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
        m_affiliations.clock(ms);
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
                    if (m_affiliations.grpAffSize() > 0) {
                        auto affs = m_affiliations.grpAffTable();
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
                            LogWarning(LOG_NET, "P25, Adjacent Site Status Expired, no data [FAILED], sysId = $%03X, rfss = $%02X, site = $%02X, chId = %u, chNo = %u, svcClass = $%02X",
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
                            LogWarning(LOG_NET, "P25, Secondary Control Channel Expired, no data [FAILED], sysId = $%03X, rfss = $%02X, site = $%02X, chId = %u, chNo = %u, svcClass = $%02X",
                                siteData.sysId(), siteData.rfssId(), siteData.siteId(), siteData.channelId(), siteData.channelNo(), siteData.serviceClass());
                        }

                        entry.second = updateCnt;
                    }
                }

                m_control->m_adjSiteUpdateTimer.setTimeout(m_control->m_adjSiteUpdateInterval);
                m_control->m_adjSiteUpdateTimer.start();
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

/* Releases a granted TG. */

void Control::releaseGrantTG(uint32_t dstId)
{
    if (!m_enableControl) {
        return;
    }

    if (m_verbose) {
        LogMessage(LOG_P25, "VC request, release TG grant, dstId = %u", dstId);
    }

    if (m_affiliations.isGranted(dstId)) {
        uint32_t chNo = m_affiliations.getGrantedCh(dstId);
        uint32_t srcId = m_affiliations.getGrantedSrcId(dstId);
        ::lookups::VoiceChData voiceCh = m_affiliations.rfCh()->getRFChData(chNo);

        if (m_verbose) {
            LogMessage(LOG_P25, "VC %s:%u, TG grant released, srcId = %u, dstId = %u, chId = %u, chNo = %u", voiceCh.address().c_str(), voiceCh.port(), srcId, dstId, voiceCh.chId(), chNo);
        }

        m_affiliations.releaseGrant(dstId, false);
    }
}

/* Touches a granted TG to keep a channel grant alive. */

void Control::touchGrantTG(uint32_t dstId)
{
    if (!m_enableControl) {
        return;
    }

    if (m_affiliations.isGranted(dstId)) {
        uint32_t chNo = m_affiliations.getGrantedCh(dstId);
        uint32_t srcId = m_affiliations.getGrantedSrcId(dstId);
        ::lookups::VoiceChData voiceCh = m_affiliations.rfCh()->getRFChData(chNo);

        if (m_verbose) {
            LogMessage(LOG_P25, "VC %s:%u, call in progress, srcId = %u, dstId = %u, chId = %u, chNo = %u", voiceCh.address().c_str(), voiceCh.port(), srcId, dstId, voiceCh.chId(), chNo);
        }

        m_affiliations.touchGrant(dstId);
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

    // is this immediate data?
    if (imm) {
        // resize immediate queue if necessary (this shouldn't really ever happen)
        uint32_t space = m_txImmQueue.freeSpace();
        if (space < (length + 1U)) {
            if (!net) {
                uint32_t queueLen = m_txImmQueue.length();
                m_txImmQueue.resize(queueLen + P25_LDU_FRAME_LENGTH_BYTES);
                LogError(LOG_P25, "overflow in the P25 queue while writing imm data; queue free is %u, needed %u; resized was %u is %u", space, length, queueLen, m_txImmQueue.length());
                return;
            }
            else {
                LogError(LOG_P25, "overflow in the P25 queue while writing imm network data; queue free is %u, needed %u", space, length);
                return;
            }
        }

        uint8_t len = length;
        m_txImmQueue.addData(&len, 1U);
        m_txImmQueue.addData(data, len);
        return;
    }

    // resize queue if necessary (this shouldn't really ever happen)
    uint32_t space = m_txQueue.freeSpace();
    if (space < (length + 1U)) {
        if (!net) {
            uint32_t queueLen = m_txQueue.length();
            m_txQueue.resize(queueLen + P25_LDU_FRAME_LENGTH_BYTES);
            LogError(LOG_P25, "overflow in the P25 queue while writing data; queue free is %u, needed %u; resized was %u is %u", space, length, queueLen, m_txQueue.length());
            return;
        }
        else {
            LogError(LOG_P25, "overflow in the P25 queue while writing network data; queue free is %u, needed %u", space, length);
            return;
        }
    }

    uint8_t len = length;
    m_txQueue.addData(&len, 1U);
    m_txQueue.addData(data, len);
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
            ret = m_data->processNetwork(data.get(), frameLength, blockLength);

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

            ret = m_voice->processNetwork(data.get(), frameLength, control, lsd, duid, frameType);
            break;

        case DUID::TDU:
        case DUID::TDULC:
            // is this an TDU with a grant demand?
            if (duid == DUID::TDU && m_enableControl && grantDemand) {
                if (m_disableNetworkGrant) {
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

                uint8_t serviceOptions = (control.getEmergency() ? 0x80U : 0x00U) +     // Emergency Flag
                    (control.getEncrypted() ? 0x40U : 0x00U) +                          // Encrypted Flag
                    (control.getPriority() & 0x07U);                                    // Priority

                if (m_verbose) {
                    LogMessage(LOG_NET, P25_TSDU_STR " remote grant demand, srcId = %u, dstId = %u, unitToUnit = %u", srcId, dstId, unitToUnit);
                }

                // are we denying the grant?
                if (grantDenial) {
                    m_control->writeRF_TSDU_Deny(srcId, dstId, ReasonCode::DENY_PTT_COLLIDE, (!unitToUnit) ? TSBKO::IOSP_GRP_VCH : TSBKO::IOSP_UU_VCH);
                    return;
                }

                // perform grant response logic
                if (!m_control->writeRF_TSDU_Grant(srcId, dstId, serviceOptions, !unitToUnit, true))
                {
                    LogError(LOG_NET, P25_TSDU_STR " call failure, network call not granted, dstId = %u", dstId);
                    return;
                }

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

        m_affiliations.releaseGrant(m_voice->m_rfLC.getDstId(), false);
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
    int state = modem::DVM_STATE::STATE_P25;
    req["state"].set<int>(state);
    req["dstId"].set<uint32_t>(dstId);

    int ret = RESTClient::send(m_controlChData.address(), m_controlChData.port(), m_controlChData.password(),
        HTTP_PUT, PUT_RELEASE_TG, req, m_controlChData.ssl(), REST_QUICK_WAIT, m_debug);
    if (ret != network::rest::http::HTTPPayload::StatusType::OK) {
        ::LogError(LOG_P25, "failed to notify the CC %s:%u of the release of, dstId = %u", m_controlChData.address().c_str(), m_controlChData.port(), dstId);
    }

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
    int state = modem::DVM_STATE::STATE_P25;
    req["state"].set<int>(state);
    req["dstId"].set<uint32_t>(dstId);

    int ret = RESTClient::send(m_controlChData.address(), m_controlChData.port(), m_controlChData.password(),
        HTTP_PUT, PUT_TOUCH_TG, req, m_controlChData.ssl(), REST_QUICK_WAIT, m_debug);
    if (ret != network::rest::http::HTTPPayload::StatusType::OK) {
        ::LogError(LOG_P25, "failed to notify the CC %s:%u of the touch of, dstId = %u", m_controlChData.address().c_str(), m_controlChData.port(), dstId);
    }
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

    if (m_debug) {
        LogDebug(LOG_P25, "writeRF_Nulls()");
    }

    addFrame(data, NULLS_LENGTH_BYTES + 2U);
}

/* Helper to write TDU preamble packet burst. */

void Control::writeRF_Preamble(uint32_t preambleCount, bool force)
{
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

    // Generate Sync
    Sync::addP25Sync(data + 2U);

    // Generate NID
    m_nid.encode(data + 2U, DUID::TDU);

    // Add busy bits
    P25Utils::addBusyBits(data + 2U, P25_TDU_FRAME_LENGTH_BITS, true, true);

    if (!noNetwork)
        m_voice->writeNetwork(data + 2U, DUID::TDU);

    if (m_duplex) {
        data[0U] = modem::TAG_EOT;
        data[1U] = 0x00U;

        addFrame(data, P25_TDU_FRAME_LENGTH_BYTES + 2U, false, imm);
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
