/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2015,2016,2017 by Jonathan Naylor G4KLX
*   Copyright (C) 2018-2020 by Bryan Biedenkapp N2PLL
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
#if !defined(__DEFINES_H__)
#define __DEFINES_H__

#include <stdint.h>

#include <string>
#include <sstream>
#include <ios>

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

#define __PROG_NAME__ "Digital Voice Modem Host"
#define __NET_NAME__ "DVM_DMR_P25"
#define __EXE_NAME__ "dvmhost"
#define __VER__ "R01.00.00"
#define __BUILD__ __DATE__ " " __TIME__

#define HOST_SW_API 

#if defined(_WIN32) || defined(_WIN64)
#define DEFAULT_CONF_FILE "config.yml"
#else
#define DEFAULT_CONF_FILE "/opt/dvm/config.yml"
#endif // defined(_WIN32) || defined(_WIN64)
#if defined(_WIN32) || defined(_WIN64)
#define DEFAULT_LOCK_FILE "dvm.lock"
#else
#define DEFAULT_LOCK_FILE "/tmp/dvm.lock"
#endif // defined(_WIN32) || defined(_WIN64)

#if defined(__GNUC__) || defined(__GNUG__)
#define __forceinline __attribute__((always_inline))
#endif

#define NULL_PORT "null"
#define UART_PORT "uart"
#define PTY_PORT "pty"
#define UDP_PORT "udp"

#define UDP_MODE_MASTER "master"
#define UDP_MODE_PEER "peer"

const uint32_t  REMOTE_MODEM_PORT = 3334;
const uint32_t  TRAFFIC_DEFAULT_PORT = 62031;
const uint32_t  RCON_DEFAULT_PORT = 9990;

const uint32_t  QUEUE_RESIZE_SIZE = 500;

const uint8_t   BIT_MASK_TABLE[] = { 0x80U, 0x40U, 0x20U, 0x10U, 0x08U, 0x04U, 0x02U, 0x01U };

enum HOST_STATE {
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
//  Inlines
// ---------------------------------------------------------------------------

inline std::string __BOOL_STR(const bool& value) {
    std::stringstream ss;
    ss << std::boolalpha << value;
    return ss.str();
}

inline std::string __INT_STR(const int& value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
}

inline std::string __INT_HEX_STR(const int& value) {
    std::stringstream ss;
    ss << std::hex << value;
    return ss.str();
}

inline std::string __FLOAT_STR(const float& value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
}

inline std::string __IP_FROM_ULONG(const ulong64_t& value) {
    std::stringstream ss;
    ss << ((value >> 24) & 0xFFU) << "." << ((value >> 16) & 0xFFU) << "." << ((value >> 8) & 0xFFU) << "." << (value & 0xFFU);
    return ss.str();
}

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

#define __FLOAT_ADDR(x)  (*(uint32_t*)& x)
#define __DOUBLE_ADDR(x) (*(uint64_t*)& x)

#define WRITE_BIT(p, i, b) p[(i) >> 3] = (b) ? (p[(i) >> 3] | BIT_MASK_TABLE[(i) & 7]) : (p[(i) >> 3] & ~BIT_MASK_TABLE[(i) & 7])
#define READ_BIT(p, i)     (p[(i) >> 3] & BIT_MASK_TABLE[(i) & 7])

#define __SET_UINT32(val, buffer, offset)           \
            buffer[0U + offset] = val >> 24;        \
            buffer[1U + offset] = val >> 16;        \
            buffer[2U + offset] = val >> 8;         \
            buffer[3U + offset] = val >> 0;          
#define __GET_UINT32(buffer, offset)                \
            (buffer[offset + 0U] << 24)     |       \
                (buffer[offset + 1U] << 16) |       \
                (buffer[offset + 2U] << 8)  |       \
                (buffer[offset + 3U] << 0);
#define __SET_UINT16(val, buffer, offset)           \
            buffer[0U + offset] = val >> 16;        \
            buffer[1U + offset] = val >> 8;         \
            buffer[2U + offset] = val >> 0;          
#define __GET_UINT16(buffer, offset)                \
            (buffer[offset + 0U] << 16)     |       \
                (buffer[offset + 1U] << 8)  |       \
                (buffer[offset + 2U] << 0);

/**
 * Property Creation
 *  These macros should always be used LAST in the "public" section of a class definition.
 */
/// <summary>Creates a read-only get property.</summary>
#define __READONLY_PROPERTY(type, variableName, propName)                               \
        private: type m_##variableName;                                                 \
        public: __forceinline type get##propName(void) const { return m_##variableName; }
/// <summary>Creates a read-only get property, does not use "get".</summary>
#define __READONLY_PROPERTY_PLAIN(type, variableName, propName)                         \
        private: type m_##variableName;                                                 \
        public: __forceinline type propName(void) const { return m_##variableName; }
/// <summary>Creates a read-only get property by reference.</summary>
#define __READONLY_PROPERTY_BYREF(type, variableName, propName)                         \
        private: type m_##variableName;                                                 \
        public: __forceinline type& get##propName(void) const { return m_##variableName; }

/// <summary>Creates a get and set property.</summary>
#define __PROPERTY(type, variableName, propName)                                        \
        private: type m_##variableName;                                                 \
        public: __forceinline type get##propName(void) const { return m_##variableName; } \
                __forceinline void set##propName(type val) { m_##variableName = val; }
/// <summary>Creates a get and set property, does not use "get"/"set".</summary>
#define __PROPERTY_PLAIN(type, variableName, propName)                                  \
        private: type m_##variableName;                                                 \
        public: __forceinline type propName(void) const { return m_##variableName; }    \
                __forceinline void propName(type val) { m_##variableName = val; }
/// <summary>Creates a get and set property by reference.</summary>
#define __PROPERTY_BYREF(type, variableName, propName)                                  \
        private: type m_##variableName;                                                 \
        public: __forceinline type& get##propName(void) const { return m_##variableName; } \
                __forceinline void set##propName(type& val) { m_##variableName = val; }

#endif // __DEFINES_H__
