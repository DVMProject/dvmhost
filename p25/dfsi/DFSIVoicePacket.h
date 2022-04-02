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
#if !defined(__P25_DFSI_VOICE_PACKET_H__)
#define __P25_DFSI_VOICE_PACKET_H__

#include "Defines.h"
#include "p25/dfsi/LC.h"
#include "p25/dfsi/DFSITrunkPacket.h"
#include "p25/TrunkPacket.h"
#include "p25/Control.h"
#include "network/BaseNetwork.h"

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------
    class HOST_SW_API VoicePacket;
    class HOST_SW_API Control;

    namespace dfsi
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      This class implements handling logic for P25 voice packets using
        //      the DFSI protocol instead of the P25 OTA protocol.
        // ---------------------------------------------------------------------------

        class HOST_SW_API DFSIVoicePacket : public VoicePacket {
        public:
            /// <summary>Resets the data states for the RF interface.</summary>
            virtual void resetRF();
            /// <summary>Resets the data states for the network.</summary>
            virtual void resetNet();

            /// <summary>Process a data frame from the RF interface.</summary>
            virtual bool process(uint8_t* data, uint32_t len);
            /// <summary>Process a data frame from the network.</summary>
            virtual bool processNetwork(uint8_t* data, uint32_t len, lc::LC& control, data::LowSpeedData& lsd, uint8_t& duid);

        protected:
            DFSITrunkPacket* m_trunk;

            LC m_rfDFSILC;
            LC m_netDFSILC;

            uint8_t* m_dfsiLDU1;
            uint8_t* m_dfsiLDU2;

            /// <summary>Initializes a new instance of the VoicePacket class.</summary>
            DFSIVoicePacket(Control* p25, network::BaseNetwork* network, bool debug, bool verbose);
            /// <summary>Finalizes a instance of the VoicePacket class.</summary>
            virtual ~DFSIVoicePacket();

            /// <summary>Helper to write a network P25 TDU packet.</summary>
            virtual void writeNet_TDU();
            /// <summary>Helper to write a network P25 LDU1 packet.</summary>
            virtual void writeNet_LDU1();
            /// <summary>Helper to write a network P25 LDU1 packet.</summary>
            virtual void writeNet_LDU2();

        private:
            friend class DFSITrunkPacket;
            friend class p25::Control;
        };
    } // namespace dfsi
} // namespace p25

#endif // __P25_DFSI_VOICE_PACKET_H__
