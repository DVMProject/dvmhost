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
#include "common/Thread.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Test thread implementation for unit tests.
 */
class TestThread final : public Thread {
public:
    /**
     * @brief Constructs a TestThread instance.
     * @param entryCount The atomic counter to track thread entry.
     * @param mutex The mutex to protect shared state.
     * @param cond The condition variable to signal thread completion.
     * @param ran The flag indicating whether the thread has run.
     */
    TestThread(std::atomic<uint32_t>& entryCount, std::mutex& mutex,
        std::condition_variable& cond, bool& ran) :
        m_entryCount(entryCount),
        m_mutex(mutex),
        m_cond(cond),
        m_ran(ran)
    {
        /* stub */
    }

    /**
     * @brief Thread entry point.
     */
    void entry() override
    {
        m_entryCount.fetch_add(1U);

        // scope is intentional
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_ran = true;
        }

        m_cond.notify_one();
    }

private:
    std::atomic<uint32_t>& m_entryCount;
    std::mutex& m_mutex;
    std::condition_variable& m_cond;
    bool& m_ran;
};

// ---------------------------------------------------------------------------
//  Structure Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Context structure for running a thread routine.
 */
struct RunAsThreadContext {
    std::atomic<uint32_t> callCount { 0U };
    std::mutex mutex;
    std::condition_variable cond;
    bool ran = false;
};

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/**
 * @brief Thread start routine that executes the thread's entry function.
 * @param arg Pointer to the thread_t structure.
 * @return void* Always returns nullptr.
 */
void* runAsThreadRoutine(void* arg)
{
    thread_t* thread = static_cast<thread_t*>(arg);
    if (thread == nullptr || thread->obj == nullptr)
        return nullptr;

    RunAsThreadContext* ctx = static_cast<RunAsThreadContext*>(thread->obj);
    ctx->callCount.fetch_add(1U);

    // scope is intentional
    {
        std::lock_guard<std::mutex> lock(ctx->mutex);
        ctx->ran = true;
    }

    ctx->cond.notify_one();
    return nullptr;
}

/**
 * @brief Waits for a flag to become true with a timeout.
 * @param mutex The mutex protecting the flag.
 * @param cond The condition variable to wait on.
 * @param flag The flag to wait for.
 * @param timeoutMs The timeout in milliseconds.
 * @return bool True if the flag became true within the timeout, false otherwise.
 */
bool waitForFlag(std::mutex& mutex, std::condition_variable& cond, bool& flag, uint32_t timeoutMs = 1000U)
{
    std::unique_lock<std::mutex> lock(mutex);
    return cond.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&flag]() { return flag; });
}

TEST_CASE("Thread runs entry once and wait joins", "[common][thread]")
{
    std::atomic<uint32_t> entryCount { 0U };
    std::mutex mutex;
    std::condition_variable cond;
    bool ran = false;

    TestThread thread(entryCount, mutex, cond, ran);

    REQUIRE_FALSE(thread.started());
    REQUIRE(thread.run());
    REQUIRE(thread.started());

    REQUIRE(waitForFlag(mutex, cond, ran));

    // A second run() call should not spawn another worker.
    REQUIRE(thread.run());
    thread.wait();

    REQUIRE(entryCount.load() == 1U);
}

TEST_CASE("Thread wait and setName are safe before run", "[common][thread]")
{
    std::atomic<uint32_t> entryCount { 0U };
    std::mutex mutex;
    std::condition_variable cond;
    bool ran = false;

    TestThread thread(entryCount, mutex, cond, ran);

    thread.setName("not-running");
    thread.wait();
    thread.detach();

    REQUIRE(entryCount.load() == 0U);
    REQUIRE_FALSE(thread.started());
}

TEST_CASE("Thread detach allows completion without wait", "[common][thread]")
{
    std::atomic<uint32_t> entryCount { 0U };
    std::mutex mutex;
    std::condition_variable cond;
    bool ran = false;

    TestThread thread(entryCount, mutex, cond, ran);

    REQUIRE(thread.run());
    thread.detach();

    REQUIRE(waitForFlag(mutex, cond, ran));
    REQUIRE(entryCount.load() == 1U);
}

TEST_CASE("Thread runAsThread executes start routine", "[common][thread]")
{
    RunAsThreadContext ctx;
    thread_t threadData;

    REQUIRE(Thread::runAsThread(&ctx, runAsThreadRoutine, &threadData));
    REQUIRE(waitForFlag(ctx.mutex, ctx.cond, ctx.ran));

#if defined(_WIN32)
    ::WaitForSingleObject(threadData.thread, INFINITE);
    ::CloseHandle(threadData.thread);
#else
    ::pthread_join(threadData.thread, nullptr);
#endif // defined(_WIN32)

    REQUIRE(ctx.callCount.load() == 1U);
}

TEST_CASE("Thread sleep delays execution", "[common][thread]")
{
    const auto begin = std::chrono::steady_clock::now();
    Thread::sleep(10U);
    const auto end = std::chrono::steady_clock::now();

    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    REQUIRE(elapsedMs >= 5);
}