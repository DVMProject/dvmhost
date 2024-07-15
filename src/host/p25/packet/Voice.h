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
*  Copyright (C) 2016,2017 Jonathan Naylor, G4KLX
*  Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
*
*/
/**
 * @file Voice.h
 * @ingroup host_p25
 * @file Voice.cpp
 * @ingroup host_p25
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
        // ---------------------------------------------------------------------------

        /**
         * @brief This class implements handling logic for P25 voice packets.
         * @ingroup host_p25
         */
        class HOST_SW_API Voice {
        public:
            /**
             * @brief Resets the data states for the RF interface.
             */
            void resetRF();
            /**
             * @brief Resets the data states for the network.
             */
            void resetNet();

            /** @name Frame Processing */
            /**
             * @brief Process a data frame from the RF interface.
             * @param data Buffer containing data frame.
             * @param len Length of data frame.
             * @returns bool True, if data frame is processed, otherwise false.
             */
            bool process(uint8_t* data, uint32_t len);
            /**
             * @brief Process a data frame from the network.
             * @param data Buffer containing data frame.
             * @param len Length of data frame.
             * @param control Link Control Data.
             * @param lsd Low Speed Data.
             * @param duid Data Unit ID.
             * @param frameType Network Frame Type.
             * @returns bool True, if data frame is processed, otherwise false.
             */
            bool processNetwork(uint8_t* data, uint32_t len, lc::LC& control, data::LowSpeedData& lsd, defines::DUID::E& duid, defines::FrameType::E& frameType);
            /** @} */

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
            bool m_rfLastHDUValid;
            lc::LC m_rfLastLDU1;
            lc::LC m_rfLastLDU2;
            bool m_rfFirstLDU2;

            lc::LC m_netLC;
            lc::LC m_netLastLDU1;
            defines::FrameType::E m_netLastFrameType;

            data::LowSpeedData m_rfLSD;
            data::LowSpeedData m_netLSD;

            dfsi::LC m_dfsiLC;
            uint8_t* m_netLDU1;
            uint8_t* m_netLDU2;

            defines::DUID::E m_lastDUID;
            uint8_t* m_lastIMBE;
            uint8_t* m_lastMI;

            bool m_hadVoice;
            uint32_t m_lastRejectId;

            uint32_t m_silenceThreshold;

            uint8_t m_vocLDU1Count;
            uint8_t m_roamLDU1Count;

            bool m_inbound;

            bool m_verbose;
            bool m_debug;

            /**
             * @brief Initializes a new instance of the Voice class.
             * @param p25 Instance of the Control class.
             * @param debug Flag indicating whether P25 debug is enabled.
             * @param verbose Flag indicating whether P25 verbose logging is enabled.
             */
            Voice(Control* p25, bool debug, bool verbose);
            /**
             * @brief Finalizes a instance of the Voice class.
             */
            ~Voice();

            /**
             * @brief Write data processed from RF to the network.
             * @param[in] data Buffer to write to the network.
             * @param duid DUID.
             * @param frameType Frame Type.
             */
            void writeNetwork(const uint8_t* data, defines::DUID::E duid, defines::FrameType::E frameType = defines::FrameType::DATA_UNIT);

            /**
             * @brief Helper to write end of voice frame data.
             */
            void writeRF_EndOfVoice();

            /**
             * @brief Helper to write a network P25 TDU packet.
             */
            void writeNet_TDU();
            /**
             * @brief Helper to check for an unflushed LDU1 packet.
             */
            void checkNet_LDU1();
            /**
             * @brief Helper to write a network P25 LDU1 packet.
             */
            void writeNet_LDU1();
            /**
             * @brief Helper to check for an unflushed LDU2 packet.
             */
            void checkNet_LDU2();
            /**
             * @brief Helper to write a network P25 LDU1 packet.
             */
            void writeNet_LDU2();

            /**
             * @brief Helper to insert IMBE silence frames for missing audio.
             * @param data Buffer containing frame data.
             */
            void insertMissingAudio(uint8_t* data);
            /**
             * @brief Helper to insert IMBE null frames for missing audio.
             * @param data Buffer containing frame data.
             */
            void insertNullAudio(uint8_t* data);
            /**
             * @brief Helper to insert encrypted IMBE null frames for missing audio.
             * @param data Buffer containing frame data.
             */
            void insertEncryptedNullAudio(uint8_t* data);
            /**
             * @brief Given the last MI, generate the next MI using LFSR.
             * @param lastMI Last MI received.
             * @param nextMI Next MI.
             */
            void getNextMI(uint8_t lastMI[9U], uint8_t nextMI[9U]);
        };
    } // namespace packet
} // namespace p25

#endif // __P25_PACKET_VOICE_H__
