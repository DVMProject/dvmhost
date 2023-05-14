/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2023 Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; version 2 of the License.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*/
#include "Defines.h"
#include "Clock.h"

using namespace system_clock;

#include <stdio.h>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

static const uint64_t EPOCH = 2208988800ULL;
static const uint64_t NTP_SCALE_FRAC = 4294967296ULL;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/// <summary>
///
/// </summary>
/// <param name="older"></param>
/// <param name="newer"></param>
/// <returns></returns>
static inline uint32_t ntpDiffMS(uint64_t older, uint64_t newer)
{
    if (older > newer) {
        // LOG_ERROR("Older timestamp is actually newer");
    }

    uint32_t s1  = (older >> 32) & 0xffffffff;
    uint32_t s2  = (newer >> 32) & 0xffffffff;
    uint64_t us1 = ((older & 0xffffffff) * 1000000UL) / NTP_SCALE_FRAC;
    uint64_t us2 = ((newer & 0xffffffff) * 1000000UL) / NTP_SCALE_FRAC;

    uint64_t r = (((uint64_t)(s2 - s1) * 1000000) + ((us2 - us1))) / 1000;

    if (r > UINT32_MAX) {
        // LOG_ERROR("NTP difference is too large: %llu. Limiting value", r);
        r = UINT32_MAX;
    }

    return (uint32_t)r;
}

/// <summary>
/// Get current time in NTP units.
/// </summary>
/// <returns>NTP timestamp.</returns>
uint64_t ntp::now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    uint64_t tv_ntp = tv.tv_sec + EPOCH;
    uint64_t tv_usecs = (uint64_t)((float)(NTP_SCALE_FRAC * tv.tv_usec) / 1000000.f);

    return (tv_ntp << 32) | tv_usecs;
}

/// <summary>
/// Calculate the time difference of two NTP times.
/// </summary>
/// <param name="ntp1">First NTP timestamp</param>
/// <param name="ntp2">Second NTP timestamp</param>
/// <returns>Difference of the timestamps in milliseconds.</returns>
uint64_t ntp::diff(uint64_t ntp1, uint64_t ntp2)
{
    return ntpDiffMS(ntp1, ntp2);
}

/// <summary>
/// Calculate the time difference of two NTP times.
///
/// This function calls clock::ntp::now() and then subtracts the input 
/// parameter from that timestamp value.
/// </summary>
/// <param name="then">NTP timestamp</param>
/// <returns>Difference of the timestamps in milliseconds.</returns>
uint64_t ntp::diffNow(uint64_t then)
{
    uint64_t now = ntp::now();
    return ntpDiffMS(then, now);
}

/// <summary>
/// Get current time in HRC units.
/// </summary>
/// <returns>NTP timestamp.</returns>
hrc::hrc_t hrc::now()
{
    return std::chrono::high_resolution_clock::now();
}

/// <summary>
/// Calculate the time difference of two HRC times.
/// </summary>
/// <param name="hrc1">First HRC timestamp</param>
/// <param name="hrc2">Second HRC timestamp</param>
/// <returns>Difference of the timestamps in milliseconds.</returns>
uint64_t hrc::diff(hrc::hrc_t hrc1, hrc::hrc_t hrc2)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(hrc1 - hrc2).count();
}

/// <summary>
/// Calculate the time difference of two HRC times.
///
/// This function calls clock::hrc::now() and then subtracts the input 
/// parameter from that timestamp value.
/// </summary>
/// <param name="then">HRC timestamp</param>
/// <returns>Difference of the timestamps in milliseconds.</returns>
uint64_t hrc::diffNow(hrc::hrc_t then)
{
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - then).count();
}

/// <summary>
/// Calculate the time difference of two HRC times.
///
/// This function calls clock::hrc::now() and then subtracts the input 
/// parameter from that timestamp value.
/// </summary>
/// <param name="then">HRC timestamp</param>
/// <returns>Difference of the timestamps in microseconds.</returns>
uint64_t hrc::diffNowUS(hrc::hrc_t& then)
{
    return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - then).count();
}

/// <summary>
///
/// </summary>
/// <param name="ms"></param>
/// <returns></returns>
uint64_t msToJiffies(uint64_t ms)
{
    return (uint64_t)(((double)ms/1000)* 65536);
}

/// <summary>
///
/// </summary>
/// <param name="ms"></param>
/// <returns></returns>
uint64_t jiffiesToMs(uint64_t jiffies)
{
    return (uint64_t)(((double)jiffies / 65536) * 1000);
}
