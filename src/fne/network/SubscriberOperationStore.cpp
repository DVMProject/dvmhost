// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/network/SubscriberOperationStore.h"

#include <algorithm>

using namespace network;

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const std::chrono::seconds OPERATION_TIMEOUT(8U);
const std::chrono::minutes RESULT_RETENTION(5U);
const std::chrono::minutes STATUS_RETENTION(15U);

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes an empty operation store. */

SubscriberOperationStore::SubscriberOperationStore() :
    m_nextId(1U),
    m_operations(),
    m_p25Statuses(),
    m_mutex()
{
    /* stub */
}

/* Starts a uniquely correlatable operation. */

uint64_t SubscriberOperationStore::start(Protocol protocol, Kind kind, uint32_t targetId, uint32_t peerId, uint16_t requestValue)
{
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(m_mutex);
    expireLocked(now);

    // check for existing pending operation with the same protocol, kind, and target ID
    for (const auto& entry : m_operations) {
        const Operation& operation = entry.second.value;
        if (operation.state == State::PENDING && operation.protocol == protocol &&
            operation.kind == kind && operation.targetId == targetId) {
            return 0U;
        }
    }

    // create a new operation with a unique ID and store it with expiration times
    uint64_t id = m_nextId++;
    if (id == 0U) {
        id = m_nextId++;
    }

    const uint64_t createdAt = unixMilliseconds();
    Operation operation { id, protocol, kind, State::PENDING, targetId, peerId, requestValue, 0U, createdAt, 0U };
    m_operations.emplace(id, StoredOperation { operation, now + OPERATION_TIMEOUT, now + RESULT_RETENTION });
    return id;
}

/* Completes the matching active operation. */

bool SubscriberOperationStore::complete(Protocol protocol, Kind kind, uint32_t targetId, uint32_t peerId,
    uint16_t resultValue, bool success)
{
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(m_mutex);

    // expire any operations that have timed out or exceeded retention before attempting completion
    expireLocked(now);

    // find the matching pending operation and complete it
    for (auto& entry : m_operations) {
        StoredOperation& stored = entry.second;
        Operation& operation = stored.value;
        if (operation.state == State::PENDING && operation.protocol == protocol && operation.kind == kind && operation.targetId == targetId &&
            (operation.peerId == 0U || operation.peerId == peerId)) {
            operation.state = success ? State::SUCCEEDED : State::REJECTED;
            operation.peerId = peerId;
            operation.resultValue = resultValue;
            operation.completedAt = unixMilliseconds();
            stored.retainUntil = now + RESULT_RETENTION;
            return true;
        }
    }

    return false;
}

/* Adds a uniquely correlatable one-shot operation. */

uint64_t SubscriberOperationStore::oneShot(Protocol protocol, Kind kind, uint32_t targetId, uint32_t peerId, uint16_t requestValue)
{
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(m_mutex);
    expireLocked(now);

    // suppress an equivalent one-shot while its result remains available
    for (const auto& entry : m_operations) {
        const Operation& operation = entry.second.value;
        if (operation.protocol == protocol && operation.kind == kind && operation.targetId == targetId) {
            return 0U;
        }
    }

    // create a new operation with a unique ID and store it with expiration times
    uint64_t id = m_nextId++;
    if (id == 0U) {
        id = m_nextId++;
    }

    const uint64_t createdAt = unixMilliseconds();
    Operation operation { id, protocol, kind, State::SUCCEEDED, targetId, peerId, requestValue, 0U, createdAt, createdAt };
    m_operations.emplace(id, StoredOperation { operation, now + OPERATION_TIMEOUT, now + RESULT_RETENTION });
    return id;
}

/* Adds a status report and completes a matching P25 status query. */

bool SubscriberOperationStore::addP25Status(uint32_t subscriberId, uint32_t peerId, uint16_t value)
{
    const bool solicited = complete(Protocol::P25, Kind::STATUS_QUERY, subscriberId, peerId, value, true);
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(m_mutex);

    // expire any operations that have timed out or exceeded retention before recording the status
    expireLocked(now);

    // record the status report with the current timestamp and retention time
    Status status { subscriberId, peerId, value, solicited, unixMilliseconds() };
    m_p25Statuses[subscriberId] = StoredStatus { status, now + STATUS_RETENTION };
    return solicited;
}

/* Retrieves an operation snapshot. */

bool SubscriberOperationStore::get(uint64_t id, Operation& operation)
{
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(m_mutex);

    // expire any operations that have timed out or exceeded retention before retrieving the snapshot
    expireLocked(now);

    // look up the operation by its ID
    const auto it = m_operations.find(id);
    if (it == m_operations.end()) {
        return false;
    }

    operation = it->second.value;
    return true;
}

/* Retrieves the most recently started operations. */

std::vector<SubscriberOperationStore::Operation> SubscriberOperationStore::getLast(uint32_t count)
{
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(m_mutex);

    // expire records before taking the history snapshot
    expireLocked(now);

    std::vector<Operation> operations;
    operations.reserve(std::min<size_t>(count, m_operations.size()));
    for (const auto& entry : m_operations) {
        operations.push_back(entry.second.value);
    }

    std::sort(operations.begin(), operations.end(), [](const Operation& lhs, const Operation& rhs) {
        return lhs.id > rhs.id;
    });
    if (operations.size() > count) {
        operations.resize(count);
    }

    return operations;
}

/* Retrieves the latest P25 status snapshot. */

bool SubscriberOperationStore::getP25Status(uint32_t subscriberId, Status& status)
{
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(m_mutex);

    // expire any operations that have timed out or exceeded retention before retrieving the status
    expireLocked(now);

    // look up the latest status for the given subscriber ID
    const auto it = m_p25Statuses.find(subscriberId);
    if (it == m_p25Statuses.end()) {
        return false;
    }

    status = it->second.value;
    return true;
}

/* Expires pending operations and removes retained results. */

void SubscriberOperationStore::expire()
{
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(m_mutex);

    // expire any operations that have timed out or exceeded retention before performing general expiration
    expireLocked(now);
}

/* Returns a stable protocol name. */

const char* SubscriberOperationStore::protocolName(Protocol protocol)
{
    switch (protocol) {
    case Protocol::DMR:
        return "dmr";
    case Protocol::P25:
        return "p25";
    case Protocol::NXDN:
        return "nxdn";
    }

    return "unknown";
}

/* Returns a stable operation name. */

const char* SubscriberOperationStore::kindName(Kind kind)
{
    switch (kind) {
    case Kind::RADIO_CHECK:
        return "radio-check";
    case Kind::STATUS_QUERY:
        return "status-query";
    case Kind::MESSAGE_UPDATE:
        return "message-update";
    case Kind::INHIBIT:
        return "inhibit";
    case Kind::UNINHIBIT:
        return "uninhibit";
    case Kind::KMM_REKEY:
        return "kmm-rekey";
    case Kind::KMM_CHANGEOVER:
        return "kmm-changeover";
    case Kind::KMM_CHANGERSI:
        return "kmm-changersi";
    case Kind::EMERG_ALRM:
        return "emerg-alrm";
    case Kind::EMERG_CALL:
        return "emerg-call";
    }

    return "unknown";
}

/* Returns a stable state name. */

const char* SubscriberOperationStore::stateName(State state)
{
    switch (state) {
    case State::PENDING:
        return "pending";
    case State::SUCCEEDED:
        return "succeeded";
    case State::REJECTED:
        return "rejected";
    case State::TIMED_OUT:
        return "timed-out";
    }

    return "unknown";
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Gets the current Unix timestamp. */

uint64_t SubscriberOperationStore::unixMilliseconds()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

/* Expires records while the store mutex is held. */

void SubscriberOperationStore::expireLocked(const std::chrono::steady_clock::time_point& now)
{
    // iterate through all stored operations and expire those that have timed out or exceeded retention
    for (auto it = m_operations.begin(); it != m_operations.end();) {
        StoredOperation& stored = it->second;

        // check if the operation has timed out and update its state accordingly
        if (stored.value.state == State::PENDING && now >= stored.deadline) {
            stored.value.state = State::TIMED_OUT;
            stored.value.completedAt = unixMilliseconds();
            stored.retainUntil = now + RESULT_RETENTION;
        }

        // remove operations that have completed and exceeded their retention period
        if (stored.value.state != State::PENDING && now >= stored.retainUntil) {
            it = m_operations.erase(it);
        } else {
            ++it;
        }
    }

    // iterate through all stored P25 statuses and remove those that have exceeded their retention period
    for (auto it = m_p25Statuses.begin(); it != m_p25Statuses.end();) {
        if (now >= it->second.retainUntil) {
            it = m_p25Statuses.erase(it);
        } else {
            ++it;
        }
    }
}
