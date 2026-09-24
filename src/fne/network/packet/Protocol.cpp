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
#include "network/callhandler/TagDMRData.h"
#include "network/callhandler/TagP25Data.h"
#include "network/callhandler/TagP25P2Data.h"
#include "network/callhandler/TagNXDNData.h"
#include "common/nxdn/NXDNDefines.h"
#include "network/callhandler/TagAnalogData.h"
#include "fne/ActivityLog.h"
#include "HostFNE.h"

using namespace network;
using namespace network::callhandler;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Handles NET_FUNC::PROTOCOL packets. */

void TrafficNetwork::PacketHandler::protocol(TrafficNetwork* network, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId, uint64_t now)
{
    // process incoming message subfunction opcodes
    switch (req->fneHeader.getSubFunction()) {
    case NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR:             // Encapsulated DMR data frame
        {
            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                FNEPeerConnection* connection = network->m_peers[peerId];
                if (connection != nullptr) {
                    std::string ip = udp::Socket::address(req->address);
                    connection->lastPing(now);

                    // validate peer (simple validation really)
                    if (connection->connected() && connection->address() == ip) {
                        if (network->m_dmrEnabled) {
                            if (network->m_tagDMR != nullptr) {
                                // check if jitter buffer is enabled for this peer
                                if (connection->jitterBufferEnabled() && req->rtpHeader.getSequence() != RTP_END_OF_CALL_SEQ) {
                                    std::vector<BufferedFrame*> readyFrames;

                                    connection->processJitterFrame(streamId, req->rtpHeader.getSequence(), req->buffer, req->length, readyFrames);

                                    // process all frames that are now ready (in sequence order)
                                    for (BufferedFrame* frame : readyFrames) {
                                        network->m_tagDMR->processFrame(frame->data, frame->length, peerId, ssrc, frame->seq, streamId);
                                        delete frame;
                                    }
                                } else {
                                    // zero-latency fast path: no jitter buffer
                                    network->m_tagDMR->processFrame(req->buffer, req->length, peerId, ssrc, req->rtpHeader.getSequence(), streamId);
                                }
                            }
                        } else {
                            network->writePeerNAK(peerId, streamId, TAG_DMR_DATA, NET_CONN_NAK_MODE_NOT_ENABLED);
                        }
                    }
                }
            }
            else {
                network->writePeerNAK(peerId, TAG_DMR_DATA, NET_CONN_NAK_FNE_UNAUTHORIZED, req->address, req->addrLen);
            }
        }
        break;

    case NET_SUBFUNC::PROTOCOL_SUBFUNC_P25:             // Encapsulated P25 data frame
        {
            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                FNEPeerConnection* connection = network->m_peers[peerId];
                if (connection != nullptr) {
                    std::string ip = udp::Socket::address(req->address);
                    connection->lastPing(now);

                    // validate peer (simple validation really)
                    if (connection->connected() && connection->address() == ip) {
                        if (network->m_p25Enabled) {
                            if (network->m_tagP25 != nullptr) {
                                // check if jitter buffer is enabled for this peer
                                if (connection->jitterBufferEnabled() && req->rtpHeader.getSequence() != RTP_END_OF_CALL_SEQ) {
                                    std::vector<BufferedFrame*> readyFrames;

                                    connection->processJitterFrame(streamId, req->rtpHeader.getSequence(), req->buffer, req->length, readyFrames);

                                    // process all frames that are now ready (in sequence order)
                                    for (BufferedFrame* frame : readyFrames) {
                                        network->m_tagP25->processFrame(frame->data, frame->length, peerId, ssrc, frame->seq, streamId);
                                        delete frame;
                                    }
                                } else {
                                    // zero-latency fast path: no jitter buffer
                                    network->m_tagP25->processFrame(req->buffer, req->length, peerId, ssrc, req->rtpHeader.getSequence(), streamId);
                                }
                            }
                        } else {
                            network->writePeerNAK(peerId, streamId, TAG_P25_DATA, NET_CONN_NAK_MODE_NOT_ENABLED);
                        }
                    }
                }
            }
            else {
                network->writePeerNAK(peerId, TAG_P25_DATA, NET_CONN_NAK_FNE_UNAUTHORIZED, req->address, req->addrLen);
            }
        }
        break;

    case NET_SUBFUNC::PROTOCOL_SUBFUNC_P25_P2:          // Encapsulated P25 Phase 2 data frame
        {
            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                FNEPeerConnection* connection = network->m_peers[peerId];
                if (connection != nullptr) {
                    std::string ip = udp::Socket::address(req->address);
                    connection->lastPing(now);

                    if (connection->connected() && connection->address() == ip) {
                        if (network->m_p25Enabled) {
                            if (network->m_tagP25P2 != nullptr) {
                                if (connection->jitterBufferEnabled() && req->rtpHeader.getSequence() != RTP_END_OF_CALL_SEQ) {
                                    std::vector<BufferedFrame*> readyFrames;
                                    connection->processJitterFrame(streamId, req->rtpHeader.getSequence(),
                                        req->buffer, req->length, readyFrames);
                                    for (BufferedFrame* frame : readyFrames) {
                                        network->m_tagP25P2->processFrame(frame->data, frame->length,
                                            peerId, ssrc, frame->seq, streamId);
                                        delete frame;
                                    }
                                }
                                else {
                                    network->m_tagP25P2->processFrame(req->buffer, req->length,
                                        peerId, ssrc, req->rtpHeader.getSequence(), streamId);
                                }
                            }
                        }
                        else {
                            network->writePeerNAK(peerId, streamId, TAG_P25_DATA, NET_CONN_NAK_MODE_NOT_ENABLED);
                        }
                    }
                }
            }
            else {
                network->writePeerNAK(peerId, TAG_P25_DATA, NET_CONN_NAK_FNE_UNAUTHORIZED,
                    req->address, req->addrLen);
            }
        }
        break;

    case NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN:            // Encapsulated NXDN data frame
        {
            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                FNEPeerConnection* connection = network->m_peers[peerId];
                if (connection != nullptr) {
                    std::string ip = udp::Socket::address(req->address);
                    connection->lastPing(now);

                    // validate peer (simple validation really)
                    if (connection->connected() && connection->address() == ip) {
                        if (network->m_nxdnEnabled) {
                            if (network->m_tagNXDN != nullptr) {
                                // check if jitter buffer is enabled for this peer
                                if (connection->jitterBufferEnabled() && req->rtpHeader.getSequence() != RTP_END_OF_CALL_SEQ) {
                                    std::vector<BufferedFrame*> readyFrames;

                                    connection->processJitterFrame(streamId, req->rtpHeader.getSequence(), req->buffer, req->length, readyFrames);

                                    // process all frames that are now ready (in sequence order)
                                    for (BufferedFrame* frame : readyFrames) {
                                        network->m_tagNXDN->processFrame(frame->data, frame->length, peerId, ssrc, frame->seq, streamId);
                                        delete frame;
                                    }
                                } else {
                                    // zero-latency fast path: no jitter buffer
                                    network->m_tagNXDN->processFrame(req->buffer, req->length, peerId, ssrc, req->rtpHeader.getSequence(), streamId);
                                }
                            }
                        } else {
                            network->writePeerNAK(peerId, streamId, TAG_NXDN_DATA, NET_CONN_NAK_MODE_NOT_ENABLED);
                        }
                    }
                }
            }
            else {
                network->writePeerNAK(peerId, TAG_NXDN_DATA, NET_CONN_NAK_FNE_UNAUTHORIZED, req->address, req->addrLen);
            }
        }
        break;

    case NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG:          // Encapsulated analog data frame
        {
            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                FNEPeerConnection* connection = network->m_peers[peerId];
                if (connection != nullptr) {
                    std::string ip = udp::Socket::address(req->address);
                    connection->lastPing(now);

                    // validate peer (simple validation really)
                    if (connection->connected() && connection->address() == ip) {
                        if (network->m_analogEnabled) {
                            if (network->m_tagAnalog != nullptr) {
                                // check if jitter buffer is enabled for this peer
                                if (connection->jitterBufferEnabled() && req->rtpHeader.getSequence() != RTP_END_OF_CALL_SEQ) {
                                    std::vector<BufferedFrame*> readyFrames;

                                    connection->processJitterFrame(streamId, req->rtpHeader.getSequence(), req->buffer, req->length, readyFrames);

                                    // process all frames that are now ready (in sequence order)
                                    for (BufferedFrame* frame : readyFrames) {
                                        network->m_tagAnalog->processFrame(frame->data, frame->length, peerId, ssrc, frame->seq, streamId);
                                        delete frame;
                                    }
                                } else {
                                    // zero-latency fast path: no jitter buffer
                                    network->m_tagAnalog->processFrame(req->buffer, req->length, peerId, ssrc, req->rtpHeader.getSequence(), streamId);
                                }
                            }
                        } else {
                            network->writePeerNAK(peerId, streamId, TAG_ANALOG_DATA, NET_CONN_NAK_MODE_NOT_ENABLED);
                        }
                    }
                }
            }
            else {
                network->writePeerNAK(peerId, TAG_ANALOG_DATA, NET_CONN_NAK_FNE_UNAUTHORIZED, req->address, req->addrLen);
            }
        }
        break;

    default:
        Utils::dump("Unknown protocol opcode from peer", req->buffer, req->length);
        break;
    }
}
