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
#include "common/p25/lc/mac/MACFactory.h"
#include "common/p25/acl/AccessControl.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "p25/phase2/Slot.h"
#include "p25/phase2/Control.h"
#include "ActivityLog.h"

using namespace p25::phase2;
using namespace p25::defines;

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

// Enforces authoritative ownership and RF RID/TGID policy before forwarding.
#define CHECK_AUTHORITATIVE(_SRC_ID, _DST_ID, _GROUP)                                   \
    do {                                                                                \
        if (!validateRFCall((_SRC_ID), (_DST_ID), (_GROUP)))                            \
            return false;                                                               \
    } while (0)

// Enforces the per-slot permit received from an authoritative P25 CC.
#define CHECK_NET_AUTHORITATIVE(_DST_ID)                                                \
    do {                                                                                \
        if (!s_authoritative && m_permittedDstId != (_DST_ID)) {                        \
            if (m_debug)                                                                \
                LogDebugEx(LOG_NET, "Slot::processNetwork()",                         \
                    "P25 Phase 2 Slot %u, network destination not permitted, dstId = %u", \
                    m_slotNo + 1U, (_DST_ID));                                          \
            ++m_netMissed;                                                              \
            return false;                                                               \
        }                                                                               \
    } while (0)

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

::p25::phase2::Control* Slot::s_control = nullptr;

bool Slot::s_authoritative = true;

uint32_t Slot::s_callHang = 0U;
uint16_t Slot::s_colorCode = 0U;

modem::Modem* Slot::s_modem = nullptr;
network::Network* Slot::s_network = nullptr;

::p25::lookups::P25AffiliationLookup* Slot::s_affiliations = nullptr;
::lookups::RadioIdLookup* Slot::s_ridLookup = nullptr;
::lookups::TalkgroupRulesLookup* Slot::s_tidLookup = nullptr;


// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a Phase 2 slot and its packet processors. */

Slot::Slot(uint32_t slotNo, uint32_t timeout, uint32_t tgHang, uint32_t queueSize, bool debug, bool verbose) :
    m_voice(this, debug, verbose),
    m_slotNo(slotNo),
    m_txQueue(queueSize, "P25 Phase 2 Slot Frame"),
    m_txImmQueue(queueSize, "P25 Phase 2 Slot Immediate Frame"),
    m_rfState(RS_RF_LISTENING),
    m_rfLastDstId(0U),
    m_rfLastSrcId(0U),
    m_netState(RS_NET_IDLE),
    m_netLastDstId(0U),
    m_netLastSrcId(0U),
    m_permittedDstId(0U),
    m_control(),
    m_rfResetPending(false),
    m_netResetPending(false),
    m_rfVCHState(VCH_STATE::IDLE),
    m_netVCHState(VCH_STATE::IDLE),
    m_rfPTTCount(0U),
    m_rfEndPTTCount(0U),
    m_netEndPTTCount(0U),
    m_rfFrames(0U),
    m_netFrames(0U),
    m_netLost(0U),
    m_netMissed(0U),
    m_rfBits(1U),
    m_netBits(1U),
    m_rfErrs(0U),
    m_netErrs(0U),
    m_rfTimeout(false),
    m_netTimeout(false),
    m_frameLossCnt(0U),
    m_frameLossThreshold(P2_DEFAULT_FRAME_LOSS_THRESHOLD),
    m_elapsedMs(0U),
    m_vcuElapsedMs(0U),
    m_rfTimeoutTimer(1000U, timeout),
    m_rfTGHang(1000U, tgHang),
    m_netTimeoutTimer(1000U, timeout),
    m_netTGHang(1000U, tgHang),
    m_networkWatchdog(1000U, 0U, 1500U),
    m_rfLossWatchdog(1000U, 0U, 1500U),
    m_rfCallHangTimer(1000U, s_callHang),
    m_netCallHangTimer(1000U, s_callHang),
    m_debug(debug),
    m_verbose(verbose),
    m_duid(defines::P2_DUID::VTCH_4V),
    m_controlByte(0U),
    m_rfBurstCount(0U),
    m_netBurstCount(0U),
    m_rfScrambleOffset(0U),
    m_netScrambleOffset(0U)
{
    /* stub */
}

/* Resets slot state, counters, and queued frames. */

void Slot::reset()
{
    // scope is intentional
    {
        std::lock_guard<std::mutex> lock(m_queueLock);
        m_txQueue.clear();
        m_txImmQueue.clear();
    }

    resetRF();
    resetNet();
    m_rfResetPending = false;
    m_netResetPending = false;
    m_elapsedMs = 0U;
    m_rfBurstCount = 0U;
    m_netBurstCount = 0U;
    m_rfScrambleOffset = 0U;
    m_netScrambleOffset = 0U;
    m_control = lc::LC();
    m_duid = defines::P2_DUID::VTCH_4V;
    m_controlByte = 0U;
}

/* Process a data frame from the RF interface. */

bool Slot::processFrame(uint8_t* data, uint32_t length)
{
    if (data == nullptr || length == 0U)
        return false;

    // Network traffic owns the physical TDMA slot until its termination has
    // drained. Do not allow an RF call to create two simultaneous owners.
    if (m_netState != RS_NET_IDLE && data[0U] != modem::TAG_LOST) {
        LogWarning(LOG_RF, "P25 Phase 2 Slot %u, RF traffic collision with active network call, srcId = %u, dstId = %u",
            m_slotNo + 1U, m_netLastSrcId, m_netLastDstId);
        return false;
    }

    if (m_rfState == RS_RF_REJECTED)
        return false;

    if (length == 1U && data[0U] == modem::TAG_LOST) {
        if (m_frameLossCnt > m_frameLossThreshold) {
            m_frameLossCnt = 0U;

            processFrameLoss(RF_LOSS_TYPE_EXCEEDED_FRAME_THRESHOLD);

            return false;
        }
        else {
            // increment the frame loss count by one for audio or data; otherwise drop
            // packets
            if (m_rfState == RS_RF_AUDIO) {
                m_rfLossWatchdog.start();
                ++m_frameLossCnt;
            }
            else {
                // drop normally
                m_frameLossCnt = 0U;
                m_rfState = RS_RF_LISTENING;

                m_rfLastDstId = 0U;
                m_rfLastSrcId = 0U;
                m_rfTGHang.stop();

                return false;
            }
        }
    }

    if (m_rfState == RS_RF_AUDIO || m_rfState == RS_RF_DATA) {
        // if RF TG hang is disabled, keep the loss watchdog alive from inbound
        // RF frames so abrupt stream loss can still recover state.
        if (m_rfTGHang.getTimeout() == 0U || m_rfLossWatchdog.isRunning()) {
            m_rfLossWatchdog.start();
        }
    }

    const uint8_t* burst = data;
    bool typedBurst = false;
    defines::P2_DUID::E duid = defines::P2_DUID::VTCH_4V;
    if (length == P25DEF::P25_P2_HOST_FRAME_LENGTH_BYTES) {
        if (data[0U] == modem::TAG_LOST) {
            processFrameLoss(RF_LOSS_TYPE_EXCEEDED_FRAME_THRESHOLD);
            return false;
        }

        if (data[0U] != modem::TAG_DATA && data[0U] != modem::TAG_EOT)
            return false;

        duid = static_cast<defines::P2_DUID::E>(data[1U] & 0x0FU);
        burst = data + 2U;
        length -= 2U;
        typedBurst = true;
    }

    if (length != P25DEF::P25_P2_FRAME_LENGTH_BYTES)
        return false;

    // TODO(P25P2-FW): The modem must own the authoritative 12-slot
    // superframe counter and scrambler phase. This host-side offset is a
    // compatibility aid until the typed modem scheduler is implemented.
    m_rfScrambleOffset = static_cast<uint16_t>(
        ((m_rfBurstCount * 2U + m_slotNo) * P25DEF::P25_P2_BURST_LENGTH_BITS) %
        P25DEF::P25_P2_SCRAMBLER_SEQUENCE_BITS);
    m_control.setP2ScrambleOffset(m_rfScrambleOffset);

    if (typedBurst) {
        // Modem supplies the DUID because the burst structure is needed to
        // locate and decode the on-air DUID itself. Inbound FACCH/SACCH are IEMI.
        if (!isVoiceDUID(duid) && !isLCCHDUID(duid) && !m_control.decodeVCH_MACPDU_IEMI(burst, isFACCHDUID(duid)))
            return false;
    } else {
        // Compatibility path for existing tests and injected logical voice.
        // TODO(P25P2-FW): Remove once all modem emits the typed envelope.
        if (!m_control.decodeVCH_MACPDU_IEMI(burst, false))
            return false;
        duid = static_cast<defines::P2_DUID::E>(m_control.getP2DUID());
    }

    m_duid = duid;

    // LCCH is an LCH designation, not VCH-associated signaling. It requires
    // IECI/OECI coding and a separate host control-channel processor.
    // TODO(P25P2-LCCH): Route LCCH bursts to a dedicated LCCH controller once
    // the firmware reports LCH designation and LCCH transition events.
    if (isLCCHDUID(duid))
        return false;
    if (!isVoiceDUID(duid) && !isFACCHDUID(duid) && !isSACCHDUID(duid))
        return false;

    const bool voice = isVoiceDUID(duid);
    bool release = false;
    if (voice) {
        if (m_rfState == RS_RF_LISTENING)
            m_rfState = RS_RF_LATE_ENTRY;
        if (!m_voice.process(const_cast<uint8_t*>(burst), length))
            return false;
    } else {
        if (!processMAC(const_cast<uint8_t*>(burst), length, duid, false, release))
            return false;

        // handle MAC PDU opcodes for FACCH bursts
        switch (m_control.getMACPDUOpcode()) {
        case defines::P2_MAC_HEADER_OPCODE::PTT:
            if (isFACCHDUID(duid)) {
                if (m_rfVCHState == VCH_STATE::IDLE) {
                    m_rfFrames = 0U;
                    m_rfBits = 1U;
                    m_rfErrs = 0U;

                    ::ActivityLog("P25P2", true, "Slot %u RF voice call from %u to %u",
                        m_slotNo + 1U, m_control.getSrcId(), m_control.getDstId());
                    LogInfoEx(LOG_RF, "P25 Phase 2 voice call start, slot = %u, srcId = %u, dstId = %u",
                        m_slotNo + 1U, m_control.getSrcId(), m_control.getDstId());
                }

                if (m_rfVCHState == VCH_STATE::HANGTIME) {
                    m_rfCallHangTimer.stop();
                    m_rfEndPTTCount = 0U;
                    m_rfResetPending = false;
                }

                m_rfPTTCount++;
                m_rfVCHState = VCH_STATE::PTT;
                m_vcuElapsedMs = 0U;
                queueMACPDU(defines::P2_MAC_HEADER_OPCODE::PTT, true);
            }
            break;
        case defines::P2_MAC_HEADER_OPCODE::ACTIVE:
            if (m_rfPTTCount > 0U) {
                m_rfVCHState = VCH_STATE::ACTIVE;
            }
            else if (m_rfState == RS_RF_LATE_ENTRY) {
                m_rfVCHState = VCH_STATE::ACTIVE;
                ::ActivityLog("P25P2", true, "Slot %u RF late entry from %u to %u",
                    m_slotNo + 1U, m_control.getSrcId(), m_control.getDstId());
            }
            break;
        case defines::P2_MAC_HEADER_OPCODE::END_PTT:
            if (isFACCHDUID(duid)) {
                const bool colorCodeMatches = m_control.getColorCode() == s_colorCode;
                const bool addressMatches = m_rfLastDstId == 0U || m_control.getDstId() == m_rfLastDstId;
                if (!colorCodeMatches || !addressMatches) {
                    if (m_debug) {
                        LogDebugEx(LOG_RF, "Slot::processFrame()",
                            "P25 Phase 2 Slot %u, ignoring END_PTT for another call, colorCode = $%03X, dstId = %u",
                            m_slotNo + 1U, m_control.getColorCode(), m_control.getDstId());
                    }
                    return true;
                }

                m_rfEndPTTCount++;
                if (m_rfEndPTTCount == 2U) {
                    ::ActivityLog("P25P2", true, "Slot %u RF voice call ended from %u to %u, %.1f seconds, BER: %.1f%%",
                        m_slotNo + 1U, m_rfLastSrcId, m_rfLastDstId, float(m_rfFrames) / 16.667F, float(m_rfErrs * 100U) / float(m_rfBits));
                    LogInfoEx(LOG_RF, "P25 Phase 2 voice call end, slot = %u, srcId = %u, dstId = %u, frames = %u, bits = %u, errors = %u, BER = %.4f%%",
                        m_slotNo + 1U, m_rfLastSrcId, m_rfLastDstId, m_rfFrames, m_rfBits, m_rfErrs, float(m_rfErrs * 100U) / float(m_rfBits));
                    beginHangtime(false);
                }
            }
            break;
        default:
            break;
        }
    }

    if (release)
        writeEnd(false);

    if (m_rfVCHState != VCH_STATE::IDLE)
        m_rfState = RS_RF_AUDIO;
    m_rfLastDstId = m_control.getDstId();
    m_rfLastSrcId = m_control.getSrcId();

    touchGrant(m_rfLastDstId);

    if (!m_rfTimeoutTimer.isRunning())
        m_rfTimeoutTimer.start();

    m_rfTGHang.start();
    m_rfLossWatchdog.start();
    m_elapsedMs = 0U;
    m_rfBurstCount++;
    return true;
}

/* Returns the next queued frame length. */

uint32_t Slot::peekFrameLength()
{
    std::lock_guard<std::mutex> lock(m_queueLock);

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

/* Returns whether the slot queue is full. */

bool Slot::isQueueFull() const
{
    if (m_txQueue.isEmpty() && m_txImmQueue.isEmpty())
        return false;

    // tx immediate queue takes priority
    if (!m_txImmQueue.isEmpty()) {
        uint32_t space = m_txImmQueue.freeSpace();
        if (space < (P25_P2_HOST_FRAME_LENGTH_BYTES + 1U))
            return true;
    }
    else {
        uint32_t space = m_txQueue.freeSpace();
        if (space < (P25_P2_HOST_FRAME_LENGTH_BYTES + 1U))
            return true;
    }

    return false;
}

/* Retrieves the next queued frame. */

uint32_t Slot::getFrame(uint8_t* data, bool* imm)
{
    if (data == nullptr)
        return 0U;

    uint8_t storedLength = 0U;
    bool shouldReset = false;
    {
        std::lock_guard<std::mutex> lock(m_queueLock);
        RingBuffer<uint8_t>& queue = !m_txImmQueue.isEmpty() ? m_txImmQueue : m_txQueue;
        if (queue.isEmpty())
            return 0U;

        queue.peek(&storedLength, 1U);

        if (queue.dataSize() <= storedLength)
            return 0U;
        if (imm != nullptr)
            *imm = !m_txImmQueue.isEmpty();

        queue.get(&storedLength, 1U);
        queue.get(data, storedLength);
        shouldReset = (m_rfResetPending || m_netResetPending) &&
            m_txQueue.isEmpty() && m_txImmQueue.isEmpty();
    }

    if (shouldReset) {
        if (m_rfResetPending) {
            m_rfResetPending = false;
            resetRF();
        }
        if (m_netResetPending) {
            m_netResetPending = false;
            resetNet();
        }
    }

    return storedLength;
}

/* Processes one decoded network frame. */

bool Slot::processNetwork(uint8_t* data, uint32_t length, const lc::LC& control,
    defines::P2_DUID::E duid, uint16_t scramblerOffset, uint8_t controlByte)
{
    CHECK_NET_AUTHORITATIVE(control.getDstId());

    if (m_rfState != RS_RF_LISTENING && m_netState == RS_NET_IDLE) {
        ++m_netMissed;
        if (m_debug)
            LogDebugEx(LOG_NET, "Slot::processNetwork()", "P25 Phase 2 Slot %u, dropping network burst while RF owns slot, srcId = %u, dstId = %u",
                m_slotNo + 1U, control.getSrcId(), control.getDstId());
        return false;
    }

    if (m_netLastDstId != 0U && control.getDstId() != 0U && m_netState != RS_NET_IDLE &&
        m_netLastDstId != control.getDstId() && m_netTGHang.isRunning() && !m_netTGHang.hasExpired()) {
        ++m_netMissed;
        if (m_debug)
            LogDebugEx(LOG_NET, "Slot::processNetwork()", "P25 Phase 2 Slot %u, dropping network burst during TG hang, lastDstId = %u, dstId = %u",
                m_slotNo + 1U, m_netLastDstId, control.getDstId());
        return false;
    }

    if (m_netLastDstId != 0U && m_netLastDstId == control.getDstId() &&
        m_netTGHang.isRunning() && !m_netTGHang.hasExpired())
        m_netTGHang.start();

    m_control = control;
    m_duid = duid;
    m_controlByte = controlByte;

    m_netScrambleOffset = scramblerOffset % P25DEF::P25_P2_SCRAMBLER_SEQUENCE_BITS;
    m_control.setP2ScrambleOffset(m_netScrambleOffset);

    if (isLCCHDUID(duid)) {
        // TODO(P25P2-LCCH): Network LCCH messages belong to a dedicated LCCH
        // controller and must be scheduled only on an LCH designated LCCH.
        return false;
    }
    if (!isVoiceDUID(duid) && !isFACCHDUID(duid) && !isSACCHDUID(duid))
        return false;

    const bool voice = isVoiceDUID(duid);
    bool release = false;
    if (voice) {
        if (!m_voice.processNetwork(data, length))
            return false;
    } else if (!processMAC(data, length, duid, true, release)) {
        return false;
    }

    m_networkWatchdog.start();

    if (!voice) {
        // handle MAC PDU opcodes for network voice calls
        switch (m_control.getMACPDUOpcode()) {
        case defines::P2_MAC_HEADER_OPCODE::PTT:
            if (m_netVCHState == VCH_STATE::IDLE) {
                m_netFrames = 0U;
                m_netLost = 0U;
                m_netMissed = 0U;
                m_netBits = 1U;
                m_netErrs = 0U;

                ::ActivityLog("P25P2", false, "Slot %u network voice call from %u to %u", m_slotNo + 1U, control.getSrcId(), control.getDstId());
                LogInfoEx(LOG_NET, "P25 Phase 2 voice call start, slot = %u, srcId = %u, dstId = %u", m_slotNo + 1U, control.getSrcId(), control.getDstId());
            }

            if (m_netVCHState == VCH_STATE::HANGTIME) {
                m_netCallHangTimer.stop();
                m_netEndPTTCount = 0U;
                m_netResetPending = false;
            }

            m_netVCHState = VCH_STATE::PTT;
            m_vcuElapsedMs = 0U;
            break;
        case defines::P2_MAC_HEADER_OPCODE::ACTIVE:
            m_netVCHState = VCH_STATE::ACTIVE;
            break;
        case defines::P2_MAC_HEADER_OPCODE::HANGTIME:
            m_netVCHState = VCH_STATE::HANGTIME;
            m_netCallHangTimer.start();
            break;
        case defines::P2_MAC_HEADER_OPCODE::END_PTT:
            m_netEndPTTCount++;
            if (m_netEndPTTCount == 2U) {
                ::ActivityLog("P25P2", false, "Slot %u network voice call ended from %u to %u, %.1f seconds, packet loss: %u, BER: %.1f%%",
                    m_slotNo + 1U, m_netLastSrcId, m_netLastDstId, float(m_netFrames) / 16.667F, m_netLost, float(m_netErrs * 100U) / float(m_netBits));
                LogInfoEx(LOG_NET, "P25 Phase 2 voice call end, slot = %u, srcId = %u, dstId = %u, frames = %u, lost = %u, missed = %u, bits = %u, errors = %u, BER = %.4f%%",
                    m_slotNo + 1U, m_netLastSrcId, m_netLastDstId, m_netFrames, m_netLost, m_netMissed, m_netBits, m_netErrs, float(m_netErrs * 100U) / float(m_netBits));

                m_netVCHState = VCH_STATE::TERMINATING;
                m_netResetPending = true;
            }
            break;
        default:
            break;
        }
    }

    if (release)
        writeEnd(true);

    m_netState = RS_NET_AUDIO;
    m_netLastDstId = control.getDstId();
    m_netLastSrcId = control.getSrcId();

    touchGrant(m_netLastDstId);

    if (!m_netTimeoutTimer.isRunning())
        m_netTimeoutTimer.start();

    m_netTGHang.start();
    m_elapsedMs = 0U;
    m_netBurstCount++;
    return true;
}

/* Advances slot timers. */

void Slot::clock(uint32_t ms)
{
    m_elapsedMs += ms;
    m_vcuElapsedMs += ms;

    if (m_frameLossCnt > 0U && m_rfState == RS_RF_LISTENING)
        m_frameLossCnt = 0U;
    if (m_frameLossCnt > 0U && m_frameLossCnt >= m_frameLossThreshold &&
        (m_rfState == RS_RF_AUDIO || m_rfState == RS_RF_DATA))
        processFrameLoss(RF_LOSS_TYPE_EXCEEDED_FRAME_THRESHOLD);

    // handle timeouts and hang timers
    m_rfTimeoutTimer.clock(ms);

    if (m_rfTimeoutTimer.isRunning() && m_rfTimeoutTimer.hasExpired()) {
        if (!m_rfTimeout) {
            LogInfoEx(LOG_RF, "P25 Phase 2 Slot %u, traffic timeout timer has expired, traffic will not transmit", m_slotNo + 1U);
            m_rfTimeout = true;
        }

        m_rfTimeoutTimer.stop();
        m_rfTimeout = true;

        writeEnd(false);
    }

    if (m_rfState == RS_RF_AUDIO || m_rfState == RS_RF_DATA) {
        if (m_rfLossWatchdog.isRunning()) {
            m_rfLossWatchdog.clock(ms);

            if (m_rfLossWatchdog.hasExpired()) {
                m_rfLossWatchdog.stop();

                processFrameLoss(RF_LOSS_TYPE_LOSS_WATCHDOG);
            }
        }
    }

    if (m_rfTGHang.isRunning()) {
        m_rfTGHang.clock(ms);

        if (m_rfTGHang.hasExpired()) {
            m_rfTGHang.stop();
            if (m_verbose) {
                LogInfoEx(LOG_RF, "P25 Phase 2 Slot %u, talkgroup hang has expired, lastDstId = %u", m_slotNo, m_rfLastDstId);
            }
            m_rfLastDstId = 0U;
            m_rfLastSrcId = 0U;

            // reset permitted ID and clear permission state
            if (!s_authoritative && m_permittedDstId != 0U) {
                m_permittedDstId = 0U;
            }

            // has the talkgroup hang timer expired while the modem is in a non-listening state?
            if (m_rfState != RS_RF_LISTENING) {
                processFrameLoss(RF_LOSS_TYPE_TG_HANG_NOT_LISTENING);
            }
            else {
                resetRF();
            }
        }
    }

    if (m_netTimeoutTimer.isRunning()) {
        m_netTimeoutTimer.clock(ms);
        if (m_netTimeoutTimer.hasExpired()) {
            m_netTimeoutTimer.stop();
            if (!m_netTimeout) {
                LogInfoEx(LOG_NET, "P25 Phase 2 Slot %u, traffic timeout timer has expired, traffic will not transmit", m_slotNo + 1U);
                m_netTimeout = true;
            }

            writeEnd(true);
        }
    }

    if (s_authoritative) {
        if (m_netTGHang.isRunning()) {
            m_netTGHang.clock(ms);

            if (m_netTGHang.hasExpired()) {
                m_netTGHang.stop();
                if (m_verbose) {
                    LogInfoEx(LOG_NET, "P25 Phase 2 Slot %u, talkgroup hang has expired, lastDstId = %u", m_slotNo, m_netLastDstId);
                }
                m_netLastDstId = 0U;
                m_netLastSrcId = 0U;

                resetNet();
            }
        }
    }
    else {
        m_netTGHang.stop();
    }

    if (m_netState == RS_NET_AUDIO || m_netState == RS_NET_DATA) {
        m_networkWatchdog.clock(ms);

        if (m_networkWatchdog.hasExpired()) {
            if (m_netState == RS_NET_AUDIO) {
                // We've received the voice header haven't we?
                ++m_netLost;
                ::ActivityLog("P25P2", false, "Slot %u network watchdog has expired, %.1f seconds, %u%% packet loss, BER: %.1f%%",
                    m_slotNo + 1U, float(m_netFrames) / 16.667F, m_netFrames > 0U ? (m_netLost * 100U) / m_netFrames : 100U, float(m_netErrs * 100U) / float(m_netBits));
                writeEnd(true);
            }
            else {
                ::ActivityLog("P25P2", false, "Slot %u network watchdog has expired", m_slotNo + 1U);
                writeEnd(true);
            }
        }
    }

    // During message-trunking hangtime, request one logical HANGTIME VCU per
    // nominal 360 ms superframe.
    // TODO(P25P2-FW): Drive this request from the modem's actual superframe
    // boundary and place it only in the LCH's physical SACCH opportunity.
    if (m_rfCallHangTimer.isRunning()) {
        m_rfCallHangTimer.clock(ms);
        if (m_elapsedMs >= 360U && m_rfVCHState == VCH_STATE::HANGTIME) {
            queueMACPDU(defines::P2_MAC_HEADER_OPCODE::HANGTIME);
            m_elapsedMs %= 360U;
        }

        if (m_rfCallHangTimer.hasExpired()) {
            m_rfCallHangTimer.stop();
            writeEnd(false);
        }
    }

    if (m_netCallHangTimer.isRunning()) {
        m_netCallHangTimer.clock(ms);
        if (m_elapsedMs >= 360U && m_netVCHState == VCH_STATE::HANGTIME) {
            queueMACPDU(defines::P2_MAC_HEADER_OPCODE::HANGTIME);
            m_elapsedMs %= 360U;
        }

        if (m_netCallHangTimer.hasExpired()) {
            m_netCallHangTimer.stop();
            writeEnd(true);
        }
    }

    // P2Voice owns VCU construction; the slot currently supplies policy cadence.
    // TODO(P25P2-FW): The modem scheduler must select the legal SACCH slot
    // and report the fourth-superframe talker SACCH opportunity to the host.
    if (m_vcuElapsedMs >= 360U && (m_rfVCHState == VCH_STATE::ACTIVE || m_netVCHState == VCH_STATE::ACTIVE)) {
        queueMACPDU(defines::P2_MAC_HEADER_OPCODE::ACTIVE);
        m_vcuElapsedMs %= 360U;
    }
}

/* Helper to initialize shared P25 Phase 2 slot configuration. */

void Slot::init(Control* control, bool authoritative, uint32_t callHang, modem::Modem* modem, network::Network* network,
    lookups::P25AffiliationLookup* affiliations, ::lookups::RadioIdLookup* ridLookup,
    ::lookups::TalkgroupRulesLookup* tidLookup, uint16_t colorCode)
{
    s_control = control;

    s_authoritative = authoritative;

    s_callHang = callHang;
    s_colorCode = colorCode & 0x0FFFU;

    s_modem = modem;
    s_network = network;

    s_affiliations = affiliations;
    s_ridLookup = ridLookup;
    s_tidLookup = tidLookup;

    if (s_ridLookup != nullptr && s_tidLookup != nullptr)
        acl::AccessControl::init(s_ridLookup, s_tidLookup);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Adds a frame to the slot queue. */

bool Slot::addFrame(const uint8_t* data, uint32_t length, defines::P2_DUID::E duid,
    bool net, bool imm)
{
    if (data == nullptr || length != P25_P2_FRAME_LENGTH_BYTES)
        return false;

    std::lock_guard<std::mutex> lock(m_queueLock);

    if (!net) {
        if (m_rfTimeoutTimer.isRunning() && m_rfTimeoutTimer.hasExpired())
            return false;
    } else {
        if (m_netTimeoutTimer.isRunning() && m_netTimeoutTimer.hasExpired())
            return false;
    }

    uint8_t len = P25_P2_HOST_FRAME_LENGTH_BYTES;
    if (m_debug) {
        Utils::symbols("!!! *Tx P25P2", data, len);
    }

    uint32_t fifoSpace = 0U;
    if (s_modem != nullptr)
        fifoSpace = m_slotNo == 0U ? s_modem->getP25P2Space1() : s_modem->getP25P2Space2();

    uint8_t tag = modem::TAG_DATA;
    uint8_t duidValue = static_cast<uint8_t>(duid);

    // is this immediate data?
    if (imm) {
        // resize immediate queue if necessary (this shouldn't really ever happen)
        uint32_t space = m_txImmQueue.freeSpace();
        if (space < (len + 1U)) {
            if (!net) {
                uint32_t queueLen = m_txImmQueue.length();
                m_txImmQueue.resize(queueLen + len);
                LogError(LOG_P25, "Slot %u, overflow in the imm Phase 2 slot queue; queue free is %u, needed %u; resized was %u is %u, fifoSpace = %u", m_slotNo, space, len, queueLen, m_txQueue.length(), fifoSpace);
                return false;
            }
            else {
                LogError(LOG_P25, "Slot %u, overflow in the imm Phase 2 slot queue while writing network data; queue free is %u, needed %u, fifoSpace = %u", m_slotNo, space, len, fifoSpace);
                return false;
            }
        }

        m_txImmQueue.addData(&len, 1U);
        m_txImmQueue.addData(&tag, 1U);
        m_txImmQueue.addData(&duidValue, 1U);
        m_txImmQueue.addData(data, length);
        return true;
    }

    uint32_t space = m_txQueue.freeSpace();
    if (space < (len + 1U)) {
        if (!net) {
            uint32_t queueLen = m_txQueue.length();
            m_txQueue.resize(queueLen + (len + 2U));
            LogError(LOG_P25, "Slot %u, overflow in the Phase 2 slot queue; queue free is %u, needed %u; resized was %u is %u, fifoSpace = %u", m_slotNo, space, len, queueLen, m_txQueue.length(), fifoSpace);
            return false;
        }
        else {
            LogError(LOG_P25, "Slot %u, overflow in the Phase 2 slot queue while writing network data; queue free is %u, needed %u, fifoSpace = %u", m_slotNo, space, len, fifoSpace);
            return false;
        }
    }

    m_txQueue.addData(&len, 1U);
    m_txQueue.addData(&tag, 1U);
    m_txQueue.addData(&duidValue, 1U);
    m_txQueue.addData(data, length);

    return true;
}

/* Resets RF state after a timeout or talkgroup hang. */

void Slot::resetRF()
{
    if (s_affiliations != nullptr && m_rfLastDstId != 0U && s_affiliations->isGranted(m_rfLastDstId))
        s_affiliations->releaseGrant(m_rfLastDstId, false);

    m_rfState = RS_RF_LISTENING;
    m_rfLastDstId = 0U;
    m_rfLastSrcId = 0U;

    m_rfTimeoutTimer.stop();
    m_rfTGHang.stop();
    m_rfLossWatchdog.stop();
    m_rfCallHangTimer.stop();

    m_rfVCHState = VCH_STATE::IDLE;

    m_rfPTTCount = 0U;
    m_rfEndPTTCount = 0U;
    m_frameLossCnt = 0U;
    m_rfTimeout = false;

    m_rfFrames = 0U;
    m_rfBits = 1U;
    m_rfErrs = 0U;

    m_vcuElapsedMs = 0U;

    m_voice.resetRF();
    m_rfResetPending = false;
}

/* Resets network state after a timeout, hang, or watchdog expiration. */

void Slot::resetNet()
{
    if (s_affiliations != nullptr && m_netLastDstId != 0U)
        s_affiliations->releaseGrant(m_netLastDstId, false);
    if (s_control != nullptr && s_control->m_network != nullptr)
        s_control->m_network->resetP25P2(m_slotNo);

    m_netState = RS_NET_IDLE;
    m_netLastDstId = 0U;
    m_netLastSrcId = 0U;

    m_netTimeoutTimer.stop();
    m_netTGHang.stop();
    m_networkWatchdog.stop();
    m_netCallHangTimer.stop();

    m_netVCHState = VCH_STATE::IDLE;
    m_netEndPTTCount = 0U;
    m_netTimeout = false;

    m_netFrames = 0U;
    m_netLost = 0U;
    m_netMissed = 0U;
    m_netBits = 1U;
    m_netErrs = 0U;

    m_vcuElapsedMs = 0U;

    m_voice.resetNet();
    m_netResetPending = false;
}

/* Permits a destination on a non-authoritative host. */

void Slot::permittedTG(uint32_t dstId)
{
    if (s_authoritative)
        return;

    if (m_verbose) {
        if (dstId == 0U)
            LogInfoEx(LOG_P25, "P25 Phase 2 Slot %u, non-authoritative TG unpermit", m_slotNo + 1U);
        else
            LogInfoEx(LOG_P25, "P25 Phase 2 Slot %u, non-authoritative TG permit, dstId = %u",
                m_slotNo + 1U, dstId);
    }

    if (m_permittedDstId != 0U && m_permittedDstId != dstId) {
        if (m_netState != RS_NET_IDLE && m_netLastDstId == m_permittedDstId)
            writeEnd(true);
        if (m_rfState != RS_RF_LISTENING && m_rfLastDstId == m_permittedDstId)
            writeEnd(false);
    }
    m_permittedDstId = dstId;
}

/* Touches a granted destination at the request of the CC. */

void Slot::touchGrantTG(uint32_t dstId)
{
    touchGrant(dstId);
}

/* Releases a granted destination at the request of the CC. */

void Slot::releaseGrantTG(uint32_t dstId)
{
    if (dstId == 0U || s_affiliations == nullptr || !s_affiliations->isGranted(dstId))
        return;

    if (m_verbose)
        LogInfoEx(LOG_P25, "P25 Phase 2 Slot %u, releasing grant, dstId = %u",
            m_slotNo + 1U, dstId);
    s_affiliations->releaseGrant(dstId, false);
}

/* Touches a grant associated with accepted traffic. */

void Slot::touchGrant(uint32_t dstId)
{
    if (dstId == 0U || s_affiliations == nullptr || !s_affiliations->isGranted(dstId))
        return;

    if (m_verbose)
        LogInfoEx(LOG_P25, "P25 Phase 2 Slot %u, call in progress, touching grant, dstId = %u",
            m_slotNo + 1U, dstId);
    s_affiliations->touchGrant(dstId);
}

/* Clears a rejected RF state. */

void Slot::clearRFReject()
{
    if (m_rfState != RS_RF_REJECTED)
        return;

    {
        std::lock_guard<std::mutex> lock(m_queueLock);
        m_txQueue.clear();
        m_txImmQueue.clear();
    }
    resetRF();
}

/* Validates RF call ownership and access-control policy. */

bool Slot::validateRFCall(uint32_t srcId, uint32_t dstId, bool group)
{
    if (!s_authoritative && m_permittedDstId != dstId) {
        LogWarning(LOG_RF,
            "P25 Phase 2 Slot %u, [NON-AUTHORITATIVE] destination not permitted, dstId = %u",
            m_slotNo + 1U, dstId);
        {
            std::lock_guard<std::mutex> lock(m_queueLock);
            m_txQueue.clear();
            m_txImmQueue.clear();
        }
        m_rfState = RS_RF_LISTENING;
        return false;
    }

    if (m_netState != RS_NET_IDLE) {
        LogWarning(LOG_RF,
            "P25 Phase 2 Slot %u, RF traffic collision with active network call, srcId = %u, dstId = %u",
            m_slotNo + 1U, srcId, dstId);
        m_rfState = RS_RF_LISTENING;
        return false;
    }

    const bool aclEnabled = s_ridLookup != nullptr && s_tidLookup != nullptr;
    const bool validSrc = !aclEnabled || acl::AccessControl::validateSrcId(srcId);
    const bool validDst = !aclEnabled || (group ? acl::AccessControl::validateTGId(dstId) :
        acl::AccessControl::validateSrcId(dstId));
    if (validSrc && validDst)
        return true;

    LogWarning(LOG_RF, "P25 Phase 2 Slot %u, RF voice rejection, srcId = %u, dstId = %u",
        m_slotNo + 1U, srcId, dstId);
    ::ActivityLog("P25P2", true, "Slot %u RF voice rejection from %u to %u",
        m_slotNo + 1U, srcId, dstId);
    m_rfLastDstId = 0U;
    m_rfLastSrcId = 0U;
    m_rfTGHang.stop();
    m_rfLossWatchdog.stop();
    {
        std::lock_guard<std::mutex> lock(m_queueLock);
        m_txQueue.clear();
        m_txImmQueue.clear();
    }
    m_rfState = RS_RF_REJECTED;
    return false;
}

/* Helper to process loss of frame stream from modem. */

void Slot::processFrameLoss(RPT_RF_LOSS_TYPE type)
{
    // resolve the type of RF frame loss into a human-readable string
    std::string typeStr;
    switch (type) {
    case RF_LOSS_TYPE_EXCEEDED_FRAME_THRESHOLD:
        typeStr = "exceeded frame loss threshold";
        break;
    case RF_LOSS_TYPE_LOSS_WATCHDOG:
        typeStr = "loss watchdog timeout";
        break;
    case RF_LOSS_TYPE_IN_CALL_CONTROL:
        typeStr = "in-call control request";
        break;
    case RF_LOSS_TYPE_TG_HANG_NOT_LISTENING:
        typeStr = "TG hang, RF not listening";
        break;
    default:
        typeStr = "no loss type set BUGBUG";
        break;
    }

    m_rfLossWatchdog.stop();
    if (m_rfState == RS_RF_AUDIO) {
        ::ActivityLog("P25P2", true, "Slot %u RF voice transmission lost, %s, %.1f seconds, BER: %.1f%%, loss count: %u",
            m_slotNo + 1U, typeStr.c_str(), float(m_rfFrames) / 16.667F, float(m_rfErrs * 100U) / float(m_rfBits), m_frameLossCnt);

        LogInfoEx(LOG_RF, "P25 Phase 2 Slot %u, transmission lost, %s, total frames: %d, total bits: %d, errors: %d, BER: %.4f%%",
            m_slotNo + 1U, typeStr.c_str(), m_rfFrames, m_rfBits, m_rfErrs, float(m_rfErrs * 100U) / float(m_rfBits));

            writeEnd(false);
    } else {
        resetRF();
    }

    m_rfState = RS_RF_LISTENING;

    m_rfLastDstId = 0U;
    m_rfTGHang.stop();
    m_rfLossWatchdog.stop();

    m_txQueue.clear();
}

/* Enters message-trunking hangtime. */

void Slot::beginHangtime(bool net)
{
    if (net)
        m_netVCHState = VCH_STATE::HANGTIME;
    else
        m_rfVCHState = VCH_STATE::HANGTIME;

    m_elapsedMs = 0U;

    queueMACPDU(defines::P2_MAC_HEADER_OPCODE::HANGTIME, true);

    if (s_callHang > 0U) {
        if (net)
            m_netCallHangTimer.start();
        else
            m_rfCallHangTimer.start();
    }
    else
        writeEnd(net);
}

/* Queues the two-burst FNE termination sequence described by TIA-102.BBAE 4.5.3. */

void Slot::writeEnd(bool net)
{
    VCH_STATE& state = net ? m_netVCHState : m_rfVCHState;
    if (state == VCH_STATE::TERMINATING)
        return;

    state = VCH_STATE::TERMINATING;
    if (net)
        m_netCallHangTimer.stop();
    else
        m_rfCallHangTimer.stop();
    if (net) {
        m_networkWatchdog.stop();
        m_netTimeoutTimer.stop();
        m_netTimeout = false;
    } else {
        m_rfLossWatchdog.stop();
        m_rfTimeoutTimer.stop();

        m_rfTimeout = false;
    }

    queueMACPDU(defines::P2_MAC_HEADER_OPCODE::END_PTT, true);
    queueMACPDU(defines::P2_MAC_HEADER_OPCODE::END_PTT, true);
    if (net)
        m_netResetPending = true;
    else
        m_rfResetPending = true;
}

/* Encodes one outbound logical FACCH PDU; physical slot pumping remains modem-owned. */

bool Slot::queueMACPDU(uint8_t opcode, bool imm)
{
    if (opcode == defines::P2_MAC_HEADER_OPCODE::ACTIVE ||
        opcode == defines::P2_MAC_HEADER_OPCODE::HANGTIME)
        return m_voice.writeVoiceLC(opcode, imm);

    lc::LC control(m_control);
    control.setMACPDUOpcode(opcode);
    control.setP2DUID(static_cast<uint8_t>(defines::P2_DUID::FACCH_UNSCRAMBLED));
    control.setP2ScrambleOffset(m_netScrambleOffset);

    if (opcode == defines::P2_MAC_HEADER_OPCODE::END_PTT)
        control.setSrcId(0xFFFFFFU);

    uint8_t burst[P25DEF::P25_P2_FRAME_LENGTH_BYTES] = { 0U };

    // Outbound FACCH uses the sync-bearing S-OEMI structure. The immediate
    // queue means "next legal FACCH opportunity", not the next physical slot.
    // TODO(P25P2-FW): Enforce that priority interpretation in modem.
    control.encodeVCH_MACPDU(burst, true);

    return addFrame(burst, sizeof(burst), defines::P2_DUID::FACCH_UNSCRAMBLED, true, imm);
}

/* Processes and forwards one logical MAC-bearing burst. */

bool Slot::processMAC(uint8_t* data, uint32_t length, defines::P2_DUID::E duid,
    bool net, bool& release)
{
    release = false;
    if (data == nullptr || length != P25DEF::P25_P2_FRAME_LENGTH_BYTES ||
        isVoiceDUID(duid) || isLCCHDUID(duid) || (!isFACCHDUID(duid) && !isSACCHDUID(duid)))
        return false;

    uint8_t frame[P25DEF::P25_P2_FRAME_LENGTH_BYTES];
    ::memcpy(frame, data, sizeof(frame));

    // RF MAC bursts were decoded during DUID dispatch. Network bursts carry
    // outbound encoding and must be decoded before message classification.
    // Outbound FACCH is S-OEMI (sync); outbound SACCH is I-OEMI (no sync).
    const bool sync = isFACCHDUID(duid);
    if (net && !m_control.decodeVCH_MACPDU_OEMI(frame, sync))
        return false;

    // The transport discriminator and the discriminator protected inside the
    // MAC burst must agree.  Do not let metadata reinterpret one coded form
    // as another after the modem has classified the logical channel.
    if ((P2_DUID::E)(m_control.getP2DUID() & 0x0FU) != duid)
        return false;

    m_control.setP2DUID(duid);
    std::unique_ptr<lc::MACPDU> mac;
    const uint8_t headerOpcode = m_control.getMACPDUOpcode();
    if (!net && (headerOpcode == defines::P2_MAC_HEADER_OPCODE::PTT ||
        (headerOpcode == defines::P2_MAC_HEADER_OPCODE::ACTIVE && m_rfState == RS_RF_LATE_ENTRY)))
        CHECK_AUTHORITATIVE(m_control.getSrcId(), m_control.getDstId(), m_control.getLCO() != defines::P2_MAC_MCO::PRIVATE);

    if (headerOpcode == defines::P2_MAC_HEADER_OPCODE::IDLE ||
        headerOpcode == defines::P2_MAC_HEADER_OPCODE::ACTIVE ||
        headerOpcode == defines::P2_MAC_HEADER_OPCODE::HANGTIME) {
        mac = lc::mac::MACFactory::createMACPDU(m_control);
        if (mac == nullptr && m_control.getLCO() != defines::P2_MAC_MCO::PDU_NULL)
            return false;
    }

    if (m_debug)
        LogDebugEx(LOG_P25, "Slot::processMAC()", "slot = %u, opcode = $%02X, MCO = $%02X, duid = $%02X", m_slotNo,
            headerOpcode, m_control.getLCO(), duid);

    if (mac != nullptr) {
        switch (mac->getOpcode()) {
        case defines::P2_MAC_MCO::MAC_RELEASE:
            if (m_verbose)
                LogInfoEx(net ? LOG_NET : LOG_RF, "P25 Phase 2 Slot %u, MAC release, srcId = %u, dstId = %u",
                    m_slotNo + 1U, m_control.getSrcId(), m_control.getDstId());
            release = true;
            break;
        case defines::P2_MAC_MCO::GROUP:
        case defines::P2_MAC_MCO::PRIVATE:
        case defines::P2_MAC_MCO::TEL_INT_VCH_USER:
            if (m_verbose)
                LogInfoEx(net ? LOG_NET : LOG_RF, "P25 Phase 2 Slot %u, MAC voice user, srcId = %u, dstId = %u",
                    m_slotNo + 1U, m_control.getSrcId(), m_control.getDstId());
            break;
        default:
            break;
        }
    }

    // Normalize the MAC PDU into the outbound logical form expected by the
    // modem's slot pump. The modem supplies the physical FACCH/SACCH timing.
    m_control.encodeVCH_MACPDU(frame, sync);
    if (net)
        return !m_netTimeout && addFrame(frame, sizeof(frame), duid, true);

    if (m_rfTimeout || !addFrame(frame, sizeof(frame), duid))
        return false;
    if (s_control != nullptr)
        s_control->writeNetwork(this, frame, sizeof(frame));
    return true;
}

/* Returns whether a DUID identifies a voice traffic burst. */

bool Slot::isVoiceDUID(defines::P2_DUID::E duid)
{
    return duid == defines::P2_DUID::VTCH_4V || duid == defines::P2_DUID::VTCH_2V;
}

/* Returns whether a DUID identifies a FACCH burst. */

bool Slot::isFACCHDUID(defines::P2_DUID::E duid)
{
    return duid == defines::P2_DUID::FACCH_SCRAMBLED || duid == defines::P2_DUID::FACCH_UNSCRAMBLED;
}

/* Returns whether a DUID identifies a SACCH burst. */

bool Slot::isSACCHDUID(defines::P2_DUID::E duid)
{
    return duid == defines::P2_DUID::SACCH_SCRAMBLED || duid == defines::P2_DUID::SACCH_UNSCRAMBLED;
}

/* Returns whether a DUID identifies an LCCH burst. */

bool Slot::isLCCHDUID(defines::P2_DUID::E duid)
{
    return duid == defines::P2_DUID::LCCH_SCRAMBLED || duid == defines::P2_DUID::LCCH_UNSCRAMBLED;
}
