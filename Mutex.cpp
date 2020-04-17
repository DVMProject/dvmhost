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
*   Copyright (C) 2015,2016 by Jonathan Naylor G4KLX
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
#include "Mutex.h"

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

#if defined(_WIN32) || defined(_WIN64)
/// <summary>
/// Initializes a new instance of the Mutex class.
/// </summary>
Mutex::Mutex() :
    m_handle()
{
    m_handle = ::CreateMutex(NULL, FALSE, NULL);
}

/// <summary>
/// Finalizes a instance of the Mutex class.
/// </summary>
Mutex::~Mutex()
{
    ::CloseHandle(m_handle);
}

/// <summary>
/// Locks the mutex.
/// </summary>
void Mutex::lock()
{
    ::WaitForSingleObject(m_handle, INFINITE);
}

/// <summary>
/// Unlocks the mutex.
/// </summary>
void Mutex::unlock()
{
    ::ReleaseMutex(m_handle);
}
#else
/// <summary>
/// Initializes a new instance of the Mutex class.
/// </summary>
Mutex::Mutex() :
    m_mutex(PTHREAD_MUTEX_INITIALIZER)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the Mutex class.
/// </summary>
Mutex::~Mutex()
{
    /* stub */
}

/// <summary>
/// Locks the mutex.
/// </summary>
void Mutex::lock()
{
    ::pthread_mutex_lock(&m_mutex);
}

/// <summary>
/// Unlocks the mutex.
/// </summary>
void Mutex::unlock()
{
    ::pthread_mutex_unlock(&m_mutex);
}
#endif
