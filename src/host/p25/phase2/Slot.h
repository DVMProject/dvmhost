// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file Slot.h
 * @ingroup host_p25
 * @file Slot.cpp
 * @ingroup host_p25
 */
#if !defined(__P25_P2_SLOT_H__)
#define __P25_P2_SLOT_H__

#include "Defines.h"
#include "common/p25/P25Defines.h"
#include "common/p25/lc/LC.h"
#include "common/network/Network.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "common/RingBuffer.h"
#include "common/Timer.h"
#include "p25/packet/phase2/Voice.h"
#include "modem/Modem.h"

#include <cstdint>
#include <mutex>

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------

#if defined(CATCH2_TEST_COMPILATION)
class HostTestHooks;
#endif

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    namespace phase2 { class HOST_SW_API Control; }
    namespace lookups { class HOST_SW_API P25AffiliationLookup; }

    namespace phase2 
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements one P25 Phase 2 TDMA slot.
         * @ingroup host_p25
         */
        class HOST_SW_API Slot
        {
        public:
            /**
             * @brief Initializes a new instance of the Slot class.
             * @param slotNo P25 Phase 2 slot number.
             * @param timeout RF/network transmit timeout in seconds.
             * @param tgHang Talkgroup hang time in seconds.
             * @param queueSize Slot queue size in bytes.
             * @param debug Enable debug logging.
             * @param verbose Enable verbose logging.
             */
            Slot(uint32_t slotNo, uint32_t timeout, uint32_t tgHang, uint32_t queueSize, bool debug, bool verbose);

            /** @brief Resets RF/network state, statistics, and queued frames. */
            void reset();

            /** @name Frame Processing */
            /**
             * @brief Process a data frame from the RF interface.
             * @param data Buffer containing data frame.
             * @param len Length of data frame.
             * @returns bool True, if frame was successfully processed, otherwise false.
             */
            bool processFrame(uint8_t* data, uint32_t len);
            /**
             * @brief Get the frame data length for the next frame in the slot queue.
             * @returns uint32_t Length of frame data retrieved.
             */
            uint32_t peekFrameLength();
            /**
             * @brief Helper to determine whether or not the slot queue is full.
             * @returns bool True if the slot queue is full, otherwise false.
             */
            bool isQueueFull() const;
            /**
             * @brief Get frame data from the slot queue.
             * @param[out] data Buffer to store frame data.
             * @param[out] imm Flag indicating whether the frame is immediate.
             * @returns uint32_t Length of frame data retrieved.
             */
            uint32_t getFrame(uint8_t* data, bool* imm = nullptr);

            /**
             * @brief Process a data frame from the network.
             * @param data Buffer containing data frame.
             * @param len Length of data frame.
             * @param control Decoded Phase 2 link control.
             * @param duid Phase 2 data unit ID.
             * @param scramblerOffset Scrambler offset carried by the network frame.
             * @param controlByte DVM network control byte.
             * @returns bool True, if frame was successfully processed, otherwise false.
             */
            bool processNetwork(uint8_t* data, uint32_t len, const lc::LC& control, defines::P2_DUID::E duid, 
                uint16_t scramblerOffset, uint8_t controlByte);
            /** @} */

            /** @name Data Clocking */
            /**
             * @brief Updates the processor.
             * @param ms Number of milliseconds.
             */
            void clock(uint32_t ms);
            /** @} */

            /** @name Supervisory Control */
            /**
             * @brief Permits a TGID on a non-authoritative host.
             * @param dstId Destination ID.
             */
            void permittedTG(uint32_t dstId);
            /** 
             * @brief Touches an active grant to keep its CC timer alive. 
             * @param dstId Destination ID.
             */
            void touchGrantTG(uint32_t dstId);
            /** 
             * @brief Releases an active grant. 
             * @param dstId Destination ID.
             */
            void releaseGrantTG(uint32_t dstId);
            /** 
             * @brief Clears a rejected RF call and returns the slot to listening. 
             */
            void clearRFReject();
            /** @} */

            /**
             * @brief Gets the P25 Phase 2 slot number.
             * @returns uint32_t P25 Phase 2 slot number.
             */
            uint32_t getSlotNo() const { return m_slotNo; }
            /** 
             * @brief Returns the current RF state. 
             * @returns RPT_RF_STATE Current RF state.
             */
            RPT_RF_STATE getRFState() const { return m_rfState; }
            /** 
             * @brief Returns the current network state. 
             * @returns RPT_NET_STATE Current network state.
             */
            RPT_NET_STATE getNetState() const { return m_netState; }
            /** 
             * @brief Returns whether RF or network traffic owns the slot. 
             * @returns bool True if the slot is busy, false otherwise.
             */
            bool isBusy() const { return m_rfState != RS_RF_LISTENING || m_netState != RS_NET_IDLE; }

            /**
             * @brief Helper to initialize shared P25 Phase 2 slot configuration.
             * @param p2 Instance of the Control class.
             * @param authoritative Flag indicating whether or not the DVM is grant authoritative.
             * @param callHang Amount of hangtime for a P25 Phase 2 call.
             * @param modem Instance of the Modem class.
             * @param network Instance of the Network class.
             * @param affiliations Instance of the P25AffiliationLookup class.
             * @param ridLookup Instance of the RadioIdLookup class.
             * @param tidLookup Instance of the TalkgroupRulesLookup class.
             * @param colorCode P25 Phase 2 color code expected on inbound unsrambled MAC PDUs.
             */
            static void init(Control* control, bool authoritative, uint32_t callHang, modem::Modem* modem, network::Network* network,
                lookups::P25AffiliationLookup* affiliations, ::lookups::RadioIdLookup* ridLookup,
                ::lookups::TalkgroupRulesLookup* tidLookup, uint16_t colorCode = 0U);

        private:
#if defined(CATCH2_TEST_COMPILATION)
            friend class ::HostTestHooks;
#endif
            friend class phase2::Control;
            friend class packet::phase2::Voice;
            packet::phase2::Voice m_voice;

            uint32_t m_slotNo;

            RingBuffer<uint8_t> m_txQueue;
            RingBuffer<uint8_t> m_txImmQueue;
            mutable std::mutex m_queueLock;

            RPT_RF_STATE m_rfState;
            uint32_t m_rfLastDstId;
            uint32_t m_rfLastSrcId;
            RPT_NET_STATE m_netState;
            uint32_t m_netLastDstId;
            uint32_t m_netLastSrcId;

            uint32_t m_permittedDstId;

            lc::LC m_control;

        public:
            /** 
             * @brief Host-side VCH lifecycle. Physical LCCH/ISCH scheduling remains in the modem. 
             */
            enum class VCH_STATE : uint8_t {
                IDLE,
                PTT,
                ACTIVE,
                HANGTIME,
                TERMINATING
            };

        private:

            bool m_rfResetPending;
            bool m_netResetPending;
            VCH_STATE m_rfVCHState;
            VCH_STATE m_netVCHState;
            uint8_t m_rfPTTCount;
            uint8_t m_rfEndPTTCount;
            uint8_t m_netEndPTTCount;

            uint32_t m_rfFrames;
            uint32_t m_netFrames;
            uint32_t m_netLost;
            uint32_t m_netMissed;

            uint32_t m_rfBits;
            uint32_t m_netBits;
            uint32_t m_rfErrs;
            uint32_t m_netErrs;

            bool m_rfTimeout;
            bool m_netTimeout;

            uint8_t m_frameLossCnt;
            uint8_t m_frameLossThreshold;

            uint32_t m_elapsedMs;
            uint32_t m_vcuElapsedMs;

            Timer m_rfTimeoutTimer;
            Timer m_rfTGHang;
            Timer m_netTimeoutTimer;
            Timer m_netTGHang;
            Timer m_networkWatchdog;
            Timer m_rfLossWatchdog;
            Timer m_rfCallHangTimer;
            Timer m_netCallHangTimer;

            bool m_debug;
            bool m_verbose;

            defines::P2_DUID::E m_duid;

            uint8_t m_controlByte;

            uint32_t m_rfBurstCount;
            uint32_t m_netBurstCount;

            uint16_t m_rfScrambleOffset;
            uint16_t m_netScrambleOffset;

            static Control* s_control;

            static bool s_authoritative;

            static uint32_t s_callHang;
            static uint16_t s_colorCode;

            static modem::Modem* s_modem;
            static network::Network* s_network;

            static lookups::P25AffiliationLookup* s_affiliations;
            static ::lookups::RadioIdLookup* s_ridLookup;
            static ::lookups::TalkgroupRulesLookup* s_tidLookup;

            /**
             * @brief Adds a frame to the slot queue.
             * @param data Buffer containing frame data.
             * @param net Flag indicating whether the frame is from the network or RF.
             * @returns bool True, if frame was successfully added to the queue, otherwise false.
             */
            bool addFrame(const uint8_t* data, uint32_t len, defines::P2_DUID::E duid,
                bool net = false, bool imm = false);
            /** 
             * @brief Resets RF call state and timers without resetting network state. 
             */
            void resetRF();
            /** 
             * @brief Resets network call state and timers without resetting RF state. 
             */
            void resetNet();

            /** 
             * @brief Validates RF call ownership and access-control policy. 
             * @param srcId Source ID of the RF call.
             * @param dstId Destination ID of the RF call.
             * @param group Flag indicating whether the call is a group call.
             * @returns bool True if the RF call is valid and allowed by access-control policy.
             */
            bool validateRFCall(uint32_t srcId, uint32_t dstId, bool group);
            /**
             *  @brief Touches the grant associated with accepted traffic. 
             *  @param dstId Destination ID of the call whose grant is being touched.
             */
            void touchGrant(uint32_t dstId);

            /** 
             * @brief Helper to process loss of frame stream from modem.
             * @param type Type of RF frame loss.
             */
            void processFrameLoss(RPT_RF_LOSS_TYPE type);
            
            /** 
             * @brief Begins message-trunking hangtime for an RF or network call. 
             * @param net Flag indicating whether the hangtime is for the network or RF call.
             */
            void beginHangtime(bool net);
            /** 
             * @brief Queues the two-burst call termination sequence. 
             * @param net Flag indicating whether the termination is for the network or RF call.
             */
            void writeEnd(bool net);

            /**
             * @brief Encodes and queues one outbound logical MAC PDU.
             * @note Physical LCCH, ISCH, slot, and superframe scheduling is modem-owned.
             */
            bool queueMACPDU(uint8_t opcode, bool imm = false);

            /**
             * @brief Processes and forwards one logical FACCH or SACCH burst.
             * @param data Buffer containing the logical MAC-bearing burst.
             * @param len Length of the logical burst.
             * @param duid Phase 2 data unit ID.
             * @param net Flag indicating that the burst originated from the network.
             * @param[out] release Flag set when the MAC message releases the current call.
             * @returns bool True if the MAC burst was successfully processed.
             */
            bool processMAC(uint8_t* data, uint32_t len, defines::P2_DUID::E duid,
                bool net, bool& release);

            /** 
             * @brief Returns whether a DUID identifies a voice traffic burst. 
             * @param duid The DUID to check.
             * @returns bool True if the DUID identifies a voice traffic burst, otherwise false.
             */
            static bool isVoiceDUID(defines::P2_DUID::E duid);
            /** 
             * @brief Returns whether a DUID identifies a VCH FACCH burst. 
             * @param duid The DUID to check.
             * @returns bool True if the DUID identifies a VCH FACCH burst, otherwise false.
             */
            static bool isFACCHDUID(defines::P2_DUID::E duid);
            /** 
             * @brief Returns whether a DUID identifies a VCH SACCH burst. 
             * @param duid The DUID to check.
             * @returns bool True if the DUID identifies a VCH SACCH burst, otherwise false.
             */
            static bool isSACCHDUID(defines::P2_DUID::E duid);
            /** 
             * @brief Returns whether a DUID identifies an LCCH burst. 
             * @param duid The DUID to check.
             * @returns bool True if the DUID identifies an LCCH burst, otherwise false.
             */
            static bool isLCCHDUID(defines::P2_DUID::E duid);
        };
    } // namespace phase2
} // namespace p25

#endif // __P25_P2_SLOT_H__
