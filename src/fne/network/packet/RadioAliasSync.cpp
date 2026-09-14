// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/Defines.h"
#include "common/Log.h"
#include "network/MetadataNetwork.h"
#include "fne/ActivityLog.h"
#include "HostFNE.h"

using namespace network;

#include <fstream>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Handles NET_FUNC::RADIO_ALIAS_SYNC packets. */

void MetadataNetwork::PacketHandler::radioAliasSync(TrafficNetwork* network, MetadataNetwork* mdNetwork, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId)
{
    (void)mdNetwork;

    if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
        FNEPeerConnection* connection = network->m_peers[peerId];
        if (connection != nullptr) {
            std::string ip = udp::Socket::address(req->address);

            // validate peer (simple validation really)
            if (connection->connected() && connection->address() == ip) {
                // read entire file into buffer
                std::stringstream b;
                std::ifstream stream(network->m_ridAliasLookup->filename(), std::ios::in | std::ios::binary);

                uint32_t len = 0U;
                UInt8Array bufferUInt8Array = nullptr;
                uint8_t* buffer = nullptr;

                if (stream.is_open()) {
                    stream.seekg(0, std::ios::end);
                    len = (uint32_t)stream.tellg();
                    stream.seekg(0, std::ios::beg);

                    bufferUInt8Array = std::make_unique<uint8_t[]>(len);
                    buffer = bufferUInt8Array.get();
                    ::memset(buffer, 0x00U, len);

                    uint32_t i = 0U;
                    while (stream.peek() != EOF) {
                        buffer[i] = (uint8_t)stream.get();
                        i++;
                    }

                    stream.close();
                }

                PacketBuffer pkt(true, "Radio Alias Sync");
                bool success = pkt.encode((uint8_t*)buffer, len);
                if (!success) {
                    LogError(LOG_REPL, "PEER %u Radio Alias Sync, failed to encode packet", peerId);
                    return;
                }

                LogInfoEx(LOG_REPL, "PEER %u Radio Alias Sync, blocks %u, streamId = %u", peerId, pkt.fragments.size(), streamId);
                if (pkt.fragments.size() > 0U) {
                    for (auto frag : pkt.fragments) {
                        // violate most handling rules for responding to packets -- we need to directly respond to the calling peer as
                        // they may not be logged in as a standard peer
                        network->writePeer(peerId, network->m_peerId, { NET_FUNC::RADIO_ALIAS_SYNC, NET_SUBFUNC::NOP }, 
                            frag.second->data, FRAG_SIZE, 0U, streamId);
                        Thread::sleep(60U); // pace block transmission
                    }
                }

                pkt.clear();
            }
            else {
                network->writePeerNAK(peerId, streamId, TAG_PEER_REPLICA, NET_CONN_NAK_FNE_UNAUTHORIZED);
            }
        }
    }
}
