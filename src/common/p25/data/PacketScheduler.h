// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file PacketScheduler.h
 * @ingroup p25_pdu
 * @file PacketScheduler.cpp
 * @ingroup p25_pdu
 */
#if !defined(__P25_DATA__PACKETSCHEDULER_H__)
#define  __P25_DATA__PACKETSCHEDULER_H__

#include "common/Defines.h"
#include "common/p25/data/DataHeader.h"

#include <cstdint>
#include <deque>
#include <vector>

namespace p25
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Struct Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief One queued packet data downlink.
         * @ingroup p25_pdu
         */
        struct DVM_COMMON_API ScheduledDataPacket {
            DataHeader header;                 //!< Header information for the P25 packet.
            std::vector<uint8_t> userData;     //!< User data payload for the P25 packet.
            uint32_t llId = 0U;                //!< Link-layer ID associated with the scheduled packet.
            uint32_t targetIPAddress = 0U;     //!< Target IP address for the scheduled packet.
            uint64_t dueAt = 0U;               //!< Timestamp indicating when the packet is due.
            uint8_t retryCount = 0U;           //!< Number of retry attempts for the packet.
            bool extendedRetry = false;        //!< Indicates if extended retry is enabled.
        };

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Owns bounded queued packet data downlinks.
         * @ingroup p25_pdu
         */
        class DVM_COMMON_API PacketScheduler {
        public:
            /**
             * @brief Initializes a new instance of the PacketScheduler class.
             * @param maxFrames The maximum number of frames the scheduler can hold.
             * @param maxBytes The maximum number of bytes the scheduler can hold.
             */
            PacketScheduler(uint32_t maxFrames = 0U, uint32_t maxBytes = 0U);

            /**
             * @brief Enqueues a scheduled packet into the scheduler.
             * @param packet The scheduled packet to enqueue.
             * @return The number of oldest frames dropped to make room.
             */
            uint32_t enqueue(ScheduledDataPacket&& packet);

            /**
             * @brief Checks if the scheduler is empty.
             * @return True if the scheduler is empty, false otherwise.
             */
            bool empty() const noexcept { return m_packets.empty(); }
            /**
             * @brief Gets the number of packets currently in the scheduler.
             * @return The number of packets in the scheduler.
             */
            size_t size() const noexcept { return m_packets.size(); }
            /**
             * @brief Gets the total byte count of packets currently in the scheduler.
             * @return The total byte count of packets in the scheduler.
             */
            uint32_t byteCount() const noexcept { return m_byteCount; }

            /**
             * @brief Gets the next queued packet.
             * @return A pointer to the next scheduled packet, or nullptr if the scheduler is empty.
             */
            ScheduledDataPacket* front();
            /**
             * @brief Removes the next queued packet.
             */
            void pop();
            /**
             * @brief Moves the next queued packet to the back of the queue.
             */
            void rotate();
            /**
             * @brief Clears all queued packets from the scheduler.
             */
            void clear();

        private:
            uint32_t m_maxFrames;
            uint32_t m_maxBytes;
            uint32_t m_byteCount;
            std::deque<ScheduledDataPacket> m_packets;
        };
    } // namespace data
} // namespace p25

#endif // __P25_DATA__PACKETSCHEDULER_H__
