// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022-2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file ControlSignaling.h
 * @ingroup host_nxdn
 * @file ControlSignaling.cpp
 * @ingroup host_nxdn
 */
#if !defined(__NXDN_PACKET_CONTROL_SIGNALING_H__)
#define __NXDN_PACKET_CONTROL_SIGNALING_H__

#include "Defines.h"
#include "common/nxdn/lc/RCCH.h"
#include "nxdn/Control.h"

#include <cstdio>
#include <string>

namespace nxdn
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    namespace packet { class HOST_SW_API Data; }
    namespace packet { class HOST_SW_API Voice; }
    class HOST_SW_API Control;

    namespace packet
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief This class implements handling logic for NXDN RCCH packets.
         * @ingroup host_nxdn
         */
        class HOST_SW_API ControlSignaling {
        public:
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
            friend class nxdn::packet::Data;
            friend class nxdn::packet::Voice;
            friend class nxdn::Control;
            Control* m_nxdn;

            uint8_t m_bcchCnt;
            uint8_t m_rcchGroupingCnt;
            uint8_t m_ccchPagingCnt;
            uint8_t m_ccchMultiCnt;
            uint8_t m_rcchIterateCnt;

            bool m_verifyAff;
            bool m_verifyReg;

            bool m_disableGrantSrcIdCheck;

            uint16_t m_lastRejectId;

            bool m_verbose;
            bool m_debug;

            /**
             * @brief Initializes a new instance of the ControlSignaling class.
             * @param nxdn Instance of the Control class.
             * @param debug Flag indicating whether NXDN debug is enabled.
             * @param verbose Flag indicating whether NXDN verbose logging is enabled.
             */
            ControlSignaling(Control* nxdn, bool debug, bool verbose);
            /**
             * @brief Finalizes a instance of the ControlSignaling class.
             */
            ~ControlSignaling();

            /**
             * @brief Write data processed from RF to the network.
             * @param[in] data Buffer to write to the network.
             * @param len Length of data frame.
             */
            void writeNetwork(const uint8_t* data, uint32_t len);

            /*
            ** Modem Frame Queuing
            */

            /**
             * @brief Helper to write a immediate single-block RCCH packet.
             * @param rcch RCCH to write to the modem.
             * @param noNetwork Flag indicating not to write the TSBK to the network.
             */
            void writeRF_Message_Imm(lc::RCCH *rcch, bool noNetwork) { writeRF_Message(rcch, noNetwork, true); }
            /**
             * @brief Helper to write a single-block RCCH packet.
             * @param rcch RCCH to write to the modem.
             * @param noNetwork Flag indicating not to write the TSBK to the network.
             * @param imm Flag indicating the TSBK should be written to the immediate queue.
             */
            void writeRF_Message(lc::RCCH* rcch, bool noNetwork, bool imm = false);

            /*
            ** Control Signalling Logic
            */

            /**
             * @brief Helper to write control channel packet data.
             * @param frameCnt Frame counter.
             * @param n 
             * @param adjSS Flag indicating whether or not adjacent site status should be broadcast.
             */
            void writeRF_ControlData(uint8_t frameCnt, uint8_t n, bool adjSS);

            /**
             * @brief Helper to write a grant packet.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             * @param serviceOptions Service Options.
             * @param grp Flag indicating the destination ID is a talkgroup.
             * @param net Flag indicating this grant is coming from network traffic.
             * @param skip Flag indicating normal grant checking is skipped.
             * @param chNo Channel Number.
             * @returns True, if granted, otherwise false.
             */
            bool writeRF_Message_Grant(uint32_t srcId, uint32_t dstId, uint8_t serviceOptions, bool grp, bool net = false, bool skip = false, uint32_t chNo = 0U);
            /**
             * @brief Helper to write a deny packet.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             * @param reason Denial Reason.
             * @param service Service being denied.
             */
            void writeRF_Message_Deny(uint32_t srcId, uint32_t dstId, uint8_t reason, uint8_t service);
            /**
             * @brief Helper to write a group registration response packet.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             * @param locId Location ID.
             */
            bool writeRF_Message_Grp_Reg_Rsp(uint32_t srcId, uint32_t dstId, uint32_t locId);
            /**
             * @brief Helper to write a unit registration response packet.
             * @param srcId Source Radio ID.
             * @param dstId Destination ID.
             * @param locId Location ID.
             */
            void writeRF_Message_U_Reg_Rsp(uint32_t srcId, uint32_t dstId, uint32_t locId);

            /**
             * @brief Helper to write a CC SITE_INFO broadcast packet on the RF interface.
             */
            void writeRF_CC_Message_Site_Info();
            /**
             * @brief Helper to write a CC SRV_INFO broadcast packet on the RF interface.
             */
            void writeRF_CC_Message_Service_Info();
        };
    } // namespace packet
} // namespace nxdn

#endif // __NXDN_PACKET_CONTROL_SIGNALING_H__
