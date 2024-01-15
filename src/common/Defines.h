/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2015,2016,2017 by Jonathan Naylor G4KLX
*   Copyright (C) 2018-2024 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#if !defined(__COMMON_DEFINES_H__)
#define __COMMON_DEFINES_H__

#include <stdint.h>

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

#define VERSION_MAJOR       "03"
#define VERSION_MINOR       "60"
#define VERSION_REV         "A"

#define __NETVER__ "DVM_R" VERSION_MAJOR VERSION_REV VERSION_MINOR
#define __VER__ VERSION_MAJOR "." VERSION_MINOR " (R" VERSION_MAJOR VERSION_REV VERSION_MINOR " " __GIT_VER__ ")"

#define __BUILD__ __DATE__ " " __TIME__

#define HOST_SW_API

#define DEFAULT_CONF_FILE "config.yml"
#define DEFAULT_LOCK_FILE "/tmp/dvm.lock"

#if defined(__GNUC__) || defined(__GNUG__)
#define __forceinline __attribute__((always_inline))
#endif

#if defined(__MINGW32__) || defined(__MINGW64__) || defined(__GNUC__) || defined(__GNUG__)
#define PACK(decl) decl __attribute__((__packed__))
#else
#define PACK(decl) __pragma(pack(push, 1)) decl __pragma(pack(pop))
#endif

#define NULL_PORT "null"
#define UART_PORT "uart"
#define PTY_PORT "pty"

const uint32_t  REMOTE_MODEM_PORT = 3334;
const uint32_t  TRAFFIC_DEFAULT_PORT = 62031;
const uint32_t  REST_API_DEFAULT_PORT = 9990;

const uint8_t   BIT_MASK_TABLE[] = { 0x80U, 0x40U, 0x20U, 0x10U, 0x08U, 0x04U, 0x02U, 0x01U };

enum HOST_STATE {
    FNE_STATE = 240U,

    HOST_STATE_LOCKOUT = 250U,
    HOST_STATE_ERROR = 254U,
    HOST_STATE_QUIT = 255U,
};

enum RPT_RF_STATE {
    RS_RF_LISTENING,
    RS_RF_LATE_ENTRY,
    RS_RF_AUDIO,
    RS_RF_DATA,
    RS_RF_REJECTED,
    RS_RF_INVALID
};

enum RPT_NET_STATE {
    RS_NET_IDLE,
    RS_NET_AUDIO,
    RS_NET_DATA
};

const uint8_t   UDP_COMPRESS_NONE = 0x00U;

const uint8_t   IP_COMPRESS_NONE = 0x00U;
const uint8_t   IP_COMPRESS_RFC1144_COMPRESS = 0x01U;
const uint8_t   IP_COMPRESS_RFC1144_UNCOMPRESS = 0x02U;

// ---------------------------------------------------------------------------
//  Class Helper Macros
// ---------------------------------------------------------------------------

/**
 * Class Copy Code Pattern
 */
/// <summary>Creates a private copy implementation.</summary>
/// <remarks>This requires the copy(const type& data) to be declared in the class definition.</remarks>
#define __COPY(type)                                                                    \
        private: virtual void copy(const type& data);                                   \
        public: __forceinline type& operator=(const type& data) {                       \
            if (this != &data) {                                                        \
                copy(data);                                                             \
            }                                                                           \
            return *this;                                                               \
        }
/// <summary>Creates a protected copy implementation.</summary>
/// <remarks>This requires the copy(const type& data) to be declared in the class definition.</remarks>
#define __PROTECTED_COPY(type)                                                          \
        protected: virtual void copy(const type& data);                                 \
        public: __forceinline type& operator=(const type& data) {                       \
            if (this != &data) {                                                        \
                copy(data);                                                             \
            }                                                                           \
            return *this;                                                               \
        }

/**
 * Property Creation
 *  These macros should always be used LAST in the "public" section of a class definition.
 */
/// <summary>Creates a read-only get property.</summary>
#define __READONLY_PROPERTY(type, variableName, propName)                               \
        private: type m_##variableName;                                                 \
        public: __forceinline type get##propName(void) const { return m_##variableName; }
/// <summary>Creates a read-only get property.</summary>
#define __PROTECTED_READONLY_PROPERTY(type, variableName, propName)                     \
        protected: type m_##variableName;                                               \
        public: __forceinline type get##propName(void) const { return m_##variableName; }

/// <summary>Creates a read-only get property, does not use "get".</summary>
#define __PROTECTED_READONLY_PROPERTY_PLAIN(type, variableName)                         \
        protected: type m_##variableName;                                               \
        public: __forceinline type variableName(void) const { return m_##variableName; }
/// <summary>Creates a read-only get property, does not use "get".</summary>
#define __READONLY_PROPERTY_PLAIN(type, variableName)                                   \
        private: type m_##variableName;                                                 \
        public: __forceinline type variableName(void) const { return m_##variableName; }

/// <summary>Creates a get and set private property.</summary>
#define __PROPERTY(type, variableName, propName)                                        \
        private: type m_##variableName;                                                 \
        public: __forceinline type get##propName(void) const { return m_##variableName; } \
                __forceinline void set##propName(type val) { m_##variableName = val; }
/// <summary>Creates a get and set protected property.</summary>
#define __PROTECTED_PROPERTY(type, variableName, propName)                              \
        protected: type m_##variableName;                                               \
        public: __forceinline type get##propName(void) const { return m_##variableName; } \
                __forceinline void set##propName(type val) { m_##variableName = val; }

/// <summary>Creates a get and set private property, does not use "get"/"set".</summary>
#define __PROPERTY_PLAIN(type, variableName)                                            \
        private: type m_##variableName;                                                 \
        public: __forceinline type variableName(void) const { return m_##variableName; }\
                __forceinline void variableName(type val) { m_##variableName = val; }
/// <summary>Creates a get and set protected property, does not use "get"/"set".</summary>
#define __PROTECTED_PROPERTY_PLAIN(type, variableName)                                  \
        protected: type m_##variableName;                                               \
        public: __forceinline type variableName(void) const { return m_##variableName; }\
                __forceinline void variableName(type val) { m_##variableName = val; }

#endif // __COMMON_DEFINES_H__
