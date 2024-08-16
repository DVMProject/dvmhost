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
#define MINIAUDIO_IMPLEMENTATION
#include "audio/miniaudio.h"

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

/** @brief Audio Input Device Index. */
extern int g_inputDevice;
/** @brief Audio Output Device Index. */
extern int g_outputDevice;

extern uint8_t* g_gitHashBytes;

#ifdef _WIN32
extern ma_backend g_backends[];
extern ma_uint32 g_backendCnt;
#else
extern ma_backend g_backends[];
extern ma_uint32 g_backendCnt;
#endif

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
