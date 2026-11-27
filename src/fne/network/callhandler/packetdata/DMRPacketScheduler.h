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
 * @file DMRPacketScheduler.h
 * @ingroup fne_packetdata
 * @file DMRPacketScheduler.cpp
 * @ingroup fne_packetdata
 */
#if !defined(__PACKETDATA__DMR_PACKET_SCHEDULER_H__)
#define  __PACKETDATA__DMR_PACKET_SCHEDULER_H__

#include "fne/Defines.h"
#include "common/dmr/DMRDefines.h"
#include "common/dmr/data/DataHeader.h"

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
             * @brief One queued DMR packet data downlink.
             * @ingroup fne_packetdata
             */
            struct HOST_SW_API ScheduledDMRDataPacket {
                dmr::data::DataHeader header;      //!< Header information for the DMR packet.
                std::vector<uint8_t> userData;     //!< User data payload for the DMR packet.
                uint32_t dstId = 0U;               //!< Destination radio ID.
                uint32_t targetIPAddress = 0U;     //!< Target IP address for the scheduled packet.
                uint64_t dueAt = 0U;               //!< Timestamp indicating when the packet is due.
                uint8_t retryCount = 0U;           //!< Number of retry attempts for the packet.
                bool extendedRetry = false;        //!< Indicates if extended retry is enabled.
            };

            // ---------------------------------------------------------------------------
            //  Class Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief Owns bounded queued DMR packet data downlinks.
             * @ingroup fne_packetdata
             */
            class HOST_SW_API DMRPacketScheduler {
            public:
                /**
                 * @brief Initializes a new instance of the DMRPacketScheduler class.
                 * @param maxFrames The maximum number of frames the scheduler can hold.
                 * @param maxBytes The maximum number of bytes the scheduler can hold.
                 */
                DMRPacketScheduler(uint32_t maxFrames = 0U, uint32_t maxBytes = 0U);

                /**
                 * @brief Enqueues a scheduled DMR packet.
                 * @param packet The scheduled packet to enqueue.
                 * @return The number of oldest frames dropped to make room.
                 */
                uint32_t enqueue(ScheduledDMRDataPacket&& packet);

                bool empty() const noexcept { return m_packets.empty(); }
                size_t size() const noexcept { return m_packets.size(); }
                uint32_t byteCount() const noexcept { return m_byteCount; }

                ScheduledDMRDataPacket* front();
                void pop();
                void rotate();
                void clear();

            private:
                uint32_t m_maxFrames;
                uint32_t m_maxBytes;
                uint32_t m_byteCount;
                std::deque<ScheduledDMRDataPacket> m_packets;
            };
        } // namespace packetdata
    } // namespace callhandler
} // namespace network

#endif // __PACKETDATA__DMR_PACKET_SCHEDULER_H__
