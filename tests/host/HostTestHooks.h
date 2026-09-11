// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#if !defined(__HOST_TEST_ACCESS_H__)
#define __HOST_TEST_ACCESS_H__

#include "host/dmr/Control.h"
#include "host/dmr/Slot.h"
#include "host/nxdn/Control.h"
#include "host/p25/Control.h"
#include "host/p25/phase2/Control.h"
#include "host/p25/phase2/Slot.h"
#include "host/p25/packet/Voice.h"

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Provides test-only access to internal members of host protocol control classes.
 * This class centralizes friend-based accessors and utility helpers used by
 * Catch2 host tests for DMR, P25, and NXDN.
 */
class HostTestHooks {
public:
    /**
     * @name Digital Mobile Radio (DMR)
     * @brief DMR accessors and RF/network call setup helpers.
     */
    /**
     * @brief Gets slot 1 instance from the DMR control object.
     * @param control DMR control instance.
     * @return dmr::Slot* Pointer to slot 1.
     */
    static dmr::Slot* dmrSlot1(const dmr::Control& control);
    /**
     * @brief Gets slot 2 instance from the DMR control object.
     * @param control DMR control instance.
     * @return dmr::Slot* Pointer to slot 2.
     */
    static dmr::Slot* dmrSlot2(const dmr::Control& control);
    /**
     * @brief Gets the current network state for a DMR slot.
     * @param slot DMR slot instance.
     * @return RPT_NET_STATE Current network state.
     */
    static RPT_NET_STATE dmrNetState(const dmr::Slot& slot);
    /**
     * @brief Gets the current RF state for a DMR slot.
     * @param slot DMR slot instance.
     * @return RPT_RF_STATE Current RF state.
     */
    static RPT_RF_STATE dmrRFState(const dmr::Slot& slot);
    /**
     * @brief Gets the currently permitted destination ID for a DMR slot.
     * @param slot DMR slot instance.
     * @return uint32_t Permitted destination ID.
     */
    static uint32_t dmrPermittedDstId(const dmr::Slot& slot);
    /**
     * @brief Gets the DMR network watchdog timer for a slot.
     * @param slot DMR slot instance.
     * @return Timer& Reference to the network watchdog timer.
     */
    static Timer& dmrNetworkWatchdog(dmr::Slot& slot);
    /**
     * @brief Gets the DMR network talkgroup hang timer for a slot.
     * @param slot DMR slot instance.
     * @return Timer& Reference to the network TG hang timer.
     */
    static Timer& dmrNetTGHang(dmr::Slot& slot);
    /**
     * @brief Gets the DMR RF talkgroup hang timer for a slot.
     * @param slot DMR slot instance.
     * @return Timer& Reference to the RF TG hang timer.
     */
    static Timer& dmrRFTGHang(dmr::Slot& slot);
    /**
     * @brief Gets the DMR RF loss watchdog timer for a slot.
     * @param slot DMR slot instance.
     * @return Timer& Reference to the RF loss watchdog timer.
     */
    static Timer& dmrRFLossWatchdog(dmr::Slot& slot);
    /**
     * @brief Forces a DMR slot into active RF call state for tests.
     * @param slot DMR slot instance.
     * @param srcId Source ID to apply to slot state.
     * @param dstId Destination ID to apply to slot state.
     */
    static void dmrSetRFCall(dmr::Slot& slot, uint32_t srcId, uint32_t dstId);
    /**
     * @brief Returns a DMR slot to RF listening state after a synthetic RF call.
     * @param slot DMR slot instance.
     */
    static void dmrClearRFCall(dmr::Slot& slot);
    /**
     * @brief Forces a DMR slot into RF rejected state for tests.
     * @param slot DMR slot instance.
     */
    static void dmrSetRFRejected(dmr::Slot& slot);
    /**
     * @brief Injects a synthetic DMR network voice call into a slot.
     * @param slot DMR slot instance.
     * @param srcId Source ID for synthetic call.
     * @param dstId Destination ID for synthetic call.
     * @param group True for group call, false for private call.
     */
    static void dmrStartNetVoiceCall(dmr::Slot& slot, uint32_t srcId, uint32_t dstId, bool group = true);
    /**
     * @brief Injects a synthetic DMR RF voice call frame into a slot.
     * @param slot DMR slot instance.
     * @param srcId Source ID for synthetic call.
     * @param dstId Destination ID for synthetic call.
     * @param group True for group call, false for private call.
     * @return bool True if frame processing succeeds.
     */
    static bool dmrStartRFVoiceCall(dmr::Slot& slot, uint32_t srcId, uint32_t dstId, bool group = true);
    /** @} */

    /**
     * @name Project 25 (P25)
     * @brief P25 accessors and RF/network call setup helpers.
     */
    /**
     * @brief Gets the current network state for P25 control.
     * @param control P25 control instance.
     * @return RPT_NET_STATE Current network state.
     */
    static RPT_NET_STATE p25NetState(const p25::Control& control);
    /**
     * @brief Gets the current RF state for P25 control.
     * @param control P25 control instance.
     * @return RPT_RF_STATE Current RF state.
     */
    static RPT_RF_STATE p25RFState(const p25::Control& control);
    /**
     * @brief Gets the last P25 network destination ID.
     * @param control P25 control instance.
     * @return uint32_t Last network destination ID.
     */
    static uint32_t p25NetLastDstId(const p25::Control& control);
    /**
     * @brief Gets the last P25 network source ID.
     * @param control P25 control instance.
     * @return uint32_t Last network source ID.
     */
    static uint32_t p25NetLastSrcId(const p25::Control& control);
    /**
     * @brief Gets the currently permitted P25 destination ID.
     * @param control P25 control instance.
     * @return uint32_t Permitted destination ID.
     */
    static uint32_t p25PermittedDstId(const p25::Control& control);
    /**
     * @brief Gets whether P25 tail is currently active on idle transition.
     * @param control P25 control instance.
     * @return bool True if tail-on-idle is set.
     */
    static bool p25TailOnIdle(const p25::Control& control);
    /**
     * @brief Gets the P25 network watchdog timer.
     * @param control P25 control instance.
     * @return Timer& Reference to the network watchdog timer.
     */
    static Timer& p25NetworkWatchdog(p25::Control& control);
    /**
     * @brief Gets the P25 network talkgroup hang timer.
     * @param control P25 control instance.
     * @return Timer& Reference to the network TG hang timer.
     */
    static Timer& p25NetTGHang(p25::Control& control);
    /**
     * @brief Gets the P25 RF talkgroup hang timer.
     * @param control P25 control instance.
     * @return Timer& Reference to the RF TG hang timer.
     */
    static Timer& p25RFTGHang(p25::Control& control);
    /**
     * @brief Gets the P25 RF loss watchdog timer.
     * @param control P25 control instance.
     * @return Timer& Reference to the RF loss watchdog timer.
     */
    static Timer& p25RFLossWatchdog(p25::Control& control);
    /**
     * @brief Forces P25 control network state and last IDs for targeted recovery-path tests.
     * @param control P25 control instance.
     * @param netState Network state to set.
     * @param srcId Last network source ID to set.
     * @param dstId Last network destination ID to set.
     */
    static void p25SetNetState(p25::Control& control, RPT_NET_STATE netState, uint32_t srcId, uint32_t dstId);
    /**
     * @brief Forces P25 control into active RF call state for tests.
     * @param control P25 control instance.
     * @param srcId Source ID to apply to control state.
     * @param dstId Destination ID to apply to control state.
     */
    static void p25SetRFCall(p25::Control& control, uint32_t srcId, uint32_t dstId);
    /**
     * @brief Returns P25 control to the RF listening state after a synthetic RF call.
     * @param control P25 control instance.
     */
    static void p25ClearRFCall(p25::Control& control);
    /**
     * @brief Forces P25 control into RF rejected state for tests.
     * @param control P25 control instance.
     */
    static void p25SetRFRejected(p25::Control& control);
    /**
     * @brief Encodes a valid P25 NID for a given DUID into a test RF frame.
     * @param control P25 control instance.
     * @param frame RF frame buffer (tag and data) to update.
     * @param duid DUID to encode into frame NID bits.
     */
    static void p25StampRFFrameNID(p25::Control& control, uint8_t* frame, p25::defines::DUID::E duid);

    /** 
     * @brief Gets a P25 Phase 2 slot for tests. 
     * @param control P25 Phase 2 control instance.
     * @param slotNo Slot number to retrieve.
     * @return p25::phase2::Slot& Reference to the requested Phase 2 slot.
     */
    static p25::phase2::Slot& p25P2Slot(p25::phase2::Control& control, uint32_t slotNo);
    /** 
     * @brief Gets P25 Phase 2 RF state. 
     * @param slot P25 Phase 2 slot instance.
     * @return RPT_RF_STATE Current RF state of the slot.
     */
    static RPT_RF_STATE p25P2RFState(const p25::phase2::Slot& slot);
    /** 
     * @brief Forces P25 Phase 2 RF state for recovery-path tests. 
     * @param slot P25 Phase 2 slot instance.
     * @param state RF state to set for the slot.
     */
    static void p25P2SetRFState(p25::phase2::Slot& slot, RPT_RF_STATE state);
    /** 
     * @brief Gets P25 Phase 2 last source ID. 
     * @param slot P25 Phase 2 slot instance.
     * @return uint32_t Last source ID of the slot.
     */
    static uint32_t p25P2LastSrcId(const p25::phase2::Slot& slot);
    /** 
     * @brief Gets P25 Phase 2 last destination ID. 
     * @param slot P25 Phase 2 slot instance.
     * @return uint32_t Last destination ID of the slot.
     */
    static uint32_t p25P2LastDstId(const p25::phase2::Slot& slot);
    /** 
     * @brief Gets P25 Phase 2 RF scramble offset. 
     * @param slot P25 Phase 2 slot instance.
     * @return uint16_t RF scramble offset of the slot.
     */
    static uint16_t p25P2RFScrambleOffset(const p25::phase2::Slot& slot);
    /** 
     * @brief Gets P25 Phase 2 network scramble offset. 
     * @param slot P25 Phase 2 slot instance.
     * @return uint16_t Network scramble offset of the slot.
     */
    static uint16_t p25P2NetScrambleOffset(const p25::phase2::Slot& slot);
    /** 
     * @brief Gets whether P25 Phase 2 ESS is complete. 
     * @param slot P25 Phase 2 slot instance.
     * @return bool True if ESS is complete, false otherwise.
     */
    static bool p25P2ESSComplete(const p25::phase2::Slot& slot);
    /** 
     * @brief Gets P25 Phase 2 ESS algorithm ID. 
     * @param slot P25 Phase 2 slot instance.
     * @return uint8_t ESS algorithm ID of the slot.
     */
    static uint8_t p25P2ESSAlgId(const p25::phase2::Slot& slot);
    /** 
     * @brief Gets P25 Phase 2 ESS key ID. 
     * @param slot P25 Phase 2 slot instance.
     * @return uint16_t ESS key ID of the slot.
     */
    static uint16_t p25P2ESSKeyId(const p25::phase2::Slot& slot);
    /** 
     * @brief Copies P25 Phase 2 ESS MI. 
     * @param slot P25 Phase 2 slot instance.
     * @param mi Buffer to copy the ESS MI into.
     */
    static void p25P2ESSMI(const p25::phase2::Slot& slot, uint8_t* mi);
    /** 
     * @brief Gets the RF VCH lifecycle state. 
     * @param slot P25 Phase 2 slot instance.
     * @return p25::phase2::Slot::VCH_STATE RF VCH lifecycle state of the slot.
     */
    static p25::phase2::Slot::VCH_STATE p25P2RFVCHState(const p25::phase2::Slot& slot);
    /** 
     * @brief Gets the network VCH lifecycle state. 
     * @param slot P25 Phase 2 slot instance.
     * @return p25::phase2::Slot::VCH_STATE Network VCH lifecycle state of the slot.
     */
    static p25::phase2::Slot::VCH_STATE p25P2NetVCHState(const p25::phase2::Slot& slot);
    /** 
     * @brief Gets the decoded Phase 2 DUID. 
     * @param slot P25 Phase 2 slot instance.
     * @return p25::defines::P2_DUID::E Decoded Phase 2 DUID of the slot.
     */
    static p25::defines::P2_DUID::E p25P2DUID(const p25::phase2::Slot& slot);
    /** 
     * @brief Gets the decoded inbound FACCH PTT count. 
     * @param slot P25 Phase 2 slot instance.
     * @return uint8_t Decoded inbound FACCH PTT count of the slot.
     */
    static uint8_t p25P2RFPTTCount(const p25::phase2::Slot& slot);
    /**
     * @brief Gets the number of RF frames received.
     * @param slot P25 Phase 2 slot instance.
     * @return uint32_t Number of RF frames received by the slot.
     */
    static uint32_t p25P2RFFrames(const p25::phase2::Slot& slot);
    /**
     * @brief Gets the number of network frames received.
     * @param slot P25 Phase 2 slot instance.
     * @return uint32_t Number of network frames received by the slot.
     */
    static uint32_t p25P2NetFrames(const p25::phase2::Slot& slot);
    /**
     * @brief Gets the number of network frames lost.
     * @param slot P25 Phase 2 slot instance.
     * @return uint32_t Number of network frames lost by the slot.
     */
    static uint32_t p25P2NetLost(const p25::phase2::Slot& slot);
    /**
     * @brief Gets the number of network frames missed.
     * @param slot P25 Phase 2 slot instance.
     * @return uint32_t Number of network frames missed by the slot.
     */
    static uint32_t p25P2NetMissed(const p25::phase2::Slot& slot);
    /**
     * @brief Gets the number of RF bits received.
     * @param slot P25 Phase 2 slot instance.
     * @return uint32_t Number of RF bits received by the slot.
     */
    static uint32_t p25P2RFBits(const p25::phase2::Slot& slot);
    /**
     * @brief Gets the number of network bits received.
     * @param slot P25 Phase 2 slot instance.
     * @return uint32_t Number of network bits received by the slot.
     */
    static uint32_t p25P2NetBits(const p25::phase2::Slot& slot);
    /**
     * @brief Gets the number of RF errors.
     * @param slot P25 Phase 2 slot instance.
     * @return uint32_t Number of RF errors of the slot.
     */
    static uint32_t p25P2RFErrs(const p25::phase2::Slot& slot);
    /**
     * @brief Gets the number of network errors.
     * @param slot P25 Phase 2 slot instance.
     * @return uint32_t Number of network errors of the slot.
     */
    static uint32_t p25P2NetErrs(const p25::phase2::Slot& slot);

    /**
     * @brief Injects a synthetic P25 network call start via voice packet path.
     * @param control P25 control instance.
     * @param lc Link control data for synthetic call.
     * @param lsd Low speed data for synthetic call.
     */
    static void p25StartNetCall(p25::Control& control, const p25::lc::LC& lc, const p25::data::LowSpeedData& lsd);
    /**
     * @brief Injects synthetic P25 network call termination via voice packet path.
     * @param control P25 control instance.
     * @param lc Link control data for synthetic termination.
     * @param duid DUID to use for termination frame.
     * @return bool True if termination processing succeeds.
     */
    static bool p25TerminateNetCall(p25::Control& control, const p25::lc::LC& lc, p25::defines::DUID::E duid = p25::defines::DUID::TDU);
    /**
     * @brief Selects the next secondary active talkgroup for a P25 voice channel.
     */
    static bool p25NextActiveTalkgroups(p25::Control& control, const std::vector<uint32_t>& activeTG,
        uint8_t& groupUpdtIndex, const uint32_t& dstId, uint32_t& dstIdB, bool& hasDstIdB);
    /** @} */

    /**
     * @name NXDN
     * @brief NXDN accessors and RF/network call setup helpers.
     */
    /**
     * @brief Gets the current network state for NXDN control.
     * @param control NXDN control instance.
     * @return RPT_NET_STATE Current network state.
     */
    static RPT_NET_STATE nxdnNetState(const nxdn::Control& control);
    /**
     * @brief Gets the current RF state for NXDN control.
     * @param control NXDN control instance.
     * @return RPT_RF_STATE Current RF state.
     */
    static RPT_RF_STATE nxdnRFState(const nxdn::Control& control);
    /**
     * @brief Gets the last NXDN network destination ID.
     * @param control NXDN control instance.
     * @return uint32_t Last network destination ID.
     */
    static uint32_t nxdnNetLastDstId(const nxdn::Control& control);
    /**
     * @brief Gets the last NXDN network source ID.
     * @param control NXDN control instance.
     * @return uint32_t Last network source ID.
     */
    static uint32_t nxdnNetLastSrcId(const nxdn::Control& control);
    /**
     * @brief Gets the currently permitted NXDN destination ID.
     * @param control NXDN control instance.
     * @return uint32_t Permitted destination ID.
     */
    static uint32_t nxdnPermittedDstId(const nxdn::Control& control);
    /**
     * @brief Gets the NXDN network watchdog timer.
     * @param control NXDN control instance.
     * @return Timer& Reference to the network watchdog timer.
     */
    static Timer& nxdnNetworkWatchdog(nxdn::Control& control);
    /**
     * @brief Gets the NXDN network talkgroup hang timer.
     * @param control NXDN control instance.
     * @return Timer& Reference to the network TG hang timer.
     */
    static Timer& nxdnNetTGHang(nxdn::Control& control);
    /**
     * @brief Gets the NXDN RF talkgroup hang timer.
     * @param control NXDN control instance.
     * @return Timer& Reference to the RF TG hang timer.
     */
    static Timer& nxdnRFTGHang(nxdn::Control& control);
    /**
     * @brief Gets the NXDN RF loss watchdog timer.
     * @param control NXDN control instance.
     * @return Timer& Reference to the RF loss watchdog timer.
     */
    static Timer& nxdnRFLossWatchdog(nxdn::Control& control);
    /**
     * @brief Forces NXDN control into active RF call state for tests.
     * @param control NXDN control instance.
     * @param srcId Source ID to apply to control state.
     * @param dstId Destination ID to apply to control state.
     */
    static void nxdnSetRFCall(nxdn::Control& control, uint32_t srcId, uint32_t dstId);
    /**
     * @brief Returns NXDN control to RF listening state after a synthetic RF call.
     * @param control NXDN control instance.
     */
    static void nxdnClearRFCall(nxdn::Control& control);
    /**
     * @brief Forces NXDN control into RF rejected state for tests.
     * @param control NXDN control instance.
     */
    static void nxdnSetRFRejected(nxdn::Control& control);
    /**
     * @brief Injects a synthetic NXDN RF call start frame.
     * @param control NXDN control instance.
     * @param srcId Source ID for synthetic call.
     * @param dstId Destination ID for synthetic call.
     * @return bool True if frame processing succeeds.
     */
    static bool nxdnStartRFCall(nxdn::Control& control, uint32_t srcId, uint32_t dstId);
    /**
     * @brief Injects a synthetic NXDN network call start frame.
     * @param control NXDN control instance.
     * @param lc Link control data for synthetic call start.
     * @return bool True if network processing succeeds.
     */
    static bool nxdnStartNetCall(nxdn::Control& control, const nxdn::lc::RTCH& lc);
    /** @} */
};

#endif // __HOST_TEST_ACCESS_H__
