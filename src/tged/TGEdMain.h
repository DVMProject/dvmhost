// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Talkgroup Editor
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file TGEdMain.h
 * @ingroup tged
 * @file TGEdMain.cpp
 * @ingroup tged
 */
#if !defined(__TGED_MAIN_H__)
#define __TGED_MAIN_H__

#include "Defines.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "common/yaml/Yaml.h"

#include <string>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#undef __PROG_NAME__
#define __PROG_NAME__ "Digital Voice Modem (DVM) Talkgroup Rules Editor"
#undef __EXE_NAME__ 
#define __EXE_NAME__ "tged"

// ---------------------------------------------------------------------------
//  Externs
// ---------------------------------------------------------------------------

/**
 * bryanb: This is some low-down, dirty, C++ hack-o-ramma.
 */

/**
 * @brief Implements RTTI type defining.
 * @typedef Tag 
 */
template<typename Tag>
struct RTTIResult {
    typedef typename Tag::type type;
    static type ptr;
};

template<typename Tag>
typename RTTIResult<Tag>::type RTTIResult<Tag>::ptr;

/**
 * @brief Implements nasty hack to access private members of a class.
 * @typedef Tag 
 * @typedef TypePtr 
 */
template<typename Tag, typename Tag::type TypePtr>
struct HackTheGibson : RTTIResult<Tag> {
    /* fill it ... */
    struct filler {
        filler() { RTTIResult<Tag>::ptr = TypePtr; }
    };
    static filler fillerObj;
};

template<typename Tag, typename Tag::type TypePtr>
typename HackTheGibson<Tag, TypePtr>::filler HackTheGibson<Tag, TypePtr>::fillerObj;

// ---------------------------------------------------------------------------
//  Externs
// ---------------------------------------------------------------------------

/** @brief  */
extern std::string g_progExe;
/** @brief  */
extern std::string g_iniFile;
/** @brief  */
extern yaml::Node g_conf;
/** @brief  */
extern bool g_debug;

/** @brief  */
extern bool g_hideLoggingWnd;

/** @brief  */
extern lookups::TalkgroupRulesLookup* g_tidLookups;

/**
 * @brief Helper to trigger a fatal error message. This will cause the program to terminate 
 * immediately with an error message.
 * 
 * @param msg String format.
 * 
 * This is a variable argument function.
 */
extern HOST_SW_API void fatal(const char* msg, ...);

#endif // __TGED_MAIN_H__
