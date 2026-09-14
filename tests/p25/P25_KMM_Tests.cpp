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
#include "common/p25/kmm/KMMFactory.h"
#include "common/p25/kmm/KMMDeregistrationCommand.h"
#include "common/p25/kmm/KMMDeregistrationResponse.h"
#include "common/p25/kmm/KMMHello.h"
#include "common/p25/kmm/KMMNoService.h"
#include "fne/FNETestHooks.h"
#include "fne/HostFNE.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

using namespace p25;
using namespace p25::defines;
using namespace p25::kmm;

namespace {

class KMMFNEHarness {
public:
    KMMFNEHarness() :
        host("fne-kmm-test.yml"),
        traffic(&host, "127.0.0.1", 62031U, 999999U, "test-password", "test-fne",
            false, false, false, false, true, true, true, true, true,
            0U, false, true, true, 5U, 10U, 2U)
    {
        /* stub */
    }

    HostFNE host;
    network::TrafficNetwork traffic;
};

template<typename T>
std::vector<uint8_t> encodeKMM(T& frame)
{
    std::vector<uint8_t> bytes(frame.fullLength(), 0U);
    frame.encode(bytes.data());
    return bytes;
}

} // namespace

TEST_CASE("KMM factory decodes encoded HELLO and NO_SERVICE frames", "[p25][kmm][factory]")
{
    SECTION("HELLO frame round-trips with message number") {
        KMMHello tx;
        tx.setDstLLId(0x123456U);
        tx.setSrcLLId(0x654321U);
        tx.setMessageNumber(0x1122U);
        tx.setFlag(KMM_HelloFlag::REKEY_REQUEST_NO_UKEK);

        UInt8Array buffer = std::make_unique<uint8_t[]>(tx.fullLength());
        ::memset(buffer.get(), 0x00U, tx.fullLength());
        tx.encode(buffer.get());

        std::unique_ptr<KMMFrame> base = KMMFactory::create(buffer.get());
        REQUIRE(base != nullptr);

        KMMHello* rx = dynamic_cast<KMMHello*>(base.get());
        REQUIRE(rx != nullptr);
        REQUIRE(rx->getMessageId() == KMM_MessageType::HELLO);
        REQUIRE(rx->getFlag() == KMM_HelloFlag::REKEY_REQUEST_NO_UKEK);
        REQUIRE(rx->getDstLLId() == 0x123456U);
        REQUIRE(rx->getSrcLLId() == 0x654321U);
        REQUIRE(rx->getHasMessageNumber() == true);
        REQUIRE(rx->getMessageNumber() == 0x1122U);
    }

    SECTION("NO_SERVICE frame round-trips") {
        KMMNoService tx;
        tx.setDstLLId(0x010203U);
        tx.setSrcLLId(0xA0B0C0U);

        UInt8Array buffer = std::make_unique<uint8_t[]>(tx.fullLength());
        ::memset(buffer.get(), 0x00U, tx.fullLength());
        tx.encode(buffer.get());

        std::unique_ptr<KMMFrame> base = KMMFactory::create(buffer.get());
        REQUIRE(base != nullptr);

        KMMNoService* rx = dynamic_cast<KMMNoService*>(base.get());
        REQUIRE(rx != nullptr);
        REQUIRE(rx->getMessageId() == KMM_MessageType::NO_SERVICE);
        REQUIRE(rx->getDstLLId() == 0x010203U);
        REQUIRE(rx->getSrcLLId() == 0xA0B0C0U);
    }

    SECTION("Factory returns nullptr for unknown message ID") {
        uint8_t buffer[16U] = { 0U };
        buffer[0U] = 0xFFU;

        std::unique_ptr<KMMFrame> frame = KMMFactory::create(buffer);
        REQUIRE(frame == nullptr);
    }
}

TEST_CASE("FNE OTAR dispatcher follows AACA response-kind procedures", "[p25][kmm][otar]")
{
    KMMFNEHarness harness;
    constexpr uint32_t SU_RSI = 0x654321U;
    constexpr uint32_t KMF_RSI = 0x123456U;

    SECTION("Response Kind 1 Hello produces no OTAR response") {
        KMMHello request;
        request.setDstLLId(KMF_RSI);
        request.setSrcLLId(SU_RSI);
        request.setResponseKind(KMM_ResponseKind::NONE);

        uint32_t payloadSize = 99U;
        UInt8Array response = FNETestHooks::processOTARKMM(harness.traffic,
            encodeKMM(request), SU_RSI, payloadSize);

        REQUIRE(response == nullptr);
        REQUIRE(payloadSize == 0U);
    }

    SECTION("SU-originated Response Kind 2 Hello is discarded") {
        KMMHello request;
        request.setDstLLId(KMF_RSI);
        request.setSrcLLId(SU_RSI);
        request.setResponseKind(KMM_ResponseKind::DELAYED);

        uint32_t payloadSize = 99U;
        UInt8Array response = FNETestHooks::processOTARKMM(harness.traffic,
            encodeKMM(request), SU_RSI, payloadSize);

        REQUIRE(response == nullptr);
        REQUIRE(payloadSize == 0U);
    }

    SECTION("Response Kind 3 Hello receives No-Service when KMF service is disabled") {
        KMMHello request;
        request.setDstLLId(KMF_RSI);
        request.setSrcLLId(SU_RSI);
        request.setResponseKind(KMM_ResponseKind::IMMEDIATE);
        request.setFlag(KMM_HelloFlag::REKEY_REQUEST_UKEK);

        uint32_t payloadSize = 0U;
        UInt8Array response = FNETestHooks::processOTARKMM(harness.traffic,
            encodeKMM(request), SU_RSI, payloadSize);

        REQUIRE(response != nullptr);
        REQUIRE(payloadSize == KMMNoService().fullLength());
        std::unique_ptr<KMMFrame> decoded = KMMFactory::create(response.get());
        REQUIRE(decoded != nullptr);
        REQUIRE(decoded->getMessageId() == KMM_MessageType::NO_SERVICE);
        REQUIRE(decoded->getResponseKind() == KMM_ResponseKind::NONE);
        REQUIRE(decoded->getDstLLId() == SU_RSI);
        REQUIRE(decoded->getSrcLLId() == WUID_FNE);
    }

    SECTION("Response Kind 3 Deregistration receives Deregistration-Response") {
        FNETestHooks::setKMFServicesEnabled(harness.traffic, true);

        KMMDeregistrationCommand request;
        request.setDstLLId(KMF_RSI);
        request.setSrcLLId(SU_RSI);
        request.setResponseKind(KMM_ResponseKind::IMMEDIATE);
        request.setKMFRSI(KMF_RSI);

        uint32_t payloadSize = 0U;
        UInt8Array response = FNETestHooks::processOTARKMM(harness.traffic,
            encodeKMM(request), SU_RSI, payloadSize);

        REQUIRE(response != nullptr);
        KMMDeregistrationResponse expected;
        REQUIRE(payloadSize == expected.fullLength());
        std::unique_ptr<KMMFrame> decoded = KMMFactory::create(response.get());
        REQUIRE(decoded != nullptr);
        REQUIRE(decoded->getMessageId() == KMM_MessageType::DEREG_RSP);
        REQUIRE(decoded->getResponseKind() == KMM_ResponseKind::NONE);
        REQUIRE(decoded->getDstLLId() == SU_RSI);
        REQUIRE(decoded->getSrcLLId() == WUID_FNE);
    }
}
