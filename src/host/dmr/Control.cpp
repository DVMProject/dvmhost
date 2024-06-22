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
*   Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
*   Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "common/dmr/acl/AccessControl.h"
#include "common/dmr/lc/CSBK.h"
#include "common/dmr/lc/csbk/CSBKFactory.h"
#include "common/Log.h"
#include "dmr/Control.h"

using namespace dmr;
using namespace dmr::defines;

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
/// <param name="chLookup">Instance of the ChannelLookup class.</param>
/// <param name="ridLookup">Instance of the RadioIdLookup class.</param>
/// <param name="tidLookup">Instance of the TalkgroupRulesLookup class.</param>
/// <param name="idenTable">Instance of the IdenTableLookup class.</param>
/// <param name="rssiMapper">Instance of the RSSIInterpolator class.</param>
/// <param name="jitter"></param>
/// <param name="dumpDataPacket"></param>
/// <param name="repeatDataPacket"></param>
/// <param name="dumpCSBKData"></param>
/// <param name="debug">Flag indicating whether DMR debug is enabled.</param>
/// <param name="verbose">Flag indicating whether DMR verbose logging is enabled.</param>
Control::Control(bool authoritative, uint32_t colorCode, uint32_t callHang, uint32_t queueSize, bool embeddedLCOnly,
    bool dumpTAData, uint32_t timeout, uint32_t tgHang, modem::Modem* modem, network::Network* network, bool duplex, ::lookups::ChannelLookup* chLookup,
    ::lookups::RadioIdLookup* ridLookup, ::lookups::TalkgroupRulesLookup* tidLookup, ::lookups::IdenTableLookup* idenTable, ::lookups::RSSIInterpolator* rssiMapper,
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
    assert(chLookup != nullptr);
    assert(ridLookup != nullptr);
    assert(tidLookup != nullptr);
    assert(idenTable != nullptr);
    assert(rssiMapper != nullptr);

    acl::AccessControl::init(m_ridLookup, m_tidLookup);
    Slot::init(this, authoritative, colorCode, SiteData(), embeddedLCOnly, dumpTAData, callHang, modem, network, duplex, chLookup, m_ridLookup, m_tidLookup, m_idenTable, rssiMapper, jitter, verbose);
    lc::CSBK::setVerbose(m_dumpCSBKData);

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
/// <param name="controlChData">Control Channel data.</param>
/// <param name="netId">DMR Network ID.</param>
/// <param name="siteId">DMR Site ID.</param>
/// <param name="channelId">Channel ID.</param>
/// <param name="channelNo">Channel Number.</param>
/// <param name="printOptions"></param>
void Control::setOptions(yaml::Node& conf, bool supervisor, ::lookups::VoiceChData controlChData, 
    uint32_t netId, uint8_t siteId, uint8_t channelId, uint32_t channelNo, bool printOptions)
{
    yaml::Node systemConf = conf["system"];
    yaml::Node dmrProtocol = conf["protocols"]["dmr"];

    m_supervisor = supervisor;

    Slot::m_verifyReg = dmrProtocol["verifyReg"].as<bool>(false);

    uint8_t nRandWait = (uint8_t)dmrProtocol["nRandWait"].as<uint32_t>(DEFAULT_NRAND_WAIT);
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

    Slot::setSiteData(controlChData, netId, siteId, channelId, channelNo, dedicatedTSCC);
    Slot::setAlohaConfig(nRandWait, backOff);

    bool disableGrantSourceIdCheck = control["disableGrantSourceIdCheck"].as<bool>(false);

    if (enableTSCC) {
        m_tsccSlotNo = (uint8_t)control["slot"].as<uint32_t>(0U);
        switch (m_tsccSlotNo) {
        case 1U:
            m_slot1->setTSCC(enableTSCC, dedicatedTSCC);
            m_slot1->setSupervisor(m_supervisor);
            m_slot1->setDisableSourceIDGrantCheck(disableGrantSourceIdCheck);
            break;
        case 2U:
            m_slot2->setTSCC(enableTSCC, dedicatedTSCC);
            m_slot2->setSupervisor(m_supervisor);
            m_slot2->setDisableSourceIDGrantCheck(disableGrantSourceIdCheck);
            break;
        default:
            LogError(LOG_DMR, "DMR, invalid slot, TSCC disabled, slotNo = %u", m_tsccSlotNo);
            break;
        }
    }

    m_enableTSCC = enableTSCC;

    yaml::Node rfssConfig = systemConf["config"];
    yaml::Node controlCh = rfssConfig["controlCh"];
    bool notifyCC = controlCh["notifyEnable"].as<bool>(false);
    m_slot1->setNotifyCC(notifyCC);
    m_slot2->setNotifyCC(notifyCC);

    bool disableUnitRegTimeout = dmrProtocol["disableUnitRegTimeout"].as<bool>(false);
    m_slot1->m_affiliations->setDisableUnitRegTimeout(disableUnitRegTimeout);
    m_slot2->m_affiliations->setDisableUnitRegTimeout(disableUnitRegTimeout);

    /*
    ** Voice Silence and Frame Loss Thresholds
    */
    uint32_t silenceThreshold = dmrProtocol["silenceThreshold"].as<uint32_t>(DEFAULT_SILENCE_THRESHOLD);
    if (silenceThreshold > MAX_DMR_VOICE_ERRORS) {
        LogWarning(LOG_DMR, "Silence threshold > %u, defaulting to %u", MAX_DMR_VOICE_ERRORS, DEFAULT_SILENCE_THRESHOLD);
        silenceThreshold = DEFAULT_SILENCE_THRESHOLD;
    }

    // either MAX_DMR_VOICE_ERRORS or 0 will disable the threshold logic
    if (silenceThreshold == 0U) {
        LogWarning(LOG_DMR, "Silence threshold set to zero, defaulting to %u", MAX_DMR_VOICE_ERRORS);
        silenceThreshold = MAX_DMR_VOICE_ERRORS;
    }

    m_slot1->setSilenceThreshold(silenceThreshold);
    m_slot2->setSilenceThreshold(silenceThreshold);

    uint8_t frameLossThreshold = (uint8_t)dmrProtocol["frameLossThreshold"].as<uint32_t>(DEFAULT_FRAME_LOSS_THRESHOLD);
    if (frameLossThreshold == 0U) {
        frameLossThreshold = 1U;
    }

    if (frameLossThreshold > DEFAULT_FRAME_LOSS_THRESHOLD * 2U) {
        LogWarning(LOG_DMR, "Frame loss threshold may be excessive, default is %u, configured is %u", DEFAULT_FRAME_LOSS_THRESHOLD, frameLossThreshold);
    }

    m_slot1->setFrameLossThreshold(frameLossThreshold);
    m_slot2->setFrameLossThreshold(frameLossThreshold);

    if (printOptions) {
        if (enableTSCC) {
            LogInfo("    TSCC Slot: %u", m_tsccSlotNo);
            LogInfo("    TSCC Aloha Random Access Wait: %u", nRandWait);
            LogInfo("    TSCC Aloha Backoff: %u", backOff);
            if (disableGrantSourceIdCheck) {
                LogInfo("    TSCC Disable Grant Source ID Check: yes");
            }
        }

        LogInfo("    Notify Control: %s", notifyCC ? "yes" : "no");
        LogInfo("    Silence Threshold: %u (%.1f%%)", silenceThreshold, float(silenceThreshold) / 1.41F);
        LogInfo("    Frame Loss Threshold: %u", frameLossThreshold);

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
    if (data[0U] != modem::TAG_DATA || data[1U] != (IDLE_RX | SYNC_DATA | DataType::CSBK))
        return false;

    // generate a new CSBK and check validity
    std::unique_ptr<lc::CSBK> csbk = lc::csbk::CSBKFactory::createCSBK(data + 2U, DataType::CSBK);
    if (csbk == nullptr)
        return false;

    uint8_t csbko = csbk->getCSBKO();
    if (csbko != CSBKO::BSDWNACT)
        return false;

    uint32_t srcId = csbk->getSrcId();

    // check the srcId against the ACL control
    bool ret = acl::AccessControl::validateSrcId(srcId);
    if (!ret) {
        LogError(LOG_RF, "DMR, invalid CSBKO, BSDWNACT, srcId = %u", srcId);
        return false;
    }

    if (m_verbose) {
        LogMessage(LOG_RF, "DMR, CSBKO, BSDWNACT, srcId = %u", srcId);
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
/// Get the frame data length for the next frame in the data ring buffer.
/// </summary>
/// <param name="slotNo"></param>
/// <returns>Length of frame data retrieved.</returns>
uint32_t Control::peekFrameLength(uint32_t slotNo)
{
    switch (slotNo) {
    case 1U:
        return m_slot1->peekFrameLength();
    case 2U:
        return m_slot2->peekFrameLength();
    default:
        LogError(LOG_DMR, "DMR, invalid slot, slotNo = %u", slotNo);
        return 0U;
    }
}

/// <summary>
/// Get a data frame for slot, from data ring buffer.
/// </summary>
/// <param name="slotNo"></param>
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
/// Updates the processor.
/// </summary>
void Control::clock()
{
    if (m_network != nullptr) {
        processNetwork();
    }

    m_slot1->clock();
    m_slot2->clock();
}

/// <summary>
/// Updates the adj. site tables.
/// </summary>
/// <param name="ms"></param>
void Control::clockSiteData(uint32_t ms)
{
    m_slot1->clockSiteData(ms);
    m_slot2->clockSiteData(ms);
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
/// Grants a TGID on a non-authoritative host.
/// </summary>
/// <param name="dstId"></param>
/// <paran name="slot"></param>
void Control::grantTG(uint32_t srcId, uint32_t dstId, uint8_t slot, bool grp)
{
    switch (slot) {
    case 1U:
        m_slot1->grantTG(srcId, dstId, grp);
        break;
    case 2U:
        m_slot2->grantTG(srcId, dstId, grp);
        break;
    default:
        LogError(LOG_DMR, "DMR, invalid slot, slotNo = %u", slot);
        break;
    }
}

/// <summary>
/// Releases a granted TG.
/// </summary>
/// <param name="dstId"></param>
/// <paran name="slot"></param>
void Control::releaseGrantTG(uint32_t dstId, uint8_t slot)
{
    switch (slot) {
    case 1U:
        m_slot1->releaseGrantTG(dstId);
        break;
    case 2U:
        m_slot2->releaseGrantTG(dstId);
        break;
    default:
        LogError(LOG_DMR, "DMR, invalid slot, slotNo = %u", slot);
        break;
    }
}

/// <summary>
/// Touchs a granted TG to keep a channel grant alive.
/// </summary>
/// <param name="dstId"></param>
/// <paran name="slot"></param>
void Control::touchGrantTG(uint32_t dstId, uint8_t slot)
{
    switch (slot) {
    case 1U:
        m_slot1->touchGrantTG(dstId);
        break;
    case 2U:
        m_slot2->touchGrantTG(dstId);
        break;
    default:
        LogError(LOG_DMR, "DMR, invalid slot, slotNo = %u", slot);
        break;
    }
}

/// <summary>
/// Gets instance of the AffiliationLookup class.
/// </summary>
dmr::lookups::DMRAffiliationLookup* Control::affiliations()
{
    switch (m_tsccSlotNo) {
    case 1U:
        return m_slot1->m_affiliations;
    case 2U:
        return m_slot2->m_affiliations;
    default:
        LogError(LOG_DMR, "DMR, invalid slot, slotNo = %u", m_tsccSlotNo);
        break;
    }

    return nullptr;
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

/// <summary>
/// Helper to get the last transmitted destination ID.
/// </summary>
/// <param name="slotNo">DMR slot number.</param>
/// <returns></returns>
uint32_t Control::getLastDstId(uint32_t slotNo) const
{
    switch (slotNo) {
    case 1U:
        return m_slot1->getLastDstId();
    case 2U:
        return m_slot2->getLastDstId();
    default:
        LogError(LOG_DMR, "DMR, invalid slot, slotNo = %u", slotNo);
        break;
    }

    return 0U;
}

/// <summary>
/// Helper to get the last transmitted source ID.
/// </summary>
/// <param name="slotNo">DMR slot number.</param>
/// <returns></returns>
uint32_t Control::getLastSrcId(uint32_t slotNo) const
{
    switch (slotNo) {
    case 1U:
        return m_slot1->getLastSrcId();
    case 2U:
        return m_slot2->getLastSrcId();
    default:
        LogError(LOG_DMR, "DMR, invalid slot, slotNo = %u", slotNo);
        break;
    }

    return 0U;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Process a data frames from the network.
/// </summary>
void Control::processNetwork()
{
    uint32_t length = 0U;
    bool ret = false;
    UInt8Array buffer = m_network->readDMR(ret, length);
    if (!ret)
        return;
    if (length == 0U)
        return;
    if (buffer == nullptr) {
        return;
    }

    data::Data data;

    // process network message header
    uint8_t seqNo = buffer[4U];

    uint32_t srcId = __GET_UINT16(buffer, 5U);
    uint32_t dstId = __GET_UINT16(buffer, 8U);

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

    data.setSeqNo(seqNo);
    data.setSlotNo(slotNo);
    data.setSrcId(srcId);
    data.setDstId(dstId);
    data.setFLCO(flco);

    bool dataSync = (buffer[15U] & 0x20U) == 0x20U;
    bool voiceSync = (buffer[15U] & 0x10U) == 0x10U;

    if (m_debug) {
        LogDebug(LOG_NET, "DMR, seqNo = %u, srcId = %u, dstId = %u, flco = $%02X, slotNo = %u, len = %u", seqNo, srcId, dstId, flco, slotNo, length);
    }

    // process raw DMR data bytes
    if (dataSync) {
        DataType::E dataType = (DataType::E)(buffer[15U] & 0x0FU);
        data.setData(buffer.get() + 20U);
        data.setDataType(dataType);
        data.setN(0U);
    }
    else if (voiceSync) {
        data.setData(buffer.get() + 20U);
        data.setDataType(DataType::VOICE_SYNC);
        data.setN(0U);
    }
    else {
        uint8_t n = buffer[15U] & 0x0FU;
        data.setData(buffer.get() + 20U);
        data.setDataType(DataType::VOICE);
        data.setN(n);
    }

    // forward onto the specific slot for final processing and delivery
    switch (slotNo) {
        case 1U:
            m_slot1->processNetwork(data);
            break;
        case 2U:
            m_slot2->processNetwork(data);
            break;
    }
}
