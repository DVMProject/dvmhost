// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/FNETestHooks.h"
#include "fne/HostFNE.h"
#include "fne/network/P25OTARService.h"
#include "common/p25/data/Assembler.h"
#include "common/Utils.h"

#include <cstring>
#include <chrono>
#include <stdexcept>

using namespace network;
using namespace p25::defines;
using namespace p25::data;

// ---------------------------------------------------------------------------
//  Global Variables
// ---------------------------------------------------------------------------

bool g_promiscuousHub = false;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/**
 * @brief Initializes an instance of the HostFNE class.
 */
HostFNE::HostFNE(const std::string& confFile) :
    m_confFile(confFile), m_conf(), m_network(nullptr), m_mdNetwork(nullptr),
    m_vtunEnabled(false), m_packetDataMode(PacketDataMode::PROJECT25),
#if !defined(_WIN32)
    m_tun(nullptr),
#endif
    m_dmrEnabled(false), m_p25Enabled(false), m_p25P2Enabled(false),
    m_nxdnEnabled(false), m_analogEnabled(false), m_ridLookup(nullptr),
    m_tidLookup(nullptr), m_peerListLookup(nullptr), m_adjSiteMapLookup(nullptr),
    m_cryptoLookup(nullptr), m_peerNetworks(), m_pingTime(5U), m_maxMissedPings(5U),
    m_updateLookupTime(10U), m_peerReplicaSavesACL(false),
    m_allowActivityTransfer(false), m_allowDiagnosticTransfer(false), m_RESTAPI(nullptr)
{
    /* stub */
}

/**
 * @brief Finalizes an instance of the HostFNE class.
 */
HostFNE::~HostFNE() = default;

/**
 * @brief Adds a peer to the specified TrafficNetwork.
 * @param network The TrafficNetwork instance.
 * @param peerId The ID of the peer to add.
 * @param state The connection state of the peer.
 * @param connected Whether the peer is connected.
 * @return Reference to the added FNEPeerConnection.
 */
FNEPeerConnection& FNETestHooks::addPeer(TrafficNetwork& network, uint32_t peerId,
    NET_CONN_STATUS state, bool connected)
{
    FNEPeerConnection* connection = new FNEPeerConnection();
    connection->m_id = peerId;
    connection->m_connectionState = state;
    connection->m_connected = connected;
    network.m_peers[peerId] = connection;
    return *connection;
}

/**
 * @brief Passes an encoded KMM through the FNE OTAR message dispatcher.
 * @param network The TrafficNetwork instance.
 * @param packet The KMM packet data.
 * @param llId The logical link ID.
 * @param payloadSize The size of the processed payload.
 * @return A unique pointer to the processed KMM frame, or nullptr if processing failed.
 */
std::unique_ptr<uint8_t[]> FNETestHooks::processOTARKMM(TrafficNetwork& network,
    const std::vector<uint8_t>& packet, uint32_t llId, uint32_t& payloadSize,
    uint8_t outerAlgoId, uint16_t outerKId, bool dataLinkIndependent)
{
    payloadSize = 0U;
    if (packet.empty() || network.m_p25OTARService == nullptr)
        return nullptr;

    return network.m_p25OTARService->processKMM(packet.data(), (uint32_t)packet.size(), llId,
        false, &payloadSize, outerAlgoId, outerKId, nullptr, dataLinkIndependent);
}

/**
 * @brief Builds an OTAR rekey command for the specified logical link ID and KMM RSI.
 * @param network The TrafficNetwork instance.
 * @param llId The logical link ID.
 * @param kmmRSI The KMM RSI value.
 * @return A vector of vectors containing the OTAR rekey frames.
 */
std::vector<std::vector<uint8_t>> FNETestHooks::buildOTARRekey(TrafficNetwork& network,
    uint32_t llId, uint32_t kmmRSI)
{
    std::vector<std::vector<uint8_t>> frames;
    if (network.m_p25OTARService == nullptr)
        return frames;

    P25OTARService::KMMAuthContext auth;
    uint32_t firstLength = 0U;
    std::vector<std::vector<uint8_t>> additional;
    const bool previousAllowClear = network.m_p25OTARService->m_allowNoUKEKRekey;
    network.m_p25OTARService->m_allowNoUKEKRekey = true;
    UInt8Array first = network.m_p25OTARService->write_KMM_Rekey_Command(llId, kmmRSI,
        KMM_HelloFlag::REKEY_REQUEST_NO_UKEK, &firstLength, auth, &additional);
    network.m_p25OTARService->m_allowNoUKEKRekey = previousAllowClear;
    if (first == nullptr || firstLength == 0U)
        return frames;

    frames.emplace_back(first.get(), first.get() + firstLength);
    for (std::vector<uint8_t>& frame : additional)
        frames.push_back(std::move(frame));
    return frames;
}

/**
 * @brief Passes a DLD KMM through the public P25 OTAR bearer entry point.
 * @param network The TrafficNetwork instance.
 * @param packet The DLD packet data.
 * @param llId The logical link ID.
 * @param n The sequence number.
 * @param encrypted Whether the packet is encrypted.
 * @param algoId The algorithm ID used for encryption.
 * @param kid The key ID used for encryption.
 * @param mi The message integrity value.
 * @return True if the packet was successfully processed, false otherwise.
 */
bool FNETestHooks::processOTARDLD(TrafficNetwork& network, const std::vector<uint8_t>& packet,
    uint32_t llId, uint8_t n, bool encrypted, uint8_t algoId, uint16_t kid, const uint8_t* mi)
{
    if (packet.empty() || network.m_p25OTARService == nullptr)
        return false;

    return network.m_p25OTARService->processDLD(packet.data(), (uint32_t)packet.size(), llId, n,
        encrypted, algoId, kid, mi);
}

/**
 * @brief Encodes and injects a complete DLD KMM through the P25 PDU assembler path.
 * @param network The TrafficNetwork instance.
 * @param packet The DLD packet data.
 * @param llId The logical link ID.
 * @param encrypted Whether the packet is encrypted.
 * @param algoId The algorithm ID used for encryption.
 * @param kid The key ID used for encryption.
 * @param mi The message integrity value.
 * @return True if the packet was successfully processed, false otherwise.
 */
bool FNETestHooks::processOTARDLDPDU(TrafficNetwork& network, const std::vector<uint8_t>& packet,
    uint32_t llId, bool encrypted, uint8_t algoId, uint16_t kid, const uint8_t* mi)
{
    if (packet.empty() || network.m_tagP25 == nullptr)
        return false;

    p25::data::DataHeader header;
    header.setFormat(PDUFormatType::CONFIRMED);
    header.setMFId(MFG_STANDARD);
    header.setAckNeeded(true);
    header.setOutbound(false);
    header.setSAP(encrypted ? PDUSAP::ENC_USER_DATA : PDUSAP::UNENC_KMM);
    header.setLLId(llId);
    header.setFullMessage(true);
    header.setBlocksToFollow(1U);
    if (encrypted) {
        if (mi == nullptr)
            return false;
        header.setEXSAP(PDUSAP::UNENC_KMM);
        header.setAlgId(algoId);
        header.setKId(kid);
        header.setMI(mi);
    }
    header.calculateLength((uint32_t)packet.size());

    p25::data::Assembler assembler;
    uint32_t bitLength = 0U;
    UInt8Array assembled = assembler.assemble(header, false, encrypted, packet.data(), &bitLength);
    if (assembled == nullptr || bitLength <= P25_PREAMBLE_LENGTH_BITS)
        return false;

    const uint32_t blockCount = (bitLength - P25_PREAMBLE_LENGTH_BITS) / P25_PDU_FEC_LENGTH_BITS;
    auto* packetData = network.m_tagP25->packetData();
    for (uint32_t block = 0U; block < blockCount; ++block) {
        uint8_t envelope[24U + P25_PDU_FEC_LENGTH_BYTES] = { 0U };
        SET_UINT24(P25_PDU_FEC_LENGTH_BYTES, envelope, 8U);
        envelope[20U] = (uint8_t)(blockCount - 1U);
        envelope[21U] = (uint8_t)block;
        Utils::getBitRange(assembled.get(), envelope + 24U,
            P25_PREAMBLE_LENGTH_BITS + block * P25_PDU_FEC_LENGTH_BITS, P25_PDU_FEC_LENGTH_BITS);
        if (!packetData->processFrame(envelope, sizeof(envelope), 1U, (uint16_t)block, 1U))
            return false;
    }
    return true;
}

/**
 * @brief Passes a complete Version-0 DLI datagram through the network receive task.
 * @param network The TrafficNetwork instance.
 * @param datagram The DLI packet data.
 * @return True if the packet was successfully processed, false otherwise.
 */
void FNETestHooks::processOTARDLI(TrafficNetwork& network, const std::vector<uint8_t>& datagram)
{
    if (datagram.empty() || network.m_p25OTARService == nullptr)
        return;

    OTARPacketRequest* req = new OTARPacketRequest();
    req->obj = network.m_p25OTARService;
    req->address = {};
    req->addrLen = 0U;
    req->length = (int)datagram.size();
    req->buffer = new uint8_t[datagram.size()];
    ::memcpy(req->buffer, datagram.data(), datagram.size());

    P25OTARService::taskNetworkRx(req);
}

/**
 * @brief Opens the real DLI UDP endpoint for an integration test.
 * @param network The TrafficNetwork instance.
 * @param address The IP address of the DLI UDP endpoint.
 * @param port The port number of the DLI UDP endpoint.
 * @return True if the endpoint was successfully opened, false otherwise.
 */
bool FNETestHooks::openOTARDLI(TrafficNetwork& network, const std::string& address, uint16_t port)
{
    return network.m_p25OTARService != nullptr && network.m_p25OTARService->open(address, port);
}

/**
 * @brief Polls the real DLI UDP endpoint.
 * @param network The TrafficNetwork instance.
 * @param ms The number of milliseconds to wait.
 */
void FNETestHooks::clockOTARDLI(TrafficNetwork& network, uint32_t ms)
{
    if (network.m_p25OTARService != nullptr)
        network.m_p25OTARService->clock(ms);
}

/**
 * @brief Closes the real DLI UDP endpoint.
 * @param network The TrafficNetwork instance.
 */
void FNETestHooks::closeOTARDLI(TrafficNetwork& network)
{
    if (network.m_p25OTARService != nullptr)
        network.m_p25OTARService->close();
}

/**
 * @brief Applies the OTAR outer encryption/decryption implementation.
 * @param network The TrafficNetwork instance.
 * @param algoId The algorithm ID used for encryption/decryption.
 * @param kid The key ID used for encryption/decryption.
 * @param mi The message integrity value.
 * @param packet The packet data to be encrypted/decrypted.
 * @param encrypt True to encrypt, false to decrypt.
 * @return The resulting encrypted/decrypted packet.
 */
std::vector<uint8_t> FNETestHooks::cryptOTARKMM(TrafficNetwork& network, uint8_t algoId,
    uint16_t kid, uint8_t* mi, const std::vector<uint8_t>& packet, bool encrypt)
{
    if (packet.empty() || network.m_p25OTARService == nullptr)
        return {};

    UInt8Array result = network.m_p25OTARService->cryptKMM(algoId, kid, mi,
        packet.data(), (uint32_t)packet.size(), encrypt);
    if (result == nullptr)
        return {};
    return std::vector<uint8_t>(result.get(), result.get() + packet.size());
}

/**
 * @brief Checks if the specified inbound message number exists for the given RSI.
 * @param network The TrafficNetwork instance.
 * @param rsi The RSI to check.
 * @param mn The message number to check.
 * @return True if the inbound message number exists and matches, false otherwise.
 */
bool FNETestHooks::hasOTARInboundMessageNumber(TrafficNetwork& network, uint32_t rsi, uint16_t mn)
{
    if (network.m_p25OTARService == nullptr)
        return false;

    auto it = network.m_p25OTARService->m_rsiInboundMessageNumber.find(rsi);
    return it != network.m_p25OTARService->m_rsiInboundMessageNumber.end() && it->second == mn;
}

/**
 * @brief Enables or disables KMF services for the specified TrafficNetwork.
 * @param network The TrafficNetwork instance.
 * @param enabled True to enable KMF services, false to disable.
 */
void FNETestHooks::setKMFServicesEnabled(TrafficNetwork& network, bool enabled)
{
    network.m_kmfServicesEnabled = enabled;
}

/**
 * @brief Adds a cryptographic key to the network's crypto lookup.
 * @param network The TrafficNetwork instance.
 * @param key The cryptographic key to add.
 */
void FNETestHooks::addCryptoKey(TrafficNetwork& network, const EKCKeyItem& key)
{
    network.m_cryptoLookup->addEntry(key);
}

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

/**
 * @brief Checks if the specified peer exists in the TrafficNetwork.
 * @param network The TrafficNetwork instance.
 * @param peerId The ID of the peer to check.
 * @return True if the peer exists, false otherwise.
 */
bool FNETestHooks::hasPeer(const TrafficNetwork& network, uint32_t peerId)
{
    return network.m_peers.find(peerId) != network.m_peers.end();
}

/**
 * @brief Retrieves the number of peers in the TrafficNetwork.
 * @param network The TrafficNetwork instance.
 * @return The number of peers.
 */
size_t FNETestHooks::peerCount(const TrafficNetwork& network)
{
    return network.m_peers.size();
}

/**
 * @brief Retrieves the connection state of the specified peer.
 * @param network The TrafficNetwork instance.
 * @param peerId The ID of the peer.
 * @return The connection state of the peer.
 */
NET_CONN_STATUS FNETestHooks::peerState(const TrafficNetwork& network, uint32_t peerId)
{
    auto it = network.m_peers.find(peerId);
    if (it == network.m_peers.end() || it->second == nullptr)
        throw std::out_of_range("peer does not exist");
    return it->second->connectionState();
}

/**
 * @brief Clears all peers from the specified TrafficNetwork.
 * @param network The TrafficNetwork instance.
 */
void FNETestHooks::clearPeers(TrafficNetwork& network)
{
    for (auto peer : network.m_peers)
        delete peer.second;
    network.m_peers.clear();
}

/**
 * @brief Creates a NetPacketRequest for the specified network, metadata, function, subFunction, peerId, and packet.
 * @param network The TrafficNetwork instance.
 * @param metadata The MetadataNetwork instance (can be nullptr).
 * @param function The network function.
 * @param subFunction The network sub-function.
 * @param peerId The ID of the peer.
 * @param packet The packet data.
 * @return Pointer to the created NetPacketRequest.
 */
static NetPacketRequest* makeRequest(TrafficNetwork& network, MetadataNetwork* metadata,
    NET_FUNC::ENUM function, NET_SUBFUNC::ENUM subFunction, uint32_t peerId,
    const std::vector<uint8_t>& packet)
{
    NetPacketRequest* request = new NetPacketRequest();
    request->obj = &network;
    request->metadataObj = metadata;
    request->peerId = peerId;
    request->fneHeader.setFunction(function);
    request->fneHeader.setSubFunction(subFunction);
    request->fneHeader.setPeerId(peerId);
    request->fneHeader.setStreamId(1U);
    request->rtpHeader.setSSRC(peerId);
    request->pktRxTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    request->length = static_cast<int>(packet.size());
    if (!packet.empty()) {
        request->buffer = new uint8_t[packet.size()];
        ::memcpy(request->buffer, packet.data(), packet.size());
    }
    return request;
}

/**
 * @brief Dispatches a traffic packet to the specified TrafficNetwork.
 * @param network The TrafficNetwork instance.
 * @param function The network function.
 * @param subFunction The network sub-function.
 * @param peerId The ID of the peer.
 * @param packet The packet data.
 */
void FNETestHooks::dispatchTraffic(TrafficNetwork& network, NET_FUNC::ENUM function,
    NET_SUBFUNC::ENUM subFunction, uint32_t peerId, const std::vector<uint8_t>& packet)
{
    TrafficNetwork::taskNetworkRx(makeRequest(network, nullptr, function, subFunction, peerId, packet));
}

/**
 * @brief Dispatches a metadata packet to the specified TrafficNetwork.
 * @param network The TrafficNetwork instance.
 * @param metadata The MetadataNetwork instance.
 * @param function The network function.
 * @param subFunction The network sub-function.
 * @param peerId The ID of the peer.
 * @param packet The packet data.
 */
void FNETestHooks::dispatchMetadata(TrafficNetwork& network, MetadataNetwork& metadata,
    NET_FUNC::ENUM function, NET_SUBFUNC::ENUM subFunction, uint32_t peerId,
    const std::vector<uint8_t>& packet)
{
    MetadataNetwork::taskNetworkRx(makeRequest(network, &metadata, function, subFunction, peerId, packet));
}
