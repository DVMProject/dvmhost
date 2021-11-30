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
*   Copyright (C) 2018-2019 by Bryan Biedenkapp N2PLL
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
#if !defined(__LOG_H__)
#define __LOG_H__

#include "Defines.h"

#include <string>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------
#define LOG_HOST    "HOST"
#define LOG_RCON    "RCON"
#define LOG_MODEM   "MODEM"
#define LOG_RF      "RF"
#define LOG_NET     "NET"
#define LOG_P25     "P25"
#define LOG_DMR     "DMR"
#define LOG_CAL     "CAL"
#define LOG_SETUP   "SETUP"

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

#define LogDebug(_module, fmt, ...)     Log(1U, _module, fmt, ##__VA_ARGS__)
#define LogMessage(_module, fmt, ...)   Log(2U, _module, fmt, ##__VA_ARGS__)
#define LogInfo(fmt, ...)               Log(3U, NULL, fmt, ##__VA_ARGS__)
#define LogInfoEx(_module, fmt, ...)    Log(3U, _module, fmt, ##__VA_ARGS__)
#define LogWarning(_module, fmt, ...)   Log(4U, _module, fmt, ##__VA_ARGS__)
#define LogError(_module, fmt, ...)     Log(5U, _module, fmt, ##__VA_ARGS__)
#define LogFatal(_module, fmt, ...)     Log(6U, _module, fmt, ##__VA_ARGS__)

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------
/// <summary>Sets the instance of the Network class to transfer the activity log with.</summary>
extern HOST_SW_API void LogSetNetwork(void* network);

/// <summary>Initializes the activity log.</summary>
extern HOST_SW_API bool ActivityLogInitialise(const std::string& filePath, const std::string& fileRoot);
/// <summary>Finalizes the activity log.</summary>
extern HOST_SW_API void ActivityLogFinalise();
/// <summary>Writes a new entry to the activity log.</summary>
extern HOST_SW_API void ActivityLog(const char* mode, const bool sourceRf, const char* msg, ...);

/// <summary>Initializes the diagnostics log.</summary>
extern HOST_SW_API bool LogInitialise(const std::string& filePath, const std::string& fileRoot, uint32_t fileLevel, uint32_t displayLevel);
/// <summary>Finalizes the diagnostics log.</summary>
extern HOST_SW_API void LogFinalise();
/// <summary>Writes a new entry to the diagnostics log.</summary>
extern HOST_SW_API void Log(uint32_t level, const char* module, const char* fmt, ...);

#endif // __LOG_H__
