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
#include "Clock.h"
#include "Log.h"
#include "StopWatch.h"
#include "Utils.h"

using namespace system_clock;
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
    m_parrotFrames(),
    m_parrotFramesReady(false),
    m_status(),
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
/// <param name="data">Network data buffer.</param>
/// <param name="len">Length of data.</param>
/// <param name="peerId">Peer ID</param>
/// <param name="pktSeq"></param>
/// <param name="streamId">Stream ID</param>
/// <returns></returns>
bool TagNXDNData::processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint16_t pktSeq, uint32_t streamId)
{
    hrc::hrc_t pktTime = hrc::now();

    uint8_t messageType = data[4U];

    uint32_t srcId = __GET_UINT16(data, 5U);
    uint32_t dstId = __GET_UINT16(data, 8U);

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

        // specifically only check the following logic for end of call, voice or data frames
        if ((messageType == nxdn::RTCH_MESSAGE_TYPE_TX_REL || messageType == nxdn::RTCH_MESSAGE_TYPE_TX_REL_EX) ||
            (messageType == nxdn::RTCH_MESSAGE_TYPE_VCALL || messageType == nxdn::RTCH_MESSAGE_TYPE_DCALL_HDR ||
             messageType == nxdn::RTCH_MESSAGE_TYPE_DCALL_DATA)) {
            // is this the end of the call stream?
            if (messageType == nxdn::RTCH_MESSAGE_TYPE_TX_REL || messageType == nxdn::RTCH_MESSAGE_TYPE_TX_REL_EX) {
                RxStatus status = m_status[dstId];
                uint64_t duration = hrc::diff(pktTime, status.callStartTime);

                if (std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) { return x.second.dstId == dstId; }) != m_status.end()) {
                    m_status.erase(dstId);
                }

                // is this a parrot talkgroup? if so, clear any remaining frames from the buffer
                lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(dstId);
                if (tg.config().parrot()) {
                    if (m_parrotFrames.size() > 0) {
                        m_parrotFramesReady = true;
                        Thread::sleep(m_network->m_parrotDelay);
                        LogMessage(LOG_NET, "NXDN, Parrot Playback will Start, peer = %u, srcId = %u", peerId, srcId);
                    }
                }

                LogMessage(LOG_NET, "NXDN, Call End, peer = %u, srcId = %u, dstId = %u, duration = %u, streamId = %u",
                    peerId, srcId, dstId, duration / 1000, streamId);
            }

            // is this a new call stream?
            if ((messageType != nxdn::RTCH_MESSAGE_TYPE_TX_REL && messageType != nxdn::RTCH_MESSAGE_TYPE_TX_REL_EX)) {
                auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) { return x.second.dstId == dstId; });
                if (it != m_status.end()) {
                    RxStatus status = m_status[dstId];
                    if (streamId != status.streamId) {
                        if (status.srcId != 0U && status.srcId != srcId) {
                            LogWarning(LOG_NET, "NXDN, Call Collision, peer = %u, srcId = %u, dstId = %u, streamId = %u", peerId, srcId, dstId, streamId);
                            return false;
                        }
                    }
                }
                else {
                    // is this a parrot talkgroup? if so, clear any remaining frames from the buffer
                    lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(dstId);
                    if (tg.config().parrot()) {                    
                        m_parrotFramesReady = false;
                        if (m_parrotFrames.size() > 0) {
                            for (auto& pkt : m_parrotFrames) {
                                if (std::get<0>(pkt) != nullptr) {
                                    delete std::get<0>(pkt);
                                }
                            }
                            m_parrotFrames.clear();
                        }
                    }

                    // this is a new call stream
                    RxStatus status = RxStatus();
                    status.callStartTime = pktTime;
                    status.srcId = srcId;
                    status.dstId = dstId;
                    status.streamId = streamId;
                    m_status[dstId] = status;
                    LogMessage(LOG_NET, "NXDN, Call Start, peer = %u, srcId = %u, dstId = %u, streamId = %u", peerId, srcId, dstId, streamId);
                }
            }
        }

        // is this a parrot talkgroup?
        lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(dstId);
        if (tg.config().parrot()) {
            uint8_t *copy = new uint8_t[len];
            ::memcpy(copy, data, len);
            m_parrotFrames.push_back(std::make_tuple(copy, len, pktSeq));
        }

        // repeat traffic to the connected peers
        for (auto peer : m_network->m_peers) {
            if (peerId != peer.first) {
                // is this peer ignored?
                if (!isPeerPermitted(peer.first, lc, messageType, streamId)) {
                    continue;
                }

                m_network->writePeer(peer.first, { NET_FUNC_PROTOCOL, NET_PROTOCOL_SUBFUNC_NXDN }, data, len, pktSeq, true);
                if (m_network->m_verbose) {
                    LogDebug(LOG_NET, "NXDN, srcPeer = %u, dstPeer = %u, messageType = $%02X, srcId = %u, dstId = %u, len = %u, pktSeq = %u, streamId = %u", 
                        peerId, peer.first, messageType, srcId, dstId, len, pktSeq, streamId);
                }
            }
        }

        m_network->m_frameQueue->flushQueue();
        return true;
    }

    return false;
}

/// <summary>
/// Helper to playback a parrot frame to the network.
/// </summary>
void TagNXDNData::playbackParrot()
{
    if (m_parrotFrames.size() == 0) {
        m_parrotFramesReady = false;
        return;
    }

    auto& pkt = m_parrotFrames[0];
    if (std::get<0>(pkt) != nullptr) {
        m_network->writePeers({ NET_FUNC_PROTOCOL, NET_PROTOCOL_SUBFUNC_NXDN }, std::get<0>(pkt), std::get<1>(pkt), std::get<2>(pkt));
        delete std::get<0>(pkt);
        Thread::sleep(60);
    }
    m_parrotFrames.pop_front();
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Helper to determine if the peer is permitted for traffic.
/// </summary>
/// <param name="peerId">Peer ID</param>
/// <param name="lc"></param>
/// <param name="messageType"></param>
/// <param name="streamId">Stream ID</param>
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
/// <param name="peerId">Peer ID</param>
/// <param name="lc"></param>
/// <param name="messageType"></param>
/// <param name="streamId">Stream ID</param>
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