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
#if !defined(__MUTEX_H__)
#define __MUTEX_H__

#include "Defines.h"

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif

// ---------------------------------------------------------------------------
//  Class Declaration
//      Implements a simple mutual exclusion locking mechanism.
// ---------------------------------------------------------------------------

class HOST_SW_API Mutex {
public:
    /// <summary>Initializes a new instance of the Mutex class.</summary>
    Mutex();
    /// <summary>Finalizes a instance of the Mutex class.</summary>
    ~Mutex();

    /// <summary>Locks the mutex.</summary>
    void lock();
    /// <summary>Unlocks the mutex.</summary>
    void unlock();

private:
#if defined(_WIN32) || defined(_WIN64)
    HANDLE m_handle;
#else
    pthread_mutex_t m_mutex;
#endif
};

#endif // __MUTEX_H__
