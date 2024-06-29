// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup system_clock Clock Routines
 * @brief Defines and implements common high-resolution system clock routines.
 * @ingroup common
 * 
 * @file Clock.h
 * @ingroup system_clock
 * @file Clock.cpp
 * @ingroup system_clock
 */
#if !defined(__CLOCK_H__)
#define __CLOCK_H__

#include "common/Defines.h"

#include <sys/time.h>
#include <chrono>

namespace system_clock 
{
    /* 
    ** Network Time Protocol
    */

    namespace ntp 
    {
        /**
         * @brief Get current time in NTP units.
         * @ingroup system_clock
         * @returns uint64_t Current time in NTP units.
         */
        uint64_t now();
        /**
         * @brief Calculate the time difference of two NTP times.
         * @ingroup system_clock
         * @param ntp1 Time in NTP units to compare.
         * @param ntp2 Time in NTP units to compare.
         * @returns uint64_t Difference in NTP units between ntp1 and ntp2.
         */
        uint64_t diff(uint64_t ntp1, uint64_t ntp2);
        /**
         * @brief Calculate the time difference the given NTP time and now.
         * @ingroup system_clock
         * @param then Time in NTP units to compare.
         * @returns uint64_t Difference in NTP units between now and then.
         */
        uint64_t diffNow(uint64_t then);
    } // namespace ntp

    /*
    ** High-Resolution Clock 
    */

    namespace hrc 
    {
        typedef std::chrono::high_resolution_clock::time_point hrc_t;

        /**
         * @brief Get current time in HRC units.
         * @ingroup system_clock
         * @returns hrc_t Current time in HRC units.
         */
        hrc_t now();
        /**
         * @brief Calculate the time difference of two HRC times.
         * @ingroup system_clock
         * @param hrc1 Time in HRC units to compare.
         * @param hrc2 Time in HRC units to compare.
         * @returns uint64_t Difference in HRC units between hrc1 and hrc2.
         */
        uint64_t diff(hrc_t hrc1, hrc_t hrc2);
        /**
         * @brief Calculate the time difference the given HRC time and now.
         * @ingroup system_clock
         * @param then Time in HRC units to compare.
         * @returns uint64_t Difference in HRC units between now and then.
         */
        uint64_t diffNow(hrc_t then);
        /**
         * @brief Calculate the time difference the given HRC time and now in microseconds.
         * @ingroup system_clock
         * @param then Time in HRC units to compare.
         * @returns uint64_t Difference in HRC units between now and then.
         */
        uint64_t diffNowUS(hrc_t& then);
    } // namespace hrc

    /**
     * @brief Convert milliseconds to jiffies.
     * @ingroup system_clock
     * @param ms Milliseconds.
     * @returns uint64_t Milliseconds in jiffies.
     */
    uint64_t msToJiffies(uint64_t ms);
    /**
     * @brief Convert jiffies to milliseconds.
     * @ingroup system_clock
     * @param jiffies Jiffies.
     * @returns uint64_t Jiffes in miilliseconds.
     */
    uint64_t jiffiesToMs(uint64_t jiffies);
} // namespace system_clock

#endif // __CLOCK_H__