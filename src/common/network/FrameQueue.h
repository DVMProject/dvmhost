// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023,2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__FRAME_QUEUE_H__)
#define __FRAME_QUEUE_H__

#include "common/Defines.h"
#include "common/network/RTPHeader.h"
#include "common/network/RTPFNEHeader.h"
#include "common/network/RawFrameQueue.h"

#include <unordered_map>

namespace network
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------
    
    const uint8_t DVM_RTP_PAYLOAD_TYPE = 0x56U;

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements the network RTP frame queuing logic.
    // ---------------------------------------------------------------------------

    class HOST_SW_API FrameQueue : public RawFrameQueue {
    public: typedef std::pair<const NET_FUNC::ENUM, const NET_SUBFUNC::ENUM> OpcodePair;
    public:
        auto operator=(FrameQueue&) -> FrameQueue& = delete;
        auto operator=(FrameQueue&&) -> FrameQueue& = delete;
        FrameQueue(FrameQueue&) = delete;

        /// <summary>Initializes a new instance of the FrameQueue class.</summary>
        FrameQueue(udp::Socket* socket, uint32_t peerId, bool debug);

        /// <summary>Read message from the received UDP packet.</summary>
        UInt8Array read(int& messageLength, sockaddr_storage& address, uint32_t& addrLen,
                frame::RTPHeader* rtpHeader = nullptr, frame::RTPFNEHeader* fneHeader = nullptr);
        /// <summary>Write message to the UDP socket.</summary>
        bool write(const uint8_t* message, uint32_t length, uint32_t streamId, uint32_t peerId,
            uint32_t ssrc, OpcodePair opcode, uint16_t rtpSeq, sockaddr_storage& addr, uint32_t addrLen);

        /// <summary>Cache message to frame queue.</summary>
        void enqueueMessage(const uint8_t* message, uint32_t length, uint32_t streamId, uint32_t peerId,
            OpcodePair opcode, uint16_t rtpSeq, sockaddr_storage& addr, uint32_t addrLen);
        /// <summary>Cache message to frame queue.</summary>
        void enqueueMessage(const uint8_t* message, uint32_t length, uint32_t streamId, uint32_t peerId,
            uint32_t ssrc, OpcodePair opcode, uint16_t rtpSeq, sockaddr_storage& addr, uint32_t addrLen);

        /// <summary>Helper method to clear any tracked stream timestamps.</summary>
        void clearTimestamps();

    private:
        uint32_t m_peerId;
        std::unordered_map<uint32_t, uint32_t> m_streamTimestamps;

        /// <summary>Generate RTP message for the frame queue.</summary>
        uint8_t* generateMessage(const uint8_t* message, uint32_t length, uint32_t streamId, uint32_t peerId,
            uint32_t ssrc, OpcodePair opcode, uint16_t rtpSeq, uint32_t* outBufferLen);
    };
} // namespace network

#endif // __FRAME_QUEUE_H__
