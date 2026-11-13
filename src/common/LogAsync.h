// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file LogAsync.h
 * @ingroup logging
 */
#if !defined(__LOG_ASYNC_H__)
#define __LOG_ASYNC_H__

#include "common/Defines.h"
#if defined(_WIN32)
#include "common/Clock.h"
#else
#include <sys/time.h>
#endif // defined(_WIN32)
#include "common/Thread.h"

#include <ctime>
#include <string>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <array>

/**
 * @addtogroup logging
 * @{
 */

namespace log_internal
{
    /**
     * @brief Function pointer type for log sinks.
     * @param[in] message The log message.
     */
    typedef void (*LogSink)(const char*);

    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    #define LOG_ASYNC_MAX_QUEUE 64U
    #define LOG_ASYNC_LENGTH 8192U

    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Structure representing an asynchronous log entry.
     * @ingroup logger
     */
    struct LogAsyncEntry {
    public:
        std::array<char, LOG_ASYNC_LENGTH> msg{};   //!< The log message text.
        log_internal::LogSink sink = nullptr;       //!< The log sink function.
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Class for managing asynchronous logging.
     * @ingroup logger
     */
    class DVM_COMMON_API LogAsync {
    public:
        /**
         * @brief Finalizes the LogAsync class.
         */
        ~LogAsync()
        {
            stop();
        }

        /**
         * @brief Starts the asynchronous logging.
         * @return True if the logging was successfully started, false otherwise.
         */
        bool start()
        {
            std::lock_guard<std::mutex> lifetime(lifecycle);
            std::lock_guard<std::mutex> lock(mutex);

            if (started)
                return true;

            stopping = false;
            head = 0U;
            entryCount = 0U;

            if (!Thread::runAsThread(this, threadLogger, &worker))
                return false;

            accepting = true;
            started = true;
            return true;
        }

        /**
         * @brief Stops the asynchronous logging and finalizes the log worker thread.
         */
        void stop()
        {
            std::lock_guard<std::mutex> lifetime(lifecycle);
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (!started)
                    return;

                accepting = false;
                stopping = true;
            }

            changed.notify_all();

#if defined(_WIN32)
            ::WaitForSingleObject(worker.thread, INFINITE);
            ::CloseHandle(worker.thread);
#else
            ::pthread_join(worker.thread, nullptr);
#endif // defined(_WIN32)

            started = false;
        }

        /**
         * @brief Logs a message asynchronously.
         * @param msg The log message text.
         * @param sink The log sink function to use for this message.
         */
        void log(const std::string& msg, log_internal::LogSink sink)
        {
            if (msg.size() >= LOG_ASYNC_LENGTH)
                return;

            std::unique_lock<std::mutex> lock(mutex);

            if (!accepting || sink == nullptr)
                return;

            if (entryCount == LOG_ASYNC_MAX_QUEUE)
                return;

            auto& entry = entries[(head + entryCount) % LOG_ASYNC_MAX_QUEUE];
            ::memcpy(entry.msg.data(), msg.data(), msg.size());

            entry.msg[msg.size()] = '\0';
            entry.sink = sink;
            ++entryCount;

            lock.unlock();
            changed.notify_one();
        }

    private:
        std::array<LogAsyncEntry, LOG_ASYNC_MAX_QUEUE> entries;
        size_t head = 0;
        size_t entryCount = 0;

        std::mutex mutex;
        std::condition_variable changed;

        bool accepting = false;
        bool stopping = false;
        bool started = false;

        thread_t worker {};

        std::mutex lifecycle;

        /**
         * @brief The main function for the log worker thread.
         * @param arg Pointer to the LogAsync instance.
         * @return Always returns nullptr.
         */
        static void* threadLogger(void* arg)
        {
            thread_t* th = (thread_t*)arg;
            if (th != nullptr) {
                LogAsync* logger = static_cast<LogAsync*>(th->obj);
                if (logger == nullptr)
                    return nullptr;

                while (true) {
                    LogAsyncEntry entry;

                    // scope is intentional
                    {
                        std::unique_lock<std::mutex> lock(logger->mutex);
                        logger->changed.wait(lock, [logger] { return logger->entryCount != 0U || logger->stopping; });
                        if (logger->entryCount == 0U && logger->stopping) {
                            lock.unlock();
                            break;
                        }

                        entry = logger->entries[logger->head];
                        logger->head = (logger->head + 1U) % LOG_ASYNC_MAX_QUEUE;
                        --logger->entryCount;
                    }

                    entry.sink(entry.msg.data());

                    if (entry.sink != nullptr) {
                        std::lock_guard<std::mutex> lock(logger->mutex);
                        logger->changed.notify_all();
                    }
                }
            }

            return nullptr;
        }
    };
} // namespace log_internal

/** @} */
#endif // __LOG_ASYNC_H__
