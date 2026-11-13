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
 * @file Voice.h
 * @ingroup host_p25
 * @file Voice.cpp
 * @ingroup host_p25
 */
#if !defined(__P25_P2_VOICE_H__)
#define __P25_P2_VOICE_H__

#include "Defines.h"
#include "common/edac/AMBE2FEC.h"
#include "common/edac/RS634717.h"
#include "common/p25/P25Defines.h"
#include "common/p25/lc/LC.h"

#include <cstdint>

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

    namespace phase2 { class HOST_SW_API Slot; }

    namespace packet
    {
        namespace phase2
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief Processes P25 Phase 2 voice bursts without modem PHY logic.
             * @ingroup host_p25
             */
            class HOST_SW_API Voice
            {
            public:
                /** 
                 * @brief Resets RF voice counters. 
                 */
                void resetRF();
                /** 
                 * @brief Resets network voice counters. 
                 */
                void resetNet();

                /** @name Frame Processing */
                /**
                 * @brief Process a data frame from the RF interface.
                 * @param data Buffer containing data frame.
                 * @param len Length of data frame.
                 * @returns bool True, if data frame is processed, otherwise false.
                 */
                bool process(uint8_t* data, uint32_t len);
                /**
                 * @brief Process a data frame from the network.
                 * @param data Buffer containing data frame.
                 * @param len Length of data frame.
                 * @returns bool True, if data frame is processed, otherwise false.
                 */
                bool processNetwork(uint8_t* data, uint32_t len);
                /**
                 * @brief Builds and queues an outbound SACCH voice-channel-user PDU.
                 * @param opcode MAC_ACTIVE or MAC_HANGTIME PDU opcode.
                 * @param imm Flag indicating whether the PDU uses the immediate queue.
                 * @returns bool True if the logical SACCH burst was queued.
                 * @note The modem selects the physical SACCH opportunity.
                 */
                bool writeVoiceLC(uint8_t opcode, bool imm = false);
                /** @} */

                /** @name Statistics */
                /** 
                 * @brief Returns the number of RF bursts processed. 
                 * @returns uint32_t The number of RF bursts processed.
                 */
                uint32_t getRFFrames() const { return m_rfFrames; }
                /** 
                 * @brief Returns the number of network bursts processed. 
                 * @returns uint32_t The number of network bursts processed.
                 */
                uint32_t getNetFrames() const { return m_netFrames; }
                /** @} */

            private:
                friend class p25::phase2::Slot;
#if defined(CATCH2_TEST_COMPILATION)
                friend class ::HostTestHooks;
#endif

                p25::phase2::Slot* m_slot;

                uint32_t m_rfFrames;
                uint32_t m_netFrames;
                uint32_t m_rfESSBCount;
                uint32_t m_netESSBCount;

                uint8_t m_rfESS[44U];
                bool m_rfESSB[4U];
                bool m_rfESSA;
                bool m_rfESSComplete;

                uint8_t m_netESS[44U];
                bool m_netESSB[4U];
                bool m_netESSA;
                bool m_netESSComplete;

                bool m_debug;
                bool m_verbose;

                edac::AMBE2FEC m_ambe2FEC;
                edac::RS634717 m_essRS;

                /**
                 * @brief Initializes a new instance of the Voice class.
                 * @param slot Instance of the Slot class.
                 * @param debug Flag indicating whether P25 debug is enabled.
                 * @param verbose Flag indicating whether P25 verbose logging is enabled.
                 */
                Voice(p25::phase2::Slot* slot, bool debug, bool verbose);

                /**
                 * @brief Validates the logical Phase 2 burst length.
                 * @param data Buffer containing the logical burst.
                 * @param length Length of the logical burst.
                 * @returns bool True if the burst has the expected host-side shape.
                 */
                bool validFrame(const uint8_t* data, uint32_t length) const;

                /** 
                 * @brief Collects and decodes ESS carried by a 4V voice burst. 
                 * @param burst Buffer containing the 4V voice burst.
                 * @param burstNo The burst number within the voice frame.
                 * @param net Flag indicating whether the burst is from the network.
                 * @returns bool True if the ESS was successfully processed.
                 */
                bool processESS4V(const uint8_t* burst, uint32_t burstNo, bool net);
                /** 
                 * @brief Collects and decodes ESS carried by a 2V voice burst. 
                 * @param burst Buffer containing the 2V voice burst.
                 * @param net Flag indicating whether the burst is from the network.
                 * @returns bool True if the ESS was successfully processed.
                 */
                bool processESS2V(const uint8_t* burst, bool net);
                /** 
                 * @brief Decodes a complete RF or network ESS codeword. 
                 * @param net Flag indicating whether the ESS is from the network.
                 * @returns bool True if the ESS was successfully decoded.
                 */
                bool decodeESS(bool net);
            };
        } // namespace phase2
    } // namespace packet
} // namespace p25

#endif // __P25_P2_VOICE_H__
