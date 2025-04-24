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
 * @file unordered_map.h
 * @ingroup concurrency
 */
#if !defined(__CONCURRENCY_UNORDERED_MAP_H__)
#define __CONCURRENCY_UNORDERED_MAP_H__

#include "common/concurrent/concurrent_lock.h"
#include "common/Thread.h"

#include <unordered_map>
#include <mutex>

namespace concurrent
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Thread-safe std::unordered_map.
     * @ingroup concurrency
     */
    template <typename Key, typename T>
    class unordered_map : public concurrent_lock
    {
        using __std = std::unordered_map<Key, T>;
    public:
        using iterator = typename __std::iterator;
        using const_iterator = typename __std::const_iterator;

        /**
         * @brief Initializes a new instance of the unordered_map class.
         */
        unordered_map() : concurrent_lock(),
            m_map()
        {
            /* stub */
        }
        /**
         * @brief Initializes a new instance of the unordered_map class.
         * @param size Initial size of the unordered_map.
         */
        unordered_map(size_t size) : concurrent_lock(),
            m_map(size)
        {
            /* stub */
        }
        /**
         * @brief Finalizes a instance of the unordered_map class.
         */
        virtual ~unordered_map()
        {
            m_map.clear();
        }

        /**
         * @brief Unordered map assignment operator.
         * @param other A map of identical element and allocator types.
         */
        unordered_map& operator=(const unordered_map& other)
        {
            __lock(false);
            m_map = other.m_map;
            __unlock();
            return *this;
        }
        /**
         * @brief Unordered map assignment operator.
         * @param other A map of identical element and allocator types.
         */
        unordered_map& operator=(const std::unordered_map<Key, T>& other)
        {
            __lock(false);
            m_map = other;
            __unlock();
            return *this;
        }
        /**
         * @brief Unordered map assignment operator.
         * @param other A map of identical element and allocator types.
         */
        unordered_map& operator=(unordered_map& other)
        {
            __lock(false);
            m_map = other.m_map;
            __unlock();
            return *this;
        }
        /**
         * @brief Unordered map assignment operator.
         * @param other A map of identical element and allocator types.
         */
        unordered_map& operator=(std::unordered_map<Key, T>& other)
        {
            __lock(false);
            m_map = other;
            __unlock();
            return *this;
        }

        /**
         * @brief Assigns a given value to a unordered_map.
         * @param size Number of elements to be assigned.
         * @param value Value to be assigned.
         */
        void assign(size_t size, const T& value)
        {
            __lock(false);
            m_map.assign(size, value);
            __unlock();
        }

        /**
         * @brief Returns a read/write iterator that points to the first
         *  element in the unordered_map. Iteration is done in ordinary
         *  element order.
         * @returns iterator 
         */
        iterator begin()
        {
            __spinlock();
            return m_map.begin();
        }
        /**
         * @brief Returns a read-only (constant) iterator that points to the
         *  first element in the unordered_map. Iteration is done in ordinary
         *  element order.
         * @returns const_iterator 
         */
        const_iterator begin() const
        {
            __spinlock();
            return m_map.begin();
        }
        /**
         * @brief Returns a read/write iterator that points one past the last
         *  element in the unordered_map. Iteration is done in ordinary
         *  element order.
         * @returns iterator 
         */
        iterator end()
        {
            __spinlock();
            return m_map.end();
        }
        /**
         * @brief Returns a read-only (constant) iterator that points one past
         *  the last element in the unordered_map. Iteration is done in ordinary
         *  element order.
         * @returns const_iterator 
         */
        const_iterator end() const
        {
            __spinlock();
            return m_map.end();
        }

        /**
         * @brief Returns a read-only (constant) iterator that points to the
         *  first element in the unordered_map. Iteration is done in ordinary
         *  element order.
         * @returns const_iterator 
         */
        const_iterator cbegin() const
        {
            __spinlock();
            return m_map.cbegin();
        }
        /**
         * @brief Returns a read-only (constant) iterator that points one past
         *  the last element in the unordered_map. Iteration is done in ordinary
         *  element order.
         * @returns const_iterator 
         */
        const_iterator cend() const
        {
            __spinlock();
            return m_map.cend();
        }

        /**
         * @brief Gets the element at the specified key.
         * @param key Key of the element to get.
         * @returns T& Element at the specified key.
         */
        T& operator[](const Key& key)
        {
            __spinlock();
            return m_map[key];
        }
        /**
         * @brief Gets the element at the specified key.
         * @param key Key of the element to get.
         * @returns const T& Element at the specified key.
         */
        const T& operator[](const Key& key) const
        {
            __spinlock();
            return m_map[key];
        }

        /**
         * @brief Gets the element at the specified key.
         * @param key Key of the element to get.
         * @returns T& Element at the specified key.
         */
        T& at(const Key& key)
        {
            __spinlock();
            return m_map.at(key);
        }
        /**
         * @brief Gets the element at the specified key.
         * @param key Key of the element to get.
         * @returns const T& Element at the specified key.
         */
        const T& at(const Key& key) const
        {
            __spinlock();
            return m_map.at(key);
        }

        /**
         * @brief Gets the total number of elements in the unordered_map.
         * @returns size_t Total number of elements in the unordered_map.
         */
        size_t size() const
        {
            __spinlock();
            return m_map.size();
        }

        /**
         * @brief Checks if the unordered_map is empty.
         * @returns bool True if the unordered_map is empty, false otherwise.
         */
        bool empty() const
        {
            __spinlock();
            return m_map.empty();
        }

        /**
         * @brief Checks if the unordered_map contains the specified key.
         * @param key Key to check.
         * @returns bool True if the unordered_map contains the specified key, false otherwise.
         */
        bool contains(const Key& key) const
        {
            __spinlock();
            return m_map.contains(key);
        }

        /**
         * @brief Inserts a new element into the unordered_map.
         * @param key Key of the element to insert.
         * @param value Value of the element to insert.
         */
        void insert(const Key& key, const T& value)
        {
            __lock(false);
            m_map.insert({key, value});
            __unlock();
        }

        /**
         * @brief Removes the element at the specified key.
         * @param key Key of the element to remove.
         */
        void erase(const Key& key)
        {
            __lock(false);
            m_map.erase(key);
            __unlock();
        }
        /**
         * @brief Removes the element at the specified iterator.
         * @param position Iterator of the element to remove.
         */
        void erase(const_iterator position)
        {
            __lock(false);
            m_map.erase(position);
            __unlock();
        }
        /**
         * @brief Removes the elements in the specified range.
         * @param first Iterator of the first element to remove.
         * @param last Iterator of the last element to remove.
         */
        void erase(const_iterator first, const_iterator last)
        {
            __lock(false);
            m_map.erase(first, last);
            __unlock();
        }

        /**
         * @brief Clears the unordered_map.
         */
        void clear()
        {
            __lock(false);
            m_map.clear();
            __unlock();
        }

        /**
         * @brief Tries to locate an element in an unordered_map.
         * @param key Key to be located.
         * @return iterator Iterator pointing to sought-after element, or end() if not
         *  found.
         */
        iterator find(const Key& key)
        {
            __spinlock();
            return m_map.find(key);
        }
        /**
         * @brief Tries to locate an element in an unordered_map.
         * @param key Key to be located.
         * @return const_iterator Iterator pointing to sought-after element, or end() if not
         *  found.
         */
        const_iterator find(const Key& key) const
        {
            __spinlock();
            return m_map.find(key);
        }

        /**
         * @brief Finds the number of elements.
         * @param key Key to count.
         * @return size_t Number of elements with specified key.
         */
        size_t count(const Key& key) const
        {
            __spinlock();
            return m_map.count(key);
        }

        /**
         * @brief Gets the underlying unordered_map.
         * @returns std::unordered_map<Key, T>& Underlying unordered_map.
         */
        std::unordered_map<Key, T>& get()
        {
            __spinlock();
            return m_map;
        }
        /**
         * @brief Gets the underlying unordered_map.
         * @returns const std::unordered_map<Key, T>& Underlying unordered_map.
         */
        const std::unordered_map<Key, T>& get() const
        {
            __spinlock();
            return m_map;
        }

    private:
        std::unordered_map<Key, T> m_map;
    };
} // namespace concurrent

#endif // __CONCURRENCY_UNORDERED_MAP_H__
