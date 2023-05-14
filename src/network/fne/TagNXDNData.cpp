/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2023 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include "Defines.h"
#include "nxdn/NXDNDefines.h"
#include "network/FNENetwork.h"
#include "network/fne/TagNXDNData.h"
#include "Log.h"
#include "StopWatch.h"
#include "Utils.h"

using namespace network;
using namespace network::fne;

#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <chrono>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the TagNXDNData class.
/// </summary>
/// <param name="network"></param>
/// <param name="debug"></param>
TagNXDNData::TagNXDNData(FNENetwork* network, bool debug) :
    m_network(network),
    m_debug(debug)
{
    assert(network != nullptr);
}

/// <summary>
/// Finalizes a instance of the TagNXDNData class.
/// </summary>
TagNXDNData::~TagNXDNData()
{
    /* stub */
}

/// <summary>
/// Process a data frame from the network.
/// </summary>
/// <param name="data"></param>
/// <param name="len"></param>
/// <param name="address"></param>
/// <returns></returns>
bool TagNXDNData::processFrame(const uint8_t* data, uint32_t len, sockaddr_storage& address)
{
    uint32_t peerId = __GET_UINT32(data, 11U);
    if (peerId > 0 && (m_network->m_peers.find(peerId) != m_network->m_peers.end())) {
        FNEPeerConnection connection = m_network->m_peers[peerId];
        std::string ip = UDPSocket::address(address);

        // validate peer (simple validation really)
        if (connection.connected() && connection.address() == ip) {
            uint8_t messageType = data[4U];

            uint32_t srcId = __GET_UINT16(data, 5U);
            uint32_t dstId = __GET_UINT16(data, 8U);

            uint32_t streamId = __GET_UINT32(data, 16U);

            nxdn::lc::RTCH lc;

            lc.setMessageType(messageType);
            lc.setSrcId((uint16_t)srcId & 0xFFFFU);
            lc.setDstId((uint16_t)dstId & 0xFFFFU);

            bool group = (data[15U] & 0x40U) == 0x40U ? false : true;
            lc.setGroup(group);

            // is the stream valid?
            if (validate(peerId, lc, messageType, streamId)) {
                // is this peer ignored?
                if (!isPeerPermitted(peerId, lc, messageType, streamId)) {
                    return false;
                }

                // TODO TODO TODO
                // TODO: handle checking if this is a parrot group and properly implement parrot

                for (auto peer : m_network->m_peers) {
                    if (peerId != peer.first) {
                        // is this peer ignored?
                        if (!isPeerPermitted(peer.first, lc, messageType, streamId)) {
                            continue;
                        }

                        m_network->writePeer(peer.first, data, len, true);
                        LogDebug(LOG_NET, "NXDN, srcPeer = %u, dstPeer = %u, messageType = $%02X, srcId = %u, dstId = %u, len = %u", 
                            peerId, peer.first, messageType, srcId, dstId, len);
                    }
                }

                m_network->m_frameQueue->flushQueue();
                return true;
            }
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Helper to determine if the peer is permitted for traffic.
/// </summary>
/// <param name="peerId"></param>
/// <param name="lc"></param>
/// <param name="messageType"></param>
/// <param name="streamId"></param>
/// <returns></returns>
bool TagNXDNData::isPeerPermitted(uint32_t peerId, nxdn::lc::RTCH& lc, uint8_t messageType, uint32_t streamId)
{
    // private calls are always permitted
    if (!lc.getGroup()) {
        return true;
    }

    // is this a group call?
    if (lc.getGroup()) {
        lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(lc.getDstId());

        std::vector<uint32_t> inclusion = tg.config().inclusion();
        std::vector<uint32_t> exclusion = tg.config().exclusion();

        // peer inclusion lists take priority over exclusion lists
        if (inclusion.size() > 0) {
            auto it = std::find(inclusion.begin(), inclusion.end(), peerId);
            if (it == inclusion.end()) {
                return false;
            }
        }
        else {
            if (exclusion.size() > 0) {
                auto it = std::find(exclusion.begin(), exclusion.end(), peerId);
                if (it != inclusion.end()) {
                    return false;
                }
            }
        }

        // TODO TODO TODO
        // TODO: handle checking group affiliations if affiliation option is enabled
    }

    return true;
}

/// <summary>
/// Helper to validate the DMR call stream.
/// </summary>
/// <param name="peerId"></param>
/// <param name="lc"></param>
/// <param name="messageType"></param>
/// <param name="streamId"></param>
/// <returns></returns>
bool TagNXDNData::validate(uint32_t peerId, nxdn::lc::RTCH& lc, uint8_t messageType, uint32_t streamId)
{
    // is the source ID a blacklisted ID?
    lookups::RadioId rid = m_network->m_ridLookup->find(lc.getSrcId());
    if (!rid.radioDefault()) {
        if (!rid.radioEnabled()) {
            return false;
        }
    }

    // always validate a terminator if the source is valid
    if (messageType == nxdn::RTCH_MESSAGE_TYPE_TX_REL || messageType == nxdn::RTCH_MESSAGE_TYPE_TX_REL_EX)
        return true;

    // is this a private call?
    if (!lc.getGroup()) {
        // is the destination ID a blacklisted ID?
        lookups::RadioId rid = m_network->m_ridLookup->find(lc.getDstId());
        if (!rid.radioDefault()) {
            if (!rid.radioEnabled()) {
                return false;
            }
        }

        return true;
    }

    lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(lc.getDstId());
    if (!tg.config().active()) {
        return false;
    }

    // TODO TODO TODO
    // TODO: handle checking group affiliations if affiliation option is enabled

    return true;
}