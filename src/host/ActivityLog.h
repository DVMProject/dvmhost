// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file ActivityLog.h
 * @ingroup host
 * @file ActivityLog.cpp
 * @ingroup host
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
 * @param mode Activity mode.
 * @param sourceRf Flag indicating whether or not the activity entry came from RF.
 * @param msg String format.
 * 
 * This is a variable argument function.
 */
extern HOST_SW_API void ActivityLog(const char* mode, const bool sourceRf, const char* msg, ...);

#endif // __ACTIVITY_LOG_H__
