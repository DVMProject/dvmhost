// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup fne Fixed Network Equipment (dvmfne)
 * @brief Digital Voice Modem - Converged FNE Software
 * @details Network "core", this provides a central server for `dvmhost` instances to connect to and be networked with, allowing relay of traffic and other data between `dvmhost` instances and other `dvmfne` instances.
 * @ingroup fne
 * 
 * @file Defines.h
 * @ingroup fne
 */
#if !defined(__DEFINES_H__)
#define __DEFINES_H__

#include "common/Defines.h"

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#undef __PROG_NAME__
#define __PROG_NAME__ "Digital Voice Modem (DVM) Converged FNE"
#undef __EXE_NAME__ 
#define __EXE_NAME__ "dvmfne"

#undef __NETVER__
#define __NETVER__ "FNE_R" VERSION_MAJOR VERSION_REV VERSION_MINOR

#undef DEFAULT_CONF_FILE
#define DEFAULT_CONF_FILE "fne-config.yml"
#undef DEFAULT_LOCK_FILE
#define DEFAULT_LOCK_FILE "/tmp/dvmfne.lock"

#endif // __DEFINES_H__
