// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / DFSI peer application
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Patrick McDonnell, W3AXL
*
*/
#if !defined(__DFSI_MAIN_H__)
#define __DFSI_MAIN_H__

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

extern std::string g_masterAddress;
extern uint16_t g_masterPort;
extern uint32_t g_peerId;

extern uint8_t* g_gitHashBytes;

extern HOST_SW_API void fatal(const char* msg, ...);

#endif // __DFSI_MAIN_H__