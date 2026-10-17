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
#include "common/edac/SHA256.h"
#include "common/Log.h"
#include "network/TrafficNetwork.h"
#include "fne/ActivityLog.h"
#include "HostFNE.h"

using namespace network;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Handles NET_FUNC::RPTK packets. */

void TrafficNetwork::PacketHandler::repeaterAuth(TrafficNetwork* network, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId, uint64_t now)
{
    // validate the incoming packet length for a repeater authentication packet
    if (req == nullptr || req->buffer == nullptr || !TrafficNetwork::validRepeaterAuthLength(req->length)) {
        LogWarning(LOG_MASTER, "PEER %u RPTK, malformed packet length", peerId);
        return;
    }

    if (peerId > 0) {
        std::lock_guard<std::mutex> peerGuard(TrafficNetwork::getPeerStateLock(peerId));

        TrafficNetwork::PeerConnectionPtr connection = network->findPeer(peerId);
        if (connection != nullptr) {
            connection->lastPing(now);

            if (connection->connectionState() == NET_STAT_WAITING_AUTHORISATION) {
                // get the hash from the frame message
                uint8_t hash[REPEATER_AUTH_HASH_LEN];
                ::memcpy(hash, req->buffer + REPEATER_PCKT_HDR_LEN, sizeof(hash));

                // generate our own hash
                uint8_t salt[4U];
                ::memset(salt, 0x00U, 4U);
                SET_UINT32(connection->salt(), salt, 0U);

                std::string passwordForPeer = network->m_password;

                // check if the peer is in the peer ACL list
                bool validAcl = true;
                if (network->m_peerListLookup->getACL()) {
                    if (!network->m_peerListLookup->isPeerAllowed(peerId) && !network->m_peerListLookup->isPeerListEmpty()) {
                        LogWarning(LOG_MASTER, "PEER %u RPTK, failed peer ACL check", peerId);
                        validAcl = false;
                    } else {
                        lookups::PeerId peerEntry = network->m_peerListLookup->find(peerId);
                        if (peerEntry.peerDefault()) {
                            validAcl = false; // default peer IDs are a no-no as they have no data thus fail ACL check
                        } else {
                            passwordForPeer = peerEntry.peerPassword();
                            if (passwordForPeer.length() == 0) {
                                passwordForPeer = network->m_password;
                            }
                        }
                    }

                    if (network->m_peerListLookup->isPeerListEmpty()) {
                        LogWarning(LOG_MASTER, "Peer List ACL enabled, but we have an empty peer list? Passing all peers.");
                        validAcl = true;
                    }
                }

                if (validAcl) {
                    size_t size = passwordForPeer.size();
                    uint8_t* in = new uint8_t[size + sizeof(uint32_t)];
                    ::memcpy(in, salt, sizeof(uint32_t));
                    for (size_t i = 0U; i < size; i++)
                        in[i + sizeof(uint32_t)] = passwordForPeer.at(i);

                    uint8_t out[32U];
                    edac::SHA256 sha256;
                    sha256.buffer(in, (uint32_t)(size + sizeof(uint32_t)), out);

                    delete[] in;

                    // validate hash
                    bool validHash = true;
                    for (size_t i = 0U; i < sizeof(hash); i++) {
                        if (hash[i] != out[i]) {
                            validHash = false;
                            break;
                        }
                    }

                    if (validHash) {
                        connection->connectionState(NET_STAT_WAITING_CONFIG);
                        network->writePeerACK(peerId, streamId);
                        LogInfoEx(LOG_MASTER, "PEER %u RPTK ACK, completed the login exchange", peerId);
                        network->storePeer(peerId, connection);
                    }
                    else {
                        LogWarning(LOG_MASTER, "PEER %u RPTK NAK, failed the login exchange", peerId);
                        network->writePeerNAK(peerId, TAG_REPEATER_AUTH, NET_CONN_NAK_FNE_UNAUTHORIZED, req->address, req->addrLen);
                        network->disconnectPeer(peerId, connection);
                    }
                } else {
                    network->writePeerNAK(peerId, TAG_REPEATER_AUTH, NET_CONN_NAK_PEER_ACL, req->address, req->addrLen);
                    network->disconnectPeer(peerId, connection);
                }
            }
            else {
                // perform source address/port validation
                if (connection->address() != udp::Socket::address(req->address) ||
                    connection->port() != udp::Socket::port(req->address)) {
                    LogError(LOG_MASTER, "PEER %u RPTK NAK, IP address/port mismatch on RPTK attempt while in an incorrect state, old = %s:%u, new = %s:%u, connectionState = %u", peerId,
                        connection->address().c_str(), connection->port(), udp::Socket::address(req->address).c_str(), udp::Socket::port(req->address), connection->connectionState());

                    network->writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_FNE_UNAUTHORIZED, req->address, req->addrLen);
                    return;
                }

                LogWarning(LOG_MASTER, "PEER %u RPTK NAK, login exchange while in an incorrect state, connectionState = %u", peerId, connection->connectionState());
                network->writePeerNAK(peerId, TAG_REPEATER_AUTH, NET_CONN_NAK_BAD_CONN_STATE, req->address, req->addrLen);
                network->disconnectPeer(peerId, connection);
            }
        }
    }
    else {
        network->writePeerNAK(peerId, TAG_REPEATER_AUTH, NET_CONN_NAK_BAD_CONN_STATE, req->address, req->addrLen);
        network->erasePeer(peerId);
        LogWarning(LOG_MASTER, "PEER %u RPTK NAK, having no connection", peerId);
    }
}
