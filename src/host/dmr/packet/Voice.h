// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2022 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file Voice.h
 * @ingroup host_dmr
 * @file Voice.cpp
 * @ingroup host_dmr
 */
#if !defined(__DMR_PACKET_VOICE_H__)
#define __DMR_PACKET_VOICE_H__

#include "Defines.h"
#include "common/dmr/data/NetData.h"
#include "common/dmr/data/EmbeddedData.h"
#include "common/dmr/lc/LC.h"
#include "common/dmr/lc/PrivacyLC.h"
#include "common/edac/AMBEFEC.h"
#include "common/network/BaseNetwork.h"

#include <vector>

namespace dmr
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    namespace packet { class HOST_SW_API Data; }
    class HOST_SW_API Slot;

    namespace packet
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief This class implements core logic for handling DMR voice packets.
         * @ingroup host_dmr
         */
        class HOST_SW_API Voice {
        public:
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
             * @param[in] data Instance of data::NetData DMR data container class.
             * @returns bool True, if data frame is processed, otherwise false.
             */
            void processNetwork(const data::NetData& dmrData);
            /** @} */

        private:
            friend class packet::Data;
            friend class dmr::Slot;
            Slot* m_slot;

            uint8_t* m_lastFrame;
            bool m_lastFrameValid;

            uint8_t m_rfN;
            uint8_t m_lastRfN;
            uint8_t m_netN;

            data::EmbeddedData m_rfEmbeddedLC;
            data::EmbeddedData* m_rfEmbeddedData;
            uint32_t m_rfEmbeddedReadN;
            uint32_t m_rfEmbeddedWriteN;

            data::EmbeddedData m_netEmbeddedLC;
            data::EmbeddedData* m_netEmbeddedData;
            uint32_t m_netEmbeddedReadN;
            uint32_t m_netEmbeddedWriteN;

            uint8_t m_rfTalkerId;
            uint8_t m_netTalkerId;

            edac::AMBEFEC m_fec;

            bool m_embeddedLCOnly;
            bool m_dumpTAData;

            bool m_verbose;
            bool m_debug;

            /**
             * @brief Initializes a new instance of the Voice class.
             * @param slot DMR slot.
             * @param network Instance of the BaseNetwork class.
             * @param embeddedLCOnly 
             * @param dumpTAData 
             * @param debug Flag indicating whether DMR debug is enabled.
             * @param verbose Flag indicating whether DMR verbose logging is enabled.
             */
            Voice(Slot* slot, network::BaseNetwork* network, bool embeddedLCOnly, bool dumpTAData, bool debug, bool verbose);
            /**
             * @brief Finalizes a instance of the Voice class.
             */
            ~Voice();

            /**
             * @brief Log GPS information.
             * @param srcId Source radio ID.
             * @param[in] data Buffer containing GPS data.
             */
            void logGPSPosition(const uint32_t srcId, const uint8_t* data);

            /**
             * @brief Helper to insert AMBE null frames for missing audio.
             * @param data Buffer containing frame data.
             */
            void insertNullAudio(uint8_t* data);
            /**
             * @brief Helper to insert DMR AMBE silence frames.
             * @param[in] data Buffer containing frame data.
             * @param seqNo DMR AMBE sequence number.
             */
            bool insertSilence(const uint8_t* data, uint8_t seqNo);
            /**
             * @brief Helper to insert DMR AMBE silence frames.
             * @param count 
             */
            void insertSilence(uint32_t count);
        };
    } // namespace packet
} // namespace dmr

#endif // __DMR_PACKET_VOICE_H__
