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
#if !defined(__CALLHANDLER__TAG_DMR_DATA_H__)
#define __CALLHANDLER__TAG_DMR_DATA_H__

#include "fne/Defines.h"
#include "common/dmr/DMRDefines.h"
#include "common/dmr/data/Data.h"
#include "common/dmr/lc/CSBK.h"
#include "common/Clock.h"
#include "network/FNENetwork.h"

#include <deque>

namespace network
{
    namespace callhandler
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements the DMR call handler and data FNE networking logic.
        // ---------------------------------------------------------------------------

        class HOST_SW_API TagDMRData {
        public:
            /// <summary>Initializes a new instance of the TagDMRData class.</summary>
            TagDMRData(FNENetwork* network, bool debug);
            /// <summary>Finalizes a instance of the TagDMRData class.</summary>
            ~TagDMRData();

            /// <summary>Process a data frame from the network.</summary>
            bool processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint16_t pktSeq, uint32_t streamId, bool external = false);
            /// <summary>Process a grant request frame from the network.</summary>
            bool processGrantReq(uint32_t srcId, uint32_t dstId, uint8_t slot, bool unitToUnit, uint32_t peerId, uint16_t pktSeq, uint32_t streamId);

            /// <summary>Helper to playback a parrot frame to the network.</summary>
            void playbackParrot();
            /// <summary>Helper to determine if there are stored parrot frames.</summary>
            bool hasParrotFrames() const { return m_parrotFramesReady && !m_parrotFrames.empty(); }

            /// <summary>Helper to write a extended function packet on the RF interface.</summary>
            void write_Ext_Func(uint32_t peerId, uint8_t slot, uint32_t func, uint32_t arg, uint32_t dstId);
            /// <summary>Helper to write a call alert packet on the RF interface.</summary>
            void write_Call_Alrt(uint32_t peerId, uint8_t slot, uint32_t srcId, uint32_t dstId);

        private:
            FNENetwork* m_network;

            class ParrotFrame {
            public:
                uint8_t* buffer;
                uint32_t bufferLen;

                uint8_t slotNo;

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
                uint8_t slotNo;
                uint32_t streamId;
                uint32_t peerId;
            };
            typedef std::pair<const uint32_t, RxStatus> StatusMapPair;
            std::unordered_map<uint32_t, RxStatus> m_status;

            bool m_debug;

            /// <summary>Helper to route rewrite the network data buffer.</summary>
            void routeRewrite(uint8_t* buffer, uint32_t peerId, dmr::data::Data& dmrData, uint8_t dataType, uint32_t dstId, uint32_t slotNo, bool outbound = true);
            /// <summary>Helper to route rewrite destination ID and slot.</summary>
            bool peerRewrite(uint32_t peerId, uint32_t& dstId, uint32_t& slotNo, bool outbound = true);

            /// <summary>Helper to process CSBKs being passed from a peer.</summary>
            bool processCSBK(uint8_t* buffer, uint32_t peerId, dmr::data::Data& dmrData);

            /// <summary>Helper to determine if the peer is permitted for traffic.</summary>
            bool isPeerPermitted(uint32_t peerId, dmr::data::Data& data, uint32_t streamId, bool external = false);
            /// <summary>Helper to validate the DMR call stream.</summary>
            bool validate(uint32_t peerId, dmr::data::Data& data, uint32_t streamId);

            /// <summary>Helper to write a NACK RSP packet.</summary>
            void write_CSBK_NACK_RSP(uint32_t peerId, uint32_t dstId, uint8_t reason, uint8_t service);

            /// <summary>Helper to write a network CSBK.</summary>
            void write_CSBK(uint32_t peerId, uint8_t slot, dmr::lc::CSBK* csbk);
        };
    } // namespace callhandler
} // namespace network

#endif // __CALLHANDLER__TAG_DMR_DATA_H__
