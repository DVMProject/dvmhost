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
#include "network/callhandler/TagNXDNData.h"
#include "fne/ActivityLog.h"
#include "HostFNE.h"

using namespace network;
using namespace network::callhandler;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Handles NET_FUNC::GRANT_REQ packets. */

void TrafficNetwork::PacketHandler::grantRequest(TrafficNetwork* network, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId, uint64_t now)
{
    TrafficNetwork::PeerConnectionPtr connection = network->findPeer(peerId);
    if (peerId > 0 && connection != nullptr) {
        std::string ip = udp::Socket::address(req->address);

        // validate peer (simple validation really)
        if (connection->connected() && connection->address() == ip) {
            uint32_t srcId = GET_UINT24(req->buffer, 11U);                  // Source Address
            uint32_t dstId = GET_UINT24(req->buffer, 15U);                  // Destination Address

            uint8_t slot = req->buffer[19U];

            bool unitToUnit = (req->buffer[19U] & 0x80U) == 0x80U;

            DVM_STATE state = (DVM_STATE)req->buffer[20U];                  // DVM Mode State
            switch (state) {
            case STATE_DMR:
                if (network->m_dmrEnabled) {
                    if (network->m_tagDMR != nullptr) {
                        network->m_tagDMR->processGrantReq(srcId, dstId, slot, unitToUnit, peerId, req->rtpHeader.getSequence(), streamId);
                    } else {
                        network->writePeerNAK(peerId, streamId, TAG_DMR_DATA, NET_CONN_NAK_MODE_NOT_ENABLED);
                    }
                }
                break;
            case STATE_P25:
                if (network->m_p25Enabled) {
                    if (network->m_tagP25 != nullptr) {
                        network->m_tagP25->processGrantReq(srcId, dstId, unitToUnit, peerId, req->rtpHeader.getSequence(), streamId);
                    } else {
                        network->writePeerNAK(peerId, streamId, TAG_P25_DATA, NET_CONN_NAK_MODE_NOT_ENABLED);
                    }
                }
                break;
            case STATE_NXDN:
                if (network->m_nxdnEnabled) {
                    if (network->m_tagNXDN != nullptr) {
                        network->m_tagNXDN->processGrantReq(srcId, dstId, unitToUnit, peerId, req->rtpHeader.getSequence(), streamId);
                    } else {
                        network->writePeerNAK(peerId, streamId, TAG_NXDN_DATA, NET_CONN_NAK_MODE_NOT_ENABLED);
                    }
                }
                break;
            default:
                network->writePeerNAK(peerId, streamId, TAG_REPEATER_GRANT, NET_CONN_NAK_ILLEGAL_PACKET);
                Utils::dump("Unknown state for grant request from the peer", req->buffer, req->length);
                break;
            }
        }
    }
}
