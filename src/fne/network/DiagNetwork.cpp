// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023-2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/Defines.h"
#include "common/zlib/Compression.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "network/DiagNetwork.h"
#include "fne/ActivityLog.h"
#include "HostFNE.h"

using namespace network;
using namespace network::callhandler;
using namespace compress;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the DiagNetwork class. */

DiagNetwork::DiagNetwork(HostFNE* host, FNENetwork* fneNetwork, const std::string& address, uint16_t port, uint16_t workerCnt) :
    BaseNetwork(fneNetwork->m_peerId, true, fneNetwork->m_debug, true, true, fneNetwork->m_allowActivityTransfer, fneNetwork->m_allowDiagnosticTransfer),
    m_fneNetwork(fneNetwork),
    m_host(host),
    m_address(address),
    m_port(port),
    m_threadPool(workerCnt, "diag")
{
    assert(fneNetwork != nullptr);
    assert(host != nullptr);
    assert(!address.empty());
    assert(port > 0U);
}

/* Finalizes a instance of the DiagNetwork class. */

DiagNetwork::~DiagNetwork() = default;

/* Sets endpoint preshared encryption key. */

void DiagNetwork::setPresharedKey(const uint8_t* presharedKey)
{
    m_socket->setPresharedKey(presharedKey);
}

/* Process a data frames from the network. */

void DiagNetwork::processNetwork()
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
            Utils::dump(1U, "DiagNetwork::processNetwork(), Network Message", buffer.get(), length);

        uint32_t peerId = fneHeader.getPeerId();

        NetPacketRequest* req = new NetPacketRequest();
        req->obj = m_fneNetwork;
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

void DiagNetwork::clock(uint32_t ms)
{
    if (m_status != NET_STAT_MST_RUNNING) {
        return;
    }
}

/* Opens connection to the network. */

bool DiagNetwork::open()
{
    if (m_debug)
        LogMessage(LOG_NET, "Opening Network");

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
        m_status = NET_STAT_INVALID;
    }

    return ret;
}

/* Closes connection to the network. */

void DiagNetwork::close()
{
    if (m_debug)
        LogMessage(LOG_NET, "Closing Network");

    m_threadPool.stop();
    m_threadPool.wait();
    
    m_socket->close();

    m_status = NET_STAT_INVALID;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Process a data frames from the network. */

void DiagNetwork::taskNetworkRx(NetPacketRequest* req)
{
    if (req != nullptr) {
        FNENetwork* network = static_cast<FNENetwork*>(req->obj);
        if (network == nullptr) {
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
                            // this could be a peer-link transfer -- in which case, we need to check the SSRC of the packet not the peer ID
                            if (network->m_peers.find(req->rtpHeader.getSSRC()) != network->m_peers.end()) {
                                FNEPeerConnection* connection = network->m_peers[req->rtpHeader.getSSRC()];
                                if (connection != nullptr) {
                                    if (connection->isExternalPeer() && connection->isPeerLink()) {
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

                                            ::ActivityLog("%.9u (%8s) %s", pktPeerId, connection->identity().c_str(), payload.c_str());

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
                                                        if (peer.second->isSysView()) {
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

                                            // attempt to repeat traffic to Peer-Link masters
                                            if (network->m_host->m_peerNetworks.size() > 0) {
                                                for (auto peer : network->m_host->m_peerNetworks) {
                                                    if (peer.second != nullptr) {
                                                        if (peer.second->isEnabled() && peer.second->isPeerLink()) {
                                                            peer.second->writeMaster({ NET_FUNC::TRANSFER, NET_SUBFUNC::TRANSFER_SUBFUNC_ACTIVITY }, 
                                                                req->buffer, req->length, RTP_END_OF_CALL_SEQ, 0U, false, true, pktPeerId);
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
                                            ::Log(9999U, nullptr, nullptr, 0U, nullptr, "%.9u (%8s) %s", peerId, connection->identity().c_str(), payload.c_str());
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
                                                    if (peer.second->isSysView()) {
                                                        sockaddr_storage addr = peer.second->socketStorage();
                                                        uint32_t addrLen = peer.second->sockStorageLen();

                                                        if (network->m_debug) {
                                                            LogDebug(LOG_NET, "SysView, srcPeer = %u, dstPeer = %u, peer status message, len = %u", 
                                                                pktPeerId, peer.first, req->length);
                                                        }
                                                        network->m_frameQueue->write(req->buffer, req->length, network->createStreamId(), pktPeerId, network->m_peerId, 
                                                            { NET_FUNC::TRANSFER, NET_SUBFUNC::TRANSFER_SUBFUNC_STATUS }, RTP_END_OF_CALL_SEQ, addr, addrLen);
                                                    }
                                                } else {
                                                    continue;
                                                }
                                            }

                                            // attempt to repeat status traffic to Peer-Link masters
                                            if (network->m_host->m_peerNetworks.size() > 0) {
                                                for (auto peer : network->m_host->m_peerNetworks) {
                                                    if (peer.second != nullptr) {
                                                        if (peer.second->isEnabled() && peer.second->isPeerLink()) {
                                                            peer.second->writeMaster({ NET_FUNC::TRANSFER, NET_SUBFUNC::TRANSFER_SUBFUNC_STATUS }, 
                                                                req->buffer, req->length, RTP_END_OF_CALL_SEQ, 0U, false, true, pktPeerId);
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
                        network->writePeerNAK(peerId, network->createStreamId(), TAG_TRANSFER, NET_CONN_NAK_ILLEGAL_PACKET);
                        Utils::dump("Unknown transfer opcode from the peer", req->buffer, req->length);
                        break;
                    }
                }
                break;

            case NET_FUNC::PEER_LINK:
                if (req->fneHeader.getSubFunction() == NET_SUBFUNC::PL_ACT_PEER_LIST) { // Peer-Link Active Peer List
                    if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                        FNEPeerConnection* connection = network->m_peers[peerId];
                        if (connection != nullptr) {
                            std::string ip = udp::Socket::address(req->address);

                            // validate peer (simple validation really)
                            if (connection->connected() && connection->address() == ip && connection->isExternalPeer() &&
                                connection->isPeerLink()) {
                                DECLARE_UINT8_ARRAY(rawPayload, req->length);
                                ::memcpy(rawPayload, req->buffer, req->length);

                                // Utils::dump(1U, "DiagNetwork::taskNetworkRx(), PEER_LINK, Raw Payload", rawPayload, req->length);

                                if (network->m_peerLinkActPkt.find(peerId) == network->m_peerLinkActPkt.end()) {
                                    network->m_peerLinkActPkt.insert(peerId, FNENetwork::PacketBufferEntry());

                                    FNENetwork::PacketBufferEntry& pkt = network->m_peerLinkActPkt[peerId];
                                    pkt.buffer = new PacketBuffer(true, "Peer-Link, Active Peer List");
                                    pkt.streamId = streamId;

                                    pkt.locked = false;
                                } else {
                                    FNENetwork::PacketBufferEntry& pkt = network->m_peerLinkActPkt[peerId];
                                    if (!pkt.locked && pkt.streamId != streamId) {
                                        LogError(LOG_NET, "PEER %u Peer-Link, Active Peer List, stream ID mismatch, expected %u, got %u", peerId, pkt.streamId, streamId);
                                        pkt.buffer->clear();
                                        pkt.streamId = streamId;
                                    }

                                    if (pkt.streamId != streamId) {
                                        // otherwise drop the packet
                                        break;
                                    }
                                }

                                FNENetwork::PacketBufferEntry& pkt = network->m_peerLinkActPkt[peerId];
                                if (pkt.locked) {
                                    while (pkt.locked)
                                        Thread::sleep(1U);
                                }

                                pkt.locked = true;

                                uint32_t decompressedLen = 0U;
                                uint8_t* decompressed = nullptr;

                                if (pkt.buffer->decode(rawPayload, &decompressed, &decompressedLen)) {
                                    std::string payload(decompressed + 8U, decompressed + decompressedLen);

                                    // parse JSON body
                                    json::value v;
                                    std::string err = json::parse(v, payload);
                                    if (!err.empty()) {
                                        LogError(LOG_NET, "PEER %u error parsing active peer list, %s", peerId, err.c_str());
                                        pkt.buffer->clear();
                                        pkt.streamId = 0U;
                                        if (decompressed != nullptr) {
                                            delete[] decompressed;
                                        }
                                        network->m_peerLinkActPkt.erase(peerId);
                                        break;
                                    }
                                    else  {
                                        // ensure parsed JSON is an array
                                        if (!v.is<json::array>()) {
                                            LogError(LOG_NET, "PEER %u error parsing active peer list, data was not valid", peerId);
                                            pkt.buffer->clear();
                                            delete pkt.buffer;
                                            pkt.streamId = 0U;
                                            if (decompressed != nullptr) {
                                                delete[] decompressed;
                                            }
                                            network->m_peerLinkActPkt.erase(peerId);
                                            break;
                                        }
                                        else {
                                            json::array arr = v.get<json::array>();
                                            LogInfoEx(LOG_NET, "PEER %u Peer-Link, Active Peer List, updating %u peer entries", peerId, arr.size());
                                            network->m_peerLinkPeers[peerId] = arr;
                                        }
                                    }

                                    pkt.buffer->clear();
                                    delete pkt.buffer;
                                    pkt.streamId = 0U;
                                    if (decompressed != nullptr) {
                                        delete[] decompressed;
                                    }
                                    network->m_peerLinkActPkt.erase(peerId);
                                } else {
                                    pkt.locked = false;
                                }
                            }
                            else {
                                network->writePeerNAK(peerId, 0U, TAG_PEER_LINK, NET_CONN_NAK_FNE_UNAUTHORIZED);
                            }
                        }
                    }
                }
                break;

            default:
                // diagostic network ignores unknowns for everything else...
                break;
            }
        }

        if (req->buffer != nullptr)
            delete[] req->buffer;
        delete req;
    }
}
