// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file ActivityLog.h
 * @ingroup fne
 * @file ActivityLog.cpp
 * @ingroup fne
 */
#if !defined(__ACTIVITY_LOG_H__)
#define __ACTIVITY_LOG_H__

#include "Defines.h"

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

    auto size = static_cast<size_t>(size_s);
    auto buf = std::make_unique<char[]>(size);

    std::snprintf(buf.get(), size, fmt.c_str(), args ...);

    std::string msg = std::string(buf.get(), buf.get() + size - 1);

    ActivityLogInternal(std::string(msg));
}

#endif // __ACTIVITY_LOG_H__
