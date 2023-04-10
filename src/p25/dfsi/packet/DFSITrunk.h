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
#if !defined(__P25_DFSI_PACKET_TRUNK_H__)
#define __P25_DFSI_PACKET_TRUNK_H__

#include "Defines.h"
#include "p25/dfsi/LC.h"
#include "p25/Control.h"
#include "network/BaseNetwork.h"

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    namespace packet { class HOST_SW_API Trunk; }
    class HOST_SW_API Control;

    namespace dfsi
    {
        namespace packet
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      This class implements handling logic for P25 trunking packets using
            //      the DFSI protocol instead of the P25 OTA protocol.
            // ---------------------------------------------------------------------------

            class HOST_SW_API DFSITrunk : public p25::packet::Trunk {
            public:
                /// <summary>Process a data frame from the RF interface.</summary>
                virtual bool process(uint8_t* data, uint32_t len, std::unique_ptr<lc::TSBK> preDecodedTSBK = nullptr);

            protected:
                LC m_rfDFSILC;
                LC m_netDFSILC;

                /// <summary>Initializes a new instance of the DFSITrunk class.</summary>
                DFSITrunk(Control* p25, network::BaseNetwork* network, bool dumpTSBKData, bool debug, bool verbose);
                /// <summary>Finalizes a instance of the DFSITrunk class.</summary>
                virtual ~DFSITrunk();

                /// <summary>Helper to write a P25 TDU w/ link control packet.</summary>
                virtual void writeRF_TDULC(lc::TDULC* lc, bool noNetwork);

                /// <summary>Helper to write a single-block P25 TSDU packet.</summary>
                virtual void writeRF_TSDU_SBF(lc::TSBK* tsbk, bool noNetwork, bool clearBeforeWrite = false, bool force = false);
                /// <summary>Helper to write a alternate multi-block trunking PDU packet.</summary>
                virtual void writeRF_TSDU_AMBT(lc::AMBT* ambt, bool clearBeforeWrite = false);

                /// <summary>Helper to write a network P25 TDU w/ link control packet.</summary>
                //virtual void writeNet_TDULC(lc::TDULC lc);
                /// <summary>Helper to write a network single-block P25 TSDU packet.</summary>
                virtual void writeNet_TSDU(lc::TSBK* tsbk);

                /// <suimmary>Helper to write start DFSI data.</summary>
                void writeRF_DFSI_Start(uint8_t type);
                /// <suimmary>Helper to write stop DFSI data.</summary>
                void writeRF_DSFI_Stop(uint8_t type);

            private:
                friend class packet::DFSIVoice;
                friend class p25::Control;
            };
        } // namespace packet
    } // namespace dfsi
} // namespace p25

#endif // __P25_DFSI_PACKET_TRUNK_H__
