/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2016,2017,2018 by Jonathan Naylor G4KLX
*   Copyright (C) 2017-2023 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/Control.h"
#include "p25/acl/AccessControl.h"
#include "p25/dfsi/packet/DFSITrunk.h"
#include "p25/dfsi/packet/DFSIVoice.h"
#include "p25/P25Utils.h"
#include "p25/Sync.h"
#include "edac/CRC.h"
#include "remote/RESTClient.h"
#include "HostMain.h"
#include "Log.h"
#include "Utils.h"

using namespace p25;
using namespace p25::packet;

#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint8_t MAX_SYNC_BYTES_ERRS = 4U;
const uint32_t TSBK_PCH_CCH_CNT = 6U;
const uint32_t MAX_PREAMBLE_TDU_CNT = 64U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the Control class.
/// </summary>
/// <param name="authoritative">Flag indicating whether or not the DVM is grant authoritative.</param>
/// <param name="nac">P25 Network Access Code.</param>
/// <param name="callHang">Amount of hangtime for a P25 call.</param>
/// <param name="queueSize">Modem frame buffer queue size (bytes).</param>
/// <param name="modem">Instance of the Modem class.</param>
/// <param name="network">Instance of the BaseNetwork class.</param>
/// <param name="timeout">Transmit timeout.</param>
/// <param name="tgHang">Amount of time to hang on the last talkgroup mode from RF.</param>
/// <param name="duplex">Flag indicating full-duplex operation.</param>
/// <param name="ridLookup">Instance of the RadioIdLookup class.</param>
/// <param name="tidLookup">Instance of the TalkgroupIdLookup class.</param>
/// <param name="idenTable">Instance of the IdenTableLookup class.</param>
/// <param name="rssi">Instance of the RSSIInterpolator class.</param>
/// <param name="dumpPDUData"></param>
/// <param name="repeatPDU"></param>
/// <param name="dumpTSBKData">Flag indicating whether TSBK data is dumped to the log.</param>
/// <param name="debug">Flag indicating whether P25 debug is enabled.</param>
/// <param name="verbose">Flag indicating whether P25 verbose logging is enabled.</param>
Control::Control(bool authoritative, uint32_t nac, uint32_t callHang, uint32_t queueSize, modem::Modem* modem, network::BaseNetwork* network,
    uint32_t timeout, uint32_t tgHang, bool duplex, ::lookups::RadioIdLookup* ridLookup,
    ::lookups::TalkgroupIdLookup* tidLookup, ::lookups::IdenTableLookup* idenTable, ::lookups::RSSIInterpolator* rssiMapper,
    bool dumpPDUData, bool repeatPDU, bool dumpTSBKData, bool debug, bool verbose) :
    m_voice(nullptr),
    m_data(nullptr),
    m_trunk(nullptr),
    m_authoritative(authoritative),
    m_supervisor(false),
    m_nac(nac),
    m_txNAC(nac),
    m_timeout(timeout),
    m_modem(modem),
    m_network(network),
    m_inhibitIllegal(false),
    m_legacyGroupGrnt(true),
    m_legacyGroupReg(false),
    m_duplex(duplex),
    m_control(false),
    m_dedicatedControl(false),
    m_voiceOnControl(false),
    m_ackTSBKRequests(true),
    m_disableNetworkHDU(false),
    m_idenTable(idenTable),
    m_ridLookup(ridLookup),
    m_tidLookup(tidLookup),
    m_affiliations(this, verbose),
    m_idenEntry(),
    m_queue(queueSize, "P25 Frame"),
    m_rfState(RS_RF_LISTENING),
    m_rfLastDstId(0U),
    m_netState(RS_NET_IDLE),
    m_netLastDstId(0U),
    m_permittedDstId(0U),
    m_tailOnIdle(false),
    m_ccRunning(false),
    m_ccPrevRunning(false),
    m_ccHalted(false),
    m_rfTimeout(1000U, timeout),
    m_rfTGHang(1000U, tgHang),
    m_netTimeout(1000U, timeout),
    m_networkWatchdog(1000U, 0U, 1500U),
    m_ccPacketInterval(1000U, 0U, 5U),
    m_hangCount(3U * 8U),
    m_tduPreambleCount(8U),
    m_ccFrameCnt(0U),
    m_ccSeq(0U),
    m_nid(nac),
    m_siteData(),
    m_rssiMapper(rssiMapper),
    m_rssi(0U),
    m_maxRSSI(0U),
    m_minRSSI(0U),
    m_aveRSSI(0U),
    m_rssiCount(0U),
    m_verbose(verbose),
    m_debug(debug)
{
    assert(ridLookup != nullptr);
    assert(tidLookup != nullptr);
    assert(idenTable != nullptr);
    assert(rssiMapper != nullptr);

    acl::AccessControl::init(m_ridLookup, m_tidLookup);

    m_hangCount = callHang * 4U;

#if ENABLE_DFSI_SUPPORT
    if (m_modem->isP25DFSI()) {
        LogMessage(LOG_P25, "DFSI protocol mode is enabled.");
        m_voice = new dfsi::packet::DFSIVoice(this, network, debug, verbose);
        m_trunk = new dfsi::packet::DFSITrunk(this, network, dumpTSBKData, debug, verbose);
    }
    else {
        m_voice = new Voice(this, network, debug, verbose);
        m_trunk = new Trunk(this, network, dumpTSBKData, debug, verbose);
        m_data = new Data(this, network, dumpPDUData, repeatPDU, debug, verbose);
    }
#else
    m_voice = new Voice(this, network, debug, verbose);
    m_trunk = new Trunk(this, network, dumpTSBKData, debug, verbose);
    m_data = new Data(this, network, dumpPDUData, repeatPDU, debug, verbose);
#endif
}

/// <summary>
/// Finalizes a instance of the Control class.
/// </summary>
Control::~Control()
{
    if (m_voice != nullptr) {
        delete m_voice;
    }

    if (m_trunk != nullptr) {
        delete m_trunk;
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

    m_queue.clear();
}

/// <summary>
/// Helper to set P25 configuration options.
/// </summary>
/// <param name="conf">Instance of the yaml::Node class.</param>
/// <param name="supervisor">Flag indicating whether the DMR has supervisory functions.</param>
/// <param name="cwCallsign">CW callsign of this host.</param>
/// <param name="voiceChNo">Voice Channel Number list.</param>
/// <param name="voiceChData">Voice Channel data map.</param>
/// <param name="pSuperGroup"></param>
/// <param name="netId">P25 Network ID.</param>
/// <param name="sysId">P25 System ID.</param>
/// <param name="rfssId">P25 RFSS ID.</param>
/// <param name="siteId">P25 Site ID.</param>
/// <param name="channelId">Channel ID.</param>
/// <param name="channelNo">Channel Number.</param>
/// <param name="printOptions"></param>
void Control::setOptions(yaml::Node& conf, bool supervisor, const std::string cwCallsign, const std::vector<uint32_t> voiceChNo,
    const std::unordered_map<uint32_t, ::lookups::VoiceChData> voiceChData, uint32_t pSuperGroup, uint32_t netId,
    uint32_t sysId, uint8_t rfssId, uint8_t siteId, uint8_t channelId, uint32_t channelNo, bool printOptions)
{
    yaml::Node systemConf = conf["system"];
    yaml::Node p25Protocol = conf["protocols"]["p25"];

    m_supervisor = supervisor;

    m_tduPreambleCount = p25Protocol["tduPreambleCount"].as<uint32_t>(8U);

    m_trunk->m_patchSuperGroup = pSuperGroup;

    m_inhibitIllegal = p25Protocol["inhibitIllegal"].as<bool>(false);
    m_legacyGroupGrnt = p25Protocol["legacyGroupGrnt"].as<bool>(true);
    m_legacyGroupReg = p25Protocol["legacyGroupReg"].as<bool>(false);

    m_trunk->m_verifyAff = p25Protocol["verifyAff"].as<bool>(false);
    m_trunk->m_verifyReg = p25Protocol["verifyReg"].as<bool>(false);

    m_trunk->m_noStatusAck = p25Protocol["noStatusAck"].as<bool>(false);
    m_trunk->m_noMessageAck = p25Protocol["noMessageAck"].as<bool>(true);
    m_trunk->m_unitToUnitAvailCheck = p25Protocol["unitToUnitAvailCheck"].as<bool>(true);

    m_trunk->m_sndcpChGrant = p25Protocol["sndcpGrant"].as<bool>(false);

    yaml::Node control = p25Protocol["control"];
    m_control = control["enable"].as<bool>(false);
    if (m_control) {
        m_dedicatedControl = control["dedicated"].as<bool>(false);
    }
    else {
        m_dedicatedControl = false;
    }

    m_voiceOnControl = p25Protocol["voiceOnControl"].as<bool>(false);
    m_ackTSBKRequests = control["ackRequests"].as<bool>(true);
    m_trunk->m_ctrlTSDUMBF = !control["disableTSDUMBF"].as<bool>(false);
    m_trunk->m_ctrlTimeDateAnn = control["enableTimeDateAnn"].as<bool>(false);

#if ENABLE_DFSI_SUPPORT
    if (m_modem->isP25DFSI()) {
        m_trunk->m_ctrlTSDUMBF = false; // force SBF for TSDUs when using DFSI
    }
#endif

    m_voice->m_silenceThreshold = p25Protocol["silenceThreshold"].as<uint32_t>(p25::DEFAULT_SILENCE_THRESHOLD);
    if (m_voice->m_silenceThreshold > MAX_P25_VOICE_ERRORS) {
        LogWarning(LOG_P25, "Silence threshold > %u, defaulting to %u", p25::MAX_P25_VOICE_ERRORS, p25::DEFAULT_SILENCE_THRESHOLD);
        m_voice->m_silenceThreshold = p25::DEFAULT_SILENCE_THRESHOLD;
    }

    // either MAX_P25_VOICE_ERRORS or 0 will disable the threshold logic
    if (m_voice->m_silenceThreshold == 0) {
        LogWarning(LOG_P25, "Silence threshold set to zero, defaulting to %u", p25::MAX_P25_VOICE_ERRORS);
        m_voice->m_silenceThreshold = p25::MAX_P25_VOICE_ERRORS;
    }

    m_disableNetworkHDU = p25Protocol["disableNetworkHDU"].as<bool>(false);

    bool disableCompositeFlag = p25Protocol["disableCompositeFlag"].as<bool>(false);
    uint8_t serviceClass = P25_SVC_CLS_VOICE | P25_SVC_CLS_DATA;
    if (m_control) {
        serviceClass |= P25_SVC_CLS_REG;
    }

    if (m_voiceOnControl) {
        if (!disableCompositeFlag) {
            serviceClass |= P25_SVC_CLS_COMPOSITE;
        }
    }

    int8_t lto = (int8_t)systemConf["localTimeOffset"].as<int32_t>(0);

    m_siteData = SiteData(netId, sysId, rfssId, siteId, 0U, channelId, channelNo, serviceClass, lto);
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

    m_siteData.setChCnt((uint8_t)voiceChNo.size());
    for (uint32_t ch : voiceChNo) {
        m_affiliations.addRFCh(ch);
    }

    std::unordered_map<uint32_t, ::lookups::VoiceChData> chData = std::unordered_map<uint32_t, ::lookups::VoiceChData>(voiceChData);
    m_affiliations.setRFChData(chData);

    // set the grant release callback
    m_affiliations.setReleaseGrantCallback([=](uint32_t chNo, uint32_t dstId, uint8_t slot) {
        // callback REST API to clear TG permit for the granted TG on the specified voice channel
        if (m_authoritative && m_supervisor) {
            ::lookups::VoiceChData voiceChData = m_affiliations.getRFChData(chNo);
            if (voiceChData.isValidCh() && !voiceChData.address().empty() && voiceChData.port() > 0 &&
                chNo != m_siteData.channelNo()) {
                json::object req = json::object();
                int state = modem::DVM_STATE::STATE_P25;
                req["state"].set<int>(state);
                dstId = 0U; // clear TG value
                req["dstId"].set<uint32_t>(dstId);

                RESTClient::send(voiceChData.address(), voiceChData.port(), voiceChData.password(),
                    HTTP_PUT, PUT_PERMIT_TG, req, m_debug);
            }
            else {
                ::LogError(LOG_P25, P25_TSDU_STR ", TSBK_IOSP_GRP_VCH (Group Voice Channel Grant), failed to clear TG permit, chNo = %u", chNo);
            }
        }
    });

    uint32_t ccBcstInterval = p25Protocol["control"]["interval"].as<uint32_t>(300U);
    m_trunk->m_adjSiteUpdateInterval += ccBcstInterval;

    if (printOptions) {
        LogInfo("    Silence Threshold: %u (%.1f%%)", m_voice->m_silenceThreshold, float(m_voice->m_silenceThreshold) / 12.33F);

        if (m_control) {
            LogInfo("    Voice on Control: %s", m_voiceOnControl ? "yes" : "no");
            LogInfo("    Ack Requests: %s", m_ackTSBKRequests ? "yes" : "no");
        }

        LogInfo("    Disable Network HDUs: %s", m_disableNetworkHDU ? "yes" : "no");
        if (!m_trunk->m_ctrlTSDUMBF) {
            LogInfo("    Disable Multi-Block TSDUs: yes");
        }
        LogInfo("    Time/Date Announcement TSBK: %s", m_trunk->m_ctrlTimeDateAnn ? "yes" : "no");

        LogInfo("    Inhibit Illegal: %s", m_inhibitIllegal ? "yes" : "no");
        LogInfo("    Legacy Group Grant: %s", m_legacyGroupGrnt ? "yes" : "no");
        LogInfo("    Legacy Group Registration: %s", m_legacyGroupReg ? "yes" : "no");
        LogInfo("    Verify Affiliation: %s", m_trunk->m_verifyAff ? "yes" : "no");
        LogInfo("    Verify Registration: %s", m_trunk->m_verifyReg ? "yes" : "no");

        LogInfo("    SNDCP Channel Grant: %s", m_trunk->m_sndcpChGrant ? "yes" : "no");

        LogInfo("    No Status ACK: %s", m_trunk->m_noStatusAck ? "yes" : "no");
        LogInfo("    No Message ACK: %s", m_trunk->m_noMessageAck ? "yes" : "no");
        LogInfo("    Unit-to-Unit Availability Check: %s", m_trunk->m_unitToUnitAvailCheck ? "yes" : "no");
    }

    // are we overriding the NAC for split NAC operations?
    uint32_t txNAC = (uint32_t)::strtoul(systemConf["config"]["txNAC"].as<std::string>("F7E").c_str(), NULL, 16);
    if (txNAC != 0xF7EU && txNAC != m_nac) {
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

/// <summary>
/// Process a data frame from the RF interface.
/// </summary>
/// <param name="data">Buffer containing data frame.</param>
/// <param name="len">Length of data frame.</param>
/// <returns></returns>
bool Control::processFrame(uint8_t* data, uint32_t len)
{
    assert(data != nullptr);

#if ENABLE_DFSI_SUPPORT
    if (m_modem->isP25DFSI()) {
        return processDFSI(data, len);
    }
#endif

    bool sync = data[1U] == 0x01U;

    if (data[0U] == modem::TAG_LOST && m_rfState == RS_RF_AUDIO) {
        if (m_rssi != 0U) {
            ::ActivityLog("P25", true, "transmission lost, %.1f seconds, BER: %.1f%%, RSSI: -%u/-%u/-%u dBm",
                float(m_voice->m_rfFrames) / 5.56F, float(m_voice->m_rfErrs * 100U) / float(m_voice->m_rfBits), m_minRSSI, m_maxRSSI, m_aveRSSI / m_rssiCount);
        }
        else {
            ::ActivityLog("P25", true, "transmission lost, %.1f seconds, BER: %.1f%%",
                float(m_voice->m_rfFrames) / 5.56F, float(m_voice->m_rfErrs * 100U) / float(m_voice->m_rfBits));
        }

        LogMessage(LOG_RF, P25_TDU_STR ", total frames: %d, bits: %d, undecodable LC: %d, errors: %d, BER: %.4f%%",
            m_voice->m_rfFrames, m_voice->m_rfBits, m_voice->m_rfUndecodableLC, m_voice->m_rfErrs, float(m_voice->m_rfErrs * 100U) / float(m_voice->m_rfBits));

        if (m_control) {
            m_affiliations.releaseGrant(m_voice->m_rfLC.getDstId(), false);
        }

        writeRF_TDU(false);
        m_voice->m_lastDUID = P25_DUID_TDU;
        m_voice->writeNetwork(data + 2U, P25_DUID_TDU);

        m_rfState = RS_RF_LISTENING;
        m_rfLastDstId = 0U;
        m_rfTGHang.stop();

        m_tailOnIdle = true;

        m_rfTimeout.stop();
        m_queue.clear();

        if (m_network != nullptr)
            m_network->resetP25();

        return false;
    }

    if (data[0U] == modem::TAG_LOST && m_rfState == RS_RF_DATA) {
        m_rfState = RS_RF_LISTENING;
        m_rfLastDstId = 0U;
        m_rfTGHang.stop();

        m_tailOnIdle = true;

        m_data->resetRF();

        m_rfTimeout.stop();
        m_queue.clear();

        return false;
    }

    if (data[0U] == modem::TAG_LOST) {
        m_rfState = RS_RF_LISTENING;

        m_voice->resetRF();
        m_data->resetRF();

        return false;
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

    uint8_t duid = m_nid.getDUID();

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
        if (duid != P25_DUID_TSDU) {
            m_ccHalted = true;
        }
    }

    bool ret = false;

    // handle individual DUIDs
    switch (duid) {
        case P25_DUID_HDU:
        case P25_DUID_LDU1:
        case P25_DUID_LDU2:
            if (!m_dedicatedControl)
                ret = m_voice->process(data, len);
            else {
                if (m_voiceOnControl && m_affiliations.isChBusy(m_siteData.channelNo())) {
                    ret = m_voice->process(data, len);
                }
            }
            break;

        case P25_DUID_TDU:
        case P25_DUID_TDULC:
            ret = m_voice->process(data, len);
            break;

        case P25_DUID_PDU:
            if (!m_dedicatedControl)
                ret = m_data->process(data, len);
            else {
                if (m_voiceOnControl && m_affiliations.isChBusy(m_siteData.channelNo())) {
                    ret = m_data->process(data, len);
                }
            }
            break;

        case P25_DUID_TSDU:
            ret = m_trunk->process(data, len);
            break;

        default:
            LogError(LOG_RF, "P25 unhandled DUID, duid = $%02X", duid);
            return false;
    }

    return ret;
}

/// <summary>
/// Get the frame data length for the next frame in the data ring buffer.
/// </summary>
/// <returns>Length of frame data retreived.</returns>
uint32_t Control::peekFrameLength()
{
    if (m_queue.isEmpty())
        return 0U;

    uint8_t len = 0U;
    m_queue.peek(&len, 1U);

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

    if (m_queue.isEmpty())
        return 0U;

    uint8_t len = 0U;
    m_queue.getData(&len, 1U);
    m_queue.getData(data, len);

    return len;
}

/// <summary>
/// Helper to write P25 adjacent site information to the network.
/// </summary>
void Control::writeAdjSSNetwork()
{
    m_trunk->writeAdjSSNetwork();
}

/// <summary>
/// Helper to write end of voice call frame data.
/// </summary>
/// <returns></returns>
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

            if (!m_control && m_duplex) {
                writeRF_Nulls();
            }

            return ret;
        }
    }

    return false;
}

/// <summary>
/// Updates the processor by the passed number of milliseconds.
/// </summary>
/// <param name="ms"></param>
void Control::clock(uint32_t ms)
{
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
    if (m_control) {
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

            // reset permitted ID and clear permission state
            if (!m_authoritative && m_permittedDstId != 0U) {
                m_permittedDstId = 0U;
            }
        }
    }

    if (m_netState == RS_NET_AUDIO || m_netState == RS_NET_DATA) {
        m_networkWatchdog.clock(ms);

        if (m_networkWatchdog.hasExpired()) {
            if (m_netState == RS_NET_AUDIO) {
                ::ActivityLog("P25", false, "network watchdog has expired, %.1f seconds, %u%% packet loss",
                    float(m_voice->m_netFrames) / 50.0F, (m_voice->m_netLost * 100U) / m_voice->m_netFrames);
            }
            else {
                ::ActivityLog("P25", false, "network watchdog has expired");
            }

            m_networkWatchdog.stop();

            if (m_control) {
                m_affiliations.releaseGrant(m_voice->m_netLC.getDstId(), false);
            }

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
        m_queue.clear();

        m_voice->resetRF();
        m_voice->resetNet();

        m_data->resetRF();

        if (m_network != nullptr)
            m_network->resetP25();

        m_rfState = RS_RF_LISTENING;
    }

    // clock data and trunking
    if (m_data != nullptr) {
        m_data->clock(ms);
    }

    if (m_trunk != nullptr) {
        m_trunk->clock(ms);
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

    m_permittedDstId = dstId;
}

/// <summary>
/// Flag indicating whether the processor or is busy or not.
/// </summary>
/// <returns>True, if processor is busy, otherwise false.</returns>
bool Control::isBusy() const
{
    return m_rfState != RS_RF_LISTENING || m_netState != RS_NET_IDLE;
}

/// <summary>
/// Helper to change the debug and verbose state.
/// </summary>
/// <param name="debug">Flag indicating whether P25 debug is enabled.</param>
/// <param name="verbose">Flag indicating whether P25 verbose logging is enabled.</param>
void Control::setDebugVerbose(bool debug, bool verbose)
{
    m_debug = m_voice->m_debug = m_data->m_debug = m_trunk->m_debug = debug;
    m_verbose = m_voice->m_verbose = m_data->m_verbose = m_trunk->m_verbose = verbose;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Add data frame to the data ring buffer.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
/// <param name="net"></param>
void Control::addFrame(const uint8_t* data, uint32_t length, bool net)
{
    assert(data != nullptr);

    if (!net) {
        if (m_rfTimeout.isRunning() && m_rfTimeout.hasExpired())
            return;
    } else {
        if (m_netTimeout.isRunning() && m_netTimeout.hasExpired())
            return;
    }

    uint32_t space = m_queue.freeSpace();
    if (space < (length + 1U)) {
        if (!net) {
            uint32_t queueLen = m_queue.length();
            m_queue.resize(queueLen + P25_LDU_FRAME_LENGTH_BYTES);
            LogError(LOG_P25, "overflow in the P25 queue while writing data; queue free is %u, needed %u; resized was %u is %u", space, length, queueLen, m_queue.length());
            return;
        }
        else {
            LogError(LOG_P25, "overflow in the P25 queue while writing network data; queue free is %u, needed %u", space, length);
            return;
        }
    }

    if (m_debug) {
        Utils::symbols("!!! *Tx P25", data + 2U, length - 2U);
    }

    uint8_t len = length;
    m_queue.addData(&len, 1U);
    m_queue.addData(data, len);
}

#if ENABLE_DFSI_SUPPORT
/// <summary>
/// Process a DFSI data frame from the RF interface.
/// </summary>
/// <param name="data">Buffer containing data frame.</param>
/// <param name="len">Length of data frame.</param>
/// <returns></returns>
bool Control::processDFSI(uint8_t* data, uint32_t len)
{
    assert(data != nullptr);

    dfsi::LC dfsiLC = dfsi::LC();

    if (data[0U] == modem::TAG_LOST && m_rfState == RS_RF_AUDIO) {
        if (m_rssi != 0U) {
            ::ActivityLog("P25", true, "transmission lost, %.1f seconds, BER: %.1f%%, RSSI: -%u/-%u/-%u dBm",
                float(m_voice->m_rfFrames) / 5.56F, float(m_voice->m_rfErrs * 100U) / float(m_voice->m_rfBits), m_minRSSI, m_maxRSSI, m_aveRSSI / m_rssiCount);
        }
        else {
            ::ActivityLog("P25", true, "transmission lost, %.1f seconds, BER: %.1f%%",
                float(m_voice->m_rfFrames) / 5.56F, float(m_voice->m_rfErrs * 100U) / float(m_voice->m_rfBits));
        }

        LogMessage(LOG_RF, P25_TDU_STR ", total frames: %d, bits: %d, undecodable LC: %d, errors: %d, BER: %.4f%%",
            m_voice->m_rfFrames, m_voice->m_rfBits, m_voice->m_rfUndecodableLC, m_voice->m_rfErrs, float(m_voice->m_rfErrs * 100U) / float(m_voice->m_rfBits));

        if (m_control) {
            m_trunk->releaseDstIdGrant(m_voice->m_rfLC.getDstId(), false);
        }

        writeRF_TDU(false);
        m_voice->m_lastDUID = P25_DUID_TDU;
        m_voice->writeNetworkRF(data + 2U, P25_DUID_TDU);

        m_rfState = RS_RF_LISTENING;
        m_rfLastDstId = 0U;
        m_rfTGHang.stop();

        m_tailOnIdle = true;

        m_rfTimeout.stop();
        m_queue.clear();

        if (m_network != nullptr)
            m_network->resetP25();

        return false;
    }

    if (data[0U] == modem::TAG_LOST && m_rfState == RS_RF_DATA) {
        m_rfState = RS_RF_LISTENING;
        m_rfLastDstId = 0U;
        m_rfTGHang.stop();

        m_tailOnIdle = true;

        m_data->resetRF();

        m_rfTimeout.stop();
        m_queue.clear();

        return false;
    }

    if (data[0U] == modem::TAG_LOST) {
        m_rfState = RS_RF_LISTENING;

        m_voice->resetRF();
        m_data->resetRF();

        m_trunk->m_rfTSBK = lc::TSBK(m_siteData, m_idenEntry);

        return false;
    }

    // Decode the NID
    bool valid = dfsiLC.decodeNID(data + 2U);

    if (!valid && m_rfState == RS_RF_LISTENING)
        return false;

    uint8_t duid = 0xFFU; // a very invalid DUID
    uint8_t frameType = dfsiLC.getFrameType();

    if (m_debug) {
        LogDebug(LOG_RF, "P25 DFSI, rfState = %u, netState = %u, frameType = %u", m_rfState, m_netState, dfsiLC.getFrameType());
    }

    // are we interrupting a running CC?
    if (m_ccRunning) {
        if (duid != P25_DUID_TSDU) {
            g_interruptP25Control = true;
        }
    }

    // convert DFSI frame-types to DUIDs (this doesn't 100% line up)
    if (frameType == dfsi::P25_DFSI_START_STOP || frameType == dfsi::P25_DFSI_VHDR1 ||
        frameType == dfsi::P25_DFSI_VHDR2) {
        duid = P25_DUID_HDU;
    }
    else if (frameType >= dfsi::P25_DFSI_LDU1_VOICE1 && frameType <= dfsi::P25_DFSI_LDU1_VOICE9) {
        duid = P25_DUID_LDU1;
    }
    else if (frameType >= dfsi::P25_DFSI_LDU2_VOICE10 && frameType <= dfsi::P25_DFSI_LDU2_VOICE18) {
        duid = P25_DUID_LDU2;
    }
    else if (frameType == dfsi::P25_DFSI_TSBK) {
        duid = P25_DUID_TSDU;
    }

    bool ret = false;

    // handle individual DUIDs
    switch (duid) {
    case P25_DUID_HDU:
    case P25_DUID_LDU1:
    case P25_DUID_LDU2:
        if (!m_dedicatedControl)
            ret = m_voice->process(data, len);
        else {
            if (m_voiceOnControl && m_trunk->isChBusy(m_siteData.channelNo())) {
                ret = m_voice->process(data, len);
            }
        }
        break;

    case P25_DUID_TDU:
    case P25_DUID_TDULC:
        ret = m_voice->process(data, len);
        break;

    case P25_DUID_PDU:
        if (!m_dedicatedControl)
            ret = m_data->process(data, len);
        else {
            if (m_voiceOnControl && m_trunk->isChBusy(m_siteData.channelNo())) {
                ret = m_data->process(data, len);
            }
        }
        break;

    case P25_DUID_TSDU:
        ret = m_trunk->process(data, len);
        break;

    default:
        LogError(LOG_RF, "P25 unhandled DUID, duid = $%02X", duid);
        return false;
    }

    return ret;
}
#endif

/// <summary>
/// Process a data frames from the network.
/// </summary>
void Control::processNetwork()
{
    if (m_rfState != RS_RF_LISTENING && m_netState == RS_NET_IDLE)
        return;

    lc::LC control;
    data::LowSpeedData lsd;
    uint8_t duid;
    uint8_t frameType;

    uint32_t length = 100U;
    bool ret = false;
    uint8_t* data = m_network->readP25(ret, control, lsd, duid, frameType, length);
    if (!ret)
        return;
    if (length == 0U)
        return;
    if (data == nullptr) {
        m_network->resetP25();
        return;
    }

    m_networkWatchdog.start();

    if (m_debug) {
        Utils::dump(2U, "!!! *P25 Network Frame", data, length);
    }

    switch (duid) {
        case P25_DUID_HDU:
        case P25_DUID_LDU1:
        case P25_DUID_LDU2:
            ret = m_voice->processNetwork(data, length, control, lsd, duid, frameType);
            break;

        case P25_DUID_TDU:
        case P25_DUID_TDULC:
            m_voice->processNetwork(data, length, control, lsd, duid, frameType);
            break;

        case P25_DUID_PDU:
            if (!m_dedicatedControl)
                ret = m_data->processNetwork(data, length, control, lsd, duid);
            else {
                if (m_voiceOnControl) {
                    ret = m_voice->processNetwork(data, length, control, lsd, duid, frameType);
                }
            }
            break;

        case P25_DUID_TSDU:
            m_trunk->processNetwork(data, length, control, lsd, duid);
            break;
    }

    delete data;
}

/// <summary>
/// Helper to write control channel frame data.
/// </summary>
/// <returns></returns>
bool Control::writeRF_ControlData()
{
    if (!m_control)
        return false;

    if (m_ccFrameCnt == 254U) {
        m_trunk->writeAdjSSNetwork();
        m_ccFrameCnt = 0U;
    }

    // don't add any frames if the queue is full
    uint8_t len = (P25_TSDU_TRIPLE_FRAME_LENGTH_BYTES * 2U) + 2U;
    uint32_t space = m_queue.freeSpace();
    if (space < (len + 1U)) {
        return false;
    }

    const uint8_t maxSeq = 9U;
    if (m_ccSeq == maxSeq) {
        m_ccSeq = 0U;
    }

    if (m_netState == RS_NET_IDLE && m_rfState == RS_RF_LISTENING) {
        m_trunk->writeRF_ControlData(m_ccFrameCnt, m_ccSeq, true);

        m_ccSeq++;
        if (m_ccSeq == maxSeq) {
            m_ccFrameCnt++;
        }

        return true;
    }

    return false;
}

/// <summary>
/// Helper to write end of control channel frame data.
/// </summary>
/// <returns></returns>
bool Control::writeRF_ControlEnd()
{
    if (!m_control)
        return false;

    m_queue.clear();
    m_ccPacketInterval.stop();

    if (m_netState == RS_NET_IDLE && m_rfState == RS_RF_LISTENING) {
        for (uint32_t i = 0; i < TSBK_PCH_CCH_CNT; i++) {
            m_trunk->queueRF_TSBK_Ctrl(TSBK_OSP_MOT_PSH_CCH);
        }

        writeRF_Nulls();
        return true;
    }

    return false;
}

/// <summary>
/// Helper to write data nulls.
/// </summary>
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

/// <summary>
/// Helper to write TDU preamble packet burst.
/// </summary>
/// <param name="preambleCount"></param>
/// <param name="force"></param>
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

/// <summary>
/// Helper to write a P25 TDU packet.
/// </summary>
/// <param name="noNetwork"></param>
void Control::writeRF_TDU(bool noNetwork)
{
#ifdef ENABLE_DFSI_SUPPORT
    // for now abort out of this...
    if (m_modem->isP25DFSI()) {
        return;
    }
#endif

    uint8_t data[P25_TDU_FRAME_LENGTH_BYTES + 2U];
    ::memset(data + 2U, 0x00U, P25_TDU_FRAME_LENGTH_BYTES);

    // Generate Sync
    Sync::addP25Sync(data + 2U);

    // Generate NID
    m_nid.encode(data + 2U, P25_DUID_TDU);

    // Add busy bits
    P25Utils::addBusyBits(data + 2U, P25_TDU_FRAME_LENGTH_BITS, true, true);

    if (!noNetwork)
        m_voice->writeNetwork(data + 2U, P25_DUID_TDU);

    if (m_duplex) {
        data[0U] = modem::TAG_EOT;
        data[1U] = 0x00U;

        addFrame(data, P25_TDU_FRAME_LENGTH_BYTES + 2U);
    }
}
