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

#include <cstdio>
#include <fstream>
#include <mutex>

using namespace network;

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define KEY_UPDATE_STATUS_BUSY 0x01U

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::mutex s_keyUpdateCommitMutex;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Handles NET_FUNC::KEYS_UPDATE packets. */

void MetadataNetwork::PacketHandler::keysUpdate(TrafficNetwork* network, MetadataNetwork* mdNetwork, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId)
{
    (void)ssrc;

    if (!network->m_host->m_cryptoLookup->isRemoteAccessEnabled()) {
        LogError(LOG_MASTER, "PEER %u requested enc. key update, but remote access is disabled, no response", peerId);
        return;
    }

    lookups::PeerId peerEntry = network->m_peerListLookup->find(peerId);
    if (peerEntry.peerDefault()) {
        LogError(LOG_MASTER, "PEER %u requested enc. key update but is not allowed, no response", peerId);
        return;
    } else {
        if (!peerEntry.canRequestKeys()) {
            LogError(LOG_MASTER, "PEER %u requested enc. key update but is not allowed, no response", peerId);
            return;
        }
    }

    // keys update operates differently from the rest of the network opcodes...and does not require
    // an established connection to the master, so we will not validate the peer connection state here.
    // update is a two-phase flow: (1) auth request frame (80 bytes), then (2) chunked PacketBuffer frames.
    if (peerId > 0 && !peerEntry.peerDefault()) {
        if (mdNetwork->m_peerKeyUpdatePkt.find(peerId) == mdNetwork->m_peerKeyUpdatePkt.end()) {
            if (req->length < 80) {
                LogError(LOG_MASTER, "PEER %u requested enc. key update, but payload length was invalid (%u bytes), no response", peerId, req->length);
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
                        LogError(LOG_MASTER, "PEER %u requested enc. key update, but had invalid authentication, no response", peerId);
                        return;
                    }
                } else {
                    LogError(LOG_MASTER, "PEER %u requested enc. key update, but had invalid ACL, no response", peerId);
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
                    LogError(LOG_MASTER, "PEER %u requested enc. key update, but had invalid access authentication, no response", peerId);
                    return;
                }
            }

            bool created = false;
            PacketBufferEntryPtr pkt = findOrCreatePacketBufferEntry(mdNetwork->m_peerKeyUpdatePkt, peerId, "Remote EKC, Key Update", streamId, &created);
            if (pkt == nullptr) {
                LogError(LOG_REPL, "PEER %u Remote EKC, Key Update, failed to initialize packet buffer", peerId);
                return;
            }

            if (!created) {
                LogWarning(LOG_REPL, "PEER %u requested enc. key update while another update is active", peerId);
                uint8_t status = KEY_UPDATE_STATUS_BUSY;
                mdNetwork->getFrameQueue()->write(&status, 1U, streamId, peerId, network->getPeerId(),
                    { NET_FUNC::KEYS_UPDATE, NET_SUBFUNC::KEYS_UPDATE_STATUS }, 0U, req->address, req->addrLen);
                return;
            }

            LogInfoEx(LOG_REPL, "PEER %u Remote EKC, Key Update, authenticated transfer streamId = %u", peerId, streamId);
            return;
        }

        // scope intentional
        {
            if (req->length == 80U) {
                LogWarning(LOG_REPL, "PEER %u requested enc. key update while another update is active", peerId);
                uint8_t status = KEY_UPDATE_STATUS_BUSY;
                mdNetwork->getFrameQueue()->write(&status, 1U, streamId, peerId, network->getPeerId(),
                    { NET_FUNC::KEYS_UPDATE, NET_SUBFUNC::KEYS_UPDATE_STATUS }, 0U, req->address, req->addrLen);
                return;
            }

            if (req->length < FRAG_SIZE) {
                LogWarning(LOG_REPL, "PEER %u Remote EKC, Key Update, ignoring short data phase frame (%u bytes)", peerId, req->length);
                return;
            }

            DECLARE_UINT8_ARRAY(rawPayload, req->length);
            ::memcpy(rawPayload, req->buffer, req->length);

            // Utils::dump(1U, "MetadataNetwork::taskNetworkRx(), KEYS_UPDATE, Raw Payload", rawPayload, req->length);

            PacketBufferEntryPtr pkt = findPacketBufferEntry(mdNetwork->m_peerKeyUpdatePkt, peerId);
            if (pkt == nullptr) {
                return;
            }

            std::unique_lock<std::mutex> pktLock(pkt->mutex, std::defer_lock);
            uint32_t timeout = 0U;
            while (!pktLock.try_lock() && timeout < TIMEOUT_MAX_REPL) {
                timeout++;
                Thread::sleep(1U);
            }

            if (!pktLock.owns_lock()) {
                LogError(LOG_STP, "PEER %u Remote EKC, Key Update, timeout waiting for packet buffer to unlock", peerId);
                // detach the stalled transfer without destroying state that
                // another worker still owns; its shared_ptr keeps it alive
                erasePacketBufferEntry(mdNetwork->m_peerKeyUpdatePkt, peerId, pkt);
                return;
            }

            // the worker that previously owned the entry may have completed the
            // transfer and released the lock after resetting the buffer
            if (!pkt->buffer) {
                erasePacketBufferEntry(mdNetwork->m_peerKeyUpdatePkt, peerId, pkt);
                return;
            }

            if (pkt->streamId != streamId) {
                LogError(LOG_REPL, "PEER %u Remote EKC, Key Update, stream ID mismatch, expected %u, got %u", peerId, pkt->streamId, streamId);
                if (pkt->buffer) {
                    pkt->buffer->clear();
                    pkt->buffer.reset();
                }
                pkt->streamId = 0U;
                erasePacketBufferEntry(mdNetwork->m_peerKeyUpdatePkt, peerId, pkt);
                return;
            }

            uint32_t decompressedLen = 0U;
            uint8_t* decompressed = nullptr;

            if (pkt->buffer->decode(rawPayload, &decompressed, &decompressedLen)) {
                std::ostringstream s;
                s << network->m_cryptoLookup->filename();

                std::string filename = s.str();
                std::lock_guard<std::mutex> commitLock(s_keyUpdateCommitMutex);
                std::string tempFilename = filename + ".tmp." + std::to_string(peerId) + "." + std::to_string(streamId);
                std::ofstream file(tempFilename, std::ios::binary | std::ios::trunc);
                if (file.fail()) {
                    LogError(LOG_PEER, "Cannot open the crypto container file - %s", filename.c_str());
                    pkt->buffer->clear();
                    pkt->buffer.reset();
                    pkt->streamId = 0U;
                    if (decompressed != nullptr) {
                        delete[] decompressed;
                    }
                    erasePacketBufferEntry(mdNetwork->m_peerKeyUpdatePkt, peerId, pkt);
                    return;
                }

                file.write(reinterpret_cast<const char*>(decompressed), static_cast<std::streamsize>(decompressedLen));
                file.flush();
                bool writeOk = file.good();

                file.close();

                if (!writeOk || std::rename(tempFilename.c_str(), filename.c_str()) != 0) {
                    LogError(LOG_PEER, "Cannot atomically install the crypto container file - %s", filename.c_str());
                    std::remove(tempFilename.c_str());
                    pkt->buffer->clear();
                    pkt->buffer.reset();
                    pkt->streamId = 0U;
                    if (decompressed != nullptr) {
                        delete[] decompressed;
                    }
                    erasePacketBufferEntry(mdNetwork->m_peerKeyUpdatePkt, peerId, pkt);
                    return;
                }

                network->m_cryptoLookup->stop(true);
                network->m_cryptoLookup->reload();

                pkt->buffer->clear();
                pkt->buffer.reset();
                pkt->streamId = 0U;
                if (decompressed != nullptr) {
                    delete[] decompressed;
                }
                erasePacketBufferEntry(mdNetwork->m_peerKeyUpdatePkt, peerId, pkt);
            }
        }
    }
}
