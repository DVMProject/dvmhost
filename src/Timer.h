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
*   Copyright (C) 2009,2010,2011,2014 by Jonathan Naylor G4KLX
*   Copyright (C) 2017-2019 by Bryan Biedenkapp N2PLL
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
#if !defined(__TIMER_H__)
#define __TIMER_H__

#include "Defines.h"

// ---------------------------------------------------------------------------
//  Class Declaration
//      Implements a timer.
// ---------------------------------------------------------------------------

class HOST_SW_API Timer {
public:
    /// <summary>Initializes a new instance of the Timer class.</summary>
    Timer();
    /// <summary>Initializes a new instance of the Timer class.</summary>
    Timer(uint32_t ticksPerSec, uint32_t secs = 0U, uint32_t msecs = 0U);
    /// <summary>Finalizes a instance of the Timer class.</summary>
    ~Timer();

    /// <summary>Sets the timeout for the timer.</summary>
    void setTimeout(uint32_t secs, uint32_t msecs = 0U);

    /// <summary>Gets the timeout for the timer.</summary>
    uint32_t getTimeout() const;
    /// <summary>Gets the current time for the timer.</summary>
    uint32_t getTimer() const;

    /// <summary>Gets the currently remaining time for the timer.</summary>
    /// <returns>Amount of time remaining before the timeout.</returns>
    uint32_t getRemaining()
    {
        if (m_timeout == 0U || m_timer == 0U)
            return 0U;

        if (m_timer >= m_timeout)
            return 0U;

        return (m_timeout - m_timer) / m_ticksPerSec;
    }

    /// <summary>Flag indicating whether the timer is running.</summary>
    /// <returns>True, if the timer is still running, otherwise false.</returns>
    bool isRunning()
    {
        return m_timer > 0U;
    }

    /// <summary>Flag indicating whether the timer is paused.</summary>
    /// <returns>True, if the timer is paused, otherwise false.</returns>
    bool isPaused()
    {
        return m_paused;
    }

    /// <summary>Starts the timer.</summary>
    /// <param name="secs"></param>
    /// <param name="msecs"></param>
    void start(uint32_t secs, uint32_t msecs = 0U)
    {
        setTimeout(secs, msecs);

        start();
    }

    /// <summary>Starts the timer.</summary>
    void start()
    {
        if (m_timeout > 0U)
            m_timer = 1U;
        m_paused = false;
    }

    /// <summary>Stops the timer.</summary>
    void stop()
    {
        m_timer = 0U;
        m_paused = false;
    }

    /// <summary>Pauses the timer.</summary>
    void pause()
    {
        m_paused = true;
    }

    /// <summary>Resumes the timer.</summary>
    void resume()
    {
        m_paused = false;
    }

    /// <summary>Flag indicating whether or not the timer has reached timeout and expired.</summary>
    /// <returns>True, if the timer is expired, otherwise false.</returns>
    bool hasExpired()
    {
        if (m_timeout == 0U || m_timer == 0U)
            return false;

        if (m_timer >= m_timeout)
            return true;

        return false;
    }

    /// <summary>Updates the timer by the passed number of ticks.</summary>
    /// <param name="ticks"></param>
    void clock(uint32_t ticks = 1U)
    {
        if (m_paused)
            return;
        if (m_timer > 0U && m_timeout > 0U)
            m_timer += ticks;
    }

private:
    uint32_t m_ticksPerSec;
    uint32_t m_timeout;
    uint32_t m_timer;

    bool m_paused;
};

#endif // __TIMER_H__
