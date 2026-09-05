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
#include "network/MetadataNetwork.h"
#include "fne/ActivityLog.h"
#include "HostFNE.h"

using namespace network;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Handles NET_FUNC::REPL packets. */

void MetadataNetwork::PacketHandler::replication(TrafficNetwork* network, MetadataNetwork* mdNetwork, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId)
{
    (void)ssrc;

    if (req->fneHeader.getSubFunction() == NET_SUBFUNC::REPL_ACT_PEER_LIST) { // Peer Replication Active Peer List
        if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
            FNEPeerConnection* connection = network->m_peers[peerId];
            if (connection != nullptr) {
                std::string ip = udp::Socket::address(req->address);

                // validate peer (simple validation really)
                if (connection->connected() && connection->address() == ip && connection->peerClass() == PEER_CONN_CLASS_NEIGHBOR &&
                    connection->isReplica()) {
                    DECLARE_UINT8_ARRAY(rawPayload, req->length);
                    ::memcpy(rawPayload, req->buffer, req->length);

                    // Utils::dump(1U, "MetadataNetwork::taskNetworkRx(), REPL_ACT_PEER_LIST, Raw Payload", rawPayload, req->length);

                    PacketBufferEntryPtr pkt = findOrCreatePacketBufferEntry(mdNetwork->m_peerReplicaActPkt, peerId, "Peer Replication, Active Peer List", streamId);
                    if (pkt == nullptr) {
                        LogError(LOG_REPL, "PEER %u (%s) Peer Replication, Active Peer List, failed to initialize packet buffer", peerId,
                            connection->identWithQualifier().c_str());
                        return;
                    }

                    std::unique_lock<std::mutex> pktLock(pkt->mutex, std::defer_lock);
                    uint32_t timeout = 0U;
                    while (!pktLock.try_lock() && timeout < TIMEOUT_MAX_REPL) {
                        timeout++;
                        Thread::sleep(1U);
                    }

                    if (!pktLock.owns_lock()) {
                        LogError(LOG_STP, "PEER %u (%s) Peer Replication, Active Peer List, timeout waiting for packet buffer to unlock", peerId,
                            connection->identWithQualifier().c_str());
                        // detach the stalled transfer without destroying state that
                        // another worker still owns; its shared_ptr keeps it alive
                        erasePacketBufferEntry(mdNetwork->m_peerReplicaActPkt, peerId, pkt);
                        return;
                    }

                    // the worker that previously owned the entry may have completed the
                    // transfer and released the lock after resetting the buffer
                    if (!pkt->buffer) {
                        erasePacketBufferEntry(mdNetwork->m_peerReplicaActPkt, peerId, pkt);
                        return;
                    }

                    if (pkt->streamId != streamId) {
                        LogError(LOG_REPL, "PEER %u (%s) Peer Replication, Active Peer List, stream ID mismatch, expected %u, got %u", peerId,
                            connection->identWithQualifier().c_str(), pkt->streamId, streamId);
                        pkt->buffer->clear();
                        pkt->streamId = streamId;
                    }

                    uint32_t decompressedLen = 0U;
                    uint8_t* decompressed = nullptr;

                    if (pkt->buffer->decode(rawPayload, &decompressed, &decompressedLen)) {
                        std::string payload(decompressed + 8U, decompressed + decompressedLen);

                        // parse JSON body
                        json::value v;
                        std::string err = json::parse(v, payload);
                        if (!err.empty()) {
                            LogError(LOG_REPL, "PEER %u (%s) error parsing active peer list, %s", peerId, connection->identWithQualifier().c_str(), err.c_str());
                            pkt->buffer->clear();
                            pkt->buffer.reset();
                            pkt->streamId = 0U;
                            if (decompressed != nullptr) {
                                delete[] decompressed;
                            }
                            erasePacketBufferEntry(mdNetwork->m_peerReplicaActPkt, peerId, pkt);
                            return;
                        }
                        else  {
                            // ensure parsed JSON is an array
                            if (!v.is<json::array>()) {
                                LogError(LOG_REPL, "PEER %u (%s) error parsing active peer list, data was not valid", peerId, connection->identWithQualifier().c_str());
                                pkt->buffer->clear();
                                pkt->buffer.reset();
                                pkt->streamId = 0U;
                                if (decompressed != nullptr) {
                                    delete[] decompressed;
                                }
                                erasePacketBufferEntry(mdNetwork->m_peerReplicaActPkt, peerId, pkt);
                                return;
                            }
                            else {
                                json::array arr = v.get<json::array>();
                                LogInfoEx(LOG_REPL, "PEER %u (%s) Peer Replication, Active Peer List, updating %u peer entries", peerId, connection->identWithQualifier().c_str(), arr.size());
                                network->m_peerReplicaPeers[peerId] = arr;
                            }
                        }

                        pkt->buffer->clear();
                        pkt->buffer.reset();
                        pkt->streamId = 0U;
                        if (decompressed != nullptr) {
                            delete[] decompressed;
                        }
                        erasePacketBufferEntry(mdNetwork->m_peerReplicaActPkt, peerId, pkt);
                    }
                }
                else {
                    network->writePeerNAK(peerId, 0U, TAG_PEER_REPLICA, NET_CONN_NAK_FNE_UNAUTHORIZED);
                }
            }
        }
    }
    else if (req->fneHeader.getSubFunction() == NET_SUBFUNC::REPL_HA_PARAMS) { // Peer Replication HA Parameters
        if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
            FNEPeerConnection* connection = network->m_peers[peerId];
            if (connection != nullptr) {
                std::string ip = udp::Socket::address(req->address);

                // validate peer (simple validation really)
                if (connection->connected() && connection->address() == ip && connection->peerClass() == PEER_CONN_CLASS_NEIGHBOR &&
                    connection->isReplica()) {
                    DECLARE_UINT8_ARRAY(rawPayload, req->length);
                    ::memcpy(rawPayload, req->buffer, req->length);

                    std::vector<HAParameters> receivedParams;

                    uint32_t len = GET_UINT32(rawPayload, 0U);
                    if (len > 0U) {
                        len /= HA_PARAMS_ENTRY_LEN;
                    }

                    uint8_t offs = 4U;
                    for (uint8_t i = 0U; i < len; i++, offs += HA_PARAMS_ENTRY_LEN) {
                        uint32_t peerId = GET_UINT32(rawPayload, offs);
                        uint32_t ipAddr = GET_UINT32(rawPayload, offs + 4U);
                        uint16_t port = GET_UINT16(rawPayload, offs + 8U);
                        receivedParams.push_back(HAParameters(peerId, ipAddr, port));
                    }

                    if (receivedParams.size() > 0U) {
                        for (auto rxEntry : receivedParams) {
                            auto it = std::find_if(network->m_peerReplicaHAParams.begin(), network->m_peerReplicaHAParams.end(),
                                [&](HAParameters& x)
                                {
                                    if (x.peerId == rxEntry.peerId)
                                        return true;
                                    return false;
                                });
                            if (it != network->m_peerReplicaHAParams.end()) {
                                it->masterIP = rxEntry.masterIP;
                                it->masterPort = rxEntry.masterPort;
                            } else {
                                HAParameters param = rxEntry;
                                network->m_peerReplicaHAParams.push_back(param);
                            }

                            if (network->m_debug) {
                                std::string address = __IP_FROM_UINT(rxEntry.masterIP);
                                LogDebugEx(LOG_REPL, "MetadataNetwork::taskNetworkRx()", "PEER %u (%s) Peer Replication, HA Parameters, %s:%u", peerId, connection->identWithQualifier().c_str(),
                                    address.c_str(), rxEntry.masterPort);
                            }
                        }

                        if (receivedParams.size() > 0) {
                            LogInfoEx(LOG_REPL, "PEER %u (%s) Peer Replication, HA Parameters, updating %u entries, %u entries", peerId, connection->identWithQualifier().c_str(), receivedParams.size(),
                                network->m_peerReplicaHAParams.size());

                            // send peer updates to replica peers
                            if (network->m_host->m_peerNetworks.size() > 0) {
                                for (auto peer : network->m_host->m_peerNetworks) {
                                    if (peer.second != nullptr) {
                                        if (peer.second->isEnabled() && peer.second->isReplica()) {
                                            std::vector<HAParameters> haParams;
                                            network->m_peerReplicaHAParams.lock(false);
                                            for (auto entry : network->m_peerReplicaHAParams) {
                                                haParams.push_back(entry);
                                            }
                                            network->m_peerReplicaHAParams.unlock();

                                            peer.second->writeHAParams(haParams);
                                        }
                                    }
                                }
                            }
                        }
                    }
                } else {
                    network->writePeerNAK(peerId, 0U, TAG_PEER_REPLICA, NET_CONN_NAK_FNE_UNAUTHORIZED);
                }
            }
        }
    }
}
