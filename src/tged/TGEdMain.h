// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Talkgroup Editor
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file TGEdMain.h
 * @ingroup tged
 * @file TGEdMain.cpp
 * @ingroup tged
 */
#if !defined(__TGED_MAIN_H__)
#define __TGED_MAIN_H__

#include "Defines.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "common/yaml/Yaml.h"

#include <string>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#undef __PROG_NAME__
#define __PROG_NAME__ "Digital Voice Modem (DVM) Talkgroup Rules Editor"
#undef __EXE_NAME__ 
#define __EXE_NAME__ "tged"

// ---------------------------------------------------------------------------
//  Externs
// ---------------------------------------------------------------------------

/** @brief  */
extern std::string g_progExe;
/** @brief  */
extern std::string g_iniFile;
/** @brief  */
extern yaml::Node g_conf;
/** @brief  */
extern bool g_debug;

/** @brief  */
extern bool g_hideLoggingWnd;

/** @brief  */
extern lookups::TalkgroupRulesLookup* g_tidLookups;

/**
 * @brief Helper to trigger a fatal error message. This will cause the program to terminate 
 * immediately with an error message.
 * 
 * @param msg String format.
 * 
 * This is a variable argument function.
 */
extern HOST_SW_API void fatal(const char* msg, ...);

#endif // __TGED_MAIN_H__
