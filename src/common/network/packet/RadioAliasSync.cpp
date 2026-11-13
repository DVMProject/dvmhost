// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/Log.h"
#include "network/Network.h"

using namespace network;

#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Handles NET_FUNC::RADIO_ALIAS_SYNC packets. */

bool Network::PacketHandler::radioAliasSync(Network* network, uint32_t peerId, uint32_t streamId, uint64_t now,
    const frame::RTPFNEHeader& fneHeader, const frame::RTPHeader& rtpHeader, const uint8_t* buffer, int length)
{
    (void)peerId;
    (void)now;
    (void)rtpHeader;

    if (fneHeader.getSubFunction() != NET_SUBFUNC::NOP)
        return false;

    if (network->m_radioAliasSyncStreamId == 0U || streamId != network->m_radioAliasSyncStreamId) {
        LogWarning(LOG_NET, "PEER %u Radio Alias Sync, ignoring unexpected stream %u", network->m_peerId, streamId);
        return true;
    }

    if (length < static_cast<int>(FRAG_SIZE)) {
        LogWarning(LOG_NET, "PEER %u Radio Alias Sync, ignoring short fragment, length = %d", network->m_peerId, length);
        network->m_radioAliasSyncPkt.clear();
        network->m_radioAliasSyncStreamId = 0U;
        return true;
    }

    uint32_t decodedLength = 0U;
    uint8_t* decoded = nullptr;
    if (!network->m_radioAliasSyncPkt.decode(buffer, &decoded, &decodedLength))
        return true;

    if (!network->m_enabled || !network->m_updateLookup || network->m_ridAliasLookup == nullptr || decodedLength == 0U) {
        LogWarning(LOG_NET, "PEER %u Radio Alias Sync, alias lookup update is unavailable", network->m_peerId);
        network->m_radioAliasSyncPkt.clear();
        network->m_radioAliasSyncStreamId = 0U;
        delete[] decoded;
        return true;
    }

    std::ostringstream name;
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<uint32_t> dist(0x00U, 0xFFFFFFFFU);
    name << "/tmp/rid_alias.dat." << dist(mt);

    const std::string tempFilename = name.str();
    std::ofstream file(tempFilename, std::ios::out | std::ios::binary);
    if (file.fail()) {
        LogError(LOG_NET, "PEER %u Radio Alias Sync, cannot open temporary lookup file - %s",
            network->m_peerId, tempFilename.c_str());
        network->m_radioAliasSyncPkt.clear();
        network->m_radioAliasSyncStreamId = 0U;
        delete[] decoded;
        return true;
    }

    file.write(reinterpret_cast<const char*>(decoded), decodedLength);
    const bool writeFailed = file.fail();
    file.close();
    delete[] decoded;

    if (writeFailed) {
        LogError(LOG_NET, "PEER %u Radio Alias Sync, failed writing temporary lookup file - %s",
            network->m_peerId, tempFilename.c_str());
        ::remove(tempFilename.c_str());
        network->m_radioAliasSyncPkt.clear();
        network->m_radioAliasSyncStreamId = 0U;
        return true;
    }

    const std::string lookupFilename = network->m_ridAliasLookup->filename();
    network->m_ridAliasLookup->filename(tempFilename);
    const bool loaded = network->m_ridAliasLookup->reload();
    network->m_ridAliasLookup->filename(lookupFilename);

    if (loaded && network->m_saveLookup) {
        network->m_ridAliasLookup->commit(true);
    }

    ::remove(tempFilename.c_str());

    if (!loaded) {
        LogError(LOG_NET, "PEER %u Radio Alias Sync, received lookup table could not be loaded", network->m_peerId);
    } else {
        LogInfoEx(LOG_NET, "PEER %u Radio Alias Sync, lookup table updated", network->m_peerId);
    }

    network->m_radioAliasSyncPkt.clear();
    network->m_radioAliasSyncStreamId = 0U;
    return true;
}
