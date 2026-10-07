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
#include "common/edac/SHA256.h"
#include "common/json/json.h"
#include "common/AESCrypto.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "network/Network.h"

using namespace network;

#include <cstdio>
#include <cassert>
#include <cmath>
#include <unordered_map>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the Network class. */

Network::Network(const std::string& address, uint16_t port, uint16_t localPort, uint32_t peerId, const std::string& password,
    bool duplex, bool debug, bool dmr, bool p25, bool nxdn, bool analog, bool slot1, bool slot2, 
    bool allowActivityTransfer, bool allowDiagnosticTransfer, bool updateLookup, bool saveLookup) :
    BaseNetwork(peerId, duplex, debug, slot1, slot2, allowActivityTransfer, allowDiagnosticTransfer, localPort),
    m_pktLastSeq(0U),
    m_address(address),
    m_port(port),
    m_configuredAddress(address),
    m_configuredPort(port),
    m_haIPs(),
    m_currentHAIP(0U),
    m_password(password),
    m_enabled(false),
    m_dmrEnabled(dmr),
    m_p25Enabled(p25),
    m_nxdnEnabled(nxdn),
    m_analogEnabled(analog),
    m_updateLookup(updateLookup),
    m_saveLookup(saveLookup),
    m_ridLookup(nullptr),
    m_tidLookup(nullptr),
    m_salt(nullptr),
    m_kmfPresharedKey(nullptr),
    m_retryTimer(1000U, DEFAULT_RETRY_TIME),
    m_retryCount(0U),
    m_maxRetryCount(MAX_RETRY_BEFORE_RECONNECT),
    m_flaggedDuplicateConn(false),
    m_timeoutTimer(1000U, MAX_PEER_PING_TIME),
    m_pingsReceived(0U),
    m_pktSeq(0U),
    m_loginStreamId(0U),
    m_metadata(nullptr),
    m_mux(nullptr),
    m_remotePeerId(0U),
    m_promiscuousPeer(false),
    m_userHandleProtocol(false),
    m_neverDisableOnACLNAK(false),
    m_passKeysWithNoPresharedKey(false),
    m_peerConnectedCallback(nullptr),
    m_peerDisconnectedCallback(nullptr),
    m_dmrInCallCallback(nullptr),
    m_p25InCallCallback(nullptr),
    m_p25P2InCallCallback(nullptr),
    m_nxdnInCallCallback(nullptr),
    m_analogInCallCallback(nullptr),
    m_keyRespCallback(nullptr),
    m_llaKeyRespCallback(nullptr)
{
    assert(!address.empty());
    assert(port > 0U);
    assert(!password.empty());

    m_salt = new uint8_t[sizeof(uint32_t)];

    m_rxDMRStreamId = new uint32_t[2U];
    m_rxDMRStreamId[0U] = 0U;
    m_rxDMRStreamId[1U] = 0U;
    m_rxP25StreamId = 0U;
    m_rxP25P2StreamId = new uint32_t[2U];
    m_rxP25P2StreamId[0U] = 0U;
    m_rxP25P2StreamId[1U] = 0U;
    m_rxNXDNStreamId = 0U;
    m_rxAnalogStreamId = 0U;

    m_metadata = new PeerMetadata();
    m_mux = new RTPStreamMultiplex();
}

/* Finalizes a instance of the Network class. */

Network::~Network()
{
    if (m_kmfPresharedKey != nullptr) {
        delete[] m_kmfPresharedKey;
        m_kmfPresharedKey = nullptr;
    }
    delete[] m_salt;
    delete[] m_rxDMRStreamId;
    delete[] m_rxP25P2StreamId;
    delete m_metadata;
    delete m_mux;
}

/* Resets the DMR ring buffer for the given slot. */

void Network::resetDMR(uint32_t slotNo)
{
    assert(slotNo == 1U || slotNo == 2U);

    BaseNetwork::resetDMR(slotNo);
    if (slotNo == 1U) {
        m_rxDMRStreamId[0U] = 0U;
    }
    else {
        m_rxDMRStreamId[1U] = 0U;
    }

    if (m_debug)
        LogDebugEx(LOG_NET, "Network::resetDMR()", "reset DMR Slot %u rx stream ID", slotNo);
}

/* Resets the P25 ring buffer. */

void Network::resetP25()
{
    BaseNetwork::resetP25();
    m_rxP25StreamId = 0U;

    if (m_debug)
        LogDebugEx(LOG_NET, "Network::resetP25()", "reset P25 rx stream ID");
}

/* Resets the P25 Phase 2 ring buffer for the given slot. */

void Network::resetP25P2(uint32_t slotNo)
{
    assert(slotNo == 1U || slotNo == 2U);

    BaseNetwork::resetP25P2(slotNo);
    if (slotNo == 1U) {
        m_rxP25P2StreamId[0U] = 0U;
    }
    else {
        m_rxP25P2StreamId[1U] = 0U;
    }

    if (m_debug)
        LogDebugEx(LOG_NET, "Network::resetP25P2()", "reset P25 Phase 2 Slot %u rx stream ID", slotNo);
}

/* Resets the NXDN ring buffer. */

void Network::resetNXDN()
{
    BaseNetwork::resetNXDN();
    m_rxNXDNStreamId = 0U;

    if (m_debug)
        LogDebugEx(LOG_NET, "Network::resetNXDN()", "reset NXDN rx stream ID");
}

/* Resets the analog ring buffer. */

void Network::resetAnalog()
{
    BaseNetwork::resetAnalog();
    m_rxAnalogStreamId = 0U;

    if (m_debug)
        LogDebugEx(LOG_NET, "Network::resetAnalog()", "reset analog rx stream ID");
}

/* Sets the instances of the Radio ID and Talkgroup ID lookup tables. */

void Network::setLookups(lookups::RadioIdLookup* ridLookup, lookups::TalkgroupRulesLookup* tidLookup)
{
    m_ridLookup = ridLookup;
    m_tidLookup = tidLookup;
}

/* Sets metadata configuration settings from the modem. */

void Network::setMetadata(const std::string& identity, uint32_t rxFrequency, uint32_t txFrequency, float txOffsetMhz, float chBandwidthKhz,
    uint8_t channelId, uint32_t channelNo, uint32_t power, float latitude, float longitude, int height, const std::string& location)
{
    m_metadata->identity = identity;

    m_metadata->rxFrequency = rxFrequency;
    m_metadata->txFrequency = txFrequency;

    m_metadata->txOffsetMhz = txOffsetMhz;
    m_metadata->chBandwidthKhz = chBandwidthKhz;
    m_metadata->channelId = channelId;
    m_metadata->channelNo = channelNo;

    m_metadata->power = power;
    m_metadata->latitude = latitude;
    m_metadata->longitude = longitude;
    m_metadata->height = height;
    m_metadata->location = location;
}

/* Sets REST API configuration settings from the modem. */

void Network::setRESTAPIData(const std::string& password, uint16_t port)
{
    m_metadata->restApiPassword = password;
    m_metadata->restApiPort = port;
}

/* Sets endpoint preshared encryption key. */

void Network::setPresharedKey(const uint8_t* presharedKey)
{
    m_socket->setPresharedKey(presharedKey);
}

/* Sets endpoint preshared encryption key. */

void Network::setKMFPresharedKey(const uint8_t* presharedKey)
{
    m_kmfPresharedKey = new uint8_t[P25DEF::MAX_ENC_KEY_LENGTH_BYTES];
    memcpy(m_kmfPresharedKey, presharedKey, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
}

/* Updates the timer by the passed number of milliseconds. */

void Network::clock(uint32_t ms)
{
    if (m_status == NET_STAT_WAITING_CONNECT) {
        m_retryTimer.clock(ms);
        if (m_retryTimer.isRunning() && m_retryTimer.hasExpired()) {
            if (m_enabled) {
                if (m_retryCount > m_maxRetryCount) {
                    if (m_flaggedDuplicateConn) {
                        LogError(LOG_NET, "PEER %u exceeded maximum duplicate connection retries, increasing delay between connection attempts", m_peerId);
                    }

                    LogError(LOG_NET, "PEER %u connection to the master has timed out, retrying connection, remotePeerId = %u", m_peerId, m_remotePeerId);

                    close();
                    open();

                    m_retryCount = 0U;
                    m_retryTimer.start();
                    return;
                }

                bool ret = m_socket->open(m_addr.ss_family);
                if (ret) {
                    m_socket->recvBufSize(262144U); // 256K recv buffer
                    ret = writeLogin();
                    if (!ret) {
                        m_retryTimer.start();
                        m_retryCount++;
                        return;
                    }

                    m_status = NET_STAT_WAITING_LOGIN;
                    m_timeoutTimer.start();
                }
            }

            m_retryTimer.start();
            m_retryCount++;
        }

        return;
    }

    // if we are waiting for a login response, check the timeout
    if (m_status == NET_STAT_WAITING_LOGIN) {
        m_retryTimer.clock(ms);
        if (m_retryTimer.isRunning() && m_retryTimer.hasExpired()) {
            LogError(LOG_NET, "PEER %u login attempt to the master has timed out, retrying connection", m_peerId);

            close();
            open();
            return;
        }
    }

    // if we aren't enabled -- bail
    if (!m_enabled) {
        return;
    }

    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // roll the RTP timestamp if no call is in progress
    if ((m_status == NET_STAT_RUNNING) &&
        (m_rxDMRStreamId[0U] == 0U && m_rxDMRStreamId[1U] == 0U) &&
        m_rxP25StreamId == 0U && m_rxNXDNStreamId == 0U) {
        frame::RTPHeader::resetStartTime();
    }

    sockaddr_storage address;
    uint32_t addrLen;

    frame::RTPHeader rtpHeader;
    frame::RTPFNEHeader fneHeader;
    int length = 0U;

    // read message
    UInt8Array buffer = m_frameQueue->read(length, address, addrLen, &rtpHeader, &fneHeader);
    if (length > 0) {
        if (!udp::Socket::match(m_addr, address)) {
            LogError(LOG_NET, "Packet received from an invalid source");
            return;
        }

        if (m_debug) {
            LogDebugEx(LOG_NET, "Network::clock()", "RTP, peerId = %u, ssrc = %u, seq = %u, streamId = %u, func = %02X, subFunc = %02X", fneHeader.getPeerId(), rtpHeader.getSSRC(), rtpHeader.getSequence(),
                fneHeader.getStreamId(), fneHeader.getFunction(), fneHeader.getSubFunction());
        }

        // is this RTP packet destined for us?
        uint32_t peerId = fneHeader.getPeerId();
        if ((m_peerId != peerId) && !m_promiscuousPeer) {
            LogError(LOG_NET, "Packet received was not destined for us? peerId = %u", peerId);
            return;
        }

        // peer connections should never encounter no stream ID
        uint32_t streamId = fneHeader.getStreamId();
        if (streamId == 0U) {
            LogWarning(LOG_NET, "BUGBUG: strange RTP packet with no stream ID?");
        }

        m_pktSeq = rtpHeader.getSequence();

        static const std::unordered_map<uint8_t, PacketHandlerFunc> handlers = {
            { NET_FUNC::PROTOCOL, &Network::PacketHandler::protocol },

            { NET_FUNC::MASTER, &Network::PacketHandler::master },

            { NET_FUNC::INCALL_CTRL, &Network::PacketHandler::inCallControl },

            { NET_FUNC::NAK, &Network::PacketHandler::nak },
            { NET_FUNC::ACK, &Network::PacketHandler::ack },

            { NET_FUNC::KEY_RSP, &Network::PacketHandler::keyResponse },
            { NET_FUNC::KEY_LLA_RSP, &Network::PacketHandler::llaKeyResponse },

            { NET_FUNC::MST_DISC, &Network::PacketHandler::masterDisconnect },

            { NET_FUNC::PONG, &Network::PacketHandler::pong }
        };

        // dispatch to the appropriate handler based on the function opcode
        auto it = handlers.find(fneHeader.getFunction());
        if (it != handlers.end()) {
            if (it->second(this, peerId, streamId, now, fneHeader, rtpHeader, buffer.get(), length)) {
                return;
            }
        }
        else {
            // if we don't have a handler for this function opcode, pass it to the user-defined packet handler (if defined)
            userPacketHandler(peerId, { fneHeader.getFunction(), fneHeader.getSubFunction() },
                buffer.get(), length, fneHeader.getStreamId(), fneHeader, rtpHeader);
        }
    }

    m_retryTimer.clock(ms);
    if (m_retryTimer.isRunning() && m_retryTimer.hasExpired()) {
        switch (m_status) {
            case NET_STAT_WAITING_LOGIN:
                LogError(LOG_NET, "PEER %u, retrying master login, remotePeerId = %u", m_peerId, m_remotePeerId);
                writeLogin();
                break;
            case NET_STAT_WAITING_AUTHORISATION:
                writeAuthorisation();
                break;
            case NET_STAT_WAITING_CONFIG:
                writeConfig();
                break;
            case NET_STAT_RUNNING:
                writePing();
                break;
            default:
                break;
        }

        m_retryTimer.start();
    }

    m_timeoutTimer.clock(ms);
    if (m_timeoutTimer.isRunning() && m_timeoutTimer.hasExpired()) {
        LogError(LOG_NET, "PEER %u connection to the master has timed out, retrying connection, remotePeerId = %u", m_peerId, m_remotePeerId);

        // fire off peer disconnected callback if we have one
        if (m_peerDisconnectedCallback != nullptr) {
            m_peerDisconnectedCallback();
        }

        close();
        open();
    }
}

/* Opens connection to the network. */

bool Network::open()
{
    if (!m_enabled)
        return false;
    if (m_debug)
        LogInfoEx(LOG_NET, "PEER %u opening network", m_peerId);

    m_status = NET_STAT_WAITING_CONNECT;

    // are we rotating IPs for HA reconnect?
    if (m_haIPs.size() > 1U && m_retryCount > 0U && !m_flaggedDuplicateConn &&
        m_maxRetryCount == MAX_RETRY_HA_RECONNECT) {

        if (m_currentHAIP >= m_haIPs.size()) {
            m_currentHAIP = 0U;
        }

        PeerHAIPEntry entry = m_haIPs[m_currentHAIP];
        m_currentHAIP++;

        LogInfoEx(LOG_NET, "PEER %u connection to the master has timed out, %s:%u is non-responsive, trying next HA %s:%u", m_peerId,
            m_address.c_str(), m_port, entry.masterAddress.c_str(), entry.masterPort);
        m_address = entry.masterAddress;
        m_port = entry.masterPort;

        LogInfoEx(LOG_NET, "PEER %u trying HA IP %s:%u", m_peerId, entry.masterAddress.c_str(), entry.masterPort);
    }

    m_timeoutTimer.start();
    m_retryTimer.start();
    m_retryCount = 0U;

    m_pingsReceived = 0U;

    if (udp::Socket::lookup(m_address, m_port, m_addr, m_addrLen) != 0) {
        LogInfoEx(LOG_NET, "!!! Could not lookup the address of the master!");
        return false;
    }

    return true;
}

/* Closes connection to the network. */

void Network::close()
{
    if (m_debug)
        LogInfoEx(LOG_NET, "PEER %u closing Network", m_peerId);

    if (m_status == NET_STAT_RUNNING) {
        uint8_t buffer[1U];
        ::memset(buffer, 0x00U, 1U);

        writeMaster({ NET_FUNC::RPT_DISC, NET_SUBFUNC::NOP }, buffer, 1U, pktSeq(true), createStreamId());
    }

    m_socket->close();

    m_retryTimer.stop();
    if (m_flaggedDuplicateConn) {
        // if we were flagged a duplicate connection, increase the retry time to avoid rapid reconnect attempts
        m_retryTimer.setTimeout(DUPLICATE_CONN_RETRY_TIME);
    }
    else {
        m_retryTimer.setTimeout(DEFAULT_RETRY_TIME);
    }
    m_timeoutTimer.stop();

    m_status = NET_STAT_WAITING_CONNECT;
    m_remotePeerId = 0U;
}

/* Sets flag enabling network communication. */

void Network::enable(bool enabled)
{
    m_enabled = enabled;
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Helper to verify the given RTP sequence for the given RTP stream. */

MULTIPLEX_RET_CODE Network::verifyStream(uint16_t* lastRxSeq)
{
    MULTIPLEX_RET_CODE ret = MUX_VALID_SUCCESS;
    if (m_pktSeq == RTP_END_OF_CALL_SEQ) {
        // reset the received sequence back to 0
        m_pktLastSeq = 0U;
    }
    else {
        *lastRxSeq = m_pktLastSeq;

        if ((m_pktSeq >= m_pktLastSeq) || (m_pktSeq == 0U)) {
            // if the sequence isn't 0, and is greater then the last received sequence + 1 frame
            // assume a packet was lost
            if ((m_pktSeq != 0U) && m_pktSeq > m_pktLastSeq + 1U) {
                ret = MUX_LOST_FRAMES;
            }

            m_pktLastSeq = m_pktSeq;
        }
        else {
            if (m_pktSeq < m_pktLastSeq) {
                ret = MUX_OUT_OF_ORDER;
            }
        }
    }

    m_pktLastSeq = m_pktSeq;
    return ret;
}

/* User overrideable handler that allows user code to process network packets not handled by this class. */

void Network::userPacketHandler(uint32_t peerId, FrameQueue::OpcodePair opcode, const uint8_t* data, uint32_t length, uint32_t streamId,
    const frame::RTPFNEHeader& fneHeader, const frame::RTPHeader& rtpHeader)
{
    Utils::dump("Unknown opcode from the master", data, length);
}

/* User overrideable handler that allows user code to process NAKs received from the master. */

bool Network::userNakHandler(uint32_t peerId, uint16_t reason, const frame::RTPFNEHeader& fneHeader, const frame::RTPHeader& rtpHeader)
{
    return false; // return false to perform default handling of the NAK
}

/* Writes login request to the network. */

bool Network::writeLogin()
{
    if (!m_enabled) {
        return false;
    }

    // reset retry timer default timeout
    if (m_retryTimer.isRunning())
        m_retryTimer.stop();
    m_retryTimer.setTimeout(DEFAULT_RETRY_TIME);

    uint8_t buffer[8U];
    ::memcpy(buffer + 0U, TAG_REPEATER_LOGIN, 4U);
    SET_UINT32(m_peerId, buffer, 4U);                                               // Peer ID

    if (m_packetDump)
        Utils::dump(1U, "Network::writeLogin(), Message, Login", buffer, 8U);

    m_loginStreamId = createStreamId();
    m_remotePeerId = 0U;
    return writeMaster({ NET_FUNC::RPTL, NET_SUBFUNC::NOP }, buffer, 8U, pktSeq(true), m_loginStreamId);
}

/* Writes network authentication challenge. */

bool Network::writeAuthorisation()
{
    if (m_loginStreamId == 0U) {
        LogWarning(LOG_NET, "BUGBUG: tried to write network authorisation with no stream ID?");
        return false;
    }

    size_t size = m_password.size();

    uint8_t* in = new uint8_t[size + sizeof(uint32_t)];
    ::memcpy(in, m_salt, sizeof(uint32_t));
    for (size_t i = 0U; i < size; i++)
        in[i + sizeof(uint32_t)] = m_password.at(i);

    uint8_t out[40U];
    ::memcpy(out + 0U, TAG_REPEATER_AUTH, 4U);
    SET_UINT32(m_peerId, out, 4U);                                                  // Peer ID

    edac::SHA256 sha256;
    sha256.buffer(in, (uint32_t)(size + sizeof(uint32_t)), out + 8U);

    delete[] in;

    if (m_packetDump)
        Utils::dump(1U, "Network::writeAuthorisation(), Message, Authorisation", out, 40U);

    return writeMaster({ NET_FUNC::RPTK, NET_SUBFUNC::NOP }, out, 40U, pktSeq(), m_loginStreamId);
}

/* Writes modem configuration to the network. */

bool Network::writeConfig()
{
    if (m_loginStreamId == 0U) {
        LogWarning(LOG_NET, "BUGBUG: tried to write network authorisation with no stream ID?");
        return false;
    }

    const char* software = __NETVER__;

    json::object config = json::object();

    // identity and frequency
    config["identity"].set<std::string>(m_metadata->identity);                      // Identity
    config["rxFrequency"].set<uint32_t>(m_metadata->rxFrequency);                   // Rx Frequency
    config["txFrequency"].set<uint32_t>(m_metadata->txFrequency);                   // Tx Frequency

    // system info
    json::object sysInfo = json::object();
    sysInfo["latitude"].set<float>(m_metadata->latitude);                           // Latitude
    sysInfo["longitude"].set<float>(m_metadata->longitude);                         // Longitude

    sysInfo["height"].set<int>(m_metadata->height);                                 // Height
    sysInfo["location"].set<std::string>(m_metadata->location);                     // Location
    config["info"].set<json::object>(sysInfo);

    // channel data
    json::object channel = json::object();
    channel["txPower"].set<uint32_t>(m_metadata->power);                            // Tx Power
    channel["txOffsetMhz"].set<float>(m_metadata->txOffsetMhz);                     // Tx Offset (Mhz)
    channel["chBandwidthKhz"].set<float>(m_metadata->chBandwidthKhz);               // Ch. Bandwidth (khz)
    channel["channelId"].set<uint8_t>(m_metadata->channelId);                       // Channel ID
    channel["channelNo"].set<uint32_t>(m_metadata->channelNo);                      // Channel No
    config["channel"].set<json::object>(channel);

    // RCON
    json::object rcon = json::object();
    rcon["password"].set<std::string>(m_metadata->restApiPassword);                 // REST API Password
    rcon["port"].set<uint16_t>(m_metadata->restApiPort);                            // REST API Port
    config["rcon"].set<json::object>(rcon);

    uint32_t peerClass = PEER_CONN_CLASS::PEER_CONN_CLASS_STANDARD;
    config["peerClass"].set<uint32_t>(peerClass);                                   // Peer Connection Class

    // Flags
    config["conventionalPeer"].set<bool>(m_metadata->isConventional);               // Conventional Peer Marker

    config["software"].set<std::string>(std::string(software));

    json::value v = json::value(config);
    std::string json = v.serialize();

    DECLARE_CHAR_ARRAY(buffer, json.length() + 9U);

    ::memcpy(buffer + 0U, TAG_REPEATER_CONFIG, 4U);
    ::snprintf(buffer + 8U, json.length() + 1U, "%s", json.c_str());

    if (m_packetDump) {
        Utils::dump(1U, "Network::writeConfig(), Message, Configuration", (uint8_t*)buffer, json.length() + 8U);
    }

    return writeMaster({ NET_FUNC::RPTC, NET_SUBFUNC::NOP }, (uint8_t*)buffer, json.length() + 8U, RTP_END_OF_CALL_SEQ, m_loginStreamId);
}

/* Writes a network stay-alive ping. */

bool Network::writePing()
{
    uint8_t buffer[1U];
    ::memset(buffer, 0x00U, 1U);

    if (m_packetDump)
        Utils::dump(1U, "Network Message, Ping", buffer, 11U);

    return writeMaster({ NET_FUNC::PING, NET_SUBFUNC::NOP }, buffer, 1U, RTP_END_OF_CALL_SEQ, createStreamId());
}
