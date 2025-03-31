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
*  Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
*  Copyright (C) 2020,2022 Bryan Biedenkapp, N2PLL
*
*/
/**
 * @file HostMain.h
 * @ingroup host
 * @file HostMain.cpp
 * @ingroup host
 */
#if !defined(__HOST_MAIN_H__)
#define __HOST_MAIN_H__

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
/** @brief (Global) Flag indicating the host should stop immediately. */
extern bool g_killed;

/** @brief (Global) Remote Modem Mode. */
extern bool g_remoteModemMode;
/** @brief (Global) Remote Modem Address. */
extern std::string g_remoteAddress;
/** @brief (Global) Remote Modem Port. */
extern uint16_t g_remotePort;
/** @brief (Global) Local Remote Modem Port (Listening Port). */
extern uint16_t g_remoteLocalPort;

/** @brief (Global) Fire DMR beacon flag. */
extern bool g_fireDMRBeacon;
/** @brief (Global) Fire P25 control flag. */
extern bool g_fireP25Control;
/** @brief (Global) Fire NXDN control flag. */
extern bool g_fireNXDNControl;
/** @brief (Global) Fire CC/VC notification flag. */
extern bool g_fireCCVCNotification;

/** @brief  */
extern uint8_t* g_gitHashBytes;

/** @brief (Global) Flag disabling NON-AUTHORITATIVE error logging. */
extern bool g_disableNonAuthoritativeLogging;

/** @brief (Global) Modem debug flag. Forces modem debug regardless of configuration settings. */
extern bool g_modemDebug;

/**
 * @brief Helper to trigger a fatal error message. This will cause the program to terminate 
 * immediately with an error message.
 * 
 * @param msg String format.
 * 
 * This is a variable argument function.
 */
extern HOST_SW_API void fatal(const char* msg, ...);

#endif // __HOST_MAIN_H__
