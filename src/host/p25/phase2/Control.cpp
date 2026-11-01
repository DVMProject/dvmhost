// SPDX-License-Identifier: GPL-2.0-only
/**
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/Log.h"
#include "p25/phase2/Control.h"

#include <cassert>

using namespace p25::phase2;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a P25 Phase 2 controller. */

Control::Control(bool authoritative, uint32_t callHang, uint32_t timeout, uint32_t tgHang,
    modem::Modem* modem, network::Network* network, lookups::P25AffiliationLookup* affiliations,
    ::lookups::RadioIdLookup* ridLookup, ::lookups::TalkgroupRulesLookup* tidLookup,
    uint32_t queueSize, bool debug, bool verbose, uint16_t colorCode) :
    m_modem(modem),
    m_network(network),
    m_slot1(nullptr),
    m_slot2(nullptr),
    m_ridLookup(ridLookup),
    m_tidLookup(tidLookup),
    m_affiliations(affiliations),
    m_callHang(callHang),
    m_timeout(timeout),
    m_tgHang(tgHang),
    m_debug(debug),
    m_verbose(verbose)
{
    Slot::init(this, authoritative, callHang, modem, network, affiliations, ridLookup, tidLookup, colorCode);
    m_slot1 = new Slot(0U, timeout, tgHang, queueSize, debug, verbose);
    m_slot2 = new Slot(1U, timeout, tgHang, queueSize, debug, verbose);
}

/* Finalizes a P25 Phase 2 controller. */

Control::~Control()
{
    delete m_slot2;
    delete m_slot1;
}

/* Resets controller modem queues and slot state. */

void Control::reset()
{
    if (m_modem != nullptr) {
        m_modem->clearP25P2Frame1();
        m_modem->clearP25P2Frame2();
    }

    m_slot1->reset();
    m_slot2->reset();
}

/* Processes one RF frame for a slot. */

bool Control::processFrame(uint32_t slotNo, uint8_t* data, uint32_t length)
{
    switch (slotNo) {
    case 0U:
        return m_slot1->processFrame(data, length);
    case 1U:
        return m_slot2->processFrame(data, length);
    default:
        LogError(LOG_P25, "P25 Phase 2, invalid RF slot, slotNo = %u", slotNo + 1U);
        return false;
    }
}

/* Returns the next queued frame length for a slot. */

uint32_t Control::peekFrameLength(uint32_t slotNo)
{
    switch (slotNo) {
    case 0U:
        return m_slot1->peekFrameLength();
    case 1U:
        return m_slot2->peekFrameLength();
    default:
        LogError(LOG_P25, "P25 Phase 2, invalid queue slot, slotNo = %u", slotNo + 1U);
        return 0U;
    }
}

/* Returns whether a slot transmit queue is full. */

bool Control::isQueueFull(uint32_t slotNo)
{
    switch (slotNo) {
    case 0U:
        return m_slot1->isQueueFull();
    case 1U:
        return m_slot2->isQueueFull();
    default:
        LogError(LOG_P25, "P25 Phase 2, invalid queue slot, slotNo = %u", slotNo + 1U);
        return true;
    }
}

/* Retrieves the next queued frame for a slot. */

uint32_t Control::getFrame(uint32_t slotNo, uint8_t* data, bool* imm)
{
    switch (slotNo) {
    case 0U:
        return m_slot1->getFrame(data, imm);
    case 1U:
        return m_slot2->getFrame(data, imm);
    default:
        LogError(LOG_P25, "P25 Phase 2, invalid queue slot, slotNo = %u", slotNo + 1U);
        return 0U;
    }
}

/* Advances controller and slot timers. */

void Control::clock(uint32_t ms)
{
    if (m_network != nullptr)
        processNetwork();

    m_slot1->clock(ms);
    m_slot2->clock(ms);
}

/* Returns a Phase 2 slot. */

Slot& Control::slot(uint32_t slotNo)
{
    assert(slotNo < SLOT_COUNT);
    return slotNo == 0U ? *m_slot1 : *m_slot2;
}

/* Returns a constant Phase 2 slot. */

const Slot& Control::slot(uint32_t slotNo) const
{
    assert(slotNo < SLOT_COUNT);
    return slotNo == 0U ? *m_slot1 : *m_slot2;
}

/* Returns the RF state for a slot. */

RPT_RF_STATE Control::getRFState(uint32_t slotNo) const
{
    if (slotNo >= SLOT_COUNT) {
        LogError(LOG_P25, "P25 Phase 2, invalid RF state slot, slotNo = %u", slotNo + 1U);
        return RS_RF_INVALID;
    }
    return slot(slotNo).getRFState();
}

/* Returns the network state for a slot. */

RPT_NET_STATE Control::getNetState(uint32_t slotNo) const
{
    if (slotNo >= SLOT_COUNT) {
        LogError(LOG_P25, "P25 Phase 2, invalid network state slot, slotNo = %u", slotNo + 1U);
        return RS_NET_IDLE;
    }
    return slot(slotNo).getNetState();
}

/* Clears a rejected RF state. */

void Control::clearRFReject(uint32_t slotNo)
{
    if (slotNo >= SLOT_COUNT) {
        LogError(LOG_P25, "P25 Phase 2, invalid RF reject slot, slotNo = %u", slotNo + 1U);
        return;
    }
    slot(slotNo).clearRFReject();
}

/* Returns whether either slot is busy. */

bool Control::isBusy() const
{
    return m_slot1->isBusy() || m_slot2->isBusy();
}

/* Permits a destination on a non-authoritative slot. */

void Control::permittedTG(uint32_t dstId, uint32_t slotNo)
{
    if (slotNo >= SLOT_COUNT) {
        LogError(LOG_P25, "P25 Phase 2, invalid permit slot, slotNo = %u", slotNo + 1U);
        return;
    }
    slot(slotNo).permittedTG(dstId);
}

/* Touches a slot grant. */

void Control::touchGrantTG(uint32_t dstId, uint32_t slotNo)
{
    if (slotNo >= SLOT_COUNT) {
        LogError(LOG_P25, "P25 Phase 2, invalid grant touch slot, slotNo = %u", slotNo + 1U);
        return;
    }
    slot(slotNo).touchGrantTG(dstId);
}

/* Releases a slot grant. */

void Control::releaseGrantTG(uint32_t dstId, uint32_t slotNo)
{
    if (slotNo >= SLOT_COUNT) {
        LogError(LOG_P25, "P25 Phase 2, invalid grant release slot, slotNo = %u", slotNo + 1U);
        return;
    }
    slot(slotNo).releaseGrantTG(dstId);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Processes the next network Phase 2 frame. */

void Control::processNetwork()
{
    if (m_network == nullptr || !m_network->hasP25P2Data())
        return;

    bool ret = false;
    uint32_t length = 0U;
    UInt8Array buffer = m_network->readP25P2(ret, length);
    if (!ret || buffer == nullptr) {
        if (m_debug)
            LogDebugEx(LOG_NET, "Control::processNetwork()", "P25 Phase 2 network read returned no frame");
        return;
    }
    if (length < (24U + P25DEF::P25_P2_FRAME_LENGTH_BYTES)) {
        LogWarning(LOG_NET, "P25 Phase 2, malformed network frame, length = %u, minimum = %u", length, 24U + P25DEF::P25_P2_FRAME_LENGTH_BYTES);
        return;
    }

    // The network header carries the slot flag and DUID in byte 19. The logical
    // 40-byte burst begins at byte 24; physical TDMA scheduling is modem-owned.
    uint32_t slotNo = (buffer[19U] & 0x80U) != 0U ? 1U : 0U;

    lc::LC control;
    control.setMACPDUOpcode(buffer[4U]);

    uint32_t srcId = (buffer[5U] << 16) | (buffer[6U] << 8) | buffer[7U];
    uint32_t dstId = (buffer[8U] << 16) | (buffer[9U] << 8) | buffer[10U];
    control.setSrcId(srcId);
    control.setDstId(dstId);

    control.setMFId(buffer[15U]);

    const uint16_t scramblerOffset = GET_UINT16(buffer.get(), 20U);
    control.setP2ScrambleOffset(scramblerOffset);

    // forward onto the specific slot for final processing and delivery
    bool processed = false;
    switch (slotNo) {
    case 0U:
        processed = m_slot1->processNetwork(buffer.get() + 24U, P25DEF::P25_P2_FRAME_LENGTH_BYTES, control, 
            (P25DEF::P2_DUID::E)(buffer[19U] & 0x7FU), scramblerOffset, buffer[14U]);
        break;
    case 1U:
        processed = m_slot2->processNetwork(buffer.get() + 24U, P25DEF::P25_P2_FRAME_LENGTH_BYTES, control, 
            (P25DEF::P2_DUID::E)(buffer[19U] & 0x7FU), scramblerOffset, buffer[14U]);
        break;
    default:
        break;
    }

    if (!processed && m_debug)
        LogDebugEx(LOG_NET, "Control::processNetwork()", "P25 Phase 2 Slot %u, network frame rejected, srcId = %u, dstId = %u, duid = $%02X",
            slotNo + 1U, srcId, dstId, buffer[19U] & 0x7FU);
}

/* Writes an RF frame to the Phase 2 network stream. */

bool Control::writeNetwork(Slot* slot, const uint8_t* data, uint32_t length)
{
    if (m_network == nullptr || slot == nullptr || data == nullptr)
        return true;

    const bool ret = m_network->writeP25P2(slot->m_control, slot->m_duid, (uint8_t)slot->m_slotNo, slot->m_rfScrambleOffset, data, slot->m_controlByte);
    if (!ret)
        LogWarning(LOG_NET, "P25 Phase 2 Slot %u, failed to write network frame, duid = $%02X", slot->m_slotNo + 1U, (uint8_t)slot->m_duid);
    return ret;
}
