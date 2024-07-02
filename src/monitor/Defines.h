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
 * @defgroup monitor Host Monitor Software (dvmmon)
 * @brief Digital Voice Modem - Host Monitor Software
 * @details Montior software that connects to the DVM hosts and is a quick TUI for monitoring activity on them.
 * @ingroup monitor
 * 
 * @file Defines.h
 * @ingroup monitor
 */
#if !defined(__DEFINES_H__)
#define __DEFINES_H__

#include "common/Defines.h"

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#undef __PROG_NAME__
#define __PROG_NAME__ "Digital Voice Modem (DVM) Monitor"
#undef __EXE_NAME__ 
#define __EXE_NAME__ "dvmmon"

#endif // __DEFINES_H__
