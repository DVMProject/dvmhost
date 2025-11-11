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
 * @file shared_vector.h
 * @ingroup concurrency
 */
#if !defined(__CONCURRENCY_SHARED_VECTOR_H__)
#define __CONCURRENCY_SHARED_VECTOR_H__

#include "common/concurrent/concurrent_shared_lock.h"
#include "common/Thread.h"

#include <vector>
#include <mutex>

namespace concurrent
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Thread-safe share-locked std::vector. Read operations
     *  must use shared_lock()/shared_unlock() to ensure thread-safety. (This includes iterators.)
     * @ingroup concurrency
     */
    template <typename T>
    class shared_vector : public concurrent_shared_lock
    {
        using __std = std::vector<T>;
    public:
        using iterator = typename __std::iterator;
        using const_iterator = typename __std::const_iterator;

        /**
         * @brief Initializes a new instance of the shared_vector class.
         */
        shared_vector() : concurrent_shared_lock(),
            m_vector()
        {
            /* stub */
        }
        /**
         * @brief Initializes a new instance of the shared_vector class.
         * @param size Initial size of the vector.
         */
        shared_vector(size_t size) : concurrent_shared_lock(),
            m_vector(size)
        {
            /* stub */
        }
        /**
         * @brief Finalizes a instance of the shared_vector class.
         */
        virtual ~shared_vector()
        {
            m_vector.clear();
        }

        /**
         * @brief Vector assignment operator.
         * @param other A vector of identical element and allocator types.
         */
        shared_vector& operator=(const shared_vector& other)
        {
            __lock();
            m_vector = other.m_vector;
            __unlock();
            return *this;
        }
        /**
         * @brief Vector assignment operator.
         * @param other A vector of identical element and allocator types.
         */
        shared_vector& operator=(const std::vector<T>& other)
        {
            __lock();
            m_vector = other;
            __unlock();
            return *this;
        }
        /**
         * @brief Vector assignment operator.
         * @param other A vector of identical element and allocator types.
         */
        shared_vector& operator=(shared_vector& other)
        {
            __lock();
            m_vector = other.m_vector;
            __unlock();
            return *this;
        }
        /**
         * @brief Vector assignment operator.
         * @param other A vector of identical element and allocator types.
         */
        shared_vector& operator=(std::vector<T>& other)
        {
            __lock();
            m_vector = other;
            __unlock();
            return *this;
        }

        /**
         * @brief Assigns a given value to a %vector.
         * @param size Number of elements to be assigned.
         * @param value Value to be assigned.
         */
        void assign(size_t size, const T& value)
        {
            __lock();
            m_vector.assign(size, value);
            __unlock();
        }

        /**
         * @brief Returns a read/write iterator that points to the first
         *  element in the vector. Iteration is done in ordinary
         *  element order.
         * @returns iterator 
         */
        iterator begin()
        {
            return m_vector.begin();
        }
        /**
         * @brief Returns a read-only (constant) iterator that points to the
         *  first element in the vector. Iteration is done in ordinary
         *  element order.
         * @returns const_iterator 
         */
        const_iterator begin() const
        {
            return m_vector.begin();
        }
        /**
         * @brief Returns a read/write iterator that points one past the last
         *  element in the vector. Iteration is done in ordinary
         *  element order.
         * @returns iterator 
         */
        iterator end()
        {
            return m_vector.end();
        }
        /**
         * @brief Returns a read-only (constant) iterator that points one past
         *  the last element in the vector. Iteration is done in ordinary
         *  element order.
         * @returns const_iterator 
         */
        const_iterator end() const
        {
            return m_vector.end();
        }

        /**
         * @brief Returns a read-only (constant) iterator that points to the
         *  first element in the vector. Iteration is done in ordinary
         *  element order.
         * @returns const_iterator 
         */
        const_iterator cbegin() const
        {
            return m_vector.cbegin();
        }
        /**
         * @brief Returns a read-only (constant) iterator that points one past
         *  the last element in the vector. Iteration is done in ordinary
         *  element order.
         * @returns const_iterator 
         */
        const_iterator cend() const
        {
            return m_vector.cend();
        }

        /**
         * @brief Gets the number of elements in the vector.
         * @returns size_t Number of elements in the vector.
         */
        size_t size() const
        {
            return m_vector.size();
        }
        /**
         * @brief Resizes the %vector to the specified number of elements.
         * @param size Number of elements the %vector should contain.
         */        
        void resize(size_t size)
        {
            __lock();
            m_vector.resize(size);
            __unlock();
        }

        /**
         * @brief Returns the total number of elements that the %vector can
         *  hold before needing to allocate more memory.
         * @returns size_t 
         */
        size_t capacity() const
        {
            return m_vector.capacity();
        }

        /**
         * @brief Checks if the vector is empty.
         * @returns bool True if the vector is empty, false otherwise.
         */
        bool empty() const
        {
            return m_vector.empty();
        }

        /**
         * @brief Gets the element at the specified index.
         * @param index Index of the element to get.
         * @returns T& Element at the specified index.
         */
        T& operator[](size_t index)
        {
            return m_vector[index];
        }
        /**
         * @brief Gets the element at the specified index.
         * @param index Index of the element to get.
         * @returns const T& Element at the specified index.
         */
        const T& operator[](size_t index) const
        {
            return m_vector[index];
        }

        /**
         * @brief Gets the element at the specified index.
         * @param index Index of the element to get.
         * @returns T& Element at the specified index.
         */
        T& at(size_t index)
        {
            return m_vector.at(index);
        }
        /**
         * @brief Gets the element at the specified index.
         * @param index Index of the element to get.
         * @returns const T& Element at the specified index.
         */
        const T& at(size_t index) const
        {
            return m_vector.at(index);
        }

        /**
         * @brief Gets the first element of the vector.
         * @returns T& First element of the vector.
         */
        T& front()
        {
            return m_vector.front();
        }
        /**
         * @brief Gets the first element of the vector.
         * @returns const T& First element of the vector.
         */
        const T& front() const
        {
            return m_vector.front();
        }

        /**
         * @brief Gets the last element of the vector.
         * @returns T& Last element of the vector.
         */
        T& back()
        {
            return m_vector.back();
        }
        /**
         * @brief Gets the last element of the vector.
         * @returns const T& Last element of the vector.
         */
        const T& back() const
        {
            return m_vector.back();
        }

        /**
         * @brief Adds an element to the end of the vector.
         * @param value Value to add.
         */
        void push_back(const T& value)
        {
            __lock();
            m_vector.push_back(value);
            __unlock();
        }
        /**
         * @brief Adds an element to the end of the vector.
         * @param value Value to add.
         */
        void push_back(T&& value)
        {
            __lock();
            m_vector.push_back(std::move(value));
            __unlock();
        }

        /**
         * @brief Removes last element.
         */
        void pop_back()
        {
            __lock();
            m_vector.pop_back();
            __unlock();
        }

        /**
         * @brief Inserts given value into vector before specified iterator.
         * @param position A const_iterator into the vector.
         * @param value Data to be inserted.
         * @return iterator An iterator that points to the inserted data.
         */       
        iterator insert(iterator position, const T& value)
        {
            __lock();
            auto it = m_vector.insert(position, value);
            __unlock();
            return it;
        }

        /**
         * @brief Removes the element at the specified index.
         * @param index Index of the element to remove.
         */
        void erase(size_t index)
        {
            __lock();
            m_vector.erase(m_vector.begin() + index);
            __unlock();
        }
        /**
         * @brief Removes the element at the specified iterator.
         * @param position Iterator of the element to remove.
         */
        void erase(const_iterator position)
        {
            __lock();
            m_vector.erase(position);
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
            m_vector.erase(first, last);
            __unlock();
        }

        /**
         * @brief Swaps data with another vector.
         * @param other A vector of the same element and allocator types.
         */
        void swap(shared_vector& other)
        {
            __lock();
            m_vector.swap(other.m_vector);
            __unlock();
        }
        
        /**
         * @brief Clears the vector.
         */
        void clear()
        {
            __lock();
            m_vector.clear();
            __unlock();
        }

        /**
         * @brief Gets the underlying vector.
         * @returns std::vector<T>& Underlying vector.
         */
        std::vector<T>& get()
        {
            return m_vector;
        }
        /**
         * @brief Gets the underlying vector.
         * @returns const std::vector<T>& Underlying vector.
         */
        const std::vector<T>& get() const
        {
            return m_vector;
        }

        /**
         *  @brief Prepare the underlying vector for a specified number of
         *      elements.
         *  @param n Number of elements required.
         */
        void reserve(size_t n)
        {
            m_vector.reserve(n);
        }

    private:
        std::vector<T> m_vector;
    };
} // namespace concurrent

#endif // __CONCURRENCY_SHARED_VECTOR_H__
