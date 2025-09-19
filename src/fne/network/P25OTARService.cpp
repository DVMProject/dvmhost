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
        LogMessage(LOG_NET, "KMM, %s, llId = %u", _PCKT_STR.c_str(), __LLID);           \
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

    ::KeyItem keyItem = m_network->m_cryptoLookup->find(kid);
    if (!keyItem.isInvalid()) {
        uint8_t key[P25DEF::MAX_ENC_KEY_LENGTH_BYTES];
        ::memset(key, 0x00U, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
        uint8_t keyLength = keyItem.getKey(key);

        if (m_network->m_debug) {
            LogDebugEx(LOG_HOST, "P25OTARService::cryptKMM()", "keyLength = %u", keyLength);
            Utils::dump(1U, "P25OTARService::cryptKMM(), Key", key, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
        }

        LogMessage(LOG_P25, P25_KMM_STR ", algId = $%02X, kID = $%04X", algoId, kid);
        crypto.setKey(key, keyLength);
        crypto.generateKeystream();

        switch (algoId) {
        case P25DEF::ALGO_AES_256:
            // TODO: implement handling AES-256 KMM encryption/decryption for data blocks
            return outBuffer;
        default:
            LogError(LOG_HOST, "unsupported KEK algorithm, algoId = $%02X", algoId);
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

    switch (frame->getMessageId()) {
        case KMM_MessageType::HELLO:
        {
            KMMHello* kmm = static_cast<KMMHello*>(frame.get());
            if (m_verbose) {
                LogMessage(LOG_P25, P25_KMM_STR ", %s, llId = %u, flag = $%02X", kmm->toString().c_str(),
                    llId, kmm->getFlag());
            }

            // respond with No-Service if KMF services are disabled
//            if (!m_network->m_kmfServicesEnabled)
                return write_KMM_NoService(llId, kmm->getSrcLLId(), payloadSize);
        }
        break;

        default:
        break;
    } // switch (frame->getMessageId())

    return nullptr;
}

/* Helper used to return a No-Service KMM to the calling SU. */

UInt8Array P25OTARService::write_KMM_NoService(uint32_t llId, uint32_t kmmRSI, uint32_t* payloadSize)
{
    if (payloadSize != nullptr)
        *payloadSize = KMM_NO_SERVICE_LENGTH;

    UInt8Array kmmFrame = std::make_unique<uint8_t[]>(KMM_NO_SERVICE_LENGTH);

    uint8_t buffer[KMM_NO_SERVICE_LENGTH];
    KMMNoService outKmm = KMMNoService();
    outKmm.setSrcLLId(WUID_FNE);
    outKmm.setDstLLId(kmmRSI);

    if (m_verbose) {
        LogMessage(LOG_P25, P25_KMM_STR ", %s, llId = %u, RSI = %u", outKmm.toString().c_str(),
            outKmm.getSrcLLId(), outKmm.getDstLLId());
    }

    outKmm.encode(buffer);

    ::memcpy(kmmFrame.get(), buffer, KMM_NO_SERVICE_LENGTH);
    return kmmFrame;
}
