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

/* Handles NET_FUNC::ANNOUNCE packets. */

void MetadataNetwork::PacketHandler::announce(TrafficNetwork* network, MetadataNetwork* mdNetwork, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId)
{
    (void)mdNetwork;

    // process incoming message subfunction opcodes
    switch (req->fneHeader.getSubFunction()) {
    case NET_SUBFUNC::ANNC_SUBFUNC_GRP_AFFIL:           // Announce Group Affiliation
        {
            TrafficNetwork::PeerConnectionPtr connection = network->findPeer(peerId);
            if (peerId > 0 && connection != nullptr) {
                std::string ip = udp::Socket::address(req->address);
                std::shared_ptr<fne_lookups::AffiliationLookup> aff = network->getPeerAffiliations(peerId);
                if (aff == nullptr) {
                    LogError(LOG_MASTER, "PEER %u (%s) has uninitialized affiliations lookup?", peerId, connection->identWithQualifier().c_str());
                    network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
                }

                // validate peer (simple validation really)
                if (connection->connected() && connection->address() == ip && aff != nullptr) {
                    uint32_t srcId = GET_UINT24(req->buffer, 0U);           // Source Address
                    uint32_t dstId = GET_UINT24(req->buffer, 3U);           // Destination Address
                    aff->groupUnaff(srcId);
                    aff->groupAff(srcId, dstId);

                    // attempt to repeat traffic to replica masters
                    if (network->m_host->m_peerNetworks.size() > 0) {
                        for (auto& peer : network->m_host->m_peerNetworks) {
                            if (peer.second != nullptr) {
                                if (peer.second->isEnabled() && peer.second->isReplica()) {
                                    peer.second->writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_GRP_AFFIL },
                                        req->buffer, req->length, req->rtpHeader.getSequence(), streamId, true);
                                }
                            }
                        }
                    }
                }
                else {
                    network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_FNE_UNAUTHORIZED);
                }
            }
        }
        break;

    case NET_SUBFUNC::ANNC_SUBFUNC_UNIT_REG:            // Announce Unit Registration
        {
            TrafficNetwork::PeerConnectionPtr connection = network->findPeer(peerId);
            if (peerId > 0 && connection != nullptr) {
                std::string ip = udp::Socket::address(req->address);
                std::shared_ptr<fne_lookups::AffiliationLookup> aff = network->getPeerAffiliations(peerId);
                if (aff == nullptr) {
                    LogError(LOG_MASTER, "PEER %u (%s) has uninitialized affiliations lookup?", peerId, connection->identWithQualifier().c_str());
                    network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
                }

                // validate peer (simple validation really)
                if (connection->connected() && connection->address() == ip && aff != nullptr) {
                    uint32_t srcId = GET_UINT24(req->buffer, 0U);           // Source Address
                    aff->unitReg(srcId, ssrc);
                    network->m_globalAff->unitReg(srcId, ssrc);

                    // attempt to repeat traffic to replica masters
                    if (network->m_host->m_peerNetworks.size() > 0) {
                        for (auto& peer : network->m_host->m_peerNetworks) {
                            if (peer.second != nullptr) {
                                if (peer.second->isEnabled() && peer.second->isReplica()) {
                                    peer.second->writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_UNIT_REG },
                                        req->buffer, req->length, req->rtpHeader.getSequence(), streamId, true, 0U, ssrc);
                                }
                            }
                        }
                    }
                }
                else {
                    network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_FNE_UNAUTHORIZED);
                }
            }
        }
        break;
    case NET_SUBFUNC::ANNC_SUBFUNC_UNIT_DEREG:          // Announce Unit Deregistration
        {
            TrafficNetwork::PeerConnectionPtr connection = network->findPeer(peerId);
            if (peerId > 0 && connection != nullptr) {
                std::string ip = udp::Socket::address(req->address);
                std::shared_ptr<fne_lookups::AffiliationLookup> aff = network->getPeerAffiliations(peerId);
                if (aff == nullptr) {
                    LogError(LOG_MASTER, "PEER %u (%s) has uninitialized affiliations lookup?", peerId, connection->identWithQualifier().c_str());
                    network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
                }

                // validate peer (simple validation really)
                if (connection->connected() && connection->address() == ip && aff != nullptr) {
                    uint32_t srcId = GET_UINT24(req->buffer, 0U);           // Source Address
                    aff->unitDereg(srcId);
                    network->m_globalAff->unitDereg(srcId);

                    // attempt to repeat traffic to replica masters
                    if (network->m_host->m_peerNetworks.size() > 0) {
                        for (auto& peer : network->m_host->m_peerNetworks) {
                            if (peer.second != nullptr) {
                                if (peer.second->isEnabled() && peer.second->isReplica()) {
                                    peer.second->writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_UNIT_DEREG },
                                        req->buffer, req->length, req->rtpHeader.getSequence(), streamId, true);
                                }
                            }
                        }
                    }
                }
                else {
                    network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_FNE_UNAUTHORIZED);
                }
            }
        }
        break;

    case NET_SUBFUNC::ANNC_SUBFUNC_GRP_UNAFFIL:         // Announce Group Affiliation Removal
        {
            TrafficNetwork::PeerConnectionPtr connection = network->findPeer(peerId);
            if (peerId > 0 && connection != nullptr) {
                std::string ip = udp::Socket::address(req->address);
                std::shared_ptr<fne_lookups::AffiliationLookup> aff = network->getPeerAffiliations(peerId);
                if (aff == nullptr) {
                    LogError(LOG_MASTER, "PEER %u (%s) has uninitialized affiliations lookup?", peerId, connection->identWithQualifier().c_str());
                    network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
                }

                // validate peer (simple validation really)
                if (connection->connected() && connection->address() == ip && aff != nullptr) {
                    uint32_t srcId = GET_UINT24(req->buffer, 0U);           // Source Address
                    aff->groupUnaff(srcId);
                    network->m_globalAff->groupUnaff(srcId);

                    // attempt to repeat traffic to replica masters
                    if (network->m_host->m_peerNetworks.size() > 0) {
                        for (auto& peer : network->m_host->m_peerNetworks) {
                            if (peer.second != nullptr) {
                                if (peer.second->isEnabled() && peer.second->isReplica()) {
                                    peer.second->writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_GRP_UNAFFIL },
                                        req->buffer, req->length, req->rtpHeader.getSequence(), streamId, true);
                                }
                            }
                        }
                    }
                }
                else {
                    network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_FNE_UNAUTHORIZED);
                }
            }
        }
        break;

    case NET_SUBFUNC::ANNC_SUBFUNC_AFFILS:              // Announce Update All Affiliations
        {
            TrafficNetwork::PeerConnectionPtr connection = network->findPeer(peerId);
            if (peerId > 0 && connection != nullptr) {
                std::string ip = udp::Socket::address(req->address);

                // validate peer (simple validation really)
                if (connection->connected() && connection->address() == ip) {
                    std::shared_ptr<fne_lookups::AffiliationLookup> aff = network->getPeerAffiliations(peerId);
                    if (aff == nullptr) {
                        LogError(LOG_MASTER, "PEER %u (%s) has uninitialized affiliations lookup?", peerId, connection->identWithQualifier().c_str());
                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
                    }

                    if (aff != nullptr) {
                        aff->clearGroupAff(0U, true);

                        // update TGID lists
                        uint32_t len = GET_UINT32(req->buffer, 0U);
                        uint32_t offs = 4U;
                        for (uint32_t i = 0; i < len; i++) {
                            uint32_t srcId = GET_UINT24(req->buffer, offs);
                            uint32_t dstId = GET_UINT24(req->buffer, offs + 4U);

                            aff->groupAff(srcId, dstId);
                            network->m_globalAff->groupAff(srcId, dstId);
                            offs += 8U;
                        }
                        LogInfoEx(LOG_MASTER, "PEER %u (%s) announced %u affiliations", peerId, connection->identWithQualifier().c_str(), len);

                        // attempt to repeat traffic to replica masters
                        if (network->m_host->m_peerNetworks.size() > 0) {
                            for (auto& peer : network->m_host->m_peerNetworks) {
                                if (peer.second != nullptr) {
                                    if (peer.second->isEnabled() && peer.second->isReplica()) {
                                        peer.second->writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_AFFILS },
                                            req->buffer, req->length, req->rtpHeader.getSequence(), streamId, true);
                                    }
                                }
                            }
                        }
                    }
                }
                else {
                    network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_FNE_UNAUTHORIZED);
                }
            }
        }
        break;

    case NET_SUBFUNC::ANNC_SUBFUNC_UNIT_REGS:           // Announce Update All Unit Registrations
        {
            TrafficNetwork::PeerConnectionPtr connection = network->findPeer(peerId);
            if (peerId > 0 && connection != nullptr) {
                std::string ip = udp::Socket::address(req->address);

                // validate peer (simple validation really)
                if (connection->connected() && connection->address() == ip) {
                    std::shared_ptr<fne_lookups::AffiliationLookup> aff = network->getPeerAffiliations(peerId);
                    if (aff == nullptr) {
                        LogError(LOG_MASTER, "PEER %u (%s) has uninitialized affiliations lookup?", peerId, connection->identWithQualifier().c_str());
                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
                    }

                    if (aff != nullptr) {
                        aff->clearUnitReg();

                        // update unit registration lists
                        uint32_t len = GET_UINT32(req->buffer, 0U);
                        uint32_t offs = 4U;
                        for (uint32_t i = 0; i < len; i++) {
                            uint32_t srcId = GET_UINT24(req->buffer, offs);

                            aff->unitReg(srcId, ssrc);
                            network->m_globalAff->unitReg(srcId, ssrc);
                            offs += 3U;
                        }
                        LogInfoEx(LOG_MASTER, "PEER %u (%s) announced %u unit registrations", peerId, connection->identWithQualifier().c_str(), len);

                        // attempt to repeat traffic to replica masters
                        if (network->m_host->m_peerNetworks.size() > 0) {
                            for (auto& peer : network->m_host->m_peerNetworks) {
                                if (peer.second != nullptr) {
                                    if (peer.second->isEnabled() && peer.second->isReplica()) {
                                        peer.second->writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_UNIT_REGS },
                                            req->buffer, req->length, req->rtpHeader.getSequence(), streamId, true);
                                    }
                                }
                            }
                        }
                    }
                }
                else {
                    network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_FNE_UNAUTHORIZED);
                }
            }
        }
        break;

    case NET_SUBFUNC::ANNC_SUBFUNC_SITE_VC:             // Announce Site VCs
        {
            TrafficNetwork::PeerConnectionPtr connection = network->findPeer(peerId);
            if (peerId > 0 && connection != nullptr) {
                std::string ip = udp::Socket::address(req->address);

                // validate peer (simple validation really)
                if (connection->connected() && connection->address() == ip) {
                    std::vector<uint32_t> vcPeers;

                    // update peer association
                    uint32_t len = GET_UINT32(req->buffer, 0U);
                    uint32_t offs = 4U;
                    for (uint32_t i = 0; i < len; i++) {
                        uint32_t vcPeerId = GET_UINT32(req->buffer, offs);
                        TrafficNetwork::PeerConnectionPtr vcConnection = network->findPeer(vcPeerId);
                        if (vcPeerId > 0 && vcConnection != nullptr) {
                            if (vcConnection != nullptr) {
                                vcConnection->ccPeerId(peerId);
                                vcPeers.push_back(vcPeerId);
                            }
                        }
                        offs += 4U;
                    }
                    LogInfoEx(LOG_MASTER, "PEER %u (%s) announced %u VCs", peerId, connection->identWithQualifier().c_str(), len);
                    network->m_ccPeerMap[peerId] = vcPeers;

                    // attempt to repeat traffic to replica masters
                    if (network->m_host->m_peerNetworks.size() > 0) {
                        for (auto& peer : network->m_host->m_peerNetworks) {
                            if (peer.second != nullptr) {
                                if (peer.second->isEnabled() && peer.second->isReplica()) {
                                    peer.second->writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_SITE_VC },
                                        req->buffer, req->length, req->rtpHeader.getSequence(), streamId, true);
                                }
                            }
                        }
                    }
                }
                else {
                    network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_FNE_UNAUTHORIZED);
                }
            }
        }
        break;
    default:
        {
            LogWarning(LOG_MASTER, "PEER %u, unknown/unsupported announcement opcode %u", peerId, req->fneHeader.getSubFunction());
            if (network->m_debug)
                Utils::dump("Unknown/unsupported announcement opcode from the peer", req->buffer, req->length);
        }
        break;
    }
}
