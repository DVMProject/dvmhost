// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023,2024,2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file FrameQueue.h
 * @ingroup network_core
 * @file FrameQueue.cpp
 * @ingroup network_core
 */
#if !defined(__FRAME_QUEUE_H__)
#define __FRAME_QUEUE_H__

#include "common/Defines.h"
#include "common/concurrent/unordered_map.h"
#include "common/network/RTPHeader.h"
#include "common/network/RTPFNEHeader.h"
#include "common/network/RawFrameQueue.h"

namespace network
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------
    
    const uint8_t DVM_RTP_PAYLOAD_TYPE = 0x56U;

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements the network RTP frame queuing logic.
     * @ingroup network_core
     */
    class HOST_SW_API FrameQueue : public RawFrameQueue {
    public: typedef std::pair<const NET_FUNC::ENUM, const NET_SUBFUNC::ENUM> OpcodePair;
    public:
        auto operator=(FrameQueue&) -> FrameQueue& = delete;
        auto operator=(FrameQueue&&) -> FrameQueue& = delete;
        FrameQueue(FrameQueue&) = delete;

        /**
         * @brief Initializes a new instance of the FrameQueue class.
         * @param socket Local port used to listen for incoming data.
         * @param peerId Unique ID of this modem on the network.
         */
        FrameQueue(udp::Socket* socket, uint32_t peerId, bool debug);

        /**
         * @brief Read message from the received UDP packet.
         * @param[out] messageLength Actual length of message read from packet.
         * @param[out] address IP address data read from.
         * @param[out] addrLen 
         * @param[out] rtpHeader RTP Header.
         * @param[out] fneHeader FNE Header.
         * @returns UInt8Array Buffer containing message read.
         */
        UInt8Array read(int& messageLength, sockaddr_storage& address, uint32_t& addrLen,
                frame::RTPHeader* rtpHeader = nullptr, frame::RTPFNEHeader* fneHeader = nullptr);
        /**
         * @brief Write message to the UDP socket.
         * @param[in] message Message buffer to frame and queue.
         * @param length Length of message.
         * @param streamId Message stream ID.
         * @param peerId Peer ID.
         * @param ssrc RTP SSRC ID.
         * @param opcode Opcode.
         * @param rtpSeq RTP Sequence.
         * @param addr IP address to write data to.
         * @param addrLen 
         * @returns bool True, if message was written, otherwise false.
         */
        bool write(const uint8_t* message, uint32_t length, uint32_t streamId, uint32_t peerId,
            uint32_t ssrc, OpcodePair opcode, uint16_t rtpSeq, sockaddr_storage& addr, uint32_t addrLen);

        /**
         * @brief Cache message to frame queue.
         * @param[in] message Message buffer to frame and queue.
         * @param length Length of message.
         * @param streamId Message stream ID.
         * @param peerId Peer ID.
         * @param opcode Opcode.
         * @param rtpSeq RTP Sequence.
         * @param addr IP address to write data to.
         * @param addrLen 
         */
        void enqueueMessage(const uint8_t* message, uint32_t length, uint32_t streamId, uint32_t peerId,
            OpcodePair opcode, uint16_t rtpSeq, sockaddr_storage& addr, uint32_t addrLen);
        /**
         * @brief Cache message to frame queue.
         * @param[in] message Message buffer to frame and queue.
         * @param length Length of message.
         * @param streamId Message stream ID.
         * @param peerId Peer ID.
         * @param ssrc RTP SSRC ID.
         * @param opcode Opcode.
         * @param rtpSeq RTP Sequence.
         * @param addr IP address to write data to.
         * @param addrLen 
         */
        void enqueueMessage(const uint8_t* message, uint32_t length, uint32_t streamId, uint32_t peerId,
            uint32_t ssrc, OpcodePair opcode, uint16_t rtpSeq, sockaddr_storage& addr, uint32_t addrLen);

        /**
         * @brief Helper method to clear any tracked stream timestamps.
         */
        void clearTimestamps();

    private:
        uint32_t m_peerId;

        std::mutex m_streamTSMtx;
        std::unordered_map<uint32_t, uint32_t> m_streamTimestamps;

        /**
         * @brief Generate RTP message for the frame queue.
         * @param[in] message Message buffer to frame and queue.
         * @param length Length of message.
         * @param streamId Message stream ID.
         * @param peerId Peer ID.
         * @param ssrc RTP SSRC ID.
         * @param opcode Opcode.
         * @param rtpSeq RTP Sequence.
         * @param[out] outBufferLen Length of buffer generated.
         * @returns uint8_t* Buffer containing RTP message.
         */
        uint8_t* generateMessage(const uint8_t* message, uint32_t length, uint32_t streamId, uint32_t peerId,
            uint32_t ssrc, OpcodePair opcode, uint16_t rtpSeq, uint32_t* outBufferLen);
    };
} // namespace network

#endif // __FRAME_QUEUE_H__
