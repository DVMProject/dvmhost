// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Host Monitor Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file MonitorMain.h
 * @ingroup monitor
 * @file MonitorMain.cpp
 * @ingroup monitor
 */
#if !defined(__MONITOR_MAIN_H__)
#define __MONITOR_MAIN_H__

#include "Defines.h"
#include "common/lookups/IdenTableLookup.h"
#include "common/yaml/Yaml.h"

#include <string>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#undef __PROG_NAME__
#define __PROG_NAME__ "Digital Voice Modem (DVM) Monitor Tool"
#undef __EXE_NAME__
#define __EXE_NAME__ "dvmmon"

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
extern lookups::IdenTableLookup* g_idenTable;

#endif // __MONITOR_MAIN_H__
