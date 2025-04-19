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
 * @file deque.h
 * @ingroup concurrency
 */
#if !defined(__CONCURRENCY_DEQUE_H__)
#define __CONCURRENCY_DEQUE_H__

#include "common/Thread.h"

#include <deque>
#include <mutex>

namespace concurrent
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Thread-safe std::deque.
     * @ingroup concurrency
     */
    template <typename T>
    class deque
    {
        using __std = std::deque<T>;
    public:
        using iterator = typename __std::iterator;
        using const_iterator = typename __std::const_iterator;

        /**
         * @brief Initializes a new instance of the deque class.
         */
        deque() :
            m_mutex(),
            m_locked(false),
            m_deque()
        {
            /* stub */
        }
        /**
         * @brief Initializes a new instance of the deque class.
         * @param size Initial size of the deque.
         */
        deque(size_t size) :
            m_mutex(),
            m_locked(false),
            m_deque(size)
        {
            /* stub */
        }
        /**
         * @brief Finalizes a instance of the deque class.
         */
        virtual ~deque()
        {
            m_deque.clear();
        }

        /**
         * @brief Deque assignment operator.
         * @param other A deque of identical element and allocator types.
         */
        deque& operator=(const deque& other)
        {
            __lock();
            m_deque = other.m_deque;
            __unlock();
            return *this;
        }
        /**
         * @brief Deque assignment operator.
         * @param other A deque of identical element and allocator types.
         */
        deque& operator=(const std::deque<T>& other)
        {
            __lock();
            m_deque = other;
            __unlock();
            return *this;
        }
        /**
         * @brief Deque assignment operator.
         * @param other A deque of identical element and allocator types.
         */
        deque& operator=(deque& other)
        {
            __lock();
            m_deque = other.m_deque;
            __unlock();
            return *this;
        }
        /**
         * @brief Deque assignment operator.
         * @param other A deque of identical element and allocator types.
         */
        deque& operator=(std::deque<T>& other)
        {
            __lock();
            m_deque = other;
            __unlock();
            return *this;
        }

        /**
         * @brief Assigns a given value to a deque.
         * @param size Number of elements to be assigned.
         * @param value Value to be assigned.
         */
        void assign(size_t size, const T& value)
        {
            __lock();
            m_deque.assign(size, value);
            __unlock();
        }

        /**
         * @brief Gets the total number of elements in the deque.
         * @returns size_t Total number of elements in the deque.
         */
        size_t size() const
        {
            __spinlock();
            return m_deque.size();
        }
        /**
         * @brief Resizes the deque to contain the specified number of elements.
         * @param size Number of elements the deque should contain.
         */
        void resize(size_t size)
        {
            __lock();
            m_deque.resize(size);
            __unlock();
        }

        /**
         * @brief Returns the total number of elements that the deque can
         *  hold before needing to allocate more memory.
         * @returns size_t 
         */
        size_t capacity() const
        {
            __spinlock();
            return m_deque.capacity();
        }

        /**
         * @brief Checks if the deque is empty.
         * @returns bool True if the deque is empty, false otherwise.
         */
        bool empty() const
        {
            __spinlock();
            return m_deque.empty();
        }

        /**
         * @brief Gets the element at the specified index.
         * @param index Index of the element to get.
         * @returns T& Element at the specified index.
         */
        T& operator[](size_t index)
        {
            __spinlock();
            return m_deque[index];
        }
        /**
         * @brief Gets the element at the specified index.
         * @param index Index of the element to get.
         * @returns const T& Element at the specified index.
         */
        const T& operator[](size_t index) const
        {
            __spinlock();
            return m_deque[index];
        }

        /**
         * @brief Gets the element at the specified index.
         * @param index Index of the element to get.
         * @returns T& Element at the specified index.
         */
        T& at(size_t index)
        {
            __spinlock();
            return m_deque.at(index);
        }
        /**
         * @brief Gets the element at the specified index.
         * @param index Index of the element to get.
         * @returns const T& Element at the specified index.
         */
        const T& at(size_t index) const
        {
            __spinlock();
            return m_deque.at(index);
        }

        /**
         * @brief Adds an element to the end of the deque.
         * @param value Value to be added.
         */
        void push_back(const T& value)
        {
            __lock();
            m_deque.push_back(value);
            __unlock();
        }
        /**
         * @brief Adds an element to the end of the deque.
         * @param value Value to be added.
         */
        void push_front(const T& value)
        {
            __lock();
            m_deque.push_front(value);
            __unlock();
        }
        /**
         * @brief Removes the last element of the deque.
         */
        void pop_back()
        {
            __lock();
            m_deque.pop_back();
            __unlock();
        }
        /**
         * @brief Removes the first element of the deque.
         */
        void pop_front()
        {
            __lock();
            m_deque.pop_front();
            __unlock();
        }

        /**
         * @brief Gets the first element of the deque.
         * @returns T& First element of the deque.
         */
        T& front()
        {
            __spinlock();
            return m_deque.front();
        }
        /**
         * @brief Gets the first element of the deque.
         * @returns const T& First element of the deque.
         */
        const T& front() const
        {
            __spinlock();
            return m_deque.front();
        }
        /**
         * @brief Gets the last element of the deque.
         * @returns T& Last element of the deque.
         */
        T& back()
        {
            __spinlock();
            return m_deque.back();
        }
        /**
         * @brief Gets the last element of the deque.
         * @returns const T& Last element of the deque.
         */
        const T& back() const
        {
            __spinlock();
            return m_deque.back();
        }

        /**
         * @brief Removes the element at the specified index.
         * @param index Index of the element to remove.
         */
        void erase(size_t index)
        {
            __lock();
            m_deque.erase(m_deque.begin() + index);
            __unlock();
        }
        /**
         * @brief Removes the element at the specified iterator.
         * @param position Iterator of the element to remove.
         */
        void erase(const_iterator position)
        {
            __lock();
            m_deque.erase(position);
            __unlock();
        }
        /**
         * @brief Removes the elements in the specified range.
         * @param first Iterator of the first element to remove.
         * @param last Iterator of the last element to remove.
         */
        void erase(const_iterator first, const_iterator last)
        {
            __lock();
            m_deque.erase(first, last);
            __unlock();
        }

        /**
         * @brief Swaps data with another deque.
         * @param other A deque of the same element and allocator types.
         */
        void swap(deque& other)
        {
            __lock();
            m_deque.swap(other.m_deque);
            __unlock();
        }

        /**
         * @brief Clears the deque.
         */
        void clear()
        {
            __lock();
            m_deque.clear();
            __unlock();
        }

        /**
         * @brief Gets the underlying deque.
         * @returns std::deque<T>& Underlying deque.
         */
        std::deque<T>& get()
        {
            __spinlock();
            return m_deque;
        }
        /**
         * @brief Gets the underlying deque.
         * @returns const std::deque<T>& Underlying deque.
         */
        const std::deque<T>& get() const
        {
            __spinlock();
            return m_deque;
        }

    private:
        mutable std::mutex m_mutex;     //! Mutex used for hard locking.
        mutable bool m_locked = false;  //! Flag used for soft locking (prevents find lookups), should be used when atomic operations (add/erase/etc) are being used.

        std::deque<T> m_deque;

        /**
         * @brief Locks the deque.
         */
        inline void __lock() const
        {
            m_mutex.lock();
            m_locked = true;
        }
        /**
         * @brief Unlocks the deque.
         */
        inline void __unlock() const
        {
            m_mutex.unlock();
            m_locked = false;
        }
        /**
         * @brief Spins until the deque is unlocked.
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

#endif // __CONCURRENCY_DEQUE_H__
