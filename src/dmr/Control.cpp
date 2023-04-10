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
*   Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
*   Copyright (C) 2017-2023 by Bryan Biedenkapp <gatekeep@jmp.cx>
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; version 2 of the License.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*/
#include "Defines.h"
#include "dmr/Control.h"
#include "dmr/acl/AccessControl.h"
#include "dmr/lc/CSBK.h"
#include "dmr/lc/csbk/CSBKFactory.h"
#include "Log.h"

using namespace dmr;

#include <cstdio>
#include <cassert>
#include <algorithm>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the Control class.
/// </summary>
/// <param name="authoritative">Flag indicating whether or not the DVM is grant authoritative.</param>
/// <param name="colorCode">DMR access color code.</param>
/// <param name="callHang">Amount of hangtime for a DMR call.</param>
/// <param name="queueSize">Size of DMR data frame ring buffer.</param>
/// <param name="embeddedLCOnly"></param>
/// <param name="dumpTAData"></param>
/// <param name="timeout">Transmit timeout.</param>
/// <param name="tgHang">Amount of time to hang on the last talkgroup mode from RF.</param>
/// <param name="modem">Instance of the Modem class.</param>
/// <param name="network">Instance of the BaseNetwork class.</param>
/// <param name="duplex">Flag indicating full-duplex operation.</param>
/// <param name="ridLookup">Instance of the RadioIdLookup class.</param>
/// <param name="tidLookup">Instance of the TalkgroupIdLookup class.</param>
/// <param name="idenTable">Instance of the IdenTableLookup class.</param>
/// <param name="rssiMapper">Instance of the RSSIInterpolator class.</param>
/// <param name="jitter"></param>
/// <param name="dumpDataPacket"></param>
/// <param name="repeatDataPacket"></param>
/// <param name="dumpCSBKData"></param>
/// <param name="debug">Flag indicating whether DMR debug is enabled.</param>
/// <param name="verbose">Flag indicating whether DMR verbose logging is enabled.</param>
Control::Control(bool authoritative, uint32_t colorCode, uint32_t callHang, uint32_t queueSize, bool embeddedLCOnly,
    bool dumpTAData, uint32_t timeout, uint32_t tgHang, modem::Modem* modem, network::BaseNetwork* network, bool duplex,
    ::lookups::RadioIdLookup* ridLookup, ::lookups::TalkgroupIdLookup* tidLookup, ::lookups::IdenTableLookup* idenTable, ::lookups::RSSIInterpolator* rssiMapper,
    uint32_t jitter, bool dumpDataPacket, bool repeatDataPacket, bool dumpCSBKData, bool debug, bool verbose) :
    m_authoritative(authoritative),
    m_supervisor(false),
    m_colorCode(colorCode),
    m_modem(modem),
    m_network(network),
    m_slot1(nullptr),
    m_slot2(nullptr),
    m_idenTable(idenTable),
    m_ridLookup(ridLookup),
    m_tidLookup(tidLookup),
    m_enableTSCC(false),
    m_tsccCnt(0U),
    m_tsccCntInterval(1000U, 0U, DMR_SLOT_TIME / 2U),
    m_tsccSlotNo(0U),
    m_tsccPayloadActive(false),
    m_ccRunning(false),
    m_ccHalted(false),
    m_dumpCSBKData(dumpCSBKData),
    m_verbose(verbose),
    m_debug(debug)
{
    assert(modem != nullptr);
    assert(ridLookup != nullptr);
    assert(tidLookup != nullptr);
    assert(idenTable != nullptr);
    assert(rssiMapper != nullptr);

    acl::AccessControl::init(m_ridLookup, m_tidLookup);
    Slot::init(this, authoritative, colorCode, SiteData(), embeddedLCOnly, dumpTAData, callHang, modem, network, duplex, m_ridLookup, m_tidLookup, m_idenTable, rssiMapper, jitter, verbose);

    m_slot1 = new Slot(1U, timeout, tgHang, queueSize, dumpDataPacket, repeatDataPacket, dumpCSBKData, debug, verbose);
    m_slot2 = new Slot(2U, timeout, tgHang, queueSize, dumpDataPacket, repeatDataPacket, dumpCSBKData, debug, verbose);

    m_tsccCntInterval.start();
}

/// <summary>
/// Finalizes a instance of the Control class.
/// </summary>
Control::~Control()
{
    delete m_slot2;
    delete m_slot1;
}

/// <summary>
/// Helper to set DMR configuration options.
/// </summary>
/// <param name="conf">Instance of the ConfigINI class.</param>
/// <param name="supervisor">Flag indicating whether the DMR has supervisory functions.</param>
/// <param name="voiceChNo">Voice Channel Number list.</param>
/// <param name="voiceChData">Voice Channel data map.</param>
/// <param name="netId">DMR Network ID.</param>
/// <param name="siteId">DMR Site ID.</param>
/// <param name="channelId">Channel ID.</param>
/// <param name="channelNo">Channel Number.</param>
/// <param name="printOptions"></param>
void Control::setOptions(yaml::Node& conf, bool supervisor, const std::vector<uint32_t> voiceChNo, const std::unordered_map<uint32_t, ::lookups::VoiceChData> voiceChData,
    uint32_t netId, uint8_t siteId, uint8_t channelId, uint32_t channelNo, bool printOptions)
{
    yaml::Node systemConf = conf["system"];
    yaml::Node dmrProtocol = conf["protocols"]["dmr"];

    m_supervisor = supervisor;

    Slot::m_verifyReg = dmrProtocol["verifyReg"].as<bool>(false);

    uint8_t nRandWait = (uint8_t)dmrProtocol["nRandWait"].as<uint32_t>(dmr::DEFAULT_NRAND_WAIT);
    if (nRandWait > 15U)
        nRandWait = 15U;
    uint8_t backOff = (uint8_t)dmrProtocol["backOff"].as<uint32_t>(1U);
    if (backOff > 15U)
        backOff = 15U;

    yaml::Node control = dmrProtocol["control"];
    bool enableTSCC = control["enable"].as<bool>(false);
    bool dedicatedTSCC = false;
    if (enableTSCC) {
        dedicatedTSCC = control["dedicated"].as<bool>(false);
    }
    else {
        dedicatedTSCC = false;
    }

    Slot::setSiteData(voiceChNo, voiceChData, netId, siteId, channelId, channelNo, dedicatedTSCC);
    Slot::setAlohaConfig(nRandWait, backOff);

    if (enableTSCC) {
        m_tsccSlotNo = (uint8_t)control["slot"].as<uint32_t>(0U);
        switch (m_tsccSlotNo) {
        case 1U:
            m_slot1->setTSCC(enableTSCC, dedicatedTSCC);
            m_slot1->setSupervisor(m_supervisor);
            break;
        case 2U:
            m_slot2->setTSCC(enableTSCC, dedicatedTSCC);
            m_slot2->setSupervisor(m_supervisor);
            break;
        default:
            LogError(LOG_DMR, "DMR, invalid slot, TSCC disabled, slotNo = %u", m_tsccSlotNo);
            break;
        }
    }

    m_enableTSCC = enableTSCC;

    uint32_t silenceThreshold = dmrProtocol["silenceThreshold"].as<uint32_t>(dmr::DEFAULT_SILENCE_THRESHOLD);
    if (silenceThreshold > MAX_DMR_VOICE_ERRORS) {
        LogWarning(LOG_DMR, "Silence threshold > %u, defaulting to %u", dmr::MAX_DMR_VOICE_ERRORS, dmr::DEFAULT_SILENCE_THRESHOLD);
        silenceThreshold = dmr::DEFAULT_SILENCE_THRESHOLD;
    }

    // either MAX_DMR_VOICE_ERRORS or 0 will disable the threshold logic
    if (silenceThreshold == 0) {
        LogWarning(LOG_DMR, "Silence threshold set to zero, defaulting to %u", dmr::MAX_DMR_VOICE_ERRORS);
        silenceThreshold = dmr::MAX_DMR_VOICE_ERRORS;
    }

    m_slot1->setSilenceThreshold(silenceThreshold);
    m_slot2->setSilenceThreshold(silenceThreshold);

    if (printOptions) {
        if (enableTSCC) {
            LogInfo("    TSCC Slot: %u", m_tsccSlotNo);
            LogInfo("    TSCC Aloha Random Access Wait: %u", nRandWait);
            LogInfo("    TSCC Aloha Backoff: %u", backOff);
        }

        LogInfo("    Silence Threshold: %u (%.1f%%)", silenceThreshold, float(silenceThreshold) / 1.41F);

        LogInfo("    Verify Registration: %s", Slot::m_verifyReg ? "yes" : "no");
    }
}

/// <summary>
/// Sets a flag indicating whether the DMR control channel is running.
/// </summary>
/// <param name="ccRunning"></param>
void Control::setCCRunning(bool ccRunning)
{
    if (!m_enableTSCC) {
        m_ccRunning = false;
        return;
    }

    m_ccRunning = ccRunning;
    switch (m_tsccSlotNo) {
    case 1U:
        m_slot1->setCCRunning(ccRunning);
        break;
    case 2U:
        m_slot2->setCCRunning(ccRunning);
        break;
    default:
        LogError(LOG_DMR, "DMR, invalid slot, TSCC disabled, slotNo = %u", m_tsccSlotNo);
        break;
    }
}

/// <summary>
/// Sets a flag indicating whether the DMR control channel is halted.
/// </summary>
/// <param name="ccHalted"></param>
void Control::setCCHalted(bool ccHalted)
{
    if (!m_enableTSCC) {
        m_ccHalted = true;
        return;
    }

    m_ccHalted = ccHalted;
    switch (m_tsccSlotNo) {
    case 1U:
        m_slot1->setCCHalted(ccHalted);
        break;
    case 2U:
        m_slot2->setCCHalted(ccHalted);
        break;
    default:
        LogError(LOG_DMR, "DMR, invalid slot, TSCC disabled, slotNo = %u", m_tsccSlotNo);
        break;
    }
}

/// <summary>
/// Helper to process wakeup frames from the RF interface.
/// </summary>
/// <param name="data">DMR wakeup frame data.</param>
/// <returns>True, if wakeup frames were processed, otherwise false.</returns>
bool Control::processWakeup(const uint8_t* data)
{
    assert(data != nullptr);

    // wakeups always come in on slot 1
    if (data[0U] != modem::TAG_DATA || data[1U] != (DMR_IDLE_RX | DMR_SYNC_DATA | DT_CSBK))
        return false;

    // generate a new CSBK and check validity
    std::unique_ptr<lc::CSBK> csbk = lc::csbk::CSBKFactory::createCSBK(data + 2U);
    if (csbk == nullptr)
        return false;

    uint8_t csbko = csbk->getCSBKO();
    if (csbko != CSBKO_BSDWNACT)
        return false;

    uint32_t srcId = csbk->getSrcId();

    // check the srcId against the ACL control
    bool ret = acl::AccessControl::validateSrcId(srcId);
    if (!ret) {
        LogError(LOG_RF, "DMR, invalid CSBKO_BSDWNACT, srcId = %u", srcId);
        return false;
    }

    if (m_verbose) {
        LogMessage(LOG_RF, "DMR, CSBKO_BSDWNACT, srcId = %u", srcId);
    }

    return true;
}

/// <summary>
/// Process a data frame for slot, from the RF interface.
/// </summary>
/// <param name="data">DMR data frame buffer.</param>
/// <param name="len">Length of data frame buffer.</param>
/// <returns>True, if data frame was processed, otherwise false.</returns>
bool Control::processFrame(uint32_t slotNo, uint8_t *data, uint32_t len)
{
    assert(data != nullptr);

    switch (slotNo) {
    case 1U:
        return m_slot1->processFrame(data, len);
    case 2U:
        return m_slot2->processFrame(data, len);
    default:
        LogError(LOG_DMR, "DMR, invalid slot, slotNo = %u", slotNo);
        return false;
    }
}

/// <summary>
/// Get a data frame for slot, from data ring buffer.
/// </summary>
/// <param name="data">Buffer to put retrieved DMR data frame data.</param>
/// <returns>Length of data retrieved from DMR ring buffer.</returns>
uint32_t Control::getFrame(uint32_t slotNo, uint8_t* data)
{
    assert(data != nullptr);

    switch (slotNo) {
    case 1U:
        return m_slot1->getFrame(data);
    case 2U:
        return m_slot2->getFrame(data);
    default:
        LogError(LOG_DMR, "DMR, invalid slot, slotNo = %u", slotNo);
        return 0U;
    }
}

/// <summary>
/// Updates the processor by the passed number of milliseconds.
/// </summary>
/// <param name="ms"></param>
void Control::clock(uint32_t ms)
{
    if (m_network != nullptr) {
        data::Data data;
        bool ret = m_network->readDMR(data);
        if (ret) {
            uint32_t slotNo = data.getSlotNo();
            switch (slotNo) {
                case 1U:
                    m_slot1->processNetwork(data);
                    break;
                case 2U:
                    m_slot2->processNetwork(data);
                    break;
                default:
                    LogError(LOG_DMR, "DMR, invalid slot, slotNo = %u", slotNo);
                    break;
            }
        }
    }

    m_tsccCntInterval.clock(ms);
    if (m_tsccCntInterval.isRunning() && m_tsccCntInterval.hasExpired()) {
        m_tsccCnt++;
        if (m_tsccCnt == TSCC_MAX_CSC_CNT) {
            m_tsccCnt = 0U;
        }

        m_tsccCntInterval.start();
    }

    m_slot1->clock();
    m_slot2->clock();
}

/// <summary>
/// Sets a flag indicating whether DMR has supervisory functions and can send permit TG to voice channels.
/// </summary>
/// <param name="supervisor"></param>
void Control::setSupervisor(bool supervisor)
{
    if (!m_enableTSCC) {
        return;
    }

    switch (m_tsccSlotNo) {
    case 1U:
        m_slot1->setSupervisor(supervisor);
        break;
    case 2U:
        m_slot2->setSupervisor(supervisor);
        break;
    default:
        LogError(LOG_DMR, "DMR, invalid slot, slotNo = %u", m_tsccSlotNo);
        break;
    }
}

/// <summary>
/// Permits a TGID on a non-authoritative host.
/// </summary>
/// <param name="dstId"></param>
/// <paran name="slot"></param>
void Control::permittedTG(uint32_t dstId, uint8_t slot)
{
    switch (slot) {
    case 1U:
        m_slot1->permittedTG(dstId);
        break;
    case 2U:
        m_slot2->permittedTG(dstId);
        break;
    default:
        LogError(LOG_DMR, "DMR, invalid slot, slotNo = %u", slot);
        break;
    }
}

/// <summary>
/// Gets instance of the AffiliationLookup class.
/// </summary>
dmr::lookups::DMRAffiliationLookup Control::affiliations()
{
    switch (m_tsccSlotNo) {
    case 1U:
        return m_slot1->m_affiliations;
    case 2U:
        return m_slot2->m_affiliations;
        break;
    default:
        LogError(LOG_DMR, "DMR, invalid slot, slotNo = %u", m_tsccSlotNo);
        break;
    }

    return 0; // ??
}

/// <summary>
/// Helper to return the slot carrying the TSCC.
/// </summary>
/// <returns>Pointer to the TSCC slot instance.</returns>
Slot* Control::getTSCCSlot() const
{
    switch (m_tsccSlotNo) {
    case 1U:
        return m_slot1;
        break;
    case 2U:
        return m_slot2;
        break;
    default:
        LogError(LOG_DMR, "DMR, invalid slot, TSCC disabled, slotNo = %u", m_tsccSlotNo);
        return nullptr;
    }
}

/// <summary>
/// Helper to payload activate the slot carrying granted payload traffic.
/// </summary>
/// <param name="slotNo">DMR slot number.</param>
/// <param name="dstId"></param>
/// <param name="srcId"></param>
/// <param name="group"></param>
/// <param name="voice"></param>
void Control::tsccActivateSlot(uint32_t slotNo, uint32_t dstId, uint32_t srcId, bool group, bool voice)
{
    if (m_verbose) {
        LogMessage(LOG_DMR, "DMR Slot %u, payload activation, srcId = %u, group = %u, dstId = %u",
            slotNo, srcId, group, dstId);
    }

    // never allow the TSCC to become payload activated
    if (m_tsccSlotNo == slotNo) {
        LogError(LOG_DMR, "DMR, cowardly refusing to, TSCC payload activation, slotNo = %u", slotNo);
        return;
    }

    switch (slotNo) {
    case 1U:
        m_tsccPayloadActive = true;
        m_slot1->setTSCCActivated(dstId, srcId, group, voice);
        break;
    case 2U:
        m_tsccPayloadActive = true;
        m_slot2->setTSCCActivated(dstId, srcId, group, voice);
        break;
    default:
        LogError(LOG_DMR, "DMR, invalid slot, TSCC payload activation, slotNo = %u", slotNo);
        break;
    }
}

/// <summary>
/// Helper to clear an activated payload slot.
/// </summary>
/// <param name="slotNo">DMR slot number.</param>
void Control::tsccClearActivatedSlot(uint32_t slotNo)
{
    if (m_verbose) {
        LogMessage(LOG_DMR, "DMR Slot %u, payload activation clear", slotNo);
    }

    switch (slotNo) {
    case 1U:
        m_slot1->clearTSCCActivated();
        break;
    case 2U:
        m_slot2->clearTSCCActivated();
        break;
    default:
        LogError(LOG_DMR, "DMR, invalid slot, TSCC payload activation, slotNo = %u", slotNo);
        break;
    }

    if (m_tsccPayloadActive && m_slot1->m_tsccPayloadDstId == 0U && m_slot2->m_tsccPayloadDstId == 0U) {
        m_tsccPayloadActive = false;
    }
}

/// <summary>
/// Helper to write a DMR extended function packet on the RF interface.
/// </summary>
/// <param name="slotNo">DMR slot number.</param>
/// <param name="func">Extended function opcode.</param>
/// <param name="arg">Extended function argument.</param>
/// <param name="dstId">Destination radio ID.</param>
void Control::writeRF_Ext_Func(uint32_t slotNo, uint32_t func, uint32_t arg, uint32_t dstId)
{
    switch (slotNo) {
    case 1U:
        m_slot1->control()->writeRF_Ext_Func(func, arg, dstId);
        break;
    case 2U:
        m_slot2->control()->writeRF_Ext_Func(func, arg, dstId);
        break;
    default:
        LogError(LOG_DMR, "DMR, invalid slot, slotNo = %u", slotNo);
        break;
    }
}

/// <summary>
/// Helper to write a DMR call alert packet on the RF interface.
/// </summary>
/// <param name="slotNo">DMR slot number.</param>
/// <param name="srcId">Source radio ID.</param>
/// <param name="dstId">Destination radio ID.</param>
void Control::writeRF_Call_Alrt(uint32_t slotNo, uint32_t srcId, uint32_t dstId)
{
    switch (slotNo) {
    case 1U:
        m_slot1->control()->writeRF_Call_Alrt(srcId, dstId);
        break;
    case 2U:
        m_slot2->control()->writeRF_Call_Alrt(srcId, dstId);
        break;
    default:
        LogError(LOG_DMR, "DMR, invalid slot, slotNo = %u", slotNo);
        break;
    }
}

/// <summary>
/// Flag indicating whether the processor or is busy or not.
/// </summary>
/// <returns>True, if processor is busy, otherwise false.</returns>
bool Control::isBusy() const
{
    return (m_slot1->m_rfState != RS_RF_LISTENING || m_slot1->m_netState != RS_NET_IDLE) &&
        (m_slot2->m_rfState != RS_RF_LISTENING || m_slot2->m_netState != RS_NET_IDLE);
}

/// <summary>
/// Helper to change the debug and verbose state.
/// </summary>
/// <param name="debug">Flag indicating whether DMR debug is enabled.</param>
/// <param name="verbose">Flag indicating whether DMR verbose logging is enabled.</param>
void Control::setDebugVerbose(bool debug, bool verbose)
{
    m_debug = debug;
    m_verbose = verbose;
    m_slot1->setDebugVerbose(debug, verbose);
    m_slot2->setDebugVerbose(debug, verbose);
}

/// <summary>
/// Helper to change the CSBK verbose state.
/// </summary>
/// <param name="verbose">Flag indicating whether CSBK dumping is enabled.</param>
void Control::setCSBKVerbose(bool verbose)
{
    m_dumpCSBKData = verbose;
    lc::CSBK::setVerbose(verbose);
}
