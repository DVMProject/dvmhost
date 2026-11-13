// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/Defines.h"
#include "common/Log.h"
#include "common/Thread.h"
#include "network/TrafficNetwork.h"
#include "FNEMain.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <limits>
#include <memory>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace network;

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define SQLITE_BATCH_SIZE 256U
#define SQLITE_MAX_QUEUE_DEPTH 32768U
#define SQLITE_BATCH_WAIT_MS 10U

static const uint64_t NS_PER_SECOND = 1000000000ULL;
static const uint64_t NS_PER_MINUTE = 60ULL * NS_PER_SECOND;
static const uint64_t NS_PER_DAY = 86400ULL * NS_PER_SECOND;

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::atomic<int32_t> TrafficNetwork::MetricsLogging::s_totalActiveCalls {0};
std::atomic<uint64_t> TrafficNetwork::MetricsLogging::s_totalCallsProcessed {0U};
std::atomic<uint64_t> TrafficNetwork::MetricsLogging::s_totalCallCollisions {0U};
std::atomic<uint64_t> TrafficNetwork::MetricsLogging::s_totalCallSwitches {0U};

namespace {
    static const char* SQLITE_COUNTER_CALLS_PROCESSED = "total_calls_processed";
    static const char* SQLITE_COUNTER_CALL_COLLISIONS = "total_call_collisions";
    static const char* SQLITE_COUNTER_CALL_SWITCHES = "total_call_switches";

    /**
     * @brief Get the current time in nanoseconds since the epoch.
     * @return The current time in nanoseconds since the epoch.
     */
    static inline uint64_t nowNs()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    /**
     * @brief Types of metric events.
     */
    enum class MetricEventType {
        ACTIVITY,
        DIAG,
        CALL_EVENT,
        CALL_ERROR_EVENT,
        CALL_COLLISION_EVENT,
        TSBK_EVENT,
        CSBK_EVENT,
        COUNTER_EVENT
    };

    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents a metric event with various attributes.
     */
    struct MetricEvent {
        MetricEventType type;
        uint64_t tsNs;
        uint32_t peerId;
        uint32_t streamId;
        uint32_t srcId;
        uint32_t dstId;
        uint64_t durationMs;
        uint8_t slotNo;
        bool hasSlot;

        std::string mode;
        std::string identity;
        std::string message;
        std::string raw;
        std::string lco;
        std::string rawDescription;
        std::string counterKey;
        uint64_t counterValue;

        uint32_t rxPeerId;
        uint32_t rxStreamId;
        uint32_t rxSrcId;
        uint32_t rxDstId;
        uint8_t rxSlot;
        bool hasRxSlot;

        /**
         * @brief Default constructor for MetricEvent. Initializes all members to default values.
         */
        MetricEvent() :
            type(MetricEventType::ACTIVITY),
            tsNs(0U),
            peerId(0U),
            streamId(0U),
            srcId(0U),
            dstId(0U),
            durationMs(0U),
            slotNo(0U),
            hasSlot(false),
            counterKey(),
            counterValue(0U),
            rxPeerId(0U),
            rxStreamId(0U),
            rxSrcId(0U),
            rxDstId(0U),
            rxSlot(0U),
            hasRxSlot(false)
        {
            /* stub */
        }
    };

    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Holds prepared SQLite statements for different metric event types.
     */
    struct SQLiteStatements {
        sqlite3_stmt* activity;
        sqlite3_stmt* diag;
        sqlite3_stmt* callEvent;
        sqlite3_stmt* callErrorEvent;
        sqlite3_stmt* callCollisionEvent;
        sqlite3_stmt* tsbkEvent;
        sqlite3_stmt* csbkEvent;
        sqlite3_stmt* counterUpsert;

        /**
         * @brief Default constructor for SQLiteStatements. Initializes all statement pointers to nullptr.
         */
        SQLiteStatements() :
            activity(nullptr),
            diag(nullptr),
            callEvent(nullptr),
            callErrorEvent(nullptr),
            callCollisionEvent(nullptr),
            tsbkEvent(nullptr),
            csbkEvent(nullptr),
            counterUpsert(nullptr)
        {
            /* stub */
        }
    };

    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Lock-free queue for metric events.
     */
    struct LockFreeMetricQueue {
        std::vector<std::atomic<MetricEvent*>> slots;
        std::atomic<size_t> head;
        std::atomic<size_t> tail;
        std::atomic<size_t> pendingCount;
        size_t maxDepth;

        /**
         * @brief Constructor for LockFreeMetricQueue.
         * @param depth The maximum depth of the queue.
         */
        explicit LockFreeMetricQueue(size_t depth) :
            slots(depth),
            head(0U),
            tail(0U),
            pendingCount(0U),
            maxDepth(depth)
        {
            for (auto& slot : slots) {
                slot.store(nullptr, std::memory_order_relaxed);
            }
        }

        /**
         * @brief Attempts to push a metric event into the queue.
         * @param event The metric event to push.
         * @return True if the event was successfully pushed, false otherwise.
         */
        bool tryPush(MetricEvent&& event)
        {
            const size_t capacity = slots.size();
            if (capacity == 0U || maxDepth == 0U) {
                return false;
            }

            size_t tailValue = tail.load(std::memory_order_relaxed);

            // attempt to reserve a slot in the queue
            while (true) {
                const size_t headValue = head.load(std::memory_order_acquire);
                const size_t pending = tailValue - headValue;
                if (pending >= maxDepth) {
                    LogError(LOG_MASTER, "Failed to push SQLite metric event: queue is full, pending = %zu, maxDepth = %zu", pending, maxDepth);
                    return false;
                }

                if (tail.compare_exchange_weak(tailValue, tailValue + 1U, std::memory_order_relaxed, std::memory_order_relaxed)) {
                    const size_t slotIndex = tailValue % capacity;
                    MetricEvent* owned = new MetricEvent(std::move(event));
                    slots[slotIndex].store(owned, std::memory_order_release);
                    pendingCount.fetch_add(1U, std::memory_order_release);
                    return true;
                }
            }
        }

        /**
         * @brief Attempts to pop a metric event from the queue.
         * @param out The metric event to populate with the popped event.
         * @return True if an event was successfully popped, false otherwise.
         */
        bool tryPop(MetricEvent& out)
        {
            const size_t capacity = slots.size();
            if (capacity == 0U) {
                return false;
            }

            const size_t headValue = head.load(std::memory_order_relaxed);
            const size_t tailValue = tail.load(std::memory_order_acquire);
            if (headValue >= tailValue) {
                return false;
            }

            const size_t slotIndex = headValue % capacity;
            MetricEvent* item = slots[slotIndex].load(std::memory_order_acquire);
            if (item == nullptr) {
                return false;
            }

            out = std::move(*item);
            delete item;
            slots[slotIndex].store(nullptr, std::memory_order_release);
            head.store(headValue + 1U, std::memory_order_release);
            pendingCount.fetch_sub(1U, std::memory_order_release);
            return true;
        }
    };

    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Holds the state for the SQLite writer, including the queue, worker thread, and prepared statements.
     */
    struct SQLiteWriterState {
        LockFreeMetricQueue queue;
        thread_t workerThread;
        std::atomic<bool> workerStarted;
        sqlite3* db;
        SQLiteStatements statements;
        std::atomic<bool> stopRequested;
        uint64_t droppedCount;
        uint32_t retentionDays;
        uint32_t pruneIntervalMinutes;
        uint64_t nextPruneNs;

        /**
         * @brief Default constructor for SQLiteWriterState. Initializes all members to default values.
         */
        SQLiteWriterState() :
            queue(SQLITE_MAX_QUEUE_DEPTH),
            workerThread(),
            workerStarted(false),
            db(nullptr),
            statements(),
            stopRequested(false),
            droppedCount(0U),
            retentionDays(0U),
            pruneIntervalMinutes(0U),
            nextPruneNs(0U)
        {
            /* stub */
        }
    };

    static std::atomic<SQLiteWriterState*> g_sqliteWriterState;
    static std::shared_ptr<SQLiteWriterState> g_sqliteWriterStateHolder;

    // ---------------------------------------------------------------------------
    //  Global Functions
    // ---------------------------------------------------------------------------

    /**
     * @brief Binds a text value to a SQLite statement at the specified index.
     * @param stmt The SQLite statement.
     * @param idx The index of the parameter to bind.
     * @param value The text value to bind.
     * @return SQLITE_OK if the binding was successful, otherwise an error code.
     */
    static inline int bindText(sqlite3_stmt* stmt, int idx, const std::string& value)
    {
        return sqlite3_bind_text(stmt, idx, value.c_str(), -1, SQLITE_TRANSIENT);
    }

    /**
     * @brief Finalizes all prepared SQLite statements and sets their pointers to nullptr.
     * @param stmts The SQLiteStatements structure containing the prepared statements.
     */
    static void finalizeStatements(SQLiteStatements& stmts)
    {
        if (stmts.activity != nullptr) {
            sqlite3_finalize(stmts.activity);
            stmts.activity = nullptr;
        }
        if (stmts.diag != nullptr) {
            sqlite3_finalize(stmts.diag);
            stmts.diag = nullptr;
        }
        if (stmts.callEvent != nullptr) {
            sqlite3_finalize(stmts.callEvent);
            stmts.callEvent = nullptr;
        }
        if (stmts.callErrorEvent != nullptr) {
            sqlite3_finalize(stmts.callErrorEvent);
            stmts.callErrorEvent = nullptr;
        }
        if (stmts.callCollisionEvent != nullptr) {
            sqlite3_finalize(stmts.callCollisionEvent);
            stmts.callCollisionEvent = nullptr;
        }
        if (stmts.tsbkEvent != nullptr) {
            sqlite3_finalize(stmts.tsbkEvent);
            stmts.tsbkEvent = nullptr;
        }
        if (stmts.csbkEvent != nullptr) {
            sqlite3_finalize(stmts.csbkEvent);
            stmts.csbkEvent = nullptr;
        }
        if (stmts.counterUpsert != nullptr) {
            sqlite3_finalize(stmts.counterUpsert);
            stmts.counterUpsert = nullptr;
        }
    }

    /**
     * @brief Prepares a single SQLite statement.
     * @param db The SQLite database connection.
     * @param sql The SQL query string.
     * @param stmt Pointer to the SQLite statement to be prepared.
     * @return true if the statement was prepared successfully, false otherwise.
     */
    static bool prepareStatement(sqlite3* db, const char* sql, sqlite3_stmt** stmt)
    {
        if (sqlite3_prepare_v2(db, sql, -1, stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        return true;
    }

    /**
     * @brief Prepares all SQLite statements in the given SQLiteStatements structure.
     * @param db The SQLite database connection.
     * @param stmts The SQLiteStatements structure to be prepared.
     * @return true if all statements were prepared successfully, false otherwise.
     */
    static bool prepareStatements(sqlite3* db, SQLiteStatements& stmts)
    {
        finalizeStatements(stmts);

        if (!prepareStatement(db, "INSERT INTO activity(ts_ns, peer_id, identity, msg) VALUES(?, ?, ?, ?);", &stmts.activity))
            return false;
        if (!prepareStatement(db, "INSERT INTO diag(ts_ns, peer_id, identity, msg) VALUES(?, ?, ?, ?);", &stmts.diag))
            return false;
        if (!prepareStatement(db, "INSERT INTO call_event(ts_ns, peer_id, mode, stream_id, src_id, dst_id, duration_ms, slot) VALUES(?, ?, ?, ?, ?, ?, ?, ?);", &stmts.callEvent))
            return false;
        if (!prepareStatement(db, "INSERT INTO call_error_event(ts_ns, peer_id, stream_id, src_id, dst_id, message, slot) VALUES(?, ?, ?, ?, ?, ?, ?);", &stmts.callErrorEvent))
            return false;
        if (!prepareStatement(db, "INSERT INTO call_collision_event(ts_ns, peer_id, stream_id, src_id, dst_id, slot, rx_peer_id, rx_stream_id, rx_src_id, rx_dst_id, rx_slot) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);", &stmts.callCollisionEvent))
            return false;
        if (!prepareStatement(db, "INSERT INTO tsbk_event(ts_ns, peer_id, lco, tsbk, raw) VALUES(?, ?, ?, ?, ?);", &stmts.tsbkEvent))
            return false;
        if (!prepareStatement(db, "INSERT INTO csbk_event(ts_ns, peer_id, lco, csbk, raw) VALUES(?, ?, ?, ?, ?);", &stmts.csbkEvent))
            return false;
        if (!prepareStatement(db,
                "INSERT INTO metrics_counters(key, value) VALUES(?, ?) "
                "ON CONFLICT(key) DO UPDATE SET value = excluded.value;",
                &stmts.counterUpsert))
            return false;

        return true;
    }

    /**
     * @brief Executes a SQLite statement, resets it, and clears its bindings.
     * @param stmt The SQLite statement to execute.
     * @return true if the statement executed successfully, false otherwise.
     */
    static bool execStepAndReset(sqlite3_stmt* stmt)
    {
        int rc = sqlite3_step(stmt);
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        return rc == SQLITE_DONE;
    }

    /**
     * @brief Inserts a metric event into the appropriate SQLite statement.
     * @param stmts The SQLiteStatements structure containing the prepared statements.
     * @param e The metric event to insert.
     * @return true if the event was inserted successfully, false otherwise.
     */
    static bool insertEvent(SQLiteStatements& stmts, const MetricEvent& e)
    {
        sqlite3_stmt* stmt = nullptr;

        switch (e.type) {
        case MetricEventType::ACTIVITY:
            stmt = stmts.activity;
            if (stmt == nullptr) 
                return false;
            if (sqlite3_bind_int64(stmt, 1, (sqlite3_int64)e.tsNs) != SQLITE_OK) 
                return false;
            if (sqlite3_bind_int(stmt, 2, (int)e.peerId) != SQLITE_OK) 
                return false;
            if (bindText(stmt, 3, e.identity) != SQLITE_OK) 
                return false;
            if (bindText(stmt, 4, e.message) != SQLITE_OK) 
                return false;
            return execStepAndReset(stmt);

        case MetricEventType::DIAG:
            stmt = stmts.diag;
            if (stmt == nullptr) 
                return false;
            if (sqlite3_bind_int64(stmt, 1, (sqlite3_int64)e.tsNs) != SQLITE_OK) 
                return false;
            if (sqlite3_bind_int(stmt, 2, (int)e.peerId) != SQLITE_OK)
                return false;
            if (bindText(stmt, 3, e.identity) != SQLITE_OK) 
                return false;
            if (bindText(stmt, 4, e.message) != SQLITE_OK) 
                return false;
            return execStepAndReset(stmt);

        case MetricEventType::CALL_EVENT:
            stmt = stmts.callEvent;
            if (stmt == nullptr) return false;
            if (sqlite3_bind_int64(stmt, 1, (sqlite3_int64)e.tsNs) != SQLITE_OK) 
                return false;
            if (sqlite3_bind_int(stmt, 2, (int)e.peerId) != SQLITE_OK) 
                return false;
            if (bindText(stmt, 3, e.mode) != SQLITE_OK) 
                return false;
            if (sqlite3_bind_int(stmt, 4, (int)e.streamId) != SQLITE_OK) 
                return false;
            if (sqlite3_bind_int(stmt, 5, (int)e.srcId) != SQLITE_OK) 
                return false;
            if (sqlite3_bind_int(stmt, 6, (int)e.dstId) != SQLITE_OK) 
                return false;
            if (sqlite3_bind_int64(stmt, 7, (sqlite3_int64)e.durationMs) != SQLITE_OK) 
                return false;
            if (e.hasSlot) {
                if (sqlite3_bind_int(stmt, 8, (int)e.slotNo) != SQLITE_OK) 
                    return false;
            } else {
                if (sqlite3_bind_null(stmt, 8) != SQLITE_OK) 
                    return false;
            }
            return execStepAndReset(stmt);

        case MetricEventType::CALL_ERROR_EVENT:
            stmt = stmts.callErrorEvent;
            if (stmt == nullptr) return false;
            if (sqlite3_bind_int64(stmt, 1, (sqlite3_int64)e.tsNs) != SQLITE_OK) 
                return false;
            if (sqlite3_bind_int(stmt, 2, (int)e.peerId) != SQLITE_OK) 
                return false;
            if (sqlite3_bind_int(stmt, 3, (int)e.streamId) != SQLITE_OK) 
                return false;
            if (sqlite3_bind_int(stmt, 4, (int)e.srcId) != SQLITE_OK) 
                return false;
            if (sqlite3_bind_int(stmt, 5, (int)e.dstId) != SQLITE_OK) 
                return false;
            if (bindText(stmt, 6, e.message) != SQLITE_OK) 
                return false;
            if (e.hasSlot) {
                if (sqlite3_bind_int(stmt, 7, (int)e.slotNo) != SQLITE_OK) 
                    return false;
            } else {
                if (sqlite3_bind_null(stmt, 7) != SQLITE_OK) 
                    return false;
            }
            return execStepAndReset(stmt);

        case MetricEventType::CALL_COLLISION_EVENT:
            stmt = stmts.callCollisionEvent;
            if (stmt == nullptr) return false;
            if (sqlite3_bind_int64(stmt, 1, (sqlite3_int64)e.tsNs) != SQLITE_OK) 
                return false;
            if (sqlite3_bind_int(stmt, 2, (int)e.peerId) != SQLITE_OK) 
                return false;
            if (sqlite3_bind_int(stmt, 3, (int)e.streamId) != SQLITE_OK) 
                return false;
            if (sqlite3_bind_int(stmt, 4, (int)e.srcId) != SQLITE_OK) 
                return false;
            if (sqlite3_bind_int(stmt, 5, (int)e.dstId) != SQLITE_OK) 
                return false;
            if (e.hasSlot) {
                if (sqlite3_bind_int(stmt, 6, (int)e.slotNo) != SQLITE_OK) 
                    return false;
            } else {
                if (sqlite3_bind_null(stmt, 6) != SQLITE_OK) 
                    return false;
            }
            if (sqlite3_bind_int(stmt, 7, (int)e.rxPeerId) != SQLITE_OK) 
                return false;
            if (sqlite3_bind_int(stmt, 8, (int)e.rxStreamId) != SQLITE_OK) 
                return false;
            if (sqlite3_bind_int(stmt, 9, (int)e.rxSrcId) != SQLITE_OK) 
                return false;
            if (sqlite3_bind_int(stmt, 10, (int)e.rxDstId) != SQLITE_OK) 
                return false;
            if (e.hasRxSlot) {
                if (sqlite3_bind_int(stmt, 11, (int)e.rxSlot) != SQLITE_OK) 
                    return false;
            } else {
                if (sqlite3_bind_null(stmt, 11) != SQLITE_OK) 
                    return false;
            }
            return execStepAndReset(stmt);

        case MetricEventType::TSBK_EVENT:
            stmt = stmts.tsbkEvent;
            if (stmt == nullptr) return false;
            if (sqlite3_bind_int64(stmt, 1, (sqlite3_int64)e.tsNs) != SQLITE_OK) 
                return false;
            if (sqlite3_bind_int(stmt, 2, (int)e.peerId) != SQLITE_OK) 
                return false;
            if (bindText(stmt, 3, e.lco) != SQLITE_OK) 
                return false;
            if (bindText(stmt, 4, e.rawDescription) != SQLITE_OK) 
                return false;
            if (bindText(stmt, 5, e.raw) != SQLITE_OK) 
                return false;
            return execStepAndReset(stmt);

        case MetricEventType::CSBK_EVENT:
            stmt = stmts.csbkEvent;
            if (stmt == nullptr) return false;
            if (sqlite3_bind_int64(stmt, 1, (sqlite3_int64)e.tsNs) != SQLITE_OK) 
                return false;
            if (sqlite3_bind_int(stmt, 2, (int)e.peerId) != SQLITE_OK) 
                return false;
            if (bindText(stmt, 3, e.lco) != SQLITE_OK) 
                return false;
            if (bindText(stmt, 4, e.rawDescription) != SQLITE_OK) 
                return false;
            if (bindText(stmt, 5, e.raw) != SQLITE_OK) 
                return false;
            return execStepAndReset(stmt);

        case MetricEventType::COUNTER_EVENT:
            stmt = stmts.counterUpsert;
            if (stmt == nullptr)
                return false;
            if (bindText(stmt, 1, e.counterKey) != SQLITE_OK)
                return false;
            if (sqlite3_bind_int64(stmt, 2, (sqlite3_int64)e.counterValue) != SQLITE_OK)
                return false;
            return execStepAndReset(stmt);
        }

        return false;
    }

    /**
     * @brief Calculates the next prune time in nanoseconds from now for the given interval.
     * @param intervalMinutes Prune interval in minutes.
     * @return Next prune timestamp in nanoseconds since epoch, or 0 if interval is disabled.
     */
    static uint64_t calculateNextPruneNs(uint32_t intervalMinutes)
    {
        if (intervalMinutes == 0U) {
            return 0U;
        }

        uint64_t now = nowNs();
        uint64_t delta = (uint64_t)intervalMinutes * NS_PER_MINUTE;
        if (std::numeric_limits<uint64_t>::max() - now < delta) {
            return std::numeric_limits<uint64_t>::max();
        }

        return now + delta;
    }

    /**
     * @brief Prunes SQLite metric event rows older than the specified number of retention days.
     * @param db The SQLite database handle.
     * @param retentionDays Number of days to retain.
     * @param prunedRows Optional pointer to receive the number of rows deleted.
     * @return true if prune operation succeeded or is disabled, false on SQLite errors.
     */
    static bool pruneSQLiteMetricEvents(sqlite3* db, uint32_t retentionDays, uint64_t* prunedRows)
    {
        if (prunedRows != nullptr) {
            *prunedRows = 0U;
        }

        if (db == nullptr || retentionDays == 0U) {
            return true;
        }

        uint64_t now = nowNs();
        uint64_t windowNs = (uint64_t)retentionDays * NS_PER_DAY;
        if (now <= windowNs) {
            return true;
        }

        uint64_t cutoffNs = now - windowNs;

        char* errMsg = nullptr;
        if (sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
            LogError(LOG_MASTER, "SQLite metrics prune begin transaction failed: %s", (errMsg != nullptr) ? errMsg : "unknown error");
            if (errMsg != nullptr) {
                sqlite3_free(errMsg);
            }
            return false;
        }

        const char* tables[] = {
            "activity",
            "diag",
            "call_event",
            "call_error_event",
            "call_collision_event",
            "tsbk_event",
            "csbk_event"
        };

        uint64_t deleted = 0U;
        bool ok = true;

        for (const char* table : tables) {
            sqlite3_stmt* stmt = nullptr;
            std::string sql = "DELETE FROM ";
            sql += table;
            sql += " WHERE ts_ns < ?;";

            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                LogError(LOG_MASTER, "SQLite metrics prune prepare failed for table %s: %s", table, sqlite3_errmsg(db));
                ok = false;
                if (stmt != nullptr) {
                    sqlite3_finalize(stmt);
                }
                break;
            }

            if (sqlite3_bind_int64(stmt, 1, (sqlite3_int64)cutoffNs) != SQLITE_OK) {
                LogError(LOG_MASTER, "SQLite metrics prune bind failed for table %s: %s", table, sqlite3_errmsg(db));
                ok = false;
                sqlite3_finalize(stmt);
                break;
            }

            int rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                LogError(LOG_MASTER, "SQLite metrics prune delete failed for table %s: %s", table, sqlite3_errmsg(db));
                ok = false;
                sqlite3_finalize(stmt);
                break;
            }

            deleted += (uint64_t)sqlite3_changes(db);
            sqlite3_finalize(stmt);
        }

        if (ok) {
            if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
                LogError(LOG_MASTER, "SQLite metrics prune commit failed: %s", (errMsg != nullptr) ? errMsg : "unknown error");
                if (errMsg != nullptr) {
                    sqlite3_free(errMsg);
                }
                sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                return false;
            }
        } else {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }

        if (prunedRows != nullptr) {
            *prunedRows = deleted;
        }

        return true;
    }

    /**
     * @brief Enqueues a metric event to be written to the SQLite database by the writer thread.
     * @param network Pointer to the TrafficNetwork instance.
     * @param event The metric event to enqueue.
     */
    static void enqueueSQLiteMetric(TrafficNetwork* network, MetricEvent&& event)
    {
        if (network == nullptr)
            return;

        SQLiteWriterState* state = g_sqliteWriterState.load(std::memory_order_acquire);
        if (state == nullptr) {
            return;
        }

        size_t queueDepth = 0U;
        uint64_t droppedCount = 0U;
        bool emitDropWarning = false;

        if (state->stopRequested.load(std::memory_order_acquire)) {
            return;
        }

        if (!state->queue.tryPush(std::move(event))) {
            state->droppedCount++;
            if ((state->droppedCount % 1000U) == 1U) {
                queueDepth = state->queue.pendingCount.load(std::memory_order_acquire);
                droppedCount = state->droppedCount;
                emitDropWarning = true;
            }

            if (emitDropWarning) {
                LogWarning(LOG_MASTER, "SQLite metrics queue full (%zu/%zu), dropped %llu events",
                    queueDepth, state->queue.maxDepth, (unsigned long long)droppedCount);
            }
            return;
        }
    }

    /**
     * @brief Entry point for the SQLite writer thread.
     * @param arg Pointer to the thread object containing the SQLite writer state.
     * @return Always returns nullptr.
     */
    static void* sqliteWriterThreadMain(void* arg)
    {
        thread_t* th = (thread_t*)arg;
        if (th == nullptr) {
            return nullptr;
        }

        SQLiteWriterState* state = (SQLiteWriterState*)th->obj;

        if (state == nullptr || state->db == nullptr)
            return nullptr;

        sqlite3* db = state->db;

        std::vector<MetricEvent> batch;
        batch.reserve(SQLITE_BATCH_SIZE);

        // helper function to run the scheduled prune of old SQLite metric events
        auto runScheduledPrune = [&]() {
            if (state->retentionDays == 0U || state->pruneIntervalMinutes == 0U || state->nextPruneNs == 0U) {
                return;
            }

            uint64_t now = nowNs();
            if (now < state->nextPruneNs) {
                return;
            }

            uint64_t deleted = 0U;
            if (pruneSQLiteMetricEvents(db, state->retentionDays, &deleted)) {
                if (deleted > 0U) {
                    LogInfoEx(LOG_MASTER, "SQLite metrics retention prune deleted %llu stale rows, retention = %u day(s)",
                        (unsigned long long)deleted, state->retentionDays);
                }
            } else {
                LogWarning(LOG_MASTER, "SQLite metrics retention prune failed, retention = %u day(s)", state->retentionDays);
            }

            state->nextPruneNs = calculateNextPruneNs(state->pruneIntervalMinutes);
        };

        std::string threadName("fne:sqlite");
        if (g_killed) {
            return nullptr;
        }

        LogInfoEx(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        while (true) {
            size_t queueDepth = state->queue.pendingCount.load(std::memory_order_acquire);
            if (queueDepth == 0U) {
                if (state->stopRequested.load(std::memory_order_acquire)) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(SQLITE_BATCH_WAIT_MS));
                continue;
            }

            if (queueDepth > (state->queue.maxDepth * 3U / 4U)) {
                LogWarning(LOG_MASTER, "High pressure on SQLite metrics queue depth, queueDepth = %zu, maxDepth = %zu", queueDepth, state->queue.maxDepth);
            }

            MetricEvent event;

            // drain the queue into the batch up to the maximum batch size
            while (state->queue.tryPop(event) && batch.size() < SQLITE_BATCH_SIZE) {
                batch.push_back(std::move(event));
            }

            // check if we should exit the writer thread
            if (state->stopRequested.load(std::memory_order_acquire) && batch.empty() && state->queue.pendingCount.load(std::memory_order_acquire) == 0U) {
                break;
            }

            // if the batch is empty, run the scheduled prune and wait before trying again
            if (batch.empty()) {
                runScheduledPrune();
                std::this_thread::sleep_for(std::chrono::milliseconds(SQLITE_BATCH_WAIT_MS));
                continue;
            }

            // begin a new SQLite transaction
            char* errMsg = nullptr;
            if (sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
                LogError(LOG_MASTER, "SQLite metrics begin transaction failed: %s", (errMsg != nullptr) ? errMsg : "unknown error");
                if (errMsg != nullptr) {
                    sqlite3_free(errMsg);
                }
                batch.clear();
                continue;
            }

            bool ok = true;
            for (const auto& event : batch) {
                if (!insertEvent(state->statements, event)) {
                    ok = false;
                    LogError(LOG_MASTER, "SQLite metrics insert failed: %s", sqlite3_errmsg(db));
                    break;
                }
            }

            if (ok) {
                if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
                    LogError(LOG_MASTER, "SQLite metrics commit failed: %s", (errMsg != nullptr) ? errMsg : "unknown error");
                    if (errMsg != nullptr) {
                        sqlite3_free(errMsg);
                    }
                    sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                }
            } else {
                sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            }

            batch.clear();

            // run the scheduled prune after processing the current batch
            runScheduledPrune();
        }

        LogInfoEx(LOG_HOST, "[STOP] %s", threadName.c_str());

        return nullptr;
    }
}

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes metric sinks for the specified TrafficNetwork instance. */

void TrafficNetwork::MetricsLogging::initialize(TrafficNetwork* network)
{
    if (network == nullptr)
        return;
    if (!network->m_enableMetrics)
        return;

    // is SQLite metrics enabled?
    if (network->m_enableSQLite) {
        if (network->m_sqliteDB == nullptr) {
            if (sqlite3_open(network->m_sqliteDBFile.c_str(), &network->m_sqliteDB) != SQLITE_OK) {
                LogError(LOG_MASTER, "Failed to open SQLite metrics database file: %s", network->m_sqliteDBFile.c_str());
                network->m_enableSQLite = false;
                if (network->m_sqliteDB != nullptr) {
                    sqlite3_close(network->m_sqliteDB);
                    network->m_sqliteDB = nullptr;
                }
                return;
            }
        }

        // ensure SQLite metrics schema is present for both new and existing databases
        initializeSQLite(network);

        if (network->m_sqliteDB != nullptr) {
            // load existing SQLite metrics counters
            loadSQLiteCounters(network);

            // perform an initial prune of old SQLite metric events if retention is configured
            if (network->m_sqlitePruneAfterDays > 0U) {
                uint64_t deleted = 0U;
                if (pruneSQLiteMetricEvents(network->m_sqliteDB, network->m_sqlitePruneAfterDays, &deleted)) {
                    if (deleted > 0U) {
                        LogInfoEx(LOG_MASTER, "SQLite metrics startup prune deleted %llu stale rows (retention=%u day(s))",
                            (unsigned long long)deleted, network->m_sqlitePruneAfterDays);
                    }
                } else {
                    LogWarning(LOG_MASTER, "SQLite metrics startup prune failed (retention=%u day(s))", network->m_sqlitePruneAfterDays);
                }
            }
        }

        // initialize the SQLite writer thread if the database is available
        if (network->m_sqliteDB != nullptr) {
            SQLiteWriterState* existingState = g_sqliteWriterState.load(std::memory_order_acquire);
            if (existingState == nullptr) {
                auto state = std::make_shared<SQLiteWriterState>();
                if (!prepareStatements(network->m_sqliteDB, state->statements)) {
                    LogError(LOG_MASTER, "Failed to prepare SQLite metrics statements: %s", sqlite3_errmsg(network->m_sqliteDB));
                    finalizeStatements(state->statements);
                    sqlite3_close(network->m_sqliteDB);
                    network->m_sqliteDB = nullptr;
                    network->m_enableSQLite = false;
                    return;
                }

                state->db = network->m_sqliteDB;
                state->retentionDays = network->m_sqlitePruneAfterDays;
                state->pruneIntervalMinutes = network->m_sqlitePruneIntervalMinutes;
                state->nextPruneNs = calculateNextPruneNs(state->pruneIntervalMinutes);

                // start the SQLite writer thread
                if (!Thread::runAsThread(state.get(), sqliteWriterThreadMain, &state->workerThread)) {
                    LogError(LOG_MASTER, "Failed to start SQLite metrics writer thread");
                    finalizeStatements(state->statements);
                    sqlite3_close(network->m_sqliteDB);
                    network->m_sqliteDB = nullptr;
                    network->m_enableSQLite = false;
                    return;
                }

                state->workerStarted.store(true, std::memory_order_release);
                SQLiteWriterState* rawState = state.get();
                g_sqliteWriterStateHolder = std::move(state);
                g_sqliteWriterState.store(rawState, std::memory_order_release);
            }
        }
    }
}

/* Finalizes metric sinks for the specified TrafficNetwork instance. */

void TrafficNetwork::MetricsLogging::finalize(TrafficNetwork* network)
{
    if (network == nullptr)
        return;

    SQLiteWriterState* state = g_sqliteWriterState.load(std::memory_order_acquire);
    if (state != nullptr) {
        state->stopRequested.store(true, std::memory_order_release);

        // wait for the SQLite writer thread to finish if it was started
        if (state->workerStarted.load(std::memory_order_acquire)) {
#if defined(_WIN32)
            ::WaitForSingleObject(state->workerThread.thread, INFINITE);
            ::CloseHandle(state->workerThread.thread);
#else
            ::pthread_join(state->workerThread.thread, nullptr);
#endif
            state->workerStarted.store(false, std::memory_order_release);
        }

        // finalize the SQLite statements
        finalizeStatements(state->statements);
    }

    // close the SQLite database if it is open
    if (network->m_sqliteDB != nullptr) {
        sqlite3_close(network->m_sqliteDB);
        network->m_sqliteDB = nullptr;
    }
}

/* Increments the active call counter. */

void TrafficNetwork::MetricsLogging::incrementActiveCalls(TrafficNetwork* network)
{
    (void)network;

    s_totalActiveCalls.fetch_add(1, std::memory_order_relaxed);
}

/* Decrements the active call counter with floor at zero. */

void TrafficNetwork::MetricsLogging::decrementActiveCalls(TrafficNetwork* network)
{
    (void)network;

    int32_t expected = s_totalActiveCalls.load(std::memory_order_relaxed);
    while (expected > 0) {
        if (s_totalActiveCalls.compare_exchange_weak(expected, expected - 1, std::memory_order_relaxed)) {
            return;
        }
    }
}

/* Resets the active call counter to zero. */

void TrafficNetwork::MetricsLogging::resetActiveCalls()
{
    s_totalActiveCalls.store(0, std::memory_order_relaxed);
}

/* Increments the total processed calls counter. */

void TrafficNetwork::MetricsLogging::incrementCallsProcessed(TrafficNetwork* network)
{
    uint64_t value = s_totalCallsProcessed.fetch_add(1U, std::memory_order_relaxed) + 1U;

    persistSQLiteCounter(network, SQLITE_COUNTER_CALLS_PROCESSED, value);
}

/* Increments the total call collisions counter. */

void TrafficNetwork::MetricsLogging::incrementCallCollisions(TrafficNetwork* network)
{
    uint64_t value = s_totalCallCollisions.fetch_add(1U, std::memory_order_relaxed) + 1U;

    persistSQLiteCounter(network, SQLITE_COUNTER_CALL_COLLISIONS, value);
}

/* Increments the total call switches counter. */

void TrafficNetwork::MetricsLogging::incrementCallSwitches(TrafficNetwork* network)
{
    uint64_t value = s_totalCallSwitches.fetch_add(1U, std::memory_order_relaxed) + 1U;

    persistSQLiteCounter(network, SQLITE_COUNTER_CALL_SWITCHES, value);
}

/* Resets the total processed calls counter to zero. */

void TrafficNetwork::MetricsLogging::resetCallsProcessed(TrafficNetwork* network)
{
    s_totalCallsProcessed.store(0U, std::memory_order_relaxed);

    persistSQLiteCounter(network, SQLITE_COUNTER_CALLS_PROCESSED, 0U);
}

/* Resets the total call collisions counter to zero. */

void TrafficNetwork::MetricsLogging::resetCallCollisions(TrafficNetwork* network)
{
    s_totalCallCollisions.store(0U, std::memory_order_relaxed);

    persistSQLiteCounter(network, SQLITE_COUNTER_CALL_COLLISIONS, 0U);
}

/* Resets the total call switches counter to zero. */

void TrafficNetwork::MetricsLogging::resetCallSwitches(TrafficNetwork* network)
{
    s_totalCallSwitches.store(0U, std::memory_order_relaxed);

    persistSQLiteCounter(network, SQLITE_COUNTER_CALL_SWITCHES, 0U);
}

/* Gets the active call counter. */

int32_t TrafficNetwork::MetricsLogging::getTotalActiveCalls()
{
    return s_totalActiveCalls.load(std::memory_order_relaxed);
}

/* Gets the total processed calls counter. */

uint64_t TrafficNetwork::MetricsLogging::getTotalCallsProcessed()
{
    return s_totalCallsProcessed.load(std::memory_order_relaxed);
}

/* Gets the total call collisions counter. */

uint64_t TrafficNetwork::MetricsLogging::getTotalCallCollisions()
{
    return s_totalCallCollisions.load(std::memory_order_relaxed);
}

/* Gets the total call switches counter. */

uint64_t TrafficNetwork::MetricsLogging::getTotalCallSwitches()
{
    return s_totalCallSwitches.load(std::memory_order_relaxed);
}

/* Logs a activity transfer event. */

void TrafficNetwork::MetricsLogging::logActivity(TrafficNetwork* network, uint32_t peerId, const std::string& identity, const std::string& msg)
{
    if (network == nullptr || !network->m_enableMetrics)
        return;

    uint64_t ts = nowNs();

    if (network->m_enableInfluxDB) {
        influxdb::QueryBuilder()
            .meas("activity")
                .tag("peerId", std::to_string(peerId))
                    .field("identity", identity)
                    .field("msg", msg)
                .timestamp(ts)
            .requestAsync(network->m_influxServer);
    }

    if (network->m_enableSQLite && network->m_sqliteDB != nullptr) {
        MetricEvent e;
        e.type = MetricEventType::ACTIVITY;
        e.tsNs = ts;
        e.peerId = peerId;
        e.identity = identity;
        e.message = msg;
        enqueueSQLiteMetric(network, std::move(e));
    }
}

/* Logs a diagnostic transfer event. */

void TrafficNetwork::MetricsLogging::logDiag(TrafficNetwork* network, uint32_t peerId, const std::string& identity, const std::string& msg)
{
    if (network == nullptr || !network->m_enableMetrics)
        return;

    uint64_t ts = nowNs();

    if (network->m_enableInfluxDB) {
        influxdb::QueryBuilder()
            .meas("diag")
                .tag("peerId", std::to_string(peerId))
                    .field("identity", identity)
                    .field("msg", msg)
                .timestamp(ts)
            .requestAsync(network->m_influxServer);
    }

    if (network->m_enableSQLite && network->m_sqliteDB != nullptr) {
        MetricEvent e;
        e.type = MetricEventType::DIAG;
        e.tsNs = ts;
        e.peerId = peerId;
        e.identity = identity;
        e.message = msg;
        enqueueSQLiteMetric(network, std::move(e));
    }
}

/* Logs a call event. */

void TrafficNetwork::MetricsLogging::logCallEvent(TrafficNetwork* network, const char* mode, uint32_t peerId, uint32_t streamId,
    uint32_t srcId, uint32_t dstId, uint64_t durationMs)
{
    if (network == nullptr || !network->m_enableMetrics)
        return;

    uint64_t ts = nowNs();

    if (network->m_enableInfluxDB) {
        influxdb::QueryBuilder()
            .meas("call_event")
                .tag("peerId", std::to_string(peerId))
                .tag("mode", mode)
                .tag("streamId", std::to_string(streamId))
                .tag("srcId", std::to_string(srcId))
                .tag("dstId", std::to_string(dstId))
                    .field("duration", durationMs)
                .timestamp(ts)
            .requestAsync(network->m_influxServer);
    }

    if (network->m_enableSQLite && network->m_sqliteDB != nullptr) {
        MetricEvent e;
        e.type = MetricEventType::CALL_EVENT;
        e.tsNs = ts;
        e.peerId = peerId;
        e.streamId = streamId;
        e.srcId = srcId;
        e.dstId = dstId;
        e.durationMs = durationMs;
        e.mode = (mode != nullptr) ? std::string(mode) : std::string("unknown");
        enqueueSQLiteMetric(network, std::move(e));
    }
}

/* Logs a call event with slot number. */

void TrafficNetwork::MetricsLogging::logCallEvent(TrafficNetwork* network, const char* mode, uint32_t peerId, uint32_t streamId,
    uint32_t srcId, uint32_t dstId, uint64_t durationMs, uint8_t slotNo)
{
    if (network == nullptr || !network->m_enableMetrics)
        return;

    uint64_t ts = nowNs();

    if (network->m_enableInfluxDB) {
        influxdb::QueryBuilder()
            .meas("call_event")
                .tag("peerId", std::to_string(peerId))
                .tag("mode", mode)
                .tag("streamId", std::to_string(streamId))
                .tag("srcId", std::to_string(srcId))
                .tag("dstId", std::to_string(dstId))
                    .field("duration", durationMs)
                    .field("slot", slotNo)
                .timestamp(ts)
            .requestAsync(network->m_influxServer);
    }

    if (network->m_enableSQLite && network->m_sqliteDB != nullptr) {
        MetricEvent e;
        e.type = MetricEventType::CALL_EVENT;
        e.tsNs = ts;
        e.peerId = peerId;
        e.streamId = streamId;
        e.srcId = srcId;
        e.dstId = dstId;
        e.durationMs = durationMs;
        e.mode = (mode != nullptr) ? std::string(mode) : std::string("unknown");
        e.hasSlot = true;
        e.slotNo = slotNo;
        enqueueSQLiteMetric(network, std::move(e));
    }
}

/* Logs a call error event. */

void TrafficNetwork::MetricsLogging::logCallErrorEvent(TrafficNetwork* network, uint32_t peerId, uint32_t streamId, uint32_t srcId,
    uint32_t dstId, const std::string& message)
{
    if (network == nullptr || !network->m_enableMetrics)
        return;

    uint64_t ts = nowNs();

    if (network->m_enableInfluxDB) {
        influxdb::QueryBuilder()
            .meas("call_error_event")
                .tag("peerId", std::to_string(peerId))
                .tag("streamId", std::to_string(streamId))
                .tag("srcId", std::to_string(srcId))
                .tag("dstId", std::to_string(dstId))
                    .field("message", message)
                .timestamp(ts)
            .requestAsync(network->m_influxServer);
    }

    if (network->m_enableSQLite && network->m_sqliteDB != nullptr) {
        MetricEvent e;
        e.type = MetricEventType::CALL_ERROR_EVENT;
        e.tsNs = ts;
        e.peerId = peerId;
        e.streamId = streamId;
        e.srcId = srcId;
        e.dstId = dstId;
        e.message = message;
        enqueueSQLiteMetric(network, std::move(e));
    }
}

/* Logs a call error event with slot number. */

void TrafficNetwork::MetricsLogging::logCallErrorEvent(TrafficNetwork* network, uint32_t peerId, uint32_t streamId, uint32_t srcId,
    uint32_t dstId, const std::string& message, uint8_t slotNo)
{
    if (network == nullptr || !network->m_enableMetrics)
        return;

    uint64_t ts = nowNs();

    if (network->m_enableInfluxDB) {
        influxdb::QueryBuilder()
            .meas("call_error_event")
                .tag("peerId", std::to_string(peerId))
                .tag("streamId", std::to_string(streamId))
                .tag("srcId", std::to_string(srcId))
                .tag("dstId", std::to_string(dstId))
                    .field("message", message)
                    .field("slot", slotNo)
                .timestamp(ts)
            .requestAsync(network->m_influxServer);
    }

    if (network->m_enableSQLite && network->m_sqliteDB != nullptr) {
        MetricEvent e;
        e.type = MetricEventType::CALL_ERROR_EVENT;
        e.tsNs = ts;
        e.peerId = peerId;
        e.streamId = streamId;
        e.srcId = srcId;
        e.dstId = dstId;
        e.message = message;
        e.hasSlot = true;
        e.slotNo = slotNo;
        enqueueSQLiteMetric(network, std::move(e));
    }
}

/* Logs a call collision event with slot number. */

void TrafficNetwork::MetricsLogging::logCallCollisionEvent(TrafficNetwork* network, uint32_t peerId, uint32_t streamId, uint32_t srcId,
    uint32_t dstId, uint8_t slotNo, uint32_t rxPeerId, uint32_t rxStreamId, uint32_t rxSrcId, uint32_t rxDstId, uint8_t rxSlot)
{
    if (network == nullptr || !network->m_enableMetrics)
        return;

    uint64_t ts = nowNs();

    if (network->m_enableInfluxDB) {
        influxdb::QueryBuilder()
            .meas("call_collision_event")
                .tag("peerId", std::to_string(peerId))
                .tag("streamId", std::to_string(streamId))
                .tag("srcId", std::to_string(srcId))
                .tag("dstId", std::to_string(dstId))
                .tag("rxPeerId", std::to_string(rxPeerId))
                .tag("rxStreamId", std::to_string(rxStreamId))
                .tag("rxSrcId", std::to_string(rxSrcId))
                .tag("rxDstId", std::to_string(rxDstId))
                    .field("slot", slotNo)
                    .field("rxSlot", rxSlot)
                .timestamp(ts)
            .requestAsync(network->m_influxServer);
    }

    if (network->m_enableSQLite && network->m_sqliteDB != nullptr) {
        MetricEvent e;
        e.type = MetricEventType::CALL_COLLISION_EVENT;
        e.tsNs = ts;
        e.peerId = peerId;
        e.streamId = streamId;
        e.srcId = srcId;
        e.dstId = dstId;
        e.rxPeerId = rxPeerId;
        e.rxStreamId = rxStreamId;
        e.rxSrcId = rxSrcId;
        e.rxDstId = rxDstId;
        if (slotNo != 0U) {
            e.hasSlot = true;
            e.slotNo = slotNo;
        }
        if (rxSlot != 0U) {
            e.hasRxSlot = true;
            e.rxSlot = rxSlot;
        }
        enqueueSQLiteMetric(network, std::move(e));
    }
}

/* Logs a P25 TSBK raw event. */

void TrafficNetwork::MetricsLogging::logTSBKEvent(TrafficNetwork* network, uint32_t peerId, const std::string& lco, const std::string& tsbk,
    const std::string& raw)
{
    if (network == nullptr || !network->m_enableMetrics)
        return;

    uint64_t ts = nowNs();

    if (network->m_enableInfluxDB && network->m_metricsLogRawData) {
        influxdb::QueryBuilder()
            .meas("tsbk_event")
                .tag("peerId", std::to_string(peerId))
                .tag("lco", lco)
                .tag("tsbk", tsbk)
                    .field("raw", raw)
                .timestamp(ts)
            .requestAsync(network->m_influxServer);
    }

    if (network->m_enableSQLite && network->m_sqliteDB != nullptr && network->m_metricsLogRawData) {
        MetricEvent e;
        e.type = MetricEventType::TSBK_EVENT;
        e.tsNs = ts;
        e.peerId = peerId;
        e.lco = lco;
        e.rawDescription = tsbk;
        e.raw = raw;
        enqueueSQLiteMetric(network, std::move(e));
    }
}

/* Logs a DMR CSBK raw event. */

void TrafficNetwork::MetricsLogging::logCSBKEvent(TrafficNetwork* network, uint32_t peerId, const std::string& lco, const std::string& csbk,
    const std::string& raw)
{
    if (network == nullptr || !network->m_enableMetrics)
        return;

    uint64_t ts = nowNs();

    if (network->m_enableInfluxDB && network->m_metricsLogRawData) {
        influxdb::QueryBuilder()
            .meas("csbk_event")
                .tag("peerId", std::to_string(peerId))
                .tag("lco", lco)
                .tag("csbk", csbk)
                    .field("raw", raw)
                .timestamp(ts)
            .requestAsync(network->m_influxServer);
    }

    if (network->m_enableSQLite && network->m_sqliteDB != nullptr && network->m_metricsLogRawData) {
        MetricEvent e;
        e.type = MetricEventType::CSBK_EVENT;
        e.tsNs = ts;
        e.peerId = peerId;
        e.lco = lco;
        e.rawDescription = csbk;
        e.raw = raw;
        enqueueSQLiteMetric(network, std::move(e));
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Checks if the SQLite database for the specified TrafficNetwork instance is blank. */

bool TrafficNetwork::MetricsLogging::isSQLiteBlank(TrafficNetwork* network)
{
    if (!network->m_enableMetrics)
        return false;
    if (!network->m_enableSQLite)
        return false;

    std::string sql = "SELECT count(*) FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%';";

    sqlite3_stmt* stmt = nullptr;
    bool blank = true;

    if (sqlite3_prepare_v2(network->m_sqliteDB, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int cnt = sqlite3_column_int(stmt, 0);
            if (cnt > 0) {
                blank = false;
            }
        }
    }
    sqlite3_finalize(stmt);
    return blank;
}

/* Initializes the SQLite database for the specified TrafficNetwork instance. */

void TrafficNetwork::MetricsLogging::initializeSQLite(TrafficNetwork* network)
{
    if (!network->m_enableMetrics)
        return;
    if (!network->m_enableSQLite)
        return;
    if (network->m_sqliteDB == nullptr)
        return;

    if (sqlite3_exec(network->m_sqliteDB, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        LogError(LOG_MASTER, "Error setting SQLite journal_mode, error: %s", sqlite3_errmsg(network->m_sqliteDB));
        sqlite3_close(network->m_sqliteDB);
        network->m_sqliteDB = nullptr;
        network->m_enableSQLite = false;
        return;
    }

    if (sqlite3_exec(network->m_sqliteDB, "PRAGMA synchronous = NORMAL;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        LogError(LOG_MASTER, "Error setting SQLite synchronous mode, error: %s", sqlite3_errmsg(network->m_sqliteDB));
        sqlite3_close(network->m_sqliteDB);
        network->m_sqliteDB = nullptr;
        network->m_enableSQLite = false;
        return;
    }

    if (sqlite3_exec(network->m_sqliteDB, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        LogError(LOG_MASTER, "Error enabling SQLite foreign_keys, error: %s", sqlite3_errmsg(network->m_sqliteDB));
        sqlite3_close(network->m_sqliteDB);
        network->m_sqliteDB = nullptr;
        network->m_enableSQLite = false;
        return;
    }

    if (!ensureSQLiteMetricTables(network)) {
        sqlite3_close(network->m_sqliteDB);
        network->m_sqliteDB = nullptr;
        network->m_enableSQLite = false;
    }
}

/* Ensures SQLite tables required for persisted metrics counters exist. */

bool TrafficNetwork::MetricsLogging::ensureSQLiteCounterTable(TrafficNetwork* network)
{
    if (network == nullptr || network->m_sqliteDB == nullptr) {
        return false;
    }

    std::string sql = "CREATE TABLE IF NOT EXISTS metrics_counters (key TEXT PRIMARY KEY, value INTEGER NOT NULL);";

    if (sqlite3_exec(network->m_sqliteDB, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
        LogError(LOG_MASTER, "Error creating SQLite metrics_counters table, error: %s", sqlite3_errmsg(network->m_sqliteDB));
        return false;
    }

    return true;
}

/* Ensures all SQLite metric tables and indexes exist. */

bool TrafficNetwork::MetricsLogging::ensureSQLiteMetricTables(TrafficNetwork* network)
{
        if (network == nullptr || network->m_sqliteDB == nullptr) {
                return false;
        }

        // SQLite schema statements for creating metric tables and indexes
        const char* schemaStatements[] = {
            // Activity table and indexes
R"(CREATE TABLE IF NOT EXISTS activity (
    id INTEGER PRIMARY KEY,
    ts_ns INTEGER NOT NULL,
    ts_s REAL GENERATED ALWAYS AS (ts_ns / 1000000000.0) STORED,
    peer_id INTEGER NOT NULL,
    identity TEXT NOT NULL,
    msg TEXT NOT NULL
);)",
            "CREATE INDEX IF NOT EXISTS idx_activity_time ON activity(ts_ns);",
            "CREATE INDEX IF NOT EXISTS idx_activity_peer_time ON activity(peer_id, ts_ns);",

            // Diagnostic table and indexes
R"(CREATE TABLE IF NOT EXISTS diag (
    id INTEGER PRIMARY KEY,
    ts_ns INTEGER NOT NULL,
    ts_s REAL GENERATED ALWAYS AS (ts_ns / 1000000000.0) STORED,
    peer_id INTEGER NOT NULL,
    identity TEXT NOT NULL,
    msg TEXT NOT NULL
);)",
            "CREATE INDEX IF NOT EXISTS idx_diag_time ON diag(ts_ns);",
            "CREATE INDEX IF NOT EXISTS idx_diag_peer_time ON diag(peer_id, ts_ns);",

            // Call event table and indexes
R"(CREATE TABLE IF NOT EXISTS call_event (
    id INTEGER PRIMARY KEY,
    ts_ns INTEGER NOT NULL,
    ts_s REAL GENERATED ALWAYS AS (ts_ns / 1000000000.0) STORED,
    peer_id INTEGER NOT NULL,
    mode TEXT NOT NULL CHECK (mode IN ('Analog','DMR','NXDN','P25')),
    stream_id INTEGER NOT NULL,
    src_id INTEGER NOT NULL,
    dst_id INTEGER NOT NULL,
    duration_ms INTEGER NOT NULL,
    slot INTEGER NULL
);)",
            "CREATE INDEX IF NOT EXISTS idx_call_event_time ON call_event(ts_ns);",
            "CREATE INDEX IF NOT EXISTS idx_call_event_mode_time ON call_event(mode, ts_ns);",
            "CREATE INDEX IF NOT EXISTS idx_call_event_peer_time ON call_event(peer_id, ts_ns);",
            "CREATE INDEX IF NOT EXISTS idx_call_event_dst_time ON call_event(dst_id, ts_ns);",
            "CREATE INDEX IF NOT EXISTS idx_call_event_stream ON call_event(stream_id);",

            // Call error event table and indexes
R"(CREATE TABLE IF NOT EXISTS call_error_event (
    id INTEGER PRIMARY KEY,
    ts_ns INTEGER NOT NULL,
    ts_s REAL GENERATED ALWAYS AS (ts_ns / 1000000000.0) STORED,
    peer_id INTEGER NOT NULL,
    stream_id INTEGER NOT NULL,
    src_id INTEGER NOT NULL,
    dst_id INTEGER NOT NULL,
    message TEXT NOT NULL,
    slot INTEGER NULL
);)",
            "CREATE INDEX IF NOT EXISTS idx_call_error_time ON call_error_event(ts_ns);",
            "CREATE INDEX IF NOT EXISTS idx_call_error_peer_time ON call_error_event(peer_id, ts_ns);",
            "CREATE INDEX IF NOT EXISTS idx_call_error_message_time ON call_error_event(message, ts_ns);",
            "CREATE INDEX IF NOT EXISTS idx_call_error_stream ON call_error_event(stream_id);",

            // Call collision event table and indexes
R"(CREATE TABLE IF NOT EXISTS call_collision_event (
    id INTEGER PRIMARY KEY,
    ts_ns INTEGER NOT NULL,
    ts_s REAL GENERATED ALWAYS AS (ts_ns / 1000000000.0) STORED,
    peer_id INTEGER NOT NULL,
    stream_id INTEGER NOT NULL,
    src_id INTEGER NOT NULL,
    dst_id INTEGER NOT NULL,
    slot INTEGER NULL,
    rx_peer_id INTEGER NOT NULL,
    rx_stream_id INTEGER NOT NULL,
    rx_src_id INTEGER NOT NULL,
    rx_dst_id INTEGER NOT NULL,
    rx_slot INTEGER NULL
);)",
            "CREATE INDEX IF NOT EXISTS idx_call_collision_time ON call_collision_event(ts_ns);",
            "CREATE INDEX IF NOT EXISTS idx_call_collision_peer_time ON call_collision_event(peer_id, ts_ns);",
            "CREATE INDEX IF NOT EXISTS idx_call_collision_stream ON call_collision_event(stream_id);",
            "CREATE INDEX IF NOT EXISTS idx_call_collision_rx_peer_time ON call_collision_event(rx_peer_id, ts_ns);",
            "CREATE INDEX IF NOT EXISTS idx_call_collision_rx_stream ON call_collision_event(rx_stream_id);",

            // TSBK event table and indexes
R"(CREATE TABLE IF NOT EXISTS tsbk_event (
    id INTEGER PRIMARY KEY,
    ts_ns INTEGER NOT NULL,
    ts_s REAL GENERATED ALWAYS AS (ts_ns / 1000000000.0) STORED,
    peer_id INTEGER NOT NULL,
    lco TEXT NOT NULL,
    tsbk TEXT NOT NULL,
    raw TEXT NOT NULL
);)",
            "CREATE INDEX IF NOT EXISTS idx_tsbk_time ON tsbk_event(ts_ns);",
            "CREATE INDEX IF NOT EXISTS idx_tsbk_peer_time ON tsbk_event(peer_id, ts_ns);",
            "CREATE INDEX IF NOT EXISTS idx_tsbk_lco_time ON tsbk_event(lco, ts_ns);",

            // CSBK event table and indexes
R"(CREATE TABLE IF NOT EXISTS csbk_event (
    id INTEGER PRIMARY KEY,
    ts_ns INTEGER NOT NULL,
    ts_s REAL GENERATED ALWAYS AS (ts_ns / 1000000000.0) STORED,
    peer_id INTEGER NOT NULL,
    lco TEXT NOT NULL,
    csbk TEXT NOT NULL,
    raw TEXT NOT NULL
);)",
            "CREATE INDEX IF NOT EXISTS idx_csbk_time ON csbk_event(ts_ns);",
            "CREATE INDEX IF NOT EXISTS idx_csbk_peer_time ON csbk_event(peer_id, ts_ns);",
            "CREATE INDEX IF NOT EXISTS idx_csbk_lco_time ON csbk_event(lco, ts_ns);"
        };

        // execute each schema statement to ensure the SQLite database has the necessary tables and indexes
        for (const char* statement : schemaStatements) {
            if (sqlite3_exec(network->m_sqliteDB, statement, nullptr, nullptr, nullptr) != SQLITE_OK) {
                LogError(LOG_MASTER, "Error ensuring SQLite metrics schema, error: %s", sqlite3_errmsg(network->m_sqliteDB));
                return false;
            }
        }

        return ensureSQLiteCounterTable(network);
}

/* Loads persisted metrics counters from SQLite. */

void TrafficNetwork::MetricsLogging::loadSQLiteCounters(TrafficNetwork* network)
{
    if (network == nullptr || network->m_sqliteDB == nullptr) {
        return;
    }

    sqlite3_stmt* stmt = nullptr;
    std::string sql = "SELECT key, value FROM metrics_counters WHERE key IN (?, ?, ?);";
    if (sqlite3_prepare_v2(network->m_sqliteDB, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        LogWarning(LOG_MASTER, "Unable to prepare SQLite counter load statement: %s", sqlite3_errmsg(network->m_sqliteDB));
        return;
    }

    sqlite3_bind_text(stmt, 1, SQLITE_COUNTER_CALLS_PROCESSED, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, SQLITE_COUNTER_CALL_COLLISIONS, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, SQLITE_COUNTER_CALL_SWITCHES, -1, SQLITE_STATIC);

    uint64_t loadedProcessed = 0U;
    uint64_t loadedCollisions = 0U;
    uint64_t loadedSwitches = 0U;
    bool hasProcessed = false;
    bool hasCollisions = false;
    bool hasSwitches = false;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* key = (const char*)sqlite3_column_text(stmt, 0);
        uint64_t value = (uint64_t)sqlite3_column_int64(stmt, 1);
        if (key == nullptr) {
            continue;
        }

        if (::strcmp(key, SQLITE_COUNTER_CALLS_PROCESSED) == 0) {
            loadedProcessed = value;
            hasProcessed = true;
        } else if (::strcmp(key, SQLITE_COUNTER_CALL_COLLISIONS) == 0) {
            loadedCollisions = value;
            hasCollisions = true;
        } else if (::strcmp(key, SQLITE_COUNTER_CALL_SWITCHES) == 0) {
            loadedSwitches = value;
            hasSwitches = true;
        }
    }

    sqlite3_finalize(stmt);

    if (hasProcessed) {
        s_totalCallsProcessed.store(loadedProcessed, std::memory_order_relaxed);
    }
    if (hasCollisions) {
        s_totalCallCollisions.store(loadedCollisions, std::memory_order_relaxed);
    }
    if (hasSwitches) {
        s_totalCallSwitches.store(loadedSwitches, std::memory_order_relaxed);
    }
}

/* Persists a single metrics counter to SQLite. */

void TrafficNetwork::MetricsLogging::persistSQLiteCounter(TrafficNetwork* network, const char* key, uint64_t value)
{
    if (network == nullptr || key == nullptr) {
        return;
    }
    if (!network->m_enableSQLite || network->m_sqliteDB == nullptr) {
        return;
    }

    MetricEvent e;
    e.type = MetricEventType::COUNTER_EVENT;
    e.counterKey = key;
    e.counterValue = value;
    enqueueSQLiteMetric(network, std::move(e));
}
