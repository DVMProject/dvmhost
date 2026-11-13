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

/* Handles NET_FUNC::INCALL_CTRL packets. */

bool Network::PacketHandler::inCallControl(Network* network, uint32_t peerId, uint32_t streamId, uint64_t now,
    const frame::RTPFNEHeader& fneHeader, const frame::RTPHeader& rtpHeader, const uint8_t* buffer, int length)
{
    (void)now;
    (void)length;

    uint32_t ssrc = rtpHeader.getSSRC();
    if (!network->m_promiscuousPeer && ssrc != peerId) {
        LogWarning(LOG_NET, "PEER %u, ignoring in-call control not destined for this peer SSRC %u", network->m_peerId, ssrc);
        return false;
    }

    // process incoming message subfunction opcodes
    switch (fneHeader.getSubFunction()) {
    case NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR:                 // DMR In-Call Control
        {
            if (network->m_enabled && network->m_dmrEnabled) {
                NET_ICC::ENUM command = (NET_ICC::ENUM)buffer[10U];
                uint32_t dstId = GET_UINT24(buffer, 11U);
                uint8_t slot = buffer[14U];

                // fire off DMR in-call callback if we have one
                if (network->m_dmrInCallCallback != nullptr) {
                    network->m_dmrInCallCallback(command, dstId, slot, peerId, ssrc, streamId);
                }
            }
        }
        break;
    case NET_SUBFUNC::PROTOCOL_SUBFUNC_P25:                 // P25 In-Call Control
        {
            if (network->m_enabled && network->m_p25Enabled) {
                NET_ICC::ENUM command = (NET_ICC::ENUM)buffer[10U];
                uint32_t dstId = GET_UINT24(buffer, 11U);

                // fire off P25 in-call callback if we have one
                if (network->m_p25InCallCallback != nullptr) {
                    network->m_p25InCallCallback(command, dstId, peerId, ssrc, streamId);
                }
            }
        }
        break;
    case NET_SUBFUNC::PROTOCOL_SUBFUNC_P25_P2:              // P25 Phase 2 In-Call Control
        {
            if (network->m_enabled && network->m_p25Enabled) {
                NET_ICC::ENUM command = (NET_ICC::ENUM)buffer[10U];
                uint32_t dstId = GET_UINT24(buffer, 11U);
                uint8_t slot = buffer[14U];

                // fire off P25 Phase 2 in-call callback if we have one
                if (network->m_p25P2InCallCallback != nullptr) {
                    network->m_p25P2InCallCallback(command, dstId, slot, peerId, ssrc, streamId);
                }
            }
        }
        break;
    case NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN:                // NXDN In-Call Control
        {
            if (network->m_enabled && network->m_nxdnEnabled) {
                NET_ICC::ENUM command = (NET_ICC::ENUM)buffer[10U];
                uint32_t dstId = GET_UINT24(buffer, 11U);

                // fire off NXDN in-call callback if we have one
                if (network->m_nxdnInCallCallback != nullptr) {
                    network->m_nxdnInCallCallback(command, dstId, peerId, ssrc, streamId);
                }
            }
        }
        break;
    case NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG:              // Analog In-Call Control
        {
            if (network->m_enabled && network->m_analogEnabled) {
                NET_ICC::ENUM command = (NET_ICC::ENUM)buffer[10U];
                uint32_t dstId = GET_UINT24(buffer, 11U);

                // fire off analog in-call callback if we have one
                if (network->m_analogInCallCallback != nullptr) {
                    network->m_analogInCallCallback(command, dstId, peerId, ssrc, streamId);
                }
            }
        }
        break;

    default:
        Utils::dump("unknown incall control opcode from the master", buffer, length);
        break;
    }

    return false;
}
