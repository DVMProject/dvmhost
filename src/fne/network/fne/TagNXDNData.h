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
#if !defined(__FNE__TAG_NXDN_DATA_H__)
#define __FNE__TAG_NXDN_DATA_H__

#include "fne/Defines.h"
#include "common/Clock.h"
#include "common/nxdn/NXDNDefines.h"
#include "common/nxdn/lc/RTCH.h"
#include "network/FNENetwork.h"

#include <deque>

namespace network
{
    namespace fne
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements the NXDN data FNE networking logic.
        // ---------------------------------------------------------------------------

        class HOST_SW_API TagNXDNData {
        public:
            /// <summary>Initializes a new instance of the TagNXDNData class.</summary>
            TagNXDNData(FNENetwork* network, bool debug);
            /// <summary>Finalizes a instance of the TagNXDNData class.</summary>
            ~TagNXDNData();

            /// <summary>Process a data frame from the network.</summary>
            bool processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint16_t pktSeq, uint32_t streamId, bool fromPeer = false);

            /// <summary>Helper to playback a parrot frame to the network.</summary>
            void playbackParrot();
            /// <summary>Helper to determine if there are stored parrot frames.</summary>
            bool hasParrotFrames() const { return m_parrotFramesReady && !m_parrotFrames.empty(); }

        private:
            FNENetwork* m_network;

            std::deque<std::tuple<uint8_t*, uint32_t, uint16_t, uint32_t>> m_parrotFrames;
            bool m_parrotFramesReady;

            class RxStatus {
            public:
                system_clock::hrc::hrc_t callStartTime;
                uint32_t srcId;
                uint32_t dstId;
                uint32_t streamId;
            };
            typedef std::pair<const uint32_t, RxStatus> StatusMapPair;
            std::unordered_map<uint32_t, RxStatus> m_status;

            bool m_debug;

            /// <summary>Helper to route rewrite the network data buffer.</summary>
            void routeRewrite(uint8_t* buffer, uint32_t peerId, uint8_t messageType, uint32_t dstId, bool outbound = true);
            /// <summary>Helper to route rewrite destination ID.</summary>
            bool peerRewrite(uint32_t peerId, uint32_t& dstId, bool outbound = true);

            /// <summary>Helper to determine if the peer is permitted for traffic.</summary>
            bool isPeerPermitted(uint32_t peerId, nxdn::lc::RTCH& lc, uint8_t messageType, uint32_t streamId);
            /// <summary>Helper to validate the NXDN call stream.</summary>
            bool validate(uint32_t peerId, nxdn::lc::RTCH& control, uint8_t messageType, uint32_t streamId);
        };
    } // namespace fne
} // namespace network

#endif // __FNE__TAG_NXDN_DATA_H__
