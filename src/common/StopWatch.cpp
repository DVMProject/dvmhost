// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
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
    m_startMS(0ULL)
{
    /* stub */
}

/* Finalizes a instance of the StopWatch class. */
StopWatch::~StopWatch()
{
    /* stub */
}

/* Gets the current running time. */
ulong64_t StopWatch::time() const
{
    struct timeval now;
    ::gettimeofday(&now, NULL);

    return now.tv_sec * 1000ULL + now.tv_usec / 1000ULL;
}

/* Starts the stopwatch. */
ulong64_t StopWatch::start()
{
    struct timespec now;
    ::clock_gettime(CLOCK_MONOTONIC, &now);

    m_startMS = now.tv_sec * 1000ULL + now.tv_nsec / 1000000ULL;

    return m_startMS;
}

/* Gets the elapsed time since the stopwatch started. */
uint32_t StopWatch::elapsed()
{
    struct timespec now;
    ::clock_gettime(CLOCK_MONOTONIC, &now);

    ulong64_t nowMS = now.tv_sec * 1000ULL + now.tv_nsec / 1000000ULL;

    return nowMS - m_startMS;
}
