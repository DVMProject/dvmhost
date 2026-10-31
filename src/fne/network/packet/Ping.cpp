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

/* Handles NET_FUNC::PING packets. */

void TrafficNetwork::PacketHandler::ping(TrafficNetwork* network, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId, uint64_t now)
{
    TrafficNetwork::PeerConnectionPtr connection = network->findPeer(peerId);
    if (peerId > 0 && connection != nullptr) {
        std::string ip = udp::Socket::address(req->address);

        // validate peer (simple validation really)
        if (connection->connected() && connection->address() == ip) {
            uint32_t pingsRx = connection->pingsReceived();
            uint64_t lastPing = connection->lastPing();
            pingsRx++;

            connection->pingsReceived(pingsRx);
            connection->lastPing(now);

            uint8_t payload[8U];
            ::memset(payload, 0x00U, 8U);

            // split ulong64_t (8 byte) value into bytes
            payload[0U] = (uint8_t)((now >> 56) & 0xFFU);
            payload[1U] = (uint8_t)((now >> 48) & 0xFFU);
            payload[2U] = (uint8_t)((now >> 40) & 0xFFU);
            payload[3U] = (uint8_t)((now >> 32) & 0xFFU);
            payload[4U] = (uint8_t)((now >> 24) & 0xFFU);
            payload[5U] = (uint8_t)((now >> 16) & 0xFFU);
            payload[6U] = (uint8_t)((now >> 8) & 0xFFU);
            payload[7U] = (uint8_t)((now >> 0) & 0xFFU);

            network->writePeerCommand(peerId, { NET_FUNC::PONG, NET_SUBFUNC::NOP }, payload, 8U, streamId, false);

            if (network->m_reportPeerPing) {
                LogInfoEx(LOG_MASTER, "PEER %u (%s) ping, pingsReceived = %u, lastPing = %u, now = %u", peerId, connection->identWithQualifier().c_str(),
                    connection->pingsReceived(), lastPing, now);
            }

            // ensure STP sanity, when we receive a ping from a downstream leaf
            //  this check ensures a STP entry for a downstream leaf isn't accidentally blown off
            //  the tree during a fast reconnect
            if (network->m_enableSpanningTree && connection->peerClass() == PEER_CONN_CLASS_NEIGHBOR) {
                std::lock_guard<std::mutex> guard(network->m_treeLock);

                if ((connection->masterId() != peerId) && (connection->masterId() != 0U)) {
                    // check if this peer is already connected via another peer
                    SpanningTree* tree = SpanningTree::findByMasterID(connection->masterId());
                    if (tree == nullptr) {
                        LogWarning(LOG_STP, "PEER %u (%s) downstream server not announced in server tree, reinitializing STP entry, this is abnormal, peerId = %u, masterId = %u, connectionState = %u", peerId, connection->identWithQualifier().c_str(),
                            peerId, connection->masterId(), connection->connectionState());
                        SpanningTree* node = new SpanningTree(peerId, connection->masterId(), network->m_treeRoot);
                        node->identity(connection->identity());
                        network->logSpanningTree(connection.get());
                    }
                }
            }
        }
        else {
            network->writePeerNAK(peerId, streamId, TAG_REPEATER_PING);
        }
    }
}
