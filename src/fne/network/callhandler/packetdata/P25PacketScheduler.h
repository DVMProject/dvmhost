// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file P25PacketScheduler.h
 * @ingroup fne_packetdata
 * @file P25PacketScheduler.cpp
 * @ingroup fne_packetdata
 */
#if !defined(__PACKETDATA__P25_PACKET_SCHEDULER_H__)
#define  __PACKETDATA__P25_PACKET_SCHEDULER_H__

#include "fne/Defines.h"
#include "common/p25/data/DataHeader.h"

#include <cstdint>
#include <deque>
#include <vector>

namespace network
{
    namespace callhandler
    {
        namespace packetdata
        {
            // ---------------------------------------------------------------------------
            //  Struct Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief One queued packet data downlink.
             * @ingroup fne_packetdata
             */
            struct HOST_SW_API ScheduledP25DataPacket {
                ::p25::data::DataHeader header;    //!< Header information for the P25 packet.
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
             * @ingroup fne_packetdata
             */
            class HOST_SW_API P25PacketScheduler {
            public:
                /**
                 * @brief Initializes a new instance of the P25PacketScheduler class.
                 * @param maxFrames The maximum number of frames the scheduler can hold.
                 * @param maxBytes The maximum number of bytes the scheduler can hold.
                 */
                P25PacketScheduler(uint32_t maxFrames = 0U, uint32_t maxBytes = 0U);

                /**
                 * @brief Enqueues a scheduled packet into the scheduler.
                 * @param packet The scheduled packet to enqueue.
                 * @return The number of oldest frames dropped to make room.
                 */
                uint32_t enqueue(ScheduledP25DataPacket&& packet);

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
                ScheduledP25DataPacket* front();
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
                std::deque<ScheduledP25DataPacket> m_packets;
            };
        } // namespace packetdata
    } // namespace callhandler
} // namespace network

#endif // __PACKETDATA__P25_PACKET_SCHEDULER_H__
