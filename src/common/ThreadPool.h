// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file ThreadPool.h
 * @ingroup threading
 * @file ThreadPool.cpp
 * @ingroup threading
 */
#if !defined(__THREAD_POOL_H__)
#define __THREAD_POOL_H__

#include "common/Defines.h"
#include "common/Thread.h"

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Represents a task run by a thread pool worker thread.
 * @ingroup threading
 */
class HOST_SW_API ThreadPoolCallback {
public:
    /**
     * @brief Initializes a new instance of the ThreadPoolCallback class.
     * @tparam F 
     * @tparam Args 
     * @param f 
     * @param args 
     */
    template<class F, class... Args>
    ThreadPoolCallback(F&& f, Args&&... args) 
    {
        task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    }
    virtual ~ThreadPoolCallback() = default;

    /**
     * @brief Starts the task function.
     */
    void run() { task(); }

private:
    std::function<void()> task;
};

/**
 * @brief 
 * @tparam F 
 * @tparam Args 
 * @param f 
 * @param args 
 * @return ThreadPoolCallback* 
 */
template<class F, class... Args>
ThreadPoolCallback* new_pooltask(F&& f, Args&&... args) { return new ThreadPoolCallback(f, args...); }

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Creates and controls a thread pool.
 * @ingroup threading
 */
class HOST_SW_API ThreadPool {
public:
    /**
     * @brief Initializes a new instance of the Thread class.
     * @param workerCnt Number of worker threads to create.
     * @param poolName Name of the thread pool.
     */
    ThreadPool(uint16_t workerCnt = 4U, std::string poolName = "pool");
    /**
     * @brief Finalizes a instance of the Thread class.
     */
    virtual ~ThreadPool();

    /**
     * @brief Enqueues a thread pool task.
     * @param task Task to enqueue.
     * @returns bool True, if task enqueued otherwise false.
     */
    bool enqueue(ThreadPoolCallback* task);

    /**
     * @brief Starts the thread pool.
     */
    void start();
    /**
     * @brief Stops the thread pool.
     */
    void stop();

    /**
     * @brief Make calling thread wait for termination of any remaining thread pool tasks.
     */
    void wait();

public:
    /**
     * @brief Maximum number of worker threads.
     */
    __PROPERTY(uint16_t, maxWorkerCnt, MaxWorkerCnt);
    /**
     * @brief Maximum number of queued tasks.
     */
    __PROPERTY(uint16_t, maxQueuedTasks, MaxQueuedTasks);

private:

    /**
     * @brief Thread pool state.
     */
    enum PoolState {
        STOP = 0,
        IDLE,
        RUNNING
    };

    PoolState m_poolState;

    std::vector<pthread_t> m_workers;
    std::queue<std::unique_ptr<ThreadPoolCallback>> m_tasks;

    std::mutex m_workerMutex;
    std::mutex m_queueMutex;
    std::condition_variable m_cond;

    std::string m_name;

    /**
     * @brief Internal worker thats used as the entry point for the worker threads.
     * @param arg 
     * @returns void* 
     */
#if defined(_WIN32)
    static DWORD __stdcall worker(LPVOID arg);
#else
    static void* worker(void* arg);
#endif // defined(_WIN32)
};

#endif // __THREAD_POOL_H__
