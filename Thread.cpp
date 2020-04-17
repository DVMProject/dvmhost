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
#include "Thread.h"

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#endif

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

#if defined(_WIN32) || defined(_WIN64)
/// <summary>
/// Initializes a new instance of the Thread class.
/// </summary>
Thread::Thread() :
    m_handle()
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the Thread class.
/// </summary>
Thread::~Thread()
{
    /* stub */
}

/// <summary>
/// Starts the thread execution.
/// </summary>
/// <returns>True, if thread started, otherwise false.</returns>
bool Thread::run()
{
    m_handle = ::CreateThread(NULL, 0, &helper, this, 0, NULL);

    return m_handle != NULL;
}

/// <summary>
///
/// </summary>
void Thread::wait()
{
    ::WaitForSingleObject(m_handle, INFINITE);

    ::CloseHandle(m_handle);
}

/// <summary>
///
/// </summary>
/// <param name="ms"></param>
void Thread::sleep(uint32_t ms)
{
    ::Sleep(ms);
}
#else
/// <summary>
/// Initializes a new instance of the Thread class.
/// </summary>
Thread::Thread() :
    m_thread()
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the Thread class.
/// </summary>
Thread::~Thread()
{
    /* stub */
}

/// <summary>
/// Starts the thread execution.
/// </summary>
/// <returns>True, if thread started, otherwise false.</returns>
bool Thread::run()
{
    return ::pthread_create(&m_thread, NULL, helper, this) == 0;
}

/// <summary>
///
/// </summary>
void Thread::wait()
{
    ::pthread_join(m_thread, NULL);
}

/// <summary>
///
/// </summary>
/// <param name="ms"></param>
void Thread::sleep(uint32_t ms)
{
    ::usleep(ms * 1000);
}
#endif

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

#if defined(_WIN32) || defined(_WIN64)
/// <summary>
///
/// </summary>
/// <param name="arg"></param>
/// <returns></returns>
DWORD Thread::helper(LPVOID arg)
{
    Thread* p = (Thread*)arg;

    p->entry();

    return 0UL;
}
#else
/// <summary>
///
/// </summary>
/// <param name="arg"></param>
/// <returns></returns>
void* Thread::helper(void* arg)
{
    Thread* p = (Thread*)arg;

    p->entry();

    return NULL;
}
#endif
