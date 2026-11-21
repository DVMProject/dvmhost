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
#include "common/p25/P25Defines.h"
#include "common/Log.h"
#include "p25/packet/phase2/Voice.h"
#include "p25/phase2/Control.h"
#include "p25/phase2/Slot.h"

#include <cstring>

using namespace p25::phase2;
using namespace p25::defines;
using namespace p25::packet;
using namespace p25::packet::phase2;


// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Resets RF voice counters. */

void Voice::resetRF()
{
    m_rfFrames = 0U;
    m_rfESSBCount = 0U;
    ::memset(m_rfESS, 0x00U, sizeof(m_rfESS));
    ::memset(m_rfESSB, 0x00U, sizeof(m_rfESSB));
    m_rfESSA = false;
    m_rfESSComplete = false;
}

/* Resets network voice counters. */

void Voice::resetNet()
{
    m_netFrames = 0U;
    m_netESSBCount = 0U;
    ::memset(m_netESS, 0x00U, sizeof(m_netESS));
    ::memset(m_netESSB, 0x00U, sizeof(m_netESSB));
    m_netESSA = false;
    m_netESSComplete = false;
}

/* Process a data frame from the RF interface. */

bool Voice::process(uint8_t* data, uint32_t length)
{
    if (!validFrame(data, length) || m_slot == nullptr) {
        if (m_debug)
            LogDebugEx(LOG_P25, "Voice::process()", "invalid RF voice frame, length = %u", length);
        return false;
    }

    defines::P2_DUID::E duid = m_slot->m_duid;
    if (duid != defines::P2_DUID::VTCH_4V && duid != defines::P2_DUID::VTCH_2V) {
        if (m_debug)
            LogDebugEx(LOG_P25, "Voice::process()", "slot = %u, unexpected voice DUID = $%02X", m_slot->m_slotNo + 1U, (uint8_t)duid);
        return false;
    }

    uint8_t rfFrame[P25_P2_FRAME_LENGTH_BYTES];
    ::memcpy(rfFrame, data, P25_P2_FRAME_LENGTH_BYTES);

    const bool fourVoice = duid == defines::P2_DUID::VTCH_4V;
    const uint32_t errors = m_ambe2FEC.regenerateBurst(rfFrame, true, fourVoice);
    if (duid == defines::P2_DUID::VTCH_4V) {
        processESS4V(rfFrame, m_rfESSBCount % 4U, false);
        ++m_rfESSBCount;
    }
    else
        processESS2V(rfFrame, false);

    if (!m_slot->m_rfTimeout && !m_slot->addFrame(rfFrame, length, duid))
        return false;
    if (!m_slot->m_rfTimeout && m_slot->s_control != nullptr)
        m_slot->s_control->writeNetwork(m_slot, rfFrame, length);

    if (m_debug)
        LogDebugEx(LOG_P25, "Voice::process()", "slot = %u, RF frame = %u, duid = $%02X",
            m_slot->m_slotNo, m_rfFrames, (uint8_t)m_slot->m_duid);

    if (m_verbose)
        LogInfoEx(LOG_RF, "P25 Phase 2 Slot %u, RF voice frame = %u, srcId = %u, dstId = %u, errs = %u/%u (%.1f%%)",
            m_slot->m_slotNo + 1U, m_rfFrames, m_slot->m_control.getSrcId(),
            m_slot->m_control.getDstId(), errors, fourVoice ? 288U : 144U,
            float(errors * 100U) / float(fourVoice ? 288U : 144U));

    m_rfFrames++;
    ++m_slot->m_rfFrames;
    m_slot->m_rfBits += fourVoice ? 288U : 144U;
    m_slot->m_rfErrs += errors;
    return true;
}

/* Process a data frame from the network. */

bool Voice::processNetwork(uint8_t* data, uint32_t length)
{
    if (!validFrame(data, length) || m_slot == nullptr) {
        if (m_debug)
            LogDebugEx(LOG_P25, "Voice::processNetwork()", "invalid network voice frame, length = %u", length);
        return false;
    }

    defines::P2_DUID::E duid = m_slot->m_duid;
    if (duid != defines::P2_DUID::VTCH_4V && duid != defines::P2_DUID::VTCH_2V) {
        if (m_debug)
            LogDebugEx(LOG_P25, "Voice::processNetwork()", "slot = %u, unexpected voice DUID = $%02X", m_slot->m_slotNo + 1U, (uint8_t)duid);
        return false;
    }

    uint8_t rfFrame[P25_P2_FRAME_LENGTH_BYTES];
    ::memcpy(rfFrame, data, P25_P2_FRAME_LENGTH_BYTES);

    const bool fourVoice = duid == defines::P2_DUID::VTCH_4V;
    const uint32_t errors = m_ambe2FEC.regenerateBurst(rfFrame, false, fourVoice);
    if (duid == defines::P2_DUID::VTCH_4V) {
        processESS4V(rfFrame, m_netESSBCount % 4U, true);
        ++m_netESSBCount;
    }
    else
        processESS2V(rfFrame, true);

    if (m_verbose)
        LogInfoEx(LOG_NET, "P25 Phase 2 Slot %u, network voice frame = %u, srcId = %u, dstId = %u, errs = %u/%u (%.1f%%)",
            m_slot->m_slotNo + 1U, m_netFrames, m_slot->m_control.getSrcId(),
            m_slot->m_control.getDstId(), errors, fourVoice ? 288U : 144U,
            float(errors * 100U) / float(fourVoice ? 288U : 144U));

    if (!m_slot->m_netTimeout && !m_slot->addFrame(rfFrame, length, duid, true))
        return false;

    m_netFrames++;
    ++m_slot->m_netFrames;
    m_slot->m_netBits += fourVoice ? 288U : 144U;
    m_slot->m_netErrs += errors;
    return true;
}

/* Builds and queues an outbound SACCH voice-channel-user PDU. */

bool Voice::writeVoiceLC(uint8_t opcode, bool imm)
{
    if (m_slot == nullptr ||
        (opcode != P2_MAC_HEADER_OPCODE::ACTIVE && opcode != P2_MAC_HEADER_OPCODE::HANGTIME))
        return false;

    lc::LC control(m_slot->m_control);
    control.setMACPDUOpcode(opcode);
    control.setMACPartition(P2_MAC_MCO_PARTITION::UNIQUE);
    control.setP2DUID((uint8_t)P2_DUID::SACCH_UNSCRAMBLED);
    control.setP2ScrambleOffset(m_slot->m_netScrambleOffset);

    uint8_t burst[P25_P2_FRAME_LENGTH_BYTES] = { 0U };
    control.encodeVCH_MACPDU(burst, false);
    return m_slot->addFrame(burst, sizeof(burst), P2_DUID::SACCH_UNSCRAMBLED, true, imm);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Initializes a Phase 2 voice processor. */

Voice::Voice(Slot* slot, bool debug, bool verbose) :
    m_slot(slot),
    m_rfFrames(0U),
    m_netFrames(0U),
    m_rfESSBCount(0U),
    m_netESSBCount(0U),
    m_rfESSA(false),
    m_rfESSComplete(false),
    m_netESSA(false),
    m_netESSComplete(false),
    m_debug(debug),
    m_verbose(verbose)
{
    ::memset(m_rfESS, 0x00U, sizeof(m_rfESS));
    ::memset(m_rfESSB, 0x00U, sizeof(m_rfESSB));
    ::memset(m_netESS, 0x00U, sizeof(m_netESS));
    ::memset(m_netESSB, 0x00U, sizeof(m_netESSB));
}

/* Validates a host-side Phase 2 burst. */

bool Voice::validFrame(const uint8_t* data, uint32_t length) const
{
    return data != nullptr && length == P25_P2_FRAME_LENGTH_BYTES;
}

/* Collects one ESS-B fragment from a 4V voice burst. */

bool Voice::processESS4V(const uint8_t* burst, uint32_t burstNo, bool net)
{
    if (burst == nullptr || burstNo >= 4U)
        return false;

    uint8_t* ess = net ? m_netESS : m_rfESS;
    bool* essB = net ? m_netESSB : m_rfESSB;
    for (uint32_t bit = 0U; bit < 24U; bit++)
        WRITE_BIT(ess, burstNo * 24U + bit, READ_BIT(burst, 148U + bit));

    essB[burstNo] = true;

    return decodeESS(net);
}

/* Collects ESS-A from a 2V voice burst. */

bool Voice::processESS2V(const uint8_t* burst, bool net)
{
    if (burst == nullptr)
        return false;

    uint8_t* ess = net ? m_netESS : m_rfESS;
    for (uint32_t bit = 0U; bit < 168U; bit++)
        WRITE_BIT(ess, 96U + bit, READ_BIT(burst, 148U + bit));

    if (net)
        m_netESSA = true;
    else
        m_rfESSA = true;

    return decodeESS(net);
}

/* Decodes a complete ESS codeword. */

bool Voice::decodeESS(bool net)
{
    bool essA = net ? m_netESSA : m_rfESSA;
    bool* essB = net ? m_netESSB : m_rfESSB;
    if (!essA || !essB[0U] || !essB[1U] || !essB[2U] || !essB[3U])
        return false;

    uint8_t* ess = net ? m_netESS : m_rfESS;
    int8_t errors = -1;
    const bool wasComplete = net ? m_netESSComplete : m_rfESSComplete;

    bool complete = m_essRS.decode441629(ess, &errors);
    if (complete && !wasComplete && m_verbose) {
        LogInfoEx(net ? LOG_NET : LOG_RF, "P25 Phase 2 Slot %u, %s ESS, algId = $%02X, keyId = $%04X, corrected = %d",
            m_slot->m_slotNo + 1U, net ? "network" : "RF", ess[0U], (uint16_t)((ess[1U] << 8U) | ess[2U]), errors);
    }
    else if (!complete && m_debug) {
        LogDebugEx(LOG_P25, "Voice::decodeESS()", "P25 Phase 2 Slot %u, %s ESS decode failed", m_slot->m_slotNo + 1U, 
            net ? "network" : "RF");
    }

    if (net)
        m_netESSComplete = complete;
    else
        m_rfESSComplete = complete;

    return complete;
}
