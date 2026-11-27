// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/ThreadPool.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/**
 * @brief Waits until the atomic value reaches the target or the timeout expires.
 * @param value The atomic value to monitor.
 * @param target The target value to wait for.
 * @param timeoutMs The maximum time to wait in milliseconds.
 * @return true if the target value was reached, false if the timeout expired.
 */
bool waitForCount(std::atomic<uint32_t>& value, uint32_t target, uint32_t timeoutMs = 1500U)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (value.load() >= target)
            return true;

        Thread::sleep(1U);
    }

    return value.load() >= target;
}

TEST_CASE("ThreadPool enforces minimum worker count", "[common][threadpool]")
{
    ThreadPool pool(1U, "tp-min");
    REQUIRE(pool.getMaxWorkerCnt() == 4U);
}

TEST_CASE("ThreadPool runs enqueued tasks", "[common][threadpool]")
{
    ThreadPool pool(4U, "tp-run");
    std::atomic<uint32_t> executed { 0U };

    pool.start();

    const uint32_t taskCount = 12U;
    bool allAccepted = true;
    for (uint32_t i = 0U; i < taskCount; i++) {
        ThreadPoolTask* task = new_pooltask([&executed]() {
            executed.fetch_add(1U);
        });

        if (!pool.enqueue(task)) {
            delete task;
            allAccepted = false;
            break;
        }
    }
    REQUIRE(allAccepted);
    REQUIRE(waitForCount(executed, taskCount));

    pool.stop();
    pool.wait();

    REQUIRE(executed.load() == taskCount);
}

TEST_CASE("ThreadPool rejects enqueue when stopped", "[common][threadpool]")
{
    ThreadPool pool(4U, "tp-stop");

    ThreadPoolTask* beforeStart = new_pooltask([]() {});
    const bool beforeStartAccepted = pool.enqueue(beforeStart);
    REQUIRE_FALSE(beforeStartAccepted);
    if (!beforeStartAccepted)
        delete beforeStart;

    pool.start();
    pool.stop();

    ThreadPoolTask* afterStop = new_pooltask([]() {});
    const bool afterStopAccepted = pool.enqueue(afterStop);
    REQUIRE_FALSE(afterStopAccepted);
    if (!afterStopAccepted)
        delete afterStop;

    pool.wait();
}

TEST_CASE("ThreadPool max queued task limit is enforced", "[common][threadpool]")
{
    ThreadPool pool(4U, "tp-queue");
    pool.setMaxQueuedTasks(0U);

    std::atomic<uint32_t> executed { 0U };
    std::mutex gateMutex;
    std::condition_variable gateCond;
    bool releaseWorkers = false;

    pool.start();

    // Block all workers so one extra task remains queued.
    bool workerTasksAccepted = true;
    for (uint32_t i = 0U; i < pool.getMaxWorkerCnt(); i++) {
        ThreadPoolTask* task = new_pooltask([&]() {
            std::unique_lock<std::mutex> lock(gateMutex);
            gateCond.wait(lock, [&]() { return releaseWorkers; });
            executed.fetch_add(1U);
        });

        if (!pool.enqueue(task)) {
            delete task;
            workerTasksAccepted = false;
            break;
        }
    }

    if (workerTasksAccepted)
        Thread::sleep(10U);

    // Now enforce a one-item queue while all workers are occupied.
    pool.setMaxQueuedTasks(1U);

    ThreadPoolTask* acceptedQueuedTask = new_pooltask([&executed]() {
        executed.fetch_add(1U);
    });
    bool queuedAccepted = false;
    if (workerTasksAccepted)
        queuedAccepted = pool.enqueue(acceptedQueuedTask);
    if (!queuedAccepted)
        delete acceptedQueuedTask;

    ThreadPoolTask* overflowTask = new_pooltask([]() {});
    bool overflowAccepted = false;
    if (workerTasksAccepted && queuedAccepted)
        overflowAccepted = pool.enqueue(overflowTask);
    if (!overflowAccepted)
        delete overflowTask;

    // scope is intentional
    {
        std::lock_guard<std::mutex> lock(gateMutex);
        releaseWorkers = true;
    }
    gateCond.notify_all();

    REQUIRE(workerTasksAccepted);
    REQUIRE(queuedAccepted);
    REQUIRE_FALSE(overflowAccepted);
    REQUIRE(waitForCount(executed, pool.getMaxWorkerCnt()));

    pool.stop();
    pool.wait();
}