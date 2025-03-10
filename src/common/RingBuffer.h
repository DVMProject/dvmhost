// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2006-2009,2012,2013,2015,2016 Jonathan Naylor, G4KLX
 *
 */
/**
 * @file Ringbuffer.h
 * @ingroup common
 */
#if !defined(__RING_BUFFER_H__)
#define __RING_BUFFER_H__

#include "common/Defines.h"
#include "common/Log.h"

#include <cstdio>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Cirular buffer for storing data.
 * @ingroup common
 * @tparam T Type of data to store in RingBuffer.
 */
template<class T>
class HOST_SW_API RingBuffer {
public:
    /**
     * @brief Initializes a new instance of the RingBuffer class.
     * @param length Length of ring buffer.
     * @param name Name of buffer.
     */
    RingBuffer(uint32_t length, const char* name) :
        m_length(length),
        m_name(name),
        m_buffer(nullptr),
        m_iPtr(0U),
        m_oPtr(0U)
    {
        assert(length > 0U);

        m_buffer = new T[length];
        ::memset(m_buffer, 0x00, m_length * sizeof(T));
    }

    /**
     * @brief Finalizes a instance of the RingBuffer class.
     */
    ~RingBuffer()
    {
        delete[] m_buffer;
    }

    /**
     * @brief Adds data to the end of the ring buffer.
     * @param buffer Data buffer.
     * @param length Length of data in buffer.
     * @return bool True, if data is added to ring buffer, otherwise false.
     */
    bool addData(const T* buffer, uint32_t length)
    {
        if (length > freeSpace()) {
            LogError(LOG_HOST, "**** Overflow in %s ring buffer, %u > %u, clearing the buffer", m_name, length, freeSpace());
            clear();
            return false;
        }
#if DEBUG_RINGBUFFER
        uint32_t iPtr_BeforeWrite = m_iPtr;
#endif
        for (uint32_t i = 0U; i < length; i++) {
            m_buffer[m_iPtr++] = buffer[i];

            if (m_iPtr == m_length)
                m_iPtr = 0U;
        }
#if DEBUG_RINGBUFFER
        LogDebugEx(LOG_HOST, "RingBuffer::addData()", "(%s): iPtr_Before = %u, iPtr_After = %u, oPtr = %u, len = %u, len_Written = %u", m_name, iPtr_BeforeWrite, m_iPtr, m_oPtr, m_length, (m_iPtr - iPtr_BeforeWrite));
#endif
        return true;
    }

    /**
     * @brief Gets data from the ring buffer.
     * @param buffer Buffer to write data to be retrieved.
     * @param length Length of data to retrieve.
     * @return bool True, if data is read from ring buffer, otherwise false.
     */
    bool get(T* buffer, uint32_t length)
    {
        if (dataSize() < length) {
            LogError(LOG_HOST, "**** Underflow get in %s ring buffer, %u < %u", m_name, dataSize(), length);
            return false;
        }
#if DEBUG_RINGBUFFER
        uint32_t oPtr_BeforeRead = m_oPtr;
#endif
        for (uint32_t i = 0U; i < length; i++) {
            buffer[i] = m_buffer[m_oPtr++];

            if (m_oPtr == m_length)
                m_oPtr = 0U;
        }
#if DEBUG_RINGBUFFER
        LogDebugEx(LOG_HOST, "RingBuffer::getData()", "(%s): iPtr = %u, oPtr_Before = %u, oPtr_After = %u, len = %u, len_Read = %u", m_name, m_iPtr, oPtr_BeforeRead, m_oPtr, m_length, (m_oPtr - oPtr_BeforeRead));
#endif
        return true;
    }

    /**
     * @brief Gets data from ring buffer without moving buffer pointers.
     * @param buffer Buffer to write data to be retrieved.
     * @param length Length of data to retrieve.
     * @return bool True, if data is read from ring buffer, otherwise false.
     */
    bool peek(T* buffer, uint32_t length)
    {
        if (dataSize() < length) {
            LogError(LOG_HOST, "**** Underflow peek in %s ring buffer, %u < %u", m_name, dataSize(), length);
            return false;
        }

        uint32_t ptr = m_oPtr;
        for (uint32_t i = 0U; i < length; i++) {
            buffer[i] = m_buffer[ptr++];

            if (ptr == m_length)
                ptr = 0U;
        }

        return true;
    }

    /**
     * @brief Clears ring buffer and resets data pointers.
     */
    void clear()
    {
        m_iPtr = 0U;
        m_oPtr = 0U;

        ::memset(m_buffer, 0x00, m_length * sizeof(T));
    }

    /**
     * @brief Resizes the ring buffer to the specified length.
     * @param length New length of the ring buffer.
     */
    void resize(uint32_t length)
    {
        clear();

        delete[] m_buffer;

        m_length = length;
        m_buffer = new T[length];

        clear();
    }

    /**
     * @brief Returns the currently available space in the ring buffer.
     * @return uint32_t Space free in the ring buffer.
     */
    uint32_t freeSpace() const
    {
        uint32_t len = m_length;

        if (m_oPtr > m_iPtr)
            len = m_oPtr - m_iPtr;
        else if (m_iPtr > m_oPtr)
            len = m_length - (m_iPtr - m_oPtr);

        if (len > m_length)
            len = 0U;

        return len;
    }

    /**
     * @brief Returns the size of the data currently stored in the ring buffer.
     * @return uint32_t Size of data stored in the ring buffer.
     */
    uint32_t dataSize() const
    {
        return m_length - freeSpace();
    }

    /**
     * @brief Gets the length of the ring buffer.
     * @return uint32_t Length of ring buffer.
     */
    uint32_t length() const
    {
        return m_length;
    }

    /**
     * @brief Helper to test if the given length of data would fit in the ring buffer.
     * @param length Length to check.
     * @return bool True, if specified length will fit in buffer, otherwise false.
     */
    bool hasSpace(uint32_t length) const
    {
        return freeSpace() > length;
    }

    /**
     * @brief Helper to return whether the ring buffer contains data.
     * @return bool True, if ring buffer contains data, otherwise false.
     */
    bool hasData() const
    {
        return m_oPtr != m_iPtr;
    }

    /**
     * @brief Helper to return whether the ring buffer is empty or not.
     * @return bool True, if the ring buffer is empty, otherwise false.
     */
    bool isEmpty() const
    {
        return m_oPtr == m_iPtr;
    }

private:
    uint32_t m_length;

    const char* m_name;

    T* m_buffer;

    uint32_t m_iPtr;
    uint32_t m_oPtr;
};

#endif // __RING_BUFFER_H__
