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
#if !defined(__DMR_DATA_PACKET_H__)
#define __DMR_DATA_PACKET_H__

#include "Defines.h"
#include "dmr/data/Data.h"
#include "dmr/data/DataHeader.h"
#include "dmr/data/EmbeddedData.h"
#include "dmr/lc/LC.h"
#include "dmr/Slot.h"
#include "edac/AMBEFEC.h"
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
    class HOST_SW_API VoicePacket;
    class HOST_SW_API ControlPacket;
    class HOST_SW_API Slot;

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      This class implements core logic for handling DMR data packets.
    // ---------------------------------------------------------------------------

    class HOST_SW_API DataPacket {
    public:
        /// <summary>Process a data frame from the RF interface.</summary>
        bool process(uint8_t* data, uint32_t len);
        /// <summary>Process a data frame from the network.</summary>
        void processNetwork(const data::Data& dmrData);

    private:
        friend class VoicePacket;
        friend class ControlPacket;
        friend class Slot;
        Slot* m_slot;

        uint8_t* m_pduUserData;
        uint32_t m_pduDataOffset;
        uint32_t m_lastRejectId;

        bool m_dumpDataPacket;
        bool m_repeatDataPacket;

        bool m_verbose;
        bool m_debug;

        /// <summary>Initializes a new instance of the DataPacket class.</summary>
        DataPacket(Slot* slot, network::BaseNetwork* network, bool dumpDataPacket, bool repeatDataPacket, bool debug, bool verbose);
        /// <summary>Finalizes a instance of the DataPacket class.</summary>
        ~DataPacket();
    };
} // namespace dmr

#endif // __DMR_DATA_PACKET_H__
