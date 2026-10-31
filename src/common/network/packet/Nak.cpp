// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2017-2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/Log.h"
#include "network/Network.h"

using namespace network;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Handles NET_FUNC::NAK packets. */

bool Network::PacketHandler::nak(Network* network, uint32_t peerId, uint32_t streamId, uint64_t now,
    const frame::RTPFNEHeader& fneHeader, const frame::RTPHeader& rtpHeader, const uint8_t* buffer, int length)
{
    (void)peerId;
    (void)streamId;
    (void)now;

    // DVM 3.6 adds support to respond with a NAK reason, as such we just check if the NAK response is greater
    // then 10 bytes and process the reason value
    uint16_t reason = NET_CONN_NAK_GENERAL_FAILURE;
    if (length > 10) {
        reason = GET_UINT16(buffer, 10U);
        switch (reason) {
        case NET_CONN_NAK_MODE_NOT_ENABLED:
            LogWarning(LOG_NET, "PEER %u master NAK; digital mode not enabled on FNE, remotePeerId = %u", network->m_peerId, rtpHeader.getSSRC());
            break;
        case NET_CONN_NAK_ILLEGAL_PACKET:
            LogWarning(LOG_NET, "PEER %u master NAK; illegal/unknown packet, remotePeerId = %u", network->m_peerId, rtpHeader.getSSRC());
            break;
        case NET_CONN_NAK_FNE_UNAUTHORIZED:
            LogWarning(LOG_NET, "PEER %u master NAK; unauthorized, remotePeerId = %u", network->m_peerId, rtpHeader.getSSRC());
            break;
        case NET_CONN_NAK_BAD_CONN_STATE:
            LogWarning(LOG_NET, "PEER %u master NAK; bad connection state, remotePeerId = %u", network->m_peerId, rtpHeader.getSSRC());
            break;
        case NET_CONN_NAK_INVALID_CONFIG_DATA:
            LogWarning(LOG_NET, "PEER %u master NAK; invalid configuration data, remotePeerId = %u", network->m_peerId, rtpHeader.getSSRC());
            break;
        case NET_CONN_NAK_FNE_MAX_CONN:
            LogWarning(LOG_NET, "PEER %u master NAK; FNE has reached maximum permitted connections, remotePeerId = %u", network->m_peerId, rtpHeader.getSSRC());
            break;
        case NET_CONN_NAK_PEER_RESET:
            LogWarning(LOG_NET, "PEER %u master NAK; FNE demanded connection reset, remotePeerId = %u", network->m_peerId, rtpHeader.getSSRC());
            break;
        case NET_CONN_NAK_PEER_ACL:
            LogError(LOG_NET, "PEER %u master NAK; ACL rejection, network disabled, remotePeerId = %u", network->m_peerId, rtpHeader.getSSRC());
            if (!network->m_neverDisableOnACLNAK) {
                network->m_status = NET_STAT_WAITING_LOGIN;
                network->m_enabled = false; // ACL rejection give up stop trying to connect
            }
            break;

        case NET_CONN_NAK_FNE_DUPLICATE_CONN:
            LogWarning(LOG_NET, "PEER %u master NAK; duplicate connection to FNE, remotePeerId = %u", network->m_peerId, rtpHeader.getSSRC());
            network->m_status = NET_STAT_WAITING_CONNECT;
            network->m_remotePeerId = 0U;
            network->m_flaggedDuplicateConn = true;
            network->m_maxRetryCount = MAX_RETRY_DUP_RECONNECT;
            network->m_retryTimer.start();
            return true;

        case NET_CONN_NAK_GENERAL_FAILURE:
        default:
            LogWarning(LOG_NET, "PEER %u master NAK; general failure, remotePeerId = %u", network->m_peerId, rtpHeader.getSSRC());
            break;
        }
    }

    // if the NAK reason is unhandled by the user code, perform default handling
    if (!network->userNakHandler(network->m_peerId, reason, fneHeader, rtpHeader)) {
        if (network->m_status == NET_STAT_RUNNING && (reason == NET_CONN_NAK_FNE_MAX_CONN)) {
            LogWarning(LOG_NET, "PEER %u master NAK; attemping to relogin, remotePeerId = %u", network->m_peerId, rtpHeader.getSSRC());
            network->m_status = NET_STAT_WAITING_LOGIN;
            network->m_timeoutTimer.start();
            network->m_retryTimer.start();
        }
        else {
            if (network->m_enabled) {
                LogError(LOG_NET, "PEER %u master NAK; network reconnect, remotePeerId = %u", network->m_peerId, rtpHeader.getSSRC());
                network->close();
                network->open();
            }
            return true;
        }
    }

    return false;
}
