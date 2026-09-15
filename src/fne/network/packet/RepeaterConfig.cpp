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

/* Handles NET_FUNC::RPTC packets. */

void TrafficNetwork::PacketHandler::repeaterConfig(TrafficNetwork* network, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId, uint64_t now)
{
    // validate the incoming packet length for a repeater configuration packet
    if (req == nullptr || req->buffer == nullptr || !TrafficNetwork::validRepeaterConfigLength(req->length)) {
        LogWarning(LOG_MASTER, "PEER %u RPTC, malformed packet length", peerId);
        return;
    }

    if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
        std::lock_guard<std::mutex> peerGuard(TrafficNetwork::getPeerStateLock(peerId));

        FNEPeerConnection* connection = network->m_peers[peerId];
        if (connection != nullptr) {
            connection->lastPing(now);

            if (connection->connectionState() == NET_STAT_WAITING_CONFIG) {
                const size_t payloadLength = (size_t)(req->length - REPEATER_PCKT_HDR_LEN);
                std::string payload((const char *)(req->buffer + REPEATER_PCKT_HDR_LEN), payloadLength);

                // parse JSON body
                json::value v;
                std::string err = json::parse(v, payload);
                if (!err.empty()) {
                    LogWarning(LOG_MASTER, "PEER %u RPTC NAK, supplied invalid configuration data", peerId);
                    network->writePeerNAK(peerId, TAG_REPEATER_AUTH, NET_CONN_NAK_INVALID_CONFIG_DATA, req->address, req->addrLen);
                    network->disconnectPeer(peerId, connection);
                }
                else  {
                    // ensure parsed JSON is an object
                    if (!v.is<json::object>()) {
                        LogWarning(LOG_MASTER, "PEER %u RPTC NAK, supplied invalid configuration data", peerId);
                        network->writePeerNAK(peerId, TAG_REPEATER_AUTH, NET_CONN_NAK_INVALID_CONFIG_DATA, req->address, req->addrLen);
                        network->disconnectPeer(peerId, connection);
                    }
                    else {
                        connection->config(v.get<json::object>());
                        connection->connectionState(NET_STAT_RUNNING);
                        connection->connected(true);
                        connection->pingsReceived(0U);
                        connection->lastPing(now);
                        connection->missedMetadataUpdates(0U);

                        lookups::PeerId peerEntry = network->m_peerListLookup->find(peerId);
                        if (!peerEntry.peerDefault()) {
                            if (peerEntry.hasCallPriority()) {
                                connection->hasCallPriority(peerEntry.hasCallPriority());
                                LogInfoEx(LOG_MASTER, "PEER %u >> Has Call Priority", peerId);
                            }
                        }

                        network->m_peers[peerId] = connection;

                        // attach extra notification data to the RPTC ACK to notify the peer of 
                        // the use of the alternate diagnostic port
                        uint8_t buffer[1U];
                        buffer[0U] = 0x80U; // this should really be a defined constant -- but
                                            // because this is the only option and its *always* sent now
                                            // we can just hardcode this for now

                        json::object peerConfig = connection->config();

                        std::string identity = "* UNK *";
                        if (peerConfig["identity"].is<std::string>()) {
                            identity = peerConfig["identity"].getDefault<std::string>("* UNK *");
                            connection->identity(identity);
                            LogInfoEx(LOG_MASTER, "PEER %u >> Identity [%8s]", peerId, identity.c_str());
                        }

                        if (peerConfig["software"].is<std::string>()) {
                            std::string software = peerConfig["software"].get<std::string>();
                            LogInfoEx(LOG_MASTER, "PEER %u >> Software Version [%s]", peerId, software.c_str());
                        }

                        /*
                        ** bryanb: this is support for older configuration structs, newer peers should
                        **  send the peerClass value
                        */
                        {
                            // is the peer reporting it is a SysView peer?
                            if (peerConfig["sysView"].is<bool>()) {
                                bool sysView = peerConfig["sysView"].get<bool>();
                                if (sysView)
                                    connection->peerClass(PEER_CONN_CLASS_SYSVIEW);
                                if (sysView)
                                    LogInfoEx(LOG_MASTER, "PEER %u >> SysView Peer", peerId);
                            }

                            // is the peer reporting it is a downstream FNE neighbor peer?
                            if (peerConfig["externalPeer"].is<bool>()) {
                                bool externalPeer = peerConfig["externalPeer"].get<bool>();
                                if (externalPeer)
                                    connection->peerClass(PEER_CONN_CLASS_NEIGHBOR);
                            }
                        }

                        // determine the peer class
                        if (peerConfig["peerClass"].is<uint32_t>()) {
                            uint32_t peerClass = peerConfig["peerClass"].get<uint32_t>();
                            if (peerClass >= PEER_CONN_CLASS_INVALID)
                                peerClass = PEER_CONN_CLASS_STANDARD;
                            connection->peerClass((PEER_CONN_CLASS)peerClass);
                        } else {
                            if (connection->peerClass() == PEER_CONN_CLASS_UNKNOWN) {
                                connection->peerClass(PEER_CONN_CLASS_STANDARD);
                            }
                        }

                        // is the peer reporting it is a conventional peer?
                        if (peerConfig["conventionalPeer"].is<bool>()) {
                            if (network->m_allowConvSiteAffOverride) {
                                bool convPeer = peerConfig["conventionalPeer"].get<bool>();
                                connection->isConventional(convPeer);
                            } else {
                                bool conventionalPeer = false;
                                peerConfig["conventionalPeer"].set<bool>(conventionalPeer);
                            }
                        } else {
                            bool conventionalPeer = false;
                            peerConfig["conventionalPeer"].set<bool>(conventionalPeer);
                        }

                        // report peer class in log
                        switch (connection->peerClass()) {
                            case PEER_CONN_CLASS_NEIGHBOR:
                                LogInfoEx(LOG_MASTER, "PEER %u >> Downstream Neighbor FNE Peer", peerId);
                                break;
                            case PEER_CONN_CLASS_SYSVIEW:
                                LogInfoEx(LOG_MASTER, "PEER %u >> SysView Peer", peerId);
                                connection->isReplica(true);
                                break;
                            case PEER_CONN_CLASS_CONSOLE:
                                LogInfoEx(LOG_MASTER, "PEER %u >> Console Peer", peerId);
                                break;
                            case PEER_CONN_CLASS_STANDARD:
                            default:
                                if (connection->isConventional())
                                    LogInfoEx(LOG_MASTER, "PEER %u >> Conventional Peer", peerId);
                                else
                                    LogInfoEx(LOG_MASTER, "PEER %u >> Standard Peer", peerId);
                                break;
                        }

                        // for the purposes of maintaining backward compat with REST API users,
                        //  we will report the peer class via the old externalPeer and sysView boolean 
                        //  fields as well (consolePeer doesn't have a legacy field, but that's ok since 
                        //  it's only used for console connections which should be new enough to support 
                        //  the new peerClass field)
                        bool externalPeer = connection->peerClass() == PEER_CONN_CLASS_NEIGHBOR;
                        peerConfig["externalPeer"].set<bool>(externalPeer);
                        bool sysViewPeer = connection->peerClass() == PEER_CONN_CLASS_SYSVIEW;
                        peerConfig["sysView"].set<bool>(sysViewPeer);
                        bool consolePeer = connection->peerClass() == PEER_CONN_CLASS_CONSOLE;
                        peerConfig["consolePeer"].set<bool>(consolePeer);
                        connection->config(peerConfig);

                        // is the peer reporting it is an downstream FNE neighbor peer?
                        /*
                        ** bryanb: don't change externalPeer to neighborPeer -- this will break backward
                        **  compat with older FNE versions (we're stuck with this naming :()
                        */
                        if (connection->peerClass() == PEER_CONN_CLASS_NEIGHBOR) {
                            uint32_t masterPeerId = 0U;
                            if (peerConfig["masterPeerId"].is<uint32_t>()) {
                                masterPeerId = peerConfig["masterPeerId"].get<uint32_t>();
                                connection->masterId(masterPeerId);
                                LogInfoEx(LOG_MASTER, "PEER %u >> Master Peer ID [%u]", peerId, masterPeerId);
                            }

                            // master peer ID should never be zero for an neighbor peer -- use the peer ID instead
                            if (masterPeerId == 0U) {
                                LogWarning(LOG_MASTER, "PEER %u reports to be a downstream FNE neighbor peer but has not supplied a valid masterPeerId, using own peerId as masterPeerId (old FNE perhaps?)", peerId);
                                masterPeerId = peerId;
                            }

                            // check if the peer a peer replication participant
                            lookups::PeerId peerEntry = network->m_peerListLookup->find(req->peerId);
                            if (!peerEntry.peerDefault()) {
                                if (peerEntry.peerReplica()) {
                                    connection->isReplica(true);
                                    LogInfoEx(LOG_MASTER, "PEER %u >> Participates in Peer Replication", peerId);
                                }
                            }

                            if (network->m_enableSpanningTree) {
                                network->m_treeLock.lock();

                                // check if this peer is already connected via another peer
                                SpanningTree* tree = SpanningTree::findByMasterID(masterPeerId);
                                if (tree != nullptr) {
                                    // are we allowing a fast reconnect? (this happens when a connecting peer
                                    //  uses the same peer ID and master ID already announced in the tree, but
                                    //  the tree entry wasn't yet erased)
                                    if ((tree->id() == peerId && tree->masterId() == masterPeerId) &&
                                        network->m_spanningTreeFastReconnect) {
                                        LogWarning(LOG_STP, "PEER %u (%s) server already announced in server tree, fast peer reconnect, peerId = %u, masterId = %u, treePeerId = %u, treeMasterId = %u, connectionState = %u", peerId, connection->identWithQualifier().c_str(),
                                            peerId, masterPeerId, tree->id(), tree->masterId(), connection->connectionState());
                                        if (identity != tree->identity()) {
                                            LogWarning(LOG_STP, "PEER %u (%s) why has this server's announced identity changed? *big hmmmm*", peerId, connection->identWithQualifier().c_str());
                                        }
                                        SpanningTree::moveParent(tree, network->m_treeRoot);
                                        network->logSpanningTree(connection);
                                    } else {
                                        LogWarning(LOG_STP, "PEER %u (%s) RPTC NAK, server already connected via PEER %u, duplicate connection denied, peerId = %u, masterId = %u, treePeerId = %u, treeMasterId = %u, connectionState = %u", peerId, connection->identWithQualifier().c_str(),
                                            peerId, masterPeerId, tree->id(), tree->masterId(), tree->id(), connection->connectionState());
                                        network->writePeerNAK(peerId, TAG_REPEATER_CONFIG, NET_CONN_NAK_FNE_DUPLICATE_CONN, req->address, req->addrLen);
                                        network->m_treeLock.unlock();
                                        network->disconnectPeer(peerId, connection);
                                        return;
                                    }
                                } else {
                                    SpanningTree* node = new SpanningTree(peerId, masterPeerId, network->m_treeRoot);
                                    node->identity(identity);
                                    network->logSpanningTree(connection);
                                }

                                network->m_treeLock.unlock();
                            }
                        }

                        network->writePeerACK(peerId, streamId, buffer, 1U);
                        LogInfoEx(LOG_MASTER, "PEER %u RPTC ACK, completed the configuration exchange", peerId);

                        // setup the affiliations list for this peer
                        std::stringstream peerName;
                        peerName << "PEER " << peerId;
                        network->createPeerAffiliations(peerId, peerName.str());

                        // spin up a thread and send metadata over to peer
                        network->peerMetadataUpdate(peerId);
                        if (network->patchStatusEnabled() && connection->peerClass() == PEER_CONN_CLASS_CONSOLE)
                            network->writePatchStatusToPeer(peerId, network->patchStatusRegistry().snapshot());
                    }
                }
            }
            else {
                // perform source address/port validation
                if (connection->address() != udp::Socket::address(req->address) ||
                    connection->port() != udp::Socket::port(req->address)) {
                    LogError(LOG_MASTER, "PEER %u (%s) RPTC NAK, IP address/port mismatch on RPTC attempt while in an incorrect state, old = %s:%u, new = %s:%u, connectionState = %u", peerId, connection->identWithQualifier().c_str(),
                        connection->address().c_str(), connection->port(), udp::Socket::address(req->address).c_str(), udp::Socket::port(req->address), connection->connectionState());

                    network->writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_FNE_UNAUTHORIZED, req->address, req->addrLen);
                    return;
                }

                LogWarning(LOG_MASTER, "PEER %u (%s) RPTC NAK, login exchange while in an incorrect state, connectionState = %u", peerId, connection->identWithQualifier().c_str(),
                    connection->connectionState());
                network->writePeerNAK(peerId, TAG_REPEATER_CONFIG, NET_CONN_NAK_BAD_CONN_STATE, req->address, req->addrLen);
                network->disconnectPeer(peerId, connection);
            }
        }
    }
    else {
        network->writePeerNAK(peerId, TAG_REPEATER_CONFIG, NET_CONN_NAK_BAD_CONN_STATE, req->address, req->addrLen);
        LogWarning(LOG_MASTER, "PEER %u RPTC NAK, having no connection", peerId);
    }
}
