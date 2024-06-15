// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Modem Host Software
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2015-2020 Jonathan Naylor, G4KLX
*   Copyright (C) 2022-2024 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "common/nxdn/NXDNDefines.h"
#include "common/nxdn/acl/AccessControl.h"
#include "common/nxdn/channel/SACCH.h"
#include "common/nxdn/channel/FACCH1.h"
#include "common/nxdn/lc/RCCH.h"
#include "common/nxdn/lc/RTCH.h"
#include "common/nxdn/Sync.h"
#include "common/nxdn/NXDNUtils.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "nxdn/Control.h"
#include "remote/RESTClient.h"
#include "ActivityLog.h"

using namespace nxdn;
using namespace nxdn::packet;

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint8_t MAX_SYNC_BYTES_ERRS = 0U;

const uint8_t SCRAMBLER[] = {
    0x00U, 0x00U, 0x00U, 0x82U, 0xA0U, 0x88U, 0x8AU, 0x00U, 0xA2U, 0xA8U, 0x82U, 0x8AU, 0x82U, 0x02U,
    0x20U, 0x08U, 0x8AU, 0x20U, 0xAAU, 0xA2U, 0x82U, 0x08U, 0x22U, 0x8AU, 0xAAU, 0x08U, 0x28U, 0x88U,
    0x28U, 0x28U, 0x00U, 0x0AU, 0x02U, 0x82U, 0x20U, 0x28U, 0x82U, 0x2AU, 0xAAU, 0x20U, 0x22U, 0x80U,
    0xA8U, 0x8AU, 0x08U, 0xA0U, 0xAAU, 0x02U };


// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the Control class.
/// </summary>
/// <param name="authoritative">Flag indicating whether or not the DVM is grant authoritative.</param>
/// <param name="ran">NXDN Radio Access Number.</param>
/// <param name="callHang">Amount of hangtime for a NXDN call.</param>
/// <param name="queueSize">Modem frame buffer queue size (bytes).</param>
/// <param name="timeout">Transmit timeout.</param>
/// <param name="tgHang">Amount of time to hang on the last talkgroup mode from RF.</param>
/// <param name="modem">Instance of the Modem class.</param>
/// <param name="network">Instance of the BaseNetwork class.</param>
/// <param name="duplex">Flag indicating full-duplex operation.</param>
/// <param name="chLookup">Instance of the ChannelLookup class.</param>
/// <param name="ridLookup">Instance of the RadioIdLookup class.</param>
/// <param name="tidLookup">Instance of the TalkgroupRulesLookup class.</param>
/// <param name="idenTable">Instance of the IdenTableLookup class.</param>
/// <param name="rssi">Instance of the RSSIInterpolator class.</param>
/// <param name="dumpRCCHData">Flag indicating whether RCCH data is dumped to the log.</param>
/// <param name="debug">Flag indicating whether P25 debug is enabled.</param>
/// <param name="verbose">Flag indicating whether P25 verbose logging is enabled.</param>
Control::Control(bool authoritative, uint32_t ran, uint32_t callHang, uint32_t queueSize, uint32_t timeout, uint32_t tgHang,
    modem::Modem* modem, network::Network* network, bool duplex, lookups::ChannelLookup* chLookup, lookups::RadioIdLookup* ridLookup,
    lookups::TalkgroupRulesLookup* tidLookup, lookups::IdenTableLookup* idenTable, lookups::RSSIInterpolator* rssiMapper,
    bool dumpRCCHData, bool debug, bool verbose) :
    m_voice(nullptr),
    m_data(nullptr),
    m_control(nullptr),
    m_authoritative(authoritative),
    m_supervisor(false),
    m_ran(ran),
    m_timeout(timeout),
    m_modem(modem),
    m_network(network),
    m_duplex(duplex),
    m_enableControl(false),
    m_dedicatedControl(false),
    m_rfLastLICH(),
    m_rfLC(),
    m_netLC(),
    m_permittedDstId(0U),
    m_rfMask(0U),
    m_netMask(0U),
    m_idenTable(idenTable),
    m_ridLookup(ridLookup),
    m_tidLookup(tidLookup),
    m_affiliations("NXDN Affiliations", chLookup, verbose),
    m_controlChData(),
    m_idenEntry(),
    m_txImmQueue(queueSize, "NXDN Imm Frame"),
    m_txQueue(queueSize, "NXDN Frame"),
    m_rfState(RS_RF_LISTENING),
    m_rfLastDstId(0U),
    m_rfLastSrcId(0U),
    m_netState(RS_NET_IDLE),
    m_netLastDstId(0U),
    m_netLastSrcId(0U),
    m_ccRunning(false),
    m_ccPrevRunning(false),
    m_ccHalted(false),
    m_rfTimeout(1000U, timeout),
    m_rfTGHang(1000U, tgHang),
    m_rfLossWatchdog(1000U, 0U, 1500U),
    m_netTimeout(1000U, timeout),
    m_netTGHang(1000U, 2U),
    m_networkWatchdog(1000U, 0U, 1500U),
    m_ccPacketInterval(1000U, 0U, 80U),
    m_interval(),
    m_frameLossCnt(0U),
    m_frameLossThreshold(DEFAULT_FRAME_LOSS_THRESHOLD),
    m_ccFrameCnt(0U),
    m_ccSeq(0U),
    m_siteData(),
    m_rssiMapper(rssiMapper),
    m_rssi(0U),
    m_maxRSSI(0U),
    m_minRSSI(0U),
    m_aveRSSI(0U),
    m_rssiCount(0U),
    m_dumpRCCH(dumpRCCHData),
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

    m_voice = new Voice(this, debug, verbose);
    m_control = new ControlSignaling(this, debug, verbose);
    m_data = new Data(this, debug, verbose);

    lc::RCCH::setVerbose(dumpRCCHData);
    lc::RTCH::setVerbose(dumpRCCHData);
}

/// <summary>
/// Finalizes a instance of the Control class.
/// </summary>
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

/// <summary>
/// Resets the data states for the RF interface.
/// </summary>
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

    m_txQueue.clear();

    m_rfMask = 0x00U;
    m_rfLC.reset();

    m_netState = RS_NET_IDLE;

    m_netMask = 0x00U;
    m_netLC.reset();
}

/// <summary>
/// Helper to set NXDN configuration options.
/// </summary>
/// <param name="conf">Instance of the yaml::Node class.</param>
/// <param name="supervisor">Flag indicating whether the DMR has supervisory functions.</param>
/// <param name="cwCallsign">CW callsign of this host.</param>
/// <param name="controlChData">Control Channel data.</param>
/// <param name="siteId">NXDN Site Code.</param>
/// <param name="sysId">NXDN System Code.</param>
/// <param name="channelId">Channel ID.</param>
/// <param name="channelNo">Channel Number.</param>
/// <param name="printOptions"></param>
void Control::setOptions(yaml::Node& conf, bool supervisor, const std::string cwCallsign, lookups::VoiceChData controlChData,
    uint16_t siteId, uint32_t sysId, uint8_t channelId, uint32_t channelNo, bool printOptions)
{
    yaml::Node systemConf = conf["system"];
    yaml::Node nxdnProtocol = conf["protocols"]["nxdn"];

    m_supervisor = supervisor;

    m_control->m_verifyAff = nxdnProtocol["verifyAff"].as<bool>(false);
    m_control->m_verifyReg = nxdnProtocol["verifyReg"].as<bool>(false);

    yaml::Node control = nxdnProtocol["control"];
    m_enableControl = control["enable"].as<bool>(false);
    if (m_enableControl) {
        m_dedicatedControl = control["dedicated"].as<bool>(false);
    }
    else {
        m_dedicatedControl = false;
    }

    m_control->m_disableGrantSrcIdCheck = control["disableGrantSourceIdCheck"].as<bool>(false);

    yaml::Node rfssConfig = systemConf["config"];
    yaml::Node controlCh = rfssConfig["controlCh"];
    m_notifyCC = controlCh["notifyEnable"].as<bool>(false);

    /*
    ** Voice Silence and Frame Loss Thresholds
    */
    m_voice->m_silenceThreshold = nxdnProtocol["silenceThreshold"].as<uint32_t>(nxdn::DEFAULT_SILENCE_THRESHOLD);
    if (m_voice->m_silenceThreshold > MAX_NXDN_VOICE_ERRORS) {
        LogWarning(LOG_NXDN, "Silence threshold > %u, defaulting to %u", nxdn::MAX_NXDN_VOICE_ERRORS, nxdn::DEFAULT_SILENCE_THRESHOLD);
        m_voice->m_silenceThreshold = nxdn::DEFAULT_SILENCE_THRESHOLD;
    }

    // either MAX_NXDN_VOICE_ERRORS or 0 will disable the threshold logic
    if (m_voice->m_silenceThreshold == 0) {
        LogWarning(LOG_NXDN, "Silence threshold set to zero, defaulting to %u", nxdn::MAX_NXDN_VOICE_ERRORS);
        m_voice->m_silenceThreshold = nxdn::MAX_NXDN_VOICE_ERRORS;
    }
    m_frameLossThreshold = (uint8_t)nxdnProtocol["frameLossThreshold"].as<uint32_t>(nxdn::DEFAULT_FRAME_LOSS_THRESHOLD);
    if (m_frameLossThreshold == 0U) {
        m_frameLossThreshold = 1U;
    }

    if (m_frameLossThreshold > nxdn::DEFAULT_FRAME_LOSS_THRESHOLD * 2U) {
        LogWarning(LOG_NXDN, "Frame loss threshold may be excessive, default is %u, configured is %u", nxdn::DEFAULT_FRAME_LOSS_THRESHOLD, m_frameLossThreshold);
    }

    /*
    ** CC Service Class
    */
    uint8_t serviceClass = NXDN_SIF1_VOICE_CALL_SVC | NXDN_SIF1_DATA_CALL_SVC;
    if (m_enableControl) {
        serviceClass |= NXDN_SIF1_GRP_REG_SVC;
    }

    /*
    ** Site Data
    */
    // calculate the NXDN location ID
    uint32_t locId = NXDN_LOC_CAT_GLOBAL; // DVM is currently fixed to "global" category
    locId = (locId << 10) + (sysId & 0x3FFU);
    locId = (locId << 12) + (siteId & 0xFFFU);

    m_siteData = SiteData(locId, channelId, (channelNo & 0x3FF), serviceClass, false);
    m_siteData.setCallsign(cwCallsign);

    m_controlChData = controlChData;

    bool disableUnitRegTimeout = nxdnProtocol["disableUnitRegTimeout"].as<bool>(false);
    m_affiliations.setDisableUnitRegTimeout(disableUnitRegTimeout);

    // set the grant release callback
    m_affiliations.setReleaseGrantCallback([=](uint32_t chNo, uint32_t dstId, uint8_t slot) {
        // callback REST API to clear TG permit for the granted TG on the specified voice channel
        if (m_authoritative && m_supervisor) {
            ::lookups::VoiceChData voiceChData = m_affiliations.rfCh()->getRFChData(chNo);
            if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0 &&
                chNo != m_siteData.channelNo()) {
                json::object req = json::object();
                int state = modem::DVM_STATE::STATE_NXDN;
                req["state"].set<int>(state);
                dstId = 0U; // clear TG value
                req["dstId"].set<uint32_t>(dstId);

                RESTClient::send(voiceChData.address(), voiceChData.port(), voiceChData.password(),
                    HTTP_PUT, PUT_PERMIT_TG, req, voiceChData.ssl(), REST_QUICK_WAIT, m_debug);
            }
            else {
                ::LogError(LOG_NXDN, "NXDN, " NXDN_RTCH_MSG_TYPE_VCALL_RESP ", failed to clear TG permit, chNo = %u", chNo);
            }
        }
    });

    // set the unit deregistration callback
    m_affiliations.setUnitDeregCallback([=](uint32_t srcId) {
        if (m_network != nullptr)
            m_network->announceUnitDeregistration(srcId);
    });

    lc::RCCH::setSiteData(m_siteData);
    lc::RCCH::setCallsign(cwCallsign);

    std::vector<lookups::IdenTable> entries = m_idenTable->list();
    for (auto entry : entries) {
        if (entry.channelId() == channelId) {
            m_idenEntry = entry;
            break;
        }
    }

    if (printOptions) {
        LogInfo("    Silence Threshold: %u (%.1f%%)", m_voice->m_silenceThreshold, float(m_voice->m_silenceThreshold) / 12.33F);
        LogInfo("    Frame Loss Threshold: %u", m_frameLossThreshold);

        if (m_enableControl) {
            if (m_control->m_disableGrantSrcIdCheck) {
                LogInfo("    Disable Grant Source ID Check: yes");
            }
        }

        LogInfo("    Notify Control: %s", m_notifyCC ? "yes" : "no");
        LogInfo("    Verify Affiliation: %s", m_control->m_verifyAff ? "yes" : "no");
        LogInfo("    Verify Registration: %s", m_control->m_verifyReg ? "yes" : "no");

        if (disableUnitRegTimeout) {
            LogInfo("    Disable Unit Registration Timeout: yes");
        }
    }

    if (m_voice != nullptr) {
        m_voice->resetRF();
        m_voice->resetNet();
    }

    if (m_data != nullptr) {
        m_data->resetRF();
    }
}

/// <summary>
/// Process a data frame from the RF interface.
/// </summary>
/// <param name="data">Buffer containing data frame.</param>
/// <param name="len">Length of data frame.</param>
/// <returns></returns>
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

                m_rfMask = 0x00U;
                m_rfLC.reset();

                return false;
            }
        }
    }

    if (m_rfState == RS_RF_AUDIO || m_rfState == RS_RF_DATA) {
        if (m_rfLossWatchdog.isRunning()) {
            m_rfLossWatchdog.start();
        }
    }

    // have we got RSSI bytes on the end?
    if (len == (NXDN_FRAME_LENGTH_BYTES + 4U)) {
        uint16_t raw = 0U;
        raw |= (data[50U] << 8) & 0xFF00U;
        raw |= (data[51U] << 0) & 0x00FFU;

        // Convert the raw RSSI to dBm
        int rssi = m_rssiMapper->interpolate(raw);
        if (m_verbose) {
            LogMessage(LOG_RF, "NXDN, raw RSSI = %u, reported RSSI = %d dBm", raw, rssi);
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

    if (!sync && m_rfState == RS_RF_LISTENING) {
        uint8_t syncBytes[NXDN_FSW_BYTES_LENGTH];
        ::memcpy(syncBytes, data + 2U, NXDN_FSW_BYTES_LENGTH);

        uint8_t errs = 0U;
        for (uint8_t i = 0U; i < NXDN_FSW_BYTES_LENGTH; i++)
            errs += Utils::countBits8(syncBytes[i] ^ NXDN_FSW_BYTES[i]);

        if (errs >= MAX_SYNC_BYTES_ERRS) {
            LogWarning(LOG_RF, "NXDN, possible sync word rejected, errs = %u, sync word = %02X %02X %02X", errs,
                syncBytes[0U], syncBytes[1U], syncBytes[2U]);

            return false;
        }
        else {
            LogWarning(LOG_RF, "NXDN, possible sync word, errs = %u, sync word = %02X %02X %02X", errs,
                syncBytes[0U], syncBytes[1U], syncBytes[2U]);

            sync = true; // we found a completely valid sync with no errors...
        }
    }

    if (sync && m_debug) {
        Utils::symbols("!!! *Rx NXDN", data + 2U, len - 2U);
    }

    NXDNUtils::scrambler(data + 2U);

    channel::LICH lich;
    bool valid = lich.decode(data + 2U);

    if (valid)
        m_rfLastLICH = lich;

    if (!valid && m_rfState == RS_RF_LISTENING) {
        if (m_debug) {
            LogDebug(LOG_RF, "NXDN, invalid LICH, rfct = %u fct = %u", lich.getRFCT(), lich.getFCT());
        }

        return false;
    }

    uint8_t rfct = m_rfLastLICH.getRFCT();
    uint8_t fct = m_rfLastLICH.getFCT();
    uint8_t option = m_rfLastLICH.getOption();

    if (m_debug) {
        LogDebug(LOG_RF, "NXDN, valid LICH, rfState = %u, netState = %u, rfct = %u, fct = %u", m_rfState, m_netState, rfct, fct);
    }

    // are we interrupting a running CC?
    if (m_ccRunning) {
        if ((fct != NXDN_LICH_CAC_INBOUND_SHORT) || (fct != NXDN_LICH_CAC_INBOUND_LONG)) {
            m_ccHalted = true;
        }
    }

    bool ret = false;

    if (rfct == NXDN_LICH_RFCT_RCCH) {
        ret = m_control->process(fct, option, data, len);
    }
    else if (rfct == NXDN_LICH_RFCT_RTCH || rfct == NXDN_LICH_RFCT_RDCH) {
        switch (fct) {
            case NXDN_LICH_USC_UDCH:
                if (!m_dedicatedControl)
                    ret = m_data->process(option, data, len);
                break;
            default:
                if (!m_dedicatedControl)
                    ret = m_voice->process(fct, option, data, len);
                break;
        }
    }

    return ret;
}

/// <summary>
/// Get the frame data length for the next frame in the data ring buffer.
/// </summary>
/// <returns>Length of frame data retrieved.</returns>
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

/// <summary>
/// Get frame data from data ring buffer.
/// </summary>
/// <param name="data">Buffer to store frame data.</param>
/// <returns>Length of frame data retreived.</returns>
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

/// <summary>
/// Updates the processor.
/// </summary>
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

        // do we need to network announce ourselves?
        if (!m_adjSiteUpdate.isRunning()) {
            m_adjSiteUpdate.start();
        }

        m_adjSiteUpdate.clock(ms);
        if (m_adjSiteUpdate.isRunning() && m_adjSiteUpdate.hasExpired()) {
            if (m_rfState == RS_RF_LISTENING && m_netState == RS_NET_IDLE) {
                if (m_network != nullptr) {
                    if (m_affiliations.grpAffSize() > 0) {
                        auto affs = m_affiliations.grpAffTable();
                        m_network->announceAffiliationUpdate(affs);
                    }
                }
                m_adjSiteUpdate.start();
            }
        }

        if (m_ccPrevRunning && !m_ccRunning) {
            m_txQueue.clear();
            m_ccPacketInterval.stop();
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

    if (m_netState == RS_NET_AUDIO) {
        m_networkWatchdog.clock(ms);

        if (m_networkWatchdog.hasExpired()) {
            if (m_netState == RS_NET_AUDIO) {
                ::ActivityLog("NXDN", false, "network watchdog has expired, %.1f seconds, %u%% packet loss",
                    float(m_voice->m_netFrames) / 50.0F, (m_voice->m_netLost * 100U) / m_voice->m_netFrames);
            }
            else {
                ::ActivityLog("NXDN", false, "network watchdog has expired");
            }

            m_networkWatchdog.stop();

            if (m_enableControl) {
                m_affiliations.releaseGrant(m_netLC.getDstId(), false);
            }

            if (m_dedicatedControl) {
                if (m_network != nullptr)
                    m_network->resetNXDN();
            }

            m_netState = RS_NET_IDLE;

            m_netTimeout.stop();

            writeEndNet();
        }
    }

    // reset states if we're in a rejected state
    if (m_rfState == RS_RF_REJECTED) {
        m_txQueue.clear();

        m_voice->resetRF();
        m_voice->resetNet();

        m_data->resetRF();

        if (m_network != nullptr)
            m_network->resetNXDN();

        m_rfState = RS_RF_LISTENING;
    }

    if (m_frameLossCnt > 0U && m_rfState == RS_RF_LISTENING)
        m_frameLossCnt = 0U;
    if (m_frameLossCnt >= m_frameLossThreshold && (m_rfState == RS_RF_AUDIO || m_rfState == RS_RF_DATA)) {
        processFrameLoss();
    }
}

/// <summary>
/// Updates the adj. site tables and affiliations.
/// </summary>
/// <param name="ms"></param>
void Control::clockSiteData(uint32_t ms)
{
    if (m_enableControl) {
        // clock all the grant timers
        m_affiliations.clock(ms);
    }
}

/// <summary>
/// Permits a TGID on a non-authoritative host.
/// </summary>
/// <param name="dstId"></param>
void Control::permittedTG(uint32_t dstId)
{
    if (m_authoritative) {
        return;
    }

    if (m_verbose) {
        LogMessage(LOG_NXDN, "non-authoritative TG permit, dstId = %u", dstId);
    }

    m_permittedDstId = dstId;
}

/// <summary>
/// Grants a TGID on a non-authoritative host.
/// </summary>
/// <param name="srcId"></param>
/// <param name="dstId"></param>
/// <param name="grp"></param>
void Control::grantTG(uint32_t srcId, uint32_t dstId, bool grp)
{
    if (!m_enableControl) {
        return;
    }

    if (m_verbose) {
        LogMessage(LOG_NXDN, "network TG grant demand, srcId = %u, dstId = %u", srcId, dstId);
    }

    m_control->writeRF_Message_Grant(srcId, dstId, 4U, grp);
}

/// <summary>
/// Releases a granted TG.
/// </summary>
/// <param name="dstId"></param>
void Control::releaseGrantTG(uint32_t dstId)
{
    if (!m_enableControl) {
        return;
    }

    if (m_verbose) {
        LogMessage(LOG_NXDN, "VC request, release TG grant, dstId = %u", dstId);
    }

    if (m_affiliations.isGranted(dstId)) {
        uint32_t chNo = m_affiliations.getGrantedCh(dstId);
        uint32_t srcId = m_affiliations.getGrantedSrcId(dstId);
        ::lookups::VoiceChData voiceCh = m_affiliations.rfCh()->getRFChData(chNo);

        if (m_verbose) {
            LogMessage(LOG_NXDN, "VC %s:%u, TG grant released, srcId = %u, dstId = %u, chId = %u, chNo = %u", voiceCh.address().c_str(), voiceCh.port(), srcId, dstId, voiceCh.chId(), chNo);
        }
    
        m_affiliations.releaseGrant(dstId, false);
    }
}

/// <summary>
/// Touches a granted TG to keep a channel grant alive.
/// </summary>
/// <param name="dstId"></param>
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
            LogMessage(LOG_NXDN, "VC %s:%u, call in progress, srcId = %u, dstId = %u, chId = %u, chNo = %u", voiceCh.address().c_str(), voiceCh.port(), srcId, dstId, voiceCh.chId(), chNo);
        }

        m_affiliations.touchGrant(dstId);
    }
}

/// <summary>
/// Flag indicating whether the process or is busy or not.
/// </summary>
/// <returns>True, if processor is busy, otherwise false.</returns>
bool Control::isBusy() const
{
    return m_rfState != RS_RF_LISTENING || m_netState != RS_NET_IDLE;
}

/// <summary>
/// Helper to change the debug and verbose state.
/// </summary>
/// <param name="debug">Flag indicating whether NXDN debug is enabled.</param>
/// <param name="verbose">Flag indicating whether NXDN verbose logging is enabled.</param>
void Control::setDebugVerbose(bool debug, bool verbose)
{
    m_debug = m_voice->m_debug = m_data->m_debug;
    m_verbose = m_voice->m_verbose = m_data->m_verbose;
}

/// <summary>
/// Helper to change the RCCH verbose state.
/// </summary>
/// <param name="verbose">Flag indicating whether RCCH dumping is enabled.</param>
void Control::setRCCHVerbose(bool verbose)
{
    m_dumpRCCH = verbose;
    lc::RCCH::setVerbose(verbose);
    lc::RTCH::setVerbose(verbose);
}

/// <summary>
/// Helper to get the last transmitted destination ID.
/// </summary>
/// <returns></returns>
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

/// <summary>
/// Helper to get the last transmitted source ID.
/// </summary>
/// <returns></returns>
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

/// <summary>
/// Add data frame to the data ring buffer.
/// </summary>
/// <param name="data">Frame data to add to Tx queue.</param>
/// <param name="net">Flag indicating whether the data came from the network or not</param>
/// <param name="imm">Flag indicating whether or not the data is priority and is added to the immediate queue.</param>
void Control::addFrame(const uint8_t *data, bool net, bool imm)
{
	assert(data != nullptr);

    if (!net) {
        if (m_rfTimeout.isRunning() && m_rfTimeout.hasExpired())
            return;
    } else {
        if (m_netTimeout.isRunning() && m_netTimeout.hasExpired())
            return;
    }

    uint8_t len = NXDN_FRAME_LENGTH_BYTES + 2U;
    if (m_debug) {
        Utils::symbols("!!! *Tx NXDN", data + 2U, len - 2U);
    }

    // is this immediate data?
    if (imm) {
        // resize immediate queue if necessary (this shouldn't really ever happen)
        uint32_t space = m_txImmQueue.freeSpace();
        if (space < (len + 1U)) {
            if (!net) {
                uint32_t queueLen = m_txImmQueue.length();
                m_txImmQueue.resize(queueLen + len);
                LogError(LOG_P25, "overflow in the NXDN queue while writing imm data; queue free is %u, needed %u; resized was %u is %u", space, len, queueLen, m_txImmQueue.length());
                return;
            }
            else {
                LogError(LOG_P25, "overflow in the NXDN queue while writing imm network data; queue free is %u, needed %u", space, len);
                return;
            }
        }

        m_txImmQueue.addData(&len, 1U);
        m_txImmQueue.addData(data, len);
        return;
    }

    uint32_t space = m_txQueue.freeSpace();
    if (space < (len + 1U)) {
        if (!net) {
            uint32_t queueLen = m_txQueue.length();
            m_txQueue.resize(queueLen + len);
            LogError(LOG_NXDN, "overflow in the NXDN queue while writing data; queue free is %u, needed %u; resized was %u is %u", space, len, queueLen, m_txQueue.length());
            return;
        }
        else {
            LogError(LOG_NXDN, "overflow in the NXDN queue while writing network data; queue free is %u, needed %u", space, len);
            return;
        }
    }

    m_txQueue.addData(&len, 1U);
    m_txQueue.addData(data, len);
}

/// <summary>
/// Process a data frames from the network.
/// </summary>
void Control::processNetwork()
{
    if (m_rfState != RS_RF_LISTENING && m_netState == RS_NET_IDLE)
        return;

    uint32_t length = 0U;
    bool ret = false;
    UInt8Array buffer = m_network->readNXDN(ret, length);
    if (!ret)
        return;
    if (length == 0U)
        return;
    if (buffer == nullptr) {
        m_network->resetNXDN();
        return;
    }

    // process network message header
    uint8_t messageType = buffer[4U];

    uint32_t srcId = __GET_UINT16(buffer, 5U);
    uint32_t dstId = __GET_UINT16(buffer, 8U);

    if (m_debug) {
        LogDebug(LOG_NET, "NXDN, messageType = $%02X, srcId = %u, dstId = %u, len = %u", messageType, srcId, dstId, length);
    }

    lc::RTCH lc;
    lc.setMessageType(messageType);
    lc.setSrcId((uint16_t)srcId & 0xFFFFU);
    lc.setDstId((uint16_t)dstId & 0xFFFFU);

    bool group = (buffer[15U] & 0x40U) == 0x40U ? false : true;
    lc.setGroup(group);

    // process raw NXDN data bytes
    UInt8Array data;
    uint8_t frameLength = buffer[23U];
    if (frameLength <= 24) {
        data = std::unique_ptr<uint8_t[]>(new uint8_t[frameLength]);
        ::memset(data.get(), 0x00U, frameLength);
    }
    else {
        data = std::unique_ptr<uint8_t[]>(new uint8_t[frameLength]);
        ::memset(data.get(), 0x00U, frameLength);
        ::memcpy(data.get(), buffer.get() + 24U, frameLength);
    }

    m_networkWatchdog.start();

    if (m_debug) {
        Utils::dump(2U, "* !!! NXDN Network Frame", data.get(), frameLength);
    }

    NXDNUtils::scrambler(data.get() + 2U);

    channel::LICH lich;
    bool valid = lich.decode(data.get() + 2U);

    if (valid)
        m_rfLastLICH = lich;

    uint8_t usc = m_rfLastLICH.getFCT();
    uint8_t option = m_rfLastLICH.getOption();

    // forward onto the specific processor for final processing and delivery
    switch (usc) {
        case NXDN_LICH_USC_UDCH:
            ret = m_data->processNetwork(option, lc, data.get(), frameLength);
            break;
        default:
            ret = m_voice->processNetwork(usc, option, lc, data.get(), frameLength);
            break;
    }
}

/// <summary>
/// Helper to process loss of frame stream from modem.
/// </summary>
void Control::processFrameLoss()
{
    if (m_rfState == RS_RF_AUDIO) {
        if (m_rssi != 0U) {
            ::ActivityLog("NXDN", true, "transmission lost, %.1f seconds, BER: %.1f%%, RSSI: -%u/-%u/-%u dBm, loss count: %u",
                float(m_voice->m_rfFrames) / 12.5F, float(m_voice->m_rfErrs * 100U) / float(m_voice->m_rfBits), m_minRSSI, m_maxRSSI, m_aveRSSI / m_rssiCount, m_frameLossCnt);
        }
        else {
            ::ActivityLog("NXDN", true, "transmission lost, %.1f seconds, BER: %.1f%%, loss count: %u",
                float(m_voice->m_rfFrames) / 12.5F, float(m_voice->m_rfErrs * 100U) / float(m_voice->m_rfBits), m_frameLossCnt);
        }

        LogMessage(LOG_RF, "NXDN, " NXDN_RTCH_MSG_TYPE_TX_REL ", total frames: %d, bits: %d, undecodable LC: %d, errors: %d, BER: %.4f%%",
            m_voice->m_rfFrames, m_voice->m_rfBits, m_voice->m_rfUndecodableLC, m_voice->m_rfErrs, float(m_voice->m_rfErrs * 100U) / float(m_voice->m_rfBits));

        m_affiliations.releaseGrant(m_rfLC.getDstId(), false);
        if (m_notifyCC) {
            notifyCC_ReleaseGrant(m_rfLC.getDstId());
        }

        writeEndRF();
    }

    if (m_rfState == RS_RF_DATA) {
        writeEndRF();
    }

    m_rfState = RS_RF_LISTENING;

    m_rfMask = 0x00U;
    m_rfLC.reset();
}

/// <summary>
/// Helper to send a REST API request to the CC to release a channel grant at the end of a call.
/// </summary>
/// <param name="dstId"></param>
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
        LogMessage(LOG_NXDN, "CC %s:%u, notifying CC of call termination, dstId = %u", m_controlChData.address().c_str(), m_controlChData.port(), dstId);
    }

    // callback REST API to release the granted TG on the specified control channel
    json::object req = json::object();
    int state = modem::DVM_STATE::STATE_NXDN;
    req["state"].set<int>(state);
    req["dstId"].set<uint32_t>(dstId);

    int ret = RESTClient::send(m_controlChData.address(), m_controlChData.port(), m_controlChData.password(),
        HTTP_PUT, PUT_RELEASE_TG, req, m_controlChData.ssl(), REST_QUICK_WAIT, m_debug);
    if (ret != network::rest::http::HTTPPayload::StatusType::OK) {
        ::LogError(LOG_NXDN, "failed to notify the CC %s:%u of the release of, dstId = %u", m_controlChData.address().c_str(), m_controlChData.port(), dstId);
    }

    m_rfLastDstId = 0U;
    m_rfLastSrcId = 0U;
    m_netLastDstId = 0U;
    m_netLastSrcId = 0U;
}

/// <summary>
/// Helper to send a REST API request to the CC to "touch" a channel grant to refresh grant timers.
/// </summary>
/// <param name="dstId"></param>
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
    int state = modem::DVM_STATE::STATE_NXDN;
    req["state"].set<int>(state);
    req["dstId"].set<uint32_t>(dstId);

    int ret = RESTClient::send(m_controlChData.address(), m_controlChData.port(), m_controlChData.password(),
        HTTP_PUT, PUT_TOUCH_TG, req, m_controlChData.ssl(), REST_QUICK_WAIT, m_debug);
    if (ret != network::rest::http::HTTPPayload::StatusType::OK) {
        ::LogError(LOG_NXDN, "failed to notify the CC %s:%u of the touch of, dstId = %u", m_controlChData.address().c_str(), m_controlChData.port(), dstId);
    }
}

/// <summary>
/// Helper to write control channel frame data.
/// </summary>
/// <returns></returns>
bool Control::writeRF_ControlData()
{
    if (!m_enableControl)
        return false;

    if (m_ccFrameCnt == 254U) {
        m_ccFrameCnt = 0U;
    }

    // don't add any frames if the queue is full
    uint8_t len = NXDN_FRAME_LENGTH_BYTES + 2U;
    uint32_t space = m_txQueue.freeSpace();
    if (space < (len + 1U)) {
        return false;
    }

    const uint8_t maxSeq = m_control->m_bcchCnt + (m_control->m_ccchPagingCnt + m_control->m_ccchMultiCnt) *
        m_control->m_rcchGroupingCnt * m_control->m_rcchIterateCnt;
    if (m_ccSeq == maxSeq) {
        m_ccSeq = 0U;
    }

    if (m_netState == RS_NET_IDLE && m_rfState == RS_RF_LISTENING) {
        m_control->writeRF_ControlData(m_ccFrameCnt, m_ccSeq, true);

        m_ccSeq++;
        if (m_ccSeq == maxSeq) {
            m_ccFrameCnt++;
        }

        return true;
    }

    return false;
}

/// <summary>
/// Helper to write a Tx release packet.
/// </summary>
/// <param name="noNetwork"></param>
void Control::writeRF_Message_Tx_Rel(bool noNetwork)
{
    uint8_t data[NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, NXDN_FRAME_LENGTH_BYTES);

    Sync::addNXDNSync(data + 2U);

    // generate the LICH
    channel::LICH lich;
    lich.setRFCT(NXDN_LICH_RFCT_RTCH);
    lich.setFCT(NXDN_LICH_USC_SACCH_NS);
    lich.setOption(NXDN_LICH_STEAL_FACCH);
    lich.setOutbound(true);
    lich.encode(data + 2U);

    uint8_t buffer[NXDN_RTCH_LC_LENGTH_BYTES];
    ::memset(buffer, 0x00U, NXDN_RTCH_LC_LENGTH_BYTES);

    m_rfLC.setMessageType(RTCH_MESSAGE_TYPE_TX_REL);
    m_rfLC.encode(buffer, NXDN_UDCH_LENGTH_BITS);

    // generate the SACCH
    channel::SACCH sacch;
    sacch.setData(SACCH_IDLE);
    sacch.setRAN(m_ran);
    sacch.setStructure(NXDN_SR_SINGLE);
    sacch.encode(data + 2U);

    // generate the FACCH1
    channel::FACCH1 facch;
    facch.setData(buffer);
    facch.encode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS);
    facch.encode(data + 2U, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS + NXDN_FACCH1_LENGTH_BITS);

    data[0U] = modem::TAG_DATA;
    data[1U] = 0x00U;

    NXDNUtils::scrambler(data + 2U);

    if (!noNetwork)
        m_data->writeNetwork(data, NXDN_FRAME_LENGTH_BYTES + 2U);

    if (m_duplex) {
        addFrame(data, NXDN_FRAME_LENGTH_BYTES + 2U);
    }
}

/// <summary>
/// Helper to write RF end of frame data.
/// </summary>
void Control::writeEndRF()
{
    m_rfState = RS_RF_LISTENING;

    m_rfMask = 0x00U;
    m_rfLC.reset();

    m_rfTimeout.stop();
    //m_queue.clear();

    if (m_network != nullptr)
        m_network->resetNXDN();
}

/// <summary>
/// Helper to write network end of frame data.
/// </summary>
void Control::writeEndNet()
{
    m_netState = RS_NET_IDLE;

    m_netMask = 0x00U;
    m_netLC.reset();

    m_netTimeout.stop();
    m_networkWatchdog.stop();

    if (m_network != nullptr)
        m_network->resetNXDN();
}
