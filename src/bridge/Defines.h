// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Bridge
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup bridge Bridge
 * @brief Digital Voice Modem - Bridge
 * @details Analog audio bridge, this provides facilities to convert analog audio into digital audio.
 * @ingroup bridge
 * 
 * @file Defines.h
 * @ingroup bridge
 */
#if !defined(__DEFINES_H__)
#define __DEFINES_H__

#include "common/Defines.h"

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#undef __PROG_NAME__
#define __PROG_NAME__ "Digital Voice Modem (DVM) Bridge"
#undef __EXE_NAME__ 
#define __EXE_NAME__ "bridge"

#undef __NETVER__
#define __NETVER__ "BRIDGE_R" VERSION_MAJOR VERSION_REV VERSION_MINOR

#undef DEFAULT_CONF_FILE
#define DEFAULT_CONF_FILE "bridge-config.yml"
#undef DEFAULT_LOCK_FILE
#define DEFAULT_LOCK_FILE "/tmp/dvmbridge.lock"

#endif // __DEFINES_H__
