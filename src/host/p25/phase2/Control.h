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
 * @file Control.h
 * @ingroup host_p25
 * @file Control.cpp
 * @ingroup host_p25
 */
#if !defined(__P25_P2_CONTROL_H__)
#define __P25_P2_CONTROL_H__

#include "Defines.h"
#include "common/network/Network.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "p25/phase2/Slot.h"
#include "p25/lookups/P25AffiliationLookup.h"
#include "modem/Modem.h"

#include <cstdint>

// ---------------------------------------------------------------------------
//  Class Prototypes
// ---------------------------------------------------------------------------

#if defined(CATCH2_TEST_COMPILATION)
class HostTestHooks;
#endif

namespace p25
{
    namespace phase2
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief This class implements core controller logic for handling P25 Phase 2.
         * @ingroup host_p25
         */
        class HOST_SW_API Control
        {
        public:
            /** @brief Number of P25 Phase 2 TDMA slots. */
            static constexpr uint32_t SLOT_COUNT = 2U;

            /**
             * @brief Initializes a new instance of the Control class.
             * @param authoritative Flag indicating whether or not the DVM is grant authoritative.
             * @param callHang Amount of hangtime for a P25 Phase 2 call.
             * @param timeout Transmit timeout.
             * @param tgHang Amount of time to hang on the last talkgroup mode from RF.
             * @param modem Instance of the Modem class.
             * @param network Instance of the BaseNetwork class.
             * @param affiliations Instance of the P25AffiliationLookup class.
             * @param ridLookup Instance of the RadioIdLookup class.
             * @param tidLookup Instance of the TalkgroupRulesLookup class.
             * @param queueSize Modem frame buffer queue size (bytes).
             * @param debug Flag indicating whether P25 debug is enabled.
             * @param verbose Flag indicating whether P25 verbose logging is enabled.
             * @param colorCode P25 Phase 2 color code expected on inbound unsrambled MAC PDUs.
             */
            Control(bool authoritative, uint32_t callHang, uint32_t timeout, uint32_t tgHang,
                modem::Modem* modem, network::Network* network, lookups::P25AffiliationLookup* affiliations,
                ::lookups::RadioIdLookup* ridLookup, ::lookups::TalkgroupRulesLookup* tidLookup,
                uint32_t queueSize, bool debug, bool verbose, uint16_t colorCode = 0U);
            /**
             * @brief Finalizes a P25 Phase 2 controller.
             */
            ~Control();

            /** @brief Resets both Phase 2 slots and modem queues. */
            void reset();

            /** @name Frame Processing */
            /**
             * @brief Process a data frame from the RF interface.
             * @param slotNo P25 Phase 2 slot number.
             * @param data Buffer containing data frame.
             * @param len Length of data frame.
             * @returns bool True, if frame was successfully processed, otherwise false.
             */
            bool processFrame(uint32_t slotNo, uint8_t* data, uint32_t len);
            /**
             * @brief Get the frame data length for the next frame in the slot queue.
             * @param slotNo P25 Phase 2 slot number.
             * @returns uint32_t Length of frame data retrieved.
             */
            uint32_t peekFrameLength(uint32_t slotNo);
            /**
             * @brief Helper to determine whether or not the slot queue is full.
             * @param slotNo P25 Phase 2 slot number.
             * @returns bool True if the slot queue is full, otherwise false.
             */
            bool isQueueFull(uint32_t slotNo);
            /**
             * @brief Get frame data from the slot queue.
             * @param slotNo P25 Phase 2 slot number.
             * @param[out] data Buffer to store frame data.
             * @param[out] imm Flag indicating whether the frame is immediate.
             * @returns uint32_t Length of frame data retrieved.
             */
            uint32_t getFrame(uint32_t slotNo, uint8_t* data, bool* imm = nullptr);
            /** @} */

            /** @name Data Clocking */
            /**
             * @brief Updates the processor.
             * @param ms Number of milliseconds.
             */
            void clock(uint32_t ms);
            /** @} */

            /**
             * @brief Gets a P25 Phase 2 slot instance.
             * @param slotNo P25 Phase 2 slot number.
             * @returns Slot& Slot instance.
             */
            Slot& slot(uint32_t slotNo);
            /**
             * @brief Gets a constant P25 Phase 2 slot instance.
             * @param slotNo P25 Phase 2 slot number.
             * @returns const Slot& Slot instance.
             */
            const Slot& slot(uint32_t slotNo) const;

            /** 
             * @brief Returns the RF state for a slot, or RS_RF_INVALID. 
             * @param slotNo P25 Phase 2 slot number.
             * @returns RPT_RF_STATE RF state for the specified slot, or RS_RF_INVALID.
             */
            RPT_RF_STATE getRFState(uint32_t slotNo) const;
            /** 
             * @brief Returns the network state for a slot. 
             * @param slotNo P25 Phase 2 slot number.
             * @returns RPT_NET_STATE Network state for the specified slot.
             */
            RPT_NET_STATE getNetState(uint32_t slotNo) const;
            /** 
             * @brief Clears a rejected RF state for a slot. 
             * @param slotNo P25 Phase 2 slot number.
             */
            void clearRFReject(uint32_t slotNo);
            /** 
             * @brief Returns whether either Phase 2 slot is busy. 
             * @returns bool True if either slot is busy, false otherwise.
             */
            bool isBusy() const;

            /** @name Supervisory Control */
            /** 
             * @brief Permits a destination on a non-authoritative Phase 2 slot. 
             * @param dstId Destination ID.
             * @param slotNo P25 Phase 2 slot number.
             */
            void permittedTG(uint32_t dstId, uint32_t slotNo);
            /** 
             * @brief Touches a CC grant associated with a Phase 2 slot. 
             * @param dstId Destination ID.
             * @param slotNo P25 Phase 2 slot number.
             */
            void touchGrantTG(uint32_t dstId, uint32_t slotNo);
            /** 
             * @brief Releases a CC grant associated with a Phase 2 slot. 
             * @param dstId Destination ID.
             * @param slotNo P25 Phase 2 slot number.
             */
            void releaseGrantTG(uint32_t dstId, uint32_t slotNo);
            /** @} */

        private:
            friend class Slot;
            friend class packet::phase2::Voice;
#if defined(CATCH2_TEST_COMPILATION)
            friend class ::HostTestHooks;
#endif

            /* Interfaces */
            modem::Modem* m_modem;
            network::Network* m_network;

            /* TDMA Slots */
            Slot* m_slot1;
            Slot* m_slot2;

            /* Access and Grant Lookups */
            ::lookups::RadioIdLookup* m_ridLookup;
            ::lookups::TalkgroupRulesLookup* m_tidLookup;
            lookups::P25AffiliationLookup* m_affiliations;

            /* Call Timing */
            uint32_t m_callHang;
            uint32_t m_timeout;
            uint32_t m_tgHang;

            /* Logging */
            bool m_debug;
            bool m_verbose;

            /**
             * @brief Process a data frame from the network.
             * @returns bool True, if frame was successfully processed, otherwise false.
             */
            void processNetwork();

            /**
             * @brief Writes an RF frame to the Phase 2 network stream.
             * @param slot Slot that owns the frame.
             * @param data Buffer containing the frame data.
             * @param len Length of the frame data.
             * @returns bool True if the frame was written, otherwise false.
             */
            bool writeNetwork(Slot* slot, const uint8_t* data, uint32_t len);
        };
    } // namespace phase2
} // namespace p25

#endif // __P25_P2_CONTROL_H__
