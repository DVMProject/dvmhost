// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Converged FNE Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Converged FNE Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2020-2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__FNE_MAIN_H__)
#define __FNE_MAIN_H__

#include "Defines.h"

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

extern HOST_SW_API void fatal(const char* msg, ...);

#endif // __FNE_MAIN_H__
