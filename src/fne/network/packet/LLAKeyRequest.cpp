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
#include "common/p25/kmm/KMMFactory.h"
#include "common/AESCrypto.h"
#include "common/Log.h"
#include "network/TrafficNetwork.h"
#include "fne/ActivityLog.h"
#include "HostFNE.h"

using namespace network;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Handles NET_FUNC::KEY_LLA_REQ packets. */

void TrafficNetwork::PacketHandler::llaKeyRequest(TrafficNetwork* network, NetPacketRequest* req, uint32_t peerId, uint32_t ssrc, uint32_t streamId, uint64_t now)
{
    using namespace p25::defines;
    using namespace p25::kmm;

    if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
        FNEPeerConnection* connection = network->m_peers[peerId];
        if (connection != nullptr) {
            std::string ip = udp::Socket::address(req->address);

            // validate peer (simple validation really)
            if (connection->connected() && connection->address() == ip) {
                // is this peer allowed to request keys?
                if (network->m_peerListLookup->getACL()) {
                    lookups::PeerId peerEntry = network->m_peerListLookup->find(peerId);
                    if (peerEntry.peerDefault()) {
                        return;
                    } else {
                        if (!peerEntry.canRequestKeys()) {
                            LogError(LOG_MASTER, "PEER %u (%s) requested enc. key but is not allowed, no response", peerId, connection->identWithQualifier().c_str());
                            return;
                        }
                    }
                }

                std::unique_ptr<KMMFrame> frame = KMMFactory::create(req->buffer + 11U);
                if (frame == nullptr) {
                    LogWarning(LOG_MASTER, "PEER %u (%s), undecodable KMM frame from peer", peerId, connection->identWithQualifier().c_str());
                    return;
                }

                switch (frame->getMessageId()) {
                case P25DEF::KMM_MessageType::MODIFY_KEY_CMD:
                    {
                        KMMModifyKey* modifyKey = static_cast<KMMModifyKey*>(frame.get());

                        if (modifyKey->getAlgId() == ALGO_AES_128 && modifyKey->getDstLLId() > 0U) {
                            if (network->m_debug)
                                LogDebugEx(LOG_MASTER, "TrafficNetwork::taskNetworkRx()", "PEER %u (%s) LLA enc. key request received, dstLLId = %u, algId = %u, kId = %u", peerId, connection->identWithQualifier().c_str(), 
                                    modifyKey->getDstLLId(), modifyKey->getAlgId(), modifyKey->getKId());

                            uint32_t requestingRid = modifyKey->getDstLLId();

                            LogInfoEx(LOG_MASTER, "PEER %u (%s) requested LLA enc. key, rsi = %u", peerId, connection->identWithQualifier().c_str(),
                                requestingRid);

                            ::EKCKeyItem keyItem = network->m_cryptoLookup->findLLA(requestingRid);
                            if (!keyItem.isInvalid()) {
                                uint8_t key[P25DEF::MAX_ENC_KEY_LENGTH_BYTES];
                                ::memset(key, 0x00U, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
                                uint8_t keyLength = keyItem.getKey(key);

                                if (network->m_debug) {
                                    LogDebugEx(LOG_HOST, "TrafficNetwork::threadedNetworkRx()", "keyLength = %u", keyLength);
                                    Utils::dump(1U, "TrafficNetwork::taskNetworkRx(), Key", key, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
                                }

                                LogInfoEx(LOG_MASTER, "PEER %u (%s) local enc. key, algId = $%02X, rsi = %u", peerId, connection->identWithQualifier().c_str(),
                                    modifyKey->getAlgId(), requestingRid);

                                // if configured to encrypt the key with a preshared key, do that now
                                if (network->m_kmfEncKeyRequest) {
                                    uint8_t* encryptedKey = nullptr;
                                    if (network->m_kmfPresharedKey != nullptr) {
                                        crypto::AES aes = crypto::AES(crypto::AESKeyLength::AES_256);
                                        encryptedKey = aes.encryptECB(key, P25DEF::MAX_ENC_KEY_LENGTH_BYTES, network->m_kmfPresharedKey);

                                        if (encryptedKey != nullptr) {
                                            ::memcpy(key, encryptedKey, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
                                            keyLength = P25DEF::MAX_ENC_KEY_LENGTH_BYTES;
                                            delete[] encryptedKey;
                                        }
                                    }

                                    if (network->m_debug) {
                                        LogDebugEx(LOG_HOST, "TrafficNetwork::threadedNetworkRx()", "keyLength = %u", keyLength);
                                        Utils::dump(1U, "TrafficNetwork::taskNetworkRx(), Encrypted Key", key, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
                                    }
                                }

                                // build response buffer
                                uint8_t buffer[DATA_PACKET_LENGTH];
                                ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

                                KMMModifyKey modifyKeyRsp = KMMModifyKey();
                                modifyKeyRsp.setDecryptInfoFmt(network->m_kmfEncKeyRequest ? KMM_DECRYPT_PEER_ENC : KMM_DECRYPT_INSTRUCT_NONE);
                                modifyKeyRsp.setAlgId(modifyKey->getAlgId());
                                modifyKeyRsp.setKId(0U);
                                modifyKeyRsp.setSrcLLId(WUID_FNE);
                                modifyKeyRsp.setDstLLId(requestingRid);

                                KeysetItem ks = KeysetItem();
                                ks.keysetId(1U);
                                ks.algId(modifyKey->getAlgId());
                                ks.keyLength(keyLength);

                                p25::kmm::KeyItem ki = p25::kmm::KeyItem();
                                ki.keyFormat(KMM_KEY_FORMAT_TEK);
                                ki.kId((uint16_t)keyItem.kId());
                                ki.sln((uint16_t)keyItem.sln());
                                ki.setKey(key, keyLength);

                                ks.push_back(ki);
                                modifyKeyRsp.setKeysetItem(ks);

                                modifyKeyRsp.encode(buffer + 11U);

                                network->writePeer(peerId, network->m_peerId, { NET_FUNC::KEY_LLA_RSP, NET_SUBFUNC::NOP }, buffer, modifyKeyRsp.length() + 11U, 
                                    RTP_END_OF_CALL_SEQ, network->createStreamId());
                            } else {
                                // attempt to forward KMM key request to replica masters
                                if (network->m_host->m_peerNetworks.size() > 0) {
                                    for (auto& peer : network->m_host->m_peerNetworks) {
                                        if (peer.second != nullptr) {
                                            if (peer.second->isEnabled() && peer.second->isReplica()) {
                                                LogInfoEx(LOG_PEER, "PEER %u (%s) no local key or container, requesting key from upstream master, algId = $%02X, rsi = %u", peerId, connection->identWithQualifier().c_str(),
                                                    modifyKey->getAlgId(), requestingRid);

                                                bool locked = network->s_llaKeyQueueMutex.try_lock_for(std::chrono::milliseconds(60));
                                                network->m_peerReplicaLLAKeyQueue[peerId] = modifyKey->getDstLLId();

                                                if (locked)
                                                    network->s_llaKeyQueueMutex.unlock();

                                                peer.second->writeMaster({ NET_FUNC::KEY_LLA_REQ, NET_SUBFUNC::NOP }, 
                                                    req->buffer, req->length, RTP_END_OF_CALL_SEQ, 0U, false);
                                            }
                                        }
                                    }
                                } else {
                                    LogError(LOG_MASTER, "PEER %u (%s) requested LLA enc. key with no local key and no upstream masters to query, algId = $%02X, rsi = %u, no response", peerId, connection->identWithQualifier().c_str(),
                                        modifyKey->getAlgId(), requestingRid);
                                }
                            }
                        }
                    }
                    break;

                default:
                    break;
                }
            }
            else {
                network->writePeerNAK(peerId, streamId, TAG_REPEATER_KEY, NET_CONN_NAK_FNE_UNAUTHORIZED);
            }
        }
    }
}
