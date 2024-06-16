// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Converged FNE Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Converged FNE Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023-2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__CALLHANDLER__TAG_P25_DATA_H__)
#define __CALLHANDLER__TAG_P25_DATA_H__

#include "fne/Defines.h"
#include "common/Clock.h"
#include "common/p25/P25Defines.h"
#include "common/p25/data/DataHeader.h"
#include "common/p25/data/LowSpeedData.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/p25/dfsi/LC.h"
#include "common/p25/lc/LC.h"
#include "common/p25/lc/TSBK.h"
#include "common/p25/lc/TDULC.h"
#include "network/FNENetwork.h"

#include <deque>

namespace network
{
    namespace callhandler
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements the P25 call handler and data FNE networking logic.
        // ---------------------------------------------------------------------------

        class HOST_SW_API TagP25Data {
        public:
            /// <summary>Initializes a new instance of the TagP25Data class.</summary>
            TagP25Data(FNENetwork* network, bool debug);
            /// <summary>Finalizes a instance of the TagP25Data class.</summary>
            ~TagP25Data();

            /// <summary>Process a data frame from the network.</summary>
            bool processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint16_t pktSeq, uint32_t streamId, bool external = false);
            /// <summary>Process a grant request frame from the network.</summary>
            bool processGrantReq(uint32_t srcId, uint32_t dstId, bool unitToUnit, uint32_t peerId, uint16_t pktSeq, uint32_t streamId);

            /// <summary>Helper to playback a parrot frame to the network.</summary>
            void playbackParrot();
            /// <summary>Helper to determine if there are stored parrot frames.</summary>
            bool hasParrotFrames() const { return m_parrotFramesReady && !m_parrotFrames.empty(); }

            /// <summary>Helper to write a call alert packet.</summary>
            void write_TSDU_Call_Alrt(uint32_t peerId, uint32_t srcId, uint32_t dstId);
            /// <summary>Helper to write a radio monitor packet.</summary>
            void write_TSDU_Radio_Mon(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t txMult);
            /// <summary>Helper to write a extended function packet.</summary>
            void write_TSDU_Ext_Func(uint32_t peerId, uint32_t func, uint32_t arg, uint32_t dstId);
            /// <summary>Helper to write a group affiliation query packet.</summary>
            void write_TSDU_Grp_Aff_Q(uint32_t peerId, uint32_t dstId);
            /// <summary>Helper to write a unit registration command packet.</summary>
            void write_TSDU_U_Reg_Cmd(uint32_t peerId, uint32_t dstId);

        private:
            FNENetwork* m_network;

            class ParrotFrame {
            public:
                uint8_t* buffer;
                uint32_t bufferLen;

                uint16_t pktSeq;
                uint32_t streamId;
                uint32_t peerId;

                uint32_t srcId;
                uint32_t dstId;
            };
            std::deque<ParrotFrame> m_parrotFrames;
            bool m_parrotFramesReady;
            bool m_parrotFirstFrame;

            class RxStatus {
            public:
                system_clock::hrc::hrc_t callStartTime;
                uint32_t srcId;
                uint32_t dstId;
                uint32_t streamId;
                uint32_t peerId;
            };
            typedef std::pair<const uint32_t, RxStatus> StatusMapPair;
            std::unordered_map<uint32_t, RxStatus> m_status;

            bool m_debug;

            /// <summary>Helper to route rewrite the network data buffer.</summary>
            void routeRewrite(uint8_t* buffer, uint32_t peerId, uint8_t duid, uint32_t dstId, bool outbound = true);
            /// <summary>Helper to route rewrite destination ID.</summary>
            bool peerRewrite(uint32_t peerId, uint32_t& dstId, bool outbound = true);

            /// <summary>Helper to process TSDUs being passed from a peer.</summary>
            bool processTSDUFrom(uint8_t* buffer, uint32_t peerId, uint8_t duid);
            /// <summary>Helper to process TSDUs being passed to a peer.</summary>
            bool processTSDUTo(uint8_t* buffer, uint32_t peerId, uint8_t duid);
            /// <summary>Helper to process TSDUs being passed to an external peer.</summary>
            bool processTSDUToExternal(uint8_t* buffer, uint32_t srcPeerId, uint32_t dstPeerId, uint8_t duid);

            /// <summary>Helper to determine if the peer is permitted for traffic.</summary>
            bool isPeerPermitted(uint32_t peerId, p25::lc::LC& control, uint8_t duid, uint32_t streamId, bool external = false);
            /// <summary>Helper to validate the P25 call stream.</summary>
            bool validate(uint32_t peerId, p25::lc::LC& control, uint8_t duid, const p25::lc::TSBK* tsbk, uint32_t streamId);

            /// <summary>Helper to write a deny packet.</summary>
            void write_TSDU_Deny(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t reason, uint8_t service, bool group = false, bool aiv = false);
            /// <summary>Helper to write a queue packet.</summary>
            void write_TSDU_Queue(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t reason, uint8_t service, bool group = false, bool aiv = false);

            /// <summary>Helper to write a network TSDU.</summary>
            void write_TSDU(uint32_t peerId, p25::lc::TSBK* tsbk);
        };
    } // namespace callhandler
} // namespace network

#endif // __CALLHANDLER__TAG_P25_DATA_H__
