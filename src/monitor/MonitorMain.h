/**
* Digital Voice Modem - Monitor
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Monitor
*
*/
/*
*   Copyright (C) 2023 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
