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
#if !defined(__STOPWATCH_H__)
#define __STOPWATCH_H__

#include "Defines.h"

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/time.h>
#endif

// ---------------------------------------------------------------------------
//  Class Declaration
//      Implements a stopwatch.
// ---------------------------------------------------------------------------

class HOST_SW_API StopWatch {
public:
    /// <summary>Initializes a new instance of the StopWatch class.</summary>
    StopWatch();
    /// <summary>Finalizes a instance of the StopWatch class.</summary>
    ~StopWatch();

    /// <summary>Gets the current running time.</summary>
    ulong64_t time() const;

    /// <summary>Starts the stopwatch.</summary>
    ulong64_t start();
    /// <summary>Gets the elpased time since the stopwatch started.</summary>
    uint32_t elapsed();

private:
#if defined(_WIN32) || defined(_WIN64)
    LARGE_INTEGER m_frequencyS;
    LARGE_INTEGER m_frequencyMS;
    LARGE_INTEGER m_start;
#else
    ulong64_t m_startMS;
#endif
};

#endif // __STOPWATCH_H__
