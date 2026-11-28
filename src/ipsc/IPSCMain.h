// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - IPSC Gateway
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file IPSCMain.h
 * @ingroup ipsc
 * @file IPSCMain.cpp
 * @ingroup ipsc
 */
#if !defined(__IPSC_MAIN_H__)
#define __IPSC_MAIN_H__

#include "Defines.h"

#include <string>

// ---------------------------------------------------------------------------
//  Externs
// ---------------------------------------------------------------------------

/** @brief Signal received by the application. */
extern int g_signal;
/** @brief Executable name used in command-line messages. */
extern std::string g_progExe;
/** @brief Full path to the active configuration file. */
extern std::string g_iniFile;
/** @brief Full path to the application lock file. */
extern std::string g_lockFile;
/** @brief Flag indicating foreground operation. */
extern bool g_foreground;
/** @brief Flag indicating that the application was asked to stop. */
extern bool g_killed;
/** @brief Binary representation of the source revision hash. */
extern uint8_t* g_gitHashBytes;

/**
 * @brief Prints a fatal error message and terminates the application.
 * @param msg Message format string.
 */
extern HOST_SW_API void fatal(const char* msg, ...);

#endif // __IPSC_MAIN_H__
