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

/**
 * @brief Test harness for KMM-related FNE functionality.
 */
class KMMFNEHarness {
public:
    /**
     * @brief Initializes a new instance of the KMMFNEHarness class.
     */
    KMMFNEHarness() :
        host("fne-kmm-test.yml"),
        crypto("", "", "", false, 0U, true),
        traffic(&host, "127.0.0.1", 62031U, 999999U, "test-password", "test-fne",
            false, false, false, false, true, true, true, true, true,
            0U, false, true, true, 5U, 10U, 2U)
    {
        traffic.setLookups(nullptr, nullptr, nullptr, nullptr, &crypto, nullptr);
    }

    HostFNE host;
    CryptoContainer crypto;
    network::TrafficNetwork traffic;
};

namespace {
    /**
     * @brief Encodes a KMM frame into a byte vector.
     * @tparam T The type of the KMM frame.
     * @param frame The KMM frame to encode.
     * @return A byte vector containing the encoded KMM frame.
     */
    template<typename T>
    std::vector<uint8_t> encodeKMM(T& frame)
    {
        std::vector<uint8_t> bytes(frame.fullLength(), 0U);
        frame.encode(bytes.data());
        return bytes;
    }
}

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

TEST_CASE("KMM air-facing factory rejects malformed lengths", "[p25][kmm][security]")
{
    KMMHello hello;
    hello.setDstLLId(WUID_FNE);
    hello.setSrcLLId(0x654321U);
    std::vector<uint8_t> bytes = encodeKMM(hello);

    REQUIRE(KMMFactory::create(bytes.data(), (uint32_t)bytes.size()) != nullptr);
    REQUIRE(KMMFactory::create(bytes.data(), 9U) == nullptr);

    std::vector<uint8_t> truncated(bytes.begin(), bytes.end() - 1U);
    REQUIRE(KMMFactory::create(truncated.data(), (uint32_t)truncated.size()) == nullptr);

    bytes[1U] = 0x02U;
    bytes[2U] = 0x00U;
    REQUIRE(KMMFactory::create(bytes.data(), (uint32_t)bytes.size()) == nullptr);
}

TEST_CASE("FNE OTAR dispatcher follows AACA response-kind procedures", "[p25][kmm][otar]")
{
    KMMFNEHarness harness;
    constexpr uint32_t SU_RSI = 0x654321U;
    constexpr uint32_t KMF_RSI = WUID_FNE;

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

TEST_CASE("FNE OTAR authenticates and replay-checks KMM before dispatch", "[p25][kmm][otar][security]")
{
    KMMFNEHarness harness;
    constexpr uint32_t SU_RSI = 0x654321U;
    uint8_t tek[32U];
    for (uint32_t i = 0U; i < sizeof(tek); ++i) tek[i] = (uint8_t)i;

    EKCKeyItem item;
    item.id(1U); item.algId(ALGO_AES_256); item.kId(0x1234U); item.sln(1U);
    item.keyMaterial("000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F");
    FNETestHooks::addCryptoKey(harness.traffic, item);

    auto makeSigned = [&](uint16_t mn, uint8_t flag, uint16_t macFormat = KMM_MAC_FORMAT_CBC) {
        KMMHello hello;
        hello.setDstLLId(WUID_FNE); hello.setSrcLLId(SU_RSI);
        hello.setResponseKind(KMM_ResponseKind::IMMEDIATE);
        hello.setHasMessageNumber(true); hello.setMessageNumber(mn); hello.setFlag(flag);
        hello.setMACType(KMM_MAC::ENH_MAC); hello.setMACAlgId(ALGO_AES_256);
        hello.setMACKId(0x1234U); hello.setMACFormat(macFormat);
        std::vector<uint8_t> bytes = encodeKMM(hello);
        hello.generateMAC(tek, bytes.data());
        return bytes;
    };

    uint32_t size = 0U;
    std::vector<uint8_t> valid = makeSigned(100U, KMM_HelloFlag::REKEY_REQUEST_UKEK);
    UInt8Array cbcResponse = FNETestHooks::processOTARKMM(harness.traffic, valid, SU_RSI, size);
    REQUIRE(cbcResponse != nullptr);
    std::unique_ptr<KMMFrame> decodedCBC = KMMFactory::create(cbcResponse.get(), size);
    REQUIRE(decodedCBC != nullptr);
    REQUIRE(decodedCBC->getMACFormat() == KMM_MAC_FORMAT_CBC);
    REQUIRE(decodedCBC->getMessageNumber() == 100U);
    REQUIRE(decodedCBC->verifyMAC(tek, cbcResponse.get(), size));
    REQUIRE(FNETestHooks::processOTARKMM(harness.traffic, valid, SU_RSI, size) != nullptr); // identical RK3 retry

    std::vector<uint8_t> changedSameMN = makeSigned(100U, KMM_HelloFlag::IDENT_ONLY);
    REQUIRE(FNETestHooks::processOTARKMM(harness.traffic, changedSameMN, SU_RSI, size) == nullptr);

    std::vector<uint8_t> badMac = makeSigned(101U, KMM_HelloFlag::REKEY_REQUEST_UKEK);
    badMac[10U] ^= 0x01U;
    REQUIRE(FNETestHooks::processOTARKMM(harness.traffic, badMac, SU_RSI, size) == nullptr);

    std::vector<uint8_t> cmac = makeSigned(101U, KMM_HelloFlag::REKEY_REQUEST_UKEK, KMM_MAC_FORMAT_CMAC);
    UInt8Array cmacResponse = FNETestHooks::processOTARKMM(harness.traffic, cmac, SU_RSI, size);
    REQUIRE(cmacResponse != nullptr);
    std::unique_ptr<KMMFrame> decodedCMAC = KMMFactory::create(cmacResponse.get(), size);
    REQUIRE(decodedCMAC != nullptr);
    REQUIRE(decodedCMAC->getMACFormat() == KMM_MAC_FORMAT_CMAC);
    REQUIRE(decodedCMAC->getMessageNumber() == 101U);
    REQUIRE(decodedCMAC->verifyMAC(tek, cmacResponse.get(), size));

    std::vector<uint8_t> next = makeSigned(102U, KMM_HelloFlag::REKEY_REQUEST_UKEK);
    REQUIRE(FNETestHooks::processOTARKMM(harness.traffic, next, SU_RSI, size) != nullptr);
}

TEST_CASE("P25 OTAR DLD processes KMM through the packet-data service entry point", "[p25][kmm][otar][dld]")
{
    KMMFNEHarness harness;
    constexpr uint32_t SU_RSI = 0x654321U;
    constexpr uint16_t MN = 0x1234U;
    uint8_t tek[32U];
    for (uint32_t i = 0U; i < sizeof(tek); ++i)
        tek[i] = (uint8_t)i;

    EKCKeyItem item;
    item.id(1U); item.algId(ALGO_AES_256); item.kId(0x1234U); item.sln(1U);
    item.keyMaterial("000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F");
    FNETestHooks::addCryptoKey(harness.traffic, item);

    KMMHello hello;
    hello.setDstLLId(WUID_FNE);
    hello.setSrcLLId(SU_RSI);
    hello.setResponseKind(KMM_ResponseKind::NONE);
    hello.setHasMessageNumber(true); hello.setMessageNumber(MN);
    hello.setMACType(KMM_MAC::ENH_MAC); hello.setMACAlgId(ALGO_AES_256);
    hello.setMACKId(0x1234U); hello.setMACFormat(KMM_MAC_FORMAT_CBC);
    hello.setFlag(KMM_HelloFlag::IDENT_ONLY);
    std::vector<uint8_t> encoded = encodeKMM(hello);
    hello.generateMAC(tek, encoded.data());

    REQUIRE(FNETestHooks::processOTARDLD(harness.traffic, encoded, SU_RSI, 7U));
    REQUIRE(FNETestHooks::hasOTARInboundMessageNumber(harness.traffic, SU_RSI, MN));

    std::vector<uint8_t> truncated(encoded.begin(), encoded.end() - 1U);
    REQUIRE_FALSE(FNETestHooks::processOTARDLD(harness.traffic, truncated, SU_RSI, 8U));

    KMMHello wrongSource = hello;
    wrongSource.setSrcLLId(SU_RSI + 1U);
    wrongSource.setMessageNumber(MN + 1U);
    std::vector<uint8_t> wrongRSI = encodeKMM(wrongSource);
    wrongSource.generateMAC(tek, wrongRSI.data());
    // DLD itself is handled, while the mismatched KMM source is discarded.
    REQUIRE(FNETestHooks::processOTARDLD(harness.traffic, wrongRSI, SU_RSI, 9U));
    REQUIRE_FALSE(FNETestHooks::hasOTARInboundMessageNumber(harness.traffic, SU_RSI + 1U, MN + 1U));
}

TEST_CASE("P25 OTAR DLI validates its Version-0 preamble and dispatches KMM", "[p25][kmm][otar][dli]")
{
    constexpr uint32_t SU_RSI = 0x654321U;
    constexpr uint16_t MN = 0x2345U;
    uint8_t tek[32U];
    for (uint32_t i = 0U; i < sizeof(tek); ++i)
        tek[i] = (uint8_t)i;

    auto makeHarnessKey = [](KMMFNEHarness& harness) {
        EKCKeyItem item;
        item.id(1U); item.algId(ALGO_AES_256); item.kId(0x1234U); item.sln(1U);
        item.keyMaterial("000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F");
        FNETestHooks::addCryptoKey(harness.traffic, item);
    };

    KMMHello hello;
    hello.setDstLLId(WUID_FNE); hello.setSrcLLId(SU_RSI);
    hello.setResponseKind(KMM_ResponseKind::NONE);
    hello.setHasMessageNumber(true); hello.setMessageNumber(MN);
    hello.setFlag(KMM_HelloFlag::IDENT_ONLY);
    hello.setMACType(KMM_MAC::ENH_MAC); hello.setMACAlgId(ALGO_AES_256);
    hello.setMACKId(0x1234U); hello.setMACFormat(KMM_MAC_FORMAT_CBC);
    std::vector<uint8_t> kmm = encodeKMM(hello);
    hello.generateMAC(tek, kmm.data());

    std::vector<uint8_t> datagram(14U + kmm.size(), 0U);
    datagram[0U] = 0U;                 // Version-0 format
    datagram[1U] = MFG_STANDARD;
    datagram[2U] = ALGO_UNENCRYPT;
    ::memcpy(datagram.data() + 14U, kmm.data(), kmm.size());

    SECTION("valid clear DLI reaches the authenticated KMM dispatcher") {
        KMMFNEHarness harness;
        makeHarnessKey(harness);
        FNETestHooks::processOTARDLI(harness.traffic, datagram);
        REQUIRE(FNETestHooks::hasOTARInboundMessageNumber(harness.traffic, SU_RSI, MN));
    }

    SECTION("invalid preamble MFID is rejected before KMM dispatch") {
        KMMFNEHarness harness;
        makeHarnessKey(harness);
        datagram[1U] ^= 0x01U;
        FNETestHooks::processOTARDLI(harness.traffic, datagram);
        REQUIRE_FALSE(FNETestHooks::hasOTARInboundMessageNumber(harness.traffic, SU_RSI, MN));
    }

    SECTION("oversized DLI KMM is rejected before KMM dispatch") {
        KMMFNEHarness harness;
        makeHarnessKey(harness);
        datagram.resize(14U + 466U, 0U);
        FNETestHooks::processOTARDLI(harness.traffic, datagram);
        REQUIRE_FALSE(FNETestHooks::hasOTARInboundMessageNumber(harness.traffic, SU_RSI, MN));
    }
}

TEST_CASE("KMM Key Format follows AACA-D Table 70", "[p25][kmm][key-format]")
{
    REQUIRE(KMM_KEY_FORMAT_TEK == 0x00U);
    REQUIRE(KMM_KEY_FORMAT_KEK == 0x80U);
    REQUIRE(KMM_KEY_FORMAT_DELETE == 0x20U);
    REQUIRE(KMM_BODY_FORMAT_TEK_INCLUDED == 0x80U);
    REQUIRE(KMM_BODY_FORMAT_KEK_MISSING == 0x40U);

    auto encodedKeyFormat = [](uint8_t format) {
        KMMRekeyCommand command;
        command.setSrcLLId(0x010203U);
        command.setDstLLId(0x040506U);
        command.setDecryptInfoFmt(KMM_DECRYPT_INSTRUCT_NONE);

        KeysetItem keyset;
        keyset.keysetId(1U);
        keyset.algId(ALGO_AES_256);
        keyset.keyLength(1U);

        KeyItem key;
        const uint8_t material = 0x5AU;
        key.keyFormat(format);
        key.sln(1U);
        key.kId(2U);
        key.setKey(&material, 1U);
        keyset.push_back(key);
        command.setKeysets({ keyset });

        std::vector<uint8_t> encoded(command.fullLength(), 0U);
        command.encode(encoded.data());
        return encoded[20U];
    };

    REQUIRE(encodedKeyFormat(KMM_KEY_FORMAT_TEK) == 0x00U);
    REQUIRE(encodedKeyFormat(KMM_KEY_FORMAT_KEK) == 0x80U);
    REQUIRE(encodedKeyFormat(KMM_KEY_FORMAT_TEK | KMM_KEY_FORMAT_DELETE) == 0x20U);
}
