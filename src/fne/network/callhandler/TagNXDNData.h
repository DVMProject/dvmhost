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
#if !defined(__CALLHANDLER__TAG_NXDN_DATA_H__)
#define __CALLHANDLER__TAG_NXDN_DATA_H__

#include "fne/Defines.h"
#include "common/Clock.h"
#include "common/nxdn/NXDNDefines.h"
#include "common/nxdn/lc/RTCH.h"
#include "common/nxdn/lc/RCCH.h"
#include "network/FNENetwork.h"

#include <deque>

namespace network
{
    namespace callhandler
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements the NXDN call handler and data FNE networking logic.
        // ---------------------------------------------------------------------------

        class HOST_SW_API TagNXDNData {
        public:
            /// <summary>Initializes a new instance of the TagNXDNData class.</summary>
            TagNXDNData(FNENetwork* network, bool debug);
            /// <summary>Finalizes a instance of the TagNXDNData class.</summary>
            ~TagNXDNData();

            /// <summary>Process a data frame from the network.</summary>
            bool processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint16_t pktSeq, uint32_t streamId, bool external = false);
            /// <summary>Process a grant request frame from the network.</summary>
            bool processGrantReq(uint32_t srcId, uint32_t dstId, bool unitToUnit, uint32_t peerId, uint16_t pktSeq, uint32_t streamId);

            /// <summary>Helper to playback a parrot frame to the network.</summary>
            void playbackParrot();
            /// <summary>Helper to determine if there are stored parrot frames.</summary>
            bool hasParrotFrames() const { return m_parrotFramesReady && !m_parrotFrames.empty(); }

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
            void routeRewrite(uint8_t* buffer, uint32_t peerId, uint8_t messageType, uint32_t dstId, bool outbound = true);
            /// <summary>Helper to route rewrite destination ID.</summary>
            bool peerRewrite(uint32_t peerId, uint32_t& dstId, bool outbound = true);

            /// <summary>Helper to determine if the peer is permitted for traffic.</summary>
            bool isPeerPermitted(uint32_t peerId, nxdn::lc::RTCH& lc, uint8_t messageType, uint32_t streamId, bool external = false);
            /// <summary>Helper to validate the NXDN call stream.</summary>
            bool validate(uint32_t peerId, nxdn::lc::RTCH& control, uint8_t messageType, uint32_t streamId);

            /// <summary>Helper to write a grant packet.</summary>
            bool write_Message_Grant(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t serviceOptions, bool grp);
            /// <summary>Helper to write a deny packet.</summary>
            void write_Message_Deny(uint32_t peerId, uint32_t srcId, uint32_t dstId, uint8_t reason, uint8_t service);

            /// <summary>Helper to write a network RCCH.</summary>
            void write_Message(uint32_t peerId, nxdn::lc::RCCH* rcch);
        };
    } // namespace callhandler
} // namespace network

#endif // __CALLHANDLER__TAG_NXDN_DATA_H__
