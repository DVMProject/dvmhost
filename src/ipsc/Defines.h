// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - IPSC Gateway
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup ipsc IPSC Gateway
 * @brief Digital Voice Modem - Motorola IPSC Gateway
 * @ingroup ipsc
 *
 * @file Defines.h
 * @ingroup ipsc
 */
#if !defined(__IPSC_DEFINES_H__)
#define __IPSC_DEFINES_H__

#include "common/Defines.h"
#include "common/GitHash.h"

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#undef __PROG_NAME__
#define __PROG_NAME__ "Digital Voice Modem (DVM) IPSC Gateway"
#undef __EXE_NAME__
#define __EXE_NAME__ "dvmipsc"

#undef DEFAULT_CONF_FILE
#define DEFAULT_CONF_FILE "ipsc-config.yml"
#undef DEFAULT_LOCK_FILE
#define DEFAULT_LOCK_FILE "/tmp/dvmipsc.lock"

#endif // __IPSC_DEFINES_H__
