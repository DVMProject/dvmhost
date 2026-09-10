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

/* Handles NET_FUNC::MASTER packets. */

bool Network::PacketHandler::master(Network* network, uint32_t peerId, uint32_t streamId, uint64_t now,
    const frame::RTPFNEHeader& fneHeader, const frame::RTPHeader& rtpHeader, const uint8_t* buffer, int length)
{
    (void)peerId;
    (void)streamId;
    (void)now;
    (void)fneHeader;
    (void)rtpHeader;

    // process incoming message subfunction opcodes
    switch (fneHeader.getSubFunction()) {
    case NET_SUBFUNC::MASTER_SUBFUNC_WL_RID:                // Radio ID Whitelist
        {
            if (network->m_enabled && network->m_updateLookup) {
                if (network->m_packetDump)
                    Utils::dump(1U, "Network::clock(), Network Rx, WL RID", buffer, length);

                if (network->m_ridLookup != nullptr) {
                    // update RID lists
                    uint32_t len = GET_UINT32(buffer, 6U);
                    uint32_t offs = 11U;
                    for (uint32_t i = 0; i < len; i++) {
                        uint32_t id = GET_UINT24(buffer, offs);
                        network->m_ridLookup->toggleEntry(id, true);
                        offs += 4U;
                    }

                    LogInfoEx(LOG_NET, "Network Announced %u whitelisted RIDs", len);

                    // save to file if enabled and we got RIDs
                    if (network->m_saveLookup && len > 0) {
                        network->m_ridLookup->commit();
                    }
                }
            }
        }
        break;
    case NET_SUBFUNC::MASTER_SUBFUNC_BL_RID:                // Radio ID Blacklist
        {
            if (network->m_enabled && network->m_updateLookup) {
                if (network->m_packetDump)
                    Utils::dump(1U, "Network::clock(), Network Rx, BL RID", buffer, length);

                if (network->m_ridLookup != nullptr) {
                    // update RID lists
                    uint32_t len = GET_UINT32(buffer, 6U);
                    uint32_t offs = 11U;
                    for (uint32_t i = 0; i < len; i++) {
                        uint32_t id = GET_UINT24(buffer, offs);
                        network->m_ridLookup->toggleEntry(id, false);
                        offs += 4U;
                    }

                    LogInfoEx(LOG_NET, "Network Announced %u blacklisted RIDs", len);

                    // save to file if enabled and we got RIDs
                    if (network->m_saveLookup && len > 0) {
                        network->m_ridLookup->commit();
                    }
                }
            }
        }
        break;

    case NET_SUBFUNC::MASTER_SUBFUNC_ACTIVE_TGS:            // Talkgroup Active IDs
        {
            if (network->m_enabled && network->m_updateLookup) {
                if (network->m_packetDump)
                    Utils::dump(1U, "Network::clock(), Network Rx, ACTIVE TGS", buffer, length);

                if (network->m_tidLookup != nullptr) {
                    // update TGID lists
                    uint32_t len = GET_UINT32(buffer, 6U);
                    uint32_t offs = 11U;
                    for (uint32_t i = 0; i < len; i++) {
                        uint32_t id = GET_UINT24(buffer, offs);
                        uint8_t slot = (buffer[offs + 3U]) & 0x03U;
                        bool affiliated = (buffer[offs + 3U] & 0x40U) == 0x40U;
                        bool nonPreferred = (buffer[offs + 3U] & 0x80U) == 0x80U;

                        // encryption strapping bits
                        bool strappedBit = (buffer[offs + 3U] & 0x20U) == 0x20U;
                        bool clearBit = (buffer[offs + 3U] & 0x10U) == 0x10U;
                        uint8_t strapping = lookups::TG_STRAPPING_SELECTABLE;

                        if (strappedBit) {
                            strapping = lookups::TG_STRAPPING_STRAPPED;
                        }

                        if (clearBit) {
                            strapping = lookups::TG_STRAPPING_CLEAR;
                        }

                        lookups::TalkgroupRuleGroupVoice tid = network->m_tidLookup->find(id, slot);

                        // if the TG is marked as non-preferred, and the TGID exists in the local entries
                        // erase the local and overwrite with the FNE data
                        if (nonPreferred) {
                            if (!tid.isInvalid()) {
                                network->m_tidLookup->eraseEntry(id, slot);
                                tid = network->m_tidLookup->find(id, slot);
                            }
                        }

                        if (tid.isInvalid()) {
                            if (!tid.config().active()) {
                                network->m_tidLookup->eraseEntry(id, slot);
                            }

                            LogInfoEx(LOG_NET, "Activated%s%s TG %u TS %u %s in TGID table",
                                (nonPreferred) ? " non-preferred" : "", (affiliated) ? " affiliated" : "", id, slot,
                                (strappedBit) ? "strapped" : (clearBit) ? "clear" : "selectable");
                            network->m_tidLookup->addEntry(id, slot, true, affiliated, nonPreferred, strapping);
                        }

                        offs += 5U;
                    }

                    LogInfoEx(LOG_NET, "Activated %u TGs; loaded %u entries into talkgroup rules table", len, network->m_tidLookup->groupVoice().size());

                    // save if saving from network is enabled
                    if (network->m_saveLookup && len > 0) {
                        network->m_tidLookup->commit();
                    }
                }
            }
        }
        break;
    case NET_SUBFUNC::MASTER_SUBFUNC_DEACTIVE_TGS:          // Talkgroup Deactivated IDs
        {
            if (network->m_enabled && network->m_updateLookup) {
                if (network->m_packetDump)
                    Utils::dump(1U, "Network::clock(), Network Rx, DEACTIVE TGS", buffer, length);

                if (network->m_tidLookup != nullptr) {
                    // update TGID lists
                    uint32_t len = GET_UINT32(buffer, 6U);
                    uint32_t offs = 11U;
                    for (uint32_t i = 0; i < len; i++) {
                        uint32_t id = GET_UINT24(buffer, offs);
                        uint8_t slot = (buffer[offs + 3U]);

                        lookups::TalkgroupRuleGroupVoice tid = network->m_tidLookup->find(id, slot);
                        if (!tid.isInvalid()) {
                            LogInfoEx(LOG_NET, "Deactivated TG %u TS %u in TGID table", id, slot);
                            network->m_tidLookup->eraseEntry(id, slot);
                        }

                        offs += 5U;
                    }

                    LogInfoEx(LOG_NET, "Deactivated %u TGs; loaded %u entries into talkgroup rules table", len, network->m_tidLookup->groupVoice().size());

                    // save if saving from network is enabled
                    if (network->m_saveLookup && len > 0) {
                        network->m_tidLookup->commit();
                    }
                }
            }
        }
        break;

    case NET_SUBFUNC::MASTER_HA_PARAMS:                     // HA Parameters
        {
            if (network->m_enabled) {
                if (network->m_packetDump)
                    Utils::dump(1U, "Network::clock(), Network Rx, HA PARAMS", buffer, length);

                network->m_haIPs.clear();
                network->m_currentHAIP = 0U;
                network->m_maxRetryCount = MAX_RETRY_HA_RECONNECT;

                // always add the configured address to the HA IP list
                network->m_haIPs.push_back(PeerHAIPEntry(network->m_configuredAddress, network->m_configuredPort));
                if (network->m_debug)
                    LogDebugEx(LOG_NET, "Network::clock()", "HA PARAMS, 1, %s:%u", network->m_configuredAddress.c_str(), network->m_configuredPort);

                uint32_t len = GET_UINT32(buffer, 6U);
                if (len > 0U) {
                    len /= HA_PARAMS_ENTRY_LEN;
                }

                uint32_t offs = 10U;
                for (uint32_t i = 0U; i < len; i++, offs += HA_PARAMS_ENTRY_LEN) {
                    uint32_t ipAddr = GET_UINT32(buffer, offs + 4U);
                    uint16_t port = GET_UINT16(buffer, offs + 8U);

                    std::string address = __IP_FROM_UINT(ipAddr);

                    if (address == network->m_configuredAddress && port == network->m_configuredPort) {
                        continue; // skip if this is the same as our configured address
                    }

                    if (network->m_debug)
                        LogDebugEx(LOG_NET, "Network::clock()", "HA PARAMS, %u, %s:%u", i + 2, address.c_str(), port);

                    network->m_haIPs.push_back(PeerHAIPEntry(address, port));
                }

                if (network->m_haIPs.size() > 1U) {
                    network->m_currentHAIP = 1U; // because the first entry is our configured entry, set
                                        // the current HA IP to the next available
                    LogInfoEx(LOG_NET, "Loaded %u HA IPs from master", network->m_haIPs.size());
                    LogInfoEx(LOG_NET, "Current HA IP %s:%u", network->m_haIPs[network->m_currentHAIP].masterAddress.c_str(), network->m_haIPs[network->m_currentHAIP].masterPort);
                }
            }
        }
        break;

    default:
        Utils::dump("unknown master control opcode from the master", buffer, length);
        break;
    }

    return false;
}
