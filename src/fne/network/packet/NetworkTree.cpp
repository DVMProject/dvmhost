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

/* Handles NET_FUNC::NET_TREE packets. */

void MetadataNetwork::PacketHandler::networkTree(TrafficNetwork* network, MetadataNetwork* mdNetwork, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId)
{
    (void)ssrc;

    if (!network->m_enableSpanningTree)
        return;

    if (req->fneHeader.getSubFunction() == NET_SUBFUNC::NET_TREE_LIST) { // FNE Network Tree List
        if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
            FNEPeerConnection* connection = network->m_peers[peerId];
            if (connection != nullptr) {
                std::string ip = udp::Socket::address(req->address);

                // validate peer (simple validation really)
                if (connection->connected() && connection->address() == ip && connection->peerClass() == PEER_CONN_CLASS_NEIGHBOR) {
                    DECLARE_UINT8_ARRAY(rawPayload, req->length);
                    ::memcpy(rawPayload, req->buffer, req->length);

                    // Utils::dump(1U, "MetadataNetwork::taskNetworkRx(), NET_TREE_LIST, Raw Payload", rawPayload, req->length);

                    PacketBufferEntryPtr pkt = findOrCreatePacketBufferEntry(mdNetwork->m_peerTreeListPkt, peerId, "Network Tree, Tree List", streamId);
                    if (pkt == nullptr) {
                        LogError(LOG_STP, "PEER %u (%s) Network Tree, Tree List, failed to initialize packet buffer", peerId,
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
                        LogError(LOG_STP, "PEER %u (%s) Network Tree, Tree List, timeout waiting for packet buffer to unlock", peerId,
                            connection->identWithQualifier().c_str());
                        // detach the stalled transfer without destroying state that
                        // another worker still owns; its shared_ptr keeps it alive
                        erasePacketBufferEntry(mdNetwork->m_peerTreeListPkt, peerId, pkt);
                        return;
                    }

                    // the worker that previously owned the entry may have completed the
                    // transfer and released the lock after resetting the buffer
                    if (!pkt->buffer) {
                        erasePacketBufferEntry(mdNetwork->m_peerTreeListPkt, peerId, pkt);
                        return;
                    }

                    if (pkt->streamId != streamId) {
                        LogError(LOG_STP, "PEER %u (%s) Network Tree, Tree List, stream ID mismatch, expected %u, got %u", peerId,
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
                            LogError(LOG_STP, "PEER %u (%s) error parsing network tree list, %s", peerId, connection->identWithQualifier().c_str(), err.c_str());
                            pkt->buffer->clear();
                            pkt->buffer.reset();
                            pkt->streamId = 0U;
                            if (decompressed != nullptr) {
                                delete[] decompressed;
                            }
                            erasePacketBufferEntry(mdNetwork->m_peerTreeListPkt, peerId, pkt);
                            return;
                        }
                        else  {
                            // ensure parsed JSON is an array
                            if (!v.is<json::array>()) {
                                LogError(LOG_STP, "PEER %u (%s) error parsing network tree list, data was not valid", peerId, connection->identWithQualifier().c_str());
                                pkt->buffer->clear();
                                pkt->buffer.reset();
                                pkt->streamId = 0U;
                                if (decompressed != nullptr) {
                                    delete[] decompressed;
                                }
                                erasePacketBufferEntry(mdNetwork->m_peerTreeListPkt, peerId, pkt);
                                return;
                            }
                            else {
                                json::array arr = v.get<json::array>();
                                LogInfoEx(LOG_STP, "PEER %u (%s) Network Tree, Tree List, updating %u peer entries", peerId, connection->identWithQualifier().c_str(), arr.size());

                                std::lock_guard<std::mutex> guard(network->m_treeLock);

                                std::vector<uint32_t> duplicatePeers;
                                SpanningTree::deserializeTree(arr, network->m_treeRoot, &duplicatePeers);

                                network->logSpanningTree(connection);

                                if (duplicatePeers.size() > 0U) {
                                    for (auto dupPeerId : duplicatePeers) {
                                        LogWarning(LOG_STP, "PEER %u (%s) Network Tree, Tree Change, disconnecting duplicate peer connection for PEER %u to prevent network loop",
                                            peerId, connection->identWithQualifier().c_str(), dupPeerId);
                                        network->writeTreeDisconnect(peerId, dupPeerId);
                                    }
                                }
                            }
                        }

                        pkt->buffer->clear();
                        pkt->buffer.reset();
                        pkt->streamId = 0U;
                        if (decompressed != nullptr) {
                            delete[] decompressed;
                        }
                        erasePacketBufferEntry(mdNetwork->m_peerTreeListPkt, peerId, pkt);
                    }
                }
                else {
                    network->writePeerNAK(peerId, 0U, TAG_PEER_REPLICA, NET_CONN_NAK_FNE_UNAUTHORIZED);
                }
            }
        }
    }
}
