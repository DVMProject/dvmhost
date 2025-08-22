// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2018-2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup logging Logging Routines
 * @brief Defines and implements logging routines.
 * @ingroup common
 * 
 * @file Log.h
 * @ingroup logging
 * @file Log.cpp
 * @ingroup logging
 */
#if !defined(__LOG_H__)
#define __LOG_H__

#include "common/Defines.h"

#include <string>

/**
 * @addtogroup logging
 * @{
 */

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

/** @cond */

#define LOG_HOST    "HOST"
#define LOG_REST    "RESTAPI"
#define LOG_SIP     "SIP"
#define LOG_MODEM   "MODEM"
#define LOG_RF      "RF"
#define LOG_NET     "NET"
#define LOG_P25     "P25"
#define LOG_NXDN    "NXDN"
#define LOG_DMR     "DMR"
#define LOG_CAL     "CAL"
#define LOG_SETUP   "SETUP"
#define LOG_SERIAL  "SERIAL"
#define LOG_DVMV24  "DVMV24"

/** @endcond */

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

/**
 * @brief Macro helper to create a debug log entry.
 * @param _module Name of module generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function.
 */
#define LogDebug(_module, fmt, ...)             Log(1U, _module, __FILE__, __LINE__, nullptr, fmt, ##__VA_ARGS__)
/**
 * @brief Macro helper to create a debug log entry.
 * @param _module Name of module generating log entry.
 * @param _func Name of function generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function.
 */
#define LogDebugEx(_module, _func, fmt, ...)    Log(1U, _module, __FILE__, __LINE__, _func, fmt, ##__VA_ARGS__)
/**
 * @brief Macro helper to create a message log entry.
 * @param _module Name of module generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function.
 */
#define LogMessage(_module, fmt, ...)           Log(2U, _module, nullptr, 0, nullptr, fmt, ##__VA_ARGS__)
/**
 * @brief Macro helper to create a informational log entry.
 * @param _module Name of module generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function. LogInfo() does not use a module
 * name when creating a log entry.
 */
#define LogInfo(fmt, ...)                       Log(3U, nullptr, nullptr, 0, nullptr, fmt, ##__VA_ARGS__)
/**
 * @brief Macro helper to create a informational log entry with module name.
 * @param _module Name of module generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function.
 */
#define LogInfoEx(_module, fmt, ...)            Log(3U, _module, nullptr, 0, nullptr, fmt, ##__VA_ARGS__)
/**
 * @brief Macro helper to create a warning log entry.
 * @param _module Name of module generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function.
 */
#define LogWarning(_module, fmt, ...)           Log(4U, _module, nullptr, 0, nullptr, fmt, ##__VA_ARGS__)
/**
 * @brief Macro helper to create a error log entry.
 * @param _module Name of module generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function.
 */
#define LogError(_module, fmt, ...)             Log(5U, _module, nullptr, 0, nullptr, fmt, ##__VA_ARGS__)
/**
 * @brief Macro helper to create a fatal log entry.
 * @param _module Name of module generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function.
 */
#define LogFatal(_module, fmt, ...)             Log(6U, _module, nullptr, 0, nullptr, fmt, ##__VA_ARGS__)

// ---------------------------------------------------------------------------
//  Externs
// ---------------------------------------------------------------------------

/**
 * @brief (Global) Display log level.
 */
extern uint32_t g_logDisplayLevel;
/**
 * @brief (Global) Flag for displaying timestamps on log entries (does not apply to syslog logging).
 */
extern bool g_disableTimeDisplay;
/**
 * @brief (Global) Flag indicating whether or not logging goes to the syslog.
 */
extern bool g_useSyslog;
/**
 * @brief (Global) Flag indicating whether or not network logging is disabled.
 */
extern bool g_disableNetworkLog;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------
/**
 * @brief Internal helper to set an output stream to direct logging to.
 * @param stream 
 */
extern HOST_SW_API void __InternalOutputStream(std::ostream& stream);

/**
 * @brief Helper to get the current log file level.
 * @returns uint32_t Current log file level.
 */
extern HOST_SW_API uint32_t CurrentLogFileLevel();

/**
 * @brief Helper to get the current log file path.
 * @returns std::string Current log file path.
 */
extern HOST_SW_API std::string LogGetFilePath();
/**
 * @brief Helper to get the current log file root.
 * @returns std::string Current log file root.
 */
extern HOST_SW_API std::string LogGetFileRoot();

/**
 * @brief Gets the instance of the Network class to transfer the activity log with.
 * @returns void* 
 */
extern HOST_SW_API void* LogGetNetwork();
/**
 * @brief Sets the instance of the Network class to transfer the activity log with.
 * @param network 
 */
extern HOST_SW_API void LogSetNetwork(void* network);

/**
 * @brief Initializes the diagnostics log.
 * @param filePath File path for the log file.
 * @param fileRoot Root name for log file.
 * @param fileLevel File log level.
 * @param displaylevel Display log level.
 * @param displayTimeDisplay Flag to disable the date and time stamp for the log entries.
 * @param syslog Flag indicating whether or not logs will be sent to syslog.
 * @returns 
 */
extern HOST_SW_API bool LogInitialise(const std::string& filePath, const std::string& fileRoot, uint32_t fileLevel, uint32_t displayLevel, bool disableTimeDisplay = false, bool useSyslog = false);
/**
 * @brief Finalizes the diagnostics log.
 */
extern HOST_SW_API void LogFinalise();
/**
 * @brief Writes a new entry to the diagnostics log.
 * @param level Log level for entry.
 * @param module Name of module generating log entry.
 * @param file Name of source code file generating log entry.
 * @param line Line number in source code file generating log entry.
 * @param func Name of function generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function. This shouldn't be called directly, utilize the LogXXXX macros above, instead.
 */
extern HOST_SW_API void Log(uint32_t level, const char* module, const char* file, const int lineNo, const char* func, const char* fmt, ...);

/** @} */
#endif // __LOG_H__
