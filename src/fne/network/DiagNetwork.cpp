// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Converged FNE Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Converged FNE Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023-2024 Bryan Biedenkapp, N2PLL
*
*/
#include "fne/Defines.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "network/DiagNetwork.h"
#include "fne/ActivityLog.h"
#include "HostFNE.h"

using namespace network;
using namespace network::fne;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the DiagNetwork class.
/// </summary>
/// <param name="host"></param>
/// <param name="network"></param>
/// <param name="address">Network Hostname/IP address to listen on.</param>
/// <param name="port">Network port number.</param>
DiagNetwork::DiagNetwork(HostFNE* host, FNENetwork* fneNetwork, const std::string& address, uint16_t port) :
    BaseNetwork(fneNetwork->m_peerId, true, fneNetwork->m_debug, true, true, fneNetwork->m_allowActivityTransfer, fneNetwork->m_allowDiagnosticTransfer),
    m_fneNetwork(fneNetwork),
    m_host(host),
    m_address(address),
    m_port(port)
{
    assert(fneNetwork != nullptr);
    assert(host != nullptr);
    assert(!address.empty());
    assert(port > 0U);
}

/// <summary>
/// Finalizes a instance of the DiagNetwork class.
/// </summary>
DiagNetwork::~DiagNetwork() = default;

/// <summary>
/// Sets endpoint preshared encryption key.
/// </summary>
void DiagNetwork::setPresharedKey(const uint8_t* presharedKey)
{
    m_socket->setPresharedKey(presharedKey);
}

/// <summary>
/// Process a data frames from the network.
/// </summary>
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
            Utils::dump(1U, "Network Message", buffer.get(), length);

        uint32_t peerId = fneHeader.getPeerId();

        NetPacketRequest* req = new NetPacketRequest();
        req->network = m_fneNetwork;
        req->peerId = peerId;

        req->address = address;
        req->addrLen = addrLen;
        req->rtpHeader = rtpHeader;
        req->fneHeader = fneHeader;

        req->length = length;
        req->buffer = new uint8_t[length];
        ::memcpy(req->buffer, buffer.get(), length);

        if (::pthread_create(&req->thread, NULL, threadedNetworkRx, req) != 0) {
            LogError(LOG_NET, "Error returned from pthread_create, err: %d", errno);
            delete req;
            return;
        }
    }
}

/// <summary>
/// Updates the timer by the passed number of milliseconds.
/// </summary>
/// <param name="ms"></param>
void DiagNetwork::clock(uint32_t ms)
{
    if (m_status != NET_STAT_MST_RUNNING) {
        return;
    }
}

/// <summary>
/// Opens connection to the network.
/// </summary>
/// <returns></returns>
bool DiagNetwork::open()
{
    if (m_debug)
        LogMessage(LOG_NET, "Opening Network");

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

/// <summary>
/// Closes connection to the network.
/// </summary>
void DiagNetwork::close()
{
    if (m_debug)
        LogMessage(LOG_NET, "Closing Network");

    m_socket->close();

    m_status = NET_STAT_INVALID;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Process a data frames from the network.
/// </summary>
void* DiagNetwork::threadedNetworkRx(void* arg)
{
    NetPacketRequest* req = (NetPacketRequest*)arg;
    if (req != nullptr) {
        ::pthread_detach(req->thread);

        FNENetwork* network = req->network;
        if (req->length > 0) {
            uint32_t peerId = req->fneHeader.getPeerId();
            uint32_t streamId = req->fneHeader.getStreamId();

            std::stringstream peerName;
            peerName << peerId << ":diag-rx-pckt";
            if (pthread_kill(req->thread, 0) == 0) {
                ::pthread_setname_np(req->thread, peerName.str().c_str());
            }

            // update current peer packet sequence and stream ID
            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end()) && streamId != 0U) {
                FNEPeerConnection* connection = network->m_peers[peerId];
                uint16_t pktSeq = req->rtpHeader.getSequence();

                if (connection != nullptr) {
                    if (pktSeq == RTP_END_OF_CALL_SEQ) {
                        connection->pktLastSeq(pktSeq);
                        connection->pktNextSeq(0U);
                    } else {
                        if ((connection->currStreamId() == streamId) && (pktSeq != connection->pktNextSeq()) && (pktSeq != (RTP_END_OF_CALL_SEQ - 1U))) {
                            LogWarning(LOG_NET, "PEER %u stream %u out-of-sequence; %u != %u", peerId, streamId, pktSeq, connection->pktNextSeq());
                        }

                        connection->currStreamId(streamId);
                        connection->pktLastSeq(pktSeq);
                        connection->pktNextSeq(pktSeq + 1);
                        if (connection->pktNextSeq() > (RTP_END_OF_CALL_SEQ - 1U)) {
                            connection->pktNextSeq(0U);
                        }
                    }
                }

                network->m_peers[peerId] = connection;
            }

            // process incoming message frame opcodes
            switch (req->fneHeader.getFunction()) {
            case NET_FUNC_TRANSFER:
                {
                    if (req->fneHeader.getSubFunction() == NET_TRANSFER_SUBFUNC_ACTIVITY) {     // Peer Activity Log Transfer
                        if (network->m_allowActivityTransfer) {
                            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                                FNEPeerConnection* connection = network->m_peers[peerId];
                                if (connection != nullptr) {
                                    std::string ip = udp::Socket::address(req->address);

                                    // validate peer (simple validation really)
                                    if (connection->connected() && connection->address() == ip) {
                                        uint8_t rawPayload[req->length - 11U];
                                        ::memset(rawPayload, 0x00U, req->length - 11U);
                                        ::memcpy(rawPayload, req->buffer + 11U, req->length - 11U);
                                        std::string payload(rawPayload, rawPayload + (req->length - 11U));

                                        ::ActivityLog("%u %s", peerId, payload.c_str());
                                    }
                                    else {
                                        network->writePeerNAK(peerId, TAG_TRANSFER_ACT_LOG, NET_CONN_NAK_FNE_UNAUTHORIZED);
                                    }
                                }
                            }
                        }
                    }
                    else if (req->fneHeader.getSubFunction() == NET_TRANSFER_SUBFUNC_DIAG) {    // Peer Diagnostic Log Transfer
                        if (network->m_allowDiagnosticTransfer) {
                            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                                FNEPeerConnection* connection = network->m_peers[peerId];
                                if (connection != nullptr) {
                                    std::string ip = udp::Socket::address(req->address);

                                    // validate peer (simple validation really)
                                    if (connection->connected() && connection->address() == ip) {
                                        uint8_t rawPayload[req->length - 11U];
                                        ::memset(rawPayload, 0x00U, req->length - 11U);
                                        ::memcpy(rawPayload, req->buffer + 11U, req->length - 11U);
                                        std::string payload(rawPayload, rawPayload + (req->length - 11U));

                                        bool currState = g_disableTimeDisplay;
                                        g_disableTimeDisplay = true;
                                        ::Log(9999U, nullptr, "%u %s", peerId, payload.c_str());
                                        g_disableTimeDisplay = currState;
                                    }
                                    else {
                                        network->writePeerNAK(peerId, TAG_TRANSFER_DIAG_LOG, NET_CONN_NAK_FNE_UNAUTHORIZED);
                                    }
                                }
                            }
                        }
                    }
                    else {
                        network->writePeerNAK(peerId, TAG_TRANSFER, NET_CONN_NAK_ILLEGAL_PACKET);
                        Utils::dump("unknown transfer opcode from the peer", req->buffer, req->length);
                    }
                }
                break;

            default:
                // diagostic network ignores unknowns for everything else...
                break;
            }
        }

        if (req->buffer != nullptr)
            delete req->buffer;
        delete req;
    }

    return nullptr;
}