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
*   Copyright (C) 2015,2016,2017 by Jonathan Naylor G4KLX
*   Copyright (C) 2017-2020 by Bryan Biedenkapp N2PLL
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
#if !defined(__DMR_CONTROL_PACKET_H__)
#define __DMR_CONTROL_PACKET_H__

#include "Defines.h"
#include "dmr/data/Data.h"
#include "dmr/data/EmbeddedData.h"
#include "dmr/lc/LC.h"
#include "dmr/Slot.h"
#include "modem/Modem.h"
#include "network/BaseNetwork.h"
#include "RingBuffer.h"
#include "lookups/RadioIdLookup.h"
#include "lookups/TalkgroupIdLookup.h"
#include "StopWatch.h"
#include "Timer.h"

#include <vector>

namespace dmr
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------
    class HOST_SW_API Slot;

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      This class implements core logic for handling DMR control (CSBK) packets.
    // ---------------------------------------------------------------------------

    class HOST_SW_API ControlPacket {
    public:
        /// <summary>Process a data frame from the RF interface.</summary>
        bool process(uint8_t* data, uint32_t len);
        /// <summary>Process a data frame from the network.</summary>
        void processNetwork(const data::Data& dmrData);

        /// <summary>Helper to write a extended function packet on the RF interface.</summary>
        void writeRF_Ext_Func(uint32_t func, uint32_t arg, uint32_t dstId);
        /// <summary>Helper to write a call alert packet on the RF interface.</summary>
        void writeRF_Call_Alrt(uint32_t srcId, uint32_t dstId);

    private:
        friend class Slot;
        Slot* m_slot;

        bool m_dumpCSBKData;
        bool m_verbose;
        bool m_debug;

        /// <summary>Initializes a new instance of the ControlPacket class.</summary>
        ControlPacket(Slot* slot, network::BaseNetwork* network, bool dumpCSBKData, bool debug, bool verbose);
        /// <summary>Finalizes a instance of the DataPacket class.</summary>
        ~ControlPacket();

        /// <summary>Helper to write a TSCC Aloha broadcast packet on the RF interface.</summary>
        void writeRF_TSCC_Aloha();
        /// <summary>Helper to write a TSCC Ann-Wd broadcast packet on the RF interface.</summary>
        void writeRF_TSCC_Bcast_Ann_Wd(uint32_t channelNo, bool annWd);
        /// <summary>Helper to write a TSCC Sys_Parm broadcast packet on the RF interface.</summary>
        void writeRF_TSCC_Bcast_Sys_Parm();
    };
} // namespace dmr

#endif // __DMR_CONTROL_PACKET_H__
