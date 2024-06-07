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
*   Copyright (C) 2018-2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__LOG_H__)
#define __LOG_H__

#include "common/Defines.h"

#include <string>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define LOG_HOST    "HOST"
#define LOG_REST    "RESTAPI"
#define LOG_MODEM   "MODEM"
#define LOG_RF      "RF"
#define LOG_NET     "NET"
#define LOG_P25     "P25"
#define LOG_NXDN    "NXDN"
#define LOG_DMR     "DMR"
#define LOG_CAL     "CAL"
#define LOG_SETUP   "SETUP"
#define LOG_SERIAL  "SERIAL"

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

#define LogDebug(_module, fmt, ...)     Log(1U, _module, fmt, ##__VA_ARGS__)
#define LogMessage(_module, fmt, ...)   Log(2U, _module, fmt, ##__VA_ARGS__)
#define LogInfo(fmt, ...)               Log(3U, nullptr, fmt, ##__VA_ARGS__)
#define LogInfoEx(_module, fmt, ...)    Log(3U, _module, fmt, ##__VA_ARGS__)
#define LogWarning(_module, fmt, ...)   Log(4U, _module, fmt, ##__VA_ARGS__)
#define LogError(_module, fmt, ...)     Log(5U, _module, fmt, ##__VA_ARGS__)
#define LogFatal(_module, fmt, ...)     Log(6U, _module, fmt, ##__VA_ARGS__)

// ---------------------------------------------------------------------------
//  Externs
// ---------------------------------------------------------------------------

extern uint32_t g_logDisplayLevel;
extern bool g_disableTimeDisplay;

extern bool g_useSyslog;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------
/// <summary>Internal helper to set an output stream to direct logging to.</summary>
extern HOST_SW_API void __InternalOutputStream(std::ostream& stream);

/// <summary>Helper to get the current log file level.</summary>
extern HOST_SW_API uint32_t CurrentLogFileLevel();

/// <summary>Helper to get the current log file path.</summary>
extern HOST_SW_API std::string LogGetFilePath();
/// <summary>Helper to get the current log file root.</summary>
extern HOST_SW_API std::string LogGetFileRoot();

/// <summary>Gets the instance of the Network class to transfer the activity log with.</summary>
extern HOST_SW_API void* LogGetNetwork();
/// <summary>Sets the instance of the Network class to transfer the activity log with.</summary>
extern HOST_SW_API void LogSetNetwork(void* network);

/// <summary>Initializes the diagnostics log.</summary>
extern HOST_SW_API bool LogInitialise(const std::string& filePath, const std::string& fileRoot, uint32_t fileLevel, uint32_t displayLevel, bool disableTimeDisplay = false, bool useSyslog = false);
/// <summary>Finalizes the diagnostics log.</summary>
extern HOST_SW_API void LogFinalise();
/// <summary>Writes a new entry to the diagnostics log.</summary>
extern HOST_SW_API void Log(uint32_t level, const char* module, const char* fmt, ...);

#endif // __LOG_H__
