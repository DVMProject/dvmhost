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
#include "common/p25/kmm/KMMFactory.h"
#include "common/Log.h"
#include "network/Network.h"

using namespace network;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Handles NET_FUNC::KEY_RSP packets. */

bool Network::PacketHandler::keyResponse(Network* network, uint32_t peerId, uint32_t streamId, uint64_t now,
    const frame::RTPFNEHeader& fneHeader, const frame::RTPHeader& rtpHeader, const uint8_t* buffer, int length)
{
    (void)peerId;
    (void)streamId;
    (void)now;
    (void)fneHeader;
    (void)rtpHeader;
    (void)length;

    if (network->m_enabled) {
        using namespace p25::kmm;

        std::unique_ptr<KMMFrame> frame = KMMFactory::create(buffer + 11U);
        if (frame == nullptr) {
            LogWarning(LOG_NET, "PEER %u, undecodable KMM frame from master", network->m_peerId);
            return false;
        }

        switch (frame->getMessageId()) {
        case P25DEF::KMM_MessageType::MODIFY_KEY_CMD:
            {
                KMMModifyKey* modifyKey = static_cast<KMMModifyKey*>(frame.get());
                if (modifyKey->getAlgId() > 0U) {
                    KeysetItem ks = modifyKey->getKeysetItem();
                    if (ks.keys().size() > 0U) {
                        // fetch first key (a master response should never really send back more then one key)
                        KeyItem ki = ks.keys()[0];
                        LogInfoEx(LOG_NET, "PEER %u, master reported enc. key, algId = $%02X, kID = $%04X", network->m_peerId,
                            ks.algId(), ki.kId());

                        if (!network->m_passKeysWithNoPresharedKey && (modifyKey->getDecryptInfoFmt() == KMM_DECRYPT_PEER_ENC)) {
                            // if the to decrypt the key with a preshared key, do that now
                            if (network->m_kmfPresharedKey != nullptr) {
                                uint8_t encryptedKey[P25DEF::MAX_ENC_KEY_LENGTH_BYTES];
                                ::memset(encryptedKey, 0x00U, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
                                ki.getKey(encryptedKey);

                                uint8_t* key = nullptr;
                                crypto::AES aes = crypto::AES(crypto::AESKeyLength::AES_256);
                                key = aes.decryptECB(encryptedKey, P25DEF::MAX_ENC_KEY_LENGTH_BYTES, network->m_kmfPresharedKey);

                                uint32_t keyLength = P25DEF::MAX_ENC_KEY_LENGTH_BYTES;
                                switch (ks.algId()) {
                                case P25DEF::ALGO_DES:
                                    keyLength = P25DEF::DES_ENC_KEY_LENGTH_BYTES;
                                    break;
                                case P25DEF::ALGO_ARC4:
                                    keyLength = P25DEF::ARC4_ENC_KEY_LENGTH_BYTES;
                                    break;

                                case P25DEF::ALGO_AES_256:
                                    break;
                                default:
                                    LogWarning(LOG_NET, "PEER %u, unknown algorithm ID $%02X, unable to determine key length", network->m_peerId, ks.algId());
                                    break;
                                }

                                if (network->m_debug)
                                    Utils::dump(1U, "Network::clock(), Key", key, keyLength);

                                if (key != nullptr) {
                                    ki.setKey(key, keyLength);
                                    ks.keyLength(keyLength);
                                    delete[] key;
                                }
                            }
                            else {
                                if (modifyKey->getDecryptInfoFmt() == KMM_DECRYPT_PEER_ENC) {
                                    LogInfoEx(LOG_NET, "PEER %u, received encrypted enc. key, but no preshared key available, algId = $%02X, kID = $%04X", network->m_peerId,
                                        ks.algId(), ki.kId());
                                    break;
                                }
                            }
                        }

                        // fire off key response callback if we have one
                        if (network->m_keyRespCallback != nullptr) {
                            network->m_keyRespCallback(ki, ks.algId(), ks.keyLength());
                        }
                    }
                }
            }
            break;

        default:
            break;
        }
    }

    return false;
}
