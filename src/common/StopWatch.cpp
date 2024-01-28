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
*
*/
#include "StopWatch.h"

#include <cstdio>
#include <ctime>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the StopWatch class.
/// </summary>
StopWatch::StopWatch() :
    m_startMS(0ULL)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the StopWatch class.
/// </summary>
StopWatch::~StopWatch()
{
    /* stub */
}

/// <summary>
/// Gets the current running time.
/// </summary>
/// <returns></returns>
ulong64_t StopWatch::time() const
{
    struct timeval now;
    ::gettimeofday(&now, NULL);

    return now.tv_sec * 1000ULL + now.tv_usec / 1000ULL;
}

/// <summary>
/// Starts the stopwatch.
/// </summary>
/// <returns></returns>
ulong64_t StopWatch::start()
{
    struct timespec now;
    ::clock_gettime(CLOCK_MONOTONIC, &now);

    m_startMS = now.tv_sec * 1000ULL + now.tv_nsec / 1000000ULL;

    return m_startMS;
}

/// <summary>
/// Gets the elapsed time since the stopwatch started.
/// </summary>
/// <returns></returns>
uint32_t StopWatch::elapsed()
{
    struct timespec now;
    ::clock_gettime(CLOCK_MONOTONIC, &now);

    ulong64_t nowMS = now.tv_sec * 1000ULL + now.tv_nsec / 1000000ULL;

    return nowMS - m_startMS;
}
