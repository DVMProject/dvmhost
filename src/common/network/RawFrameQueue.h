// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__RAW_FRAME_QUEUE_H__)
#define __RAW_FRAME_QUEUE_H__

#include "common/Defines.h"
#include "common/network/udp/Socket.h"
#include "common/Utils.h"

namespace network
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    const uint32_t DATA_PACKET_LENGTH = 8192U;

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements the network frame queuing logic.
    // ---------------------------------------------------------------------------

    class HOST_SW_API RawFrameQueue {
    public:
        auto operator=(RawFrameQueue&) -> RawFrameQueue& = delete;
        auto operator=(RawFrameQueue&&) -> RawFrameQueue& = delete;
        RawFrameQueue(RawFrameQueue&) = delete;

        /// <summary>Initializes a new instance of the RawFrameQueue class.</summary>
        RawFrameQueue(udp::Socket* socket, bool debug);
        /// <summary>Finalizes a instance of the RawFrameQueue class.</summary>
        virtual ~RawFrameQueue();

        /// <summary>Read message from the received UDP packet.</summary>
        UInt8Array read(int& messageLength, sockaddr_storage& address, uint32_t& addrLen);
        /// <summary>Write message to the UDP socket.</summary>
        bool write(const uint8_t* message, uint32_t length, sockaddr_storage& addr, uint32_t addrLen);

        /// <summary>Cache message to frame queue.</summary>
        void enqueueMessage(const uint8_t* message, uint32_t length, sockaddr_storage& addr, uint32_t addrLen);

        /// <summary>Flush the message queue.</summary>
        bool flushQueue();

    protected:
        sockaddr_storage m_addr;
        uint32_t m_addrLen;
        udp::Socket* m_socket;

        udp::BufferVector m_buffers;

        bool m_debug;

    private:
        /// <summary>Helper to ensure buffers are deleted.</summary>
        void deleteBuffers();
    };
} // namespace network

#endif // __FRAME_QUEUE_H__