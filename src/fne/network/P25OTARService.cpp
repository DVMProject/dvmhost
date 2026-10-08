// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025-2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/Defines.h"
#include "common/p25/kmm/KMMFactory.h"
#include "common/Log.h"
#include "common/Thread.h"
#include "common/Utils.h"
#include "network/TrafficNetwork.h"
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
#include <algorithm>

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

// Macro helper to verbose log a generic KMM.
#define VERBOSE_LOG_KMM(_PCKT_STR, __LLID)                                              \
    if (m_verbose) {                                                                    \
        LogInfoEx(LOG_P25, "KMM, %s, llId = %u", _PCKT_STR.c_str(), __LLID);            \
    }

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define MAX_THREAD_CNT 4U

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/**
 * @brief Determines if a KMM message type is supported.
 * @param messageId The KMM message type identifier.
 * @return true if the message type is supported, false otherwise.
 */
bool isSupportedKMM(uint8_t messageId)
{
    switch (messageId) {
    case KMM_MessageType::HELLO:
    case KMM_MessageType::INVENTORY_CMD:
    case KMM_MessageType::INVENTORY_RSP:
    case KMM_MessageType::MODIFY_KEY_CMD:
    case KMM_MessageType::NAK:
    case KMM_MessageType::NO_SERVICE:
    case KMM_MessageType::REKEY_ACK:
    case KMM_MessageType::REKEY_CMD:
    case KMM_MessageType::ZEROIZE_CMD:
    case KMM_MessageType::ZEROIZE_RSP:
    case KMM_MessageType::DEREG_CMD:
    case KMM_MessageType::DEREG_RSP:
    case KMM_MessageType::REG_CMD:
    case KMM_MessageType::REG_RSP:
    case KMM_MessageType::UNABLE_TO_DECRYPT:
        return true;
    default:
        return false;
    }
}

/**
 * @brief Determines if a KMM message type allows unauthenticated processing.
 * @param messageId The KMM message type identifier.
 * @return true if the message type allows unauthenticated processing, false otherwise.
 */
bool allowsUnauthenticatedKMM(uint8_t messageId)
{
    switch (messageId) {
    case KMM_MessageType::HELLO:
    case KMM_MessageType::NO_SERVICE:
    case KMM_MessageType::DEREG_CMD:
    case KMM_MessageType::DEREG_RSP:
    case KMM_MessageType::REG_CMD:
    case KMM_MessageType::REG_RSP:
    case KMM_MessageType::UNABLE_TO_DECRYPT:
        return true;
    default:
        return false;
    }
}

/**
 * @brief Determines if a KMM message type requires outer encryption.
 * @param messageId The KMM message type identifier.
 * @return true if the message type requires outer encryption, false otherwise.
 */
bool requiresEncryptedKMM(uint8_t messageId)
{
    switch (messageId) {
    case KMM_MessageType::HELLO:
    case KMM_MessageType::NO_SERVICE:
    case KMM_MessageType::DEREG_CMD:
    case KMM_MessageType::DEREG_RSP:
    case KMM_MessageType::REG_CMD:
    case KMM_MessageType::REG_RSP:
    case KMM_MessageType::UNABLE_TO_DECRYPT:
        return false;
    default:
        return true;
    }
}

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Represents an opaque KMM frame that only decodes the header.
 */
class KMMOpaqueFrame final : public KMMFrame {
public:
    /** 
     * @brief Decodes the KMM frame header from the provided data.
     * @param data The raw data containing the KMM frame.
     * @return true if the header was successfully decoded, false otherwise.
     */
    bool decode(const uint8_t* data) override { return decodeHeader(data); }
    /**
     * @brief Encodes the KMM frame into the provided buffer.
     * @param buffer The buffer to write the encoded KMM frame into.
     */
    void encode(uint8_t*) override { }
};

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the P25OTARService class. */

P25OTARService::P25OTARService(TrafficNetwork* network, P25PacketData* packetData, bool debug, bool verbose) :
    m_socket(nullptr),
    m_frameQueue(nullptr),
    m_threadPool(MAX_THREAD_CNT, "otar"),
    m_network(network),
    m_packetData(packetData),
    m_rsiMessageNumber(),
    m_rsiInboundMessageNumber(),
    m_rsiInboundFingerprint(),
    m_dliRegistered(),
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

/* Resolves the required outer-encryption context for a generated response. */

bool P25OTARService::resolveResponseSecurity(const uint8_t* data, uint32_t len, bool& encrypted,
    uint8_t& algoId, uint16_t& kid) const
{
    if (data == nullptr) {
        LogError(LOG_P25, P25_KMM_STR ", cannot resolve outer encryption for a null generated response");
        return false;
    }

    const uint8_t messageId = len > 0U ? data[0U] : KMM_MessageType::NULL_CMD;
    const uint32_t declaredLength = len >= 3U ?
        ((((uint32_t)data[1U] << 8U) | data[2U]) + 3U) : 0U;
    if (len < 10U || len > 512U || declaredLength != len) {
        LogError(LOG_P25, P25_KMM_STR ", cannot outer-encrypt generated KMM: invalid length, messageId = $%02X, declared/actual = %u/%u",
            messageId, declaredLength, len);
        return false;
    }

    if (encrypted || !requiresEncryptedKMM(messageId))
        return true;

    // This is locally generated and its total length has already been checked.
    // Decode only the common header: the bounded air-facing factory intentionally
    // rejects nested-count messages such as RK3 until it has a structural validator.
    KMMOpaqueFrame response;
    if (!response.decode(data) || response.getMACType() != KMM_MAC::ENH_MAC ||
        response.getMACAlgId() != ALGO_AES_256 || response.getMACKId() == 0U) {
        LogError(LOG_P25, P25_KMM_STR ", generated KMM requires outer encryption but has no usable enhanced-MAC TEK context, messageId = $%02X, macType = $%02X, macAlgId = $%02X, macKId = $%04X",
            messageId, response.getMACType(), response.getMACAlgId(), response.getMACKId());
        return false;
    }

    EKCKeyItem outerTek = m_network->m_cryptoLookup->find(response.getMACKId());
    if (outerTek.isInvalid()) {
        LogError(LOG_P25, P25_KMM_STR ", generated KMM requires outer encryption but MAC TEK was not found, messageId = $%02X, algId = $%02X, kId = $%04X",
            messageId, response.getMACAlgId(), response.getMACKId());
        return false;
    }
    if (outerTek.algId() != response.getMACAlgId()) {
        LogError(LOG_P25, P25_KMM_STR ", generated KMM outer TEK algorithm mismatch, messageId = $%02X, required/found algId = $%02X/$%02X, kId = $%04X",
            messageId, response.getMACAlgId(), outerTek.algId(), response.getMACKId());
        return false;
    }

    algoId = response.getMACAlgId();
    kid = response.getMACKId();
    encrypted = true;
    if (m_debug) {
        LogDebugEx(LOG_P25, "P25OTARService::resolveResponseSecurity()",
            "selected outer encryption for generated KMM, messageId = $%02X, algId = $%02X, kId = $%04X",
            messageId, algoId, kid);
    }
    return true;
}

/* Helper used to process KMM frames from PDU data. */

bool P25OTARService::processDLD(const uint8_t* data, uint32_t len, uint32_t llId, uint8_t n, bool encrypted,
    uint8_t algoId, uint16_t kid, const uint8_t* mi)
{
    auto sendNack = [&](uint8_t nackType) {
        m_packetData->write_PDU_Ack_Response(PDUAckClass::NACK, nackType, n, llId, false);
    };

    uint8_t resolvedAlgoId = algoId;
    uint16_t resolvedKId = kid;
    uint8_t resolvedMI[MI_LENGTH_BYTES];
    ::memset(resolvedMI, 0x00U, MI_LENGTH_BYTES);

    if (m_debug)
        Utils::dump(1U, "P25OTARService::processDLD(), KMM Network Message", data, len);

    UInt8Array kmmPayload = std::make_unique<uint8_t[]>(len);
    ::memset(kmmPayload.get(), 0x00U, len);
    ::memcpy(kmmPayload.get(), data, len);

    if (encrypted) {
        // prefer metadata provided from decoded Auxiliary ES header
        if (mi != nullptr) {
            ::memcpy(resolvedMI, mi, MI_LENGTH_BYTES);
        }
        else {
            // AACA-D 12.2 encrypted DLD requires the Auxiliary ES context.
            // Do not guess at a non-standard payload-embedded layout.
            LogError(LOG_P25, P25_KMM_STR ", encrypted DLD missing Auxiliary ES metadata");
            sendNack(PDUAckType::NACK_ILLEGAL);
            return false;
        }

        kmmPayload = cryptKMM(resolvedAlgoId, resolvedKId, resolvedMI, data, len, false);
        if (kmmPayload == nullptr) {
            LogError(LOG_P25, P25_KMM_STR ", unable to decrypt KMM, algoId = $%02X, kID = $%04X", resolvedAlgoId, resolvedKId);
            sendNack(PDUAckType::NACK_UNDELIVERABLE);
            return false;
        }
    }

    if (len < 10U || len > 512U || ((((uint32_t)kmmPayload[1U] << 8U) | kmmPayload[2U]) + 3U) != len) {
        LogWarning(LOG_P25, P25_KMM_STR ", invalid KMM length, len = %u", len);
        sendNack(PDUAckType::NACK_ILLEGAL);
        return false;
    }

    uint32_t payloadSize = 0U;
    // The payload is already decrypted. Preserve the outer encryption context so
    // processKMM can construct the encrypted/authenticated NACKs required by
    // AACA-D 6.22 and 7.4 without decrypting the request a second time.
    std::vector<std::vector<uint8_t>> additionalResponses;
    UInt8Array pduUserData = processKMM(kmmPayload.get(), len, llId, false, &payloadSize,
        encrypted ? resolvedAlgoId : ALGO_UNENCRYPT, encrypted ? resolvedKId : 0U,
        encrypted ? resolvedMI : nullptr, false, &additionalResponses);
    if (pduUserData == nullptr || payloadSize == 0U) {
        // no OTAR response is required for this message; acknowledge successful processing
        m_packetData->write_PDU_Ack_Response(PDUAckClass::ACK, PDUAckType::ACK, n, llId, false);
        return true;
    }

    bool responseEncrypted = encrypted;
    if (!resolveResponseSecurity(pduUserData.get(), payloadSize, responseEncrypted,
        resolvedAlgoId, resolvedKId)) {
        LogError(LOG_P25, P25_KMM_STR ", secured response has no usable outer TEK context");
        sendNack(PDUAckType::NACK_UNDELIVERABLE);
        return false;
    }

    // Complete the inbound confirmed-delivery transaction before starting the
    // independent outbound KMM transaction. The SU is still in stop-and-wait
    // for this N(R) and may discard an RK3 transmitted ahead of it.
    m_packetData->write_PDU_Ack_Response(PDUAckClass::ACK, PDUAckType::ACK, n, llId, false);

    // lambda function to handle dispatching of KMM responses, including encryption if required
    auto dispatchResponse = [&](const uint8_t* response, uint32_t responseLength) -> bool {
        uint8_t responseMI[MI_LENGTH_BYTES];
        ::memset(responseMI, 0x00U, sizeof(responseMI));
        const uint8_t* outgoing = response;

        UInt8Array encryptedResponse;
        if (responseEncrypted) {
            encryptedResponse = cryptKMM(resolvedAlgoId, resolvedKId, responseMI, response, responseLength, true);
            if (encryptedResponse == nullptr)
                return false;

            outgoing = encryptedResponse.get();
        }

        return m_packetData->write_PDU_KMM(outgoing, responseLength, llId, responseEncrypted,
            resolvedAlgoId, resolvedKId, responseEncrypted ? responseMI : nullptr);
    };

    // dispatch the initial KMM response
    if (!dispatchResponse(pduUserData.get(), payloadSize)) {
        if (responseEncrypted) {
            LogError(LOG_P25, P25_KMM_STR ", unable to encrypt or dispatch KMM response, algoId = $%02X, kID = $%04X", resolvedAlgoId, resolvedKId);
        }
        else {
            LogError(LOG_P25, P25_KMM_STR ", unable to dispatch KMM response, llId = %u", llId);
        }

        return false;
    }

    // dispatch any additional Rekey Command responses
    for (const std::vector<uint8_t>& response : additionalResponses) {
        if (!dispatchResponse(response.data(), (uint32_t)response.size())) {
            LogError(LOG_P25, P25_KMM_STR ", unable to dispatch additional Rekey Command, llId = %u", llId);
            return false;
        }
    }

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

        // AACA-D 12.3: Version-0 DLI preamble is Format(1), MFID(1),
        // ALGID(1), KID(2), MI(9) = 14 octets
        if (req->length >= 24 && (req->buffer[0U] & 0xFFU) == 0U && req->buffer[1U] == MFG_STANDARD) {
            uint8_t algoId = req->buffer[2U];
            uint16_t kid = GET_UINT16(req->buffer, 3U);

            uint8_t mi[MI_LENGTH_BYTES];
            ::memset(mi, 0x00U, MI_LENGTH_BYTES);
            for (uint8_t i = 0; i < MI_LENGTH_BYTES; i++) {
                mi[i] = req->buffer[5U + i];
            }

            // KMM frame
            const uint32_t kmmLength = req->length - 14U;
            if (kmmLength > 465U) {
                LogWarning(LOG_P25, P25_KMM_STR ", invalid DLI KMM length, len = %u", kmmLength);
                if (req->buffer != nullptr) delete[] req->buffer;
                delete req;
                return;
            }

            UInt8Array buffer = std::make_unique<uint8_t[]>(kmmLength);
            ::memset(buffer.get(), 0x00U, kmmLength);
            ::memcpy(buffer.get(), req->buffer + 14U, kmmLength);

            bool encrypted = (algoId != ALGO_UNENCRYPT);
            if (encrypted) {
                buffer = network->cryptKMM(algoId, kid, mi, buffer.get(), kmmLength, false);
                if (buffer == nullptr) {
                    LogError(LOG_P25, P25_KMM_STR ", unable to decrypt KMM, algoId = $%02X, kID = $%04X", algoId, kid);
                    if (req->buffer != nullptr)
                        delete[] req->buffer;
                    delete req;
                    return;
                }
            }

            uint32_t payloadSize = 0U;
            std::vector<std::vector<uint8_t>> additionalResponses;
            UInt8Array pduUserData = network->processKMM(buffer.get(), kmmLength, 0U, false, &payloadSize,
                encrypted ? algoId : ALGO_UNENCRYPT, encrypted ? kid : 0U, encrypted ? mi : nullptr, true, &additionalResponses);
            if (pduUserData == nullptr || payloadSize == 0U) {
                if (network->m_debug)
                    LogDebug(LOG_P25, P25_KMM_STR ", no KMM response generated for network request");

                if (req->buffer != nullptr)
                    delete[] req->buffer;
                delete req;
                return;
            }

            bool responseEncrypted = encrypted;
            if (!network->resolveResponseSecurity(pduUserData.get(), payloadSize,
                responseEncrypted, algoId, kid)) {
                LogError(LOG_P25, P25_KMM_STR ", secured DLI response has no usable outer TEK context");
                if (req->buffer != nullptr) delete[] req->buffer;
                delete req;
                return;
            }

            // lambda function to send KMM responses, handling encryption if necessary
            auto sendResponse = [&](const uint8_t* response, uint32_t responseLength) -> bool {
                uint8_t responseMI[MI_LENGTH_BYTES] = { 0U };
                const uint8_t* outgoing = response;
                UInt8Array encryptedResponse;
                if (responseEncrypted) {
                    encryptedResponse = network->cryptKMM(algoId, kid, responseMI, response, responseLength, true);
                    if (encryptedResponse == nullptr)
                        return false;
                    outgoing = encryptedResponse.get();
                }

                UInt8Array datagram = std::make_unique<uint8_t[]>(responseLength + 14U);
                ::memset(datagram.get(), 0x00U, responseLength + 14U);

                datagram[0U] = 0U;
                datagram[1U] = MFG_STANDARD;
                datagram[2U] = responseEncrypted ? algoId : ALGO_UNENCRYPT;
                SET_UINT16(responseEncrypted ? kid : 0U, datagram.get(), 3U);

                if (responseEncrypted)
                    ::memcpy(datagram.get() + 5U, responseMI, MI_LENGTH_BYTES);

                ::memcpy(datagram.get() + 14U, outgoing, responseLength);

                return network->m_frameQueue->write(datagram.get(), responseLength + 14U,
                    req->address, req->addrLen);
            };

            // send the initial DLI KMM response
            if (!sendResponse(pduUserData.get(), payloadSize)) {
                LogError(LOG_P25, P25_KMM_STR ", unable to send DLI KMM response");
            }
            else {
                for (const std::vector<uint8_t>& response : additionalResponses) {
                    if (!sendResponse(response.data(), (uint32_t)response.size())) {
                        LogError(LOG_P25, P25_KMM_STR ", unable to send additional DLI Rekey Command");
                        break;
                    }
                }
            }
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

    // AACA-D 12.4: a DLD KMM is at most 512 octets including the three
    // octets excluded from Message Length
    if (len < 10U || len > 512U) {
        LogError(LOG_P25, P25_KMM_STR ", invalid DLD KMM length, len = %u", len);
        return nullptr;
    }

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
    // find() searches the existing EKC <Keys> collection only. UKEKs are kept
    // in the separate <UKEKs> collection and are available only via findUKEK().
    if (!keyItem.isInvalid()) {
        uint8_t key[P25DEF::MAX_ENC_KEY_LENGTH_BYTES];
        ::memset(key, 0x00U, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
        uint8_t keyLength = keyItem.getKey(key);

        if (m_network->m_debug)
            LogDebugEx(LOG_P25, "P25OTARService::cryptKMM()", "keyLength = %u", keyLength);

        LogInfoEx(LOG_P25, P25_KMM_STR ", algId = $%02X, kID = $%04X", algoId, kid);
        crypto.setTEKAlgoId(algoId);
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
    else {
        LogError(LOG_P25, P25_KMM_STR ", unable to %s outer KMM, KEK not found, algId = $%02X, kId = $%04X",
            encrypt ? "encrypt" : "decrypt", algoId, kid);
    }

    return nullptr;
}

/* Helper used to process KMM frames. */

UInt8Array P25OTARService::processKMM(const uint8_t* data, uint32_t len, uint32_t llId, bool encrypted, uint32_t* payloadSize,
    uint8_t algoId, uint16_t kid, const uint8_t* mi, bool dataLinkIndependent, std::vector<std::vector<uint8_t>>* additionalResponses)
{
    if (payloadSize != nullptr)
        *payloadSize = 0U;

    if (data == nullptr || len < 10U || len > 512U ||
        ((((uint32_t)data[1U] << 8U) | data[2U]) + 3U) != len) {
        LogWarning(LOG_P25, P25_KMM_STR ", invalid KMM framing length, len = %u", len);
        return nullptr;
    }

    UInt8Array buffer = std::make_unique<uint8_t[]>(len);
    ::memset(buffer.get(), 0x00U, len);
    ::memcpy(buffer.get(), data, len);

    // handle DLD encrypted KMM frame
    if (encrypted) {
        uint8_t resolvedAlgoId = algoId;
        uint16_t resolvedKId = kid;

        uint8_t resolvedMI[MI_LENGTH_BYTES];
        ::memset(resolvedMI, 0x00U, MI_LENGTH_BYTES);

        // prefer metadata provided from decoded Auxiliary ES header
        if (mi != nullptr) {
            ::memcpy(resolvedMI, mi, MI_LENGTH_BYTES);
            buffer = cryptKMM(algoId, kid, resolvedMI, data, len, false);
        }
        else
            return nullptr;

        if (buffer == nullptr) {
            LogError(LOG_P25, P25_KMM_STR ", unable to decrypt KMM, algoId = $%02X, kID = $%04X", resolvedAlgoId, resolvedKId);
            return nullptr;
        }

        if (m_debug)
            Utils::dump(1U, "P25OTARService::processKMM(), (Decrypted) KMM Network Message", buffer.get(), len);
    }

    std::unique_ptr<KMMFrame> frame = KMMFactory::create(buffer.get(), len);
    if (frame == nullptr) {
        LogWarning(LOG_P25, P25_KMM_STR ", undecodable KMM packet");

        // a structurally valid but unsupported Message ID is the one factory
        // failure for which AACA-D 7.4 requires an RK3 KMM NACK -- decode only the
        // common header, then authenticate it before reflecting any fields
        const uint32_t declaredLength = len >= 3U ? ((((uint32_t)buffer[1U] << 8U) | buffer[2U]) + 3U) : 0U;
        const uint8_t mnCode = len >= 4U ? ((buffer[3U] >> 4U) & 0x03U) : 0xFFU;
        const uint8_t macType = len >= 4U ? ((buffer[3U] >> 2U) & 0x03U) : 0xFFU;
        const uint32_t minimumAuthenticatedLength = 10U + (mnCode == 2U ? 2U : 0U) + P25DEF::KMM_AES_MAC_LENGTH + 5U;
        if (len >= 1U && !isSupportedKMM(buffer[0U]) && declaredLength >= minimumAuthenticatedLength && declaredLength <= len &&
            (mnCode == 0U || mnCode == 2U) && macType == KMM_MAC::ENH_MAC) {
            KMMOpaqueFrame opaque;
            opaque.decode(buffer.get());

            if (opaque.getDstLLId() == WUID_FNE && opaque.getSrcLLId() != 0U && (llId == 0U || opaque.getSrcLLId() == llId) &&
                opaque.getResponseKind() == KMM_ResponseKind::IMMEDIATE && opaque.getMACType() == KMM_MAC::ENH_MAC && opaque.getMACAlgId() == ALGO_AES_256 &&
                (opaque.getMACFormat() == KMM_MAC_FORMAT_CBC || opaque.getMACFormat() == KMM_MAC_FORMAT_CMAC)) {
                EKCKeyItem key = m_network->m_cryptoLookup->find(opaque.getMACKId());
                if (!key.isInvalid() && key.algId() == opaque.getMACAlgId()) {
                    uint8_t tek[P25DEF::MAX_ENC_KEY_LENGTH_BYTES] = { 0U };
                    key.getKey(tek);

                    if (opaque.verifyMAC(tek, buffer.get(), len) && algoId != ALGO_UNENCRYPT) {
                        KMMAuthContext nackAuth;
                        nackAuth.authenticated = true;
                        nackAuth.hasMessageNumber = opaque.getHasMessageNumber();
                        nackAuth.messageNumber = opaque.getMessageNumber();
                        nackAuth.algorithmId = opaque.getMACAlgId();
                        nackAuth.keyId = opaque.getMACKId();
                        nackAuth.format = opaque.getMACFormat();

                        return write_KMM_NegativeAck(opaque.getSrcLLId(), opaque.getMessageId(), opaque.getHasMessageNumber() ? opaque.getMessageNumber() : 0U, 
                            KMM_Status::INVALID_MSG_ID, payloadSize, nackAuth);
                    }
                }
            }
        }
        return nullptr;
    }

    // AACA-D 7.4: validate addressing, MN/MAC coupling, authentication and
    // anti-replay state before executing message-specific behavior
    if (frame->getDstLLId() != WUID_FNE || frame->getSrcLLId() == 0U ||
        (llId != 0U && frame->getSrcLLId() != llId)) {
        LogWarning(LOG_P25, P25_KMM_STR ", invalid source/destination RSI, llid = %u, srcLlId = %u, dstLlId = %u", llId, frame->getSrcLLId(), frame->getDstLLId());
        return nullptr;
    }

    const bool outerEncrypted = algoId != ALGO_UNENCRYPT;
    if (requiresEncryptedKMM(frame->getMessageId()) && !outerEncrypted) {
        LogWarning(LOG_P25, P25_KMM_STR ", encrypted KMM required, messageId = $%02X, RSI = %u",
            frame->getMessageId(), frame->getSrcLLId());

        return nullptr;
    }

    // initialize the authentication context based on the frame's MAC information
    KMMAuthContext auth;
    auth.hasMessageNumber = frame->getHasMessageNumber();
    auth.messageNumber = frame->getMessageNumber();
    auth.algorithmId = frame->getMACAlgId();
    auth.keyId = frame->getMACKId();
    auth.format = frame->getMACFormat();

    // AACA-D Table 79 requires DLI registration -- registration itself and a
    // repeated deregistration are allowed before registered service exists
    if (dataLinkIndependent && frame->getMessageId() != KMM_MessageType::REG_CMD &&
        frame->getMessageId() != KMM_MessageType::DEREG_CMD &&
        m_dliRegistered.find(frame->getSrcLLId()) == m_dliRegistered.end()) {
        LogWarning(LOG_P25, P25_KMM_STR ", rejecting unregistered DLI RSI = %u", frame->getSrcLLId());
        if (frame->getResponseKind() == KMM_ResponseKind::IMMEDIATE)
            return write_KMM_NoService(llId, frame->getSrcLLId(), payloadSize, auth);

        return nullptr;
    }

    // lambda function to create a negative acknowledgment (NACK) for the KMM frame
    auto makeNack = [&](uint8_t status) -> UInt8Array {
        // table 55 requires a NACK to be encrypted and authenticated - prefer
        // the request MAC TEK; when it is unavailable, the outer AES TEK is the
        // only established common authentication key available to this service
        if (!auth.authenticated && algoId == ALGO_AES_256 && kid != 0U) {
            EKCKeyItem outerTek = m_network->m_cryptoLookup->find(kid);
            if (!outerTek.isInvalid() && outerTek.algId() == ALGO_AES_256) {
                auth.authenticated = true;
                auth.algorithmId = ALGO_AES_256;
                auth.keyId = kid;
                auth.format = KMM_MAC_FORMAT_CBC;
            }
        }

        if (frame->getResponseKind() != KMM_ResponseKind::IMMEDIATE ||
            algoId == ALGO_UNENCRYPT || !auth.authenticated)
            return nullptr;

        return write_KMM_NegativeAck(frame->getSrcLLId(), frame->getMessageId(),
            frame->getHasMessageNumber() ? frame->getMessageNumber() : 0U,
            status, payloadSize, auth);
    };

    // check if a MAC is required and present for the KMM frame
    if (frame->getMACType() == KMM_MAC::NO_MAC && (frame->getHasMessageNumber() || !allowsUnauthenticatedKMM(frame->getMessageId()))) {
        LogWarning(LOG_P25, P25_KMM_STR ", MAC required for message type/MN, RSI = %u", frame->getSrcLLId());
        return makeNack(KMM_Status::INVALID_MSG_NUMBER);
    }

    // verify the MAC if it is present
    if (frame->getMACType() != KMM_MAC::NO_MAC) {
        if (frame->getMACType() != KMM_MAC::ENH_MAC || frame->getMACAlgId() != ALGO_AES_256 ||
            (frame->getMACFormat() != KMM_MAC_FORMAT_CBC && frame->getMACFormat() != KMM_MAC_FORMAT_CMAC)) {
            LogWarning(LOG_P25, P25_KMM_STR ", unsupported MAC type/algorithm, RSI = %u", frame->getSrcLLId());
            return nullptr;
        }

        EKCKeyItem macTek = m_network->m_cryptoLookup->find(frame->getMACKId());
        if (macTek.isInvalid() || macTek.algId() != frame->getMACAlgId()) {
            LogWarning(LOG_P25, P25_KMM_STR ", MAC TEK not found, RSI = %u", frame->getSrcLLId());
            return makeNack(KMM_Status::ITEM_NOT_EXIST);
        }

        auth.authenticated = true;

        uint8_t tek[P25DEF::MAX_ENC_KEY_LENGTH_BYTES];
        ::memset(tek, 0x00U, sizeof(tek));
        macTek.getKey(tek);

        if (!frame->verifyMAC(tek, buffer.get(), len)) {
            LogWarning(LOG_P25, P25_KMM_STR ", invalid MAC, RSI = %u", frame->getSrcLLId());
            return makeNack(KMM_Status::INVALID_MAC);
        }
    }

    // validate the inbound message number (MN) and maintain a fingerprint for replay protection
    if (frame->getHasMessageNumber()) {
        const uint32_t rsi = frame->getSrcLLId();
        const uint16_t received = frame->getMessageNumber();

        uint64_t fingerprint = 1469598103934665603ULL;
        const uint32_t declared = (((uint32_t)buffer[1U] << 8U) | buffer[2U]) + 3U;
        for (uint32_t i = 0U; i < declared; ++i) {
            fingerprint ^= buffer[i];
            fingerprint *= 1099511628211ULL;
        }

        auto lastIt = m_rsiInboundMessageNumber.find(rsi);
        if (lastIt != m_rsiInboundMessageNumber.end()) {
            const uint16_t last = lastIt->second;
            const uint16_t distance = (uint16_t)(received - last);
            const bool identicalRetry = distance == 0U &&
                frame->getResponseKind() == KMM_ResponseKind::IMMEDIATE &&
                m_rsiInboundFingerprint.find(rsi) != m_rsiInboundFingerprint.end() &&
                m_rsiInboundFingerprint[rsi] == fingerprint;

            if (!identicalRetry && (distance == 0U || distance >= 1680U)) {
                LogWarning(LOG_P25, P25_KMM_STR ", invalid/replayed message number, RSI = %u, MN = %u", rsi, received);
                return makeNack(KMM_Status::INVALID_MSG_NUMBER);
            }
        }

        m_rsiInboundMessageNumber[rsi] = received;
        m_rsiInboundFingerprint[rsi] = fingerprint;
    }

    if (llId == 0U) {
        llId = frame->getSrcLLId();
    }

    // maintain outbound state independently from the validated inbound MN
    if (m_rsiMessageNumber.find(llId) == m_rsiMessageNumber.end())
        m_rsiMessageNumber[llId] = 0U;

    // handle the KMM message based on its type
    switch (frame->getMessageId()) {
        case KMM_MessageType::HELLO:
        {
            KMMHello* kmm = static_cast<KMMHello*>(frame.get());
            uint8_t respKind = kmm->getResponseKind();
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

            // ignore Response Kind 2 command requests initiated from a SU
            if (respKind == KMM_ResponseKind::DELAYED) {
                LogWarning(LOG_P25, P25_KMM_STR ", %s, discarding SU initiated Response Kind 2 command, llId = %u", kmm->toString().c_str(), llId);
                return nullptr;
            }

            // response Kind 1 requests no OTAR response message
            if (respKind == KMM_ResponseKind::NONE) {
                if (m_verbose) {
                    LogInfoEx(LOG_P25, P25_KMM_STR ", %s, Response Kind 1 request, no OTAR response sent, llId = %u", kmm->toString().c_str(), llId);
                }
                return nullptr;
            }

            // respond with No-Service if KMF services are disabled
            if (!m_network->m_kmfServicesEnabled)
                return write_KMM_NoService(llId, kmm->getSrcLLId(), payloadSize, auth);
            else {
                if (kmm->getFlag() == KMM_HelloFlag::REKEY_REQUEST_UKEK ||
                    (kmm->getFlag() == KMM_HelloFlag::REKEY_REQUEST_NO_UKEK && m_allowNoUKEKRekey)) {
                    lookups::RadioId ridEntry = m_network->m_ridLookup->find(kmm->getSrcLLId());
                    if (ridEntry.radioDefault()) {
                        LogInfoEx(LOG_P25, P25_KMM_STR ", %s, rekey denied; RID %u has no key policy entry", kmm->toString().c_str(), kmm->getSrcLLId());
                        return write_KMM_NoService(llId, kmm->getSrcLLId(), payloadSize, auth);
                    }

                    if (!ridEntry.radioEnabled()) {
                        LogInfoEx(LOG_P25, P25_KMM_STR ", %s, rekey denied; RID %u disabled", kmm->toString().c_str(), kmm->getSrcLLId());
                        return write_KMM_NoService(llId, kmm->getSrcLLId(), payloadSize, auth);
                    }

                    if (!ridEntry.canRekey()) {
                        LogInfoEx(LOG_P25, P25_KMM_STR ", %s, rekey denied; RID %u not rekeyable", kmm->toString().c_str(), kmm->getSrcLLId());
                        return write_KMM_NoService(llId, kmm->getSrcLLId(), payloadSize, auth);
                    }

                    // send rekey-command
                    EKCKeyItem keyItem = m_network->m_cryptoLookup->findUKEK(llId);
                    if (keyItem.isInvalid()) {
                        LogInfoEx(LOG_P25, P25_KMM_STR ", %s, no UKEK found for rekey request, llId = %u", kmm->toString().c_str(), llId);
                        return write_KMM_NoService(llId, kmm->getSrcLLId(), payloadSize, auth);
                    } else {
                        return write_KMM_Rekey_Command(llId, kmm->getSrcLLId(), kmm->getFlag(), payloadSize, auth, additionalResponses);
                    }
                } else {
                    LogInfoEx(LOG_P25, P25_KMM_STR ", %s, rekey request denied, llId = %u", kmm->toString().c_str(), llId);
                    return write_KMM_NoService(llId, kmm->getSrcLLId(), payloadSize, auth);
                }
            }
        }
        break;

        case KMM_MessageType::NAK:
        {
            KMMNegativeAck* kmm = static_cast<KMMNegativeAck*>(frame.get());
            LogInfoEx(LOG_P25, P25_KMM_STR ", %s, llId = %u, nackMessageId = $%02X, messageNo = %u, status = $%02X", kmm->toString().c_str(),
                llId, kmm->getNakMessageId(), kmm->getMessageNumber(), kmm->getStatus());
            logResponseStatus(llId, kmm->toString(), kmm->getStatus());
        }
        break;

        case KMM_MessageType::REKEY_ACK:
        {
            KMMRekeyAck* kmm = static_cast<KMMRekeyAck*>(frame.get());
            LogInfoEx(LOG_P25, P25_KMM_STR ", %s, llId = %u, ackMessageId = $%02X, numOfStatus = %u", kmm->toString().c_str(),
                llId, kmm->getAckMessageId(), kmm->getNumberOfKeyStatus());

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
            uint8_t respKind = kmm->getResponseKind();
            LogInfoEx(LOG_P25, P25_KMM_STR ", %s, llId = %u", kmm->toString().c_str(),
                llId);

            // ignore Response Kind 2 command requests initiated from a SU
            if (respKind == KMM_ResponseKind::DELAYED) {
                LogWarning(LOG_P25, P25_KMM_STR ", %s, discarding SU initiated Response Kind 2 command, llId = %u", kmm->toString().c_str(), llId);
                return nullptr;
            }

            // response Kind 1 requests no OTAR response message
            if (respKind == KMM_ResponseKind::NONE) {
                if (m_verbose) {
                    LogInfoEx(LOG_P25, P25_KMM_STR ", %s, Response Kind 1 request, no OTAR response sent, llId = %u", kmm->toString().c_str(), llId);
                }
                return nullptr;
            }

            // respond with No-Service if KMF services are disabled
            if (!m_network->m_kmfServicesEnabled)
                return write_KMM_NoService(llId, kmm->getSrcLLId(), payloadSize, auth);
            else {
                m_dliRegistered.erase(kmm->getSrcLLId());
                return write_KMM_Dereg_Response(llId, kmm->getSrcLLId(), payloadSize, auth);
            }
        }
        break;
        case KMM_MessageType::REG_CMD:
        {
            KMMRegistrationCommand* kmm = static_cast<KMMRegistrationCommand*>(frame.get());
            if (kmm->getKMFRSI() != WUID_FNE || !m_network->m_kmfServicesEnabled)
                return write_KMM_NoService(llId, kmm->getSrcLLId(), payloadSize, auth);

            m_dliRegistered[kmm->getSrcLLId()] = true;
            KMMRegistrationResponse response;
            response.setSrcLLId(WUID_FNE);
            response.setDstLLId(kmm->getSrcLLId());
            response.setResponseKind(KMM_ResponseKind::NONE);
            response.setStatus(KMM_Status::CMD_PERFORMED);
            return encode_KMM_Response(response, payloadSize, auth);
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
        break;

        default:
        {
            LogWarning(LOG_P25, P25_KMM_STR ", unsupported inbound messageId = $%02X, RSI = %u", frame->getMessageId(), frame->getSrcLLId());
            return makeNack(KMM_Status::INVALID_MSG_ID);
        }
        break;
    } // switch (frame->getMessageId())

    return nullptr;
}

/* Helper used to return a Rekey-Command KMM to the calling SU. */

UInt8Array P25OTARService::write_KMM_Rekey_Command(uint32_t llId, uint32_t kmmRSI, uint8_t flags, uint32_t* payloadSize,
    const KMMAuthContext& auth, std::vector<std::vector<uint8_t>>* additionalResponses)
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

    // Attempt to find the UKEK (User Key Encryption Key) for the given RSI (Rekey Sequence Identifier)
    ::EKCKeyItem keyItem = m_network->m_cryptoLookup->findUKEK(kmmRSI);
    if (!keyItem.isInvalid()) {
        uint8_t keyLength = keyItem.getKey(kekKey);

        kekAlgId = keyItem.algId();
        kekKId = keyItem.kId();

        if (m_network->m_debug)
            LogDebugEx(LOG_P25, "P25OTARService::cryptKMM()", "keyLength = %u", keyLength);
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
    outKmm.setMACFormat(auth.authenticated ? auth.format : KMM_MAC_FORMAT_CBC);
    outKmm.setHasMessageNumber(true);
    outKmm.setMessageNumber(mn);

    outKmm.setAlgId(kekAlgId);
    outKmm.setKId(kekKId);

    lookups::RadioId ridEntry = m_network->m_ridLookup->find(kmmRSI);
    std::vector<uint16_t> allowedKIds;
    if (ridEntry.radioDefault()) {
        LogWarning(LOG_P25, P25_KMM_STR ", %s, aborting rekey, RID %u has no key policy entry", outKmm.toString().c_str(), kmmRSI);
        return nullptr;
    }

    allowedKIds = ridEntry.allowedKIds();

    // AACA-D 13.5: the MAC key is a TEK, never the UKEK used for inner
    // key wrap; select an AES TEK already authorized for this subscriber
    EKCKeyItem macTekItem;
    if (auth.authenticated) {
        macTekItem = m_network->m_cryptoLookup->find(auth.keyId);
        if (macTekItem.isInvalid() || macTekItem.algId() != auth.algorithmId ||
            (!allowedKIds.empty() && std::find(allowedKIds.begin(), allowedKIds.end(), auth.keyId) == allowedKIds.end())) {
            LogError(LOG_P25, P25_KMM_STR ", %s, authenticated request MAC TEK is not authorized for response, RSI = %u",
                outKmm.toString().c_str(), kmmRSI);
            return nullptr;
        }
    } else {
        for (const EKCKeyItem& candidate : m_network->m_cryptoLookup->keys()) {
            if (candidate.algId() == ALGO_AES_256 &&
                (allowedKIds.empty() || std::find(allowedKIds.begin(), allowedKIds.end(),
                    (uint16_t)candidate.kId()) != allowedKIds.end())) {
                macTekItem = candidate;
                break;
            }
        }
    }

    // ensure that a valid MAC TEK (Traffic Encryption Key) has been selected for the KMM response
    if (macTekItem.isInvalid()) {
        LogError(LOG_P25, P25_KMM_STR ", %s, no common AES TEK available for MAC, RSI = %u",
            outKmm.toString().c_str(), kmmRSI);
        return nullptr;
    }

    uint8_t macTek[P25DEF::MAX_ENC_KEY_LENGTH_BYTES];
    ::memset(macTek, 0x00U, sizeof(macTek));
    macTekItem.getKey(macTek);
    outKmm.setMACAlgId(macTekItem.algId());
    outKmm.setMACKId((uint16_t)macTekItem.kId());

    KeysetItem ks;
    ks.keysetId(1U);
    ks.algId(ALGO_AES_256); // we currently can only OTAR AES256 keys
    if (kekAlgId != P25DEF::ALGO_UNENCRYPT)
        ks.keyLength(P25DEF::MAX_WRAPPED_ENC_KEY_LENGTH_BYTES);
    else
        ks.keyLength(P25DEF::MAX_ENC_KEY_LENGTH_BYTES);

    // iterate through all available AES-256 keys and prepare them for inclusion in the KMM response
    for (EKCKeyItem keyItem : m_network->m_cryptoLookup->keys()) {
        if (keyItem.algId() != ALGO_AES_256) {
            LogWarning(LOG_P25, P25_KMM_STR", %s, ignoring kId = %u, is not an AES-256 key, llId = %u, RSI = %u", outKmm.toString().c_str(),
                keyItem.kId(), outKmm.getSrcLLId(), outKmm.getDstLLId());
            continue;
        }

        // check if this keyItem is allowed for the RID
        if (!allowedKIds.empty() && std::find(allowedKIds.begin(), allowedKIds.end(), (uint16_t)keyItem.kId()) == allowedKIds.end()) {
            if (m_verbose) {
                LogInfoEx(LOG_P25, P25_KMM_STR ", %s, skipping kId = %u; not allowed for RID %u", outKmm.toString().c_str(),
                    keyItem.kId(), kmmRSI);
            }
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
        ki.keyFormat(KMM_KEY_FORMAT_TEK);
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

    // AACA-D permits a multi-KMM response and defines the D bit as
    // "more to follow". Emit independently authenticated RK3 Rekey Commands,
    // each with its own monotonically increasing MN and no more than four
    // wrapped keys. Each command is answered by its own Rekey-Acknowledgment.
    static constexpr size_t MAX_KEYS_PER_REKEY = 4U;
    static constexpr uint32_t MAX_NATIVE_ENCRYPTED_KMM_LENGTH = (19U * P25_PDU_CONFIRMED_DATA_LENGTH_BYTES) - 13U - 4U;
    const std::vector<p25::kmm::KeyItem> keys = ks.keys();
    const size_t batchCount = (keys.size() + MAX_KEYS_PER_REKEY - 1U) / MAX_KEYS_PER_REKEY;
    UInt8Array firstFrame;

    // prepare to batch keys into multiple RK3 Rekey Commands if necessary
    if (additionalResponses != nullptr)
        additionalResponses->clear();

    // iterate over each batch of keys and construct the corresponding RK3 Rekey Command
    for (size_t batchIndex = 0U; batchIndex < batchCount; batchIndex++) {
        KeysetItem batchKeyset;
        batchKeyset.keysetId(ks.keysetId());
        batchKeyset.algId(ks.algId());
        batchKeyset.keyLength(ks.keyLength());

        const size_t begin = batchIndex * MAX_KEYS_PER_REKEY;
        const size_t end = std::min(begin + MAX_KEYS_PER_REKEY, keys.size());
        for (size_t i = begin; i < end; i++)
            batchKeyset.push_back(keys[i]);

        // KMMRekeyCommand owns its MI buffer and is intentionally built per
        // packet here -- do not copy outKmm: the KMM classes are not
        // safely copyable, and every RK3 is an independent authenticated KMM
        KMMRekeyCommand batch;
        batch.setDecryptInfoFmt(KMM_DECRYPT_INSTRUCT_NONE);
        batch.setSrcLLId(WUID_FNE);
        batch.setDstLLId(kmmRSI);
        batch.setMACType(KMM_MAC::ENH_MAC);
        batch.setMACFormat(auth.authenticated ? auth.format : KMM_MAC_FORMAT_CBC);
        batch.setHasMessageNumber(true);
        batch.setAlgId(kekAlgId);
        batch.setKId(kekKId);
        batch.setMACAlgId(macTekItem.algId());
        batch.setMACKId((uint16_t)macTekItem.kId());
        batch.setKeysets(std::vector<KeysetItem>{batchKeyset});
        batch.setMessageNumber((uint16_t)(mn + batchIndex));
        batch.setComplete((batchIndex + 1U) == batchCount);

        // check if the frame length exceeds the maximum allowed native encrypted KMM length
        const uint32_t frameLength = batch.fullLength();
        if (frameLength > MAX_NATIVE_ENCRYPTED_KMM_LENGTH) {
            LogError(LOG_P25, P25_KMM_STR ", %s, four-key rekey batch exceeds native PDU transport, len/max = %u/%u, llId = %u, RSI = %u",
                batch.toString().c_str(), frameLength, MAX_NATIVE_ENCRYPTED_KMM_LENGTH,
                batch.getSrcLLId(), batch.getDstLLId());
            return write_KMM_NoService(llId, kmmRSI, payloadSize, auth);
        }

        std::vector<uint8_t> encoded(frameLength, 0x00U);
        batch.encode(encoded.data());
        batch.generateMAC(macTek, encoded.data());

        if (m_verbose) {
            LogInfoEx(LOG_P25, P25_KMM_STR ", %s, llId = %u, RSI = %u, batch = %u/%u, keyCount = %u, moreToFollow = %u",
                batch.toString().c_str(), batch.getSrcLLId(), batch.getDstLLId(), uint32_t(batchIndex + 1U),
                uint32_t(batchCount), uint32_t(end - begin), batch.getComplete() ? 0U : 1U);
        }

        if (batchIndex == 0U) {
            if (payloadSize != nullptr)
                *payloadSize = frameLength;
            firstFrame = std::make_unique<uint8_t[]>(frameLength);
            ::memcpy(firstFrame.get(), encoded.data(), frameLength);
        }
        else if (additionalResponses != nullptr) {
            additionalResponses->push_back(std::move(encoded));
        }
    }

    m_rsiMessageNumber[llId] = (uint16_t)(mn + batchCount);
    return firstFrame;
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

    const uint32_t frameLength = outKmm.fullLength();
    if (payloadSize != nullptr)
        *payloadSize = frameLength;

    UInt8Array kmmFrame = std::make_unique<uint8_t[]>(frameLength);
    outKmm.encode(kmmFrame.get());
    return kmmFrame;
}

/* Helper used to return a Deregistration-Response KMM to the calling SU. */

UInt8Array P25OTARService::write_KMM_Dereg_Response(uint32_t llId, uint32_t kmmRSI, uint32_t* payloadSize,
    const KMMAuthContext& auth)
{
    KMMDeregistrationResponse outKmm = KMMDeregistrationResponse();
    outKmm.setSrcLLId(WUID_FNE);
    outKmm.setDstLLId(kmmRSI);
    outKmm.setStatus(KMM_Status::CMD_PERFORMED);

    if (m_verbose) {
        LogInfoEx(LOG_P25, P25_KMM_STR ", %s, llId = %u, RSI = %u", outKmm.toString().c_str(),
            outKmm.getSrcLLId(), outKmm.getDstLLId());
    }

    return encode_KMM_Response(outKmm, payloadSize, auth);
}

/* Helper used to return a No-Service KMM to the calling SU. */

UInt8Array P25OTARService::write_KMM_NoService(uint32_t llId, uint32_t kmmRSI, uint32_t* payloadSize,
    const KMMAuthContext& auth)
{
    KMMNoService outKmm = KMMNoService();
    outKmm.setSrcLLId(WUID_FNE);
    outKmm.setDstLLId(kmmRSI);

    if (m_verbose) {
        LogInfoEx(LOG_P25, P25_KMM_STR ", %s, llId = %u, RSI = %u", outKmm.toString().c_str(),
            outKmm.getSrcLLId(), outKmm.getDstLLId());
    }

    return encode_KMM_Response(outKmm, payloadSize, auth);
}

/* Helper used to return a secured Negative-Acknowledgment KMM. */

UInt8Array P25OTARService::write_KMM_NegativeAck(uint32_t kmmRSI, uint8_t messageId,
    uint16_t messageNumber, uint8_t status, uint32_t* payloadSize, const KMMAuthContext& auth)
{
    if (!auth.authenticated)
        return nullptr;

    KMMNegativeAck outKmm;
    outKmm.setSrcLLId(WUID_FNE);
    outKmm.setDstLLId(kmmRSI);
    outKmm.setNakMessageId(messageId);
    outKmm.setMessageNumber(messageNumber);
    outKmm.setStatus(status);

    LogWarning(LOG_P25, P25_KMM_STR ", sending NACK, RSI = %u, messageId = $%02X, MN = %u, status = $%02X",
        kmmRSI, messageId, messageNumber, status);

    return encode_KMM_Response(outKmm, payloadSize, auth);
}

/* Encodes a response using the authenticated request's MAC format and MN. */

UInt8Array P25OTARService::encode_KMM_Response(KMMFrame& frame, uint32_t* payloadSize,
    const KMMAuthContext& auth)
{
    uint8_t macTek[P25DEF::MAX_ENC_KEY_LENGTH_BYTES];
    ::memset(macTek, 0x00U, sizeof(macTek));

    if (auth.authenticated) {
        EKCKeyItem key = m_network->m_cryptoLookup->find(auth.keyId);
        if (key.isInvalid() || key.algId() != auth.algorithmId)
            return nullptr;

        key.getKey(macTek);
        frame.setMACType(KMM_MAC::ENH_MAC);
        frame.setMACAlgId(auth.algorithmId);
        frame.setMACKId(auth.keyId);
        frame.setMACFormat(auth.format);
        if (auth.hasMessageNumber) {
            frame.setHasMessageNumber(true);
            frame.setMessageNumber(auth.messageNumber);
        }
    }

    const uint32_t frameLength = frame.fullLength();
    if (payloadSize != nullptr)
        *payloadSize = frameLength;

    UInt8Array encoded = std::make_unique<uint8_t[]>(frameLength);
    ::memset(encoded.get(), 0x00U, frameLength);
    frame.encode(encoded.get());
    if (auth.authenticated)
        frame.generateMAC(macTek, encoded.get());

    return encoded;
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

    const uint32_t frameLength = outKmm.fullLength();
    if (payloadSize != nullptr)
        *payloadSize = frameLength;

    UInt8Array kmmFrame = std::make_unique<uint8_t[]>(frameLength);
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
