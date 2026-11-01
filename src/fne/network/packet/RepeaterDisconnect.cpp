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

/* Handles NET_FUNC::RPT_DISC packets. */

void TrafficNetwork::PacketHandler::repeaterDisconnect(TrafficNetwork* network, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId, uint64_t now)
{
    if (peerId > 0) {
        std::lock_guard<std::mutex> peerGuard(TrafficNetwork::getPeerStateLock(peerId));

        TrafficNetwork::PeerConnectionPtr connection = network->findPeer(peerId);
        if (connection != nullptr) {
            std::string ip = udp::Socket::address(req->address);

            // validate peer (simple validation really)
            if (connection->connected() && connection->address() == ip) {
                LogInfoEx(LOG_MASTER, "PEER %u (%s) disconnected", peerId, connection->identWithQualifier().c_str());
                network->disconnectPeer(peerId, connection);
            }
        }
    }
}
