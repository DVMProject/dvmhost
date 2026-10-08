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
#include "common/p25/data/DataRouting.h"
#include "common/p25/data/PacketScheduler.h"

using namespace p25::data;
using namespace p25::defines;

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <utility>
#include <vector>

namespace {
    /**
     * @brief Creates an encoded conventional registration packet.
     * @param type The type of the conventional registration packet.
     * @param llId The logical link ID.
     * @param ipAddress The IP address.
     * @param options Optional registration options.
     * @return Encoded conventional registration packet as a byte array.
     */
    std::array<uint8_t, ConventionalRegistration::LENGTH> registrationPacket(uint8_t type, uint32_t llId, uint32_t ipAddress, uint8_t options = 0U)
    {
        ConventionalRegistration registration;
        registration.type = type;
        registration.options = options;
        registration.llId = llId;
        registration.ipAddress = ipAddress;
        std::array<uint8_t, ConventionalRegistration::LENGTH> data {};
        REQUIRE(registration.encode(data.data(), data.size()));
        return data;
    }

    /**
     * @brief Creates an IPv4 packet with the specified source and destination addresses.
     * @param source The source IP address.
     * @param destination The destination IP address.
     * @param length The total length of the IPv4 packet (default is 20 bytes).
     * @return Encoded IPv4 packet as a byte vector.
     */
    std::vector<uint8_t> ipv4Packet(uint32_t source, uint32_t destination, uint16_t length = 20U)
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

TEST_CASE("P25 conventional locations move, expire, and resolve targeted routes",
    "[p25][packet-data][location][routing]")
{
    DataLocationRegistry locations(100U);
    ConventionalLocation location;
    location.peerId = 10U;
    location.channelId = 1U;
    location.channelNo = 101U;
    location.lastSeen = 1000U;
    REQUIRE(locations.updateConventional(1001U, location));

    DataRoute route = locations.resolve(1001U, AccessMode::CONVENTIONAL, 1050U);
    REQUIRE(route.valid);
    CHECK(route.peerId == 10U);
    CHECK(route.channelId == 1U);
    CHECK(route.channelNo == 101U);

    // Later inbound traffic moves the subscriber to peer B.
    location.peerId = 20U;
    location.channelId = 2U;
    location.channelNo = 202U;
    location.lastSeen = 1060U;
    REQUIRE(locations.updateConventional(1001U, location));
    route = locations.resolve(1001U, AccessMode::CONVENTIONAL, 1070U);
    REQUIRE(route.valid);
    CHECK(route.peerId == 20U);

    locations.expire(1160U);
    CHECK_FALSE(locations.resolve(1001U, AccessMode::CONVENTIONAL, 1160U).valid);
}

TEST_CASE("P25 group and trunked route state coexist with conventional locations",
    "[p25][packet-data][location][routing]")
{
    DataLocationRegistry locations;
    ConventionalLocation conventional;
    conventional.peerId = 20U;
    conventional.lastSeen = 1U;
    REQUIRE(locations.updateConventional(1001U, conventional));

    DataRoute trunked;
    trunked.mode = AccessMode::TRUNKED;
    trunked.peerId = 30U;
    trunked.channelId = 4U;
    trunked.channelNo = 404U;
    trunked.grantId = 77U;
    trunked.lastSeen = 1U;
    REQUIRE(locations.updateTrunked(2002U, trunked));

    CHECK(locations.resolve(1001U, AccessMode::CONVENTIONAL).peerId == 20U);
    CHECK(locations.resolve(2002U, AccessMode::TRUNKED).peerId == 30U);
    CHECK_FALSE(locations.resolve(2002U, AccessMode::CONVENTIONAL).valid);

    locations.setGroupPeers({ 20U, 40U });
    std::vector<DataRoute> routes = locations.resolveGroup({ 10U, 20U, 30U, 40U });
    REQUIRE(routes.size() == 2U);
    CHECK(routes[0U].peerId == 20U);
    CHECK(routes[1U].peerId == 40U);
}

TEST_CASE("P25 unknown conventional location behavior is policy controlled",
    "[p25][packet-data][location][policy]")
{
    DataLocationRegistry locations(100U, false, UnknownLocationPolicy::QUEUE);
    CHECK_FALSE(locations.learnFromInbound());
    CHECK(locations.unknownLocationPolicy() == UnknownLocationPolicy::QUEUE);

    locations.setLearnFromInbound(true);
    locations.setUnknownLocationPolicy(UnknownLocationPolicy::ARP);
    CHECK(locations.learnFromInbound());
    CHECK(locations.unknownLocationPolicy() == UnknownLocationPolicy::ARP);

    locations.setUnknownLocationPolicy(UnknownLocationPolicy::DROP);
    CHECK(locations.unknownLocationPolicy() == UnknownLocationPolicy::DROP);
}

TEST_CASE("P25 neighbor observations do not create bindings or routes",
    "[p25][packet-data][neighbor][security]")
{
    RouteNeighborCache neighbors;
    DataBindingRegistry bindings;
    DataLocationRegistry locations;

    neighbors.observe(1001U, 0x0A000001U, 10U);
    REQUIRE(neighbors.findByLLId(1001U) != nullptr);
    CHECK(neighbors.findByIPAddress(0x0A000001U)->llId == 1001U);
    CHECK(bindings.findByLLId(1001U) == nullptr);
    CHECK_FALSE(locations.resolve(1001U, AccessMode::CONVENTIONAL).valid);
}

TEST_CASE("P25 packet scheduler bounds and rotates queued downlinks",
    "[p25][packet-data][scheduler]")
{
    PacketScheduler scheduler(2U, 8U);
    ScheduledDataPacket first;
    first.llId = 1U;
    first.userData.assign(4U, 0x11U);
    CHECK(scheduler.enqueue(std::move(first)) == 0U);

    ScheduledDataPacket second;
    second.llId = 2U;
    second.userData.assign(4U, 0x22U);
    CHECK(scheduler.enqueue(std::move(second)) == 0U);
    REQUIRE(scheduler.front() != nullptr);
    CHECK(scheduler.front()->llId == 1U);

    scheduler.rotate();
    CHECK(scheduler.front()->llId == 2U);

    ScheduledDataPacket third;
    third.llId = 3U;
    third.userData.assign(4U, 0x33U);
    CHECK(scheduler.enqueue(std::move(third)) == 1U);
    CHECK(scheduler.size() == 2U);
    CHECK(scheduler.byteCount() == 8U);
}

TEST_CASE("P25 scheduled packet header assignment owns an independent copy",
    "[p25][packet-data][scheduler][memory]")
{
    PacketScheduler scheduler(1U, 64U);
    std::array<uint8_t, MI_LENGTH_BYTES> originalMI {
        0x01U, 0x23U, 0x45U, 0x67U, 0x89U, 0xABU, 0xCDU, 0xEFU, 0x10U
    };

    {
        DataHeader source;
        source.setLLId(1001U);
        source.setSAP(PDUSAP::PACKET_DATA);
        source.setMI(originalMI.data());

        ScheduledDataPacket packet;
        packet.header = source;
        packet.userData.assign(20U, 0x55U);
        REQUIRE(scheduler.enqueue(std::move(packet)) == 0U);
    }

    REQUIRE(scheduler.front() != nullptr);
    CHECK(scheduler.front()->header.getLLId() == 1001U);
    CHECK(scheduler.front()->header.getSAP() == PDUSAP::PACKET_DATA);

    std::array<uint8_t, MI_LENGTH_BYTES> copiedMI {};
    scheduler.front()->header.getMI(copiedMI.data());
    CHECK(copiedMI == originalMI);

    scheduler.clear();
    CHECK(scheduler.empty());
}
