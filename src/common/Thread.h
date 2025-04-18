// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2023,2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup threading Threading
 * @brief Defines and implements threading routines.
 * @ingroup common
 *
 * @file Thread.h
 * @ingroup threading
 * @file Thread.cpp
 * @ingroup threading
 */
#if !defined(__THREAD_H__)
#define __THREAD_H__

#include "common/Defines.h"

#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <pthread.h>
#endif // defined(_WIN32)

#if defined(_WIN32)
 /* Thread identifiers.  The structure of the attribute type is not
    exposed on purpose.  */
typedef HANDLE pthread_t;
#endif // defined(_WIN32)

// ---------------------------------------------------------------------------
//  Structure Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Represents the data passed to a thread runner.
 * @ingroup common
 */
struct thread_t {
    void* obj;                          //! Object that created this thread.
    pthread_t thread;                   //! Thread Handle.
};

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
     * @brief Executes the specified start routine to run as a thread.
     *  On POSIX, if the thread fails to spawn for any reason, the caller should clean up the passed thread_t
     *  argument. This shouldn't be used for spawning threads used for things like packet processing.
     * @param obj Instance of a object to pass to the threaded function.
     * @param startRoutine Represents the function that executes on a thread.
     * @param[out] thread Instance of the thread data.
     * @returns bool True, if successful, otherwise error occurred.
     */
    static bool runAsThread(void* obj, void *(*startRoutine)(void *), thread_t* thread = nullptr);

    /**
     * @brief Suspends the current thread for the specified amount of time.
     * @param ms Time in milliseconds to sleep.
     * @param us Time in microseconds to sleep.
     */
    static void sleep(uint32_t ms, uint32_t us = 0U);

private:
    pthread_t m_thread;

    /**
     * @brief Internal helper thats used as the entry point for the thread.
     * @param arg 
     * @returns void* 
     */
#if defined(_WIN32)
    static DWORD __stdcall helper(LPVOID arg);
#else
    static void* helper(void* arg);
#endif // defined(_WIN32)

public:
    /**
     * @brief Flag indicating if the thread was started.
     */
    __PROTECTED_READONLY_PROPERTY_PLAIN(bool, started);
};

#endif // __THREAD_H__
