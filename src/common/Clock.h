// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023 Bryan Biedenkapp, N2PLL
*
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