// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - DFSI V.24/UDP Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Patrick McDonnell, W3AXL
 *
 */
/**
 * @file DfsiMain.h
 * @ingroup dfsi
 * @file DfsiMain.cpp
 * @ingroup dfsi
 */
#if !defined(__DFSI_MAIN_H__)
#define __DFSI_MAIN_H__

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
extern std::string g_masterAddress;
/** @brief  */
extern uint16_t g_masterPort;
/** @brief  */
extern uint32_t g_peerId;

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

#endif // __DFSI_MAIN_H__