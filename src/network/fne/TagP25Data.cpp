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
#include "network/FNENetwork.h"
#include "network/fne/TagP25Data.h"
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
/// Initializes a new instance of the TagP25Data class.
/// </summary>
/// <param name="network"></param>
/// <param name="debug"></param>
TagP25Data::TagP25Data(FNENetwork* network, bool debug) :
    m_network(network),
    m_debug(debug)
{
    assert(network != nullptr);
}

/// <summary>
/// Finalizes a instance of the TagP25Data class.
/// </summary>
TagP25Data::~TagP25Data()
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
bool TagP25Data::processFrame(const uint8_t* data, uint32_t len, sockaddr_storage& address)
{
    uint32_t peerId = __GET_UINT32(data, 11U);
    if (peerId > 0 && (m_network->m_peers.find(peerId) != m_network->m_peers.end())) {
        FNEPeerConnection connection = m_network->m_peers[peerId];
        std::string ip = UDPSocket::address(address);

        // validate peer (simple validation really)
        if (connection.connected() && connection.address() == ip) {
            uint8_t lco = data[4U];

            uint32_t srcId = __GET_UINT16(data, 5U);
            uint32_t dstId = __GET_UINT16(data, 8U);

            uint8_t MFId = data[15U];

            uint8_t lsd1 = data[20U];
            uint8_t lsd2 = data[21U];

            uint8_t duid = data[22U];
            uint8_t frameType = p25::P25_FT_DATA_UNIT;

            uint32_t streamId = __GET_UINT32(data, 16U);

            p25::lc::LC control;
            p25::data::LowSpeedData lsd;

            // is this a LDU1, is this the first of a call?
            if (duid == p25::P25_DUID_LDU1) {
                frameType = data[180U];

                if (m_debug) {
                    LogDebug(LOG_NET, "P25, frameType = $%02X", frameType);
                }

                if (frameType == p25::P25_FT_HDU_VALID) {
                    uint8_t algId = data[181U];
                    uint32_t kid = (data[182U] << 8) | (data[183U] << 0);

                    // copy MI data
                    uint8_t mi[p25::P25_MI_LENGTH_BYTES];
                    ::memset(mi, 0x00U, p25::P25_MI_LENGTH_BYTES);

                    for (uint8_t i = 0; i < p25::P25_MI_LENGTH_BYTES; i++) {
                        mi[i] = data[184U + i];
                    }

                    if (m_debug) {
                        LogDebug(LOG_NET, "P25, HDU algId = $%02X, kId = $%02X", algId, kid);
                        Utils::dump(1U, "P25 HDU Network MI", mi, p25::P25_MI_LENGTH_BYTES);
                    }

                    control.setAlgId(algId);
                    control.setKId(kid);
                    control.setMI(mi);
                }
            }

            control.setLCO(lco);
            control.setSrcId(srcId);
            control.setDstId(dstId);
            control.setMFId(MFId);

            lsd.setLSD1(lsd1);
            lsd.setLSD2(lsd2);

            // is the stream valid?
            if (validate(peerId, control, duid, streamId)) {
                // is this peer ignored?
                if (!isPeerPermitted(peerId, control, duid, streamId)) {
                    return false;
                }

                for (auto peer : m_network->m_peers) {
                    if (peerId != peer.first) {
                        // is this peer ignored?
                        if (!isPeerPermitted(peer.first, control, duid, streamId)) {
                            continue;
                        }

                        m_network->writePeer(peer.first, data, len);
                        LogDebug(LOG_NET, "P25, srcPeer = %u, dstPeer = %u, duid = $%02X, lco = $%02X, MFId = $%02X, srcId = %u, dstId = %u, len = %u", 
                            peerId, peer.first, duid, lco, MFId, srcId, dstId, len);
                    }
                }

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
/// <param name="control"></param>
/// <param name="duid"></param>
/// <param name="streamId"></param>
/// <returns></returns>
bool TagP25Data::isPeerPermitted(uint32_t peerId, p25::lc::LC& control, uint8_t duid, uint32_t streamId)
{
    // TODO TODO TODO
    return true;
}

/// <summary>
/// Helper to validate the DMR call stream.
/// </summary>
/// <param name="peerId"></param>
/// <param name="control"></param>
/// <param name="duid"></param>
/// <param name="streamId"></param>
/// <returns></returns>
bool TagP25Data::validate(uint32_t peerId, p25::lc::LC& control, uint8_t duid, uint32_t streamId)
{
    // TODO TODO TODO
    return true;
}