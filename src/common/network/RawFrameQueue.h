/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
*
*/
/*
*   Copyright (C) 2024 by Bryan Biedenkapp N2PLL
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
#if !defined(__RAW_FRAME_QUEUE_H__)
#define __RAW_FRAME_QUEUE_H__

#include "common/Defines.h"
#include "common/network/UDPSocket.h"
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
        /// <summary>Initializes a new instance of the RawFrameQueue class.</summary>
        RawFrameQueue(UDPSocket* socket, bool debug);
        /// <summary>Finalizes a instance of the RawFrameQueue class.</summary>
        virtual ~RawFrameQueue();

        /// <summary>Read message from the received UDP packet.</summary>
        UInt8Array read(int& messageLength, sockaddr_storage& address, uint32_t& addrLen);

        /// <summary>Cache "message" to frame queue.</summary>
        void enqueueMessage(const uint8_t* message, uint32_t length, sockaddr_storage& addr, uint32_t addrLen);

        /// <summary>Flush the message queue.</summary>
        bool flushQueue();

    protected:
        sockaddr_storage m_addr;
        uint32_t m_addrLen;
        UDPSocket* m_socket;

        BufferVector m_buffers;

        bool m_debug;

    private:
        /// <summary>Helper to ensure buffers are deleted.</summary>
        void deleteBuffers();
    };
} // namespace network

#endif // __FRAME_QUEUE_H__