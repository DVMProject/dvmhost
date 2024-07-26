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
/**
 * @defgroup timers Timers
 * @brief Defines and implements various timing routines.
 * @ingroup common
 * 
 * @file StopWatch.h
 * @ingroup timers
 * @file StopWatch.cpp
 * @ingroup timers
 */
#if !defined(__STOPWATCH_H__)
#define __STOPWATCH_H__

#include "common/Defines.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <sys/time.h>
#endif // defined(_WIN32)

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief StopWatch Provides a set of methods and properties that you can use to accurately measure elapsed time.
 * @ingroup timers
 */
class HOST_SW_API StopWatch {
public:
    /**
     * @brief Initializes a new instance of the StopWatch class.
     */
    StopWatch();
    /**
     * @brief Finalizes a instance of the StopWatch class.
     */
    ~StopWatch();

    /**
     * @brief Gets the current running time.
     * @returns ulong64_t Current running time.
     */
    ulong64_t time() const;

    /**
     * @brief Starts the Stopwatch.
     * @returns ulong64_t Start time.
     */
    ulong64_t start();
    /**
     * @brief Gets the elapsed time since the Stopwatch started.
     * @returns uint32_t Elapsed time since Stopwatch started.
     */
    uint32_t elapsed();

private:
#if defined(_WIN32)
    LARGE_INTEGER m_frequencyS;
    LARGE_INTEGER m_frequencyMS;
    LARGE_INTEGER m_start;
#else
    ulong64_t m_startMS;
#endif // defined(_WIN32)
};

#endif // __STOPWATCH_H__
