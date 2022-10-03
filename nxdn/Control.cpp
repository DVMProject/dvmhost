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
*   Copyright (C) 2015-2020 by Jonathan Naylor G4KLX
*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
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
#include "nxdn/NXDNDefines.h"
#include "nxdn/Control.h"
#include "nxdn/acl/AccessControl.h"
#include "nxdn/channel/SACCH.h"
#include "nxdn/channel/FACCH1.h"
#include "nxdn/lc/RTCH.h"
#include "nxdn/Sync.h"
#include "edac/AMBEFEC.h"
#include "HostMain.h"
#include "Log.h"
#include "Utils.h"

using namespace nxdn;
using namespace nxdn::packet;

#include <cstdio>
#include <cassert>
#include <cstring>
#include <ctime>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

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
/// <param name="ran">NXDN Radio Access Number.</param>
/// <param name="callHang">Amount of hangtime for a NXDN call.</param>
/// <param name="queueSize">Modem frame buffer queue size (bytes).</param>
/// <param name="timeout">Transmit timeout.</param>
/// <param name="tgHang">Amount of time to hang on the last talkgroup mode from RF.</param>
/// <param name="modem">Instance of the Modem class.</param>
/// <param name="network">Instance of the BaseNetwork class.</param>
/// <param name="duplex">Flag indicating full-duplex operation.</param>
/// <param name="ridLookup">Instance of the RadioIdLookup class.</param>
/// <param name="tidLookup">Instance of the TalkgroupIdLookup class.</param>
/// <param name="idenTable">Instance of the IdenTableLookup class.</param>
/// <param name="rssi">Instance of the RSSIInterpolator class.</param>
/// <param name="dumpRCCHData">Flag indicating whether RCCH data is dumped to the log.</param>
/// <param name="debug">Flag indicating whether P25 debug is enabled.</param>
/// <param name="verbose">Flag indicating whether P25 verbose logging is enabled.</param>
Control::Control(uint32_t ran, uint32_t callHang, uint32_t queueSize, uint32_t timeout, uint32_t tgHang,
    modem::Modem* modem, network::BaseNetwork* network, bool duplex, lookups::RadioIdLookup* ridLookup,
    lookups::TalkgroupIdLookup* tidLookup, lookups::IdenTableLookup* idenTable, lookups::RSSIInterpolator* rssiMapper,
    bool dumpRCCHData, bool debug, bool verbose) :
    m_voice(NULL),
    m_data(NULL),
    m_ran(ran),
    m_timeout(timeout),
    m_modem(modem),
    m_network(network),
    m_duplex(duplex),
    m_control(false),
    m_dedicatedControl(false),
    m_voiceOnControl(false),
    m_rfLastLICH(),
    m_rfLC(),
    m_netLC(),
    m_rfMask(0U),
    m_netMask(0U),
    m_idenTable(idenTable),
    m_ridLookup(ridLookup),
    m_tidLookup(tidLookup),
    m_affiliations("NXDN Affiliations", verbose),
    m_idenEntry(),
    m_queue(queueSize, "NXDN Frame"),
    m_rfState(RS_RF_LISTENING),
    m_rfLastDstId(0U),
    m_netState(RS_NET_IDLE),
    m_netLastDstId(0U),
    m_ccRunning(false),
    m_ccPrevRunning(false),
    m_ccHalted(false),
    m_rfTimeout(1000U, timeout),
    m_rfTGHang(1000U, tgHang),
    m_netTimeout(1000U, timeout),
    m_networkWatchdog(1000U, 0U, 1500U),
    m_ccPacketInterval(1000U, 0U, 80U),
    m_ccFrameCnt(0U),
    m_ccSeq(0U),
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
    assert(ridLookup != NULL);
    assert(tidLookup != NULL);
    assert(idenTable != NULL);
    assert(rssiMapper != NULL);

    acl::AccessControl::init(m_ridLookup, m_tidLookup);

    m_voice = new Voice(this, network, debug, verbose);
    m_trunk = new Trunk(this, network, debug, verbose, dumpRCCHData);
    m_data = new Data(this, network, debug, verbose);
}

/// <summary>
/// Finalizes a instance of the Control class.
/// </summary>
Control::~Control()
{
    if (m_voice != NULL) {
        delete m_voice;
    }

    if (m_trunk != NULL) {
        delete m_trunk;
    }

    if (m_data != NULL) {
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

    if (m_voice != NULL) {
        m_voice->resetRF();
    }

    if (m_data != NULL) {
        m_data->resetRF();
    }

    m_queue.clear();

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
/// <param name="cwCallsign"></param>
/// <param name="voiceChNo"></param>
/// <param name="locId"></param>
/// <param name="channelId"></param>
/// <param name="channelNo"></param>
/// <param name="printOptions"></param>
void Control::setOptions(yaml::Node& conf, const std::string cwCallsign, const std::vector<uint32_t> voiceChNo,
    uint16_t locId, uint8_t channelId, uint32_t channelNo, bool printOptions)
{
    yaml::Node systemConf = conf["system"];
    yaml::Node nxdnProtocol = conf["protocols"]["nxdn"];

    m_trunk->m_verifyAff = nxdnProtocol["verifyAff"].as<bool>(false);
    m_trunk->m_verifyReg = nxdnProtocol["verifyReg"].as<bool>(false);

    yaml::Node control = nxdnProtocol["control"];
    m_control = control["enable"].as<bool>(false);
    if (m_control) {
        m_dedicatedControl = control["dedicated"].as<bool>(false);
    }
    else {
        m_dedicatedControl = false;
    }

    m_voiceOnControl = nxdnProtocol["voiceOnControl"].as<bool>(false);

    m_voice->m_silenceThreshold = nxdnProtocol["silenceThreshold"].as<uint32_t>(nxdn::DEFAULT_SILENCE_THRESHOLD);
    if (m_voice->m_silenceThreshold > MAX_NXDN_VOICE_ERRORS) {
        LogWarning(LOG_NXDN, "Silence threshold > %u, defaulting to %u", nxdn::MAX_NXDN_VOICE_ERRORS, nxdn::DEFAULT_SILENCE_THRESHOLD);
        m_voice->m_silenceThreshold = nxdn::DEFAULT_SILENCE_THRESHOLD;
    }

    bool disableCompositeFlag = nxdnProtocol["disableCompositeFlag"].as<bool>(false);
    uint8_t serviceClass = NXDN_SIF1_VOICE_CALL_SVC | NXDN_SIF1_DATA_CALL_SVC;
    if (m_control) {
        serviceClass |= NXDN_SIF1_GRP_REG_SVC;
    }

    if (m_voiceOnControl) {
        if (!disableCompositeFlag) {
            serviceClass |= NXDN_SIF1_COMPOSITE_CONTROL;
        }
    }

    m_siteData = SiteData(locId, channelId, channelNo, serviceClass, false);
    m_siteData.setCallsign(cwCallsign);

    std::vector<lookups::IdenTable> entries = m_idenTable->list();
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        lookups::IdenTable entry = *it;
        if (entry.channelId() == channelId) {
            m_idenEntry = entry;
            break;
        }
    }

    std::vector<uint32_t> availCh = voiceChNo;
    for (auto it = availCh.begin(); it != availCh.end(); ++it) {
        m_affiliations.addRFCh(*it);
    }

    if (printOptions) {
        LogInfo("    Silence Threshold: %u (%.1f%%)", m_voice->m_silenceThreshold, float(m_voice->m_silenceThreshold) / 12.33F);

        if (m_control) {
            LogInfo("    Voice on Control: %s", m_voiceOnControl ? "yes" : "no");
        }

        LogInfo("    Verify Affiliation: %s", m_trunk->m_verifyAff ? "yes" : "no");
        LogInfo("    Verify Registration: %s", m_trunk->m_verifyReg ? "yes" : "no");
    }

    if (m_voice != NULL) {
        m_voice->resetRF();
        m_voice->resetNet();
    }

    if (m_data != NULL) {
        m_data->resetRF();
    }

    if (m_trunk != NULL) {
        m_trunk->resetRF();
        m_trunk->resetNet();
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
    assert(data != NULL);

    uint8_t type = data[0U];

    if (type == modem::TAG_LOST && m_rfState == RS_RF_AUDIO) {
        if (m_rssi != 0U) {
            ::ActivityLog("NXDN", true, "transmission lost, %.1f seconds, BER: %.1f%%, RSSI: -%u/-%u/-%u dBm", 
                float(m_voice->m_rfFrames) / 12.5F, float(m_voice->m_rfErrs * 100U) / float(m_voice->m_rfBits), m_minRSSI, m_maxRSSI, m_aveRSSI / m_rssiCount);
        }
        else {
            ::ActivityLog("NXDN", true, "transmission lost, %.1f seconds, BER: %.1f%%", 
                float(m_voice->m_rfFrames) / 12.5F, float(m_voice->m_rfErrs * 100U) / float(m_voice->m_rfBits));
        }

        LogMessage(LOG_RF, "NXDN, " NXDN_RTCH_MSG_TYPE_TX_REL ", total frames: %d, bits: %d, undecodable LC: %d, errors: %d, BER: %.4f%%", 
            m_voice->m_rfFrames, m_voice->m_rfBits, m_voice->m_rfUndecodableLC, m_voice->m_rfErrs, float(m_voice->m_rfErrs * 100U) / float(m_voice->m_rfBits));

        if (m_control) {
            m_affiliations.releaseGrant(m_rfLC.getDstId(), false);
        }

        writeEndRF();
        return false;
    }

	if (type == modem::TAG_LOST && m_rfState == RS_RF_DATA) {
		writeEndRF();
		return false;
	}

	if (type == modem::TAG_LOST) {
		m_rfState = RS_RF_LISTENING;
		m_rfMask  = 0x00U;
		m_rfLC.reset();
		return false;
	}

	// Have we got RSSI bytes on the end?
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

    scrambler(data + 2U);

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
        ret = m_trunk->process(fct, option, data, len);
    }
    else if (rfct == NXDN_LICH_RFCT_RTCH || rfct == NXDN_LICH_RFCT_RDCH) {
        switch (fct) {
            case NXDN_LICH_USC_UDCH:
                if (!m_dedicatedControl) {
                    ret = m_data->process(option, data, len);
                }
                else {
                    if (m_voiceOnControl && m_affiliations.isChBusy(m_siteData.channelNo())) {
                        ret = m_data->process(option, data, len);
                    }
                }
                break;
            default:
                if (!m_dedicatedControl) {
                    ret = m_voice->process(fct, option, data, len);
                }
                else {
                    if (m_voiceOnControl && m_affiliations.isChBusy(m_siteData.channelNo())) {
                        ret = m_voice->process(fct, option, data, len);
                    }
                }
                break;
        }
    }

    return ret;
}

/// <summary>
/// Get frame data from data ring buffer.
/// </summary>
/// <param name="data">Buffer to store frame data.</param>
/// <returns>Length of frame data retreived.</returns>
uint32_t Control::getFrame(uint8_t* data)
{
	assert(data != NULL);

	if (m_queue.isEmpty())
		return 0U;

	uint8_t len = 0U;
	m_queue.getData(&len, 1U);
	m_queue.getData(data, len);

	return len;
}

/// <summary>
/// Updates the processor by the passed number of milliseconds.
/// </summary>
/// <param name="ms"></param>
void Control::clock(uint32_t ms)
{
    if (m_network != NULL) {
        processNetwork();

        if (m_network->getStatus() == network::NET_STAT_RUNNING) {
            m_siteData.setNetActive(true);
        }
        else {
            m_siteData.setNetActive(false);
        }
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
            m_queue.clear();
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
        }
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

            if (m_control) {
                m_affiliations.releaseGrant(m_netLC.getDstId(), false);
            }

            if (m_dedicatedControl) {
                if (m_network != NULL)
                    m_network->resetNXDN();
            }

            m_netState = RS_NET_IDLE;

            m_netTimeout.stop();

            writeEndNet();
        }
    }

    // reset states if we're in a rejected state
    if (m_rfState == RS_RF_REJECTED) {
        m_queue.clear();

        m_voice->resetRF();
        m_voice->resetNet();

        m_data->resetRF();

        if (m_network != NULL)
            m_network->resetNXDN();

        m_rfState = RS_RF_LISTENING;
    }

    // clock data and trunking
    if (m_trunk != NULL) {
        m_trunk->clock(ms);
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

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Add data frame to the data ring buffer.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
/// <param name="net"></param>
void Control::addFrame(const uint8_t *data, uint32_t length, bool net)
{
	assert(data != NULL);

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
            m_queue.resize(queueLen + NXDN_FRAME_LENGTH_BYTES);
            LogError(LOG_NXDN, "overflow in the NXDN queue while writing data; queue free is %u, needed %u; resized was %u is %u", space, length, queueLen, m_queue.length());
            return;
        }
        else {
            LogError(LOG_NXDN, "overflow in the NXDN queue while writing network data; queue free is %u, needed %u", space, length);
            return;
        }
    }

    if (m_debug) {
        Utils::symbols("!!! *Tx NXDN", data + 2U, length - 2U);
    }

    uint8_t len = length;
    m_queue.addData(&len, 1U);
    m_queue.addData(data, len);
}

/// <summary>
/// Process a data frames from the network.
/// </summary>
void Control::processNetwork()
{
    if (m_rfState != RS_RF_LISTENING && m_netState == RS_NET_IDLE)
        return;

    lc::RTCH lc;

    uint32_t length = 100U;
    bool ret = false;
    uint8_t* data = m_network->readNXDN(ret, lc, length);
    if (!ret)
        return;
    if (length == 0U)
        return;
    if (data == NULL) {
        m_network->resetNXDN();
        return;
    }

    m_networkWatchdog.start();

    if (m_debug) {
        Utils::dump(2U, "!!! *NXDN Network Frame", data, length);
    }

    scrambler(data + 2U);

    channel::LICH lich;
    bool valid = lich.decode(data + 2U);

    if (valid)
        m_rfLastLICH = lich;

    uint8_t usc = m_rfLastLICH.getFCT();
    uint8_t option = m_rfLastLICH.getOption();

    switch (usc) {
        case NXDN_LICH_USC_UDCH:
            ret = m_data->processNetwork(option, lc, data, length);
            break;
        default:
            ret = m_voice->processNetwork(usc, option, lc, data, length);
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
        m_ccFrameCnt = 0U;
    }

    // don't add any frames if the queue is full
    uint8_t len = NXDN_FRAME_LENGTH_BYTES + 2U;
    uint32_t space = m_queue.freeSpace();
    if (space < (len + 1U)) {
        return false;
    }

    const uint8_t maxSeq = m_trunk->m_rfLC.getBcchCnt() + (m_trunk->m_rfLC.getCcchPagingCnt() + m_trunk->m_rfLC.getCcchMultiCnt()) *
        m_trunk->m_rfLC.getRcchGroupingCnt() * m_trunk->m_rfLC.getRcchIterateCount();
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

    scrambler(data + 2U);

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

    if (m_network != NULL)
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

    if (m_network != NULL)
        m_network->resetP25();
}

/// <summary>
///
/// </summary>
/// <param name="data"></param>
void Control::scrambler(uint8_t* data) const
{
    assert(data != NULL);

    if (m_debug) {
        Utils::symbols("!!! *Tx NXDN (Unscrambled)", data, NXDN_FRAME_LENGTH_BYTES);
    }

    for (uint32_t i = 0U; i < NXDN_FRAME_LENGTH_BYTES; i++)
        data[i] ^= SCRAMBLER[i];
}
