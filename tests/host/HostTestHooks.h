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
     * @brief Forces a DMR slot into residual RF talkgroup hang state for tests.
     * @param slot DMR slot instance.
     * @param srcId Source ID to retain as the last RF source.
     * @param dstId Destination ID to retain as the last RF destination.
     */
    static void dmrSetRFHang(dmr::Slot& slot, uint32_t srcId, uint32_t dstId);
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
     * @brief Injects an FNE-generated DMR voice grant response.
     * @param slot Slot receiving the network CSBK.
     * @param srcId Calling subscriber ID.
     * @param dstId Called subscriber or talkgroup ID.
     * @param group True for a talkgroup grant; false for a private grant.
     * @param emergency Emergency service flag carried by the grant.
     */
    static void dmrProcessNetworkGrant(dmr::Slot& slot, uint32_t srcId, uint32_t dstId,
        bool group, bool emergency = false);
    /**
     * @brief Injects a synthetic DMR RF voice call frame into a slot.
     * @param slot DMR slot instance.
     * @param srcId Source ID for synthetic call.
     * @param dstId Destination ID for synthetic call.
     * @param group True for group call, false for private call.
     * @return bool True if frame processing succeeds.
     */
    static bool dmrStartRFVoiceCall(dmr::Slot& slot, uint32_t srcId, uint32_t dstId, bool group = true);
    /**
     * @brief Detaches the shared DMR network pointer during test-fixture teardown.
     */
    static void dmrDetachNetwork();
    /**
     * @brief Configures one slot as a Tier III control channel for tests.
     * @param control DMR controller to configure.
     * @param channelNo Control-channel number.
     * @param tsccSlot Control-channel slot number.
     * @param dedicated True to configure a dedicated TSCC.
     */
    static void dmrEnableTier3(dmr::Control& control, uint32_t channelNo, uint8_t tsccSlot,
        bool dedicated = false);
    /**
     * @brief Gets the active Tier III payload destination for a slot.
     */
    static uint32_t dmrPayloadDstId(const dmr::Slot& slot);
    /**
     * @brief Tests whether the shared DMR affiliation table contains a grant.
     */
    static bool dmrIsGranted(const dmr::Slot& slot, uint32_t dstId);
    /**
     * @brief Tests whether a DMR grant is classified as group traffic.
     * @param slot DMR control-channel slot.
     * @param dstId Granted destination ID.
     * @return True when the grant is a group grant.
     */
    static bool dmrIsGroupGrant(const dmr::Slot& slot, uint32_t dstId);
    /**
     * @brief Registers a DMR subscriber on the local Tier III site.
     * @param slot DMR control-channel slot.
     * @param unitId Subscriber ID to register.
     */
    static void dmrRegisterUnit(dmr::Slot& slot, uint32_t unitId);
    /**
     * @brief Starts a synthetic DMR Tier III OACSU transaction.
     * @param slot DMR control-channel slot.
     * @param callerId Calling subscriber ID.
     * @param calledId Called subscriber ID.
     * @param serviceOptions Voice service options.
     * @return True when the transaction is accepted.
     */
    static bool dmrBeginU2UOACSU(dmr::Slot& slot, uint32_t callerId,
        uint32_t calledId, uint8_t serviceOptions = 0U);
    /**
     * @brief Returns the number of pending DMR OACSU transactions.
     * @param slot DMR control-channel slot.
     * @return Number of pending transactions.
     */
    static size_t dmrPendingU2UOACSU(const dmr::Slot& slot);
    /**
     * @brief Delivers a synthetic called-unit OACSU response.
     * @param slot DMR control-channel slot.
     * @param calledId Called subscriber ID.
     * @param callerId Calling subscriber ID.
     * @param accepted Whether the called unit accepted the request.
     * @return True when a matching transaction was processed.
     */
    static bool dmrRespondU2UOACSU(dmr::Slot& slot, uint32_t calledId,
        uint32_t callerId, bool accepted);
    /**
     * @brief Cancels a synthetic pending DMR OACSU transaction.
     * @param slot DMR control-channel slot.
     * @param callerId Calling subscriber ID.
     * @return True when a transaction was cancelled.
     */
    static bool dmrCancelU2UOACSU(dmr::Slot& slot, uint32_t callerId);
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
    static uint8_t p25NetAlgId(const p25::Control& control);
    static uint32_t p25NetKId(const p25::Control& control);
    static void p25NetMI(const p25::Control& control, uint8_t* mi);
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
     * @brief Configures P25 RF grant talkgroup steering for a test.
     * @param control P25 control instance.
     * @param enabled Flag indicating whether RF grant steering is enabled.
     * @param dedicated Flag indicating whether the control channel is dedicated.
     */
    static void p25ConfigureRFGrantSteering(p25::Control& control, bool enabled, bool dedicated);
    /**
     * @brief Configures whether a dedicated P25 control channel carries voice.
     * @param control P25 control instance.
     * @param enabled Flag indicating voice on the control channel is enabled.
     */
    static void p25SetVoiceOnControl(p25::Control& control, bool enabled);
    /**
     * @brief Runs P25 network-voice admission for a test call.
     * @param control P25 control instance.
     * @param srcId Source radio ID.
     * @param dstId Destination ID.
     * @returns bool True when the network voice frame must be discarded.
     */
    static bool p25CheckNetworkVoiceAdmission(p25::Control& control, uint32_t srcId, uint32_t dstId);
    /**
     * @brief Tests whether the P25 affiliation table contains a grant.
     * @param control P25 control instance.
     * @param dstId Destination ID to test.
     * @returns bool True if the destination is granted; otherwise, false.
     */
    static bool p25IsGranted(const p25::Control& control, uint32_t dstId);
    /**
     * @brief Seeds a P25 grant for a steering-policy test.
     * @param control P25 control instance.
     * @param srcId Source radio ID.
     * @param dstId Destination ID.
     * @param netGranted Flag indicating whether the grant originated from the network.
     * @returns bool True if the grant was created; otherwise, false.
     */
    static bool p25SeedGrant(p25::Control& control, uint32_t srcId, uint32_t dstId, bool netGranted);
    /**
     * @brief Seeds a P25 unit-to-unit grant for a call-procedure test.
     * @param control P25 control instance.
     * @param srcId Source radio ID.
     * @param dstId Destination radio ID.
     * @returns bool True if the grant was created; otherwise, false.
     */
    static bool p25SeedU2UGrant(p25::Control& control, uint32_t srcId, uint32_t dstId);
    /**
     * @brief Configures P25 unit-to-unit call-procedure policy for a test.
     * @param control P25 control instance.
     * @param availabilityCheck Flag indicating whether the availability check is enabled.
     */
    static void p25ConfigureU2U(p25::Control& control, bool availabilityCheck);
    /**
     * @brief Registers a P25 unit for a call-procedure test.
     * @param control P25 control instance.
     * @param srcId Radio ID to register.
     */
    static void p25RegisterUnit(p25::Control& control, uint32_t srcId);
    /**
     * @brief Injects an independently encoded raw TSBK into P25 control signaling.
     * @param control P25 control instance.
     * @param raw Raw 12-byte TSBK including CRC.
     * @returns bool True if the TSBK was accepted; otherwise, false.
     */
    static bool p25ProcessRawTSBK(p25::Control& control, const uint8_t* raw);
    /**
     * @brief Injects an independently encoded raw TSBK through P25 network processing.
     * @param control P25 control instance.
     * @param raw Raw 12-byte TSBK including CRC.
     * @returns bool True if the network TSBK was accepted; otherwise, false.
     */
    static bool p25ProcessNetworkRawTSBK(p25::Control& control, const uint8_t* raw);
    /**
     * @brief Emits a P25 deny response through the RF control-signaling path.
     * @param control P25 control instance.
     * @param srcId Source radio ID.
     * @param dstId Destination ID.
     */
    static void p25WriteDenyResponse(p25::Control& control, uint32_t srcId, uint32_t dstId);
    /**
     * @brief Tests whether one P25 private grant owns the specified RID pair.
     * @param control P25 control instance.
     * @param srcId One participant RID.
     * @param dstId The other participant RID.
     * @returns bool True if the pair resolves to an active private grant.
     */
    static bool p25IsU2UGranted(const p25::Control& control, uint32_t srcId, uint32_t dstId);
    /**
     * @brief Gets the current source RID advertised for a P25 private grant.
     * @param control P25 control instance.
     * @param srcId One private-call participant.
     * @param dstId The other private-call participant.
     * @returns uint32_t Current advertised source RID, or zero.
     */
    static uint32_t p25U2UGrantSource(const p25::Control& control, uint32_t srcId, uint32_t dstId);
    /**
     * @brief Gets the current target RID advertised for a P25 private grant.
     * @param control P25 control instance.
     * @param srcId One private-call participant.
     * @param dstId The other private-call participant.
     * @returns uint32_t Current advertised target RID, or zero.
     */
    static uint32_t p25U2UGrantTarget(const p25::Control& control, uint32_t srcId, uint32_t dstId);
    /**
     * @brief Gets the number of pending P25 U2U availability transactions.
     * @param control P25 control instance.
     * @returns size_t Number of pending transactions.
     */
    static size_t p25PendingU2UCount(const p25::Control& control);
    /**
     * @brief Advances P25 U2U availability transaction timers.
     * @param control P25 control instance.
     * @param ms Elapsed milliseconds.
     */
    static void p25ClockU2U(p25::Control& control, uint32_t ms);
    /**
     * @brief Stores an MFID transaction for P25 control-signaling tests.
     */
    static void p25StoreMFId(p25::Control& control, uint32_t srcId, uint32_t dstId,
        uint8_t lco, uint8_t mfId, uint32_t timeoutSeconds);
    /**
     * @brief Resolves an MFID transaction with a caller-provided fallback.
     */
    static uint8_t p25TransactionMFId(const p25::Control& control, uint32_t srcId,
        uint32_t dstId, uint8_t lco, uint8_t fallback);
    /**
     * @brief Tests whether an exact MFID transaction exists.
     */
    static bool p25HasMFIdTransaction(const p25::Control& control, uint32_t srcId,
        uint32_t dstId, uint8_t lco);
    /**
     * @brief Clears one exact MFID transaction.
     */
    static void p25ClearMFId(p25::Control& control, uint32_t srcId, uint32_t dstId, uint8_t lco);
    /**
     * @brief Clears MFID transactions by LCO and optional subscriber.
     */
    static void p25ClearMFIds(p25::Control& control, uint8_t lco, uint32_t subscriberId = 0U);
    /**
     * @brief Gets the legacy ambient MFID compatibility value.
     */
    static uint8_t p25LastMFId(const p25::Control& control);
    /**
     * @brief Sets the legacy ambient MFID compatibility value.
     */
    static void p25SetLastMFId(p25::Control& control, uint8_t mfId);
    /**
     * @brief Releases a P25 grant for a steering-policy test.
     * @param control P25 control instance.
     * @param dstId Destination ID to release.
     * @returns bool True if the grant was released; otherwise, false.
     */
    static bool p25ReleaseGrant(p25::Control& control, uint32_t dstId);
    /**
     * @brief Releases the active P25 private grant involving a radio ID.
     * @param control P25 control instance.
     * @param unitId RID of either private-call participant.
     * @returns bool True if a private grant was released.
     */
    static bool p25ReleaseU2UGrant(p25::Control& control, uint32_t unitId);
    /**
     * @brief Applies the P25 voice-channel grant-release policy.
     * @param control P25 control instance.
     * @param dstId Destination-keyed grant owner reported by the voice channel.
     */
    static void p25VoiceChannelReleaseGrant(p25::Control& control, uint32_t dstId);
    /**
     * @brief Gets the last P25 RF destination ID.
     * @param control P25 control instance.
     * @returns uint32_t Last RF destination ID.
     */
    static uint32_t p25RFLastDstId(const p25::Control& control);
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
    static void p25UpdateNetEncryption(p25::Control& control, const p25::lc::LC& lc, const p25::data::LowSpeedData& lsd);
    /**
     * @brief Injects synthetic P25 network call termination via voice packet path.
     * @param control P25 control instance.
     * @param lc Link control data for synthetic termination.
     * @param duid DUID to use for termination frame.
     * @return bool True if termination processing succeeds.
     */
    static bool p25TerminateNetCall(p25::Control& control, const p25::lc::LC& lc, p25::defines::DUID::E duid = p25::defines::DUID::TDU);
    /**
     * @brief Attempts to write a synthetic P25 RF voice frame to the network.
     * @param control P25 control instance.
     * @param srcId Source radio ID.
     * @param dstId Destination radio or talkgroup ID.
     * @param group True for group traffic; false for unit-to-unit traffic.
     */
    static void p25WriteRFVoiceToNetwork(p25::Control& control, uint32_t srcId, uint32_t dstId, bool group);
    /**
     * @brief Processes one queued P25 network frame.
     * @param control P25 control instance.
     */
    static void p25ProcessNetworkQueue(p25::Control& control);
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
    static uint8_t nxdnNetAlgId(const nxdn::Control& control);
    static uint8_t nxdnNetKId(const nxdn::Control& control);
    static void nxdnNetMI(const nxdn::Control& control, uint8_t* mi);
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
     * @brief Forces NXDN control into residual RF talkgroup hang state for tests.
     * @param control NXDN control instance.
     * @param srcId Source ID to retain as the last RF source.
     * @param dstId Destination ID to retain as the last RF destination.
     */
    static void nxdnSetRFHang(nxdn::Control& control, uint32_t srcId, uint32_t dstId);
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
    static bool nxdnStartRFCall(nxdn::Control& control, uint32_t srcId, uint32_t dstId, bool group = true);
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
