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
#define __HOST_MAIN_H__

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

extern bool g_remoteModemMode;
extern std::string g_remoteAddress;
extern uint16_t g_remotePort;

extern bool g_fireDMRBeacon;
extern bool g_fireP25Control;
extern bool g_fireNXDNControl;

extern bool g_fireCCVCNotification;

extern uint8_t* g_gitHashBytes;

extern bool g_modemDebug;

extern HOST_SW_API void fatal(const char* msg, ...);

#endif // __HOST_MAIN_H__
