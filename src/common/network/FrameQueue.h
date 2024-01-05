/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
*
*/
/*
*   Copyright (C) 2023 by Bryan Biedenkapp N2PLL
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
#if !defined(__FRAME_QUEUE_H__)
#define __FRAME_QUEUE_H__

#include "common/Defines.h"
#include "common/network/UDPSocket.h"
#include "common/network/RTPHeader.h"
#include "common/network/RTPExtensionHeader.h"
#include "common/network/RTPFNEHeader.h"

namespace network
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    const uint32_t DATA_PACKET_LENGTH = 8192U;
    
    const uint8_t DVM_RTP_PAYLOAD_TYPE = 0x56U;
    const uint8_t DVM_CTRL_RTP_PAYLOAD_TYPE = 0x57U; // these are still RTP, but do not carry stream IDs or sequence data

    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements the network frame queuing logic.
    // ---------------------------------------------------------------------------

    class HOST_SW_API FrameQueue {
    public: typedef std::pair<const uint8_t, const uint8_t> OpcodePair;
    public:
        /// <summary>Initializes a new instance of the FrameQueue class.</summary>
        FrameQueue(UDPSocket* socket, uint32_t peerId, bool debug);
        /// <summary>Finalizes a instance of the FrameQueue class.</summary>
        virtual ~FrameQueue();

        /// <summary>Read message from the received UDP packet.</summary>
        UInt8Array read(int& messageLength, sockaddr_storage& address, uint32_t& addrLen,
                frame::RTPHeader* rtpHeader = nullptr, frame::RTPFNEHeader* fneHeader = nullptr);

        /// <summary>Cache "message" to frame queue.</summary>
        void enqueueMessage(const uint8_t* message, uint32_t length, uint32_t streamId, uint32_t peerId,
            OpcodePair opcode, uint16_t rtpSeq, sockaddr_storage& addr, uint32_t addrLen);
        /// <summary>Cache "message" to frame queue.</summary>
        void enqueueMessage(const uint8_t* message, uint32_t length, uint32_t streamId, uint32_t peerId,
            uint32_t ssrc, OpcodePair opcode, uint16_t rtpSeq, sockaddr_storage& addr, uint32_t addrLen);

        /// <summary>Flush the message queue.</summary>
        bool flushQueue();

    private:
        uint32_t m_peerId;

        sockaddr_storage m_addr;
        uint32_t m_addrLen;
        UDPSocket* m_socket;

        BufferVector m_buffers;

        bool m_debug;
    };
} // namespace network

#endif // __FRAME_QUEUE_H__