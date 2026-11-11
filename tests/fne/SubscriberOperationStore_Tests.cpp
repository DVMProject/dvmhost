// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/network/SubscriberOperationStore.h"

#include <catch2/catch_test_macros.hpp>

using namespace network;

TEST_CASE("FNE subscriber operations correlate protocol results", "[fne][subscriber-operations]")
{
    SubscriberOperationStore store;
    const uint32_t targetId = 0x123456U;

    uint64_t operationId = store.start(SubscriberOperationStore::Protocol::P25,
        SubscriberOperationStore::Kind::STATUS_QUERY, targetId, 42U);
    REQUIRE(operationId != 0U);
    CHECK(store.start(SubscriberOperationStore::Protocol::P25,
              SubscriberOperationStore::Kind::STATUS_QUERY, targetId, 42U) == 0U);
    CHECK_FALSE(store.addP25Status(targetId, 43U, 0x1234U));
    CHECK(store.addP25Status(targetId, 42U, 0x5678U));

    SubscriberOperationStore::Operation operation;
    REQUIRE(store.get(operationId, operation));
    CHECK(operation.state == SubscriberOperationStore::State::SUCCEEDED);
    CHECK(operation.resultValue == 0x5678U);

    SubscriberOperationStore::Status status;
    REQUIRE(store.getP25Status(targetId, status));
    CHECK(status.value == 0x5678U);
    CHECK(status.solicited);
}

TEST_CASE("FNE subscriber operations isolate protocols and kinds", "[fne][subscriber-operations]")
{
    SubscriberOperationStore store;
    const uint32_t targetId = 0x654321U;
    const uint64_t dmrId = store.start(SubscriberOperationStore::Protocol::DMR,
        SubscriberOperationStore::Kind::RADIO_CHECK, targetId, 0U);
    const uint64_t p25Id = store.start(SubscriberOperationStore::Protocol::P25,
        SubscriberOperationStore::Kind::RADIO_CHECK, targetId, 0U);

    REQUIRE(dmrId != 0U);
    REQUIRE(p25Id != 0U);
    REQUIRE(store.complete(SubscriberOperationStore::Protocol::DMR,
        SubscriberOperationStore::Kind::RADIO_CHECK, targetId, 12U));

    SubscriberOperationStore::Operation dmrOperation;
    SubscriberOperationStore::Operation p25Operation;
    REQUIRE(store.get(dmrId, dmrOperation));
    REQUIRE(store.get(p25Id, p25Operation));
    CHECK(dmrOperation.state == SubscriberOperationStore::State::SUCCEEDED);
    CHECK(dmrOperation.peerId == 12U);
    CHECK(p25Operation.state == SubscriberOperationStore::State::PENDING);
}
