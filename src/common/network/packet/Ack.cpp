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

/* Handles NET_FUNC::ACK packets. */

bool Network::PacketHandler::ack(Network* network, uint32_t peerId, uint32_t streamId, uint64_t now,
    const frame::RTPFNEHeader& fneHeader, const frame::RTPHeader& rtpHeader, const uint8_t* buffer, int length)
{
    (void)peerId;
    (void)streamId;
    (void)now;
    (void)fneHeader;

    switch (network->m_status) {
        case NET_STAT_WAITING_LOGIN:
            LogInfoEx(LOG_NET, "PEER %u RPTL ACK, performing login exchange, remotePeerId = %u", network->m_peerId, rtpHeader.getSSRC());

            ::memcpy(network->m_salt, buffer + 6U, sizeof(uint32_t));
            network->writeAuthorisation();

            network->m_status = NET_STAT_WAITING_AUTHORISATION;
            network->m_timeoutTimer.start();
            network->m_retryTimer.start();
            break;
        case NET_STAT_WAITING_AUTHORISATION:
            LogInfoEx(LOG_NET, "PEER %u RPTK ACK, performing configuration exchange, remotePeerId = %u", network->m_peerId, rtpHeader.getSSRC());

            network->writeConfig();

            network->m_status = NET_STAT_WAITING_CONFIG;
            network->m_timeoutTimer.start();
            network->m_retryTimer.start();
            break;
        case NET_STAT_WAITING_CONFIG:
            LogInfoEx(LOG_NET, "PEER %u RPTC ACK, logged into the master successfully, remotePeerId = %u", network->m_peerId, rtpHeader.getSSRC());
            network->m_loginStreamId = 0U;
            network->m_remotePeerId = rtpHeader.getSSRC();

            network->pktSeq(true);

            // fire off peer connected callback if we have one
            if (network->m_peerConnectedCallback != nullptr) {
                network->m_peerConnectedCallback();
            }

            network->m_status = NET_STAT_RUNNING;
            network->m_timeoutTimer.start();
            network->m_retryTimer.setTimeout(DEFAULT_RETRY_TIME);
            network->m_retryTimer.start();

            if (length > 6) {
                bool useAlternatePortForDiagnostics = (buffer[6U] & 0x80U) == 0x80U;
                if (!useAlternatePortForDiagnostics) {
                    // disable diagnostic and activity logging automatically if the master doesn't utilize the secondary port
                    network->m_allowDiagnosticTransfer = false;
                    network->m_allowActivityTransfer = false;
                    LogError(LOG_NET, "PEER %u RPTC ACK, master does not enable secondary port for metadata, diagnostic and activity logging are disabled, remotePeerId = %u", network->m_peerId, rtpHeader.getSSRC());
                    LogError(LOG_NET, "PEER %u RPTC ACK, **please update your FNE**, secondary port for metadata, is required for all services as of R05A04, remotePeerId = %u", network->m_peerId, rtpHeader.getSSRC());
                }
            }
            break;
        default:
            break;
    }

    return false;
}
