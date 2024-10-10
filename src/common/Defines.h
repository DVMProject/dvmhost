// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
 *  Copyright (C) 2018-2024 Bryan Biedenkapp, N2PLL
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
#define VERSION_MINOR       "11"
#define VERSION_REV         "F"

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

// ---------------------------------------------------------------------------
//  Class Helper Macros
// ---------------------------------------------------------------------------

/**
 * Class Copy Code Pattern
 */

/**
 * @brief Creates a private copy implementation.
 * This requires the copy(const type& data) to be declared in the class definition.
 * @param type Atomic type.
 */
#define __COPY(type)                                                                    \
        private: virtual void copy(const type& data);                                   \
        public: __forceinline type& operator=(const type& data) {                       \
            if (this != &data) {                                                        \
                copy(data);                                                             \
            }                                                                           \
            return *this;                                                               \
        }
/**
 * @brief Creates a protected copy implementation.
 * This requires the copy(const type& data) to be declared in the class definition.
 * @param type Atomic type.
 */
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

/**
 * @brief Creates a read-only get property.
 * @param type Atomic type for property.
 * @param variableName Variable name for property.
 * @param propName Property name.
 */
#define __READONLY_PROPERTY(type, variableName, propName)                               \
        private: type m_##variableName;                                                 \
        public: __forceinline type get##propName(void) const { return m_##variableName; }
/**
 * @brief Creates a read-only get property.
 * @param type Atomic type for property.
 * @param variableName Variable name for property.
 * @param propName Property name.
 */
#define __PROTECTED_READONLY_PROPERTY(type, variableName, propName)                     \
        protected: type m_##variableName;                                               \
        public: __forceinline type get##propName(void) const { return m_##variableName; }

/**
 * @brief Creates a read-only get property, does not use "get".
 * @param type Atomic type for property.
 * @param variableName Variable name for property.
 */
#define __PROTECTED_READONLY_PROPERTY_PLAIN(type, variableName)                         \
        protected: type m_##variableName;                                               \
        public: __forceinline type variableName(void) const { return m_##variableName; }
/**
 * @brief Creates a read-only get property, does not use "get".
 * @param type Atomic type for property.
 * @param variableName Variable name for property.
 */
#define __READONLY_PROPERTY_PLAIN(type, variableName)                                   \
        private: type m_##variableName;                                                 \
        public: __forceinline type variableName(void) const { return m_##variableName; }

/**
 * @brief Creates a get and set private property.
 * @param type Atomic type for property.
 * @param variableName Variable name for property.
 * @param propName Property name.
 */
#define __PROPERTY(type, variableName, propName)                                        \
        private: type m_##variableName;                                                 \
        public: __forceinline type get##propName(void) const { return m_##variableName; } \
                __forceinline void set##propName(type val) { m_##variableName = val; }
/**
 * @brief Creates a get and set protected property.
 * @param type Atomic type for property.
 * @param variableName Variable name for property.
 * @param propName Property name.
 */
#define __PROTECTED_PROPERTY(type, variableName, propName)                              \
        protected: type m_##variableName;                                               \
        public: __forceinline type get##propName(void) const { return m_##variableName; } \
                __forceinline void set##propName(type val) { m_##variableName = val; }

/**
 * @brief Creates a get and set private property, does not use "get"/"set".
 * @param type Atomic type for property.
 * @param variableName Variable name for property.
 */
#define __PROPERTY_PLAIN(type, variableName)                                            \
        private: type m_##variableName;                                                 \
        public: __forceinline type variableName(void) const { return m_##variableName; }\
                __forceinline void variableName(type val) { m_##variableName = val; }
/**
 * @brief Creates a get and set protected property, does not use "get"/"set".
 * @param type Atomic type for property.
 * @param variableName Variable name for property.
 */
#define __PROTECTED_PROPERTY_PLAIN(type, variableName)                                  \
        protected: type m_##variableName;                                               \
        public: __forceinline type variableName(void) const { return m_##variableName; }\
                __forceinline void variableName(type val) { m_##variableName = val; }

/** @} */
#endif // __COMMON_DEFINES_H__
