// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */

#include "common/p25/P25Defines.h"
#include "common/p25/data/IPv4Packet.h"
#include "common/p25/sndcp/SNDCPFactory.h"
#include "network/callhandler/packetdata/p25/IPConvergenceService.h"
#include "network/callhandler/packetdata/p25/PacketDataState.h"

using namespace p25::defines;
using namespace p25::sndcp;
using namespace network::callhandler::packetdata::p25data;

#include <catch2/catch_test_macros.hpp>

#include <array>

TEST_CASE("P25 SNDCP activation request decoding is length safe", "[p25][sndcp][codec][security]")
{
    const std::array<uint8_t, 10U> encoded {
        0x01U, 0x31U, 0x0AU, 0x0AU, 0x01U, 0x2AU, 0x50U, 0x00U, 0x00U, 0x07U
    };

    for (uint32_t length = 0U; length < encoded.size(); length++)
        CHECK(SNDCPFactory::create(encoded.data(), length) == nullptr);
    CHECK(SNDCPFactory::create(nullptr, encoded.size()) == nullptr);

    std::unique_ptr<SNDCPPacket> packet = SNDCPFactory::create(encoded.data(), encoded.size());
    REQUIRE(packet != nullptr);
    REQUIRE(packet->getPDUType() == SNDCP_PDUType::ACT_TDS_CTX);

    SNDCPCtxActRequest* request = static_cast<SNDCPCtxActRequest*>(packet.get());
    CHECK(request->getSNDCPVersion() == SNDCP_VERSION_1);
    CHECK(request->getNSAPI() == 3U);
    CHECK(request->getNAT() == SNDCPNAT::IPV4_DYN_ADDR);
    CHECK(request->getIPAddress() == 0x0A0A012AU);
    CHECK(request->getDSUT() == SNDCP_DSUT::ALT_T_AND_C_DATA_VOICE);
    CHECK(request->getMDPCO() == 7U);
}

TEST_CASE("P25 SNDCP deactivation decoding is length safe", "[p25][sndcp][codec][security]")
{
    const std::array<uint8_t, 2U> encoded {
        uint8_t((SNDCP_PDUType::DEACT_TDS_CTX_REQ << 4U) | 3U),
        SNDCPDeactivationType::DEACT_THIS_PDU
    };

    CHECK(SNDCPFactory::create(encoded.data(), 0U) == nullptr);
    CHECK(SNDCPFactory::create(encoded.data(), 1U) == nullptr);

    std::unique_ptr<SNDCPPacket> packet = SNDCPFactory::create(encoded.data(), encoded.size());
    REQUIRE(packet != nullptr);
    SNDCPCtxDeactivation* request = static_cast<SNDCPCtxDeactivation*>(packet.get());
    CHECK(request->getNSAPI() == 3U);
    CHECK(request->getDeactType() == SNDCPDeactivationType::DEACT_THIS_PDU);
}

TEST_CASE("P25 SNDCP activation responses preserve negotiated fields", "[p25][sndcp][codec]")
{
    SNDCPCtxActAccept accept;
    accept.setNSAPI(3U);
    accept.setPriority(4U);
    accept.setReadyTimer(SNDCPReadyTimer::TEN_SECONDS);
    accept.setStandbyTimer(SNDCPStandbyTimer::FOUR_HOURS);
    accept.setNAT(SNDCPNAT::IPV4_DYN_ADDR);
    accept.setIPAddress(0x0A0A012AU);
    accept.setMTU(SNDCP_MTU_510);
    accept.setMDPCO(7U);

    std::array<uint8_t, 13U> encoded {};
    accept.encode(encoded.data());
    CHECK(encoded[0U] == 0x03U);
    CHECK(encoded[1U] == 0x46U);
    CHECK(encoded[2U] == 0x91U);
    CHECK(encoded[3U] == 0x0AU);
    CHECK(encoded[4U] == 0x0AU);
    CHECK(encoded[5U] == 0x01U);
    CHECK(encoded[6U] == 0x2AU);
    CHECK(encoded[9U] == 0x20U);
    CHECK(encoded[10U] == 0x07U);

    SNDCPCtxActReject reject;
    reject.setNSAPI(3U);
    reject.setRejectCode(SNDCPRejectReason::DYN_IP_POOL_EMPTY);
    std::array<uint8_t, 2U> rejected {};
    reject.encode(rejected.data());
    CHECK(rejected[0U] == 0x33U);
    CHECK(rejected[1U] == SNDCPRejectReason::DYN_IP_POOL_EMPTY);
}

TEST_CASE("P25 SNDCP bindings authorize both static and dynamic contexts and revoke cleanly",
    "[p25][sndcp][state][security]")
{
    DataBindingRegistry bindings;
    SCEPService convergence(bindings);

    IPBinding binding;
    binding.link.llId = 1001U;
    binding.ipAddress = 0x0A0A010AU;
    binding.accessMode = DataAccessMode::TRUNKED;
    binding.convergenceMode = DataConvergenceMode::SNDCP;
    binding.origin = DataBindingOrigin::SNDCP_DYNAMIC;
    binding.nsapi = 1U;
    binding.hasNSAPI = true;
    binding.authorized = true;
    REQUIRE(bindings.upsert(binding));

    std::array<uint8_t, 20U> packet {};
    packet[0U] = 0x45U;
    packet[2U] = 0x00U;
    packet[3U] = uint8_t(packet.size());
    packet[12U] = 0x0AU;
    packet[13U] = 0x0AU;
    packet[14U] = 0x01U;
    packet[15U] = 0x0AU;
    packet[16U] = 0x0AU;
    packet[17U] = 0x0AU;
    packet[18U] = 0x01U;
    packet[19U] = 0x0AU;

    IPDecodeResult decoded = convergence.decode(packet.data(), packet.size());
    REQUIRE(decoded.result == ConvergenceResult::OK);
    CHECK(convergence.authorizeUplink(binding.link.llId, decoded.packet) == ConvergenceResult::OK);

    IPEncodeResult encoded = convergence.encode(packet.data(), packet.size());
    REQUIRE(encoded.result == ConvergenceResult::OK);
    CHECK(encoded.llId == binding.link.llId);

    REQUIRE(bindings.erase(binding.link.llId));
    CHECK(convergence.authorizeUplink(binding.link.llId, decoded.packet) == ConvergenceResult::NO_BINDING);
    CHECK(convergence.encode(packet.data(), packet.size()).result == ConvergenceResult::NO_BINDING);

    binding.origin = DataBindingOrigin::SNDCP_STATIC;
    REQUIRE(bindings.upsert(binding));
    CHECK(convergence.authorizeUplink(binding.link.llId, decoded.packet) == ConvergenceResult::OK);
}
