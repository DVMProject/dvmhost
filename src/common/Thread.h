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
#if !defined(__THREAD_H__)
#define __THREAD_H__

#include "common/Defines.h"

#include <string>

#include <pthread.h>

// ---------------------------------------------------------------------------
//  Class Declaration
//      Implements a simple threading mechanism.
// ---------------------------------------------------------------------------

class HOST_SW_API Thread {
public:
    /// <summary>Initializes a new instance of the Thread class.</summary>
    Thread();
    /// <summary>Finalizes a instance of the Thread class.</summary>
    virtual ~Thread();

    /// <summary>Starts the thread execution.</summary>
    virtual bool run();

    /// <summary>User-defined function to run for the thread main.</summary>
    virtual void entry() = 0;

    /// <summary>Make calling thread wait for termination of the thread.</summary>
    virtual void wait();

    /// <summary>Set thread name visible in the kernel and its interfaces.</summary>
    virtual void setName(std::string name);

    /// <summary>Indicate that the thread is never to be joined with wait().
    /// The resources of thread will therefore be freed immediately when it
    /// terminates, instead of waiting for another thread to perform wait()
    /// on it.</summary>
    virtual void detach();

    /// <summary></summary>
    static void sleep(uint32_t ms);

private:
    pthread_t m_thread;

    /// <summary>Internal helper thats used as the entry point for the thread.</summary>
    static void* helper(void* arg);

public:
    /// <summary>Flag indicating if the thread was started.</summary>
    __PROTECTED_READONLY_PROPERTY_PLAIN(bool, started);
};

#endif // __THREAD_H__
