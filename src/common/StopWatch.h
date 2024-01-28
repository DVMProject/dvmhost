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
*   Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
*
*/
#if !defined(__STOPWATCH_H__)
#define __STOPWATCH_H__

#include "common/Defines.h"

#include <sys/time.h>

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
    /// <summary>Gets the elapsed time since the stopwatch started.</summary>
    uint32_t elapsed();

private:
    ulong64_t m_startMS;
};

#endif // __STOPWATCH_H__
