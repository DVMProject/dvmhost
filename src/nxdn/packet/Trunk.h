/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#if !defined(__NXDN_PACKET_TRUNK_H__)
#define __NXDN_PACKET_TRUNK_H__

#include "Defines.h"
#include "nxdn/Control.h"
#include "nxdn/lc/RCCH.h"
#include "network/BaseNetwork.h"

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
        //      This class implements handling logic for NXDN control signalling packets.
        // ---------------------------------------------------------------------------

        class HOST_SW_API Trunk {
        public:
            /// <summary>Process a data frame from the RF interface.</summary>
            virtual bool process(uint8_t fct, uint8_t option, uint8_t* data, uint32_t len);
            /// <summary>Process a data frame from the network.</summary>
            virtual bool processNetwork(uint8_t fct, uint8_t option, lc::RTCH& netLC, uint8_t* data, uint32_t len);

            /// <summary>Updates the processor by the passed number of milliseconds.</summary>
            void clock(uint32_t ms);

        protected:
            friend class nxdn::packet::Data;
            friend class nxdn::packet::Voice;
            friend class nxdn::Control;
            Control* m_nxdn;

            network::BaseNetwork* m_network;

            uint8_t m_bcchCnt;
            uint8_t m_rcchGroupingCnt;
            uint8_t m_ccchPagingCnt;
            uint8_t m_ccchMultiCnt;
            uint8_t m_rcchIterateCnt;

            bool m_verifyAff;
            bool m_verifyReg;

            uint16_t m_lastRejectId;

            bool m_verbose;
            bool m_debug;

            /// <summary>Initializes a new instance of the Trunk class.</summary>
            Trunk(Control* nxdn, network::BaseNetwork* network, bool debug, bool verbose);
            /// <summary>Finalizes a instance of the Trunk class.</summary>
            virtual ~Trunk();

            /// <summary>Write data processed from RF to the network.</summary>
            void writeNetwork(const uint8_t* data, uint32_t len);

            /// <summary>Helper to write control channel packet data.</summary>
            void writeRF_ControlData(uint8_t frameCnt, uint8_t n, bool adjSS);

            /// <summary>Helper to write a single-block RCCH packet.</summary>
            void writeRF_Message(lc::RCCH* rcch, bool noNetwork, bool clearBeforeWrite = false);

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

            /// <summary>Helper to add the post field bits on NXDN frame data.</summary>
            void addPostBits(uint8_t* data);
        };
    } // namespace packet
} // namespace nxdn

#endif // __NXDN_PACKET_TRUNK_H__
