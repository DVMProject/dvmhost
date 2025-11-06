// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file GitHash.h
 * @ingroup common
 */
#pragma once
#if !defined(__GIT_HASH_H__)
#define __GIT_HASH_H__

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#ifndef __GIT_VER__
#define __GIT_VER__ "00000000"
#endif
#ifndef __GIT_VER_HASH__
#define __GIT_VER_HASH__ "00000000"
#endif

#ifdef __VER__
#undef __VER__
#define __VER__ VERSION_MAJOR "." VERSION_MINOR VERSION_REV " (R" VERSION_MAJOR VERSION_REV VERSION_MINOR " " __GIT_VER__ ")"
#endif

/** @} */

#endif // __GIT_HASH_H__
