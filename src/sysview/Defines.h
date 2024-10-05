// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - FNE System View
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup fneSysView FNE System View (dvmmon)
 * @brief Digital Voice Modem - FNE System View
 * @details Monitor software that connects to the DVM FNE and is a quick TUI for monitoring affiliations on them.
 * @ingroup fneSysView
 * 
 * @file Defines.h
 * @ingroup fneSysView
 */
#if !defined(__DEFINES_H__)
#define __DEFINES_H__

#include "common/Defines.h"

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#undef __PROG_NAME__
#define __PROG_NAME__ "Digital Voice Modem (DVM) FNE System View"
#undef __EXE_NAME__ 
#define __EXE_NAME__ "sysview"

#undef __NETVER__
#define __NETVER__ "SYSVIEW_R" VERSION_MAJOR VERSION_REV VERSION_MINOR

#endif // __DEFINES_H__
