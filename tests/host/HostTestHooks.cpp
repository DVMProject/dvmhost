// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "host/HostTestHooks.h"
#include "common/dmr/SlotType.h"
#include "common/dmr/Sync.h"
#include "common/dmr/lc/FullLC.h"
#include "common/nxdn/NXDNDefines.h"
#include "common/nxdn/NXDNUtils.h"
#include "common/nxdn/Sync.h"
#include "common/nxdn/channel/FACCH1.h"
#include "common/nxdn/channel/LICH.h"
#include "common/nxdn/channel/SACCH.h"
#include "common/nxdn/lc/RTCH.h"

#include <cstring>

#if defined(CATCH2_TEST_COMPILATION)

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

static const uint8_t DMR_CAL_VH_1K_PAYLOAD[dmr::defines::DMR_FRAME_LENGTH_BYTES] = {
    0x00U, 0x20U, 0x08U, 0x08U, 0x02U, 0x38U, 0x15U, 0x00U, 0x2CU, 0xA0U, 0x14U,
    0x60U, 0x84U, 0x6DU, 0xFFU, 0x57U, 0xD7U, 0x5DU, 0xF5U, 0xDEU, 0x30U, 0x30U,
    0x01U, 0x10U, 0x01U, 0x40U, 0x03U, 0xC0U, 0x13U, 0xC1U, 0x1EU, 0x80U, 0x6FU
};

static const uint8_t NXDN_CAL_1K_PAYLOAD[nxdn::defines::NXDN_FRAME_LENGTH_BYTES] = {
    0xCDU, 0xF5U, 0x9DU, 0x5DU, 0x7CU, 0xFAU, 0x0AU, 0x6EU, 0x8AU, 0x23U, 0x56U, 0xE8U,
    0x4CU, 0xAAU, 0xDEU, 0x8BU, 0x26U, 0xE4U, 0xF2U, 0x82U, 0x88U,
    0xC6U, 0x8AU, 0x74U, 0x29U, 0xA4U, 0xECU, 0xD0U, 0x08U, 0x22U,
    0xCEU, 0xA2U, 0xFCU, 0x01U, 0x8CU, 0xECU, 0xDAU, 0x0AU, 0xA0U,
    0xEEU, 0x8AU, 0x7EU, 0x2BU, 0x26U, 0xCCU, 0xF8U, 0x8AU, 0x08U
};

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

/* Gets DMR slot 1 instance. */

dmr::Slot* HostTestHooks::dmrSlot1(const dmr::Control& control) { return control.m_slot1; }

/* Gets DMR slot 2 instance. */

dmr::Slot* HostTestHooks::dmrSlot2(const dmr::Control& control) { return control.m_slot2; }

/* Gets DMR network state. */

RPT_NET_STATE HostTestHooks::dmrNetState(const dmr::Slot& slot) { return slot.m_netState; }

/* Gets DMR RF state. */

RPT_RF_STATE HostTestHooks::dmrRFState(const dmr::Slot& slot) { return slot.m_rfState; }

/* Gets DMR permitted destination ID. */

uint32_t HostTestHooks::dmrPermittedDstId(const dmr::Slot& slot) { return slot.m_permittedDstId; }

/* Gets DMR network watchdog timer. */

Timer& HostTestHooks::dmrNetworkWatchdog(dmr::Slot& slot) { return slot.m_networkWatchdog; }

/* Gets DMR network talkgroup hang timer. */

Timer& HostTestHooks::dmrNetTGHang(dmr::Slot& slot) { return slot.m_netTGHang; }

/* Gets DMR RF talkgroup hang timer. */

Timer& HostTestHooks::dmrRFTGHang(dmr::Slot& slot) { return slot.m_rfTGHang; }

/* Gets DMR RF loss watchdog timer. */

Timer& HostTestHooks::dmrRFLossWatchdog(dmr::Slot& slot) { return slot.m_rfLossWatchdog; }

/* Forces DMR slot into active RF call state. */

void HostTestHooks::dmrSetRFCall(dmr::Slot& slot, uint32_t srcId, uint32_t dstId)
{
    slot.m_rfState = RS_RF_AUDIO;
    slot.m_rfLastSrcId = srcId;
    slot.m_rfLastDstId = dstId;
    slot.m_rfTGHang.start();
}

/* Returns DMR slot to RF listening state. */

void HostTestHooks::dmrClearRFCall(dmr::Slot& slot)
{
    slot.m_rfState = RS_RF_LISTENING;
    slot.m_rfLastSrcId = 0U;
    slot.m_rfLastDstId = 0U;
    slot.m_rfTGHang.stop();
}

/* Forces DMR slot into RF rejected state. */

void HostTestHooks::dmrSetRFRejected(dmr::Slot& slot)
{
    slot.m_rfState = RS_RF_REJECTED;
}

/* Injects synthetic DMR network voice call. */

void HostTestHooks::dmrStartNetVoiceCall(dmr::Slot& slot, uint32_t srcId, uint32_t dstId, bool group)
{
    dmr::data::NetData dmrData;
    dmrData.setSlotNo(slot.m_slotNo);
    dmrData.setSrcId(srcId);
    dmrData.setDstId(dstId);
    dmrData.setFLCO(group ? dmr::defines::FLCO::GROUP : dmr::defines::FLCO::PRIVATE);
    dmrData.setDataType(dmr::defines::DataType::VOICE_LC_HEADER);

    uint8_t data[dmr::defines::DMR_FRAME_LENGTH_BYTES];
    ::memset(data, 0x00U, sizeof(data));

    dmr::lc::LC lc(dmrData.getFLCO(), srcId, dstId);
    dmr::lc::FullLC fullLC;
    fullLC.encode(lc, data, dmr::defines::DataType::VOICE_LC_HEADER);

    dmr::SlotType slotType;
    slotType.setColorCode(dmr::Slot::s_colorCode);
    slotType.setDataType(dmr::defines::DataType::VOICE_LC_HEADER);
    slotType.encode(data);

    dmr::Sync::addDMRDataSync(data, dmr::Slot::s_duplex);

    dmrData.setData(data);
    slot.processNetwork(dmrData);
}

/* Injects synthetic DMR RF voice call frame. */

bool HostTestHooks::dmrStartRFVoiceCall(dmr::Slot& slot, uint32_t srcId, uint32_t dstId, bool group)
{
    uint8_t frame[dmr::defines::DMR_FRAME_LENGTH_BYTES + 2U];
    ::memset(frame, 0x00U, sizeof(frame));

    ::memcpy(frame + 2U, DMR_CAL_VH_1K_PAYLOAD, dmr::defines::DMR_FRAME_LENGTH_BYTES);

    frame[0U] = modem::TAG_DATA;
    frame[1U] = dmr::defines::SYNC_DATA | dmr::defines::DataType::VOICE_LC_HEADER;

    dmr::lc::LC lc(group ? dmr::defines::FLCO::GROUP : dmr::defines::FLCO::PRIVATE, srcId, dstId);
    dmr::lc::FullLC fullLC;
    fullLC.encode(lc, frame + 2U, dmr::defines::DataType::VOICE_LC_HEADER);

    dmr::SlotType slotType;
    slotType.setColorCode(dmr::Slot::s_colorCode);
    slotType.setDataType(dmr::defines::DataType::VOICE_LC_HEADER);
    slotType.encode(frame + 2U);

    dmr::Sync::addDMRDataSync(frame + 2U, dmr::Slot::s_duplex);

    return slot.processFrame(frame, sizeof(frame));
}

/* Gets a P25 Phase 2 slot for tests. */

p25::phase2::Slot& HostTestHooks::p25P2Slot(p25::phase2::Control& control, uint32_t slotNo)
{
    assert(slotNo < p25::phase2::Control::SLOT_COUNT);
    return slotNo == 0U ? *control.m_slot1 : *control.m_slot2;
}

/* Gets P25 Phase 2 RF state. */

RPT_RF_STATE HostTestHooks::p25P2RFState(const p25::phase2::Slot& slot) { return slot.m_rfState; }

void HostTestHooks::p25P2SetRFState(p25::phase2::Slot& slot, RPT_RF_STATE state) { slot.m_rfState = state; }

/* Gets P25 Phase 2 last source ID. */

uint32_t HostTestHooks::p25P2LastSrcId(const p25::phase2::Slot& slot)
{
    return slot.m_rfLastSrcId != 0U ? slot.m_rfLastSrcId : slot.m_netLastSrcId;
}

/* Gets P25 Phase 2 last destination ID. */

uint32_t HostTestHooks::p25P2LastDstId(const p25::phase2::Slot& slot)
{
    return slot.m_rfLastDstId != 0U ? slot.m_rfLastDstId : slot.m_netLastDstId;
}

/* Gets P25 Phase 2 RF scramble offset. */

uint16_t HostTestHooks::p25P2RFScrambleOffset(const p25::phase2::Slot& slot) { return slot.m_rfScrambleOffset; }

/* Gets P25 Phase 2 network scramble offset. */

uint16_t HostTestHooks::p25P2NetScrambleOffset(const p25::phase2::Slot& slot) { return slot.m_netScrambleOffset; }

/* Gets whether P25 Phase 2 ESS is complete. */

bool HostTestHooks::p25P2ESSComplete(const p25::phase2::Slot& slot)
{
    return slot.m_voice.m_rfESSComplete || slot.m_voice.m_netESSComplete;
}

/* Gets P25 Phase 2 ESS algorithm ID. */

uint8_t HostTestHooks::p25P2ESSAlgId(const p25::phase2::Slot& slot)
{
    const uint8_t* ess = slot.m_voice.m_netESSComplete ? slot.m_voice.m_netESS : slot.m_voice.m_rfESS;
    return ess[0U];
}

/* Gets P25 Phase 2 ESS key ID. */

uint16_t HostTestHooks::p25P2ESSKeyId(const p25::phase2::Slot& slot)
{
    const uint8_t* ess = slot.m_voice.m_netESSComplete ? slot.m_voice.m_netESS : slot.m_voice.m_rfESS;
    return static_cast<uint16_t>((ess[1U] << 8U) | ess[2U]);
}

/* Copies P25 Phase 2 ESS MI. */

void HostTestHooks::p25P2ESSMI(const p25::phase2::Slot& slot, uint8_t* mi)
{
    if (mi != nullptr)
        ::memcpy(mi, slot.m_voice.m_netESSComplete ? slot.m_voice.m_netESS + 3U :
            slot.m_voice.m_rfESS + 3U, 9U);
}

/* Gets P25 Phase 2 RF VCH state. */

p25::phase2::Slot::VCH_STATE HostTestHooks::p25P2RFVCHState(const p25::phase2::Slot& slot) { return slot.m_rfVCHState; }

/* Gets P25 Phase 2 network VCH state. */

p25::phase2::Slot::VCH_STATE HostTestHooks::p25P2NetVCHState(const p25::phase2::Slot& slot) { return slot.m_netVCHState; }

/* Gets P25 Phase 2 DUID. */

p25::defines::P2_DUID::E HostTestHooks::p25P2DUID(const p25::phase2::Slot& slot) { return slot.m_duid; }

/* Gets P25 Phase 2 RF PTT count. */

uint8_t HostTestHooks::p25P2RFPTTCount(const p25::phase2::Slot& slot) { return slot.m_rfPTTCount; }

/* Gets P25 Phase 2 RF frames count. */

uint32_t HostTestHooks::p25P2RFFrames(const p25::phase2::Slot& slot) { return slot.m_rfFrames; }

/* Gets P25 Phase 2 network frames count. */

uint32_t HostTestHooks::p25P2NetFrames(const p25::phase2::Slot& slot) { return slot.m_netFrames; }

/* Gets P25 Phase 2 network lost frames count. */
uint32_t HostTestHooks::p25P2NetLost(const p25::phase2::Slot& slot) { return slot.m_netLost; }

/* Gets P25 Phase 2 network missed frames count. */
uint32_t HostTestHooks::p25P2NetMissed(const p25::phase2::Slot& slot) { return slot.m_netMissed; }

/* Gets P25 Phase 2 RF bits, network bits, RF errors, and network errors. */

uint32_t HostTestHooks::p25P2RFBits(const p25::phase2::Slot& slot) { return slot.m_rfBits; }

/* Gets P25 Phase 2 network bits. */

uint32_t HostTestHooks::p25P2NetBits(const p25::phase2::Slot& slot) { return slot.m_netBits; }

/* Gets P25 Phase 2 RF errors. */

uint32_t HostTestHooks::p25P2RFErrs(const p25::phase2::Slot& slot) { return slot.m_rfErrs; }

/* Gets P25 Phase 2 network errors. */

uint32_t HostTestHooks::p25P2NetErrs(const p25::phase2::Slot& slot) { return slot.m_netErrs; }

/* Gets P25 network state. */

RPT_NET_STATE HostTestHooks::p25NetState(const p25::Control& control) { return control.m_netState; }

/* Gets P25 RF state. */

RPT_RF_STATE HostTestHooks::p25RFState(const p25::Control& control) { return control.m_rfState; }

/* Gets P25 last network destination ID. */

uint32_t HostTestHooks::p25NetLastDstId(const p25::Control& control) { return control.m_netLastDstId; }

/* Gets P25 last network source ID. */

uint32_t HostTestHooks::p25NetLastSrcId(const p25::Control& control) { return control.m_netLastSrcId; }

/* Gets P25 permitted destination ID. */

uint32_t HostTestHooks::p25PermittedDstId(const p25::Control& control) { return control.m_permittedDstId; }

/* Gets P25 tail-on-idle flag. */

bool HostTestHooks::p25TailOnIdle(const p25::Control& control) { return control.m_tailOnIdle; }

/* Gets P25 network watchdog timer. */

Timer& HostTestHooks::p25NetworkWatchdog(p25::Control& control) { return control.m_networkWatchdog; }

/* Gets P25 network talkgroup hang timer. */

Timer& HostTestHooks::p25NetTGHang(p25::Control& control) { return control.m_netTGHang; }

/* Gets P25 RF talkgroup hang timer. */

Timer& HostTestHooks::p25RFTGHang(p25::Control& control) { return control.m_rfTGHang; }

/* Gets P25 RF loss watchdog timer. */

Timer& HostTestHooks::p25RFLossWatchdog(p25::Control& control) { return control.m_rfLossWatchdog; }

/* Forces P25 control network state and last IDs. */

void HostTestHooks::p25SetNetState(p25::Control& control, RPT_NET_STATE netState, uint32_t srcId, uint32_t dstId)
{
    control.m_netState = netState;
    control.m_netLastSrcId = srcId;
    control.m_netLastDstId = dstId;
}

/* Forces P25 control into active RF call state. */

void HostTestHooks::p25SetRFCall(p25::Control& control, uint32_t srcId, uint32_t dstId)
{
    control.m_rfState = RS_RF_AUDIO;
    control.m_rfLastSrcId = srcId;
    control.m_rfLastDstId = dstId;
    control.m_rfTGHang.start();
}

/* Returns P25 control to RF listening state. */

void HostTestHooks::p25ClearRFCall(p25::Control& control)
{
    control.m_rfState = RS_RF_LISTENING;
    control.m_rfLastSrcId = 0U;
    control.m_rfLastDstId = 0U;
    control.m_rfTGHang.stop();
}

/* Forces P25 control into RF rejected state. */

void HostTestHooks::p25SetRFRejected(p25::Control& control)
{
    control.m_rfState = RS_RF_REJECTED;
}

/* Encodes and stamps a valid NID for a DUID into a P25 RF frame. */

void HostTestHooks::p25StampRFFrameNID(p25::Control& control, uint8_t* frame, p25::defines::DUID::E duid)
{
    control.m_nid.encode(frame + 2U, duid);
}

/* Injects synthetic P25 network call start. */

void HostTestHooks::p25StartNetCall(p25::Control& control, const p25::lc::LC& lc, const p25::data::LowSpeedData& lsd)
{
    p25::packet::Voice* voice = control.m_voice;
    voice->m_dfsiLC = p25::dfsi::LC(lc, lsd);
    voice->m_netLastLDU1 = lc;
    voice->writeNet_LDU1();
}

/* Injects synthetic P25 network call termination. */

bool HostTestHooks::p25TerminateNetCall(p25::Control& control, const p25::lc::LC& lc, p25::defines::DUID::E duid)
{
    p25::packet::Voice* voice = control.m_voice;
    uint8_t buffer[1U] = { 0x00U };
    p25::data::LowSpeedData lsd;
    p25::defines::FrameType::E frameType = p25::defines::FrameType::DATA_UNIT;
    p25::lc::LC localControl = lc;
    p25::defines::DUID::E localDuid = duid;
    return voice->processNetwork(buffer, 1U, localControl, lsd, localDuid, frameType);
}

/* Selects the next secondary active talkgroup for a P25 voice channel. */

bool HostTestHooks::p25NextActiveTalkgroups(p25::Control& control, const std::vector<uint32_t>& activeTG,
    uint8_t& groupUpdtIndex, const uint32_t& dstId, uint32_t& dstIdB, bool& hasDstIdB)
{
    return control.m_voice->nextActiveTalkgroups(activeTG, groupUpdtIndex, dstId, dstIdB, hasDstIdB);
}

/* Gets NXDN network state. */

RPT_NET_STATE HostTestHooks::nxdnNetState(const nxdn::Control& control) { return control.m_netState; }

/* Gets NXDN RF state. */

RPT_RF_STATE HostTestHooks::nxdnRFState(const nxdn::Control& control) { return control.m_rfState; }

/* Gets NXDN last network destination ID. */

uint32_t HostTestHooks::nxdnNetLastDstId(const nxdn::Control& control) { return control.m_netLastDstId; }

/* Gets NXDN last network source ID. */

uint32_t HostTestHooks::nxdnNetLastSrcId(const nxdn::Control& control) { return control.m_netLastSrcId; }

/* Gets NXDN permitted destination ID. */

uint32_t HostTestHooks::nxdnPermittedDstId(const nxdn::Control& control) { return control.m_permittedDstId; }

/* Gets NXDN network watchdog timer. */

Timer& HostTestHooks::nxdnNetworkWatchdog(nxdn::Control& control) { return control.m_networkWatchdog; }

/* Gets NXDN network talkgroup hang timer. */

Timer& HostTestHooks::nxdnNetTGHang(nxdn::Control& control) { return control.m_netTGHang; }

/* Gets NXDN RF talkgroup hang timer. */

Timer& HostTestHooks::nxdnRFTGHang(nxdn::Control& control) { return control.m_rfTGHang; }

/* Gets NXDN RF loss watchdog timer. */

Timer& HostTestHooks::nxdnRFLossWatchdog(nxdn::Control& control) { return control.m_rfLossWatchdog; }

/* Forces NXDN control into active RF call state. */

void HostTestHooks::nxdnSetRFCall(nxdn::Control& control, uint32_t srcId, uint32_t dstId)
{
    control.m_rfState = RS_RF_AUDIO;
    control.m_rfLastSrcId = srcId;
    control.m_rfLastDstId = dstId;
    control.m_rfTGHang.start();
}

/* Returns NXDN control to RF listening state. */

void HostTestHooks::nxdnClearRFCall(nxdn::Control& control)
{
    control.m_rfState = RS_RF_LISTENING;
    control.m_rfLastSrcId = 0U;
    control.m_rfLastDstId = 0U;
    control.m_rfTGHang.stop();
}

/* Forces NXDN control into RF rejected state. */

void HostTestHooks::nxdnSetRFRejected(nxdn::Control& control)
{
    control.m_rfState = RS_RF_REJECTED;
}

/* Injects synthetic NXDN RF call start frame. */

bool HostTestHooks::nxdnStartRFCall(nxdn::Control& control, uint32_t srcId, uint32_t dstId)
{
    uint8_t start[nxdn::defines::NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(start, 0x00U, sizeof(start));

    ::memcpy(start + 2U, NXDN_CAL_1K_PAYLOAD, nxdn::defines::NXDN_FRAME_LENGTH_BYTES);

    start[0U] = modem::TAG_DATA;
    start[1U] = 0x01U;

    nxdn::Sync::addNXDNSync(start + 2U);

    nxdn::channel::LICH lich;
    lich.setRFCT(nxdn::defines::RFChannelType::RTCH);
    lich.setFCT(nxdn::defines::FuncChannelType::USC_SACCH_NS);
    lich.setOption(nxdn::defines::ChOption::STEAL_FACCH);
    lich.setOutbound(false);
    lich.encode(start + 2U);

    nxdn::channel::SACCH sacch;
    sacch.setData(nxdn::defines::SACCH_IDLE);
    sacch.setRAN(control.m_ran);
    sacch.setStructure(nxdn::defines::ChStructure::SR_SINGLE);
    sacch.encode(start + 2U);

    uint8_t buffer[nxdn::defines::NXDN_RTCH_LC_LENGTH_BYTES];
    nxdn::lc::RTCH lc;
    lc.setMessageType(nxdn::defines::MessageType::RTCH_VCALL);
    lc.setSrcId(srcId);
    lc.setDstId(dstId);
    lc.setGroup(true);
    lc.setTransmissionMode(nxdn::defines::TransmissionMode::MODE_4800);
    lc.encode(buffer, nxdn::defines::NXDN_RTCH_LC_LENGTH_BITS);

    nxdn::channel::FACCH1 facch;
    facch.setData(buffer);
    facch.encode(start + 2U, nxdn::defines::NXDN_FSW_LENGTH_BITS + nxdn::defines::NXDN_LICH_LENGTH_BITS + nxdn::defines::NXDN_SACCH_FEC_LENGTH_BITS);
    facch.encode(start + 2U, nxdn::defines::NXDN_FSW_LENGTH_BITS + nxdn::defines::NXDN_LICH_LENGTH_BITS + nxdn::defines::NXDN_SACCH_FEC_LENGTH_BITS + nxdn::defines::NXDN_FACCH1_FEC_LENGTH_BITS);

    nxdn::NXDNUtils::scrambler(start + 2U);

    return control.processFrame(start, sizeof(start));
}

/* Injects synthetic NXDN network call start frame. */

bool HostTestHooks::nxdnStartNetCall(nxdn::Control& control, const nxdn::lc::RTCH& lc)
{
    uint8_t start[nxdn::defines::NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(start, 0x00U, sizeof(start));

    nxdn::Sync::addNXDNSync(start + 2U);

    nxdn::channel::LICH lich;
    lich.setRFCT(nxdn::defines::RFChannelType::RCCH);
    lich.setFCT(nxdn::defines::FuncChannelType::USC_SACCH_NS);
    lich.setOption(nxdn::defines::ChOption::STEAL_FACCH);
    lich.setOutbound(true);
    lich.encode(start + 2U);

    nxdn::channel::SACCH sacch;
    sacch.setData(nxdn::defines::SACCH_IDLE);
    sacch.setRAN(control.m_ran);
    sacch.setStructure(nxdn::defines::ChStructure::SR_SINGLE);
    sacch.encode(start + 2U);

    uint8_t buffer[nxdn::defines::NXDN_RTCH_LC_LENGTH_BYTES];
    nxdn::lc::RTCH localControl = lc;
    localControl.encode(buffer, nxdn::defines::NXDN_RTCH_LC_LENGTH_BITS);

    nxdn::channel::FACCH1 facch;
    facch.setData(buffer);
    facch.encode(start + 2U, nxdn::defines::NXDN_FSW_LENGTH_BITS + nxdn::defines::NXDN_LICH_LENGTH_BITS + nxdn::defines::NXDN_SACCH_FEC_LENGTH_BITS);
    facch.encode(start + 2U, nxdn::defines::NXDN_FSW_LENGTH_BITS + nxdn::defines::NXDN_LICH_LENGTH_BITS + nxdn::defines::NXDN_SACCH_FEC_LENGTH_BITS + nxdn::defines::NXDN_FACCH1_FEC_LENGTH_BITS);

    return control.m_voice->processNetwork(nxdn::defines::FuncChannelType::USC_SACCH_NS, nxdn::defines::ChOption::STEAL_FACCH, localControl, start, sizeof(start));
}
#endif
