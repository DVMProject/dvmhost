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

#include <string>

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

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
 * @brief Writes a new entry to the activity log.
 * @param msg String format.
 * 
 * This is a variable argument function.
 */
extern HOST_SW_API void ActivityLog(const char* msg, ...);

#endif // __ACTIVITY_LOG_H__
