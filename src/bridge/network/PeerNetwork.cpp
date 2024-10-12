// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Bridge
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "bridge/Defines.h"
#include "common/dmr/data/EMB.h"
#include "common/dmr/lc/FullLC.h"
#include "common/dmr/SlotType.h"
#include "common/network/json/json.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/p25/dfsi/LC.h"
#include "common/Utils.h"
#include "network/PeerNetwork.h"

using namespace network;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the PeerNetwork class. */

PeerNetwork::PeerNetwork(const std::string& address, uint16_t port, uint16_t localPort, uint32_t peerId, const std::string& password,
    bool duplex, bool debug, bool dmr, bool p25, bool nxdn, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, bool updateLookup, bool saveLookup) :
    Network(address, port, localPort, peerId, password, duplex, debug, dmr, p25, nxdn, slot1, slot2, allowActivityTransfer, allowDiagnosticTransfer, updateLookup, saveLookup)
{
    assert(!address.empty());
    assert(port > 0U);
    assert(!password.empty());
}

/* Writes P25 LDU1 frame data to the network. */

bool PeerNetwork::writeP25LDU1(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t* data, p25::defines::FrameType::E frameType)
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

    return writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, pktSeq(resetSeq), m_p25StreamId);
}

/* Writes P25 LDU2 frame data to the network. */

bool PeerNetwork::writeP25LDU2(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, const uint8_t* data)
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

    return writeMaster({ NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 }, message.get(), messageLength, pktSeq(resetSeq), m_p25StreamId);
}

/* Helper to send a DMR terminator with LC message. */

void PeerNetwork::writeDMRTerminator(dmr::data::NetData& data, uint32_t* seqNo, uint8_t* dmrN, dmr::data::EmbeddedData& embeddedData)
{
    using namespace dmr;
    using namespace dmr::defines;

    uint8_t n = (uint8_t)((*seqNo - 3U) % 6U);
    uint32_t fill = 6U - n;

    uint8_t* buffer = nullptr;
    if (n > 0U) {
        for (uint32_t i = 0U; i < fill; i++) {
            // generate DMR AMBE data
            buffer = new uint8_t[DMR_FRAME_LENGTH_BYTES];
            ::memcpy(buffer, SILENCE_DATA, DMR_FRAME_LENGTH_BYTES);

            uint8_t lcss = embeddedData.getData(buffer, n);

            // generated embedded signalling
            data::EMB emb = data::EMB();
            emb.setColorCode(0U);
            emb.setLCSS(lcss);
            emb.encode(buffer);

            // generate DMR network frame
            data.setData(buffer);

            writeDMR(data);

            seqNo++;
            dmrN++;
            delete[] buffer;
        }
    }

    buffer = new uint8_t[DMR_FRAME_LENGTH_BYTES];

    // generate DMR LC
    lc::LC dmrLC = lc::LC();
    dmrLC.setFLCO(FLCO::GROUP);
    dmrLC.setSrcId(data.getSrcId());
    dmrLC.setDstId(data.getDstId());

    // generate the Slot Type
    SlotType slotType = SlotType();
    slotType.setDataType(DataType::TERMINATOR_WITH_LC);
    slotType.encode(buffer);

    lc::FullLC fullLC = lc::FullLC();
    fullLC.encode(dmrLC, buffer, DataType::TERMINATOR_WITH_LC);

    // generate DMR network frame
    data.setData(buffer);

    writeDMR(data);

    seqNo = 0;
    dmrN = 0;
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Writes configuration to the network. */

bool PeerNetwork::writeConfig()
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

    CharArray __buffer = std::make_unique<char[]>(json.length() + 9U);
    char* buffer = __buffer.get();

    ::memcpy(buffer + 0U, TAG_REPEATER_CONFIG, 4U);
    ::snprintf(buffer + 8U, json.length() + 1U, "%s", json.c_str());

    if (m_debug) {
        Utils::dump(1U, "Network Message, Configuration", (uint8_t*)buffer, json.length() + 8U);
    }

    return writeMaster({ NET_FUNC::RPTC, NET_SUBFUNC::NOP }, (uint8_t*)buffer, json.length() + 8U, RTP_END_OF_CALL_SEQ, m_loginStreamId);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Creates an P25 LDU1 frame message. */

UInt8Array PeerNetwork::createP25_LDU1Message_Raw(uint32_t& length, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, 
    const uint8_t* data, p25::defines::FrameType::E frameType)
{
    using namespace p25::defines;
    using namespace p25::dfsi::defines;
    assert(data != nullptr);

    p25::dfsi::LC dfsiLC = p25::dfsi::LC(control, lsd);

    uint8_t* buffer = new uint8_t[P25_LDU1_PACKET_LENGTH + PACKET_PAD];
    ::memset(buffer, 0x00U, P25_LDU1_PACKET_LENGTH + PACKET_PAD);

    // construct P25 message header
    createP25_MessageHdr(buffer, DUID::LDU1, control, lsd, frameType);

    // pack DFSI data
    uint32_t count = MSG_HDR_SIZE;
    uint8_t imbe[RAW_IMBE_LENGTH_BYTES];

    dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE1);
    ::memcpy(imbe, data + 10U, RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU1(buffer + 24U, imbe);
    count += DFSI_LDU1_VOICE1_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE2);
    ::memcpy(imbe, data + 26U, RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU1(buffer + 46U, imbe);
    count += DFSI_LDU1_VOICE2_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE3);
    ::memcpy(imbe, data + 55U, RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU1(buffer + 60U, imbe);
    count += DFSI_LDU1_VOICE3_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE4);
    ::memcpy(imbe, data + 80U, RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU1(buffer + 77U, imbe);
    count += DFSI_LDU1_VOICE4_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE5);
    ::memcpy(imbe, data + 105U, RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU1(buffer + 94U, imbe);
    count += DFSI_LDU1_VOICE5_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE6);
    ::memcpy(imbe, data + 130U, RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU1(buffer + 111U, imbe);
    count += DFSI_LDU1_VOICE6_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE7);
    ::memcpy(imbe, data + 155U, RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU1(buffer + 128U, imbe);
    count += DFSI_LDU1_VOICE7_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE8);
    ::memcpy(imbe, data + 180U, RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU1(buffer + 145U, imbe);
    count += DFSI_LDU1_VOICE8_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU1_VOICE9);
    ::memcpy(imbe, data + 204U, RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU1(buffer + 162U, imbe);
    count += DFSI_LDU1_VOICE9_FRAME_LENGTH_BYTES;

    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "Network Message, P25 LDU1", buffer, (P25_LDU1_PACKET_LENGTH + PACKET_PAD));

    length = (P25_LDU1_PACKET_LENGTH + PACKET_PAD);
    return UInt8Array(buffer);
}

/* Creates an P25 LDU2 frame message. */

UInt8Array PeerNetwork::createP25_LDU2Message_Raw(uint32_t& length, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, 
    const uint8_t* data)
{
    using namespace p25::defines;
    using namespace p25::dfsi::defines;
    assert(data != nullptr);

    p25::dfsi::LC dfsiLC = p25::dfsi::LC(control, lsd);

    uint8_t* buffer = new uint8_t[P25_LDU2_PACKET_LENGTH + PACKET_PAD];
    ::memset(buffer, 0x00U, P25_LDU2_PACKET_LENGTH + PACKET_PAD);

    // construct P25 message header
    createP25_MessageHdr(buffer, DUID::LDU2, control, lsd, FrameType::DATA_UNIT);

    // pack DFSI data
    uint32_t count = MSG_HDR_SIZE;
    uint8_t imbe[RAW_IMBE_LENGTH_BYTES];

    dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE10);
    ::memcpy(imbe, data + 10U, RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU2(buffer + 24U, imbe);
    count += DFSI_LDU2_VOICE10_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE11);
    ::memcpy(imbe, data + 26U, RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU2(buffer + 46U, imbe);
    count += DFSI_LDU2_VOICE11_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE12);
    ::memcpy(imbe, data + 55U, RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU2(buffer + 60U, imbe);
    count += DFSI_LDU2_VOICE12_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE13);
    ::memcpy(imbe, data + 80U, RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU2(buffer + 77U, imbe);
    count += DFSI_LDU2_VOICE13_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE14);
    ::memcpy(imbe, data + 105U, RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU2(buffer + 94U, imbe);
    count += DFSI_LDU2_VOICE14_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE15);
    ::memcpy(imbe, data + 130U, RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU2(buffer + 111U, imbe);
    count += DFSI_LDU2_VOICE15_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE16);
    ::memcpy(imbe, data + 155U, RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU2(buffer + 128U, imbe);
    count += DFSI_LDU2_VOICE16_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE17);
    ::memcpy(imbe, data + 180U, RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU2(buffer + 145U, imbe);
    count += DFSI_LDU2_VOICE17_FRAME_LENGTH_BYTES;

    dfsiLC.setFrameType(DFSIFrameType::LDU2_VOICE18);
    ::memcpy(imbe, data + 204U, RAW_IMBE_LENGTH_BYTES);
    dfsiLC.encodeLDU2(buffer + 162U, imbe);
    count += DFSI_LDU2_VOICE18_FRAME_LENGTH_BYTES;

    buffer[23U] = count;

    if (m_debug)
        Utils::dump(1U, "Network Message, P25 LDU2", buffer, (P25_LDU2_PACKET_LENGTH + PACKET_PAD));

    length = (P25_LDU2_PACKET_LENGTH + PACKET_PAD);
    return UInt8Array(buffer);
}
