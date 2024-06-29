// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup threading Threading
 * @brief Defines and implements threading routines.
 * @ingroup common

 * @file Thread.h
 * @ingroup threading
 * @file Thread.cpp
 * @ingroup threading
 */
#if !defined(__THREAD_H__)
#define __THREAD_H__

#include "common/Defines.h"

#include <string>

#include <pthread.h>

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Creates and controls a thread.
 * @ingroup threading
 */
class HOST_SW_API Thread {
public:
    /**
     * @brief Initializes a new instance of the Thread class.
     */
    Thread();
    /**
     * @brief Finalizes a instance of the Thread class.
     */
    virtual ~Thread();

    /**
     * @brief Starts the thread execution.
     * @returns bool True, if thread started, otherwise false.
     */
    virtual bool run();

    /**
     * @brief User-defined function to run for the thread main.
     */
    virtual void entry() = 0;

    /**
     * @brief Make calling thread wait for termination of the thread.
     */
    virtual void wait();

    /**
     * @brief Set thread name visible in the kernel and its interfaces.
     * @param name Textual name for thread.
     */
    virtual void setName(std::string name);

    /**
     * @brief The resources of thread will therefore be freed immediately when it
     *  terminates, instead of waiting for another thread to perform wait()
     */
    virtual void detach();

    /**
     * @brief Helper to sleep the current thread.
     * @param ms Time in milliseconds to sleep.
     */
    static void sleep(uint32_t ms);

private:
    pthread_t m_thread;

    /**
     * @brief Internal helper thats used as the entry point for the thread.
     * @param arg 
     * @returns void* 
     */
    static void* helper(void* arg);

public:
    /**
     * @brief Flag indicating if the thread was started.
     */
    __PROTECTED_READONLY_PROPERTY_PLAIN(bool, started);
};

#endif // __THREAD_H__
