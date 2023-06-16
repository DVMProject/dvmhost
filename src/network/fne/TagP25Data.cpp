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
#include "Clock.h"
#include "Log.h"
#include "StopWatch.h"
#include "Thread.h"
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
/// Initializes a new instance of the TagP25Data class.
/// </summary>
/// <param name="network"></param>
/// <param name="debug"></param>
TagP25Data::TagP25Data(FNENetwork* network, bool debug) :
    m_network(network),
    m_parrotFrames(),
    m_parrotFramesReady(false),
    m_status(),
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
/// <param name="data">Network data buffer.</param>
/// <param name="len">Length of data.</param>
/// <param name="peerId">Peer ID</param>
/// <param name="pktSeq"></param>
/// <param name="streamId">Stream ID</param>
/// <returns></returns>
bool TagP25Data::processFrame(const uint8_t* data, uint32_t len, uint32_t peerId, uint16_t pktSeq, uint32_t streamId)
{
    hrc::hrc_t pktTime = hrc::now();

    uint8_t lco = data[4U];

    uint32_t srcId = __GET_UINT16(data, 5U);
    uint32_t dstId = __GET_UINT16(data, 8U);

    uint8_t MFId = data[15U];

    uint8_t lsd1 = data[20U];
    uint8_t lsd2 = data[21U];

    uint8_t duid = data[22U];
    uint8_t frameType = p25::P25_FT_DATA_UNIT;

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

        // specifically only check the following logic for end of call or voice frames
        if (duid != p25::P25_DUID_TSDU && duid != p25::P25_DUID_PDU) {
            // is this the end of the call stream?
            if ((duid == p25::P25_DUID_TDU) || (duid == p25::P25_DUID_TDULC)) {
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
                        LogMessage(LOG_NET, "P25, Parrot Playback will Start, peer = %u, srcId = %u", peerId, srcId);
                    }
                }

                LogMessage(LOG_NET, "P25, Call End, peer = %u, srcId = %u, dstId = %u, duration = %u, streamId = %u",
                    peerId, srcId, dstId, duration / 1000, streamId);
            }

            // is this a new call stream?
            if ((duid != p25::P25_DUID_TDU) && (duid != p25::P25_DUID_TDULC)) {
                auto it = std::find_if(m_status.begin(), m_status.end(), [&](StatusMapPair x) { return x.second.dstId == dstId; });
                if (it != m_status.end()) {
                    RxStatus status = m_status[dstId];
                    if (streamId != status.streamId && ((duid != p25::P25_DUID_TDU) && (duid != p25::P25_DUID_TDULC))) {
                        if (status.srcId != 0U && status.srcId != srcId) {
                            LogWarning(LOG_NET, "P25, Call Collision, peer = %u, srcId = %u, dstId = %u, streamId = %u", peerId, srcId, dstId, streamId);
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
                    LogMessage(LOG_NET, "P25, Call Start, peer = %u, srcId = %u, dstId = %u, streamId = %u", peerId, srcId, dstId, streamId);
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
                if (!isPeerPermitted(peer.first, control, duid, streamId)) {
                    continue;
                }

                m_network->writePeer(peer.first, { NET_FUNC_PROTOCOL, NET_PROTOCOL_SUBFUNC_P25 }, data, len, pktSeq, true);
                if (m_network->m_verbose) {
                    LogDebug(LOG_NET, "P25, srcPeer = %u, dstPeer = %u, duid = $%02X, lco = $%02X, MFId = $%02X, srcId = %u, dstId = %u, len = %u, pktSeq = %u, streamId = %u", 
                        peerId, peer.first, duid, lco, MFId, srcId, dstId, len, pktSeq, streamId);
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
void TagP25Data::playbackParrot()
{
    if (m_parrotFrames.size() == 0) {
        m_parrotFramesReady = false;
        return;
    }

    auto& pkt = m_parrotFrames[0];
    if (std::get<0>(pkt) != nullptr) {
        m_network->writePeers({ NET_FUNC_PROTOCOL, NET_PROTOCOL_SUBFUNC_P25 }, std::get<0>(pkt), std::get<1>(pkt), std::get<2>(pkt));
        delete std::get<0>(pkt);
        Thread::sleep(120);
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
/// <param name="control"></param>
/// <param name="duid"></param>
/// <param name="streamId">Stream ID</param>
/// <returns></returns>
bool TagP25Data::isPeerPermitted(uint32_t peerId, p25::lc::LC& control, uint8_t duid, uint32_t streamId)
{
    // private calls are always permitted
    if (control.getLCO() == p25::LC_PRIVATE) {
        return true;
    }

    // always permit a TSDU or PDU
    if (duid == p25::P25_DUID_TSDU || duid == p25::P25_DUID_PDU)
        return true;

    // always permit a terminator
    if (duid == p25::P25_DUID_TDU || duid == p25::P25_DUID_TDULC)
        return true;

    // is this a group call?
    lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(control.getDstId());

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

    return true;
}

/// <summary>
/// Helper to validate the DMR call stream.
/// </summary>
/// <param name="peerId">Peer ID</param>
/// <param name="control"></param>
/// <param name="duid"></param>
/// <param name="streamId">Stream ID</param>
/// <returns></returns>
bool TagP25Data::validate(uint32_t peerId, p25::lc::LC& control, uint8_t duid, uint32_t streamId)
{
    // is the source ID a blacklisted ID?
    lookups::RadioId rid = m_network->m_ridLookup->find(control.getSrcId());
    if (!rid.radioDefault()) {
        if (!rid.radioEnabled()) {
            return false;
        }
    }

    // always validate a TSDU or PDU if the source is valid
    if (duid == p25::P25_DUID_TSDU || duid == p25::P25_DUID_PDU)
        return true;

    // always validate a terminator if the source is valid
    if (duid == p25::P25_DUID_TDU || duid == p25::P25_DUID_TDULC)
        return true;

    // is this a private call?
    if (control.getLCO() == p25::LC_PRIVATE) {
        // is the destination ID a blacklisted ID?
        lookups::RadioId rid = m_network->m_ridLookup->find(control.getDstId());
        if (!rid.radioDefault()) {
            if (!rid.radioEnabled()) {
                return false;
            }
        }

        return true;
    }

    lookups::TalkgroupRuleGroupVoice tg = m_network->m_tidLookup->find(control.getDstId());
    if (!tg.config().active()) {
        return false;
    }

    // TODO TODO TODO
    // TODO: handle checking group affiliations if affiliation option is enabled

    return true;
}