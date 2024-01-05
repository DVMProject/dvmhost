/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2015,2016 by Jonathan Naylor G4KLX
*   Copyright (C) 2023 by Bryan Biedenkapp N2PLL
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

#include <sys/prctl.h>
#include <signal.h>
#include <unistd.h>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the Thread class.
/// </summary>
Thread::Thread() :
    m_thread(),
    m_started(false)
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
    if (m_started)
        return m_started;

    m_started = true;
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
/// <param name="name"></param>
void Thread::setName(std::string name)
{
    if (!m_started)
        return;
    if (pthread_kill(m_thread, 0) != 0)
        return;
#ifdef _GNU_SOURCE
    ::pthread_setname_np(m_thread, name.c_str());
#endif // _GNU_SOURCE
}

/// <summary>
///
/// </summary>
/// <param name="ms"></param>
void Thread::sleep(uint32_t ms)
{
    ::usleep(ms * 1000);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
///
/// </summary>
/// <param name="arg"></param>
/// <returns></returns>
void* Thread::helper(void* arg)
{
    Thread* p = (Thread*)arg;
    p->entry();

    return nullptr;
}
