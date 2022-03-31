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
*   Copyright (C) 2017-2021 by Bryan Biedenkapp <gatekeep@jmp.cx>
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
Control::Control(uint32_t colorCode, uint32_t callHang, uint32_t queueSize, bool embeddedLCOnly,
    bool dumpTAData, uint32_t timeout, uint32_t tgHang, modem::Modem* modem, network::BaseNetwork* network, bool duplex,
    lookups::RadioIdLookup* ridLookup, lookups::TalkgroupIdLookup* tidLookup, lookups::IdenTableLookup* idenTable, lookups::RSSIInterpolator* rssiMapper,
    uint32_t jitter, bool dumpDataPacket, bool repeatDataPacket, bool dumpCSBKData, bool debug, bool verbose) :
    m_colorCode(colorCode),
    m_modem(modem),
    m_network(network),
    m_slot1(NULL),
    m_slot2(NULL),
    m_idenTable(idenTable),
    m_ridLookup(ridLookup),
    m_tidLookup(tidLookup),
    m_tsccSlotNo(0U),
    m_ccRunning(false),
    m_dumpCSBKData(dumpCSBKData),
    m_verbose(verbose),
    m_debug(debug)
{
    assert(modem != NULL);
    assert(ridLookup != NULL);
    assert(tidLookup != NULL);
    assert(idenTable != NULL);
    assert(rssiMapper != NULL);

    acl::AccessControl::init(m_ridLookup, m_tidLookup);
    Slot::init(colorCode, SiteData(), embeddedLCOnly, dumpTAData, callHang, modem, network, duplex, m_ridLookup, m_tidLookup, m_idenTable, rssiMapper, jitter);
    
    m_slot1 = new Slot(1U, timeout, tgHang, queueSize, dumpDataPacket, repeatDataPacket, dumpCSBKData, debug, verbose);
    m_slot2 = new Slot(2U, timeout, tgHang, queueSize, dumpDataPacket, repeatDataPacket, dumpCSBKData, debug, verbose);
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
/// <param name="netId"></param>
/// <param name="siteId"></param>
/// <param name="channelId"></param>
/// <param name="channelNo"></param>
/// <param name="printOptions"></param>
void Control::setOptions(yaml::Node& conf, uint32_t netId, uint8_t siteId, uint8_t channelId, uint32_t channelNo, bool printOptions)
{
    yaml::Node systemConf = conf["system"];
    yaml::Node dmrProtocol = conf["protocols"]["dmr"];

    Slot::setSiteData(netId, siteId, channelId, channelNo);

    yaml::Node control = dmrProtocol["control"];
    bool enableTSCC = control["enable"].as<bool>(false);
    bool dedicatedTSCC = false;
    if (enableTSCC) {
        dedicatedTSCC = control["dedicated"].as<bool>(false);
    }
    else {
        dedicatedTSCC = false;
    }

    m_tsccSlotNo = (uint8_t)control["slot"].as<uint32_t>(0U);
    switch (m_tsccSlotNo) {
    case 1U:
        m_slot1->setTSCC(enableTSCC, dedicatedTSCC);
        break;
    case 2U:
        m_slot2->setTSCC(enableTSCC, dedicatedTSCC);
        break;
    default:
        LogError(LOG_DMR, "DMR, invalid slot, TSCC disabled, slotNo = %u", m_tsccSlotNo);
        break;
    }

    uint32_t silenceThreshold = dmrProtocol["silenceThreshold"].as<uint32_t>(dmr::DEFAULT_SILENCE_THRESHOLD);
    if (silenceThreshold > MAX_DMR_VOICE_ERRORS) {
        LogWarning(LOG_DMR, "Silence threshold > %u, defaulting to %u", dmr::MAX_DMR_VOICE_ERRORS, dmr::DEFAULT_SILENCE_THRESHOLD);
        silenceThreshold = dmr::DEFAULT_SILENCE_THRESHOLD;
    }

    m_slot1->setSilenceThreshold(silenceThreshold);
    m_slot2->setSilenceThreshold(silenceThreshold);

    if (printOptions) {
        LogInfo("    TSCC Slot: %u", m_tsccSlotNo);
        LogInfo("    Silence Threshold: %u (%.1f%%)", silenceThreshold, float(silenceThreshold) / 1.41F);
    }
}

/// <summary>
/// Sets a flag indicating whether the DMR control channel is running.
/// </summary>
/// <param name="ccRunning"></param>
void Control::setCCRunning(bool ccRunning)
{
    m_ccRunning = ccRunning;
    switch (m_tsccSlotNo) {
    case 1U:
        m_slot1->setCCRunning(ccRunning);
        break;
    case 2U:
        m_slot2->setCCRunning(ccRunning);
        break;
    default:
        LogError(LOG_NET, "DMR, invalid slot, TSCC disabled, slotNo = %u", m_tsccSlotNo);
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
    assert(data != NULL);

    // wakeups always come in on slot 1
    if (data[0U] != modem::TAG_DATA || data[1U] != (DMR_IDLE_RX | DMR_SYNC_DATA | DT_CSBK))
        return false;

    // generate a new CSBK and check validity
    lc::CSBK csbk = lc::CSBK(SiteData(), lookups::IdenTable(), m_dumpCSBKData);

    bool valid = csbk.decode(data + 2U);
    if (!valid)
        return false;

    uint8_t csbko = csbk.getCSBKO();
    if (csbko != CSBKO_BSDWNACT)
        return false;

    uint32_t srcId = csbk.getSrcId();

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
/// Process a data frame for slot 1, from the RF interface.
/// </summary>
/// <param name="data">DMR data frame buffer.</param>
/// <param name="len">Length of data frame buffer.</param>
/// <returns>True, if data frame was processed, otherwise false.</returns>
bool Control::processFrame1(uint8_t *data, uint32_t len)
{
    assert(data != NULL);

    return m_slot1->processFrame(data, len);
}

/// <summary>
/// Get a frame data for slot 1, from data ring buffer.
/// </summary>
/// <param name="data">Buffer to put retrieved DMR data frame data.</param>
/// <returns>Length of data retrieved from DMR ring buffer.</returns>
uint32_t Control::getFrame1(uint8_t* data)
{
    assert(data != NULL);

    return m_slot1->getFrame(data);
}

/// <summary>
/// Process a data frame for slot 2, from the RF interface.
/// </summary>
/// <param name="data">DMR data frame buffer.</param>
/// <param name="len">Length of data frame buffer.</param>
/// <returns>True, if data frame was processed, otherwise false.</returns>
bool Control::processFrame2(uint8_t *data, uint32_t len)
{
    assert(data != NULL);

    return m_slot2->processFrame(data, len);
}

/// <summary>
/// Get a frame data for slot 2, from data ring buffer.
/// </summary>
/// <param name="data">Buffer to put retrieved DMR data frame data.</param>
/// <returns>Length of data retrieved from DMR ring buffer.</returns>
uint32_t Control::getFrame2(uint8_t *data)
{
    assert(data != NULL);

    return m_slot2->getFrame(data);
}

/// <summary>
/// Updates the processor.
/// </summary>
void Control::clock()
{
    if (m_network != NULL) {
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
                    LogError(LOG_NET, "DMR, invalid slot, slotNo = %u", slotNo);
                    break;
            }
        }
    }

    m_slot1->clock();
    m_slot2->clock();
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
        LogError(LOG_NET, "DMR, invalid slot, slotNo = %u", slotNo);
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
        LogError(LOG_NET, "DMR, invalid slot, slotNo = %u", slotNo);
        break;
    }
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
