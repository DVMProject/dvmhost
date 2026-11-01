// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023-2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/Defines.h"
#include "common/Log.h"
#include "network/TrafficNetwork.h"
#include "fne/ActivityLog.h"
#include "HostFNE.h"

using namespace network;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Handles NET_FUNC::INCALL_CTRL packets. */

void TrafficNetwork::PacketHandler::inCallControl(TrafficNetwork* network, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId, uint64_t now)
{
    if (network->m_disallowInCallCtrl)
        return;

    TrafficNetwork::PeerConnectionPtr connection = network->findPeer(peerId);
    if (peerId > 0 && connection != nullptr) {
        std::string ip = udp::Socket::address(req->address);

        // validate peer (simple validation really)
        if (connection->connected() && connection->address() == ip) {
            NET_ICC::ENUM command = (NET_ICC::ENUM)req->buffer[10U];
            uint32_t dstId = GET_UINT24(req->buffer, 11U);
            uint8_t slot = req->buffer[14U];

            network->processInCallCtrl(command, req->fneHeader.getSubFunction(), dstId, slot, peerId, ssrc, streamId);
        }
    }
}
