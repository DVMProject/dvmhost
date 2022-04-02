/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
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
#if !defined(__P25_DFSI_TRUNK_PACKET_H__)
#define __P25_DFSI_TRUNK_PACKET_H__

#include "Defines.h"
#include "p25/dfsi/LC.h"
#include "p25/TrunkPacket.h"
#include "p25/Control.h"
#include "network/BaseNetwork.h"

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------
    class HOST_SW_API TrunkPacket;
    class HOST_SW_API Control;

    namespace dfsi
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      This class implements handling logic for P25 trunking packets using
        //      the DFSI protocol instead of the P25 OTA protocol.
        // ---------------------------------------------------------------------------

        class HOST_SW_API DFSITrunkPacket : public TrunkPacket {
        public:
            /// <summary>Resets the data states for the RF interface.</summary>
            virtual void resetRF();
            /// <summary>Resets the data states for the network.</summary>
            virtual void resetNet();

            /// <summary>Process a data frame from the RF interface.</summary>
            virtual bool process(uint8_t* data, uint32_t len, bool preDecoded = false);

        protected:
            LC m_rfDFSILC;
            LC m_netDFSILC;

            /// <summary>Initializes a new instance of the DFSITrunkPacket class.</summary>
            DFSITrunkPacket(Control* p25, network::BaseNetwork* network, bool dumpTSBKData, bool debug, bool verbose);
            /// <summary>Finalizes a instance of the DFSITrunkPacket class.</summary>
            virtual ~DFSITrunkPacket();

            /// <summary>Helper to write a P25 TDU w/ link control packet.</summary>
            virtual void writeRF_TDULC(lc::TDULC lc, bool noNetwork);

            /// <summary>Helper to write a single-block P25 TSDU packet.</summary>
            virtual void writeRF_TSDU_SBF(bool noNetwork, bool clearBeforeWrite = false, bool force = false);

            /// <summary>Helper to write a network P25 TDU w/ link control packet.</summary>
            //virtual void writeNet_TDULC(lc::TDULC lc);
            /// <summary>Helper to write a network single-block P25 TSDU packet.</summary>
            virtual void writeNet_TSDU();

            /// <suimmary>Helper to write start DFSI data.</summary>
            void writeRF_DFSI_Start(uint8_t type);
            /// <suimmary>Helper to write stop DFSI data.</summary>
            void writeRF_DSFI_Stop(uint8_t type);

        private:
            friend class DFSIVoicePacket;
            friend class p25::Control;
        };
    } // namespace dfsi
} // namespace p25

#endif // __P25_DFSI_TRUNK_PACKET_H__
