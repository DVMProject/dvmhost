// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file PacketBuffer.h
 * @ingroup network_core
 * @file PacketBuffer.cpp
 * @ingroup network_core
 */
#if !defined(__PACKET_BUFFER_H__)
#define __PACKET_BUFFER_H__

#include "common/Defines.h"
#include "common/concurrent/unordered_map.h"
#include "common/zlib/Compression.h"

#include <unordered_map>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define FRAG_HDR_SIZE 10
#define FRAG_BLOCK_SIZE 534
#define FRAG_SIZE (FRAG_HDR_SIZE + FRAG_BLOCK_SIZE)

namespace network
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents a fragmented packet buffer.
     * @ingroup network_core
     */
    class HOST_SW_API PacketBuffer {
    public:
        /**
         * @brief Initializes a new instance of the PacketBuffer class.
         * @param compression Flag indicating whether packet data should be compressed automatically.
         * @param name Name of buffer.
         */
        explicit PacketBuffer(bool compression, const char* name);
        /**
         * @brief Finalizes a instance of the PacketBuffer class.
         */
        ~PacketBuffer();

        /**
         * @brief Decode a network packet fragment.
         * @param[in] data Buffer containing packet fragment to decode.
         * @param[out] message Buffer containing assembled message.
         * @param[out] outLength Length of assembled message.
         */
        bool decode(const uint8_t* data, uint8_t** message, uint32_t* outLength);
        /**
         * @brief Encode a network packet fragment.
         * @param[out] data Message to encode.
         * @param length Length of message.
         */
        void encode(uint8_t* data, uint32_t length);

        /**
         * @brief Helper to clear currently buffered fragments.
         */
        void clear();

    public:
        /**
         * @brief Represents a packet buffer fragment.
         */
        class Fragment {
        public:
            /**
             * @brief Compressed size of the packet.
             */
            uint32_t compressedSize;
            /**
             * @brief Uncompressed size of the packet.
             */
            uint32_t size;

            /**
             * @brief Size of the packet fragment block.
             */
            uint32_t blockSize;
            /**
             * @brief Block ID of the fragment.
             */
            uint8_t blockId;

            /**
             * @brief Fragment data.
             */
            uint8_t* data;
        };
        concurrent::unordered_map<uint8_t, Fragment*> fragments;

    private:
        bool m_compression;
        const char* m_name;
    };
} // namespace network

#endif // __PACKET_BUFFER_H__
