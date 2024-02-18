// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Modem Host Software
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
*   Copyright (C) 2020,2022 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__HOST_MAIN_H__)
#define __KMF_MAIN_H__

#include "Defines.h"
#include "common/Utils.h"
#include "common/Log.h"
#include "Kmf.h"

#include <string>

// ---------------------------------------------------------------------------
//  Externs
// ---------------------------------------------------------------------------

extern int g_signal;
extern std::string g_progExe;
extern std::string g_iniFile;
extern std::string g_lockFile;

extern bool g_foreground;
extern bool g_killed;

extern uint8_t* g_gitHashBytes;

extern bool g_debug;

extern void fatal(const char* msg, ...);

#endif // __HOST_MAIN_H__
