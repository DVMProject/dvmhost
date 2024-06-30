// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - DFSI V.24/UDP Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Patrick McDonnell, W3AXL
 *
 */
/**
 * @defgroup dfsi DFSI V.24/UDP Software (dvmdfsi)
 * @brief Digital Voice Modem - DFSI V.24/UDP Software
 * @details TIA/V.24 standard interface application that connects to a V.24 interface board or UDP to allow for P25 DFSI communications with commercial P25 hardware.
 * @ingroup dfsi
 * 
 * @file Defines.h
 * @ingroup dfsi
 */
#if !defined(__DEFINES_H__)
#define __DEFINES_H__

#include "common/Defines.h"

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#undef __PROG_NAME__
#define __PROG_NAME__ "Digital Voice Modem (DVM) DFSI V.24/UDP Peer"
#undef __EXE_NAME__ 
#define __EXE_NAME__ "dvmdfsi"

#undef __NETVER__
#define __NETVER__ "DFSI" VERSION_MAJOR VERSION_REV VERSION_MINOR

#undef DEFAULT_CONF_FILE
#define DEFAULT_CONF_FILE "dfsi-config.yml"
#undef DEFAULT_LOCK_FILE
#define DEFAULT_LOCK_FILE "/tmp/dvmdfsi.lock"

#define DFSI_MODE_UDP_FNE   1
#define DFSI_MODE_V24_FNE   2
#define DFSI_MODE_UDP_V24   3

#endif // __DEFINES_H__