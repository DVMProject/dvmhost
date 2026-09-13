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
#include "common/json/json.h"
#include "network/MetadataNetwork.h"
#include "fne/ActivityLog.h"
#include "HostFNE.h"

using namespace network;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Handles NET_FUNC::TRANSFER packets. */

void MetadataNetwork::PacketHandler::transfer(TrafficNetwork* network, MetadataNetwork* mdNetwork, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId)
{
    (void)mdNetwork;
    (void)ssrc;
    (void)streamId;

    // resolve peer ID (used for Activity Log and Status Transfer)
    bool validPeerId = false;
    uint32_t pktPeerId = 0U;
    if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
        validPeerId = true;
        pktPeerId = peerId;
    } else {
        if (peerId > 0) {
            // this could be a replica transfer -- in which case, we need to check the SSRC of the packet not the peer ID
            if (network->m_peers.find(req->rtpHeader.getSSRC()) != network->m_peers.end()) {
                FNEPeerConnection* connection = network->m_peers[req->rtpHeader.getSSRC()];
                if (connection != nullptr) {
                    if (connection->peerClass() == PEER_CONN_CLASS_NEIGHBOR && connection->isReplica()) {
                        validPeerId = true;
                        pktPeerId = req->rtpHeader.getSSRC();
                    }
                }
            }
        }
    }

    // process incoming message subfunction opcodes
    switch (req->fneHeader.getSubFunction()) {
    case NET_SUBFUNC::TRANSFER_SUBFUNC_ACTIVITY:        // Peer Activity Log Transfer
        {
            if (network->m_allowActivityTransfer) {
                if (pktPeerId > 0 && validPeerId) {
                    FNEPeerConnection* connection = network->m_peers[pktPeerId];
                    if (connection != nullptr) {
                        std::string ip = udp::Socket::address(req->address);

                        // validate peer (simple validation really)
                        if (connection->connected() && connection->address() == ip) {
                            const size_t payloadLength = (size_t)(req->length - TRANSFER_PCKT_HDR_LEN);
                            std::string payload((const char *)(req->buffer + TRANSFER_PCKT_HDR_LEN), payloadLength);

                            ::ActivityLog("%.9u (%8s) %s", pktPeerId, connection->identWithQualifier().c_str(), payload.c_str());

                            // report activity log to metrics
                            TrafficNetwork::MetricsLogging::logActivity(network, pktPeerId, connection->identity(), payload);

                            // repeat traffic to the connected SysView peers
                            if (network->m_peers.size() > 0U) {
                                for (auto peer : network->m_peers) {
                                    if (peer.second != nullptr) {
                                        if (peer.second->peerClass() == PEER_CONN_CLASS_SYSVIEW) {
                                            sockaddr_storage addr = peer.second->socketStorage();
                                            uint32_t addrLen = peer.second->sockStorageLen();

                                            network->m_frameQueue->write(req->buffer, req->length, network->createStreamId(), pktPeerId, network->m_peerId,
                                                { NET_FUNC::TRANSFER, NET_SUBFUNC::TRANSFER_SUBFUNC_ACTIVITY }, RTP_END_OF_CALL_SEQ, addr, addrLen);
                                        }
                                    } else {
                                        continue;
                                    }
                                }
                            }

                            // attempt to repeat traffic to replica masters
                            if (network->m_host->m_peerNetworks.size() > 0) {
                                for (auto peer : network->m_host->m_peerNetworks) {
                                    if (peer.second != nullptr) {
                                        if (peer.second->isEnabled() && peer.second->isReplica()) {
                                            peer.second->writeMaster({ NET_FUNC::TRANSFER, NET_SUBFUNC::TRANSFER_SUBFUNC_ACTIVITY },
                                                req->buffer, req->length, RTP_END_OF_CALL_SEQ, 0U, true, pktPeerId);
                                        }
                                    }
                                }
                            }
                        }
                        else {
                            network->writePeerNAK(pktPeerId, network->createStreamId(), TAG_TRANSFER_ACT_LOG, NET_CONN_NAK_FNE_UNAUTHORIZED);
                        }
                    }
                }
            }
        }
        break;

    case NET_SUBFUNC::TRANSFER_SUBFUNC_DIAG:            // Peer Diagnostic Log Transfer
        {
            if (network->m_allowDiagnosticTransfer) {
                if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                    FNEPeerConnection* connection = network->m_peers[peerId];
                    if (connection != nullptr) {
                        std::string ip = udp::Socket::address(req->address);

                        // validate peer (simple validation really)
                        if (connection->connected() && connection->address() == ip) {
                            const size_t payloadLength = (size_t)(req->length - TRANSFER_PCKT_HDR_LEN);
                            std::string payload((const char *)(req->buffer + TRANSFER_PCKT_HDR_LEN), payloadLength);

                            bool currState = g_disableTimeDisplay;
                            g_disableTimeDisplay = true;
                            ::Log(9999U, {nullptr, nullptr, 0U, nullptr}, "%.9u (%8s) %s", peerId, connection->identWithQualifier().c_str(), payload.c_str());
                            g_disableTimeDisplay = currState;

                            // report diagnostic log to metrics
                            TrafficNetwork::MetricsLogging::logDiag(network, peerId, connection->identity(), payload);
                        }
                        else {
                            network->writePeerNAK(peerId, network->createStreamId(), TAG_TRANSFER_DIAG_LOG, NET_CONN_NAK_FNE_UNAUTHORIZED);
                        }
                    }
                }
            }
        }
        break;

    case NET_SUBFUNC::TRANSFER_SUBFUNC_STATUS:          // Peer Status Transfer
        {
            if (pktPeerId > 0 && validPeerId) {
                FNEPeerConnection* connection = network->m_peers[pktPeerId];
                if (connection != nullptr) {
                    std::string ip = udp::Socket::address(req->address);

                    // validate peer (simple validation really)
                    if (connection->connected() && connection->address() == ip) {
                        if (network->m_peers.size() > 0U) {
                            // attempt to repeat status traffic to SysView clients
                            for (auto peer : network->m_peers) {
                                if (peer.second != nullptr) {
                                    if (peer.second->peerClass() == PEER_CONN_CLASS_SYSVIEW) {
                                        sockaddr_storage addr = peer.second->socketStorage();
                                        uint32_t addrLen = peer.second->sockStorageLen();

                                        if (network->m_debug) {
                                            LogDebug(LOG_DIAG, "SysView, srcPeer = %u, dstPeer = %u, peer status message, len = %u",
                                                pktPeerId, peer.first, req->length);
                                        }
                                        network->m_frameQueue->write(req->buffer, req->length, network->createStreamId(), pktPeerId, network->m_peerId,
                                            { NET_FUNC::TRANSFER, NET_SUBFUNC::TRANSFER_SUBFUNC_STATUS }, RTP_END_OF_CALL_SEQ, addr, addrLen);
                                    }
                                } else {
                                    continue;
                                }
                            }

                            // attempt to repeat status traffic to replica masters
                            if (network->m_host->m_peerNetworks.size() > 0) {
                                for (auto peer : network->m_host->m_peerNetworks) {
                                    if (peer.second != nullptr) {
                                        if (peer.second->isEnabled() && peer.second->isReplica()) {
                                            peer.second->writeMaster({ NET_FUNC::TRANSFER, NET_SUBFUNC::TRANSFER_SUBFUNC_STATUS },
                                                req->buffer, req->length, RTP_END_OF_CALL_SEQ, 0U, true, pktPeerId);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else {
                        network->writePeerNAK(pktPeerId, network->createStreamId(), TAG_TRANSFER_STATUS, NET_CONN_NAK_FNE_UNAUTHORIZED);
                    }
                }
            }
        }
        break;

    case NET_SUBFUNC::TRANSFER_SUBFUNC_PATCH_STATUS:    // Console Patch Status Transfer
        {
            if (pktPeerId > 0 && validPeerId) {
                FNEPeerConnection* connection = network->m_peers[pktPeerId];
                if (connection != nullptr) {
                    if (!network->patchStatusEnabled()) {
                        network->writePeerNAK(pktPeerId, network->createStreamId(), TAG_TRANSFER_PATCH_STATUS, NET_CONN_NAK_FNE_UNAUTHORIZED);
                        break;
                    }

                    std::string ip = udp::Socket::address(req->address);

                    // Only authenticated console peers may publish or request patch registry state.
                    if (req->length <= TRANSFER_PCKT_HDR_LEN) {
                        network->writePeerNAK(pktPeerId, network->createStreamId(), TAG_TRANSFER_PATCH_STATUS, NET_CONN_NAK_ILLEGAL_PACKET);
                        break;
                    }

                    if (connection->connected() && connection->address() == ip && connection->peerClass() == PEER_CONN_CLASS_CONSOLE) {
                        const size_t payloadLength = (size_t)(req->length - TRANSFER_PCKT_HDR_LEN);
                        std::string payload((const char *)(req->buffer + TRANSFER_PCKT_HDR_LEN), payloadLength);

                        json::value v;
                        std::string err = json::parse(v, payload);
                        if (!err.empty() || !v.is<json::object>()) {
                            network->writePeerNAK(pktPeerId, network->createStreamId(), TAG_TRANSFER_PATCH_STATUS, NET_CONN_NAK_ILLEGAL_PACKET);
                            break;
                        }

                        json::object reqObj = v.get<json::object>();
                        std::string type = "snapshot";
                        if (reqObj["type"].is<std::string>())
                            type = reqObj["type"].get<std::string>();

                        if (type == "request") {
                            json::object snapshot = network->patchStatusRegistry().snapshot();
                            network->writePatchStatusToPeer(pktPeerId, snapshot);
                            break;
                        }

                        // The authenticated peer identity is authoritative; do not allow spoofed peer IDs.
                        reqObj["peerId"].set<uint32_t>(pktPeerId);
                        reqObj["originFnePeerId"].set<uint32_t>(network->m_peerId);
                        if (!reqObj["peerName"].is<std::string>() || reqObj["peerName"].get<std::string>().empty())
                            reqObj["peerName"].set<std::string>(connection->identity());

                        json::object response = json::object();
                        std::string errorMessage;
                        bool changed = false;
                        if (!network->patchStatusRegistry().publish(reqObj, response, errorMessage, &changed)) {
                            LogWarning(LOG_MASTER, "PEER %u (%s) invalid patch status payload, %s", pktPeerId, connection->identWithQualifier().c_str(), errorMessage.c_str());
                            network->writePeerNAK(pktPeerId, network->createStreamId(), TAG_TRANSFER_PATCH_STATUS, NET_CONN_NAK_ILLEGAL_PACKET);
                            break;
                        }

                        if (changed) {
                            network->writePatchStatusToConsoles(response);
                            network->replicatePatchStatus(reqObj);
                        }
                    }
                    else {
                        network->writePeerNAK(pktPeerId, network->createStreamId(), TAG_TRANSFER_PATCH_STATUS, NET_CONN_NAK_FNE_UNAUTHORIZED);
                    }
                }
            }
        }
        break;

    default:
        {
            LogWarning(LOG_MASTER, "PEER %u, unknown/unsupported transfer opcode %u", peerId, req->fneHeader.getSubFunction());
            if (network->m_debug)
                Utils::dump("Unknown/unsupported transfer opcode from the peer", req->buffer, req->length);
        }
        break;
    }
}
