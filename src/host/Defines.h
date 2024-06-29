// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup host Modem Host Software (dvmhost)
 * @brief Digital Voice Modem - Modem Host Software
 * @details Host software that connects to the DVM modems (both repeater and hotspot) and is the primary data processing application for digital modes.
 * @ingroup host
 * 
 * @file Defines.h
 * @ingroup host
 */
#if !defined(__DEFINES_H__)
#define __DEFINES_H__

#include "common/Defines.h"

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#undef __PROG_NAME__
#define __PROG_NAME__ "Digital Voice Modem (DVM) Host"
#undef __EXE_NAME__ 
#define __EXE_NAME__ "dvmhost"

#endif // __DEFINES_H__
