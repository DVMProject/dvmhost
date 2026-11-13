// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2017-2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/Log.h"
#include "network/Network.h"

using namespace network;

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define MAX_SERVER_DIFF 360ULL // maximum difference in time between a server timestamp and local timestamp in milliseconds

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Handles NET_FUNC::PONG packets. */

bool Network::PacketHandler::pong(Network* network, uint32_t peerId, uint32_t streamId, uint64_t now,
    const frame::RTPFNEHeader& fneHeader, const frame::RTPHeader& rtpHeader, const uint8_t* buffer, int length)
{
    (void)peerId;
    (void)streamId;
    (void)fneHeader;
    (void)rtpHeader;

    network->m_timeoutTimer.start();
    if (length >= 14) {
        if (network->m_packetDump)
            Utils::dump(1U, "Network::clock(), Network Rx, PONG", buffer, length);

        ulong64_t serverNow = 0U;

        // combine bytes into ulong64_t (8 byte) value
        serverNow = buffer[6U];
        serverNow = (serverNow << 8) + buffer[7U];
        serverNow = (serverNow << 8) + buffer[8U];
        serverNow = (serverNow << 8) + buffer[9U];
        serverNow = (serverNow << 8) + buffer[10U];
        serverNow = (serverNow << 8) + buffer[11U];
        serverNow = (serverNow << 8) + buffer[12U];
        serverNow = (serverNow << 8) + buffer[13U];

        // check the ping RTT and report any over the maximum defined time
        uint64_t dt = (uint64_t)fabs((double)now - (double)serverNow);
        if (dt > MAX_SERVER_DIFF)
            LogWarning(LOG_NET, "PEER %u pong, time delay greater than %llums, now = %llu, server = %llu, dt = %llu", network->m_peerId, MAX_SERVER_DIFF, now, serverNow, dt);

        ++network->m_pingsReceived;

        // if we've been connected for at least 10 PING/PONG cycles and we're flagged duplicate connection, clear the flag
        if (network->m_pingsReceived > 10U && network->m_flaggedDuplicateConn) {
            network->m_flaggedDuplicateConn = false;
        }
    }

    return false;
}
