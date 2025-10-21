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
 * @file concurrent_shared_lock.h
 * @ingroup concurrency
 */
#if !defined(__CONCURRENCY_CONCURRENT_SHARED_LOCK_H__)
#define __CONCURRENCY_CONCURRENT_SHARED_LOCK_H__

#include "common/Thread.h"

#include <shared_mutex>
#include <condition_variable>

namespace concurrent
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Base class for a concurrently shared locked container.
     * @ingroup concurrency
     */
    class concurrent_shared_lock
    {
    public:
        /**
         * @brief Initializes a new instance of the concurrent_shared_lock class.
         */
        concurrent_shared_lock() :
            m_mutex()
        {
            /* stub */
        }

        /**
         * @brief Locks the object.
         */
        void lock() const { __lock(); }
        /**
         * @brief Unlocks the object.
         */
        void unlock() const { __unlock(); }

        /**
         * @brief Share locks the object.
         */
        void shared_lock() const { __shared_lock(); }
        /**
         * @brief Share unlocks the object.
         */
        void shared_unlock() const { __shared_unlock(); }

    protected:
        mutable std::shared_timed_mutex m_mutex;    //!< Mutex used for locking.

        /**
         * @brief Lock the object.
         */
        inline void __lock() const { m_mutex.lock(); }
        /**
         * @brief Lock the object.
         */
        inline void __shared_lock() const { m_mutex.lock_shared(); }

        /**
         * @brief Unlock the object.
         */
        inline void __unlock() const { m_mutex.unlock(); }
        /**
         * @brief Unlock the object.
         */
        inline void __shared_unlock() const { m_mutex.unlock_shared(); }
    };
} // namespace concurrent

#endif // __CONCURRENCY_CONCURRENT_SHARED_LOCK_H__
