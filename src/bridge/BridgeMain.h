// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Bridge
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 * 
 */
/**
 * @file BridgeMain.h
 * @ingroup bridge
 * @file BridgeMain.cpp
 * @ingroup bridge
 */
#if !defined(__BRIDGE_MAIN_H__)
#define __BRIDGE_MAIN_H__

#include "Defines.h"

#include <string>

// ---------------------------------------------------------------------------
//  Externs
// ---------------------------------------------------------------------------

/** @brief  */
extern int g_signal;
/** @brief  */
extern std::string g_progExe;
/** @brief  */
extern std::string g_iniFile;
/** @brief  */
extern std::string g_lockFile;

/** @brief (Global) Flag indicating foreground operation. */
extern bool g_foreground;
/** @brief (Global) Flag indicating the FNE should stop immediately. */
extern bool g_killed;
/** @brief  */
extern bool g_hideMessages;

/** @brief Audio Input Device Index. */
extern uint32_t g_inputDevice;
/** @brief Audio Output Device Index. */
extern uint32_t g_outputDevice;

extern uint8_t* g_gitHashBytes;

/**
 * @brief Helper to trigger a fatal error message. This will cause the program to terminate 
 * immediately with an error message.
 * 
 * @param msg String format.
 * 
 * This is a variable argument function.
 */
extern HOST_SW_API void fatal(const char* msg, ...);

#endif // __BRIDGE_MAIN_H__
