// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - TG Patch
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup patch TG Patch
 * @brief Digital Voice Modem - TG Patch
 * @details Talkgroup patching utility, this provides facilities to patch two talkgroups together.
 * @ingroup patch
 * 
 * @file Defines.h
 * @ingroup patch
 */
#if !defined(__DEFINES_H__)
#define __DEFINES_H__

#include "common/Defines.h"

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#undef __PROG_NAME__
#define __PROG_NAME__ "Digital Voice Modem (DVM) TG Patch"
#undef __EXE_NAME__ 
#define __EXE_NAME__ "patch"

#undef __NETVER__
#define __NETVER__ "PATCH_R" VERSION_MAJOR VERSION_REV VERSION_MINOR

#undef DEFAULT_CONF_FILE
#define DEFAULT_CONF_FILE "patch-config.yml"
#undef DEFAULT_LOCK_FILE
#define DEFAULT_LOCK_FILE "/tmp/dvmpatch.lock"

#endif // __DEFINES_H__
