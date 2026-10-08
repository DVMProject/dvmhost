// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "host/Defines.h"
#include "common/p25/P25Defines.h"
#include "common/p25/data/ConventionalRegistration.h"
#include "common/p25/data/ConventionalDataService.h"
#include "common/p25/data/IPConvergenceService.h"
#include "common/p25/data/IPv4Packet.h"
#include "common/p25/data/PacketDataState.h"

using namespace p25::data;
using namespace p25::defines;

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

namespace
{
std::array<uint8_t, ConventionalRegistration::LENGTH> registrationPacket(
    uint8_t type, uint32_t llId, uint32_t ipAddress, uint8_t options = 0U)
{
    ConventionalRegistration registration;
    registration.type = type;
    registration.options = options;
    registration.llId = llId;
    registration.ipAddress = ipAddress;
    std::array<uint8_t, ConventionalRegistration::LENGTH> data {};
    REQUIRE(registration.encode(data.data(), data.size()));
    return data;
} // namespace

std::vector<uint8_t> ipv4Packet(uint32_t source, uint32_t destination,
    uint16_t length = 20U)
{
    std::vector<uint8_t> packet(length, 0U);
    packet[0U] = 0x45U;
    packet[2U] = uint8_t(length >> 8U);
    packet[3U] = uint8_t(length);
    packet[9U] = 17U;
    packet[12U] = uint8_t(source >> 24U);
    packet[13U] = uint8_t(source >> 16U);
    packet[14U] = uint8_t(source >> 8U);
    packet[15U] = uint8_t(source);
    packet[16U] = uint8_t(destination >> 24U);
    packet[17U] = uint8_t(destination >> 16U);
    packet[18U] = uint8_t(destination >> 8U);
    packet[19U] = uint8_t(destination);
    return packet;
}
}

TEST_CASE("P25 conventional registration codec is length safe", "[p25][packet-data][conventional][security]")
{
    ConventionalRegistration original;
    original.type = PDURegType::CONNECT;
    original.options = 0x05U;
    original.llId = 0x54369FU;
    original.ipAddress = 0x0A0A010AU;

    std::array<uint8_t, ConventionalRegistration::LENGTH> encoded {};
    REQUIRE(original.encode(encoded.data(), encoded.size()));

    ConventionalRegistration decoded;
    REQUIRE(ConventionalRegistration::decode(encoded.data(), encoded.size(), decoded));
    CHECK(decoded.type == original.type);
    CHECK(decoded.options == original.options);
    CHECK(decoded.llId == original.llId);
    CHECK(decoded.ipAddress == original.ipAddress);

    for (uint32_t length = 0U; length < ConventionalRegistration::LENGTH; length++)
        CHECK_FALSE(ConventionalRegistration::decode(encoded.data(), length, decoded));

    CHECK_FALSE(ConventionalRegistration::decode(nullptr, encoded.size(), decoded));
    CHECK_FALSE(original.encode(nullptr, encoded.size()));
    CHECK_FALSE(original.encode(encoded.data(), encoded.size() - 1U));

    encoded[0U] = 0xF0U;
    CHECK_FALSE(ConventionalRegistration::decode(encoded.data(), encoded.size(), decoded));

    ConventionalRegistration disconnect;
    disconnect.type = PDURegType::DISCONNECT;
    disconnect.llId = original.llId;
    std::array<uint8_t, ConventionalRegistration::DISCONNECT_LENGTH> disconnectData {};
    REQUIRE(disconnect.encode(disconnectData.data(), disconnectData.size()));
    REQUIRE(ConventionalRegistration::decode(disconnectData.data(), disconnectData.size(), decoded));
    CHECK(decoded.type == PDURegType::DISCONNECT);
    CHECK(decoded.ipAddress == 0U);
}

TEST_CASE("P25 packet data bindings preserve authorization and reject collisions",
    "[p25][packet-data][state]")
{
    DataBindingRegistry registry;

    IPBinding first;
    first.link.llId = 1001U;
    first.ipAddress = 0x0A000001U;
    first.accessMode = DataAccessMode::CONVENTIONAL;
    first.convergenceMode = DataConvergenceMode::SCEP;
    first.origin = DataBindingOrigin::CONVENTIONAL_REGISTRATION;
    first.authorized = true;
    REQUIRE(registry.upsert(first));

    const IPBinding* byLink = registry.findByLLId(1001U);
    REQUIRE(byLink != nullptr);
    CHECK(byLink->ipAddress == first.ipAddress);
    CHECK(byLink->authorized);
    REQUIRE(registry.findByIPAddress(first.ipAddress) != nullptr);

    IPBinding collision = first;
    collision.link.llId = 1002U;
    CHECK_FALSE(registry.upsert(collision));

    IPBinding invalid = first;
    invalid.ipAddress = 0U;
    CHECK_FALSE(registry.upsert(invalid));

    CHECK(registry.erase(1001U));
    CHECK(registry.findByLLId(1001U) == nullptr);
}

TEST_CASE("P25 LLC state is isolated by LLID", "[p25][packet-data][llc]")
{
    DataLinkManager links;
    bool synchronize = false;

    CHECK(links.nextSendSequence(1001U, synchronize) == 0U);
    CHECK(synchronize);
    CHECK(links.nextSendSequence(2002U, synchronize) == 0U);
    CHECK(links.nextSendSequence(1001U, synchronize) == 1U);

    for (uint8_t sequence = 2U; sequence < 8U; sequence++)
        CHECK(links.nextSendSequence(1001U, synchronize) == sequence);
    CHECK(links.nextSendSequence(1001U, synchronize) == 0U);
    CHECK_FALSE(synchronize);

    uint8_t expected = 0U;
    CHECK(links.acceptReceiveSequence(1001U, 0U, true, 11U, expected) == ReceiveSequenceResult::ACCEPTED);
    CHECK(links.acceptReceiveSequence(1001U, 0U, false, 11U, expected) == ReceiveSequenceResult::DUPLICATE);
    CHECK(links.acceptReceiveSequence(1001U, 1U, false, 12U, expected) == ReceiveSequenceResult::ACCEPTED);
    CHECK(links.acceptReceiveSequence(1001U, 7U, false, 13U, expected) == ReceiveSequenceResult::ACCEPTED);
    CHECK(expected == 2U);

    links.setReady(1001U, false);
    CHECK_FALSE(links.isReady(1001U));
    CHECK(links.isReady(2002U));
    links.erase(1001U);
    CHECK_FALSE(links.hasState(1001U));
}

TEST_CASE("P25 LLC receive sequence wraps and suppresses duplicates", "[p25][packet-data][llc]")
{
    DataLinkManager links;
    uint8_t expected = 0U;

    REQUIRE(links.acceptReceiveSequence(77U, 7U, false, 21U, expected) == ReceiveSequenceResult::ACCEPTED);
    CHECK(links.acceptReceiveSequence(77U, 0U, false, 22U, expected) == ReceiveSequenceResult::ACCEPTED);
    CHECK(links.acceptReceiveSequence(77U, 0U, false, 22U, expected) == ReceiveSequenceResult::DUPLICATE);
    CHECK(links.acceptReceiveSequence(77U, 2U, false, 23U, expected) == ReceiveSequenceResult::ACCEPTED);
    CHECK(expected == 1U);

    CHECK(links.acceptReceiveSequence(88U, 3U, true, 1U, expected) == ReceiveSequenceResult::OUT_OF_SEQUENCE);
}

TEST_CASE("P25 IPv4 packet validation bounds declared length", "[p25][packet-data][ipv4][security]")
{
    std::array<uint8_t, 24U> packet {};
    packet[0U] = 0x45U;
    packet[2U] = 0x00U;
    packet[3U] = 24U;
    packet[9U] = 17U;
    packet[12U] = 10U;
    packet[15U] = 1U;
    packet[16U] = 10U;
    packet[19U] = 2U;

    IPv4Packet view;
    REQUIRE(IPv4Packet::parse(packet.data(), packet.size(), view));
    CHECK(view.headerLength == 20U);
    CHECK(view.totalLength == 24U);
    CHECK(view.protocol == 17U);
    CHECK(view.sourceAddress == 0x0A000001U);
    CHECK(view.destinationAddress == 0x0A000002U);

    for (uint32_t length = 0U; length < 20U; length++)
        CHECK_FALSE(IPv4Packet::parse(packet.data(), length, view));

    packet[3U] = 25U;
    CHECK_FALSE(IPv4Packet::parse(packet.data(), packet.size(), view));
    packet[3U] = 19U;
    CHECK_FALSE(IPv4Packet::parse(packet.data(), packet.size(), view));
    packet[3U] = 24U;
    packet[0U] = 0x46U;
    CHECK_FALSE(IPv4Packet::parse(packet.data(), 20U, view));
    packet[0U] = 0x65U;
    CHECK_FALSE(IPv4Packet::parse(packet.data(), packet.size(), view));
}

TEST_CASE("P25 conventional registration service owns binding transactions",
    "[p25][packet-data][conventional][service]")
{
    DataBindingRegistry bindings;
    DataLinkManager links;
    ConventionalRegistrationPolicy policy;
    policy.allowCMSScan = true;
    policy.registrationLifetimeMs = 100U;
    policy.allocateDynamicAddress = [](uint32_t llId, uint32_t) {
        return 0x0A000000U | (llId & 0xFFU);
    };
    ConventionalDataService service(bindings, links, policy);

    ConventionalRegistrationProvisioning staticProvisioning;
    staticProvisioning.known = true;
    staticProvisioning.enabled = true;
    staticProvisioning.staticIPAddress = 0x0A000001U;
    auto request = registrationPacket(PDURegType::CONNECT, 1U, 0U, 1U);
    ConventionalRegistrationResult result = service.process(request.data(), request.size(),
        1U, staticProvisioning, 10U);
    REQUIRE(result.decision == RegistrationDecision::ACCEPT);
    CHECK(result.response.ipAddress == 0x0A000001U);
    CHECK(result.response.options == 1U);
    REQUIRE(bindings.findByLLId(1U) != nullptr);

    // Duplicate connects are idempotent and refresh the lifetime.
    result = service.process(request.data(), request.size(), 1U, staticProvisioning, 50U);
    CHECK(result.decision == RegistrationDecision::ACCEPT);
    CHECK_FALSE(result.bindingChanged);
    service.expire(120U);
    CHECK(service.isRegistered(1U));
    service.expire(151U);
    CHECK_FALSE(service.isRegistered(1U));

    ConventionalRegistrationProvisioning dynamicProvisioning;
    dynamicProvisioning.known = true;
    dynamicProvisioning.enabled = true;
    dynamicProvisioning.allowDynamicAddress = true;
    request = registrationPacket(PDURegType::CONNECT, 2U, 0U);
    result = service.process(request.data(), request.size(), 2U, dynamicProvisioning, 0U);
    REQUIRE(result.decision == RegistrationDecision::ACCEPT);
    CHECK(result.response.ipAddress == 0x0A000002U);

    // The same LLID may change its dynamic IP, but an IP cannot change owners.
    request = registrationPacket(PDURegType::CONNECT, 2U, 0x0A000022U);
    CHECK(service.process(request.data(), request.size(), 2U, dynamicProvisioning).decision ==
        RegistrationDecision::ACCEPT);
    request = registrationPacket(PDURegType::CONNECT, 3U, 0x0A000022U);
    CHECK(service.process(request.data(), request.size(), 3U, dynamicProvisioning).denyReason ==
        RegistrationDenyReason::ADDRESS_CONFLICT);
    CHECK(bindings.findByLLId(3U) == nullptr);

    request = registrationPacket(PDURegType::DISCONNECT, 2U, 0U);
    result = service.process(request.data(), request.size(), 2U, dynamicProvisioning);
    CHECK(result.decision == RegistrationDecision::DISCONNECTED);
    CHECK(result.bindingChanged);
    result = service.process(request.data(), request.size(), 2U, dynamicProvisioning);
    CHECK(result.decision == RegistrationDecision::DISCONNECTED);
    CHECK_FALSE(result.bindingChanged);
}

TEST_CASE("P25 conventional registration denies before changing shared state",
    "[p25][packet-data][conventional][service][security]")
{
    DataBindingRegistry bindings;
    DataLinkManager links;
    ConventionalRegistrationPolicy policy;
    policy.allocateDynamicAddress = [](uint32_t, uint32_t) { return 0U; };
    ConventionalDataService service(bindings, links, policy);
    ConventionalRegistrationProvisioning provisioning;
    auto request = registrationPacket(PDURegType::CONNECT, 10U, 0U);

    CHECK(service.process(request.data(), request.size(), 10U, provisioning).denyReason ==
        RegistrationDenyReason::NOT_PROVISIONED);
    provisioning.known = true;
    CHECK(service.process(request.data(), request.size(), 10U, provisioning).denyReason ==
        RegistrationDenyReason::DISABLED);
    provisioning.enabled = true;
    provisioning.allowDynamicAddress = true;
    CHECK(service.process(request.data(), request.size(), 10U, provisioning).denyReason ==
        RegistrationDenyReason::ADDRESS_UNAVAILABLE);

    request = registrationPacket(PDURegType::CONNECT, 10U, 0x0A00000AU, 0x02U);
    CHECK(service.process(request.data(), request.size(), 10U, provisioning).denyReason ==
        RegistrationDenyReason::UNSUPPORTED_OPTIONS);
    CHECK(service.process(request.data(), request.size(), 11U, provisioning).denyReason ==
        RegistrationDenyReason::IDENTITY_MISMATCH);
    CHECK(bindings.size() == 0U);
    CHECK(service.registrationCount() == 0U);
}

TEST_CASE("P25 SCEP enforces binding, IP ownership, MTU, and delivery policy",
    "[p25][packet-data][scep][security]")
{
    DataBindingRegistry bindings;
    DataLinkManager links;
    ConventionalDataService registrations(bindings, links);
    REQUIRE(registrations.installStatic(100U, 0x0A000064U));

    SCEPPolicy policy;
    policy.mtu = 100U;
    policy.allowBroadcast = true;
    policy.allowMulticast = false;
    policy.unicastDelivery = DataDeliveryMode::CONFIRMED;
    policy.broadcastDelivery = DataDeliveryMode::UNCONFIRMED;
    SCEPService scep(bindings, policy);

    std::vector<uint8_t> packet = ipv4Packet(0x0A000064U, 0x0A000065U);
    IPDecodeResult decoded = scep.decode(packet.data(), packet.size());
    REQUIRE(decoded.result == ConvergenceResult::OK);
    CHECK(scep.authorizeUplink(100U, decoded.packet) == ConvergenceResult::OK);
    CHECK(scep.authorizeUplink(101U, decoded.packet) == ConvergenceResult::NO_BINDING);

    packet = ipv4Packet(0x0A000099U, 0x0A000065U);
    decoded = scep.decode(packet.data(), packet.size());
    CHECK(scep.authorizeUplink(100U, decoded.packet) == ConvergenceResult::SOURCE_MISMATCH);

    REQUIRE(registrations.installStatic(101U, 0x0A000065U));
    IPEncodeResult encoded = scep.encode(packet.data(), packet.size());
    CHECK(encoded.result == ConvergenceResult::OK);
    CHECK(encoded.llId == 101U);
    CHECK(encoded.delivery == DataDeliveryMode::CONFIRMED);

    packet = ipv4Packet(0x0A000064U, 0xFFFFFFFFU);
    encoded = scep.encode(packet.data(), packet.size());
    CHECK(encoded.result == ConvergenceResult::OK);
    CHECK(encoded.llId == WUID_ALL);
    CHECK(encoded.delivery == DataDeliveryMode::UNCONFIRMED);

    packet = ipv4Packet(0x0A000064U, 0xE0000001U);
    CHECK(scep.encode(packet.data(), packet.size()).result == ConvergenceResult::POLICY_REJECTED);
    decoded = scep.decode(packet.data(), packet.size());
    CHECK(scep.authorizeUplink(100U, decoded.packet) == ConvergenceResult::POLICY_REJECTED);

    packet = ipv4Packet(0x0A000064U, 0x0A000065U, 101U);
    CHECK(scep.decode(packet.data(), packet.size()).result == ConvergenceResult::MTU_EXCEEDED);

    // ARP-like observations are not inputs to SCEP authorization; only typed
    // provisioned or registered bindings are visible to this service.
    DataBindingRegistry emptyBindings;
    SCEPService noBinding(emptyBindings, policy);
    packet = ipv4Packet(0x0A000064U, 0x0A000065U);
    decoded = noBinding.decode(packet.data(), packet.size());
    CHECK(noBinding.authorizeUplink(100U, decoded.packet) == ConvergenceResult::NO_BINDING);
    CHECK(noBinding.encode(packet.data(), packet.size()).result == ConvergenceResult::NO_BINDING);
}
