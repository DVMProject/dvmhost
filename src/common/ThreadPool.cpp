// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "ThreadPool.h"
#include "Log.h"

#include <cerrno>
#include <signal.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif // !defined(_WIN32)

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define MIN_WORKER_CNT 4U

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the ThreadPool class. */

ThreadPool::ThreadPool(uint16_t workerCnt, std::string name) :
    m_maxWorkerCnt(workerCnt),
    m_maxQueuedTasks(0U),
    m_poolState(STOP),
    m_workers(),
    m_tasks(),
    m_workerMutex(),
    m_queueMutex(),
    m_cond(),
    m_name(name)
{
    if (m_maxWorkerCnt < MIN_WORKER_CNT)
        m_maxWorkerCnt = MIN_WORKER_CNT;
}

/* Finalizes a instance of the ThreadPool class. */

ThreadPool::~ThreadPool() = default;

/* Enqueue a thread pool task. */

bool ThreadPool::enqueue(ThreadPoolTask* task)
{
    // scope is intentional
    {
        std::unique_lock<std::mutex> lock(m_workerMutex);
        if (m_poolState == RUNNING && m_workers.size() < m_maxWorkerCnt) {
            thread_t* thread = new thread_t();
            thread->obj = this;
        
#if defined(_WIN32)
            HANDLE hnd = ::CreateThread(NULL, 0, worker, thread, CREATE_SUSPENDED, NULL);
            if (hnd == NULL) {
                LogError(LOG_HOST, "Error returned from CreateThread, err: %lu", ::GetLastError());
                return false;
            }

            thread->thread = hnd;
            ::ResumeThread(hnd);
#else
            if (::pthread_create(&thread->thread, NULL, worker, thread) != 0) {
                LogError(LOG_HOST, "Error returned from pthread_create, err: %d", errno);
                return false;
            }
#endif // defined(_WIN32)

            m_workers.emplace_back(thread->thread);
        }
    }

    // scope is intentional
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        if (m_poolState == STOP) {
            LogError(LOG_HOST, "Cannot enqueue task on a stopped thread pool!");
            return false;
        }

        if (m_maxQueuedTasks > 0U && m_tasks.size() >= m_maxQueuedTasks) {
            LogError(LOG_HOST, "Cannot enqueue task, thread pool queue is full!");
            return false;
        }

        m_tasks.emplace(std::unique_ptr<ThreadPoolTask>(task));
    }

    m_cond.notify_one();
    return true;
}

/* Starts the thread pool. */

void ThreadPool::start()
{
    // scope is intentional
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_poolState = RUNNING;

        for (uint32_t i = m_workers.size(); i < m_maxWorkerCnt; i++) {
            thread_t* thread = new thread_t();
            thread->obj = this;
        
#if defined(_WIN32)
            HANDLE hnd = ::CreateThread(NULL, 0, worker, thread, CREATE_SUSPENDED, NULL);
            if (hnd == NULL) {
                LogError(LOG_HOST, "Error returned from CreateThread, err: %lu", ::GetLastError());
                continue;
            }
        
            thread->thread = hnd;
            ::ResumeThread(hnd);
#else
            if (::pthread_create(&thread->thread, NULL, worker, thread) != 0) {
                LogError(LOG_HOST, "Error returned from pthread_create, err: %d", errno);
                continue;
            }
#endif // defined(_WIN32)

            m_workers.emplace_back(thread->thread);
        }
      }

      m_cond.notify_all();
}

/* Stops the thread pool. */

void ThreadPool::stop()
{
    // scope is intentional
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_poolState = STOP;
    }

    m_cond.notify_all();
}

/* Make calling thread wait for termination of any remaining thread pool tasks. */

void ThreadPool::wait()
{
    m_cond.notify_all();

    for (pthread_t worker : m_workers) {
#if defined(_WIN32)
        ::WaitForSingleObject(worker, INFINITE);
        ::CloseHandle(worker);
#else
        ::pthread_join(worker, NULL);
#endif // defined(_WIN32)
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal worker thread that is used to execute task functions. */

#if defined(_WIN32)
DWORD ThreadPool::worker(LPVOID arg)
#else
void* ThreadPool::worker(void* arg)
#endif // defined(_WIN32)
{
    thread_t* thread = (thread_t*)arg;
    if (thread == nullptr) {
        LogError(LOG_HOST, "Fatal error starting thread pool worker! No thread!");
#if defined(_WIN32)
        return 0UL;
#else
        return nullptr;
#endif // defined(_WIN32)
    }

    ThreadPool* threadPool = (ThreadPool*)thread->obj;
    if (threadPool == nullptr) {
        LogError(LOG_HOST, "Fatal error starting thread pool worker! No thread pool owner!");
        delete thread;
#if defined(_WIN32)
        return 0UL;
#else
        return nullptr;
#endif // defined(_WIN32)
    }

    std::stringstream threadName;
    threadName << threadPool->m_name << ":worker";
#ifdef _GNU_SOURCE
    ::pthread_setname_np(thread->thread, threadName.str().c_str());
#endif // _GNU_SOURCE

    ThreadPoolTask* task = nullptr;
    while (threadPool->m_poolState != STOP) {
        // scope is intentional
        {
            std::unique_lock<std::mutex> lock(threadPool->m_queueMutex);
            threadPool->m_cond.wait(lock, [=] { return threadPool->m_poolState == STOP || !threadPool->m_tasks.empty(); });
            if (threadPool->m_poolState == STOP && threadPool->m_tasks.empty()) {
                delete thread;
#if defined(_WIN32)
                return 0UL;
#else
                return nullptr;
#endif // defined(_WIN32)
            }

            task = (threadPool->m_tasks.front()).release();
            threadPool->m_tasks.pop();
        }

        if (task == nullptr)
            continue;

        task->run();
        delete task;
    }

#if defined(_WIN32)
    return 0UL;
#else
    return nullptr;
#endif // defined(_WIN32)
}
