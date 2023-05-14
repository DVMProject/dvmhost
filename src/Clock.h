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
#if !defined(__CLOCK_H__)
#define __CLOCK_H__

#include "Defines.h"

#include <sys/time.h>
#include <chrono>

namespace system_clock 
{
    /* 
    ** Network Time Protocol
    */
    namespace ntp 
    {
        /// <summary>Get current time in NTP units.</summary>
        uint64_t now();
        /// <summary>Calculate the time difference of two NTP times.</summary>
        uint64_t diff(uint64_t ntp1, uint64_t ntp2);
        /// <summary>Calculate the time difference of two NTP times.</summary>
        uint64_t diffNow(uint64_t then);
    } // namespace ntp

    /*
    ** High-Resolution Clock 
    */
    namespace hrc 
    {
        typedef std::chrono::high_resolution_clock::time_point hrc_t;

        /// <summary>Get current time in HRC units.</summary>
        hrc_t now();
        /// <summary>Calculate the time difference of two HRC times.</summary>
        uint64_t diff(hrc_t hrc1, hrc_t hrc2);
        /// <summary>Calculate the time difference of two HRC times.</summary>
        uint64_t diffNow(hrc_t then);
        /// <summary>Calculate the time difference of two HRC times.</summary>
        uint64_t diffNowUS(hrc_t& then);
    } // namespace hrc

    /// <summary></summary>
    uint64_t msToJiffies(uint64_t ms);
    /// <summary></summary>
    uint64_t jiffiesToMs(uint64_t jiffies);
} // namespace system_clock

#endif // __CLOCK_H__