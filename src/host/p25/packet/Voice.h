// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Modem Host Software
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2016,2017 Jonathan Naylor, G4KLX
*   Copyright (C) 2017-2022 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__P25_PACKET_VOICE_H__)
#define __P25_PACKET_VOICE_H__

#include "Defines.h"
#include "common/p25/data/LowSpeedData.h"
#include "common/p25/dfsi/LC.h"
#include "common/p25/lc/LC.h"
#include "common/p25/Audio.h"
#include "p25/Control.h"

#include <cstdio>
#include <string>

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    namespace packet { class HOST_SW_API ControlSignaling; }
    class HOST_SW_API Control;

    namespace packet
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      This class implements handling logic for P25 voice packets.
        // ---------------------------------------------------------------------------

        class HOST_SW_API Voice {
        public:
            /// <summary>Resets the data states for the RF interface.</summary>
            void resetRF();
            /// <summary>Resets the data states for the network.</summary>
            void resetNet();

            /// <summary>Process a data frame from the RF interface.</summary>
            bool process(uint8_t* data, uint32_t len);
            /// <summary>Process a data frame from the network.</summary>
            bool processNetwork(uint8_t* data, uint32_t len, lc::LC& control, data::LowSpeedData& lsd, uint8_t& duid, uint8_t& frameType);

        protected:
            friend class packet::ControlSignaling;
            friend class p25::Control;
            Control* m_p25;

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
            uint8_t m_netLastFrameType;

            data::LowSpeedData m_rfLSD;
            data::LowSpeedData m_netLSD;

            dfsi::LC m_dfsiLC;
            uint8_t* m_netLDU1;
            uint8_t* m_netLDU2;

            uint8_t m_lastDUID;
            uint8_t* m_lastIMBE;
            uint8_t* m_lastMI;

            bool m_hadVoice;
            uint32_t m_lastRejectId;

            uint32_t m_silenceThreshold;

            uint8_t m_vocLDU1Count;
            uint8_t m_roamLDU1Count;

            bool m_verbose;
            bool m_debug;

            /// <summary>Initializes a new instance of the Voice class.</summary>
            Voice(Control* p25, bool debug, bool verbose);
            /// <summary>Finalizes a instance of the Voice class.</summary>
            ~Voice();

            /// <summary>Write data processed from RF to the network.</summary>
            void writeNetwork(const uint8_t* data, uint8_t duid, uint8_t frameType = P25_FT_DATA_UNIT);

            /// <summary>Helper to write end of voice frame data.</summary>
            void writeRF_EndOfVoice();

            /// <summary>Helper to write a network P25 TDU packet.</summary>
            void writeNet_TDU();
            /// <summary>Helper to check for an unflushed LDU1 packet.</summary>
            void checkNet_LDU1();
            /// <summary>Helper to write a network P25 LDU1 packet.</summary>
            void writeNet_LDU1();
            /// <summary>Helper to check for an unflushed LDU2 packet.</summary>
            void checkNet_LDU2();
            /// <summary>Helper to write a network P25 LDU1 packet.</summary>
            void writeNet_LDU2();

            /// <summary>Helper to insert IMBE silence frames for missing audio.</summary>
            void insertMissingAudio(uint8_t* data);
            /// <summary>Helper to insert IMBE null frames for missing audio.</summary>
            void insertNullAudio(uint8_t* data);
            /// <summary>Helper to insert encrypted IMBE null frames for missing audio.</summary>
            void insertEncryptedNullAudio(uint8_t* data);
            /// <summary>Given the last MI, generate the next MI using LFSR.</summary>
            void getNextMI(uint8_t lastMI[9U], uint8_t nextMI[9U]);
        };
    } // namespace packet
} // namespace p25

#endif // __P25_PACKET_VOICE_H__
