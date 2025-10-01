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
#if defined(_WIN32)
#include "common/Clock.h"
#else
#include <sys/time.h>
#endif // defined(_WIN32)

#include <ctime>
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
#define LogDebug(_module, fmt, ...)             Log(1U, {_module, __FILE__, __LINE__, nullptr}, fmt, ##__VA_ARGS__)
/**
 * @brief Macro helper to create a debug log entry.
 * @param _module Name of module generating log entry.
 * @param _func Name of function generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function.
 */
#define LogDebugEx(_module, _func, fmt, ...)    Log(1U, {_module, __FILE__, __LINE__, _func}, fmt, ##__VA_ARGS__)
/**
 * @brief Macro helper to create a message log entry.
 * @param _module Name of module generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function.
 */
#define LogMessage(_module, fmt, ...)           Log(2U, {_module, nullptr, 0, nullptr}, fmt, ##__VA_ARGS__)
/**
 * @brief Macro helper to create a informational log entry.
 * @param _module Name of module generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function. LogInfo() does not use a module
 * name when creating a log entry.
 */
#define LogInfo(fmt, ...)                       Log(3U, {nullptr, nullptr, 0, nullptr}, fmt, ##__VA_ARGS__)
/**
 * @brief Macro helper to create a informational log entry with module name.
 * @param _module Name of module generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function.
 */
#define LogInfoEx(_module, fmt, ...)            Log(3U, {_module, nullptr, 0, nullptr}, fmt, ##__VA_ARGS__)
/**
 * @brief Macro helper to create a warning log entry.
 * @param _module Name of module generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function.
 */
#define LogWarning(_module, fmt, ...)           Log(4U, {_module, nullptr, 0, nullptr}, fmt, ##__VA_ARGS__)
/**
 * @brief Macro helper to create a error log entry.
 * @param _module Name of module generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function.
 */
#define LogError(_module, fmt, ...)             Log(5U, {_module, nullptr, 0, nullptr}, fmt, ##__VA_ARGS__)
/**
 * @brief Macro helper to create a fatal log entry.
 * @param _module Name of module generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function.
 */
#define LogFatal(_module, fmt, ...)             Log(6U, {_module, nullptr, 0, nullptr}, fmt, ##__VA_ARGS__)

namespace log_internal
{
    constexpr static char LOG_LEVELS[] = " DMIWEF";

    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents a source code location.
     * @ingroup logger
     */
    struct SourceLocation {
    public:
        /**
         * @brief Initializes a new instance of the SourceLocation class.
         */
        constexpr SourceLocation() = default;
        /**
         * @brief Initializes a new instance of the SourceLocation class.
         * @param module Application module.
         * @param filename Source code filename.
         * @param line Line number in source code file.
         * @param func Function name within source code.
         */
        constexpr SourceLocation(const char* module, const char* filename, int line, const char* func) :
            module(module),
            filename(filename),
            line(line),
            funcname(func)
        {
            /* stub */
        }

    public:
        const char* module = nullptr;
        const char* filename = nullptr;
        int line = 0;
        const char* funcname = nullptr;
    };

    /**
     * @brief Internal helper to set an output stream to direct logging to.
     * @param stream 
     */
    extern HOST_SW_API void SetInternalOutputStream(std::ostream& stream);
    /**
     * @brief Writes a new entry to the diagnostics log.
     * @param level Log level for entry.
     * @param log Fully formatted log message.
     */
    extern HOST_SW_API void LogInternal(uint32_t level, const std::string& log);
} // namespace log_internal

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
extern HOST_SW_API bool LogInitialise(const std::string& filePath, const std::string& fileRoot, 
    uint32_t fileLevel, uint32_t displayLevel, bool disableTimeDisplay = false, bool useSyslog = false);
/**
 * @brief Finalizes the diagnostics log.
 */
extern HOST_SW_API void LogFinalise();

/**
 * @brief Writes a new entry to the diagnostics log.
 * @param level Log level for entry.
 * @param sourceLog Source code location information.
 * @param fmt String format.
 * 
 * This is a variable argument function. This shouldn't be called directly, utilize the LogXXXX macros above, instead.
 */
template<typename ... Args>
HOST_SW_API void Log(uint32_t level, log_internal::SourceLocation sourceLoc, const std::string& fmt, Args... args)
{
    using namespace log_internal;

    int size_s = std::snprintf(nullptr, 0, fmt.c_str(), args...) + 1; // Extra space for '\0'
    if (size_s <= 0) {
        throw std::runtime_error("Error during formatting.");
    }

#if defined(CATCH2_TEST_COMPILATION)
    g_disableTimeDisplay = true;
#endif
    int prefixLen = 0;
    char prefixBuf[256];

    if (!g_disableTimeDisplay && !g_useSyslog) {
        time_t now;
        ::time(&now);
        struct tm* tm = ::localtime(&now);

        struct timeval nowMillis;
        ::gettimeofday(&nowMillis, NULL);

        if (level > 7U)
            level = 3U; // default this sort of log message to INFO

        if (sourceLoc.module != nullptr) {
            // level 1 is DEBUG
            if (level == 1U) {
                // if we have a file and line number -- add that to the log entry
                if (sourceLoc.filename != nullptr && sourceLoc.line > 0) {
                    // if we have a function name add that to the log entry
                    if (sourceLoc.funcname != nullptr) {
                        prefixLen = ::sprintf(prefixBuf, "%c: %04d-%02d-%02d %02d:%02d:%02d.%03lu (%s)[%s:%u][%s] ", LOG_LEVELS[level], 
                            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, nowMillis.tv_usec / 1000U, 
                            sourceLoc.module, sourceLoc.filename, sourceLoc.line, sourceLoc.funcname);
                    }
                    else {
                        prefixLen = ::sprintf(prefixBuf, "%c: %04d-%02d-%02d %02d:%02d:%02d.%03lu (%s)[%s:%u] ", LOG_LEVELS[level], 
                            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, nowMillis.tv_usec / 1000U, 
                            sourceLoc.module, sourceLoc.filename, sourceLoc.line);
                    }
                } else {
                    prefixLen = ::sprintf(prefixBuf, "%c: %04d-%02d-%02d %02d:%02d:%02d.%03lu (%s) ", LOG_LEVELS[level], 
                        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, nowMillis.tv_usec / 1000U, 
                        sourceLoc.module);
                }
            } else {
                prefixLen = ::sprintf(prefixBuf, "%c: %04d-%02d-%02d %02d:%02d:%02d.%03lu (%s) ", LOG_LEVELS[level], 
                    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, nowMillis.tv_usec / 1000U, 
                    sourceLoc.module);
            }
        }
        else {
            // level 1 is DEBUG
            if (level == 1U) {
                // if we have a file and line number -- add that to the log entry
                if (sourceLoc.filename != nullptr && sourceLoc.line > 0) {
                    // if we have a function name add that to the log entry
                    if (sourceLoc.funcname != nullptr) {
                        prefixLen = ::sprintf(prefixBuf, "%c: %04d-%02d-%02d %02d:%02d:%02d.%03lu [%s:%u][%s] ", LOG_LEVELS[level], 
                            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, nowMillis.tv_usec / 1000U, 
                            sourceLoc.filename, sourceLoc.line, sourceLoc.funcname);
                    }
                    else {
                        prefixLen = ::sprintf(prefixBuf, "%c: %04d-%02d-%02d %02d:%02d:%02d.%03lu [%s:%u] ", LOG_LEVELS[level], 
                            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, nowMillis.tv_usec / 1000U, 
                            sourceLoc.filename, sourceLoc.line);
                    }
                } else {
                    prefixLen = ::sprintf(prefixBuf, "%c: %04d-%02d-%02d %02d:%02d:%02d.%03lu ", LOG_LEVELS[level], tm->tm_year + 1900, 
                        tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, nowMillis.tv_usec / 1000U);
                }
            } else {
                prefixLen = ::sprintf(prefixBuf, "%c: %04d-%02d-%02d %02d:%02d:%02d.%03lu ", LOG_LEVELS[level], 
                    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, nowMillis.tv_usec / 1000U);
            }
        }
    }
    else {
        if (sourceLoc.module != nullptr) {
            if (level > 7U)
                level = 3U; // default this sort of log message to INFO

            // level 1 is DEBUG
            if (level == 1U) {
                // if we have a file and line number -- add that to the log entry
                if (sourceLoc.filename != nullptr && sourceLoc.line > 0) {
                    // if we have a function name add that to the log entry
                    if (sourceLoc.funcname != nullptr) {
                        prefixLen = ::sprintf(prefixBuf, "%c: (%s)[%s:%u][%s] ", LOG_LEVELS[level], 
                            sourceLoc.module, sourceLoc.filename, sourceLoc.line, sourceLoc.funcname);
                    }
                    else {
                        prefixLen = ::sprintf(prefixBuf, "%c: (%s)[%s:%u] ", LOG_LEVELS[level], 
                            sourceLoc.module, sourceLoc.filename, sourceLoc.line);
                    }
                }
                else {
                    prefixLen = ::sprintf(prefixBuf, "%c: (%s) ", LOG_LEVELS[level], 
                        sourceLoc.module);
                }
            } else {
                prefixLen = ::sprintf(prefixBuf, "%c: (%s) ", LOG_LEVELS[level], 
                    sourceLoc.module);
            }
        }
        else {
            if (level >= 9999U) {
                prefixLen = ::sprintf(prefixBuf, "U: ");
            }
            else {
                if (level > 7U)
                    level = 3U; // default this sort of log message to INFO

                 // if we have a file and line number -- add that to the log entry
                 if (sourceLoc.filename != nullptr && sourceLoc.line > 0) {
                    // if we have a function name add that to the log entry
                    if (sourceLoc.funcname != nullptr) {
                        prefixLen = ::sprintf(prefixBuf, "%c: [%s:%u][%s] ", LOG_LEVELS[level], 
                            sourceLoc.filename, sourceLoc.line, sourceLoc.funcname);
                    }
                    else {
                        prefixLen = ::sprintf(prefixBuf, "%c: [%s:%u] ", LOG_LEVELS[level], 
                            sourceLoc.filename, sourceLoc.line);
                    }
                }
                else {
                    prefixLen = ::sprintf(prefixBuf, "%c: ", LOG_LEVELS[level]);
                }
            }
        }
    }

    auto size = static_cast<size_t>(size_s);
    auto buf = std::make_unique<char[]>(size);

    std::snprintf(buf.get(), size, fmt.c_str(), args ...);

    std::string prefix = std::string(prefixBuf, prefixBuf + prefixLen);
    std::string msg = std::string(buf.get(), buf.get() + size - 1);

    LogInternal(level, std::string(prefix + msg));
}

/** @} */
#endif // __LOG_H__
