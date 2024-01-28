// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Host Monitor Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Monitor Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023 Bryan Biedenkapp, N2PLL
*
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

extern std::string g_progExe;
extern std::string g_iniFile;
extern yaml::Node g_conf;
extern bool g_debug;

extern bool g_hideLoggingWnd;

extern lookups::IdenTableLookup* g_idenTable;

#endif // __MONITOR_MAIN_H__
