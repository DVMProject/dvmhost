/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2006-2009,2012,2013,2015,2016 by Jonathan Naylor G4KLX
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#if !defined(__RING_BUFFER_H__)
#define __RING_BUFFER_H__

#include "Defines.h"
#include "Log.h"

#include <cstdio>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Class Declaration
//      Implements a circular buffer for storing data.
// ---------------------------------------------------------------------------

template<class T>
class HOST_SW_API RingBuffer {
public:
    /// <summary>Initializes a new instance of the RingBuffer class.</summary>
    /// <param name="length">Length of ring buffer.</param>
    /// <param name="name">Name of buffer.</param>
    RingBuffer(uint32_t length, const char* name) :
        m_length(length),
        m_name(name),
        m_buffer(NULL),
        m_iPtr(0U),
        m_oPtr(0U)
    {
        assert(length > 0U);
        assert(name != NULL);

        m_buffer = new T[length];

        ::memset(m_buffer, 0x00, m_length * sizeof(T));
    }

    /// <summary>Finalizes a instance of the RingBuffer class.</summary>
    ~RingBuffer()
    {
        delete[] m_buffer;
    }

    /// <summary>Adds data to the end of the ring buffer.</summary>
    /// <param name="buffer">Data buffer.</param>
    /// <param name="length">Length of data in buffer.</param>
    /// <returns>True, if data is added to ring buffer, otherwise false.</returns>
    bool addData(const T* buffer, uint32_t length)
    {
        if (length >= freeSpace()) {
            LogError(LOG_HOST, "%s buffer overflow, clearing the buffer. (%u >= %u)", m_name, length, freeSpace());
            clear();
            return false;
        }

        for (uint32_t i = 0U; i < length; i++) {
            m_buffer[m_iPtr++] = buffer[i];

            if (m_iPtr == m_length)
                m_iPtr = 0U;
        }

        return true;
    }

    /// <summary>Gets data from the ring buffer.</summary>
    /// <param name="buffer">Buffer to write data to be retrieved.</param>
    /// <param name="length">Length of data to retrieve.</param>
    /// <returns>True, if data is read from ring buffer, otherwise false.</returns>
    bool getData(T* buffer, uint32_t length)
    {
        if (dataSize() < length) {
            LogError(LOG_HOST, "**** Underflow in %s ring buffer, %u < %u", m_name, dataSize(), length);
            return false;
        }

        for (uint32_t i = 0U; i < length; i++) {
            buffer[i] = m_buffer[m_oPtr++];

            if (m_oPtr == m_length)
                m_oPtr = 0U;
        }

        return true;
    }

    /// <summary>Gets data from ring buffer without moving buffer pointers.</summary>
    /// <param name="buffer">Buffer to write data to be retrieved.</param>
    /// <param name="length">Length of data to retrieve.</param>
    /// <returns>True, if data is read from ring buffer, otherwise false.</returns>
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

    /// <summary>Clears ring buffer and resets data pointers.</summary>
    void clear()
    {
        m_iPtr = 0U;
        m_oPtr = 0U;

        ::memset(m_buffer, 0x00, m_length * sizeof(T));
    }

    /// <summary>Resizes the ring buffer to the specified length.</summary>
    /// <param name="length">New length of the ring buffer.</param>
    void resize(uint32_t length)
    {
        clear();

        delete[] m_buffer;

        m_length = length;
        m_buffer = new T[length];

        clear();
    }

    /// <summary>Returns the currently available space in the ring buffer.</summary>
    /// <returns>Space free in the ring buffer.</returns>
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

    /// <summary>Returns the size of the data currently stored in the ring buffer.</summary>
    /// <returns>Size of data stored in the ring buffer.</returns>
    uint32_t dataSize() const
    {
        return m_length - freeSpace();
    }

    /// <summary>Gets the length of the ring buffer.</summary>
    /// <returns>Length of ring buffer.</returns>
    uint32_t length() const
    {
        return m_length;
    }

    /// <summary>Helper to test if the given length of data would fit in the ring buffer.</summary>
    /// <returns>True, if specified length will fit in buffer, otherwise false.</returns>
    bool hasSpace(uint32_t length) const
    {
        return freeSpace() > length;
    }

    /// <summary>Helper to return whether the ring buffer contains data.</summary>
    /// <returns>True, if ring buffer contains data, otherwise false.</returns>
    bool hasData() const
    {
        return m_oPtr != m_iPtr;
    }

    /// <summary>Helper to return whether the ring buffer is empty or not.</summary>
    /// <returns>True, if the ring buffer is empty, otherwise false.</returns>
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
