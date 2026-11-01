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
                    if (req->length >= static_cast<int>(MSG_ANNC_GRP_AFFIL)) {
                        uint32_t srcId = GET_UINT24(req->buffer, 0U);           // Source Address
                        uint32_t dstId = GET_UINT24(req->buffer, 3U);           // Destination Address
                        network->updatePeerAffiliation(peerId, srcId, dstId);
                    }
                    else {
                        LogWarning(LOG_MASTER, "PEER %u (%s) announced malformed group affiliation, length = %u",
                            peerId, connection->identWithQualifier().c_str(), req->length);
                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
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
                    if (req->length >= static_cast<int>(MSG_ANNC_UNIT_REG)) {
                        uint32_t srcId = GET_UINT24(req->buffer, 0U);           // Source Address
                        network->updatePeerUnitRegistration(peerId, srcId, ssrc);
                    }
                    else {
                        LogWarning(LOG_MASTER, "PEER %u (%s) announced malformed unit registration, length = %u",
                            peerId, connection->identWithQualifier().c_str(), req->length);
                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
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
                    if (req->length >= static_cast<int>(MSG_ANNC_UNIT_REG)) {
                        uint32_t srcId = GET_UINT24(req->buffer, 0U);           // Source Address
                        network->removePeerUnitRegistration(peerId, srcId);
                    }
                    else {
                        LogWarning(LOG_MASTER, "PEER %u (%s) announced malformed unit deregistration, length = %u",
                            peerId, connection->identWithQualifier().c_str(), req->length);
                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
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
                    if (req->length >= static_cast<int>(MSG_ANNC_GRP_UNAFFIL)) {
                        uint32_t srcId = GET_UINT24(req->buffer, 0U);           // Source Address
                        network->removePeerAffiliation(peerId, srcId);
                    }
                    else {
                        LogWarning(LOG_MASTER, "PEER %u (%s) announced malformed group unaffiliation, length = %u",
                            peerId, connection->identWithQualifier().c_str(), req->length);
                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
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

                    if (aff != nullptr && req->length >= 4) {
                        const uint32_t len = GET_UINT32(req->buffer, 0U);
                        const uint32_t availableEntries = (static_cast<uint32_t>(req->length) - 4U) / 8U;
                        if (len != availableEntries || static_cast<uint32_t>(req->length) != 4U + (len * 8U)) {
                            LogWarning(LOG_MASTER, "PEER %u (%s) announced malformed affiliations, count = %u, length = %u",
                                peerId, connection->identWithQualifier().c_str(), len, req->length);
                            network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
                            break;
                        }

                        std::unordered_map<uint32_t, uint32_t> affiliations;
                        affiliations.reserve(len);
                        uint32_t offs = 4U;
                        for (uint32_t i = 0U; i < len; i++) {
                            const uint32_t srcId = GET_UINT24(req->buffer, offs);
                            const uint32_t dstId = GET_UINT24(req->buffer, offs + 4U);
                            affiliations[srcId] = dstId;
                            offs += 8U;
                        }

                        network->replacePeerAffiliations(peerId, affiliations);
                        LogInfoEx(LOG_MASTER, "PEER %u (%s) announced %u affiliations", peerId, connection->identWithQualifier().c_str(), len);
                    }
                    else if (aff != nullptr) {
                        LogWarning(LOG_MASTER, "PEER %u (%s) announced malformed affiliations, length = %u",
                            peerId, connection->identWithQualifier().c_str(), req->length);
                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
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

                    if (aff != nullptr && req->length >= 4) {
                        const uint32_t len = GET_UINT32(req->buffer, 0U);
                        const uint32_t availableEntries = (static_cast<uint32_t>(req->length) - 4U) / 3U;
                        if (len != availableEntries || static_cast<uint32_t>(req->length) != 4U + (len * 3U)) {
                            LogWarning(LOG_MASTER, "PEER %u (%s) announced malformed unit registrations, count = %u, length = %u",
                                peerId, connection->identWithQualifier().c_str(), len, req->length);
                            network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
                            break;
                        }

                        std::vector<uint32_t> registrations;
                        registrations.reserve(len);
                        uint32_t offs = 4U;
                        for (uint32_t i = 0U; i < len; i++) {
                            const uint32_t srcId = GET_UINT24(req->buffer, offs);
                            registrations.push_back(srcId);
                            offs += 3U;
                        }

                        network->replacePeerUnitRegistrations(peerId, registrations, ssrc);
                        LogInfoEx(LOG_MASTER, "PEER %u (%s) announced %u unit registrations", peerId, connection->identWithQualifier().c_str(), len);
                    }
                    else if (aff != nullptr) {
                        LogWarning(LOG_MASTER, "PEER %u (%s) announced malformed unit registrations, length = %u",
                            peerId, connection->identWithQualifier().c_str(), req->length);
                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
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
