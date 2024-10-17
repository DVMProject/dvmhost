// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - FNE System View
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file SysViewMain.h
 * @ingroup fneSysView
 * @file SysViewMain.cpp
 * @ingroup fneSysView
 */
#if !defined(__SYS_VIEW_MAIN_H__)
#define __SYS_VIEW_MAIN_H__

#include "Defines.h"
#include "common/lookups/RadioIdLookup.h"
#include "common/lookups/TalkgroupRulesLookup.h"
#include "common/lookups/IdenTableLookup.h"
#include "common/yaml/Yaml.h"
#include "network/PeerNetwork.h"

#include <string>
#include <functional>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#undef __PROG_NAME__
#define __PROG_NAME__ "Digital Voice Modem (DVM) FNE System View"
#undef __EXE_NAME__ 
#define __EXE_NAME__ "sysview"

#undef __NETVER__
#define __NETVER__ "SYSVIEW_R" VERSION_MAJOR VERSION_REV VERSION_MINOR

// ---------------------------------------------------------------------------
//  Externs
// ---------------------------------------------------------------------------

/** @brief  */
extern int g_signal;

/** @brief  */
extern std::string g_progExe;
/** @brief  */
extern std::string g_iniFile;
/** @brief  */
extern yaml::Node g_conf;
/** @brief  */
extern bool g_debug;

/** @brief (Global) Flag indicating foreground operation. */
extern bool g_foreground;
/** @brief (Global) Flag indicating the SysView should stop immediately. */
extern bool g_killed;

/** @brief  */
extern bool g_hideLoggingWnd;

/** @brief  */
extern lookups::RadioIdLookup* g_ridLookup;
/** @brief  */
extern lookups::TalkgroupRulesLookup* g_tidLookup;
/** @brief  */
extern lookups::IdenTableLookup* g_idenTable;

typedef std::pair<const uint32_t, std::string> PeerIdentityMapPair;
/** @brief  */
extern std::unordered_map<uint32_t, std::string> g_peerIdentityNameMap;

/**
 * @brief Helper to trigger a fatal error message. This will cause the program to terminate 
 * immediately with an error message.
 * 
 * @param msg String format.
 * 
 * This is a variable argument function.
 */
extern HOST_SW_API void fatal(const char* msg, ...);

/**
 * @brief Helper to set the network data event callback.
 * @param callback 
 */
extern HOST_SW_API void setNetDataEventCallback(std::function<void(json::object)>&& callback);

/**
 * @brief Helper to resolve a TGID to a textual name.
 * @param id Talkgroup ID.
 * @return std::string Textual name for TGID.
 */
extern HOST_SW_API std::string resolveRID(uint32_t id);

/**
 * @brief Helper to resolve a TGID to a textual name.
 * @param id Talkgroup ID.
 * @return std::string Textual name for TGID.
 */
extern HOST_SW_API std::string resolveTGID(uint32_t id);

/**
 * @brief Initializes peer network connectivity. 
 * @returns bool 
 */
extern HOST_SW_API bool createPeerNetwork();

/**
 * @brief 
 * @returns PeerNetwork* 
 */
extern HOST_SW_API network::PeerNetwork* getNetwork();

#endif // __SYS_VIEW_MAIN_H__
