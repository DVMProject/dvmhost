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
//  Public Class Members
// ---------------------------------------------------------------------------

/* Handles NET_FUNC::MST_DISC packets. */

bool Network::PacketHandler::masterDisconnect(Network* network, uint32_t peerId, uint32_t streamId, uint64_t now,
    const frame::RTPFNEHeader& fneHeader, const frame::RTPHeader& rtpHeader, const uint8_t* buffer, int length)
{
    (void)peerId;
    (void)streamId;
    (void)now;
    (void)fneHeader;
    (void)rtpHeader;
    (void)buffer;
    (void)length;

    LogError(LOG_NET, "PEER %u master disconnect, remotePeerId = %u", network->m_peerId, network->m_remotePeerId);
    network->m_status = NET_STAT_WAITING_CONNECT;

    // fire off peer disconnected callback if we have one
    if (network->m_peerDisconnectedCallback != nullptr) {
        network->m_peerDisconnectedCallback();
    }

    network->close();
    network->open();

    return false;
}
