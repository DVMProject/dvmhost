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
 * @file concurrent_lock.h
 * @ingroup concurrency
 */
#if !defined(__CONCURRENCY_CONCURRENT_LOCK_H__)
#define __CONCURRENCY_CONCURRENT_LOCK_H__

#include "common/Thread.h"

#include <mutex>
#include <condition_variable>

namespace concurrent
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Base class for a concurrently locked container.
     * @ingroup concurrency
     */
    class concurrent_lock
    {
    public:
        /**
         * @brief Initializes a new instance of the concurrent_lock class.
         */
        concurrent_lock() :
            m_mutex(),
            m_locked(false)
        {
            /* stub */
        }

        /**
         * @brief Locks the object.
         * @param readLock Flag indicating whether or not to use read locking.
         */
        void lock(bool readLock = true) const { __lock(readLock); }
        /**
         * @brief Unlocks the object.
         */
        void unlock() const { __unlock(); }
        /**
         * @brief Flag indicating whether or not the object is read locked.
         * @return bool True if the object is read locked, false otherwise.
         */
        bool isReadLocked() const { return m_locked; }
        /**
         * @brief Spins until the object is unlocked.
         */
        void spinlock() const { __spinlock(); }

    protected:
        mutable std::mutex m_mutex;     //! Mutex used for change locking.
        mutable bool m_locked = false;  //! Flag used for read locking (prevents find lookups), should be used when atomic operations (add/erase/etc) are being used.

        /**
         * @brief Lock the object.
         * @param readLock Flag indicating whether or not to use read locking.
         */
        inline void __lock(bool readLock = true) const
        {
            m_mutex.lock();
            if (readLock)
                m_locked = true;
        }

        /**
         * @brief Unlock the object.
         */
        inline void __unlock() const
        {
            m_mutex.unlock();
            m_locked = false;
        }

        /**
         * @brief Spins until the object is read unlocked.
         */
        inline void __spinlock() const
        {
            if (m_locked) {
                while (m_locked)
                    Thread::sleep(1U);
            }
        }
    };
} // namespace concurrent

#endif // __CONCURRENCY_CONCURRENT_LOCK_H__
