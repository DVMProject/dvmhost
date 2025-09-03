// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015-2020 Jonathan Naylor, G4KLX
 *  Copyright (C) 2022-2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file Voice.h
 * @ingroup host_nxdn
 * @file Voice.cpp
 * @ingroup host_nxdn
 */
#if !defined(__NXDN_PACKET_VOICE_H__)
#define __NXDN_PACKET_VOICE_H__

#include "Defines.h"
#include "nxdn/Control.h"

#include <cstdio>
#include <string>

namespace nxdn
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    namespace packet { class HOST_SW_API Data; }
    class HOST_SW_API Control;

    namespace packet
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief This class implements handling logic for NXDN voice packets.
         * @ingroup host_nxdn
         */
        class HOST_SW_API Voice {
        public:
            /**
             * @brief Resets the data states for the RF interface.
             */
            void resetRF();
            /**
             * @brief Resets the data states for the network.
             */
            void resetNet();

            /** @name Frame Processing */
            /**
             * @brief Process a data frame from the RF interface.
             * @param fct Functional channel type.
             * @param option Channel options.
             * @param data Buffer containing data frame.
             * @param len Length of data frame.
             * @returns bool True, if data frame is processed, otherwise false.
             */
            bool process(defines::FuncChannelType::E fct, defines::ChOption::E option, uint8_t* data, uint32_t len);
            /**
             * @brief Process a data frame from the network.
             * @param fct Functional channel type.
             * @param option Channel options.
             * @param netLC RTCH from the network.
             * @param data Buffer containing data frame.
             * @param len Length of data frame.
             * @returns bool True, if data frame is processed, otherwise false.
             */
            bool processNetwork(defines::FuncChannelType::E fct, defines::ChOption::E option, lc::RTCH& netLC, uint8_t* data, uint32_t len);
            /** @} */

        protected:
            friend class packet::Data;
            friend class nxdn::Control;
            Control* m_nxdn;

            uint32_t m_rfFrames;
            uint32_t m_rfBits;
            uint32_t m_rfErrs;
            uint32_t m_rfUndecodableLC;
            uint32_t m_netFrames;
            uint32_t m_netLost;

            uint16_t m_lastRejectId;

            uint32_t m_silenceThreshold;

            bool m_verbose;
            bool m_debug;

            /**
             * @brief Initializes a new instance of the Voice class.
             * @param nxdn Instance of the Control class.
             * @param debug Flag indicating whether NXDN debug is enabled.
             * @param verbose Flag indicating whether NXDN verbose logging is enabled.
             */
            Voice(Control* nxdn, bool debug, bool verbose);
            /**
             * @brief Finalizes a instance of the Voice class.
             */
            ~Voice();

            /**
             * @brief Write data processed from RF to the network.
             * @param[in] data Buffer to write to the network.
             * @param len Length of data frame.
             */
            void writeNetwork(const uint8_t* data, uint32_t len);

            /**
             * @brief Helper to perform RF traffic collision checking.
             * @param srcId Source ID.
             * @param dstId Destination ID.
             * @return bool True, if traffic collision, otherwise false.
             */
            bool checkRFTrafficCollision(uint32_t srcId, uint32_t dstId);
            /**
             * @brief Helper to perform network traffic collision checking.
             * @param lc 
             * @param srcId Source ID.
             * @param dstId Destination ID.
             * @return bool True, if traffic collision, otherwise false.
             */
            bool checkNetTrafficCollision(lc::RTCH lc, uint32_t srcId, uint32_t dstId);
        };
    } // namespace packet
} // namespace nxdn

#endif // __NXDN_PACKET_VOICE_H__
