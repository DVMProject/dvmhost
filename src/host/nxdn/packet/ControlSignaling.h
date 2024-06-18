// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Modem Host Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022-2023 Bryan Biedenkapp, N2PLL
*
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
        //      This class implements handling logic for NXDN RCCH packets.
        // ---------------------------------------------------------------------------

        class HOST_SW_API ControlSignaling {
        public:
            /// <summary>Process a data frame from the RF interface.</summary>
            bool process(uint8_t fct, uint8_t option, uint8_t* data, uint32_t len);
            /// <summary>Process a data frame from the network.</summary>
            bool processNetwork(uint8_t fct, uint8_t option, lc::RTCH& netLC, uint8_t* data, uint32_t len);

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

            /// <summary>Initializes a new instance of the ControlSignaling class.</summary>
            ControlSignaling(Control* nxdn, bool debug, bool verbose);
            /// <summary>Finalizes a instance of the ControlSignaling class.</summary>
            ~ControlSignaling();

            /// <summary>Write data processed from RF to the network.</summary>
            void writeNetwork(const uint8_t* data, uint32_t len);

            /*
            ** Modem Frame Queuing
            */

            /// <summary>Helper to write a immediate single-block RCCH packet.</summary>
            void writeRF_Message_Imm(lc::RCCH *rcch, bool noNetwork) { writeRF_Message(rcch, noNetwork, true); }
            /// <summary>Helper to write a single-block RCCH packet.</summary>
            void writeRF_Message(lc::RCCH* rcch, bool noNetwork, bool imm = false);

            /*
            ** Control Signalling Logic
            */

            /// <summary>Helper to write control channel packet data.</summary>
            void writeRF_ControlData(uint8_t frameCnt, uint8_t n, bool adjSS);

            /// <summary>Helper to write a grant packet.</summary>
            bool writeRF_Message_Grant(uint32_t srcId, uint32_t dstId, uint8_t serviceOptions, bool grp, bool net = false, bool skip = false, uint32_t chNo = 0U);
            /// <summary>Helper to write a deny packet.</summary>
            void writeRF_Message_Deny(uint32_t srcId, uint32_t dstId, uint8_t reason, uint8_t service);
            /// <summary>Helper to write a group registration response packet.</summary>
            bool writeRF_Message_Grp_Reg_Rsp(uint32_t srcId, uint32_t dstId, uint32_t locId);
            /// <summary>Helper to write a unit registration response packet.</summary>
            void writeRF_Message_U_Reg_Rsp(uint32_t srcId, uint32_t locId);

            /// <summary>Helper to write a CC SITE_INFO broadcast packet on the RF interface.</summary>
            void writeRF_CC_Message_Site_Info();
            /// <summary>Helper to write a CC SRV_INFO broadcast packet on the RF interface.</summary>
            void writeRF_CC_Message_Service_Info();
        };
    } // namespace packet
} // namespace nxdn

#endif // __NXDN_PACKET_CONTROL_SIGNALING_H__
