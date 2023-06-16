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
#if !defined(__P25_DFSI_PACKET_VOICE_H__)
#define __P25_DFSI_PACKET_VOICE_H__

#include "Defines.h"
#include "p25/dfsi/LC.h"
#include "p25/dfsi/packet/DFSITrunk.h"
#include "p25/Control.h"
#include "network/BaseNetwork.h"

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    namespace packet { class HOST_SW_API Voice; }
    class HOST_SW_API Control;

    namespace dfsi
    {
        namespace packet
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      This class implements handling logic for P25 voice packets using
            //      the DFSI protocol instead of the P25 OTA protocol.
            // ---------------------------------------------------------------------------

            class HOST_SW_API DFSIVoice : public p25::packet::Voice {
            public:
                /// <summary>Resets the data states for the RF interface.</summary>
                void resetRF() override;
                /// <summary>Resets the data states for the network.</summary>
                void resetNet() override;

                /// <summary>Process a data frame from the RF interface.</summary>
                bool process(uint8_t* data, uint32_t len) override;
                /// <summary>Process a data frame from the network.</summary>
                bool processNetwork(uint8_t* data, uint32_t len, lc::LC& control, data::LowSpeedData& lsd, uint8_t& duid, uint8_t& frameType) override;

            protected:
                DFSITrunk* m_trunk;

                LC m_rfDFSILC;
                LC m_netDFSILC;

                uint8_t* m_dfsiLDU1;
                uint8_t* m_dfsiLDU2;

                /// <summary>Initializes a new instance of the DFSIVoice class.</summary>
                DFSIVoice(Control* p25, network::BaseNetwork* network, bool debug, bool verbose);
                /// <summary>Finalizes a instance of the DFSIVoice class.</summary>
                virtual ~DFSIVoice();

                /// <summary>Helper to write a network P25 TDU packet.</summary>
                void writeNet_TDU() override;
                /// <summary>Helper to write a network P25 LDU1 packet.</summary>
                void writeNet_LDU1() override;
                /// <summary>Helper to write a network P25 LDU1 packet.</summary>
                void writeNet_LDU2() override;

            private:
                friend class packet::DFSITrunk;
                friend class p25::Control;
            };
        } // namespace packet
    } // namespace dfsi
} // namespace p25

#endif // __P25_DFSI_PACKET_VOICE_H__
