// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
*   Copyright (C) 2023 Bryan Biedenkapp, N2PLL
*
*/
#include "Thread.h"
#include "Log.h"

#include <cerrno>
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
Thread::~Thread() = default;

/// <summary>
/// Starts the thread execution.
/// </summary>
/// <returns>True, if thread started, otherwise false.</returns>
bool Thread::run()
{
    if (m_started)
        return m_started;

    m_started = true;
    int err = ::pthread_create(&m_thread, NULL, helper, this);
    if (err != 0) {
        LogError(LOG_NET, "Error returned from pthread_create, err: %d", errno);
        return false;
    }

    return true;
}

/// <summary>
/// Make calling thread wait for termination of the thread.
/// </summary>
void Thread::wait()
{
    ::pthread_join(m_thread, NULL);
}

/// <summary>
/// Set thread name visible in the kernel and its interfaces.
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
/// Indicate that the thread is never to be joined with wait().
/// The resources of thread will therefore be freed immediately when it
/// terminates, instead of waiting for another thread to perform wait()
/// on it.
/// </summary>
void Thread::detach()
{
    if (!m_started)
        return;
    ::pthread_detach(m_thread);
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
/// Internal helper thats used as the entry point for the thread.
/// </summary>
/// <param name="arg"></param>
/// <returns></returns>
void* Thread::helper(void* arg)
{
    Thread* p = (Thread*)arg;
    p->entry();

    return nullptr;
}
