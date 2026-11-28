// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - IPSC Gateway
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "protocol/IPSCClient.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "common/network/RTPExtensionHeader.h"
#include "common/network/RTPHeader.h"

#include <openssl/crypto.h>
#include <openssl/hmac.h>

#include <cstring>
#include <random>

using namespace ipsc;
using namespace ipsc::defines;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the IPSCClient class. */

IPSCClient::IPSCClient(const std::string& masterAddress, uint16_t masterPort, uint16_t localPort, uint32_t peerId, const std::string& authenticationKey, bool slot1, bool slot2, bool debug) :
    m_masterAddress(masterAddress),
    m_masterPort(masterPort),
    m_localPort(localPort),
    m_peerId(peerId),
    m_authenticationKey(),
    m_authenticated(false),
    m_slot1(slot1),
    m_slot2(slot2),
    m_debug(debug),
    m_socket(localPort),
    m_master(),
    m_masterLength(0U),
    m_state(State::IPSC_DISCONNECTED),
    m_protocolVersion(defines::PROTOCOL_MAX_VERSION),
    m_peers(),
    m_lastMasterTraffic(),
    m_nextAction(),
    m_nextKeepAlive(),
    m_voiceCallback(nullptr),
    m_rtpSequence(0U),
    m_rtpTimestamp(0U),
    m_rtpSSRC(std::random_device()())
{
    if (!authenticationKey.empty()) {
        std::string key(authenticationKey);
        if (key.length() < 40U)
            key.insert(0U, 40U - key.length(), '0');
        if (key.length() == 40U && key.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos) {
            for (uint32_t i = 0U; i < defines::AUTHENTICATION_KEY_LENGTH; ++i)
                m_authenticationKey[i] = uint8_t(::strtoul(key.substr(i * 2U, 2U).c_str(), nullptr, 16));
            m_authenticated = true;
        }
    }
}

/* Finalizes an instance of the IPSCClient class. */

IPSCClient::~IPSCClient()
{
    close();
}

/* Opens the IPSC UDP transport and begins registration. */

bool IPSCClient::open()
{
    if (m_peerId == 0U || (m_peerId & 0xFFFFFFU) == 0xFFFFFFU || m_peerId > 0xFFFFFFU) {
        LogError(LOG_NET, "IPSC peer ID is reserved or out of range, peerId = %u", m_peerId);
        return false;
    }

    if (network::udp::Socket::lookup(m_masterAddress, m_masterPort, m_master, m_masterLength) != 0)
        return false;

    if (!m_socket.open(m_master.ss_family))
        return false;

    m_state = State::IPSC_REGISTERING;
    sendRegistration();
    return true;
}

/* Deregisters and closes the IPSC UDP transport. */

void IPSCClient::close()
{
    if (m_state != State::IPSC_DISCONNECTED) {
        uint8_t packet[defines::RETR_HEADER_LENGTH] = {0U};
        packet[0U] = uint8_t(MessageType::DEREGISTRATION_REQUEST);
        SET_UINT32(m_peerId, packet, 1U);
        sendMaster(packet, sizeof(packet));
    }

    m_socket.close();

    m_state = State::IPSC_DISCONNECTED;

    m_peers.clear();
}

/* Processes received packets and session timers. */

void IPSCClient::clock()
{
    uint8_t buffer[defines::MAX_PACKET_LENGTH + defines::AUTHENTICATION_TAG_LENGTH];
    sockaddr_storage source;
    uint32_t sourceLength = 0U;

    // read all available packets from the socket
    while (true) {
        const ssize_t length = m_socket.read(buffer, sizeof(buffer), source, sourceLength);
        if (length <= 0)
            break;

        process(buffer, uint32_t(length), source, sourceLength);
    }

    const auto now = std::chrono::steady_clock::now();
    if (m_state == State::IPSC_REGISTERING && now >= m_nextAction)
        sendRegistration();
    else if (m_state == State::IPSC_REQUESTING_MAP && now >= m_nextAction)
        sendMapRequest();
    else if (m_state == State::IPSC_CONNECTED && now >= m_nextKeepAlive)
        sendKeepAlives();

    // check for master timeout and restart registration if necessary
    if (m_state == State::IPSC_CONNECTED && now - m_lastMasterTraffic > std::chrono::seconds(defines::MASTER_TIMEOUT_SECONDS)) {
        LogWarning(LOG_NET, "IPSC master timed out; restarting registration");
        m_state = State::IPSC_REGISTERING;
        sendRegistration();
    }

    // check for peer timeouts and remove stale peers
    for (auto it = m_peers.begin(); it != m_peers.end();) {
        if (now - it->second.lastSeen > std::chrono::seconds(defines::PEER_TIMEOUT_SECONDS))
            it = m_peers.erase(it);
        else
            ++it;
    }
}

/* Writes an IPSC voice packet to the master and registered peers. */

bool IPSCClient::writeVoice(uint32_t sourceId, uint32_t destinationId, bool group, uint8_t slotNo, uint8_t sequence, bool last, 
    const uint8_t* burst, uint32_t burstLength)
{
    if (!connected() || burst == nullptr || burstLength == 0U || defines::VOICE_PAYLOAD_OFFSET + burstLength > defines::MAX_PACKET_LENGTH)
        return false;

    // prepare the voice packet with the appropriate headers and payload
    uint8_t packet[defines::MAX_PACKET_LENGTH];
    const uint32_t packetLength = defines::VOICE_PAYLOAD_OFFSET + burstLength;
    ::memset(packet, 0x00U, packetLength);

    packet[0U] = uint8_t(group ? MessageType::GROUP_VOICE : MessageType::PRIVATE_VOICE);
    SET_UINT32(m_peerId, packet, 1U);
    packet[5U] = sequence;
    SET_UINT24(sourceId, packet, 6U);
    SET_UINT24(destinationId, packet, 9U);
    packet[12U] = 2U;
    packet[17U] = (slotNo == 2U ? defines::SLOT_TWO_FLAG : defines::SLOT_ONE_FLAG) | (last ? defines::LAST_PACKET_FLAG : 0U);

    const uint8_t burstType = burst[0U] & defines::BURST_TYPE_MASK;

    // prepare the RTP header for the voice packet
    network::frame::RTPHeader rtpHeader;
    rtpHeader.setMarker(burstType == uint8_t(defines::BurstType::VOICE_HEADER));
    rtpHeader.setPayloadType(defines::RTP_PAYLOAD_AMBE);
    rtpHeader.setSequence(m_rtpSequence++);
    rtpHeader.setTimestamp(m_rtpTimestamp);
    rtpHeader.setSSRC(m_rtpSSRC);
    rtpHeader.encode(packet + defines::VOICE_HEADER_LENGTH);
    m_rtpTimestamp += defines::RTP_TIMESTAMP_STEP;
    ::memcpy(packet + defines::VOICE_PAYLOAD_OFFSET, burst, burstLength);

    bool result = sendMaster(packet, packetLength);

    // send the voice packet to all connected peers
    for (const auto& entry : m_peers) {
        if (entry.second.connected)
            result = send(packet, packetLength, entry.second.address, entry.second.addressLength) || result;
    }

    return result;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Creates a registration or keepalive status message. */

void IPSCClient::createStatusMessage(MessageType type, uint8_t* packet) const
{
    ::memset(packet, 0x00U, defines::STATUS_MESSAGE_LENGTH);
    packet[0U] = uint8_t(type);
    SET_UINT32(m_peerId, packet, 1U);

    uint8_t flags = PEER_DIGITAL_MODE | PEER_ENABLED;
    flags |= m_slot2 ? SLOT_TWO_IPSC_SUPPORT : 0U;
    flags |= m_slot1 ? SLOT_ONE_IPSC_SUPPORT : 0U;
    packet[5U] = flags;

    uint32_t services = uint32_t(defines::PeerService::VOICE_CALL) | 
        uint32_t(defines::PeerService::CONSOLE_APPLICATION) | uint32_t(defines::PeerService::CALL_MONITORING);
    if (m_authenticated)
        services |= uint32_t(defines::PeerService::PACKET_AUTHENTICATION);

    SET_UINT32(services, packet, 6U);
    SET_UINT16(defines::encodeProtocolVersion(m_protocolVersion), packet, 10U);
    SET_UINT16(defines::encodeProtocolVersion(defines::PROTOCOL_MIN_VERSION), packet, 12U);
}

/* Appends the configured IPSC HMAC authentication tag. */

uint32_t IPSCClient::authenticate(const uint8_t* packet, uint32_t length, uint8_t* output) const
{
    ::memcpy(output, packet, length);
    if (!m_authenticated)
        return length;

    unsigned int digestLength = 0U;
    uint8_t digest[EVP_MAX_MD_SIZE];
    HMAC(EVP_sha1(), m_authenticationKey, int(sizeof(m_authenticationKey)), packet, length, digest, &digestLength);

    ::memcpy(output + length, digest, defines::AUTHENTICATION_TAG_LENGTH);

    return length + defines::AUTHENTICATION_TAG_LENGTH;
}

/* Verifies and removes an IPSC HMAC authentication tag. */

bool IPSCClient::verify(const uint8_t* packet, uint32_t& length) const
{
    if (!m_authenticated)
        return true;

    if (length < defines::RETR_HEADER_LENGTH + defines::AUTHENTICATION_TAG_LENGTH)
        return false;

    const uint32_t bodyLength = length - defines::AUTHENTICATION_TAG_LENGTH;

    unsigned int digestLength = 0U;
    uint8_t digest[EVP_MAX_MD_SIZE];
    HMAC(EVP_sha1(), m_authenticationKey, int(sizeof(m_authenticationKey)), packet, bodyLength, digest, &digestLength);
    if (CRYPTO_memcmp(digest, packet + bodyLength, defines::AUTHENTICATION_TAG_LENGTH) != 0)
        return false;

    length = bodyLength;
    return true;
}

/* Sends an IPSC datagram to a resolved network address. */

bool IPSCClient::send(const uint8_t* packet, uint32_t length, const sockaddr_storage& address, uint32_t addressLength)
{
    if (packet == nullptr || length == 0U || length > defines::MAX_PACKET_LENGTH)
        return false;
 
    if (!m_authenticated) {
       if (m_debug)
            Utils::dump(1U, "IPSC TX", packet, length);
        return m_socket.write(packet, length, address, addressLength);
    }

    uint8_t output[defines::MAX_PACKET_LENGTH + defines::AUTHENTICATION_TAG_LENGTH];
    const uint32_t outputLength = authenticate(packet, length, output);
    if (m_debug)
        Utils::dump(1U, "IPSC TX", output, outputLength);

    return m_socket.write(output, outputLength, address, addressLength);
}

/* Sends an IPSC datagram to the configured master. */

bool IPSCClient::sendMaster(const uint8_t* packet, uint32_t length)
{
    return send(packet, length, m_master, m_masterLength);
}

/* Sends a master registration request. */

void IPSCClient::sendRegistration()
{
    uint8_t packet[defines::STATUS_MESSAGE_LENGTH];
    createStatusMessage(MessageType::MASTER_REGISTRATION_REQUEST, packet);

    sendMaster(packet, sizeof(packet));

    m_nextAction = std::chrono::steady_clock::now() + std::chrono::seconds(defines::REGISTRATION_RETRY_SECONDS);
}

/* Sends a master peer-map request. */

void IPSCClient::sendMapRequest()
{
    uint8_t packet[defines::RETR_HEADER_LENGTH] = {0U};
    packet[0U] = uint8_t(MessageType::MASTER_MAP_REQUEST);
    SET_UINT32(m_peerId, packet, 1U);

    sendMaster(packet, sizeof(packet));

    m_nextAction = std::chrono::steady_clock::now() + std::chrono::seconds(defines::REGISTRATION_RETRY_SECONDS);
}

/* Sends a response to an inbound peer session message. */

void IPSCClient::reply(MessageType type, uint32_t peerId, const sockaddr_storage& source, uint32_t sourceLength)
{
    uint8_t packet[defines::STATUS_MESSAGE_LENGTH] = {0U};
    uint32_t length = 5U;

    if (type == MessageType::PEER_REGISTRATION_RESPONSE) {
        length = defines::PEER_REGISTRATION_LENGTH;
        SET_UINT16(defines::encodeProtocolVersion(m_protocolVersion), packet, 5U);
        SET_UINT16(defines::encodeProtocolVersion(defines::PROTOCOL_MIN_VERSION), packet, 7U);
    } else if (type == MessageType::PEER_KEEP_ALIVE_RESPONSE) {
        createStatusMessage(type, packet);
        length = defines::PEER_KEEP_ALIVE_LENGTH;
    }

    packet[0U] = uint8_t(type);
    SET_UINT32(m_peerId, packet, 1U);

    send(packet, length, source, sourceLength);

    auto it = m_peers.find(peerId);
    if (it != m_peers.end()) {
        it->second.lastSeen = std::chrono::steady_clock::now();
        it->second.connected = true;
    }
}

/* Processes a master peer-map broadcast. */

void IPSCClient::processMap(const uint8_t* packet, uint32_t length)
{
    if (length < 7U)
        return;
 
    const uint16_t payloadLength = GET_UINT16(packet, 5U);
    if (payloadLength % 11U != 0U || length < 7U + payloadLength)
        return;

    // iterate over each peer entry in the payload; each entry is 11 bytes long
    for (uint32_t offset = 7U; offset < 7U + payloadLength; offset += 11U) {
        const uint32_t id = GET_UINT32(packet, offset);
        if (id == m_peerId || id == 0U || (id & 0xFFFFFFU) == 0xFFFFFFU)
            continue;

        char address[INET_ADDRSTRLEN];
        ::snprintf(address, sizeof(address), "%u.%u.%u.%u", packet[offset + 4U], packet[offset + 5U], packet[offset + 6U], packet[offset + 7U]);

        Peer peer{};
        peer.id = id;
        const uint16_t port = GET_UINT16(packet, offset + 8U);
        if (network::udp::Socket::lookup(address, port, peer.address, peer.addressLength) != 0)
            continue;

        peer.lastSeen = std::chrono::steady_clock::now();
        peer.connected = false;

        m_peers[id] = peer;

        uint8_t request[defines::PEER_REGISTRATION_LENGTH] = {0U};
        request[0U] = uint8_t(MessageType::PEER_REGISTRATION_REQUEST);
        SET_UINT32(m_peerId, request, 1U);
        SET_UINT16(defines::encodeProtocolVersion(m_protocolVersion), request, 5U);
        SET_UINT16(defines::encodeProtocolVersion(defines::PROTOCOL_MIN_VERSION), request, 7U);

        send(request, sizeof(request), peer.address, peer.addressLength);
    }

    m_state = State::IPSC_CONNECTED;
    m_nextKeepAlive = std::chrono::steady_clock::now();

    LogInfoEx(LOG_NET, "IPSC peer %u connected, peer count = %u", m_peerId, uint32_t(m_peers.size()));
}

/* Decodes an IPSC voice message and RTP payload. */

void IPSCClient::processVoice(const uint8_t* packet, uint32_t length)
{
    if (length < defines::VOICE_PAYLOAD_OFFSET + 1U || m_voiceCallback == nullptr)
        return;

    uint32_t packetEnd = length;
    const MessageType messageType = MessageType(packet[0U]);
    if (messageType == MessageType::GROUP_VOICE_PLUS || messageType == MessageType::PRIVATE_VOICE_PLUS) {
        if (packetEnd < defines::VOICE_PAYLOAD_OFFSET + defines::CALL_PLUS_TRAILER_LENGTH + 1U)
            return;

        packetEnd -= defines::CALL_PLUS_TRAILER_LENGTH;
    }

    const uint32_t rtpOffset = defines::VOICE_HEADER_LENGTH;
    network::frame::RTPHeader rtpHeader;
    if (!rtpHeader.decode(packet + rtpOffset) || rtpHeader.getPayloadType() != defines::RTP_PAYLOAD_AMBE)
        return;

    uint32_t rtpHeaderLength = RTP_HEADER_LENGTH_BYTES + uint32_t(rtpHeader.getCSRCCount()) * 4U;
    if (rtpOffset + rtpHeaderLength > packetEnd)
        return;

    if (rtpHeader.getExtension()) {
        if (rtpOffset + rtpHeaderLength + RTP_EXTENSION_HEADER_LENGTH_BYTES > packetEnd)
            return;

        network::frame::RTPExtensionHeader extensionHeader;
        extensionHeader.decode(packet + rtpOffset + rtpHeaderLength);
        rtpHeaderLength += RTP_EXTENSION_HEADER_LENGTH_BYTES + uint32_t(extensionHeader.getPayloadLength()) * 4U;
        if (rtpOffset + rtpHeaderLength > packetEnd)
            return;
    }

    uint32_t payloadEnd = packetEnd;
    if (rtpHeader.getPadding()) {
        const uint8_t padding = packet[payloadEnd - 1U];
        if (padding == 0U || padding > payloadEnd - rtpOffset - rtpHeaderLength)
            return;

        payloadEnd -= padding;
    }

    const uint32_t payloadOffset = rtpOffset + rtpHeaderLength;
    if (payloadOffset >= payloadEnd)
        return;

    uint32_t burstLength = payloadEnd - payloadOffset;
    const uint8_t burstType = packet[payloadOffset] & defines::BURST_TYPE_MASK;
    if (burstType == uint8_t(defines::BurstType::VOICE) || burstType == uint8_t(defines::BurstType::VOICE_PLUS)) {
        if (burstLength < 2U || uint32_t(packet[payloadOffset + 1U]) + 2U > burstLength)
            return;

        burstLength = uint32_t(packet[payloadOffset + 1U]) + 2U;
    } else if (burstType == uint8_t(defines::BurstType::VOICE_HEADER) || burstType == uint8_t(defines::BurstType::VOICE_TERMINATOR)) {
        if (burstLength < 8U)
            return;

        const uint16_t burstWords = GET_UINT16(packet, payloadOffset + 2U);
        const uint32_t renderedLength = uint32_t(burstWords) * 2U + 4U;
        if (renderedLength < 20U || renderedLength > burstLength)
            return;

        burstLength = renderedLength;
    }

    VoicePacket voice{};
    voice.peerId = GET_UINT32(packet, 1U);
    voice.sequence = packet[5U];
    voice.sourceId = GET_UINT24(packet, 6U);
    voice.destinationId = GET_UINT24(packet, 9U);
    voice.group = messageType == MessageType::GROUP_VOICE || messageType == MessageType::GROUP_VOICE_PLUS;
    voice.last = (packet[17U] & defines::LAST_PACKET_FLAG) != 0U;
    voice.slotNo = (packet[17U] & defines::SLOT_TWO_FLAG) != 0U ? 2U : 1U;
    voice.hasColorCode = false;
    voice.colorCode = 0U;
    voice.burst = packet + payloadOffset;
    voice.burstLength = burstLength;

    // extract optional color code from the burst if present
    if ((burstType == uint8_t(defines::BurstType::VOICE_HEADER) || burstType == uint8_t(defines::BurstType::VOICE_TERMINATOR)) && burstLength >= 8U) {
        const uint16_t flags = GET_UINT16(voice.burst, 4U);
        const uint16_t payloadBits = GET_UINT16(voice.burst, 6U);
        const uint32_t payloadLength = (uint32_t(payloadBits) + 7U) / 8U;
        const uint32_t slotOffset = 8U + payloadLength;
        if ((flags & defines::BURST_SLOT_TYPE_FLAG) != 0U && slotOffset + 2U <= burstLength) {
            voice.colorCode = voice.burst[slotOffset + 1U] >> 4U;
            voice.hasColorCode = true;
        }
    } else if ((burstType == uint8_t(defines::BurstType::VOICE) || burstType == uint8_t(defines::BurstType::VOICE_PLUS)) && 
        burstLength >= defines::VOICE_BURST_BASE_LENGTH) {
        uint32_t optionOffset = defines::VOICE_BURST_BASE_LENGTH;
        const uint8_t control = voice.burst[2U];

        if ((control & defines::VOICE_LC_HARD_BITS_FLAG) != 0U)
            optionOffset += defines::VOICE_LC_HARD_BITS_LENGTH;

        if ((control & defines::VOICE_EMBEDDED_LC_FLAG) != 0U)
            optionOffset += defines::VOICE_EMBEDDED_LC_LENGTH;

        if ((control & defines::VOICE_EMB_FLAG) != 0U && optionOffset < burstLength) {
            voice.colorCode = voice.burst[optionOffset] >> 4U;
            voice.hasColorCode = true;
        }
    }

    m_voiceCallback(voice);
}

/* Processes and logs an IPSC call-control response. */

void IPSCClient::processCallResponse(const uint8_t* packet, uint32_t length) const
{
    if (length < 17U)
        return;

    const uint32_t sourceId = GET_UINT24(packet, 9U);
    const uint32_t destinationId = GET_UINT24(packet, 12U);

    LogInfoEx(LOG_NET, "IPSC call response, type = $%02X, srcId = %u, dstId = %u, slot = %u, status = %u", packet[0U], sourceId, destinationId,
              packet[16U] + 1U, packet[15U]);
}

/* Processes and logs an IPSC RCM call status message. */

void IPSCClient::processCallMonitor(const uint8_t* packet, uint32_t length) const
{
    if (length < 31U)
        return;

    const uint32_t peerId = GET_UINT32(packet, 5U);
    const uint32_t sourceId = GET_UINT24(packet, 16U);
    const uint32_t destinationId = GET_UINT24(packet, 19U);
    const uint16_t status = GET_UINT16(packet, 14U);

    LogInfoEx(LOG_NET, "IPSC call monitor, peerId = %u, srcId = %u, dstId = %u, slot = %u, type = %u, status = %u", peerId, sourceId, destinationId,
              packet[13U] + 1U, packet[22U], status);
}

/* Dispatches a verified IPSC datagram. */

void IPSCClient::process(const uint8_t* packet, uint32_t length, const sockaddr_storage& source, uint32_t sourceLength)
{
    if (!verify(packet, length) || length < 5U)
        return;

    if (m_debug)
        Utils::dump(1U, "IPSC RX", packet, length);

    const MessageType type = MessageType(packet[0U]);
    const uint32_t sender = GET_UINT32(packet, 1U);
    const auto now = std::chrono::steady_clock::now();
    if (network::udp::Socket::match(source, m_master))
        m_lastMasterTraffic = now;

    auto peer = m_peers.find(sender);
    if (peer != m_peers.end())
        peer->second.lastSeen = now;

    switch (type) {
    case MessageType::CALL_CONTROL_REQUEST:
    case MessageType::COMMON_CSBK_CALL:
    case MessageType::PRIVATE_CALL_RESPONSE:
    case MessageType::CALL_CONTROL_RESPONSE:
    case MessageType::CALL_STATUS:
        processCallResponse(packet, length);
        break;
    case MessageType::RCM_CALL_TRANSMISSION_STATUS:
        processCallMonitor(packet, length);
        break;
    case MessageType::RCM_CALL_CONTROL_NOTIFICATION:
    case MessageType::RCM_REPEAT_BLOCKED_INDICATION:
        LogInfoEx(LOG_NET, "IPSC call notification, type = $%02X, peerId = %u", packet[0U], sender);
        break;
    case MessageType::MASTER_REGISTRATION_RESPONSE:
        if (length >= 16U) {
            const uint16_t peerCount = GET_UINT16(packet, 10U);
            const uint16_t protocol = GET_UINT16(packet, 12U);
            const uint16_t protocolType = protocol >> defines::PROTOCOL_TYPE_SHIFT;
            if (protocolType != defines::PROTOCOL_TYPE_IPSC) {
                LogWarning(LOG_NET, "IPSC master returned unsupported protocol type, type = %u", protocolType);
                break;
            }

            m_protocolVersion = uint8_t(protocol & defines::PROTOCOL_VERSION_MASK);
            m_lastMasterTraffic = now;
            if (peerCount == 0U) {
                m_state = State::IPSC_CONNECTED;
                m_nextKeepAlive = now;
            } else {
                m_state = State::IPSC_REQUESTING_MAP;
                sendMapRequest();
            }
        }
        break;
    case MessageType::MASTER_MAP_BROADCAST:
        processMap(packet, length);
        break;
    case MessageType::PEER_REGISTRATION_REQUEST:
        reply(MessageType::PEER_REGISTRATION_RESPONSE, sender, source, sourceLength);
        break;
    case MessageType::PEER_REGISTRATION_RESPONSE:
    case MessageType::PEER_KEEP_ALIVE_RESPONSE:
        if (peer != m_peers.end())
            peer->second.connected = true;
        break;
    case MessageType::MASTER_KEEP_ALIVE_RESPONSE:
        break;
    case MessageType::PEER_KEEP_ALIVE_REQUEST:
        reply(MessageType::PEER_KEEP_ALIVE_RESPONSE, sender, source, sourceLength);
        break;
    case MessageType::DEREGISTRATION_REQUEST:
        reply(MessageType::DEREGISTRATION_RESPONSE, sender, source, sourceLength);
        m_peers.erase(sender);
        break;
    case MessageType::GROUP_VOICE:
    case MessageType::PRIVATE_VOICE:
    case MessageType::GROUP_VOICE_PLUS:
    case MessageType::PRIVATE_VOICE_PLUS:
        processVoice(packet, length);
        break;
    default:
        break;
    }
}

/* Sends master and peer keepalive requests. */

void IPSCClient::sendKeepAlives()
{
    uint8_t masterKeepAlive[defines::STATUS_MESSAGE_LENGTH];
    createStatusMessage(MessageType::MASTER_KEEP_ALIVE_REQUEST, masterKeepAlive);
    sendMaster(masterKeepAlive, sizeof(masterKeepAlive));

    uint8_t peerKeepAlive[defines::STATUS_MESSAGE_LENGTH];
    createStatusMessage(MessageType::PEER_KEEP_ALIVE_REQUEST, peerKeepAlive);
    for (auto& entry : m_peers)
        send(peerKeepAlive, defines::PEER_KEEP_ALIVE_LENGTH, entry.second.address, entry.second.addressLength);

    m_nextKeepAlive = std::chrono::steady_clock::now() + std::chrono::seconds(defines::KEEP_ALIVE_SECONDS);
}
