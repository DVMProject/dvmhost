// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES FROM THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/network/RPCHeader.h"
#include "common/network/RTPExtensionHeader.h"
#include "common/network/RTPFNEHeader.h"
#include "common/network/RTPHeader.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>

using namespace network;
using namespace network::frame;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief A test subclass of RTPExtensionHeader that exposes the type and length fields for 
 * testing purposes.
 */
class TestRTPExtensionHeader : public RTPExtensionHeader {
public:
    void setType(uint16_t type) { setPayloadType(type); }
    void setLength(uint16_t length) { setPayloadLength(length); }
    uint16_t type() const { return m_payloadType; }
    uint16_t length() const { return m_payloadLength; }
};

TEST_CASE("RTPHeader encodes and decodes all fields", "[network][rtp][header]")
{
    const uint8_t payloadType = 0x56U;

    RTPHeader encoded = RTPHeader();
    encoded.setExtension(true);
    encoded.setMarker(true);
    encoded.setPayloadType(payloadType);
    encoded.setSequence(0x1234U);
    encoded.setTimestamp(0x11223344U);
    encoded.setSSRC(0x55667788U);

    std::array<uint8_t, RTP_HEADER_LENGTH_BYTES> buffer = {};
    encoded.encode(buffer.data());

    RTPHeader decoded = RTPHeader();
    REQUIRE(decoded.decode(buffer.data()));

    REQUIRE(decoded.getVersion() == 2U);
    REQUIRE(decoded.getPadding() == false);
    REQUIRE(decoded.getExtension() == true);
    REQUIRE(decoded.getCSRCCount() == 0U);
    REQUIRE(decoded.getMarker() == true);
    REQUIRE(decoded.getPayloadType() == payloadType);
    REQUIRE(decoded.getSequence() == 0x1234U);
    REQUIRE(decoded.getTimestamp() == 0x11223344U);
    REQUIRE(decoded.getSSRC() == 0x55667788U);
}

TEST_CASE("RTPHeader rejects invalid RTP version", "[network][rtp][header]")
{
    std::array<uint8_t, RTP_HEADER_LENGTH_BYTES> buffer = {};
    buffer[0U] = 0x40U;

    RTPHeader decoded = RTPHeader();
    REQUIRE_FALSE(decoded.decode(buffer.data()));
}

TEST_CASE("RTPHeader generates a timestamp when unset", "[network][rtp][header]")
{
    RTPHeader header = RTPHeader();
    header.setTimestamp(INVALID_TS);
    header.setSSRC(0x01020304U);

    std::array<uint8_t, RTP_HEADER_LENGTH_BYTES> buffer = {};
    header.encode(buffer.data());

    RTPHeader decoded = RTPHeader();
    REQUIRE(decoded.decode(buffer.data()));
    REQUIRE(decoded.getTimestamp() != INVALID_TS);
    REQUIRE(decoded.getSSRC() == 0x01020304U);
}

TEST_CASE("RTPExtensionHeader encodes and decodes payload metadata", "[network][rtp][extension]")
{
    TestRTPExtensionHeader encoded = TestRTPExtensionHeader();
    encoded.setType(0xF00DU);
    encoded.setLength(0x0004U);

    std::array<uint8_t, RTP_EXTENSION_HEADER_LENGTH_BYTES> buffer = {};
    encoded.encode(buffer.data());

    TestRTPExtensionHeader decoded = TestRTPExtensionHeader();
    REQUIRE(decoded.decode(buffer.data()));
    REQUIRE(decoded.type() == 0xF00DU);
    REQUIRE(decoded.length() == 0x0004U);
}

TEST_CASE("RTPFNEHeader encodes and decodes frame metadata", "[network][rtp][fne]")
{
    RTPFNEHeader encoded = RTPFNEHeader();
    encoded.setCRC(0xA1B2U);
    encoded.setFunction(NET_FUNC::TRANSFER);
    encoded.setSubFunction(NET_SUBFUNC::TRANSFER_SUBFUNC_STATUS);
    encoded.setStreamId(0x10203040U);
    encoded.setPeerId(0x55667788U);
    encoded.setMessageLength(123U);

    std::array<uint8_t, RTP_EXTENSION_HEADER_LENGTH_BYTES + RTP_FNE_HEADER_LENGTH_BYTES> buffer = {};
    encoded.encode(buffer.data());

    RTPFNEHeader decoded = RTPFNEHeader();
    REQUIRE(decoded.decode(buffer.data()));
    REQUIRE(decoded.getCRC() == 0xA1B2U);
    REQUIRE(decoded.getFunction() == NET_FUNC::TRANSFER);
    REQUIRE(decoded.getSubFunction() == NET_SUBFUNC::TRANSFER_SUBFUNC_STATUS);
    REQUIRE(decoded.getStreamId() == 0x10203040U);
    REQUIRE(decoded.getPeerId() == 0x55667788U);
    REQUIRE(decoded.getMessageLength() == 123U);
}

TEST_CASE("RTPFNEHeader rejects invalid extension payload shape", "[network][rtp][fne]")
{
    std::array<uint8_t, RTP_EXTENSION_HEADER_LENGTH_BYTES + RTP_FNE_HEADER_LENGTH_BYTES> buffer = {};

    RTPFNEHeader baseline = RTPFNEHeader();
    baseline.setCRC(0x0102U);
    baseline.setFunction(NET_FUNC::PROTOCOL);
    baseline.setSubFunction(NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR);
    baseline.setStreamId(1U);
    baseline.setPeerId(2U);
    baseline.setMessageLength(3U);
    baseline.encode(buffer.data());

    buffer[0U] = 0x00U;
    buffer[1U] = 0x00U;

    RTPFNEHeader decoded = RTPFNEHeader();
    REQUIRE_FALSE(decoded.decode(buffer.data()));

    baseline.encode(buffer.data());
    buffer[2U] = 0x00U;
    buffer[3U] = 0x03U;
    REQUIRE_FALSE(decoded.decode(buffer.data()));
}

TEST_CASE("RPCHeader encodes and decodes frame metadata", "[network][rpc][header]")
{
    RPCHeader encoded = RPCHeader();
    encoded.setCRC(0xBEEFU);
    encoded.setFunction(0x1234U);
    encoded.setMessageLength(0x10203040U);

    std::array<uint8_t, RPC_HEADER_LENGTH_BYTES> buffer = {};
    encoded.encode(buffer.data());

    RPCHeader decoded = RPCHeader();
    REQUIRE(decoded.decode(buffer.data()));
    REQUIRE(decoded.getCRC() == 0xBEEFU);
    REQUIRE(decoded.getFunction() == 0x1234U);
    REQUIRE(decoded.getMessageLength() == 0x10203040U);
}