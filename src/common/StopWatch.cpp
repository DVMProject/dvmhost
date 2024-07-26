// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "StopWatch.h"

#include <cstdio>
#include <ctime>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the StopWatch class. */

StopWatch::StopWatch() :
#if defined(_WIN32)
    m_frequencyS(),
    m_frequencyMS(),
    m_start()
#else
    m_startMS(0ULL)
#endif // defined(_WIN32)
{
#if defined(_WIN32)
    ::QueryPerformanceFrequency(&m_frequencyS);
    m_frequencyMS.QuadPart = m_frequencyS.QuadPart / 1000ULL;
#endif // defined(_WIN32)
}

/* Finalizes a instance of the StopWatch class. */

StopWatch::~StopWatch()
{
    /* stub */
}

/* Gets the current running time. */

ulong64_t StopWatch::time() const
{
#if defined(_WIN32)
    LARGE_INTEGER now;
    ::QueryPerformanceCounter(&now);

    return (ulong64_t)(now.QuadPart / m_frequencyMS.QuadPart);
#else
    struct timeval now;
    ::gettimeofday(&now, NULL);

    return now.tv_sec * 1000ULL + now.tv_usec / 1000ULL;
#endif // defined(_WIN32)
}

/* Starts the stopwatch. */

ulong64_t StopWatch::start()
{
#if defined(_WIN32)
    ::QueryPerformanceCounter(&m_start);

    return (ulong64_t)(m_start.QuadPart / m_frequencyS.QuadPart);
#else
    struct timespec now;
    ::clock_gettime(CLOCK_MONOTONIC, &now);

    m_startMS = now.tv_sec * 1000ULL + now.tv_nsec / 1000000ULL;

    return m_startMS;
#endif // defined(_WIN32)
}

/* Gets the elapsed time since the stopwatch started. */

uint32_t StopWatch::elapsed()
{
#if defined(_WIN32)
    LARGE_INTEGER now;
    ::QueryPerformanceCounter(&now);

    LARGE_INTEGER temp;
    temp.QuadPart = (now.QuadPart - m_start.QuadPart) * 1000;

    return (uint32_t)(temp.QuadPart / m_frequencyS.QuadPart);
#else
    struct timespec now;
    ::clock_gettime(CLOCK_MONOTONIC, &now);

    ulong64_t nowMS = now.tv_sec * 1000ULL + now.tv_nsec / 1000000ULL;

    return nowMS - m_startMS;
#endif // defined(_WIN32)
}
