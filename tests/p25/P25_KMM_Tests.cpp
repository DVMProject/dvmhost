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
#include "common/p25/kmm/KMMChangeover.h"
#include "common/p25/kmm/KMMDeregistrationCommand.h"
#include "common/p25/kmm/KMMDeregistrationResponse.h"
#include "common/p25/kmm/KMMHello.h"
#include "common/p25/kmm/KMMInventoryCommand.h"
#include "common/p25/kmm/KMMNoService.h"
#include "common/p25/kmm/KMMNegativeAck.h"
#include "common/p25/kmm/KMMRekeyAck.h"
#include "common/p25/kmm/KMMRekeyCommand.h"
#include "fne/FNETestHooks.h"
#include "fne/HostFNE.h"
#include "common/lookups/RadioIdLookup.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <chrono>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

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
        rid("", 0U, false, false),
        traffic(&host, "127.0.0.1", 62031U, 999999U, "test-password", "test-fne",
            false, false, false, false, true, true, true, true, true,
            0U, false, true, true, 5U, 10U, 2U)
    {
        traffic.setLookups(&rid, nullptr, nullptr, nullptr, &crypto, nullptr);
    }

    HostFNE host;
    CryptoContainer crypto;
    lookups::RadioIdLookup rid;
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

    SECTION("security-sensitive KMM received without outer encryption is discarded") {
        KMMRekeyAck ack;
        ack.setDstLLId(KMF_RSI);
        ack.setSrcLLId(SU_RSI);
        ack.setResponseKind(KMM_ResponseKind::NONE);
        ack.setMessageId(KMM_MessageType::REKEY_CMD);
        ack.setNumberOfKeyStatus(0U);

        uint32_t payloadSize = 99U;
        UInt8Array response = FNETestHooks::processOTARKMM(harness.traffic,
            encodeKMM(ack), SU_RSI, payloadSize);
        REQUIRE(response == nullptr);
        REQUIRE(payloadSize == 0U);
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

TEST_CASE("FNE OTAR returns secured RK3 KMM NACKs for validation failures", "[p25][kmm][otar][nack]")
{
    KMMFNEHarness harness;
    constexpr uint32_t SU_RSI = 0x654321U;
    constexpr uint16_t TEK_KID = 0x1234U;
    uint8_t tek[32U];
    for (uint32_t i = 0U; i < sizeof(tek); ++i)
        tek[i] = (uint8_t)i;

    EKCKeyItem item;
    item.id(1U); item.algId(ALGO_AES_256); item.kId(TEK_KID); item.sln(1U);
    item.keyMaterial("000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F");
    FNETestHooks::addCryptoKey(harness.traffic, item);

    auto signedHello = [&](uint16_t mn) {
        KMMHello hello;
        hello.setDstLLId(WUID_FNE); hello.setSrcLLId(SU_RSI);
        hello.setResponseKind(KMM_ResponseKind::IMMEDIATE);
        hello.setHasMessageNumber(true); hello.setMessageNumber(mn);
        hello.setMACType(KMM_MAC::ENH_MAC); hello.setMACAlgId(ALGO_AES_256);
        hello.setMACKId(TEK_KID); hello.setMACFormat(KMM_MAC_FORMAT_CBC);
        std::vector<uint8_t> bytes = encodeKMM(hello);
        hello.generateMAC(tek, bytes.data());
        return bytes;
    };

    auto requireNack = [&](const std::vector<uint8_t>& request, uint8_t status, uint16_t mn,
        uint8_t rejectedMessageId = KMM_MessageType::HELLO) {
        uint32_t size = 0U;
        UInt8Array response = FNETestHooks::processOTARKMM(harness.traffic, request, SU_RSI,
            size, ALGO_AES_256, TEK_KID);
        REQUIRE(response != nullptr);
        std::unique_ptr<KMMFrame> decoded = KMMFactory::create(response.get(), size);
        REQUIRE(decoded != nullptr);
        KMMNegativeAck* nack = dynamic_cast<KMMNegativeAck*>(decoded.get());
        REQUIRE(nack != nullptr);
        REQUIRE(nack->getResponseKind() == KMM_ResponseKind::NONE);
        REQUIRE(nack->getNakMessageId() == rejectedMessageId);
        REQUIRE(nack->getMessageNumber() == mn);
        REQUIRE(nack->getStatus() == status);
        REQUIRE(nack->getMACType() == KMM_MAC::ENH_MAC);
        REQUIRE(nack->getMACKId() == TEK_KID);
        REQUIRE(nack->verifyMAC(tek, response.get(), size));
    };

    std::vector<uint8_t> accepted = signedHello(100U);
    uint32_t ignoredSize = 0U;
    REQUIRE(FNETestHooks::processOTARKMM(harness.traffic, accepted, SU_RSI, ignoredSize,
        ALGO_AES_256, TEK_KID) != nullptr);

    std::vector<uint8_t> replay = signedHello(99U);
    requireNack(replay, KMM_Status::INVALID_MSG_NUMBER, 99U);

    std::vector<uint8_t> badMac = signedHello(101U);
    badMac[12U] ^= 0x01U;
    requireNack(badMac, KMM_Status::INVALID_MAC, 101U);

    KMMHello noMac;
    noMac.setDstLLId(WUID_FNE); noMac.setSrcLLId(SU_RSI);
    noMac.setResponseKind(KMM_ResponseKind::IMMEDIATE);
    noMac.setHasMessageNumber(true); noMac.setMessageNumber(102U);
    requireNack(encodeKMM(noMac), KMM_Status::INVALID_MSG_NUMBER, 102U);

    std::vector<uint8_t> unknownMessage = signedHello(103U);
    unknownMessage[0U] = 0x7FU;
    KMMHello signer;
    signer.setHasMessageNumber(true); signer.setMessageNumber(103U);
    signer.setMACType(KMM_MAC::ENH_MAC); signer.setMACAlgId(ALGO_AES_256);
    signer.setMACKId(TEK_KID); signer.setMACFormat(KMM_MAC_FORMAT_CBC);
    signer.fullLength();
    signer.generateMAC(tek, unknownMessage.data());
    requireNack(unknownMessage, KMM_Status::INVALID_MSG_ID, 103U, 0x7FU);

    std::vector<uint8_t> missingMacKey = signedHello(104U);
    SET_UINT16(0x4321U, missingMacKey.data(), missingMacKey.size() - 3U);
    requireNack(missingMacKey, KMM_Status::ITEM_NOT_EXIST, 104U);

    KMMInventoryCommand unsupportedProcedure;
    unsupportedProcedure.setDstLLId(WUID_FNE); unsupportedProcedure.setSrcLLId(SU_RSI);
    unsupportedProcedure.setResponseKind(KMM_ResponseKind::IMMEDIATE);
    unsupportedProcedure.setHasMessageNumber(true); unsupportedProcedure.setMessageNumber(105U);
    unsupportedProcedure.setMACType(KMM_MAC::ENH_MAC); unsupportedProcedure.setMACAlgId(ALGO_AES_256);
    unsupportedProcedure.setMACKId(TEK_KID); unsupportedProcedure.setMACFormat(KMM_MAC_FORMAT_CBC);
    unsupportedProcedure.setInventoryType(KMM_InventoryType::LIST_ACTIVE_KEYSET_IDS);
    std::vector<uint8_t> unsupportedBytes = encodeKMM(unsupportedProcedure);
    unsupportedProcedure.generateMAC(tek, unsupportedBytes.data());
    requireNack(unsupportedBytes, KMM_Status::INVALID_MSG_ID, 105U, KMM_MessageType::INVENTORY_CMD);
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

    SECTION("encrypted DLD decrypts through the Auxiliary ES context") {
        hello.setMessageNumber(MN + 1U);
        std::vector<uint8_t> next = encodeKMM(hello);
        hello.generateMAC(tek, next.data());
        uint8_t mi[MI_LENGTH_BYTES] = { 0x10U, 0x21U, 0x32U, 0x43U, 0x54U,
            0x65U, 0x76U, 0x87U, 0x98U };
        std::vector<uint8_t> encrypted = FNETestHooks::cryptOTARKMM(harness.traffic,
            ALGO_AES_256, 0x1234U, mi, next, true);
        REQUIRE(encrypted.size() == next.size());
        REQUIRE(encrypted != next);
        REQUIRE(FNETestHooks::processOTARDLDPDU(harness.traffic, encrypted, SU_RSI,
            true, ALGO_AES_256, 0x1234U, mi));
        REQUIRE(FNETestHooks::hasOTARInboundMessageNumber(harness.traffic, SU_RSI, MN + 1U));
    }

    SECTION("encrypted DLD without Auxiliary ES metadata is rejected") {
        REQUIRE_FALSE(FNETestHooks::processOTARDLD(harness.traffic, encoded, SU_RSI, 10U,
            true, ALGO_AES_256, 0x1234U, nullptr));
    }

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

TEST_CASE("P25 FNE accepts a KMM immediately after a PDU response", "[p25][kmm][otar][pdu-state]")
{
    KMMFNEHarness harness;
    constexpr uint32_t SU_RSI = 0x654321U;
    constexpr uint16_t MN = 1U;
    uint8_t tek[32U];
    for (uint32_t i = 0U; i < sizeof(tek); ++i)
        tek[i] = (uint8_t)i;

    EKCKeyItem item;
    item.id(1U); item.algId(ALGO_AES_256); item.kId(0x1234U); item.sln(1U);
    item.keyMaterial("000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F");
    FNETestHooks::addCryptoKey(harness.traffic, item);

    REQUIRE(FNETestHooks::processP25PDUResponse(harness.traffic, SU_RSI, 5U));

    KMMHello hello;
    hello.setDstLLId(WUID_FNE);
    hello.setSrcLLId(SU_RSI);
    hello.setResponseKind(KMM_ResponseKind::NONE);
    hello.setHasMessageNumber(true);
    hello.setMessageNumber(MN);
    hello.setFlag(KMM_HelloFlag::IDENT_ONLY);
    hello.setMACType(KMM_MAC::ENH_MAC);
    hello.setMACAlgId(ALGO_AES_256);
    hello.setMACKId(0x1234U);
    hello.setMACFormat(KMM_MAC_FORMAT_CBC);

    std::vector<uint8_t> encoded = encodeKMM(hello);
    hello.generateMAC(tek, encoded.data());
    REQUIRE(FNETestHooks::processOTARDLDPDU(harness.traffic, encoded, SU_RSI));
    REQUIRE(FNETestHooks::hasOTARInboundMessageNumber(harness.traffic, SU_RSI, MN));
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
        FNETestHooks::setKMFServicesEnabled(harness.traffic, true);

        KMMRegistrationCommand registration;
        registration.setDstLLId(WUID_FNE);
        registration.setSrcLLId(SU_RSI);
        registration.setResponseKind(KMM_ResponseKind::IMMEDIATE);
        registration.setKMFRSI(WUID_FNE);
        uint32_t registrationResponseSize = 0U;
        UInt8Array registrationResponse = FNETestHooks::processOTARKMM(harness.traffic,
            encodeKMM(registration), SU_RSI, registrationResponseSize,
            ALGO_UNENCRYPT, 0U, true);
        REQUIRE(registrationResponse != nullptr);
        std::unique_ptr<KMMFrame> decodedRegistration = KMMFactory::create(
            registrationResponse.get(), registrationResponseSize);
        REQUIRE(decodedRegistration != nullptr);
        REQUIRE(decodedRegistration->getMessageId() == KMM_MessageType::REG_RSP);

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

TEST_CASE("P25 OTAR Rekey Commands batch four keys per KMM", "[p25][kmm][otar][rekey]")
{
    KMMFNEHarness harness;
    constexpr uint32_t SU_RSI = 0x654321U;
    std::vector<uint16_t> allowed;

    for (uint16_t i = 0U; i < 9U; i++) {
        EKCKeyItem key;
        key.id(i + 1U);
        key.algId(ALGO_AES_256);
        key.kId((uint16_t)(0x2000U + i));
        key.sln((uint16_t)(0x0100U + i));
        key.keyMaterial("000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F");
        FNETestHooks::addCryptoKey(harness.traffic, key);
        allowed.push_back((uint16_t)key.kId());
    }

    EKCKeyItem ukek;
    ukek.id(100U);
    ukek.rsiId(SU_RSI);
    ukek.algId(ALGO_AES_256);
    ukek.kId(0x1001U);
    ukek.sln(0x0002U);
    ukek.keyMaterial("101112131415161718191A1B1C1D1E1F202122232425262728292A2B2C2D2E2F");
    FNETestHooks::addCryptoKey(harness.traffic, ukek);
    harness.rid.addEntry(SU_RSI, true, "batch-test", "", true, true, allowed);

    std::vector<std::vector<uint8_t>> frames =
        FNETestHooks::buildOTARRekey(harness.traffic, SU_RSI, SU_RSI);
    REQUIRE(frames.size() == 3U);

    bool outerEncrypted = false;
    uint8_t outerAlgId = ALGO_UNENCRYPT;
    uint16_t outerKId = 0U;
    REQUIRE(FNETestHooks::resolveOTARResponseSecurity(harness.traffic, frames[0U],
        outerEncrypted, outerAlgId, outerKId));
    CHECK(outerEncrypted);
    CHECK(outerAlgId == ALGO_AES_256);
    CHECK(outerKId == allowed[0U]);

    const uint32_t expectedCounts[] = { 4U, 4U, 1U };
    for (size_t i = 0U; i < frames.size(); i++) {
        REQUIRE(frames[i].size() >= 3U);
        const uint32_t declaredLength = (((uint32_t)frames[i][1U] << 8U) | frames[i][2U]) + 3U;
        REQUIRE(declaredLength == frames[i].size());
        // The bounded air-facing factory deliberately rejects nested-count
        // KMMs until it has a dedicated structural validator.  This is a
        // trusted, locally encoded RK3, so use the normal typed decoder here.
        std::unique_ptr<KMMFrame> decoded = KMMFactory::create(frames[i].data());
        REQUIRE(decoded != nullptr);
        KMMRekeyCommand* rekey = dynamic_cast<KMMRekeyCommand*>(decoded.get());
        REQUIRE(rekey != nullptr);
        REQUIRE(rekey->getKeysets().size() == 1U);
        CHECK(rekey->getKeysets()[0U].keys().size() == expectedCounts[i]);
        // A new RK3 uses Rule 1 message-number validation. The SU initializes
        // MNL to zero, so the first valid outbound KMF message number is one.
        CHECK(rekey->getMessageNumber() == i + 1U);
        CHECK(rekey->getComplete() == (i + 1U == frames.size()));
        CHECK(frames[i].size() <= 287U);
    }
}

TEST_CASE("P25 OTAR builds authenticated Changeover Commands without dispatching them",
    "[p25][kmm][otar][changeover]")
{
    KMMFNEHarness harness;
    constexpr uint32_t SU_RSI = 0x654321U;
    constexpr uint16_t TEK_KID = 0x2345U;
    constexpr uint8_t SUPERSEDED_KEYSET = 0x01U;
    constexpr uint8_t ACTIVE_KEYSET = 0x02U;

    EKCKeyItem tek;
    tek.id(1U);
    tek.algId(ALGO_AES_256);
    tek.kId(TEK_KID);
    tek.sln(1U);
    tek.keyMaterial("000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F");
    FNETestHooks::addCryptoKey(harness.traffic, tek);
    harness.rid.addEntry(SU_RSI, true, "changeover-test", "", true, true, { TEK_KID });

    const std::vector<uint8_t> encoded = FNETestHooks::buildOTARChangeover(harness.traffic,
        SU_RSI, SU_RSI, SUPERSEDED_KEYSET, ACTIVE_KEYSET);
    REQUIRE_FALSE(encoded.empty());

    std::unique_ptr<KMMFrame> base = KMMFactory::create(encoded.data(), (uint32_t)encoded.size());
    REQUIRE(base != nullptr);
    KMMChangeover* changeover = dynamic_cast<KMMChangeover*>(base.get());
    REQUIRE(changeover != nullptr);
    CHECK(changeover->getMessageId() == KMM_MessageType::CHANGEOVER_CMD);
    CHECK(changeover->getResponseKind() == KMM_ResponseKind::IMMEDIATE);
    CHECK(changeover->getSrcLLId() == WUID_FNE);
    CHECK(changeover->getDstLLId() == SU_RSI);
    CHECK(changeover->getSupersededKeysetId() == SUPERSEDED_KEYSET);
    CHECK(changeover->getActiveKeysetId() == ACTIVE_KEYSET);
    CHECK(changeover->getHasMessageNumber());
    CHECK(changeover->getMessageNumber() == 1U);
    CHECK(changeover->getMACType() == KMM_MAC::ENH_MAC);
    CHECK(changeover->getMACAlgId() == ALGO_AES_256);
    CHECK(changeover->getMACKId() == TEK_KID);

    uint8_t material[P25DEF::MAX_ENC_KEY_LENGTH_BYTES] = { 0U };
    tek.getKey(material);
    CHECK(changeover->verifyMAC(material, encoded.data(), (uint32_t)encoded.size()));

    bool encrypted = false;
    uint8_t algoId = ALGO_UNENCRYPT;
    uint16_t kid = 0U;
    REQUIRE(FNETestHooks::resolveOTARResponseSecurity(harness.traffic, encoded,
        encrypted, algoId, kid));
    CHECK(encrypted);
    CHECK(algoId == ALGO_AES_256);
    CHECK(kid == TEK_KID);
}

#if !defined(_WIN32)
TEST_CASE("P25 OTAR DLI traverses the real UDP endpoint", "[p25][kmm][otar][dli][integration]")
{
    KMMFNEHarness harness;
    constexpr uint32_t SU_RSI = 0x654321U;
    FNETestHooks::setKMFServicesEnabled(harness.traffic, true);

    int probe = ::socket(AF_INET, SOCK_DGRAM, 0);
    REQUIRE(probe >= 0);
    sockaddr_in probeAddress = {};
    probeAddress.sin_family = AF_INET;
    probeAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    probeAddress.sin_port = 0U;
    REQUIRE(::bind(probe, reinterpret_cast<sockaddr*>(&probeAddress), sizeof(probeAddress)) == 0);
    socklen_t probeLength = sizeof(probeAddress);
    REQUIRE(::getsockname(probe, reinterpret_cast<sockaddr*>(&probeAddress), &probeLength) == 0);
    const uint16_t serverPort = ntohs(probeAddress.sin_port);
    ::close(probe);

    REQUIRE(FNETestHooks::openOTARDLI(harness.traffic, "127.0.0.1", serverPort));

    int client = ::socket(AF_INET, SOCK_DGRAM, 0);
    REQUIRE(client >= 0);
    sockaddr_in serverAddress = {};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serverAddress.sin_port = htons(serverPort);

    KMMRegistrationCommand registration;
    registration.setDstLLId(WUID_FNE);
    registration.setSrcLLId(SU_RSI);
    registration.setResponseKind(KMM_ResponseKind::IMMEDIATE);
    registration.setKMFRSI(WUID_FNE);
    std::vector<uint8_t> registrationKMM = encodeKMM(registration);
    std::vector<uint8_t> datagram(14U + registrationKMM.size(), 0U);
    datagram[0U] = 0U;
    datagram[1U] = MFG_STANDARD;
    datagram[2U] = ALGO_UNENCRYPT;
    ::memcpy(datagram.data() + 14U, registrationKMM.data(), registrationKMM.size());
    REQUIRE(::sendto(client, datagram.data(), datagram.size(), 0,
        reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == (ssize_t)datagram.size());

    std::vector<uint8_t> response(512U, 0U);
    ssize_t responseLength = -1;
    for (uint32_t attempt = 0U; attempt < 100U && responseLength < 0; ++attempt) {
        FNETestHooks::clockOTARDLI(harness.traffic);
        responseLength = ::recvfrom(client, response.data(), response.size(), MSG_DONTWAIT, nullptr, nullptr);
        if (responseLength < 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    REQUIRE(responseLength >= 24);
    response.resize((size_t)responseLength);
    REQUIRE(response[0U] == 0U);
    REQUIRE(response[1U] == MFG_STANDARD);
    REQUIRE(response[2U] == ALGO_UNENCRYPT);
    std::unique_ptr<KMMFrame> decoded = KMMFactory::create(response.data() + 14U,
        (uint32_t)response.size() - 14U);
    REQUIRE(decoded != nullptr);
    REQUIRE(decoded->getMessageId() == KMM_MessageType::REG_RSP);
    REQUIRE(decoded->getDstLLId() == SU_RSI);

    ::close(client);
    FNETestHooks::closeOTARDLI(harness.traffic);
}
#endif

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
