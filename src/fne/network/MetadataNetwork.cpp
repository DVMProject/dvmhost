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
#include "common/zlib/Compression.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "network/MetadataNetwork.h"
#include "fne/ActivityLog.h"
#include "HostFNE.h"

using namespace network;
using namespace network::callhandler;
using namespace compress;

#include <cassert>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t TIMEOUT_MAX_REPL = 5000U; // 5 seconds

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MetadataNetwork class. */

MetadataNetwork::MetadataNetwork(HostFNE* host, TrafficNetwork* trafficNetwork, const std::string& address, uint16_t port, uint16_t workerCnt) :
    BaseNetwork(trafficNetwork->m_peerId, true, trafficNetwork->m_debug, true, true, trafficNetwork->m_allowActivityTransfer, trafficNetwork->m_allowDiagnosticTransfer),
    m_trafficNetwork(trafficNetwork),
    m_host(host),
    m_address(address),
    m_port(port),
    m_status(NET_STAT_INVALID),
    m_peerReplicaActPkt(),
    m_peerTreeListPkt(),
    m_threadPool(workerCnt, "meta")
{
    assert(trafficNetwork != nullptr);
    assert(host != nullptr);
    assert(!address.empty());
    assert(port > 0U);
}

/* Finalizes a instance of the MetadataNetwork class. */

MetadataNetwork::~MetadataNetwork() = default;

/* Sets endpoint preshared encryption key. */

void MetadataNetwork::setPresharedKey(const uint8_t* presharedKey)
{
    m_socket->setPresharedKey(presharedKey);
}

/* Process a data frames from the network. */

void MetadataNetwork::processNetwork()
{
    if (m_status != NET_STAT_MST_RUNNING) {
        return;
    }

    sockaddr_storage address;
    uint32_t addrLen;
    frame::RTPHeader rtpHeader;
    frame::RTPFNEHeader fneHeader;
    int length = 0U;

    // read message
    UInt8Array buffer = m_frameQueue->read(length, address, addrLen, &rtpHeader, &fneHeader);
    if (length > 0) {
        if (m_debug)
            Utils::dump(1U, "MetadataNetwork::processNetwork(), Network Message", buffer.get(), length);

        uint32_t peerId = fneHeader.getPeerId();

        NetPacketRequest* req = new NetPacketRequest();
        req->obj = m_trafficNetwork;
        req->metadataObj = this;
        req->peerId = peerId;

        req->address = address;
        req->addrLen = addrLen;
        req->rtpHeader = rtpHeader;
        req->fneHeader = fneHeader;

        req->length = length;
        req->buffer = new uint8_t[length];
        ::memcpy(req->buffer, buffer.get(), length);

        if (!m_threadPool.enqueue(new_pooltask(taskNetworkRx, req))) {
            LogError(LOG_NET, "Failed to task enqueue network packet request, peerId = %u, %s:%u", peerId, 
                udp::Socket::address(address).c_str(), udp::Socket::port(address));
            if (req != nullptr) {
                if (req->buffer != nullptr)
                    delete[] req->buffer;
                delete req;
            }
        }
    }
}

/* Updates the timer by the passed number of milliseconds. */

void MetadataNetwork::clock(uint32_t ms)
{
    if (m_status != NET_STAT_MST_RUNNING) {
        return;
    }
}

/* Opens connection to the network. */

bool MetadataNetwork::open()
{
    if (m_debug)
        LogInfoEx(LOG_DIAG, "Opening Network");

    m_threadPool.start();

    m_status = NET_STAT_MST_RUNNING;

    m_socket = new udp::Socket(m_address, m_port);

    // reinitialize the frame queue
    if (m_frameQueue != nullptr) {
        delete m_frameQueue;
        m_frameQueue = new FrameQueue(m_socket, m_peerId, m_debug);
    }

    bool ret = m_socket->open();
    if (!ret) {
        m_socket->recvBufSize(524288U); // 512K recv buffer
        m_socket->sendBufSize(524288U); // 512K send buffer
        m_status = NET_STAT_INVALID;
    }

    return ret;
}

/* Closes connection to the network. */

void MetadataNetwork::close()
{
    if (m_debug)
        LogInfoEx(LOG_DIAG, "Closing Network");

    m_threadPool.stop();
    m_threadPool.wait();
    
    m_socket->close();

    m_status = NET_STAT_INVALID;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Process a data frames from the network. */

void MetadataNetwork::taskNetworkRx(NetPacketRequest* req)
{
    if (req != nullptr) {
        TrafficNetwork* network = static_cast<TrafficNetwork*>(req->obj);
        if (network == nullptr) {
            if (req != nullptr) {
                if (req->buffer != nullptr)
                    delete[] req->buffer;
                delete req;
            }

            return;
        }

        MetadataNetwork* mdNetwork = static_cast<MetadataNetwork*>(req->metadataObj);
        if (mdNetwork == nullptr) {
            if (req != nullptr) {
                if (req->buffer != nullptr)
                    delete[] req->buffer;
                delete req;
            }

            return;
        }

        if (req == nullptr)
            return;

        if (req->length > 0) {
            uint32_t peerId = req->fneHeader.getPeerId();
            uint32_t ssrc = req->rtpHeader.getSSRC();
            uint32_t streamId = req->fneHeader.getStreamId();

            // process incoming message function opcodes
            switch (req->fneHeader.getFunction()) {
            case NET_FUNC::TRANSFER:                                    // Transfer
                {
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
                                            DECLARE_UINT8_ARRAY(rawPayload, req->length - 11U);
                                            ::memcpy(rawPayload, req->buffer + 11U, req->length - 11U);
                                            std::string payload(rawPayload, rawPayload + (req->length - 11U));

                                            ::ActivityLog("%.9u (%8s) %s", pktPeerId, connection->identWithQualifier().c_str(), payload.c_str());

                                            // report activity log to InfluxDB
                                            if (network->m_enableInfluxDB) {
                                                influxdb::QueryBuilder()
                                                    .meas("activity")
                                                        .tag("peerId", std::to_string(pktPeerId))
                                                            .field("identity", connection->identity())
                                                            .field("msg", payload)
                                                        .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                                                    .requestAsync(network->m_influxServer);
                                            }

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
                                            DECLARE_UINT8_ARRAY(rawPayload, req->length - 11U);
                                            ::memcpy(rawPayload, req->buffer + 11U, req->length - 11U);
                                            std::string payload(rawPayload, rawPayload + (req->length - 11U));

                                            bool currState = g_disableTimeDisplay;
                                            g_disableTimeDisplay = true;
                                            ::Log(9999U, {nullptr, nullptr, 0U, nullptr}, "%.9u (%8s) %s", peerId, connection->identWithQualifier().c_str(), payload.c_str());
                                            g_disableTimeDisplay = currState;

                                            // report diagnostic log to InfluxDB
                                            if (network->m_enableInfluxDB) {
                                                influxdb::QueryBuilder()
                                                    .meas("diag")
                                                        .tag("peerId", std::to_string(peerId))
                                                            .field("identity", connection->identity())
                                                            .field("msg", payload)
                                                        .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                                                    .requestAsync(network->m_influxServer);
                                            }
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

                    default:
                        {
                            LogWarning(LOG_MASTER, "PEER %u, unknown/unsupported transfer opcode %u", peerId, req->fneHeader.getSubFunction());
                            if (network->m_debug)
                                Utils::dump("Unknown/unsupported transfer opcode from the peer", req->buffer, req->length);
                        }
                        break;
                    }
                }
                break;

            case NET_FUNC::ANNOUNCE:                                    // Announce
                {
                    // process incoming message subfunction opcodes
                    switch (req->fneHeader.getSubFunction()) {
                    case NET_SUBFUNC::ANNC_SUBFUNC_GRP_AFFIL:           // Announce Group Affiliation
                        {
                            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                                FNEPeerConnection* connection = network->m_peers[peerId];
                                if (connection != nullptr) {
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
                        }
                        break;

                    case NET_SUBFUNC::ANNC_SUBFUNC_UNIT_REG:            // Announce Unit Registration
                        {
                            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                                FNEPeerConnection* connection = network->m_peers[peerId];
                                if (connection != nullptr) {
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
                        }
                        break;
                    case NET_SUBFUNC::ANNC_SUBFUNC_UNIT_DEREG:          // Announce Unit Deregistration
                        {
                            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                                FNEPeerConnection* connection = network->m_peers[peerId];
                                if (connection != nullptr) {
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
                        }
                        break;

                    case NET_SUBFUNC::ANNC_SUBFUNC_GRP_UNAFFIL:         // Announce Group Affiliation Removal
                        {
                            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                                FNEPeerConnection* connection = network->m_peers[peerId];
                                if (connection != nullptr) {
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
                        }
                        break;

                    case NET_SUBFUNC::ANNC_SUBFUNC_AFFILS:              // Announce Update All Affiliations
                        {
                            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                                FNEPeerConnection* connection = network->m_peers[peerId];
                                if (connection != nullptr) {
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
                        }
                        break;

                    case NET_SUBFUNC::ANNC_SUBFUNC_UNIT_REGS:           // Announce Update All Unit Registrations
                        {
                            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                                FNEPeerConnection* connection = network->m_peers[peerId];
                                if (connection != nullptr) {
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
                        }
                        break;

                    case NET_SUBFUNC::ANNC_SUBFUNC_SITE_VC:             // Announce Site VCs
                        {
                            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                                FNEPeerConnection* connection = network->m_peers[peerId];
                                if (connection != nullptr) {
                                    std::string ip = udp::Socket::address(req->address);

                                    // validate peer (simple validation really)
                                    if (connection->connected() && connection->address() == ip) {
                                        std::vector<uint32_t> vcPeers;

                                        // update peer association
                                        uint32_t len = GET_UINT32(req->buffer, 0U);
                                        uint32_t offs = 4U;
                                        for (uint32_t i = 0; i < len; i++) {
                                            uint32_t vcPeerId = GET_UINT32(req->buffer, offs);
                                            if (vcPeerId > 0 && (network->m_peers.find(vcPeerId) != network->m_peers.end())) {
                                                FNEPeerConnection* vcConnection = network->m_peers[vcPeerId];
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
                break;

            case NET_FUNC::KEYS_INVENTORY:                              // Encryption Key Container Inventory
                {
                    lookups::PeerId peerEntry = network->m_peerListLookup->find(peerId);
                    if (peerEntry.peerDefault()) {
                        LogError(LOG_MASTER, "PEER %u requested enc. key inventory but is not allowed, no response", peerId);
                        break;
                    } else {
                        if (!peerEntry.canRequestKeys()) {
                            LogError(LOG_MASTER, "PEER %u requested enc. key inventory but is not allowed, no response", peerId);
                            break;
                        }
                    }

                    // keys inventory operates differently from the rest of the network opcodes...and does not require
                    // an established connection to the master, so we will not validate the peer connection state here
                    if (peerId > 0 && !peerEntry.peerDefault()) {
                        if (req->length < 80) {
                            LogError(LOG_MASTER, "PEER %u requested enc. key inventory, but payload length was invalid (%u bytes), no response", peerId, req->length);
                            break;
                        }

                        // scope intentional
                        {
                            // get the peer password hash from the frame message
                            DECLARE_UINT8_ARRAY(peerHash, 32U);
                            ::memcpy(peerHash, req->buffer + 8U, 32U);

                            uint8_t peerSalt[4U];
                            ::memset(peerSalt, 0x00U, 4U);
                            ::memcpy(peerSalt, req->buffer + 40U, 4U);

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
                                ::memcpy(in, peerSalt, sizeof(uint32_t));
                                for (size_t i = 0U; i < size; i++)
                                    in[i + sizeof(uint32_t)] = passwordForPeer.at(i);

                                uint8_t out[32U];
                                edac::SHA256 sha256;
                                sha256.buffer(in, (uint32_t)(size + sizeof(uint32_t)), out);

                                delete[] in;

                                // validate hash
                                bool validHash = false;
                                if (req->length >= 80) {
                                    validHash = true;
                                    for (uint8_t i = 0; i < 32U; i++) {
                                        if (peerHash[i] != out[i]) {
                                            validHash = false;
                                            break;
                                        }
                                    }
                                }

                                if (!validHash) {
                                    LogError(LOG_MASTER, "PEER %u requested enc. key inventory, but had invalid authentication, no response", peerId);
                                    break;
                                }
                            } else {
                                LogError(LOG_MASTER, "PEER %u requested enc. key inventory, but had invalid ACL, no response", peerId);
                                break;
                            }
                        }

                        // scope intentional
                        {
                            // get remote access password hash from the frame message
                            DECLARE_UINT8_ARRAY(remoteAccessHash, 32U);
                            ::memcpy(remoteAccessHash, req->buffer + 44U, 32U);

                            uint8_t remoteSalt[4U];
                            ::memset(remoteSalt, 0x00U, 4U);
                            ::memcpy(remoteSalt, req->buffer + 76U, 4U);

                            std::string remoteAccessPassword = network->m_host->m_cryptoLookup->getRemotePassword();

                            size_t size = remoteAccessPassword.size();
                            uint8_t* in = new uint8_t[size + sizeof(uint32_t)];
                            ::memcpy(in, remoteSalt, sizeof(uint32_t));
                            for (size_t i = 0U; i < size; i++)
                                in[i + sizeof(uint32_t)] = remoteAccessPassword.at(i);

                            uint8_t out[32U];
                            edac::SHA256 sha256;
                            sha256.buffer(in, (uint32_t)(size + sizeof(uint32_t)), out);

                            delete[] in;

                            // validate hash
                            bool validHash = false;
                            if (req->length >= 80) {
                                validHash = true;
                                for (uint8_t i = 0; i < 32U; i++) {
                                    if (remoteAccessHash[i] != out[i]) {
                                        validHash = false;
                                        break;
                                    }
                                }
                            }

                            if (!validHash) {
                                LogError(LOG_MASTER, "PEER %u requested enc. key inventory, but had invalid access authentication, no response", peerId);
                                break;
                            }
                        }

                        // scope intentional
                        {
                            // read entire file into buffer
                            std::stringstream b;
                            std::ifstream stream(network->m_host->m_cryptoLookup->filename(), std::ios::in | std::ios::binary);

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

                            PacketBuffer pkt(true, "Remote EKC, Key Inventory");
                            pkt.encode((uint8_t*)buffer, len);

                            LogInfoEx(LOG_REPL, "PEER %u Remote EKC, Key Inventory, blocks %u, streamId = %u", peerId, pkt.fragments.size(), streamId);
                            if (pkt.fragments.size() > 0U) {
                                for (auto frag : pkt.fragments) {
                                    network->writePeer(peerId, network->m_peerId, { NET_FUNC::KEYS_INVENTORY, NET_SUBFUNC::NOP }, 
                                        frag.second->data, FRAG_SIZE, 0U, streamId);
                                    Thread::sleep(60U); // pace block transmission
                                }
                            }

                            pkt.clear();
                        }
                    }
                }
                break;

            case NET_FUNC::KEYS_UPDATE:                                 // Encryption Key Container Update
                {
                    lookups::PeerId peerEntry = network->m_peerListLookup->find(peerId);
                    if (peerEntry.peerDefault()) {
                        LogError(LOG_MASTER, "PEER %u requested enc. key update but is not allowed, no response", peerId);
                        break;
                    } else {
                        if (!peerEntry.canRequestKeys()) {
                            LogError(LOG_MASTER, "PEER %u requested enc. key update but is not allowed, no response", peerId);
                            break;
                        }
                    }

                    // keys update operates differently from the rest of the network opcodes...and does not require
                    // an established connection to the master, so we will not validate the peer connection state here.
                    // update is a two-phase flow: (1) auth request frame (80 bytes), then (2) chunked PacketBuffer frames.
                    if (peerId > 0 && !peerEntry.peerDefault()) {
                        if (mdNetwork->m_peerKeyUpdatePkt.find(peerId) == mdNetwork->m_peerKeyUpdatePkt.end()) {
                            if (req->length < 80) {
                                LogError(LOG_MASTER, "PEER %u requested enc. key update, but payload length was invalid (%u bytes), no response", peerId, req->length);
                                break;
                            }

                            // scope intentional
                            {
                                // get the peer password hash from the frame message
                                DECLARE_UINT8_ARRAY(peerHash, 32U);
                                ::memcpy(peerHash, req->buffer + 8U, 32U);

                                uint8_t peerSalt[4U];
                                ::memset(peerSalt, 0x00U, 4U);
                                ::memcpy(peerSalt, req->buffer + 40U, 4U);

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
                                    ::memcpy(in, peerSalt, sizeof(uint32_t));
                                    for (size_t i = 0U; i < size; i++)
                                        in[i + sizeof(uint32_t)] = passwordForPeer.at(i);

                                    uint8_t out[32U];
                                    edac::SHA256 sha256;
                                    sha256.buffer(in, (uint32_t)(size + sizeof(uint32_t)), out);

                                    delete[] in;

                                    // validate hash
                                    bool validHash = false;
                                    if (req->length >= 80) {
                                        validHash = true;
                                        for (uint8_t i = 0; i < 32U; i++) {
                                            if (peerHash[i] != out[i]) {
                                                validHash = false;
                                                break;
                                            }
                                        }
                                    }

                                    if (!validHash) {
                                        LogError(LOG_MASTER, "PEER %u requested enc. key update, but had invalid authentication, no response", peerId);
                                        break;
                                    }
                                } else {
                                    LogError(LOG_MASTER, "PEER %u requested enc. key update, but had invalid ACL, no response", peerId);
                                    break;
                                }
                            }

                            // scope intentional
                            {
                                // get remote access password hash from the frame message
                                DECLARE_UINT8_ARRAY(remoteAccessHash, 32U);
                                ::memcpy(remoteAccessHash, req->buffer + 44U, 32U);

                                uint8_t remoteSalt[4U];
                                ::memset(remoteSalt, 0x00U, 4U);
                                ::memcpy(remoteSalt, req->buffer + 76U, 4U);

                                std::string remoteAccessPassword = network->m_host->m_cryptoLookup->getRemotePassword();

                                size_t size = remoteAccessPassword.size();
                                uint8_t* in = new uint8_t[size + sizeof(uint32_t)];
                                ::memcpy(in, remoteSalt, sizeof(uint32_t));
                                for (size_t i = 0U; i < size; i++)
                                    in[i + sizeof(uint32_t)] = remoteAccessPassword.at(i);

                                uint8_t out[32U];
                                edac::SHA256 sha256;
                                sha256.buffer(in, (uint32_t)(size + sizeof(uint32_t)), out);

                                delete[] in;

                                // validate hash
                                bool validHash = false;
                                if (req->length >= 80) {
                                    validHash = true;
                                    for (uint8_t i = 0; i < 32U; i++) {
                                        if (remoteAccessHash[i] != out[i]) {
                                            validHash = false;
                                            break;
                                        }
                                    }
                                }

                                if (!validHash) {
                                    LogError(LOG_MASTER, "PEER %u requested enc. key update, but had invalid access authentication, no response", peerId);
                                    break;
                                }
                            }

                            mdNetwork->m_peerKeyUpdatePkt.insert(peerId, MetadataNetwork::PacketBufferEntry());
                            MetadataNetwork::PacketBufferEntry& pkt = mdNetwork->m_peerKeyUpdatePkt[peerId];
                            pkt.buffer = new PacketBuffer(true, "Remote EKC, Key Update");
                            pkt.streamId = streamId;
                            pkt.locked = false;
                            pkt.timeout = 0U;

                            LogInfoEx(LOG_REPL, "PEER %u Remote EKC, Key Update, authenticated transfer streamId = %u", peerId, streamId);
                            break;
                        }

                        // scope intentional
                        {
                            DECLARE_UINT8_ARRAY(rawPayload, req->length);
                            ::memcpy(rawPayload, req->buffer, req->length);

                            // Utils::dump(1U, "MetadataNetwork::taskNetworkRx(), KEYS_UPDATE, Raw Payload", rawPayload, req->length);

                            MetadataNetwork::PacketBufferEntry& pkt = mdNetwork->m_peerKeyUpdatePkt[peerId];
                            if (!pkt.locked && pkt.streamId != streamId) {
                                LogError(LOG_REPL, "PEER %u Remote EKC, Key Update, stream ID mismatch, expected %u, got %u", peerId, pkt.streamId, streamId);
                                pkt.buffer->clear();
                                delete pkt.buffer;
                                pkt.streamId = 0U;
                                mdNetwork->m_peerKeyUpdatePkt.erase(peerId);
                                break;
                            }

                            if (pkt.streamId != streamId) {
                                // otherwise drop the packet
                                break;
                            }

                            if (pkt.locked) {
                                while (pkt.locked && pkt.timeout < TIMEOUT_MAX_REPL) {
                                    pkt.timeout++;
                                    Thread::sleep(1U);
                                }

                                if (pkt.timeout >= TIMEOUT_MAX_REPL) {
                                    LogError(LOG_STP, "PEER %u Remote EKC, Key Update, timeout waiting for packet buffer to unlock", peerId);
                                    pkt.buffer->clear();
                                    delete pkt.buffer;
                                    pkt.streamId = 0U;
                                    mdNetwork->m_peerKeyUpdatePkt.erase(peerId);
                                    break;
                                }
                            }

                            pkt.locked = true;
                            pkt.timeout = 0U;

                            uint32_t decompressedLen = 0U;
                            uint8_t* decompressed = nullptr;

                            if (pkt.buffer->decode(rawPayload, &decompressed, &decompressedLen)) {
                                mdNetwork->m_peerKeyUpdatePkt.lock();
                                std::ostringstream s;
                                s << network->m_cryptoLookup->filename();

                                std::string filename = s.str();
                                std::ofstream file(filename, std::ofstream::out);
                                if (file.fail()) {
                                    LogError(LOG_PEER, "Cannot open the crypto container file - %s", filename.c_str());
                                    pkt.buffer->clear();
                                    delete pkt.buffer;
                                    pkt.streamId = 0U;
                                    if (decompressed != nullptr) {
                                        delete[] decompressed;
                                    }
                                    mdNetwork->m_peerKeyUpdatePkt.unlock();
                                    mdNetwork->m_peerKeyUpdatePkt.erase(peerId);
                                    break;
                                }

                                for (uint32_t i = 0U; i < decompressedLen; i++) {
                                    file << (char)decompressed[i];
                                }

                                file.close();

                                network->m_cryptoLookup->stop(true);
                                network->m_cryptoLookup->reload();

                                pkt.buffer->clear();
                                delete pkt.buffer;
                                pkt.streamId = 0U;
                                if (decompressed != nullptr) {
                                    delete[] decompressed;
                                }
                                mdNetwork->m_peerKeyUpdatePkt.unlock();
                                mdNetwork->m_peerKeyUpdatePkt.erase(peerId);
                            } else {
                                pkt.locked = false;
                            }
                        }
                    }
                }
                break;

            case NET_FUNC::REPL:
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

                                if (mdNetwork->m_peerReplicaActPkt.find(peerId) == mdNetwork->m_peerReplicaActPkt.end()) {
                                    mdNetwork->m_peerReplicaActPkt.insert(peerId, MetadataNetwork::PacketBufferEntry());

                                    MetadataNetwork::PacketBufferEntry& pkt = mdNetwork->m_peerReplicaActPkt[peerId];
                                    pkt.buffer = new PacketBuffer(true, "Peer Replication, Active Peer List");
                                    pkt.streamId = streamId;

                                    pkt.locked = false;
                                } else {
                                    MetadataNetwork::PacketBufferEntry& pkt = mdNetwork->m_peerReplicaActPkt[peerId];
                                    if (!pkt.locked && pkt.streamId != streamId) {
                                        LogError(LOG_REPL, "PEER %u (%s) Peer Replication, Active Peer List, stream ID mismatch, expected %u, got %u", peerId,
                                            connection->identWithQualifier().c_str(), pkt.streamId, streamId);
                                        pkt.buffer->clear();
                                        pkt.streamId = streamId;
                                    }

                                    if (pkt.streamId != streamId) {
                                        // otherwise drop the packet
                                        break;
                                    }
                                }

                                MetadataNetwork::PacketBufferEntry& pkt = mdNetwork->m_peerReplicaActPkt[peerId];
                                if (pkt.locked) {
                                    while (pkt.locked && pkt.timeout < TIMEOUT_MAX_REPL) {
                                        pkt.timeout++;
                                        Thread::sleep(1U);
                                    }

                                    if (pkt.timeout >= TIMEOUT_MAX_REPL) {
                                        LogError(LOG_STP, "PEER %u (%s) Peer Replication, Active Peer List, timeout waiting for packet buffer to unlock", peerId,
                                            connection->identWithQualifier().c_str());
                                        pkt.buffer->clear();
                                        pkt.streamId = 0U;
                                        mdNetwork->m_peerReplicaActPkt.erase(peerId);
                                        break;
                                    }
                                }

                                pkt.locked = true;
                                pkt.timeout = 0U;

                                uint32_t decompressedLen = 0U;
                                uint8_t* decompressed = nullptr;

                                if (pkt.buffer->decode(rawPayload, &decompressed, &decompressedLen)) {
                                    mdNetwork->m_peerReplicaActPkt.lock();
                                    std::string payload(decompressed + 8U, decompressed + decompressedLen);

                                    // parse JSON body
                                    json::value v;
                                    std::string err = json::parse(v, payload);
                                    if (!err.empty()) {
                                        LogError(LOG_REPL, "PEER %u (%s) error parsing active peer list, %s", peerId, connection->identWithQualifier().c_str(), err.c_str());
                                        pkt.buffer->clear();
                                        pkt.streamId = 0U;
                                        if (decompressed != nullptr) {
                                            delete[] decompressed;
                                        }
                                        mdNetwork->m_peerReplicaActPkt.unlock();
                                        mdNetwork->m_peerReplicaActPkt.erase(peerId);
                                        break;
                                    }
                                    else  {
                                        // ensure parsed JSON is an array
                                        if (!v.is<json::array>()) {
                                            LogError(LOG_REPL, "PEER %u (%s) error parsing active peer list, data was not valid", peerId, connection->identWithQualifier().c_str());
                                            pkt.buffer->clear();
                                            delete pkt.buffer;
                                            pkt.streamId = 0U;
                                            if (decompressed != nullptr) {
                                                delete[] decompressed;
                                            }
                                            mdNetwork->m_peerReplicaActPkt.unlock();
                                            mdNetwork->m_peerReplicaActPkt.erase(peerId);
                                            break;
                                        }
                                        else {
                                            json::array arr = v.get<json::array>();
                                            LogInfoEx(LOG_REPL, "PEER %u (%s) Peer Replication, Active Peer List, updating %u peer entries", peerId, connection->identWithQualifier().c_str(), arr.size());
                                            network->m_peerReplicaPeers[peerId] = arr;
                                        }
                                    }

                                    pkt.buffer->clear();
                                    delete pkt.buffer;
                                    pkt.streamId = 0U;
                                    if (decompressed != nullptr) {
                                        delete[] decompressed;
                                    }
                                    mdNetwork->m_peerReplicaActPkt.unlock();
                                    mdNetwork->m_peerReplicaActPkt.erase(peerId);
                                } else {
                                    pkt.locked = false;
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
                                            LogDebugEx(LOG_REPL, "MetadataNetwork::taskNetworkRx", "PEER %u (%s) Peer Replication, HA Parameters, %s:%u", peerId, connection->identWithQualifier().c_str(),
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
                break;

            case NET_FUNC::NET_TREE:
                if (!network->m_enableSpanningTree)
                    break;
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

                                if (mdNetwork->m_peerTreeListPkt.find(peerId) == mdNetwork->m_peerTreeListPkt.end()) {
                                    mdNetwork->m_peerTreeListPkt.insert(peerId, MetadataNetwork::PacketBufferEntry());

                                    MetadataNetwork::PacketBufferEntry& pkt = mdNetwork->m_peerTreeListPkt[peerId];
                                    pkt.buffer = new PacketBuffer(true, "Network Tree, Tree List");
                                    pkt.streamId = streamId;

                                    pkt.locked = false;
                                } else {
                                    MetadataNetwork::PacketBufferEntry& pkt = mdNetwork->m_peerTreeListPkt[peerId];
                                    if (!pkt.locked && pkt.streamId != streamId) {
                                        LogError(LOG_STP, "PEER %u (%s) Network Tree, Tree List, stream ID mismatch, expected %u, got %u", peerId,
                                            connection->identWithQualifier().c_str(), pkt.streamId, streamId);
                                        pkt.buffer->clear();
                                        pkt.streamId = streamId;
                                    }

                                    if (pkt.streamId != streamId) {
                                        // otherwise drop the packet
                                        break;
                                    }
                                }

                                MetadataNetwork::PacketBufferEntry& pkt = mdNetwork->m_peerTreeListPkt[peerId];
                                if (pkt.locked) {
                                    while (pkt.locked && pkt.timeout < TIMEOUT_MAX_REPL) {
                                        pkt.timeout++;
                                        Thread::sleep(1U);
                                    }

                                    if (pkt.timeout >= TIMEOUT_MAX_REPL) {
                                        LogError(LOG_STP, "PEER %u (%s) Network Tree, Tree List, timeout waiting for packet buffer to unlock", peerId,
                                            connection->identWithQualifier().c_str());
                                        pkt.buffer->clear();
                                        pkt.streamId = 0U;
                                        mdNetwork->m_peerTreeListPkt.erase(peerId);
                                        break;
                                    }
                                }

                                pkt.locked = true;
                                pkt.timeout = 0U;

                                uint32_t decompressedLen = 0U;
                                uint8_t* decompressed = nullptr;

                                if (pkt.buffer->decode(rawPayload, &decompressed, &decompressedLen)) {
                                    mdNetwork->m_peerTreeListPkt.lock();
                                    std::string payload(decompressed + 8U, decompressed + decompressedLen);

                                    // parse JSON body
                                    json::value v;
                                    std::string err = json::parse(v, payload);
                                    if (!err.empty()) {
                                        LogError(LOG_STP, "PEER %u (%s) error parsing network tree list, %s", peerId, connection->identWithQualifier().c_str(), err.c_str());
                                        pkt.buffer->clear();
                                        pkt.streamId = 0U;
                                        if (decompressed != nullptr) {
                                            delete[] decompressed;
                                        }
                                        mdNetwork->m_peerTreeListPkt.unlock();
                                        mdNetwork->m_peerTreeListPkt.erase(peerId);
                                        break;
                                    }
                                    else  {
                                        // ensure parsed JSON is an array
                                        if (!v.is<json::array>()) {
                                            LogError(LOG_STP, "PEER %u (%s) error parsing network tree list, data was not valid", peerId, connection->identWithQualifier().c_str());
                                            pkt.buffer->clear();
                                            delete pkt.buffer;
                                            pkt.streamId = 0U;
                                            if (decompressed != nullptr) {
                                                delete[] decompressed;
                                            }
                                            mdNetwork->m_peerTreeListPkt.unlock();
                                            mdNetwork->m_peerTreeListPkt.erase(peerId);
                                            break;
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

                                    pkt.buffer->clear();
                                    delete pkt.buffer;
                                    pkt.streamId = 0U;
                                    if (decompressed != nullptr) {
                                        delete[] decompressed;
                                    }
                                    mdNetwork->m_peerTreeListPkt.unlock();
                                    mdNetwork->m_peerTreeListPkt.erase(peerId);
                                } else {
                                    pkt.locked = false;
                                }
                            }
                            else {
                                network->writePeerNAK(peerId, 0U, TAG_PEER_REPLICA, NET_CONN_NAK_FNE_UNAUTHORIZED);
                            }
                        }
                    }
                }
                break;

            default:
                // metadata network ignores unknowns for everything else...
                break;
            }
        }

        if (req->buffer != nullptr)
            delete[] req->buffer;
        delete req;
    }
}
