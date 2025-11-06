// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/Defines.h"
#include "common/p25/kmm/KMMFactory.h"
#include "common/Log.h"
#include "common/Thread.h"
#include "common/Utils.h"
#include "network/FNENetwork.h"
#include "network/P25OTARService.h"
#include "HostFNE.h"

using namespace network;
using namespace network::callhandler;
using namespace network::callhandler::packetdata;
using namespace network::frame;
using namespace network::udp;
using namespace p25;
using namespace p25::defines;
using namespace p25::crypto;
using namespace p25::kmm;

#include <cassert>
#include <chrono>

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

// Macro helper to verbose log a generic KMM.
#define VERBOSE_LOG_KMM(_PCKT_STR, __LLID)                                              \
    if (m_verbose) {                                                                    \
        LogInfoEx(LOG_P25, "KMM, %s, llId = %u", _PCKT_STR.c_str(), __LLID);           \
    }

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define MAX_THREAD_CNT 4U

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the P25OTARService class. */

P25OTARService::P25OTARService(FNENetwork* network, P25PacketData* packetData, bool debug, bool verbose) :
    m_socket(nullptr),
    m_frameQueue(nullptr),
    m_threadPool(MAX_THREAD_CNT, "otar"),
    m_network(network),
    m_packetData(packetData),
    m_rsiMessageNumber(),
    m_allowNoUKEKRekey(false),
    m_debug(debug),
    m_verbose(verbose)
{
    assert(network != nullptr);
    assert(packetData != nullptr);
}

/* Finalizes a instance of the P25OTARService class. */

P25OTARService::~P25OTARService()
{
    if (m_frameQueue != nullptr)
        delete m_frameQueue;
    if (m_socket != nullptr)
        delete m_socket;
}

/* Helper used to process KMM frames from PDU data. */

bool P25OTARService::processDLD(const uint8_t* data, uint32_t len, uint32_t llId, uint8_t n, bool encrypted)
{
    m_packetData->write_PDU_Ack_Response(PDUAckClass::ACK, PDUAckType::ACK, n, llId, false);

    if (m_debug)
        Utils::dump(1U, "P25OTARService::processDLD(), KMM Network Message", data, len);

    uint32_t payloadSize = 0U;
    UInt8Array pduUserData = processKMM(data, len, llId, encrypted, &payloadSize);
    if (pduUserData == nullptr)
        return false;

    // handle DLD encrypted KMM frame
    if (encrypted) {
        // read crypto parameters from KMM header
        uint8_t mi[MI_LENGTH_BYTES];
        ::memset(mi, 0x00U, MI_LENGTH_BYTES);
        for (uint8_t i = 0; i < MI_LENGTH_BYTES; i++) {
            mi[i] = data[i];
        }

        uint8_t algoId = data[9U];
        uint16_t kid = GET_UINT16(data, 10U);

        // re-encrypt the KMM response
        pduUserData = cryptKMM(algoId, kid, mi, pduUserData.get(), payloadSize, true);
        if (pduUserData == nullptr) {
            LogError(LOG_P25, P25_KMM_STR ", unable to encrypt KMM response, algoId = $%02X, kID = $%04X", algoId, kid);
            return false;
        }
    }

    m_packetData->write_PDU_KMM(pduUserData.get(), payloadSize, llId, encrypted);
    return true;
}

/* Updates the timer by the passed number of milliseconds. */

void P25OTARService::clock(uint32_t ms)
{
    if (m_socket != nullptr) {
        sockaddr_storage address;
        uint32_t addrLen;
        int length = 0U;

        // read message
        UInt8Array buffer = m_frameQueue->read(length, address, addrLen);
        if (length > 0) {
            if (m_debug)
                Utils::dump(1U, "P25OTARService::clock(), KMM Network Message", buffer.get(), length);

            OTARPacketRequest* req = new OTARPacketRequest();
            req->obj = this;

            req->address = address;
            req->addrLen = addrLen;

            req->length = length;
            req->buffer = new uint8_t[length];
            ::memcpy(req->buffer, buffer.get(), length);

            // enqueue the task
            if (!m_threadPool.enqueue(new_pooltask(taskNetworkRx, req))) {
                LogError(LOG_P25, "Failed to task enqueue KMM network packet request, %s:%u", 
                    udp::Socket::address(address).c_str(), udp::Socket::port(address));
                if (req != nullptr) {
                    if (req->buffer != nullptr)
                        delete[] req->buffer;
                    delete req;
                }
            }
        }
    }
}

/* Opens a connection to the OTAR port. */

bool P25OTARService::open(const std::string& address, uint16_t port)
{
    m_socket = new Socket(port);
    m_frameQueue = new RawFrameQueue(m_socket, m_debug);

    sockaddr_storage addr;
    uint32_t addrLen;
    if (udp::Socket::lookup(address, port, addr, addrLen) != 0)
        addrLen = 0U;

    if (addrLen > 0U) {
        m_threadPool.start();

        if (m_socket != nullptr) {
            return m_socket->open(addr);
        }
    }

    return false;
}

/* Closes the connection to the OTAR port. */

void P25OTARService::close()
{
    if (m_socket != nullptr) {
        m_threadPool.stop();
        m_threadPool.wait();

        m_socket->close();
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Process a data frames from the network. */

void P25OTARService::taskNetworkRx(OTARPacketRequest* req)
{
    if (req != nullptr) {
        P25OTARService* network = static_cast<P25OTARService*>(req->obj);
        if (network == nullptr) {
            delete req;
            return;
        }

        if (req->length >= 13) {
            // read crypto parameters from KMM header
            uint8_t mfId = req->buffer[1U];
            uint8_t algoId = req->buffer[2U];
            uint16_t kid = GET_UINT16(req->buffer, 3U);

            uint8_t mi[MI_LENGTH_BYTES];
            ::memset(mi, 0x00U, MI_LENGTH_BYTES);
            for (uint8_t i = 0; i < MI_LENGTH_BYTES; i++) {
                mi[i] = req->buffer[4U + i];
            }

            // KMM frame
            UInt8Array buffer = std::make_unique<uint8_t[]>(req->length - 13U);
            ::memset(buffer.get(), 0x00U, req->length - 13U);
            ::memcpy(buffer.get(), req->buffer + 13U, req->length - 13U);

            bool encrypted = (mfId != ALGO_UNENCRYPT);
            if (encrypted) {
                buffer = network->cryptKMM(algoId, kid, mi, buffer.get(), req->length - 13U, false);
                if (buffer == nullptr) {
                    LogError(LOG_P25, P25_KMM_STR ", unable to decrypt KMM, algoId = $%02X, kID = $%04X", algoId, kid);
                    if (req->buffer != nullptr)
                        delete[] req->buffer;
                    delete req;
                    return;
                }
            }

            uint32_t payloadSize = 0U;
            UInt8Array pduUserData = network->processKMM(buffer.get(), req->length - 13U, 0U, false, &payloadSize);

            if (encrypted) {
                // re-encrypt the KMM response
                pduUserData = network->cryptKMM(algoId, kid, mi, pduUserData.get(), payloadSize, true);
                if (pduUserData == nullptr) {
                    LogError(LOG_P25, P25_KMM_STR ", unable to encrypt KMM response, algoId = $%02X, kID = $%04X", algoId, kid);
                    if (req->buffer != nullptr)
                        delete[] req->buffer;
                    delete req;
                    return;
                }
            }

            network->m_frameQueue->write(pduUserData.get(), payloadSize, req->address, req->addrLen);
        }

        if (req->buffer != nullptr)
            delete[] req->buffer;
        delete req;
    }
}

/* Encrypt/decrypt KMM frame. */

UInt8Array P25OTARService::cryptKMM(uint8_t algoId, uint16_t kid, uint8_t* mi, const uint8_t* buffer, uint32_t len, bool encrypt)
{
    assert(buffer != nullptr);

    P25Crypto crypto;
    if (!encrypt)
        crypto.setMI(mi);
    else {
        crypto.generateMI();
        crypto.getMI(mi);
    }

    UInt8Array outBuffer = std::make_unique<uint8_t[]>(len);
    ::memset(outBuffer.get(), 0x00U, len);
    ::memcpy(outBuffer.get(), buffer, len);

    if (algoId == P25DEF::ALGO_UNENCRYPT)
        return outBuffer;

    /*
    ** bryanb: Architecturally this is a problem. Because KMF services would essentially be limited to the local FNE
    **  because we aren't performing FNE KEY_REQ's to upstream peer'ed FNEs to find the key used to encrypt the KMM.
    */

    ::EKCKeyItem keyItem = m_network->m_cryptoLookup->find(kid);
    if (!keyItem.isInvalid()) {
        uint8_t key[P25DEF::MAX_ENC_KEY_LENGTH_BYTES];
        ::memset(key, 0x00U, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
        uint8_t keyLength = keyItem.getKey(key);

        if (m_network->m_debug) {
            LogDebugEx(LOG_P25, "P25OTARService::cryptKMM()", "keyLength = %u", keyLength);
            Utils::dump(1U, "P25OTARService::cryptKMM(), Key", key, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
        }

        LogInfoEx(LOG_P25, P25_KMM_STR ", algId = $%02X, kID = $%04X", algoId, kid);
        crypto.setKey(key, keyLength);
        crypto.generateKeystream();

        switch (algoId) {
        case P25DEF::ALGO_AES_256:
            crypto.cryptAES_PDU(outBuffer.get(), len);
            return outBuffer;
        default:
            LogError(LOG_P25, "unsupported KEK algorithm, algoId = $%02X", algoId);
            break;
        }
    }

    return nullptr;
}

/* Helper used to process KMM frames. */

UInt8Array P25OTARService::processKMM(const uint8_t* data, uint32_t len, uint32_t llId, bool encrypted, uint32_t* payloadSize)
{
    if (payloadSize != nullptr)
        *payloadSize = 0U;

    UInt8Array buffer = std::make_unique<uint8_t[]>(len);
    ::memset(buffer.get(), 0x00U, len);
    ::memcpy(buffer.get(), data, len);

    // handle DLD encrypted KMM frame
    if (encrypted) {
        // read crypto parameters from KMM header
        uint8_t mi[MI_LENGTH_BYTES];
        ::memset(mi, 0x00U, MI_LENGTH_BYTES);
        for (uint8_t i = 0; i < MI_LENGTH_BYTES; i++) {
            mi[i] = data[i];
        }

        uint8_t algoId = data[9U];
        uint16_t kid = GET_UINT16(data, 10U);

        // decrypt frame before processing
        buffer = cryptKMM(algoId, kid, mi, data + 10U, len - 10U, false);
        if (buffer == nullptr) {
            LogError(LOG_P25, P25_KMM_STR ", unable to decrypt KMM, algoId = $%02X, kID = $%04X", algoId, kid);
            return nullptr;
        }

        if (m_debug)
            Utils::dump(1U, "P25OTARService::processKMM(), (Decrypted) KMM Network Message", buffer.get(), len);
    }

    std::unique_ptr<KMMFrame> frame = KMMFactory::create(buffer.get());
    if (frame == nullptr) {
        LogWarning(LOG_P25, P25_KMM_STR ", undecodable KMM packet");
        return nullptr;
    }

    if (llId == 0U) {
        llId = frame->getSrcLLId();
    }

    // does this llId have a RSI message number setup?
    if (m_rsiMessageNumber.find(llId) == m_rsiMessageNumber.end()) {
        m_rsiMessageNumber[llId] = 0U;
    } else {
        // roll message number
        uint16_t mn = m_rsiMessageNumber[llId];
        mn++;

        m_rsiMessageNumber[llId] = mn;
    }

    switch (frame->getMessageId()) {
        case KMM_MessageType::HELLO:
        {
            KMMHello* kmm = static_cast<KMMHello*>(frame.get());
            if (m_verbose) {
                LogInfoEx(LOG_P25, P25_KMM_STR ", %s, llId = %u, flag = $%02X", kmm->toString().c_str(),
                    llId, kmm->getFlag());
                switch (kmm->getFlag()) {
                case KMM_HelloFlag::REKEY_REQUEST_UKEK:
                    LogInfoEx(LOG_P25, P25_KMM_STR ", %s, rekey requested with UKEK, llId = %u", kmm->toString().c_str(), llId);
                    break;
                case KMM_HelloFlag::REKEY_REQUEST_NO_UKEK:
                    LogInfoEx(LOG_P25, P25_KMM_STR ", %s, rekey requested with no UKEK, llId = %u", kmm->toString().c_str(), llId);
                    break;
                }
            }

            // respond with No-Service if KMF services are disabled
//            if (!m_network->m_kmfServicesEnabled)
                return write_KMM_NoService(llId, kmm->getSrcLLId(), payloadSize);
        }
        break;

        case KMM_MessageType::NAK:
        {
            KMMNegativeAck* kmm = static_cast<KMMNegativeAck*>(frame.get());
            LogInfoEx(LOG_P25, P25_KMM_STR ", %s, llId = %u, messageId = $%02X, messageNo = %u, status = $%02X", kmm->toString().c_str(),
                llId, kmm->getMessageId(), kmm->getMessageNumber(), kmm->getStatus());
            logResponseStatus(llId, kmm->toString(), kmm->getStatus());
        }
        break;

        case KMM_MessageType::REKEY_ACK:
        {
            KMMRekeyAck* kmm = static_cast<KMMRekeyAck*>(frame.get());
            LogInfoEx(LOG_P25, P25_KMM_STR ", %s, llId = %u, messageId = $%02X, numOfStatus = %u", kmm->toString().c_str(),
                llId, kmm->getMessageId(), kmm->getNumberOfKeyStatus());

            if (kmm->getNumberOfKeyStatus() > 0U) {
                for (auto entry : kmm->getKeyStatus()) {
                    LogInfoEx(LOG_P25, P25_KMM_STR ", %s, llId = %u, algId = $%02X, kId = $%04X, status = $%02X", kmm->toString().c_str(),
                        llId, entry.algId(), entry.kId(), entry.status());
                    logResponseStatus(llId, kmm->toString(), entry.status());
                }
            }
        }
        break;

        case KMM_MessageType::DEREG_CMD:
        {
            KMMDeregistrationCommand* kmm = static_cast<KMMDeregistrationCommand*>(frame.get());
            LogInfoEx(LOG_P25, P25_KMM_STR ", %s, llId = %u", kmm->toString().c_str(),
                llId);

            // respond with No-Service if KMF services are disabled
            if (!m_network->m_kmfServicesEnabled)
                return write_KMM_NoService(llId, kmm->getSrcLLId(), payloadSize);
            else
                return write_KMM_Dereg_Response(llId, kmm->getSrcLLId(), payloadSize);
        }
        break;
        case KMM_MessageType::REG_RSP:
        {
            KMMRegistrationResponse* kmm = static_cast<KMMRegistrationResponse*>(frame.get());
            LogInfoEx(LOG_P25, P25_KMM_STR ", %s, llId = %u, status = $%02X", kmm->toString().c_str(),
                llId, kmm->getStatus());
            logResponseStatus(llId, kmm->toString(), kmm->getStatus());
        }
        break;

        case KMM_MessageType::UNABLE_TO_DECRYPT:
        {
            KMMUnableToDecrypt* kmm = static_cast<KMMUnableToDecrypt*>(frame.get());
            LogInfoEx(LOG_P25, P25_KMM_STR ", %s, llId = %u, status = $%02X", kmm->toString().c_str(),
                llId, kmm->getStatus());
            logResponseStatus(llId, kmm->toString(), kmm->getStatus());
        }

        default:
        break;
    } // switch (frame->getMessageId())

    return nullptr;
}

/* Helper used to return a Rekey-Command KMM to the calling SU. */

UInt8Array P25OTARService::write_KMM_Rekey_Command(uint32_t llId, uint32_t kmmRSI, uint8_t flags, uint32_t* payloadSize)
{
    uint8_t mi[MI_LENGTH_BYTES];
    ::memset(mi, 0x00U, MI_LENGTH_BYTES);

    uint16_t mn = 0U;
    if (m_rsiMessageNumber.find(llId) != m_rsiMessageNumber.end()) {
        mn = m_rsiMessageNumber[llId];
    }

    P25Crypto crypto;
    crypto.generateMI();
    crypto.getMI(mi);

    KMMRekeyCommand outKmm = KMMRekeyCommand();

    /*
    ** bryanb: Architecturally this is a problem. Because KMF services would essentially be limited to the local FNE
    **  because we aren't performing FNE KEY_REQ's to upstream peer'ed FNEs to find the key used to encrypt the KMM.
    */

    uint8_t kekKey[P25DEF::MAX_ENC_KEY_LENGTH_BYTES];
    ::memset(kekKey, 0x00U, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);

    uint8_t kekAlgId = P25DEF::ALGO_UNENCRYPT;
    uint16_t kekKId = 0U;

    ::EKCKeyItem keyItem = m_network->m_cryptoLookup->findUKEK(kmmRSI);
    if (!keyItem.isInvalid()) {
        uint8_t keyLength = keyItem.getKey(kekKey);

        kekAlgId = keyItem.algId();
        kekKId = keyItem.kId();

        if (m_network->m_debug) {
            LogDebugEx(LOG_P25, "P25OTARService::cryptKMM()", "keyLength = %u", keyLength);
            Utils::dump(1U, "P25OTARService::cryptKMM(), Key", kekKey, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
        }
    }
    else {
        if (!m_allowNoUKEKRekey) {
            LogError(LOG_P25, P25_KMM_STR", %s, aborting rekey, no KEK to keyload with, llId = %u, RSI = %u", outKmm.toString().c_str(),
                outKmm.getSrcLLId(), outKmm.getDstLLId());
            return nullptr;
        } else {
            LogWarning(LOG_P25, P25_KMM_STR", %s, WARNING WARNING WARNING, rekey without KEK enabled, WARNING WARNING WARNING, keys transmitted in the clear, llId = %u, RSI = %u", outKmm.toString().c_str(),
                outKmm.getSrcLLId(), outKmm.getDstLLId());
        }
    }

    outKmm.setDecryptInfoFmt(KMM_DECRYPT_INSTRUCT_NONE);
    outKmm.setSrcLLId(WUID_FNE);
    outKmm.setDstLLId(kmmRSI);

    outKmm.setMACType(KMM_MAC::ENH_MAC);
    outKmm.setMACAlgId(kekAlgId);
    outKmm.setMACKId(kekKId);
    outKmm.setMACFormat(KMM_MAC_FORMAT_CBC);

    outKmm.setMessageNumber(mn);

    outKmm.setAlgId(kekAlgId);
    outKmm.setKId(kekKId);

    KeysetItem ks;
    ks.keysetId(1U);
    ks.algId(ALGO_AES_256); // we currently can only OTAR AES256 keys
    if (kekAlgId != P25DEF::ALGO_UNENCRYPT)
        ks.keyLength(P25DEF::MAX_WRAPPED_ENC_KEY_LENGTH_BYTES);
    else
        ks.keyLength(P25DEF::MAX_ENC_KEY_LENGTH_BYTES);

    for (EKCKeyItem keyItem : m_network->m_cryptoLookup->keys()) {
        if (keyItem.algId() != ALGO_AES_256) {
            LogWarning(LOG_P25, P25_KMM_STR", %s, ignoring kId = %u, is not an AES-256 key, llId = %u, RSI = %u", outKmm.toString().c_str(),
                keyItem.kId(), outKmm.getSrcLLId(), outKmm.getDstLLId());
            continue;
        }

        uint8_t key[P25DEF::MAX_WRAPPED_ENC_KEY_LENGTH_BYTES];
        ::memset(key, 0x00U, P25DEF::MAX_WRAPPED_ENC_KEY_LENGTH_BYTES);
        uint8_t keyLength = keyItem.getKey(key);

        // encrypt key
        UInt8Array wrappedKey = nullptr;
        if (kekAlgId != P25DEF::ALGO_UNENCRYPT) {
            wrappedKey = crypto.cryptAES_TEK(kekKey, key, keyLength);
            keyLength = P25DEF::MAX_WRAPPED_ENC_KEY_LENGTH_BYTES;
        } else {
            wrappedKey = std::make_unique<uint8_t[]>(P25DEF::MAX_WRAPPED_ENC_KEY_LENGTH_BYTES);
            ::memcpy(wrappedKey.get(), key, keyLength);
        }

        p25::kmm::KeyItem ki = p25::kmm::KeyItem();
        ki.keyFormat(KEY_FORMAT_TEK);
        ki.kId((uint16_t)keyItem.kId());
        ki.sln((uint16_t)keyItem.sln());
        ki.setKey(wrappedKey.get(), keyLength);

        ks.push_back(ki);
    }

    if (ks.keys().size() == 0U) {
        LogWarning(LOG_P25, P25_KMM_STR", %s, aborting rekey, no keys to keyload, llId = %u, RSI = %u", outKmm.toString().c_str(),
            outKmm.getSrcLLId(), outKmm.getDstLLId());
        return nullptr;
    }

    std::vector<KeysetItem> keysets;
    keysets.push_back(ks);

    outKmm.setKeysets(keysets);

    if (m_verbose) {
        LogInfoEx(LOG_P25, P25_KMM_STR ", %s, llId = %u, RSI = %u, keyCount = %u", outKmm.toString().c_str(),
            outKmm.getSrcLLId(), outKmm.getDstLLId(), ks.keys().size());
    }

    if (payloadSize != nullptr)
        *payloadSize = outKmm.length();

    UInt8Array kmmFrame = std::make_unique<uint8_t[]>(outKmm.length());
    outKmm.encode(kmmFrame.get());
    outKmm.generateMAC(kekKey, kmmFrame.get());
    return kmmFrame;
}

/* Helper used to return a Registration-Command KMM to the calling SU. */

UInt8Array P25OTARService::write_KMM_Reg_Command(uint32_t llId, uint32_t kmmRSI, uint32_t* payloadSize)
{
    KMMRegistrationCommand outKmm = KMMRegistrationCommand();
    outKmm.setSrcLLId(WUID_FNE);
    outKmm.setDstLLId(kmmRSI);

    if (m_verbose) {
        LogInfoEx(LOG_P25, P25_KMM_STR ", %s, llId = %u, RSI = %u", outKmm.toString().c_str(),
            outKmm.getSrcLLId(), outKmm.getDstLLId());
    }

    if (payloadSize != nullptr)
        *payloadSize = outKmm.length();

    UInt8Array kmmFrame = std::make_unique<uint8_t[]>(outKmm.length());
    outKmm.encode(kmmFrame.get());
    return kmmFrame;
}

/* Helper used to return a Deregistration-Response KMM to the calling SU. */

UInt8Array P25OTARService::write_KMM_Dereg_Response(uint32_t llId, uint32_t kmmRSI, uint32_t* payloadSize)
{
    KMMDeregistrationResponse outKmm = KMMDeregistrationResponse();
    outKmm.setSrcLLId(WUID_FNE);
    outKmm.setDstLLId(kmmRSI);
    outKmm.setStatus(KMM_Status::CMD_PERFORMED);

    if (m_verbose) {
        LogInfoEx(LOG_P25, P25_KMM_STR ", %s, llId = %u, RSI = %u", outKmm.toString().c_str(),
            outKmm.getSrcLLId(), outKmm.getDstLLId());
    }

    if (payloadSize != nullptr)
        *payloadSize = outKmm.length();

    UInt8Array kmmFrame = std::make_unique<uint8_t[]>(outKmm.length());
    outKmm.encode(kmmFrame.get());
    return kmmFrame;
}

/* Helper used to return a No-Service KMM to the calling SU. */

UInt8Array P25OTARService::write_KMM_NoService(uint32_t llId, uint32_t kmmRSI, uint32_t* payloadSize)
{
    KMMNoService outKmm = KMMNoService();
    outKmm.setSrcLLId(WUID_FNE);
    outKmm.setDstLLId(kmmRSI);

    if (m_verbose) {
        LogInfoEx(LOG_P25, P25_KMM_STR ", %s, llId = %u, RSI = %u", outKmm.toString().c_str(),
            outKmm.getSrcLLId(), outKmm.getDstLLId());
    }

    if (payloadSize != nullptr)
        *payloadSize = outKmm.length();
    
    UInt8Array kmmFrame = std::make_unique<uint8_t[]>(outKmm.length());
    outKmm.encode(kmmFrame.get());
    return kmmFrame;
}

/* Helper used to return a Zeroize KMM to the calling SU. */

UInt8Array P25OTARService::write_KMM_Zeroize(uint32_t llId, uint32_t kmmRSI, uint32_t* payloadSize)
{
    KMMZeroize outKmm = KMMZeroize();
    outKmm.setSrcLLId(WUID_FNE);
    outKmm.setDstLLId(kmmRSI);

    if (m_verbose) {
        LogInfoEx(LOG_P25, P25_KMM_STR ", %s, llId = %u, RSI = %u", outKmm.toString().c_str(),
            outKmm.getSrcLLId(), outKmm.getDstLLId());
    }

    if (payloadSize != nullptr)
        *payloadSize = outKmm.length();

    UInt8Array kmmFrame = std::make_unique<uint8_t[]>(outKmm.length());
    outKmm.encode(kmmFrame.get());
    return kmmFrame;
}

/* Helper used to log a KMM response. */

void P25OTARService::logResponseStatus(uint32_t llId, std::string kmmString, uint8_t status)
{
    switch (status) {
    case KMM_Status::CMD_PERFORMED:
        if (m_verbose) {
            LogInfoEx(LOG_P25, P25_KMM_STR ", %s, command performed, llId = %u", kmmString.c_str(), llId);
        }
        break;
    case KMM_Status::CMD_NOT_PERFORMED:
        LogWarning(LOG_P25, P25_KMM_STR ", %s, command not performed, llId = %u", kmmString.c_str(), llId);
        break;

    case KMM_Status::ITEM_NOT_EXIST:
        LogWarning(LOG_P25, P25_KMM_STR ", %s, item does not exist, llId = %u", kmmString.c_str(), llId);
        break;
    case KMM_Status::INVALID_MSG_ID:
        LogWarning(LOG_P25, P25_KMM_STR ", %s, invalid message ID, llId = %u", kmmString.c_str(), llId);
        break;
    case KMM_Status::INVALID_MAC:
        LogWarning(LOG_P25, P25_KMM_STR ", %s, invalid auth code, llId = %u", kmmString.c_str(), llId);
        break;

    case KMM_Status::OUT_OF_MEMORY:
        LogWarning(LOG_P25, P25_KMM_STR ", %s, out of memory, llId = %u", kmmString.c_str(), llId);
        break;
    case KMM_Status::FAILED_TO_DECRYPT:
        LogWarning(LOG_P25, P25_KMM_STR ", %s, failed to decrypt message, llId = %u", kmmString.c_str(), llId);
        break;

    case KMM_Status::INVALID_MSG_NUMBER:
        LogWarning(LOG_P25, P25_KMM_STR ", %s, invalid message number, llId = %u", kmmString.c_str(), llId);
        break;
    case KMM_Status::INVALID_KID:
        LogWarning(LOG_P25, P25_KMM_STR ", %s, invalid key ID, llId = %u", kmmString.c_str(), llId);
        break;
    case KMM_Status::INVALID_ALGID:
        LogWarning(LOG_P25, P25_KMM_STR ", %s, invalid algorithm ID, llId = %u", kmmString.c_str(), llId);
        break;
    case KMM_Status::INVALID_MFID:
        LogWarning(LOG_P25, P25_KMM_STR ", %s, invalid manufacturer ID, llId = %u", kmmString.c_str(), llId);
        break;

    case KMM_Status::MI_ALL_ZERO:
        LogWarning(LOG_P25, P25_KMM_STR ", %s, message indicator was all zeros, llId = %u", kmmString.c_str(), llId);
        break;
    case KMM_Status::KEY_FAIL:
        LogWarning(LOG_P25, P25_KMM_STR ", %s, key identified by algo/key is erased, llId = %u", kmmString.c_str(), llId);
        break;

    case KMM_Status::UNKNOWN:
    default:
        LogWarning(LOG_P25, P25_KMM_STR ", llId = %u, status = $%02X; unknown status", llId, status);
        break;
    }
}
