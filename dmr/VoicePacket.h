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
*   Copyright (C) 2017-2021 by Bryan Biedenkapp N2PLL
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
#if !defined(__DMR_VOICE_PACKET_H__)
#define __DMR_VOICE_PACKET_H__

#include "Defines.h"
#include "dmr/data/Data.h"
#include "dmr/data/EmbeddedData.h"
#include "dmr/lc/LC.h"
#include "dmr/lc/PrivacyLC.h"
#include "dmr/Slot.h"
#include "edac/AMBEFEC.h"
#include "network/BaseNetwork.h"
#include "lookups/RadioIdLookup.h"
#include "lookups/TalkgroupIdLookup.h"

#include <vector>

namespace dmr
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------
    class HOST_SW_API DataPacket;
    class HOST_SW_API Slot;

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      This class implements core logic for handling DMR voice packets.
    // ---------------------------------------------------------------------------

    class HOST_SW_API VoicePacket {
    public:
        /// <summary>Process a voice frame from the RF interface.</summary>
        bool process(uint8_t* data, uint32_t len);
        /// <summary>Process a voice frame from the network.</summary>
        void processNetwork(const data::Data& dmrData);

    private:
        friend class DataPacket;
        friend class Slot;
        Slot* m_slot;

        uint8_t* m_lastFrame;
        bool m_lastFrameValid;

        uint8_t m_rfN;
        uint8_t m_lastRfN;
        uint8_t m_netN;

        data::EmbeddedData m_rfEmbeddedLC;
        data::EmbeddedData* m_rfEmbeddedData;
        uint32_t m_rfEmbeddedReadN;
        uint32_t m_rfEmbeddedWriteN;

        data::EmbeddedData m_netEmbeddedLC;
        data::EmbeddedData* m_netEmbeddedData;
        uint32_t m_netEmbeddedReadN;
        uint32_t m_netEmbeddedWriteN;

        uint8_t m_rfTalkerId;
        uint8_t m_netTalkerId;

        edac::AMBEFEC m_fec;

        bool m_embeddedLCOnly;
        bool m_dumpTAData;

        bool m_verbose;
        bool m_debug;

        /// <summary>Initializes a new instance of the VoicePacket class.</summary>
        VoicePacket(Slot* slot, network::BaseNetwork* network, bool embeddedLCOnly, bool dumpTAData, bool debug, bool verbose);
        /// <summary>Finalizes a instance of the VoicePacket class.</summary>
        ~VoicePacket();

        /// <summary></summary>
        void logGPSPosition(const uint32_t srcId, const uint8_t* data);

        /// <summary>Helper to insert AMBE null frames for missing audio.</summary>
        void insertNullAudio(uint8_t* data);
        /// <summary>Helper to insert DMR AMBE silence frames.</summary>
        bool insertSilence(const uint8_t* data, uint8_t seqNo);
        /// <summary>Helper to insert DMR AMBE silence frames.</summary>
        void insertSilence(uint32_t count);
    };
} // namespace dmr

#endif // __DMR_VOICE_PACKET_H__
