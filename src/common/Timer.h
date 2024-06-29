// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2009,2010,2011,2014 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2019 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file Timer.h
 * @ingroup timers
 * @file Timer.cpp
 * @ingroup timers
 */
#if !defined(__TIMER_H__)
#define __TIMER_H__

#include "common/Defines.h"

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Timer Simple timer that tracks the time and marks if an expiration period has been reached. 
 * @ingroup timers
 */
class HOST_SW_API Timer {
public:
    /**
     * @brief Initializes a new instance of the Timer class.
     */
    Timer();
    /**
     * @brief Initializes a new instance of the Timer class.
     * @param ticksPerSec Count of ticks per second.
     * @param sec Number of seconds until timer expires.
     * @param msec Number of milliseconds until timer expires.
     */
    Timer(uint32_t ticksPerSec, uint32_t secs = 0U, uint32_t msecs = 0U);
    /**
     * @brief Finalizes a instance of the Timer class.
     */
    ~Timer();

    /**
     * @brief Sets the timeout for the timer.
     * @param sec Number of seconds until timer expires.
     * @param msec Number of milliseconds until timer expires.
     */
    void setTimeout(uint32_t secs, uint32_t msecs = 0U);

    /**
     * @brief Gets the timeout for the timer.
     * @returns uint32_t Timeout for the timer.
     */
    uint32_t getTimeout() const;
    /**
     * @brief Gets the current time for the timer.
     * @returns uint32_t Current time for the timer.
     */
    uint32_t getTimer() const;

    /**
     * @brief Gets the currently remaining time for the timer.
     * @return uint32_t Amount of time remaining before the timeout.
     */
    uint32_t getRemaining() const
    {
        if (m_timeout == 0U || m_timer == 0U)
            return 0U;

        if (m_timer >= m_timeout)
            return 0U;

        return (m_timeout - m_timer) / m_ticksPerSec;
    }

    /**
     * @brief Flag indicating whether the timer is running.
     * @return bool True, if the timer is still running, otherwise false.
     */
    bool isRunning() const
    {
        return m_timer > 0U;
    }

    /**
     * @brief Flag indicating whether the timer is paused.
     * @return bool True, if the timer is paused, otherwise false.
     */
    bool isPaused() const
    {
        return m_paused;
    }

    /**
     * @brief Starts the timer.
     * @param sec Number of seconds until timer expires.
     * @param msec Number of milliseconds until timer expires.
     */
    void start(uint32_t secs, uint32_t msecs = 0U)
    {
        setTimeout(secs, msecs);

        start();
    }

    /**
     * @brief Starts the timer.
     */
    void start()
    {
        if (m_timeout > 0U)
            m_timer = 1U;
        m_paused = false;
    }

    /**
     * @brief Stops the timer.
     */
    void stop()
    {
        m_timer = 0U;
        m_paused = false;
    }

    /**
     * @brief Pauses the timer.
     */
    void pause()
    {
        m_paused = true;
    }

    /**
     * @brief Resumes the timer.
     */
    void resume()
    {
        m_paused = false;
    }

    /**
     * @brief Flag indicating whether or not the timer has reached timeout and expired.
     * @return bool True, if the timer is expired, otherwise false.
     */
    bool hasExpired() const
    {
        if (m_timeout == 0U || m_timer == 0U)
            return false;

        if (m_timer >= m_timeout)
            return true;

        return false;
    }

    /**
     * @brief Updates the timer by the passed number of ticks.
     * @param ticks Number of passed ticks.
     */
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
