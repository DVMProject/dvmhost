// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
 *  Copyright (C) 2018-2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup common Common Library
 * @brief Digital Voice Modem - Common Library
 * @details This library implements common core code used by the majority of dvmhost projects.
 * @ingroup common
 *
 * @defgroup edac Error Detection and Correction
 * @brief Implementation for various Error Detection and Correction methods.
 * @ingroup common
 *
 * @file Defines.h
 * @ingroup common
 */
#pragma once
#if !defined(__COMMON_DEFINES_H__)
#define __COMMON_DEFINES_H__

#include <cstdint>
#include <memory>
#include <sstream>
#include <ios>
#include <algorithm>
#include <string>

// ---------------------------------------------------------------------------
//  Types
// ---------------------------------------------------------------------------

#ifndef _INT8_T_DECLARED
#ifndef __INT8_TYPE__
typedef signed char         int8_t;
#endif // __INT8_TYPE__
#endif // _INT8_T_DECLARED
#ifndef _INT16_T_DECLARED
#ifndef __INT16_TYPE__
typedef short               int16_t;
#endif // __INT16_TYPE__
#endif // _INT16_T_DECLARED
#ifndef _INT32_T_DECLARED
#ifndef __INT32_TYPE__
typedef int                 int32_t;
#endif // __INT32_TYPE__
#endif // _INT32_T_DECLARED
#ifndef _INT64_T_DECLARED
#ifndef __INT64_TYPE__
typedef long long           int64_t;
#endif // __INT64_TYPE__
#endif // _INT64_T_DECLARED
#ifndef _UINT8_T_DECLARED
#ifndef __UINT8_TYPE__
typedef unsigned char       uint8_t;
#endif // __UINT8_TYPE__
#endif // _UINT8_T_DECLARED
#ifndef _UINT16_T_DECLARED
#ifndef __UINT16_TYPE__
typedef unsigned short      uint16_t;
#endif // __UINT16_TYPE__
#endif // _UINT16_T_DECLARED
#ifndef _UINT32_T_DECLARED
#ifndef __UINT32_TYPE__
typedef unsigned int        uint32_t;
#endif // __UINT32_TYPE__
#endif // _UINT32_T_DECLARED
#ifndef _UINT64_T_DECLARED
#ifndef __UINT64_TYPE__
typedef unsigned long long  uint64_t;
#endif // __UINT64_TYPE__
#endif // _UINT64_T_DECLARED

#ifndef __LONG64_TYPE__
typedef long long           long64_t;
#endif // __LONG64_TYPE__
#ifndef __ULONG64_TYPE__
typedef unsigned long long  ulong64_t;
#endif // __ULONG64_TYPE__

#if defined(__GNUC__) || defined(__GNUG__)
#define __forceinline __attribute__((always_inline))
#endif

#if defined(__MINGW32__) || defined(__MINGW64__) || defined(__GNUC__) || defined(__GNUG__)
#define PACK(decl) decl __attribute__((__packed__))
#else
#define PACK(decl) __pragma(pack(push, 1)) decl __pragma(pack(pop))
#endif

// ---------------------------------------------------------------------------
//  Meta-Programming Macro Includes
// ---------------------------------------------------------------------------

#include "common/ClassProperties.h"
#include "common/BitManipulation.h"
#include "common/VariableLengthArray.h"

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#ifndef __GIT_VER__
#define __GIT_VER__ "00000000"
#endif
#ifndef __GIT_VER_HASH__
#define __GIT_VER_HASH__ "00000000"
#endif

#define __PROG_NAME__ ""
#define __EXE_NAME__ ""

#define VERSION_MAJOR       "04"
#define VERSION_MINOR       "32"
#define VERSION_REV         "J"

#define __NETVER__ "DVM_R" VERSION_MAJOR VERSION_REV VERSION_MINOR
#define __VER__ VERSION_MAJOR "." VERSION_MINOR VERSION_REV " (R" VERSION_MAJOR VERSION_REV VERSION_MINOR " " __GIT_VER__ ")"

#define __BUILD__ __DATE__ " " __TIME__
#if !defined(NDEBUG)
#undef __BUILD__
#define __BUILD__ __DATE__ " " __TIME__ " DEBUG_FACTORY_LABTOOL"
#endif // DEBUG

#define __BANNER__ "\r\n" \
"                                        .         .           \r\n" \
"8 888888888o.   `8.`888b           ,8' ,8.       ,8.          \r\n" \
"8 8888    `^888. `8.`888b         ,8' ,888.     ,888.         \r\n" \
"8 8888        `88.`8.`888b       ,8' .`8888.   .`8888.        \r\n" \
"8 8888         `88 `8.`888b     ,8' ,8.`8888. ,8.`8888.       \r\n" \
"8 8888          88  `8.`888b   ,8' ,8'8.`8888,8^8.`8888.      \r\n" \
"8 8888          88   `8.`888b ,8' ,8' `8.`8888' `8.`8888.     \r\n" \
"8 8888         ,88    `8.`888b8' ,8'   `8.`88'   `8.`8888.    \r\n" \
"8 8888        ,88'     `8.`888' ,8'     `8.`'     `8.`8888.   \r\n" \
"8 8888    ,o88P'        `8.`8' ,8'       `8        `8.`8888.  \r\n" \
"8 888888888P'            `8.` ,8'         `         `8.`8888. \r\n"

#define HOST_SW_API

/**
 * @addtogroup common
 * @{
 */

#define DEFAULT_CONF_FILE "config.yml"
#define DEFAULT_LOCK_FILE "/tmp/dvm.lock"

#define NULL_PORT       "null"
#define UART_PORT       "uart"
#define PTY_PORT        "pty"

#define MODEM_MODE_AIR  "air"
#define MODEM_MODE_DFSI "dfsi"

const uint32_t  REMOTE_MODEM_PORT = 3334;
const uint32_t  TRAFFIC_DEFAULT_PORT = 62031;
const uint32_t  REST_API_DEFAULT_PORT = 9990;
const uint32_t  RPC_DEFAULT_PORT = 9890;

/**
 * @brief Operational Host States
 */
enum HOST_STATE {
    FNE_STATE = 240U,                   //! FNE (only used by dvmfne)

    HOST_STATE_LOCKOUT = 250U,          //! Lockout (dvmhost traffic lockout state)
    HOST_STATE_ERROR = 254U,            //! Error (dvmhost error state)
    HOST_STATE_QUIT = 255U,             //! Quit (dvmhost quit state)
};

/**
 * @brief Operational RF States
 */
enum RPT_RF_STATE {
    RS_RF_LISTENING,                    //! Modem Listening
    RS_RF_LATE_ENTRY,                   //! Traffic Late Entry
    RS_RF_AUDIO,                        //! Audio
    RS_RF_DATA,                         //! Data
    RS_RF_REJECTED,                     //! Traffic Rejected
    RS_RF_INVALID                       //! Traffic Invalid
};

/**
 * @brief Operational Network States
 */
enum RPT_NET_STATE {
    RS_NET_IDLE,                        //! Idle
    RS_NET_AUDIO,                       //! Audio
    RS_NET_DATA                         //! Data
};

const uint8_t   UDP_COMPRESS_NONE = 0x00U;

const uint8_t   IP_COMPRESS_NONE = 0x00U;
const uint8_t   IP_COMPRESS_RFC1144_COMPRESS = 0x01U;
const uint8_t   IP_COMPRESS_RFC1144_UNCOMPRESS = 0x02U;

/** @} */

#endif // __COMMON_DEFINES_H__
