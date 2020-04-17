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
#include "StopWatch.h"

#if !defined(_WIN32) || !defined(_WIN64)
#include <cstdio>
#include <ctime>
#endif

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

#if defined(_WIN32) || defined(_WIN64)
/// <summary>
/// Initializes a new instance of the StopWatch class.
/// </summary>
StopWatch::StopWatch() :
    m_frequencyS(),
    m_frequencyMS(),
    m_start()
{
    ::QueryPerformanceFrequency(&m_frequencyS);

    m_frequencyMS.QuadPart = m_frequencyS.QuadPart / 1000ULL;
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
    LARGE_INTEGER now;
    ::QueryPerformanceCounter(&now);

    return (ulong64_t)(now.QuadPart / m_frequencyMS.QuadPart);
}

/// <summary>
/// Starts the stopwatch.
/// </summary>
/// <returns></returns>
ulong64_t StopWatch::start()
{
    ::QueryPerformanceCounter(&m_start);

    return (ulong64_t)(m_start.QuadPart / m_frequencyS.QuadPart);
}

/// <summary>
/// Gets the elpased time since the stopwatch started.
/// </summary>
/// <returns></returns>
uint32_t StopWatch::elapsed()
{
    LARGE_INTEGER now;
    ::QueryPerformanceCounter(&now);

    LARGE_INTEGER temp;
    temp.QuadPart = (now.QuadPart - m_start.QuadPart) * 1000;

    return (uint32_t)(temp.QuadPart / m_frequencyS.QuadPart);
}
#else
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
/// Gets the elpased time since the stopwatch started.
/// </summary>
/// <returns></returns>
uint32_t StopWatch::elapsed()
{
    struct timespec now;
    ::clock_gettime(CLOCK_MONOTONIC, &now);

    ulong64_t nowMS = now.tv_sec * 1000ULL + now.tv_nsec / 1000000ULL;

    return nowMS - m_startMS;
}
#endif
