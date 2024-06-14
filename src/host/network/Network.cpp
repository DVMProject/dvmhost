// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Modem Host Software
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
*   Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "common/edac/SHA256.h"
#include "common/network/RTPHeader.h"
#include "common/network/RTPFNEHeader.h"
#include "common/network/json/json.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "network/Network.h"

using namespace network;

#include <cstdio>
#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the Network class.
/// </summary>
/// <param name="address">Network Hostname/IP address to connect to.</param>
/// <param name="port">Network port number.</param>
/// <param name="localPort"></param>
/// <param name="peerId">Unique ID on the network.</param>
/// <param name="password">Network authentication password.</param>
/// <param name="duplex">Flag indicating full-duplex operation.</param>
/// <param name="debug">Flag indicating whether network debug is enabled.</param>
/// <param name="dmr">Flag indicating whether DMR is enabled.</param>
/// <param name="p25">Flag indicating whether P25 is enabled.</param>
/// <param name="nxdn">Flag indicating whether NXDN is enabled.</param>
/// <param name="slot1">Flag indicating whether DMR slot 1 is enabled for network traffic.</param>
/// <param name="slot2">Flag indicating whether DMR slot 2 is enabled for network traffic.</param>
/// <param name="allowActivityTransfer">Flag indicating that the system activity logs will be sent to the network.</param>
/// <param name="allowDiagnosticTransfer">Flag indicating that the system diagnostic logs will be sent to the network.</param>
/// <param name="updateLookup">Flag indicating that the system will accept radio ID and talkgroup ID lookups from the network.</param>
Network::Network(const std::string& address, uint16_t port, uint16_t localPort, uint32_t peerId, const std::string& password,
    bool duplex, bool debug, bool dmr, bool p25, bool nxdn, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, bool updateLookup, bool saveLookup) :
    BaseNetwork(peerId, duplex, debug, slot1, slot2, allowActivityTransfer, allowDiagnosticTransfer, localPort),
    m_pktLastSeq(0U),
    m_address(address),
    m_port(port),
    m_password(password),
    m_enabled(false),
    m_dmrEnabled(dmr),
    m_p25Enabled(p25),
    m_nxdnEnabled(nxdn),
    m_updateLookup(updateLookup),
    m_saveLookup(saveLookup),
    m_ridLookup(nullptr),
    m_tidLookup(nullptr),
    m_salt(nullptr),
    m_retryTimer(1000U, 10U),
    m_timeoutTimer(1000U, 60U),
    m_pktSeq(0U),
    m_loginStreamId(0U),
    m_identity(),
    m_rxFrequency(0U),
    m_txFrequency(0U),
    m_txOffsetMhz(0.0F),
    m_chBandwidthKhz(0.0F),
    m_channelId(0U),
    m_channelNo(0U),
    m_power(0U),
    m_latitude(0.0F),
    m_longitude(0.0F),
    m_height(0),
    m_location(),
    m_restApiPassword(),
    m_restApiPort(0),
    m_conventional(false),
    m_remotePeerId(0U)
{
    assert(!address.empty());
    assert(port > 0U);
    assert(!password.empty());

    m_salt = new uint8_t[sizeof(uint32_t)];

    m_rxDMRStreamId = new uint32_t[2U];
    m_rxDMRStreamId[0U] = 0U;
    m_rxDMRStreamId[1U] = 0U;
    m_rxP25StreamId = 0U;
    m_rxNXDNStreamId = 0U;
}

/// <summary>
/// Finalizes a instance of the Network class.
/// </summary>
Network::~Network()
{
    delete[] m_salt;
    delete[] m_rxDMRStreamId;
}

/// <summary>
/// Resets the DMR ring buffer for the given slot.
/// </summary>
/// <param name="slotNo">DMR slot ring buffer to reset.</param>
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
}

/// <summary>
/// Resets the P25 ring buffer.
/// </summary>
void Network::resetP25()
{
    BaseNetwork::resetP25();
    m_rxP25StreamId = 0U;
}

/// <summary>
/// Resets the NXDN ring buffer.
/// </summary>
void Network::resetNXDN()
{
    BaseNetwork::resetNXDN();
    m_rxNXDNStreamId = 0U;
}

/// <summary>
/// Sets the instances of the Radio ID and Talkgroup ID lookup tables.
/// </summary>
/// <param name="ridLookup">Radio ID Lookup Table Instance</param>
/// <param name="tidLookup">Talkgroup Rules Lookup Table Instance</param>
void Network::setLookups(lookups::RadioIdLookup* ridLookup, lookups::TalkgroupRulesLookup* tidLookup)
{
    m_ridLookup = ridLookup;
    m_tidLookup = tidLookup;
}

/// <summary>
/// Sets metadata configuration settings from the modem.
/// </summary>
/// <param name="identity"></param>
/// <param name="rxFrequency"></param>
/// <param name="txFrequency"></param>
/// <param name="txOffsetMhz"></param>
/// <param name="chBandwidthKhz"></param>
/// <param name="channelId"></param>
/// <param name="channelNo"></param>
/// <param name="power"></param>
/// <param name="latitude"></param>
/// <param name="longitude"></param>
/// <param name="height"></param>
/// <param name="location"></param>
void Network::setMetadata(const std::string& identity, uint32_t rxFrequency, uint32_t txFrequency, float txOffsetMhz, float chBandwidthKhz,
    uint8_t channelId, uint32_t channelNo, uint32_t power, float latitude, float longitude, int height, const std::string& location)
{
    m_identity = identity;
    m_rxFrequency = rxFrequency;
    m_txFrequency = txFrequency;

    m_txOffsetMhz = txOffsetMhz;
    m_chBandwidthKhz = chBandwidthKhz;
    m_channelId = channelId;
    m_channelNo = channelNo;

    m_power = power;
    m_latitude = latitude;
    m_longitude = longitude;
    m_height = height;
    m_location = location;
}

/// <summary>
/// Sets REST API configuration settings from the modem.
/// </summary>
/// <param name="password"></param>
/// <param name="port"></param>
void Network::setRESTAPIData(const std::string& password, uint16_t port)
{
    m_restApiPassword = password;
    m_restApiPort = port;
}

/// <summary>
/// Sets endpoint preshared encryption key.
/// </summary>
void Network::setPresharedKey(const uint8_t* presharedKey)
{
    m_socket->setPresharedKey(presharedKey);
}

/// <summary>
/// Updates the timer by the passed number of milliseconds.
/// </summary>
/// <param name="ms"></param>
void Network::clock(uint32_t ms)
{
    if (m_status == NET_STAT_WAITING_CONNECT) {
        m_retryTimer.clock(ms);
        if (m_retryTimer.isRunning() && m_retryTimer.hasExpired()) {
            if (m_enabled) {
                bool ret = m_socket->open(m_addr.ss_family);
                if (ret) {
                    ret = writeLogin();
                    if (!ret) {
                        m_retryTimer.start();
                        return;
                    }

                    m_status = NET_STAT_WAITING_LOGIN;
                    m_timeoutTimer.start();
                }
            }

            m_retryTimer.start();
        }

        return;
    }

    // if we aren't enabled -- bail
    if (!m_enabled) {
        return;
    }

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
            LogDebug(LOG_NET, "RTP, peerId = %u, seq = %u, streamId = %u, func = %02X, subFunc = %02X", fneHeader.getPeerId(), rtpHeader.getSequence(),
                fneHeader.getStreamId(), fneHeader.getFunction(), fneHeader.getSubFunction());
        }

        // ensure the RTP synchronization source ID matches the FNE peer ID
        if (m_remotePeerId != 0U && rtpHeader.getSSRC() != m_remotePeerId) {
            LogWarning(LOG_NET, "RTP header and traffic session do not agree on remote peer ID? %u != %u", rtpHeader.getSSRC(), m_remotePeerId);
            // should this be a fatal error?
        }

        // is this RTP packet destined for us?
        uint32_t peerId = fneHeader.getPeerId();
        if (m_peerId != peerId) {
            LogError(LOG_NET, "Packet received was not destined for us? peerId = %u", peerId);
            return;
        }

        // peer connections should never encounter no stream ID
        uint32_t streamId = fneHeader.getStreamId();
        if (streamId == 0U) {
            LogWarning(LOG_NET, "BUGBUG: strange RTP packet with no stream ID?");
        }

        m_pktSeq = rtpHeader.getSequence();
        
        if (m_pktSeq == RTP_END_OF_CALL_SEQ) {
            m_pktSeq = 0U;
            m_pktLastSeq = 0U;
        }

        // process incoming message frame opcodes
        switch (fneHeader.getFunction()) {
        case NET_FUNC_PROTOCOL:
            {
                if (fneHeader.getSubFunction() == NET_PROTOCOL_SUBFUNC_DMR) {           // Encapsulated DMR data frame
                    if (m_enabled && m_dmrEnabled) {
                        uint32_t slotNo = (buffer[15U] & 0x80U) == 0x80U ? 2U : 1U;
                        if (m_rxDMRStreamId[slotNo] == 0U) {
                            m_rxDMRStreamId[slotNo] = streamId;
                            m_pktLastSeq = m_pktSeq;
                        }
                        else {
                            if (m_rxDMRStreamId[slotNo] == streamId) {
                                if (m_pktSeq != 0U && m_pktLastSeq != 0U) {
                                    if (m_pktSeq >= 1U && ((m_pktSeq != m_pktLastSeq + 1) && (m_pktSeq - 1 != m_pktLastSeq + 1))) {
                                        LogWarning(LOG_NET, "DMR Stream %u out-of-sequence; %u != %u", streamId, m_pktSeq, m_pktLastSeq + 1);
                                    }
                                }
        
                                m_pktLastSeq = m_pktSeq;
                            }
                        }
                       
                        if (m_debug)
                            Utils::dump(1U, "Network Received, DMR", buffer.get(), length);

                        uint8_t len = length;
                        m_rxDMRData.addData(&len, 1U);
                        m_rxDMRData.addData(buffer.get(), len);
                    }
                }
                else if (fneHeader.getSubFunction() == NET_PROTOCOL_SUBFUNC_P25) {      // Encapsulated P25 data frame
                    if (m_enabled && m_p25Enabled) {
                        if (m_rxP25StreamId == 0U) {
                            m_rxP25StreamId = streamId;
                            m_pktLastSeq = m_pktSeq;
                        }
                        else {
                            if (m_rxP25StreamId == streamId) {
                                if (m_pktSeq != 0U && m_pktLastSeq != 0U) {
                                    if (m_pktSeq >= 1U && ((m_pktSeq != m_pktLastSeq + 1) && (m_pktSeq - 1 != m_pktLastSeq + 1))) {
                                        LogWarning(LOG_NET, "P25 Stream %u out-of-sequence; %u != %u", streamId, m_pktSeq, m_pktLastSeq + 1);
                                    }
                                }
        
                                m_pktLastSeq = m_pktSeq;
                            }
                        }

                        if (m_debug)
                            Utils::dump(1U, "Network Received, P25", buffer.get(), length);

                        uint8_t len = length;
                        m_rxP25Data.addData(&len, 1U);
                        m_rxP25Data.addData(buffer.get(), len);
                    }
                }
                else if (fneHeader.getSubFunction() == NET_PROTOCOL_SUBFUNC_NXDN) {     // Encapsulated NXDN data frame
                    if (m_enabled && m_nxdnEnabled) {
                        if (m_rxNXDNStreamId == 0U) {
                            m_rxNXDNStreamId = streamId;
                            m_pktLastSeq = m_pktSeq;
                        }
                        else {
                            if (m_rxNXDNStreamId == streamId) {
                                if (m_pktSeq != 0U && m_pktLastSeq != 0U) {
                                    if (m_pktSeq >= 1U && ((m_pktSeq != m_pktLastSeq + 1) && (m_pktSeq - 1 != m_pktLastSeq + 1))) {
                                        LogWarning(LOG_NET, "NXDN Stream %u out-of-sequence; %u != %u", streamId, m_pktSeq, m_pktLastSeq + 1);
                                    }
                                }
        
                                m_pktLastSeq = m_pktSeq;
                            }
                        }

                        if (m_debug)
                            Utils::dump(1U, "Network Received, NXDN", buffer.get(), length);

                        uint8_t len = length;
                        m_rxNXDNData.addData(&len, 1U);
                        m_rxNXDNData.addData(buffer.get(), len);
                    }
                }
                else {
                    Utils::dump("unknown protocol opcode from the master", buffer.get(), length);
                }
            }
            break;

        case NET_FUNC_MASTER:
            {
                if (fneHeader.getSubFunction() == NET_MASTER_SUBFUNC_WL_RID) {          // Radio ID Whitelist
                    if (m_enabled && m_updateLookup) {
                        if (m_debug)
                            Utils::dump(1U, "Network Received, WL RID", buffer.get(), length);

                        if (m_ridLookup != nullptr) {
                            // update RID lists
                            uint32_t len = __GET_UINT32(buffer, 6U);
                            uint32_t offs = 11U;
                            for (uint32_t i = 0; i < len; i++) {
                                uint32_t id = __GET_UINT16(buffer, offs);
                                m_ridLookup->toggleEntry(id, true);
                                offs += 4U;
                            }

                            LogMessage(LOG_NET, "Network Announced %u whitelisted RIDs", len);

                            // save to file if enabled and we got RIDs
                            if (m_saveLookup && len > 0) {
                                m_ridLookup->commit();
                            }
                        }
                    }
                }
                else if (fneHeader.getSubFunction() == NET_MASTER_SUBFUNC_BL_RID) {     // Radio ID Blacklist
                    if (m_enabled && m_updateLookup) {
                        if (m_debug)
                            Utils::dump(1U, "Network Received, BL RID", buffer.get(), length);

                        if (m_ridLookup != nullptr) {
                            // update RID lists
                            uint32_t len = __GET_UINT32(buffer, 6U);
                            uint32_t offs = 11U;
                            for (uint32_t i = 0; i < len; i++) {
                                uint32_t id = __GET_UINT16(buffer, offs);
                                m_ridLookup->toggleEntry(id, false);
                                offs += 4U;
                            }

                            LogMessage(LOG_NET, "Network Announced %u blacklisted RIDs", len);

                            // save to file if enabled and we got RIDs
                            if (m_saveLookup && len > 0) {
                                m_ridLookup->commit();
                            }
                        }
                    }
                }
                else if (fneHeader.getSubFunction() == NET_MASTER_SUBFUNC_ACTIVE_TGS) { // Talkgroup Active IDs
                    if (m_enabled && m_updateLookup) {
                        if (m_debug)
                            Utils::dump(1U, "Network Received, ACTIVE TGS", buffer.get(), length);

                        if (m_tidLookup != nullptr) {
                            // update TGID lists
                            uint32_t len = __GET_UINT32(buffer, 6U);
                            uint32_t offs = 11U;
                            for (uint32_t i = 0; i < len; i++) {
                                uint32_t id = __GET_UINT16(buffer, offs);
                                uint8_t slot = (buffer[offs + 3U]) & 0x03U;
                                bool nonPreferred = (buffer[offs + 3U] & 0x80U) == 0x80U;

                                lookups::TalkgroupRuleGroupVoice tid = m_tidLookup->find(id, slot);

                                // if the TG is marked as non-preferred, and the TGID exists in the local entries
                                // erase the local and overwrite with the FNE data
                                if (nonPreferred) {
                                    if (!tid.isInvalid()) {
                                        m_tidLookup->eraseEntry(id, slot);
                                        tid = m_tidLookup->find(id, slot);
                                    }
                                }

                                if (tid.isInvalid()) {
                                    if (!tid.config().active()) {
                                        m_tidLookup->eraseEntry(id, slot);
                                    }
                                    
                                    LogMessage(LOG_NET, "Activated%s TG %u TS %u in TGID table", (nonPreferred) ? " non-preferred" : "", id, slot);
                                    m_tidLookup->addEntry(id, slot, true, nonPreferred);
                                }

                                offs += 5U;
                            }

                            LogMessage(LOG_NET, "Activated %u TGs; loaded %u entries into lookup table", len, m_tidLookup->groupVoice().size());

                            // save if saving from network is enabled
                            if (m_saveLookup && len > 0) {
                                m_tidLookup->commit();
                            }
                        }
                    }
                }
                else if (fneHeader.getSubFunction() == NET_MASTER_SUBFUNC_DEACTIVE_TGS) { // Talkgroup Deactivated IDs
                    if (m_enabled && m_updateLookup) {
                        if (m_debug)
                            Utils::dump(1U, "Network Received, DEACTIVE TGS", buffer.get(), length);

                        if (m_tidLookup != nullptr) {
                            // update TGID lists
                            uint32_t len = __GET_UINT32(buffer, 6U);
                            uint32_t offs = 11U;
                            for (uint32_t i = 0; i < len; i++) {
                                uint32_t id = __GET_UINT16(buffer, offs);
                                uint8_t slot = (buffer[offs + 3U]);

                                lookups::TalkgroupRuleGroupVoice tid = m_tidLookup->find(id, slot);
                                if (!tid.isInvalid()) {
                                    LogMessage(LOG_NET, "Deactivated TG %u TS %u in TGID table", id, slot);
                                    m_tidLookup->eraseEntry(id, slot);
                                }

                                offs += 5U;
                            }

                            LogMessage(LOG_NET, "Deactivated %u TGs; loaded %u entries into lookup table", len, m_tidLookup->groupVoice().size());

                            // save if saving from network is enabled
                            if (m_saveLookup && len > 0) {
                                m_tidLookup->commit();
                            }
                        }
                    }
                }
                else {
                    Utils::dump("unknown master control opcode from the master", buffer.get(), length);
                }
            }
            break;

        case NET_FUNC_NAK:                                                              // Master Negative Ack
            {
                // DVM 3.6 adds support to respond with a NAK reason, as such we just check if the NAK response is greater
                // then 10 bytes and process the reason value
                uint16_t reason = 0U;
                if (length > 10) {
                    reason = __GET_UINT16B(buffer, 10U);
                    switch (reason) {
                    case NET_CONN_NAK_MODE_NOT_ENABLED:
                        LogWarning(LOG_NET, "PEER %u master NAK; digital mode not enabled on FNE, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                        break;
                    case NET_CONN_NAK_ILLEGAL_PACKET:
                        LogWarning(LOG_NET, "PEER %u master NAK; illegal/unknown packet, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                        break;
                    case NET_CONN_NAK_FNE_UNAUTHORIZED:
                        LogWarning(LOG_NET, "PEER %u master NAK; unauthorized, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                        break;
                    case NET_CONN_NAK_BAD_CONN_STATE:
                        LogWarning(LOG_NET, "PEER %u master NAK; bad connection state, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                        break;
                    case NET_CONN_NAK_INVALID_CONFIG_DATA:
                        LogWarning(LOG_NET, "PEER %u master NAK; invalid configuration data, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                        break;
                    case NET_CONN_NAK_FNE_MAX_CONN:
                        LogWarning(LOG_NET, "PEER %u master NAK; FNE has reached maximum permitted connections, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                        break;
                    case NET_CONN_NAK_PEER_RESET:
                        LogWarning(LOG_NET, "PEER %u master NAK; FNE Called for a connection reset, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                        break;
                    case NET_CONN_NAK_PEER_ACL:
                        LogWarning(LOG_NET, "PEER %u master NAK; ACL Rejection, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                        break;

                    case NET_CONN_NAK_GENERAL_FAILURE:
                    default:
                        LogWarning(LOG_NET, "PEER %u master NAK; general failure, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                        break;
                    }
                }

                if (m_status == NET_STAT_RUNNING || (reason == NET_CONN_NAK_FNE_MAX_CONN)) {
                    LogWarning(LOG_NET, "PEER %u master NAK; attemping to relogin, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                    m_status = NET_STAT_WAITING_LOGIN;
                    m_timeoutTimer.start();
                    m_retryTimer.start();
                }
                else {
                    LogError(LOG_NET, "PEER %u master NAK; network reconnect, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                    close();
                    open();
                    return;
                }
            }
            break;
        case NET_FUNC_ACK:                                                              // Repeater Ack
            {
                switch (m_status) {
                    case NET_STAT_WAITING_LOGIN:
                        LogDebug(LOG_NET, "PEER %u RPTL ACK, performing login exchange, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());

                        ::memcpy(m_salt, buffer.get() + 6U, sizeof(uint32_t));
                        writeAuthorisation();

                        m_status = NET_STAT_WAITING_AUTHORISATION;
                        m_timeoutTimer.start();
                        m_retryTimer.start();
                        break;
                    case NET_STAT_WAITING_AUTHORISATION:
                        LogDebug(LOG_NET, "PEER %u RPTK ACK, performing configuration exchange, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());

                        writeConfig();

                        m_status = NET_STAT_WAITING_CONFIG;
                        m_timeoutTimer.start();
                        m_retryTimer.start();
                        break;
                    case NET_STAT_WAITING_CONFIG:
                        LogMessage(LOG_NET, "PEER %u RPTC ACK, logged into the master successfully, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                        m_loginStreamId = 0U;
                        m_remotePeerId = rtpHeader.getSSRC();

                        pktSeq(true);

                        m_status = NET_STAT_RUNNING;
                        m_timeoutTimer.start();
                        m_retryTimer.start();

                        if (length > 6) {
                            m_useAlternatePortForDiagnostics = (buffer[6U] & 0x80U) == 0x80U;
                            if (m_useAlternatePortForDiagnostics) {
                                LogMessage(LOG_NET, "PEER %u RPTC ACK, master commanded alternate port for diagnostics and activity logging, remotePeerId = %u", m_peerId, rtpHeader.getSSRC());
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
            break;
        case NET_FUNC_MST_CLOSING:                                                      // Master Shutdown
            {
                LogError(LOG_NET, "PEER %u master is closing down, remotePeerId = %u", m_peerId, m_remotePeerId);
                close();
                open();
            }
            break;
        case NET_FUNC_PONG:                                                             // Master Ping Response
            m_timeoutTimer.start();
            break;
        default:
            Utils::dump("unknown opcode from the master", buffer.get(), length);
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
        close();
        open();
    }
}

/// <summary>
/// Opens connection to the network.
/// </summary>
/// <returns></returns>
bool Network::open()
{
    if (!m_enabled)
        return false;
    if (m_debug)
        LogMessage(LOG_NET, "PEER %u opening network", m_peerId);

    if (udp::Socket::lookup(m_address, m_port, m_addr, m_addrLen) != 0) {
        LogMessage(LOG_NET, "Could not lookup the address of the master");
        return false;
    }

    m_status = NET_STAT_WAITING_CONNECT;
    m_timeoutTimer.start();
    m_retryTimer.start();

    return true;
}

/// <summary>
/// Closes connection to the network.
/// </summary>
void Network::close()
{
    if (m_debug)
        LogMessage(LOG_NET, "PEER %u closing Network", m_peerId);

    if (m_status == NET_STAT_RUNNING) {
        uint8_t buffer[1U];
        ::memset(buffer, 0x00U, 1U);

        writeMaster({ NET_FUNC_RPT_CLOSING, NET_SUBFUNC_NOP }, buffer, 1U, pktSeq(true), createStreamId());
    }

    m_socket->close();

    m_retryTimer.stop();
    m_timeoutTimer.stop();

    m_status = NET_STAT_WAITING_CONNECT;
}

/// <summary>
/// Sets flag enabling network communication.
/// </summary>
/// <param name="enabled"></param>
void Network::enable(bool enabled)
{
    m_enabled = enabled;
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Writes login request to the network.
/// </summary>
/// <returns></returns>
bool Network::writeLogin()
{
    if (!m_enabled) {
        return false;
    }

    uint8_t buffer[8U];
    ::memcpy(buffer + 0U, TAG_REPEATER_LOGIN, 4U);
    __SET_UINT32(m_peerId, buffer, 4U);                                             // Peer ID

    if (m_debug)
        Utils::dump(1U, "Network Message, Login", buffer, 8U);

    m_loginStreamId = createStreamId();
    m_remotePeerId = 0U;
    return writeMaster({ NET_FUNC_RPTL, NET_SUBFUNC_NOP }, buffer, 8U, pktSeq(true), m_loginStreamId);
}

/// <summary>
/// Writes network authentication challenge.
/// </summary>
/// <returns></returns>
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
    __SET_UINT32(m_peerId, out, 4U);                                                // Peer ID

    edac::SHA256 sha256;
    sha256.buffer(in, (uint32_t)(size + sizeof(uint32_t)), out + 8U);

    delete[] in;

    if (m_debug)
        Utils::dump(1U, "Network Message, Authorisation", out, 40U);

    return writeMaster({ NET_FUNC_RPTK, NET_SUBFUNC_NOP }, out, 40U, pktSeq(), m_loginStreamId);
}

/// <summary>
/// Writes modem configuration to the network.
/// </summary>
/// <returns></returns>
bool Network::writeConfig()
{
    if (m_loginStreamId == 0U) {
        LogWarning(LOG_NET, "BUGBUG: tried to write network authorisation with no stream ID?");
        return false;
    }

    const char* software = __NETVER__;

    json::object config = json::object();

    // identity and frequency
    config["identity"].set<std::string>(m_identity);                                // Identity
    config["rxFrequency"].set<uint32_t>(m_rxFrequency);                             // Rx Frequency
    config["txFrequency"].set<uint32_t>(m_txFrequency);                             // Tx Frequency

    // system info
    json::object sysInfo = json::object();
    sysInfo["latitude"].set<float>(m_latitude);                                     // Latitude
    sysInfo["longitude"].set<float>(m_longitude);                                   // Longitude

    sysInfo["height"].set<int>(m_height);                                           // Height
    sysInfo["location"].set<std::string>(m_location);                               // Location
    config["info"].set<json::object>(sysInfo);

    // channel data
    json::object channel = json::object();
    channel["txPower"].set<uint32_t>(m_power);                                      // Tx Power
    channel["txOffsetMhz"].set<float>(m_txOffsetMhz);                               // Tx Offset (Mhz)
    channel["chBandwidthKhz"].set<float>(m_chBandwidthKhz);                         // Ch. Bandwidth (khz)
    channel["channelId"].set<uint8_t>(m_channelId);                                 // Channel ID
    channel["channelNo"].set<uint32_t>(m_channelNo);                                // Channel No
    config["channel"].set<json::object>(channel);

    // RCON
    json::object rcon = json::object();
    rcon["password"].set<std::string>(m_restApiPassword);                           // REST API Password
    rcon["port"].set<uint16_t>(m_restApiPort);                                      // REST API Port
    config["rcon"].set<json::object>(rcon);

    config["conventionalPeer"].set<bool>(m_conventional);                           // Conventional Peer Marker
    config["software"].set<std::string>(std::string(software));                     // Software ID

    json::value v = json::value(config);
    std::string json = v.serialize();

    char buffer[json.length() + 9U];

    ::memcpy(buffer + 0U, TAG_REPEATER_CONFIG, 4U);
    ::snprintf(buffer + 8U, json.length() + 1U, "%s", json.c_str());

    if (m_debug) {
        Utils::dump(1U, "Network Message, Configuration", (uint8_t*)buffer, json.length() + 8U);
    }

    return writeMaster({ NET_FUNC_RPTC, NET_SUBFUNC_NOP }, (uint8_t*)buffer, json.length() + 8U, RTP_END_OF_CALL_SEQ, m_loginStreamId);
}

/// <summary>
/// Writes a network stay-alive ping.
/// </summary>
bool Network::writePing()
{
    uint8_t buffer[1U];
    ::memset(buffer, 0x00U, 1U);

    if (m_debug)
        Utils::dump(1U, "Network Message, Ping", buffer, 11U);

    return writeMaster({ NET_FUNC_PING, NET_SUBFUNC_NOP }, buffer, 1U, RTP_END_OF_CALL_SEQ, createStreamId());
}
