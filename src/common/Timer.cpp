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
*   Copyright (C) 2009,2010,2015 Jonathan Naylor, G4KLX
*   Copyright (C) 2017-2019 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "Timer.h"

#include <cstdio>
#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the Timer class.
/// </summary>
Timer::Timer() :
    m_ticksPerSec(1000U),
    m_timeout(0U),
    m_timer(0U),
    m_paused(false)
{
    /* stub */
}

/// <summary>
/// Initializes a new instance of the Timer class.
/// </summary>
/// <param name="ticksPerSec"></param>
/// <param name="secs"></param>
/// <param name="msecs"></param>
Timer::Timer(uint32_t ticksPerSec, uint32_t secs, uint32_t msecs) :
    m_ticksPerSec(ticksPerSec),
    m_timeout(0U),
    m_timer(0U),
    m_paused(false)
{
    assert(ticksPerSec > 0U);

    if (secs > 0U || msecs > 0U) {
        // m_timeout = ((secs * 1000U + msecs) * m_ticksPerSec) / 1000U + 1U;
        ulong64_t temp = (secs * 1000ULL + msecs) * m_ticksPerSec;
        m_timeout = (uint32_t)(temp / 1000ULL + 1ULL);
    }
}

/// <summary>
/// Finalizes a instance of the Timer class.
/// </summary>
Timer::~Timer() = default;

/// <summary>
/// Sets the timeout for the timer.
/// </summary>
/// <param name="secs"></param>
/// <param name="msecs"></param>
void Timer::setTimeout(uint32_t secs, uint32_t msecs)
{
    if (secs > 0U || msecs > 0U) {
        // m_timeout = ((secs * 1000U + msecs) * m_ticksPerSec) / 1000U + 1U;
        ulong64_t temp = (secs * 1000ULL + msecs) * m_ticksPerSec;
        m_timeout = (uint32_t)(temp / 1000ULL + 1ULL);
    }
    else {
        m_timeout = 0U;
        m_timer = 0U;
    }
}

/// <summary>
/// Gets the timeout for the timer.
/// </summary>
/// <returns></returns>
uint32_t Timer::getTimeout() const
{
    if (m_timeout == 0U)
        return 0U;

    return (m_timeout - 1U) / m_ticksPerSec;
}

/// <summary>
/// Gets the current time for the timer.
/// </summary>
/// <returns></returns>
uint32_t Timer::getTimer() const
{
    if (m_timer == 0U)
        return 0U;

    return (m_timer - 1U) / m_ticksPerSec;
}
