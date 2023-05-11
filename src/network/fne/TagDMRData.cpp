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
#include "network/fne/TagDMRData.h"
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
/// Initializes a new instance of the TagDMRData class.
/// </summary>
/// <param name="network"></param>
/// <param name="debug"></param>
TagDMRData::TagDMRData(FNENetwork* network, bool debug) :
    m_network(network),
    m_debug(debug)
{
    assert(network != nullptr);
}

/// <summary>
/// Finalizes a instance of the TagDMRData class.
/// </summary>
TagDMRData::~TagDMRData()
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
bool TagDMRData::processFrame(const uint8_t* data, uint32_t len, sockaddr_storage& address)
{
    uint32_t peerId = __GET_UINT32(data, 11U);
    if (peerId > 0 && (m_network->m_peers.find(peerId) != m_network->m_peers.end())) {
        FNEPeerConnection connection = m_network->m_peers[peerId];
        std::string ip = UDPSocket::address(address);

        // validate peer (simple validation really)
        if (connection.connected() && connection.address() == ip) {
            uint8_t seqNo = data[4U];

            uint32_t srcId = __GET_UINT16(data, 5U);
            uint32_t dstId = __GET_UINT16(data, 8U);

            uint8_t flco = (data[15U] & 0x40U) == 0x40U ? dmr::FLCO_PRIVATE : dmr::FLCO_GROUP;

            uint32_t slotNo = (data[15U] & 0x80U) == 0x80U ? 2U : 1U;

            dmr::data::Data dmrData;
            dmrData.setSeqNo(seqNo);
            dmrData.setSlotNo(slotNo);
            dmrData.setSrcId(srcId);
            dmrData.setDstId(dstId);
            dmrData.setFLCO(flco);

            bool dataSync = (data[15U] & 0x20U) == 0x20U;
            bool voiceSync = (data[15U] & 0x10U) == 0x10U;

            uint32_t streamId = __GET_UINT32(data, 16U);

            if (dataSync) {
                uint8_t dataType = data[15U] & 0x0FU;
                dmrData.setData(data + 20U);
                dmrData.setDataType(dataType);
                dmrData.setN(0U);
            }
            else if (voiceSync) {
                dmrData.setData(data + 20U);
                dmrData.setDataType(dmr::DT_VOICE_SYNC);
                dmrData.setN(0U);
            }
            else {
                uint8_t n = data[15U] & 0x0FU;
                dmrData.setData(data + 20U);
                dmrData.setDataType(dmr::DT_VOICE);
                dmrData.setN(n);
            }

            // is the stream valid?
            if (validate(peerId, dmrData, streamId)) {
                // is this peer ignored?
                if (!isPeerPermitted(peerId, dmrData, streamId)) {
                    return false;
                }

                for (auto peer : m_network->m_peers) {
                    if (peerId != peer.first) {
                        // is this peer ignored?
                        if (!isPeerPermitted(peer.first, dmrData, streamId)) {
                            continue;
                        }

                        m_network->writePeer(peer.first, data, len);
                        LogDebug(LOG_NET, "DMR, srcPeer = %u, dstPeer = %u, seqNo = %u, srcId = %u, dstId = %u, flco = $%02X, slotNo = %u, len = %u, stream = %u", 
                            peerId, peer.first, seqNo, srcId, dstId, flco, slotNo, len, streamId);
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
/// <param name="data"></param>
/// <param name="streamId"></param>
/// <returns></returns>
bool TagDMRData::isPeerPermitted(uint32_t peerId, dmr::data::Data& data, uint32_t streamId)
{
    // TODO TODO TODO
    return true;
}

/// <summary>
/// Helper to validate the DMR call stream.
/// </summary>
/// <param name="peerId"></param>
/// <param name="data"></param>
/// <param name="streamId"></param>
/// <returns></returns>
bool TagDMRData::validate(uint32_t peerId, dmr::data::Data& data, uint32_t streamId)
{
    // TODO TODO TODO
    return true;
}