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
*   Copyright (C) 2016,2017 by Jonathan Naylor G4KLX
*   Copyright (C) 2017-2022 by Bryan Biedenkapp N2PLL
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
#if !defined(__P25_VOICE_PACKET_H__)
#define __P25_VOICE_PACKET_H__

#include "Defines.h"
#include "p25/data/LowSpeedData.h"
#include "p25/dfsi/LC.h"
#include "p25/lc/LC.h"
#include "p25/Control.h"
#include "p25/Audio.h"
#include "network/BaseNetwork.h"

#include <cstdio>
#include <string>

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------
    class HOST_SW_API TrunkPacket;
    class HOST_SW_API Control;

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      This class implements handling logic for P25 voice packets.
    // ---------------------------------------------------------------------------

    class HOST_SW_API VoicePacket {
    public:
        /// <summary>Resets the data states for the RF interface.</summary>
        virtual void resetRF();
        /// <summary>Resets the data states for the network.</summary>
        virtual void resetNet();

        /// <summary>Process a data frame from the RF interface.</summary>
        virtual bool process(uint8_t* data, uint32_t len);
        /// <summary>Process a data frame from the network.</summary>
        virtual bool processNetwork(uint8_t* data, uint32_t len, lc::LC& control, data::LowSpeedData& lsd, uint8_t& duid);

        /// <summary>Helper to write end of frame data.</summary>
        bool writeEndRF();

    protected:
        friend class TrunkPacket;
        friend class Control;
        Control* m_p25;

        network::BaseNetwork* m_network;

        uint32_t m_rfFrames;
        uint32_t m_rfBits;
        uint32_t m_rfErrs;
        uint32_t m_rfUndecodableLC;
        uint32_t m_netFrames;
        uint32_t m_netLost;

        Audio m_audio;

        lc::LC m_rfLC;
        lc::LC m_rfLastHDU;
        lc::LC m_rfLastLDU1;
        lc::LC m_rfLastLDU2;

        lc::LC m_netLC;
        lc::LC m_netLastLDU1;

        data::LowSpeedData m_rfLSD;
        data::LowSpeedData m_netLSD;

        dfsi::LC m_dfsiLC;
        uint8_t* m_netLDU1;
        uint8_t* m_netLDU2;

        uint8_t m_lastDUID;
        uint8_t* m_lastIMBE;

        bool m_hadVoice;
        uint32_t m_lastRejectId;

        uint32_t m_lastPatchGroup;

        uint32_t m_silenceThreshold;

        uint8_t m_vocLDU1Count;

        bool m_verbose;
        bool m_debug;

        /// <summary>Initializes a new instance of the VoicePacket class.</summary>
        VoicePacket(Control* p25, network::BaseNetwork* network, bool debug, bool verbose);
        /// <summary>Finalizes a instance of the VoicePacket class.</summary>
        virtual ~VoicePacket();

        /// <summary>Write data processed from RF to the network.</summary>
        void writeNetworkRF(const uint8_t* data, uint8_t duid);

        /// <summary>Helper to write end of voice frame data.</summary>
        void writeRF_EndOfVoice();

        /// <summary>Helper to write a network P25 TDU packet.</summary>
        virtual void writeNet_TDU();
        /// <summary>Helper to check for an unflushed LDU1 packet.</summary>
        void checkNet_LDU1();
        /// <summary>Helper to write a network P25 LDU1 packet.</summary>
        virtual void writeNet_LDU1();
        /// <summary>Helper to check for an unflushed LDU2 packet.</summary>
        void checkNet_LDU2();
        /// <summary>Helper to write a network P25 LDU1 packet.</summary>
        virtual void writeNet_LDU2();

        /// <summary>Helper to insert IMBE silence frames for missing audio.</summary>
        void insertMissingAudio(uint8_t* data);
        /// <summary>Helper to insert IMBE null frames for missing audio.</summary>
        void insertNullAudio(uint8_t* data);
    };
} // namespace p25

#endif // __P25_VOICE_PACKET_H__
