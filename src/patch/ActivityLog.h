// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - TG Patch
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file ActivityLog.h
 * @ingroup patch
 * @file ActivityLog.cpp
 * @ingroup patch
 */
#if !defined(__ACTIVITY_LOG_H__)
#define __ACTIVITY_LOG_H__

#include "Defines.h"

#if defined(_WIN32)
#include "common/Clock.h"
#else
#include <sys/time.h>
#endif // defined(_WIN32)

#include <string>

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

namespace log_internal
{
    /**
     * @brief Writes a new entry to the diagnostics log.
     * @param level Log level for entry.
     * @param log Fully formatted log message.
     */
    extern HOST_SW_API void ActivityLogInternal(const std::string& log);
} // namespace log_internal

/**
 * @brief Initializes the activity log.
 * @param filePath File path for the log file.
 * @param fileRoot Root name for log file.
 */
extern HOST_SW_API bool ActivityLogInitialise(const std::string& filePath, const std::string& fileRoot);
/**
 * @brief Finalizes the activity log.
 */
extern HOST_SW_API void ActivityLogFinalise();

/**
 * @brief Writes a new entry to the diagnostics log.
 * @param fmt String format.
 * 
 * This is a variable argument function. This shouldn't be called directly, utilize the LogXXXX macros above, instead.
 */
template<typename ... Args>
HOST_SW_API void ActivityLog(const std::string& fmt, Args... args)
{
    using namespace log_internal;

    int size_s = std::snprintf(nullptr, 0, fmt.c_str(), args...) + 1; // Extra space for '\0'
    if (size_s <= 0) {
        throw std::runtime_error("Error during formatting.");
    }

    int prefixLen = 0;
    char prefixBuf[256];

    time_t now;
    ::time(&now);
    struct tm* tm = ::localtime(&now);

    struct timeval nowMillis;
    ::gettimeofday(&nowMillis, NULL);

    prefixLen = ::sprintf(prefixBuf, "A: %04d-%02d-%02d %02d:%02d:%02d.%03lu ", 
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, nowMillis.tv_usec / 1000U);

    auto size = static_cast<size_t>(size_s);
    auto buf = std::make_unique<char[]>(size);

    std::snprintf(buf.get(), size, fmt.c_str(), args ...);

    std::string prefix = std::string(prefixBuf, prefixBuf + prefixLen);
    std::string msg = std::string(buf.get(), buf.get() + size - 1);

    ActivityLogInternal(std::string(prefix + msg));
}

#endif // __ACTIVITY_LOG_H__
