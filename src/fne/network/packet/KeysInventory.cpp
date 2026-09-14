// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023-2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/Defines.h"
#include "common/edac/SHA256.h"
#include "common/Log.h"
#include "network/MetadataNetwork.h"
#include "fne/ActivityLog.h"
#include "HostFNE.h"

#include <fstream>

using namespace network;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Handles NET_FUNC::KEYS_INVENTORY packets. */

void MetadataNetwork::PacketHandler::keysInventory(TrafficNetwork* network, MetadataNetwork* mdNetwork, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId)
{
    (void)ssrc;

    if (!network->m_host->m_cryptoLookup->isRemoteAccessEnabled()) {
        LogError(LOG_MASTER, "PEER %u requested enc. key inventory, but remote access is disabled, no response", peerId);
        return;
    }

    lookups::PeerId peerEntry = network->m_peerListLookup->find(peerId);
    if (peerEntry.peerDefault()) {
        LogError(LOG_MASTER, "PEER %u requested enc. key inventory but is not allowed, no response", peerId);
        return;
    } else {
        if (!peerEntry.canRequestKeys()) {
            LogError(LOG_MASTER, "PEER %u requested enc. key inventory but is not allowed, no response", peerId);
            return;
        }
    }

    // keys inventory operates differently from the rest of the network opcodes...and does not require
    // an established connection to the master, so we will not validate the peer connection state here
    if (peerId > 0 && !peerEntry.peerDefault()) {
        if (req->length < 80) {
            LogError(LOG_MASTER, "PEER %u requested enc. key inventory, but payload length was invalid (%u bytes), no response", peerId, req->length);
            return;
        }

        // scope intentional
        {
            // get the peer password hash from the frame message
            DECLARE_UINT8_ARRAY(peerHash, 32U);
            ::memcpy(peerHash, req->buffer + 8U, 32U);

            uint8_t peerSalt[4U];
            ::memset(peerSalt, 0x00U, 4U);
            ::memcpy(peerSalt, req->buffer + 40U, 4U);

            std::string passwordForPeer = network->m_password;

            // check if the peer is in the peer ACL list
            bool validAcl = true;
            if (network->m_peerListLookup->getACL()) {
                if (!network->m_peerListLookup->isPeerAllowed(peerId) && !network->m_peerListLookup->isPeerListEmpty()) {
                    LogWarning(LOG_MASTER, "PEER %u RPTK, failed peer ACL check", peerId);
                    validAcl = false;
                } else {
                    lookups::PeerId peerEntry = network->m_peerListLookup->find(peerId);
                    if (peerEntry.peerDefault()) {
                        validAcl = false; // default peer IDs are a no-no as they have no data thus fail ACL check
                    } else {
                        passwordForPeer = peerEntry.peerPassword();
                        if (passwordForPeer.length() == 0) {
                            passwordForPeer = network->m_password;
                        }
                    }
                }

                if (network->m_peerListLookup->isPeerListEmpty()) {
                    LogWarning(LOG_MASTER, "Peer List ACL enabled, but we have an empty peer list? Passing all peers.");
                    validAcl = true;
                }
            }

            if (validAcl) {
                size_t size = passwordForPeer.size();
                uint8_t* in = new uint8_t[size + sizeof(uint32_t)];
                ::memcpy(in, peerSalt, sizeof(uint32_t));
                for (size_t i = 0U; i < size; i++)
                    in[i + sizeof(uint32_t)] = passwordForPeer.at(i);

                uint8_t out[32U];
                edac::SHA256 sha256;
                sha256.buffer(in, (uint32_t)(size + sizeof(uint32_t)), out);

                delete[] in;

                // validate hash
                bool validHash = false;
                if (req->length >= 80) {
                    validHash = true;
                    for (uint8_t i = 0; i < 32U; i++) {
                        if (peerHash[i] != out[i]) {
                            validHash = false;
                            break;
                        }
                    }
                }

                if (!validHash) {
                    LogError(LOG_MASTER, "PEER %u requested enc. key inventory, but had invalid authentication, no response", peerId);
                    return;
                }
            } else {
                LogError(LOG_MASTER, "PEER %u requested enc. key inventory, but had invalid ACL, no response", peerId);
                return;
            }
        }

        // scope intentional
        {
            // get remote access password hash from the frame message
            DECLARE_UINT8_ARRAY(remoteAccessHash, 32U);
            ::memcpy(remoteAccessHash, req->buffer + 44U, 32U);

            uint8_t remoteSalt[4U];
            ::memset(remoteSalt, 0x00U, 4U);
            ::memcpy(remoteSalt, req->buffer + 76U, 4U);

            std::string remoteAccessPassword = network->m_host->m_cryptoLookup->getRemotePassword();

            size_t size = remoteAccessPassword.size();
            uint8_t* in = new uint8_t[size + sizeof(uint32_t)];
            ::memcpy(in, remoteSalt, sizeof(uint32_t));
            for (size_t i = 0U; i < size; i++)
                in[i + sizeof(uint32_t)] = remoteAccessPassword.at(i);

            uint8_t out[32U];
            edac::SHA256 sha256;
            sha256.buffer(in, (uint32_t)(size + sizeof(uint32_t)), out);

            delete[] in;

            // validate hash
            bool validHash = false;
            if (req->length >= 80) {
                validHash = true;
                for (uint8_t i = 0; i < 32U; i++) {
                    if (remoteAccessHash[i] != out[i]) {
                        validHash = false;
                        break;
                    }
                }
            }

            if (!validHash) {
                LogError(LOG_MASTER, "PEER %u requested enc. key inventory, but had invalid access authentication, no response", peerId);
                return;
            }
        }

        // scope intentional
        {
            // read entire file into buffer
            std::stringstream b;
            std::ifstream stream(network->m_host->m_cryptoLookup->filename(), std::ios::in | std::ios::binary);

            uint32_t len = 0U;
            UInt8Array bufferUInt8Array = nullptr;
            uint8_t* buffer = nullptr;

            if (stream.is_open()) {
                stream.seekg(0, std::ios::end);
                len = (uint32_t)stream.tellg();
                stream.seekg(0, std::ios::beg);

                bufferUInt8Array = std::make_unique<uint8_t[]>(len);
                buffer = bufferUInt8Array.get();
                ::memset(buffer, 0x00U, len);

                uint32_t i = 0U;
                while (stream.peek() != EOF) {
                    buffer[i] = (uint8_t)stream.get();
                    i++;
                }

                stream.close();
            }

            PacketBuffer pkt(true, "Remote EKC, Key Inventory");
            bool success = pkt.encode((uint8_t*)buffer, len);
            if (!success) {
                LogError(LOG_REPL, "PEER %u Remote EKC, Key Inventory, failed to encode packet", peerId);
                return;
            }

            LogInfoEx(LOG_REPL, "PEER %u Remote EKC, Key Inventory, blocks %u, streamId = %u", peerId, pkt.fragments.size(), streamId);
            if (pkt.fragments.size() > 0U) {
                for (auto frag : pkt.fragments) {
                    // violate most handling rules for responding to packets -- we need to directly respond to the calling peer as
                    // they may not be logged in as a standard peer
                    mdNetwork->m_frameQueue->write(frag.second->data, FRAG_SIZE, streamId, peerId, network->m_peerId, { NET_FUNC::KEYS_INVENTORY, NET_SUBFUNC::NOP },
                        0U, req->address, req->addrLen);
                    Thread::sleep(60U); // pace block transmission
                }
            }

            pkt.clear();
        }
    }
}
