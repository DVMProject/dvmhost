// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / DFSI peer application
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Patrick McDonnell, W3AXL
*
*/
#include "dfsi/Defines.h"
#include "common/network/json/json.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/p25/dfsi/LC.h"
#include "common/Utils.h"
#include "network/DfsiPeerNetwork.h"

using namespace network;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the PeerNetwork class.
/// </summary>
/// <param name="address">Network Hostname/IP address to connect to.</param>
/// <param name="port">Network port number.</param>
/// <param name="local"></param>
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
DfsiPeerNetwork::DfsiPeerNetwork(const std::string& address, uint16_t port, uint16_t localPort, uint32_t peerId, const std::string& password,
    bool duplex, bool debug, bool dmr, bool p25, bool nxdn, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, bool updateLookup, bool saveLookup) :
    Network(address, port, localPort, peerId, password, duplex, debug, dmr, p25, nxdn, slot1, slot2, allowActivityTransfer, allowDiagnosticTransfer, updateLookup, saveLookup)
{
    assert(!address.empty());
    assert(port > 0U);
    assert(!password.empty());
}

/// <summary>
/// Writes P25 LDU1 frame data to the network.
/// </summary>
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <param name="data"></param>
/// <param name="frameType"></param>
/// <returns></returns>
bool DfsiPeerNetwork::writeP25LDU1(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t* data, uint8_t frameType)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    bool resetSeq = false;
    if (m_p25StreamId == 0U) {
        resetSeq = true;
        m_p25StreamId = createStreamId();
    }

    uint32_t messageLength = 0U;
    UInt8Array message = createP25_LDU1Message_Raw(messageLength, control, lsd, data, frameType);
    if (message == nullptr) {
        return false;
    }

    return writeMaster({ NET_FUNC_PROTOCOL, NET_PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, pktSeq(resetSeq), m_p25StreamId);
}

/// <summary>
/// Writes P25 LDU2 frame data to the network.
/// </summary>
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <param name="data"></param>
/// <returns></returns>
bool DfsiPeerNetwork::writeP25LDU2(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t* data)
{
    if (m_status != NET_STAT_RUNNING && m_status != NET_STAT_MST_RUNNING)
        return false;

    bool resetSeq = false;
    if (m_p25StreamId == 0U) {
        resetSeq = true;
        m_p25StreamId = createStreamId();
    }

    uint32_t messageLength = 0U;
    UInt8Array message = createP25_LDU2Message_Raw(messageLength, control, lsd, data);
    if (message == nullptr) {
        return false;
    }

    return writeMaster({ NET_FUNC_PROTOCOL, NET_PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, pktSeq(resetSeq), m_p25StreamId);
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Writes configuration to the network.
/// </summary>
/// <returns></returns>
bool DfsiPeerNetwork::writeConfig()
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

    config["software"].set<std::string>(std::string(software));                 // Software ID

    json::value v = json::value(config);
    std::string json = v.serialize();

    char buffer[json.length() + 8U];

    ::memcpy(buffer + 0U, TAG_REPEATER_CONFIG, 4U);
    ::sprintf(buffer + 8U, "%s", json.c_str());

    if (m_debug) {
        Utils::dump(1U, "Network Message, Configuration", (uint8_t*)buffer, json.length() + 8U);
    }

    return writeMaster({ NET_FUNC_RPTC, NET_SUBFUNC_NOP }, (uint8_t*)buffer, json.length() + 8U, pktSeq(), m_loginStreamId);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Creates an P25 LDU1 frame message.
/// </summary>
/// <param name="length"></param>
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <param name="data"></param>
/// <param name="frameType"></param>
/// <returns></returns>
UInt8Array DfsiPeerNetwork::createP25_LDU1Message_Raw(uint32_t& length, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, 
    const uint8_t* data, uint8_t frameType)
{
    assert(data != nullptr);

    p25::dfsi::LC dfsiLC = p25::dfsi::LC(control, lsd);

    uint8_t* buffer = new uint8_t[P25_LDU1_PACKET_LENGTH + PACKET_PAD];
    ::memset(buffer, 0x00U, P25_LDU1_PACKET_LENGTH + PACKET_PAD);

    // construct P25 message header
    createP25_MessageHdr(buffer, p25::P25_DUID_LDU1, control, lsd, frameType);

    // pack DFSI data
    uint32_t count = MSG_HDR_SIZE;
    uint8_t imbe[p25::P25_RAW_IMBE_LENGTH_BYTES];

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE1);
    ::memcpy(imbe, data + 10U, p25::P25_RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU1(buffer + 24U, imbe);
    count += p25::dfsi::P25_DFSI_LDU1_VOICE1_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE2);
    ::memcpy(imbe, data + 26U, p25::P25_RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU1(buffer + 46U, imbe);
    count += p25::dfsi::P25_DFSI_LDU1_VOICE2_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE3);
    ::memcpy(imbe, data + 55U, p25::P25_RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU1(buffer + 60U, imbe);
    count += p25::dfsi::P25_DFSI_LDU1_VOICE3_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE4);
    ::memcpy(imbe, data + 80U, p25::P25_RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU1(buffer + 77U, imbe);
    count += p25::dfsi::P25_DFSI_LDU1_VOICE4_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE5);
    ::memcpy(imbe, data + 105U, p25::P25_RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU1(buffer + 94U, imbe);
    count += p25::dfsi::P25_DFSI_LDU1_VOICE5_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE6);
    ::memcpy(imbe, data + 130U, p25::P25_RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU1(buffer + 111U, imbe);
    count += p25::dfsi::P25_DFSI_LDU1_VOICE6_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE7);
    ::memcpy(imbe, data + 155U, p25::P25_RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU1(buffer + 128U, imbe);
    count += p25::dfsi::P25_DFSI_LDU1_VOICE7_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE8);
    ::memcpy(imbe, data + 180U, p25::P25_RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU1(buffer + 145U, imbe);
    count += p25::dfsi::P25_DFSI_LDU1_VOICE8_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU1_VOICE9);
    ::memcpy(imbe, data + 204U, p25::P25_RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU1(buffer + 162U, imbe);
    count += p25::dfsi::P25_DFSI_LDU1_VOICE9_FRAME_LENGTH_BYTES;

    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "Network Message, P25 LDU1", buffer, (P25_LDU1_PACKET_LENGTH + PACKET_PAD));

    length = (P25_LDU1_PACKET_LENGTH + PACKET_PAD);
    return UInt8Array(buffer);
}

/// <summary>
/// Creates an P25 LDU2 frame message.
/// </summary>
/// <param name="length"></param>
/// <param name="control"></param>
/// <param name="lsd"></param>
/// <param name="data"></param>
/// <returns></returns>
UInt8Array DfsiPeerNetwork::createP25_LDU2Message_Raw(uint32_t& length, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, 
    const uint8_t* data)
{
    assert(data != nullptr);

    p25::dfsi::LC dfsiLC = p25::dfsi::LC(control, lsd);

    uint8_t* buffer = new uint8_t[P25_LDU2_PACKET_LENGTH + PACKET_PAD];
    ::memset(buffer, 0x00U, P25_LDU2_PACKET_LENGTH + PACKET_PAD);

    // construct P25 message header
    createP25_MessageHdr(buffer, p25::P25_DUID_LDU2, control, lsd, p25::P25_FT_DATA_UNIT);

    // pack DFSI data
    uint32_t count = MSG_HDR_SIZE;
    uint8_t imbe[p25::P25_RAW_IMBE_LENGTH_BYTES];

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE10);
    ::memcpy(imbe, data + 10U, p25::P25_RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU2(buffer + 24U, imbe);
    count += p25::dfsi::P25_DFSI_LDU2_VOICE10_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE11);
    ::memcpy(imbe, data + 26U, p25::P25_RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU2(buffer + 46U, imbe);
    count += p25::dfsi::P25_DFSI_LDU2_VOICE11_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE12);
    ::memcpy(imbe, data + 55U, p25::P25_RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU2(buffer + 60U, imbe);
    count += p25::dfsi::P25_DFSI_LDU2_VOICE12_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE13);
    ::memcpy(imbe, data + 80U, p25::P25_RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU2(buffer + 77U, imbe);
    count += p25::dfsi::P25_DFSI_LDU2_VOICE13_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE14);
    ::memcpy(imbe, data + 105U, p25::P25_RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU2(buffer + 94U, imbe);
    count += p25::dfsi::P25_DFSI_LDU2_VOICE14_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE15);
    ::memcpy(imbe, data + 130U, p25::P25_RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU2(buffer + 111U, imbe);
    count += p25::dfsi::P25_DFSI_LDU2_VOICE15_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE16);
    ::memcpy(imbe, data + 155U, p25::P25_RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU2(buffer + 128U, imbe);
    count += p25::dfsi::P25_DFSI_LDU2_VOICE16_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE17);
    ::memcpy(imbe, data + 180U, p25::P25_RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU2(buffer + 145U, imbe);
    count += p25::dfsi::P25_DFSI_LDU2_VOICE17_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(p25::dfsi::P25_DFSI_LDU2_VOICE18);
    ::memcpy(imbe, data + 204U, p25::P25_RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU2(buffer + 162U, imbe);
    count += p25::dfsi::P25_DFSI_LDU2_VOICE18_FRAME_LENGTH_BYTES;

    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "Network Message, P25 LDU2", buffer, (P25_LDU2_PACKET_LENGTH + PACKET_PAD));

    length = (P25_LDU2_PACKET_LENGTH + PACKET_PAD);
    return UInt8Array(buffer);
}