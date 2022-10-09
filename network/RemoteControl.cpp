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
*   Copyright (C) 2019 by Jonathan Naylor G4KLX
*   Copyright (C) 2019-2022 by Bryan Biedenkapp N2PLL
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
#include "Defines.h"
#include "edac/SHA256.h"
#include "dmr/Control.h"
#include "p25/Control.h"
#include "nxdn/Control.h"
#include "host/Host.h"
#include "network/UDPSocket.h"
#include "RemoteControl.h"
#include "HostMain.h"
#include "Log.h"
#include "Utils.h"

using namespace network;
using namespace modem;

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>

#include <memory>
#include <stdexcept>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define BAD_CMD_STR                     "Bad or invalid remote command."
#define INVALID_AUTH_STR                "Invalid authentication"
#define INVALID_OPT_STR                 "Invalid command arguments: "
#define CMD_FAILED_STR                  "Remote command failed: "

#define RCD_GET_VERSION                 "version"
#define RCD_GET_HELP                    "help"

#define RCD_MODE_CMD                    "mdm-mode"
#define RCD_MODE_OPT_IDLE               "idle"
#define RCD_MODE_OPT_LCKOUT             "lockout"
#define RCD_MODE_OPT_FDMR               "dmr"
#define RCD_MODE_OPT_FP25               "p25"
#define RCD_MODE_OPT_FNXDN              "nxdn"

#define RCD_KILL_CMD                    "mdm-kill"

#define RCD_RID_WLIST_CMD               "rid-whitelist"
#define RCD_RID_BLIST_CMD               "rid-blacklist"

#define RCD_DMR_BEACON_CMD              "dmr-beacon"
#define RCD_P25_CC_CMD                  "p25-cc"
#define RCD_P25_CC_FALLBACK             "p25-cc-fallback"

#define RCD_DMR_RID_PAGE_CMD            "dmr-rid-page"
#define RCD_DMR_RID_CHECK_CMD           "dmr-rid-check"
#define RCD_DMR_RID_INHIBIT_CMD         "dmr-rid-inhibit"
#define RCD_DMR_RID_UNINHIBIT_CMD       "dmr-rid-uninhibit"

#define RCD_P25_SET_MFID_CMD            "p25-set-mfid"
#define RCD_P25_RID_PAGE_CMD            "p25-rid-page"
#define RCD_P25_RID_CHECK_CMD           "p25-rid-check"
#define RCD_P25_RID_INHIBIT_CMD         "p25-rid-inhibit"
#define RCD_P25_RID_UNINHIBIT_CMD       "p25-rid-uninhibit"
#define RCD_P25_RID_GAQ_CMD             "p25-rid-gaq"
#define RCD_P25_RID_UREG_CMD            "p25-rid-ureg"

#define RCD_P25_PATCH_CMD               "p25-patch"

#define RCD_P25_RELEASE_GRANTS_CMD      "p25-rel-grnts"
#define RCD_P25_RELEASE_AFFS_CMD        "p25-rel-affs"

#define RCD_DMR_CC_DEDICATED_CMD        "dmr-cc-dedicated"
#define RCD_DMR_CC_BCAST_CMD            "dmr-cc-bcast"

#define RCD_P25_CC_DEDICATED_CMD        "p25-cc-dedicated"
#define RCD_P25_CC_BCAST_CMD            "p25-cc-bcast"

#define RCD_DMR_DEBUG                   "dmr-debug"
#define RCD_P25_DEBUG                   "p25-debug"
#define RCD_P25_DUMP_TSBK               "p25-dump-tsbk"
#define RCD_NXDN_DEBUG                  "nxdn-debug"

#define RCD_DMRD_MDM_INJ_CMD            "debug-dmrd-mdm-inj"
#define RCD_P25D_MDM_INJ_CMD            "debug-p25d-mdm-inj"
#define RCD_NXDD_MDM_INJ_CMD            "debug-nxdd-mdm-inj"

const uint8_t RCON_FRAME_START = 0xFEU;
const uint8_t START_OF_TEXT = 0x02U;
const uint8_t END_OF_TEXT = 0x03U;
const uint8_t END_OF_BLOCK = 0x17U;
const uint8_t REC_SEPARATOR = 0x1EU;

const uint32_t RC_BUFFER_LENGTH = 250U;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

template<typename ... FormatArgs>
std::string string_format(const std::string& format, FormatArgs ... args)
{
    int size_s = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; // extra space for '\0'
    if (size_s <= 0)
        throw std::runtime_error("Error during string formatting.");

    auto size = static_cast<size_t>(size_s);
    std::unique_ptr<char[]> buf(new char[ size ]);
    std::snprintf(buf.get(), size, format.c_str(), args ...);

    return std::string(buf.get(), buf.get() + size - 1); 
}

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the RemoteControl class.
/// </summary>
/// <param name="address">Network Hostname/IP address to connect to.</param>
/// <param name="port">Network port number.</param>
/// <param name="password">Authentication password.</param>
/// <param name="debug"></param>
RemoteControl::RemoteControl(const std::string& address, uint16_t port, const std::string& password, bool debug) :
    m_socket(address, port),
    m_p25MFId(p25::P25_MFG_STANDARD),
    m_password(password),
    m_passwordHash(NULL),
    m_debug(debug)
{
    assert(!address.empty());
    assert(port > 0U);

    if (!password.empty()) {
        size_t size = password.size();

        uint8_t* in = new uint8_t[size];
        for (size_t i = 0U; i < size; i++)
            in[i] = password.at(i);

        m_passwordHash = new uint8_t[32U];
        ::memset(m_passwordHash, 0x00U, 32U);

        edac::SHA256 sha256;
        sha256.buffer(in, (uint32_t)(size), m_passwordHash);
    }
}

/// <summary>
/// Finalizes a instance of the RemoteControl class.
/// </summary>
RemoteControl::~RemoteControl()
{
    /* stub */
}

/// <summary>
/// Sets the instances of the Radio ID and Talkgroup ID lookup tables.
/// </summary>
/// <param name="ridLookup">Radio ID Lookup Table Instance</param>
/// <param name="tidLookup">Talkgroup ID Lookup Table Instance</param>
void RemoteControl::setLookups(lookups::RadioIdLookup* ridLookup, lookups::TalkgroupIdLookup* tidLookup)
{
    m_ridLookup = ridLookup;
    m_tidLookup = tidLookup;
}

/// <summary>
/// Process remote network command data.
/// </summary>
/// <param name="host">Instance of the Host class.</param>
/// <param name="dmr">Instance of the Control class.</param>
/// <param name="p25">Instance of the Control class.</param>
/// <param name="nxdn">Instance of the Control class.</param>
void RemoteControl::process(Host* host, dmr::Control* dmr, p25::Control* p25, nxdn::Control* nxdn)
{
    std::vector<std::string> args = std::vector<std::string>();
    args.clear();

    uint8_t buffer[RC_BUFFER_LENGTH];
    std::string reply = "OK";

    sockaddr_storage address;
    uint32_t addrLen;
    uint32_t len = m_socket.read((uint8_t*)buffer, RC_BUFFER_LENGTH, address, addrLen);
    if (len > 0U) {
        buffer[len] = '\0';

        if (m_debug)
            Utils::dump(1U, "RCON Received", (uint8_t*)buffer, len);

        // make sure this is an RCON command
        if (buffer[0U] != RCON_FRAME_START) {
            LogWarning(LOG_RCON, BAD_CMD_STR);
            return;
        }

        if (buffer[1U] != START_OF_TEXT) {
            LogWarning(LOG_RCON, BAD_CMD_STR);
            return;
        }

        // ensure we have at least 34 bytes
        if (len < 34U) {
            LogWarning(LOG_RCON, BAD_CMD_STR);
            reply = BAD_CMD_STR;
            writeResponse(reply, address, addrLen);
            return;
        }

        if (m_passwordHash != NULL) {
            uint8_t hash[32U];
            ::memset(hash, 0x00U, 32U);
            if (::memcmp(m_passwordHash, buffer + 2U, 32U) != 0) {
                LogError(LOG_RCON, CMD_FAILED_STR INVALID_AUTH_STR " from %s", UDPSocket::address(address).c_str());
                reply = INVALID_AUTH_STR;
                writeResponse(reply, address, addrLen);
                return;
            }
        }

        // make sure we have arguments after the hash
        if ((buffer[34U] != REC_SEPARATOR) || (len - 35U) <= 0U) {
            LogWarning(LOG_RCON, BAD_CMD_STR);
            reply = BAD_CMD_STR;
            writeResponse(reply, address, addrLen);
            return;
        }

        uint32_t size = len - 36U;
        char argBuffer[RC_BUFFER_LENGTH];
        ::memset(argBuffer, 0x00U, RC_BUFFER_LENGTH);
        ::memcpy(argBuffer, buffer + 35U, size);

        // parse the original command into a vector of strings.
        char* b = argBuffer;
        char* p = NULL;
        while ((p = ::strtok(b, " ")) != NULL) {
            b = NULL;
            args.push_back(std::string(p));
        }

        if (args.size() < 1 || args.at(0U) == "") {
            args.clear();
            LogWarning(LOG_RCON, BAD_CMD_STR);
            reply = BAD_CMD_STR;
        }
        else {
            std::string rcom = args.at(0);
            uint32_t argCnt = args.size() - 1;

            LogInfoEx(LOG_RCON, "RCON %s, argCnt = %u from %s", rcom.c_str(), argCnt, UDPSocket::address(address).c_str());

            // process command
            if (rcom == RCD_GET_VERSION) {
                reply = (__PROG_NAME__ " " __VER__ " (" DESCR_DMR DESCR_P25 DESCR_NXDN "CW Id, Network) (built " __BUILD__ ")");
            }
            else if (rcom == RCD_GET_HELP) {
                reply = displayHelp();
            }
            else if (rcom == RCD_MODE_CMD && argCnt >= 1U) {
                std::string mode = getArgString(args, 0U);
                // Command is in the form of: "mode <mode>"
                if (mode == RCD_MODE_OPT_IDLE) {
                    host->m_fixedMode = false;
                    host->setState(STATE_IDLE);

                    reply = string_format("Dynamic mode, mode %u", host->m_state);
                    LogInfoEx(LOG_RCON, reply.c_str());
                }
                else if (mode == RCD_MODE_OPT_LCKOUT) {
                    host->m_fixedMode = false;
                    host->setState(HOST_STATE_LOCKOUT);
                    reply = string_format("Lockout mode, mode %u", host->m_state);
                    LogInfoEx(LOG_RCON, reply.c_str());
                }
#if defined(ENABLE_DMR)
                else if (mode == RCD_MODE_OPT_FDMR) {
                    if (dmr != NULL) {
                        host->m_fixedMode = true;
                        host->setState(STATE_DMR);
                        reply = string_format("Fixed mode, mode %u", host->m_state);
                        LogInfoEx(LOG_RCON, reply.c_str());
                    }
                    else {
                        reply = CMD_FAILED_STR "DMR mode is not enabled!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
#endif // defined(ENABLE_DMR)
#if defined(ENABLE_P25)
                else if (mode == RCD_MODE_OPT_FP25) {
                    if (p25 != NULL) {
                        host->m_fixedMode = true;
                        host->setState(STATE_P25);
                        reply = string_format("Fixed mode, mode %u", host->m_state);
                        LogInfoEx(LOG_RCON, reply.c_str());
                    }
                    else {
                        reply = CMD_FAILED_STR "P25 mode is not enabled!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
#endif // defined(ENABLE_P25)
#if defined(ENABLE_NXDN)
                else if (mode == RCD_MODE_OPT_FNXDN) {
                    if (p25 != NULL) {
                        host->m_fixedMode = true;
                        host->setState(STATE_NXDN);
                        reply = string_format("Fixed mode, mode %u", host->m_state);
                        LogInfoEx(LOG_RCON, reply.c_str());
                    }
                    else {
                        reply = CMD_FAILED_STR "NXDN mode is not enabled!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
#endif // defined(ENABLE_NXDN)
                else {
                        reply = INVALID_OPT_STR "invalid mode!";
                        LogError(LOG_RCON, reply.c_str());
                }
            }
            else if (rcom == RCD_KILL_CMD) {
                // Command is in the form of: "kill"
                g_killed = true;
                host->setState(HOST_STATE_QUIT);
            }
            else if (rcom == RCD_RID_WLIST_CMD && argCnt >= 1U) {
                // Command is in the form of: "rid-whitelist <RID>"
                uint32_t srcId = getArgUInt32(args, 0U);
                if (srcId != 0U) {
                    m_ridLookup->toggleEntry(srcId, true);
                }
                else {
                    reply = INVALID_OPT_STR "tried to whitelist RID 0!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
            else if (rcom == RCD_RID_BLIST_CMD && argCnt >= 1U) {
                // Command is in the form of: "rid-blacklist <RID>"
                uint32_t srcId = getArgUInt32(args, 0U);
                if (srcId != 0U) {
                    m_ridLookup->toggleEntry(srcId, false);
                }
                else {
                    reply = INVALID_OPT_STR "tried to blacklist RID 0!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
#if defined(ENABLE_DMR)
            else if (rcom == RCD_DMR_BEACON_CMD) {
                // Command is in the form of: "dmr-beacon"
                if (dmr != NULL) {
                    if (host->m_dmrBeacons) {
                        g_fireDMRBeacon = true;
                    }
                    else {
                        reply = CMD_FAILED_STR "DMR beacons is not enabled!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
                else {
                    reply = CMD_FAILED_STR "DMR mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
#endif // defined(ENABLE_DMR)
#if defined(ENABLE_P25)
            else if (rcom == RCD_P25_CC_CMD) {
                // Command is in the form of: "p25-cc"
                if (p25 != NULL) {
                    if (host->m_p25CCData) {
                        g_fireP25Control = true;
                    }
                    else {
                        reply = CMD_FAILED_STR "P25 control data is not enabled!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
                else {
                    reply = CMD_FAILED_STR "P25 mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
            else if (rcom == RCD_P25_CC_FALLBACK) {
                // Command is in the form of: "p25-cc-fallback 0/1"
                uint8_t fallback = getArgUInt8(args, 0U);
                if (p25 != NULL) {
                    if (host->m_p25CCData) {
                        p25->trunk()->setConvFallback((fallback == 1U) ? true : false);
                    }
                    else {
                        reply = CMD_FAILED_STR "P25 control data is not enabled!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
                else {
                    reply = CMD_FAILED_STR "P25 mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
#endif // defined(ENABLE_P25)
#if defined(ENABLE_DMR)
            else if (rcom == RCD_DMR_RID_PAGE_CMD && argCnt >= 2U) {
                // Command is in the form of: "dmr-rid-page <slot> <RID>"
                if (dmr != NULL) {
                    uint32_t slotNo = getArgUInt32(args, 0U);
                    uint32_t dstId = getArgUInt32(args, 1U);
                    if (slotNo > 0U && slotNo < 3U) {
                        if (dstId != 0U) {
                            dmr->writeRF_Call_Alrt(slotNo, p25::P25_WUID_FNE, dstId);
                        }
                        else {
                            reply = INVALID_OPT_STR "tried to DMR call alert RID 0!";
                            LogError(LOG_RCON, reply.c_str());
                        }
                    }
                    else {
                        reply = INVALID_OPT_STR "invalid DMR slot number for call alert!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
                else {
                    reply = CMD_FAILED_STR "DMR mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
            else if (rcom == RCD_DMR_RID_CHECK_CMD && argCnt >= 2U) {
                // Command is in the form of: "dmr-rid-check <slot> <RID>"
                if (dmr != NULL) {
                    uint32_t slotNo = getArgUInt32(args, 0U);
                    uint32_t dstId = getArgUInt32(args, 1U);
                    if (slotNo > 0U && slotNo < 3U) {
                        if (dstId != 0U) {
                            dmr->writeRF_Ext_Func(slotNo, dmr::DMR_EXT_FNCT_CHECK, p25::P25_WUID_FNE, dstId);
                        }
                        else {
                            reply = INVALID_OPT_STR "tried to DMR radio check RID 0!";
                            LogError(LOG_RCON, reply.c_str());
                        }
                    }
                    else {
                        reply = INVALID_OPT_STR "invalid DMR slot number for radio check!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
                else {
                    reply = CMD_FAILED_STR "DMR mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
            else if (rcom == RCD_DMR_RID_INHIBIT_CMD && argCnt >= 2U) {
                // Command is in the form of: "dmr-rid-inhibit <slot> <RID>"
                if (dmr != NULL) {
                    uint32_t slotNo = getArgUInt32(args, 0U);
                    uint32_t dstId = getArgUInt32(args, 1U);
                    if (slotNo > 0U && slotNo < 3U) {
                        if (dstId != 0U) {
                            dmr->writeRF_Ext_Func(slotNo, dmr::DMR_EXT_FNCT_INHIBIT, p25::P25_WUID_FNE, dstId);
                        }
                        else {
                            reply = INVALID_OPT_STR "tried to DMR radio inhibit RID 0!";
                            LogError(LOG_RCON, reply.c_str());
                        }
                    }
                    else {
                        reply = INVALID_OPT_STR "invalid DMR slot number for radio inhibit!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
                else {
                    reply = CMD_FAILED_STR "DMR mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
            else if (rcom == RCD_DMR_RID_UNINHIBIT_CMD && argCnt >= 2U) {
                // Command is in the form of: "dmr-rid-uninhibit <slot> <RID>"
                if (dmr != NULL) {
                    uint32_t slotNo = getArgUInt32(args, 0U);
                    uint32_t dstId = getArgUInt32(args, 1U);
                    if (slotNo > 0U && slotNo < 3U) {
                        if (dstId != 0U) {
                            dmr->writeRF_Ext_Func(slotNo, dmr::DMR_EXT_FNCT_UNINHIBIT, p25::P25_WUID_FNE, dstId);
                        }
                        else {
                            reply = INVALID_OPT_STR "tried to DMR radio uninhibit RID 0!";
                            LogError(LOG_RCON, reply.c_str());
                        }
                    }
                    else {
                        reply = INVALID_OPT_STR "invalid DMR slot number for radio uninhibit!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
                else {
                    reply = CMD_FAILED_STR "DMR mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
#endif // defined(ENABLE_DMR)
#if defined(ENABLE_P25)
            else if (rcom == RCD_P25_SET_MFID_CMD && argCnt >= 1U) {
                // Command is in the form of: "p25-set-mfid <Mfg. ID>
                if (p25 != NULL) {
                    uint8_t mfId = getArgUInt8(args, 0U);
                    if (mfId != 0U) {
                        LogMessage(LOG_RCON, "Remote P25, mfgId = $%02X", mfId);
                        m_p25MFId = mfId;
                    }
                    else {
                        LogMessage(LOG_RCON, "Remote P25, mfgId reset, mfgId = $%02X", mfId);
                        m_p25MFId = p25::P25_MFG_STANDARD;
                    }
                }
                else {
                    reply = CMD_FAILED_STR "P25 mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
            else if (rcom == RCD_P25_RID_PAGE_CMD && argCnt >= 1U) {
                // Command is in the form of: "p25-rid-page <RID>"
                if (p25 != NULL) {
                    uint32_t dstId = getArgUInt32(args, 0U);
                    if (dstId != 0U) {
                        p25->trunk()->setMFId(m_p25MFId);
                        p25->trunk()->writeRF_TSDU_Call_Alrt(p25::P25_WUID_FNE, dstId);
                    }
                    else {
                        reply = INVALID_OPT_STR "tried to P25 call alert RID 0!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
                else {
                    reply = CMD_FAILED_STR "P25 mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
            else if (rcom == RCD_P25_RID_CHECK_CMD && argCnt >= 1U) {
                // Command is in the form of: "p25-rid-check <RID>"
                if (p25 != NULL) {
                    uint32_t dstId = getArgUInt32(args, 0U);
                    if (dstId != 0U) {
                        p25->trunk()->setMFId(m_p25MFId);
                        p25->trunk()->writeRF_TSDU_Ext_Func(p25::P25_EXT_FNCT_CHECK, p25::P25_WUID_FNE, dstId);
                    }
                    else {
                        reply = INVALID_OPT_STR "tried to P25 radio check RID 0!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
                else {
                    reply = CMD_FAILED_STR "P25 mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
            else if (rcom == RCD_P25_RID_INHIBIT_CMD && argCnt >= 1U) {
                // Command is in the form of: "p25-rid-inhibit <RID>"
                if (p25 != NULL) {
                    uint32_t dstId = getArgUInt32(args, 0U);
                    if (dstId != 0U) {
                        p25->trunk()->setMFId(m_p25MFId);
                        p25->trunk()->writeRF_TSDU_Ext_Func(p25::P25_EXT_FNCT_INHIBIT, p25::P25_WUID_FNE, dstId);
                    }
                    else {
                        reply = INVALID_OPT_STR "tried to P25 inhibit RID 0!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
                else {
                    reply = CMD_FAILED_STR "P25 mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
            else if (rcom == RCD_P25_RID_UNINHIBIT_CMD && argCnt >= 1U) {
                // Command is in the form of: "p25-rid-uninhibit <RID>"
                if (p25 != NULL) {
                    uint32_t dstId = getArgUInt32(args, 0U);
                    if (dstId != 0U) {
                        p25->trunk()->setMFId(m_p25MFId);
                        p25->trunk()->writeRF_TSDU_Ext_Func(p25::P25_EXT_FNCT_UNINHIBIT, p25::P25_WUID_FNE, dstId);
                    }
                    else {
                        reply = INVALID_OPT_STR "tried to P25 uninhibit RID 0!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
                else {
                    reply = CMD_FAILED_STR "P25 mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
            else if (rcom == RCD_P25_RID_GAQ_CMD && argCnt >= 1U) {
                // Command is in the form of: "p25-rid-gaq <RID>"
                if (p25 != NULL) {
                    uint32_t dstId = getArgUInt32(args, 0U);
                    if (dstId != 0U) {
                        p25->trunk()->setMFId(m_p25MFId);
                        p25->trunk()->writeRF_TSDU_Grp_Aff_Q(dstId);
                    }
                    else {
                        reply = INVALID_OPT_STR "tried to P25 grp aff. query RID 0!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
                else {
                    reply = CMD_FAILED_STR "P25 mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
            else if (rcom == RCD_P25_RID_UREG_CMD && argCnt >= 1U) {
                // Command is in the form of: "p25-rid-ureg <RID>"
                if (p25 != NULL) {
                    uint32_t dstId = getArgUInt32(args, 0U);
                    if (dstId != 0U) {
                        p25->trunk()->setMFId(m_p25MFId);
                        p25->trunk()->writeRF_TSDU_U_Reg_Cmd(dstId);
                    }
                    else {
                        reply = INVALID_OPT_STR "tried to P25 unit reg. command RID 0!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
                else {
                    reply = CMD_FAILED_STR "P25 mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
            else if (rcom == RCD_P25_PATCH_CMD && argCnt >= 1U) {
                // Command is in the form of: "p25-patch <group 1> <group 2> <group 3>"
                if (p25 != NULL) {
                    uint32_t group1 = getArgUInt32(args, 0U);
                    uint32_t group2 = getArgUInt32(args, 1U);
                    uint32_t group3 = getArgUInt32(args, 2U);

                    if (group1 != 0U) {
                        p25->trunk()->setMFId(m_p25MFId);
                        p25->trunk()->writeRF_TSDU_Mot_Patch(group1, group2, group3);
                    }
                    else {
                        reply = INVALID_OPT_STR "tried to add P25 group patch with no TGID?";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
                else {
                    reply = CMD_FAILED_STR "P25 mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
            else if (rcom == RCD_P25_RELEASE_GRANTS_CMD) {
                // Command is in the form of: "p25-rel-grnts"
                if (p25 != NULL) {
                    p25->affiliations().releaseGrant(0, true);
                }
                else {
                    reply = CMD_FAILED_STR "P25 mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
            else if (rcom == RCD_P25_RELEASE_AFFS_CMD) {
                // Command is in the form of: "p25-rel-affs <group>"
                if (p25 != NULL) {
                    uint32_t grp = getArgUInt32(args, 0U);

                    if (grp == 0) {
                        p25->affiliations().clearGroupAff(0, true);
                    }
                    else {
                        p25->affiliations().clearGroupAff(grp, false);
                    }
                }
                else {
                    reply = CMD_FAILED_STR "P25 mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
#endif // defined(ENABLE_P25)
#if defined(ENABLE_DMR)
            else if (rcom == RCD_DMR_CC_DEDICATED_CMD) {
                // Command is in the form of: "dmr-cc-dedicated"
                if (dmr != NULL) {
                    if (host->m_dmrTSCCData) {
                        if (p25 != NULL) {
                            reply = CMD_FAILED_STR "Can't enable DMR control channel while P25 is enabled!";
                            LogError(LOG_RCON, reply.c_str());
                        }
                        else {
                            host->m_dmrCtrlChannel = !host->m_dmrCtrlChannel;
                            reply = string_format("DMR CC is %s", host->m_p25CtrlChannel ? "enabled" : "disabled");
                            LogInfoEx(LOG_RCON, reply.c_str());
                        }
                    }
                    else {
                        reply = CMD_FAILED_STR "DMR control data is not enabled!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
                else {
                    reply = CMD_FAILED_STR "DMR mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
            else if (rcom == RCD_DMR_CC_BCAST_CMD) {
                // Command is in the form of: "dmr-cc-bcast"
                if (dmr != NULL) {
                    host->m_dmrTSCCData = !host->m_dmrTSCCData;
                    reply = string_format("DMR CC broadcast is %s", host->m_dmrTSCCData ? "enabled" : "disabled");
                    LogInfoEx(LOG_RCON, reply.c_str());
                }
                else {
                    reply = CMD_FAILED_STR "DMR mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
#endif // defined(ENABLE_DMR)
#if defined(ENABLE_P25)
            else if (rcom == RCD_P25_CC_DEDICATED_CMD) {
                // Command is in the form of: "p25-cc-dedicated"
                if (p25 != NULL) {
                    if (host->m_p25CCData) {
                        if (dmr != NULL) {
                            reply = CMD_FAILED_STR "Can't enable P25 control channel while DMR is enabled!";
                            LogError(LOG_RCON, reply.c_str());
                        }
                        else {
                            host->m_p25CtrlChannel = !host->m_p25CtrlChannel;
                            host->m_p25CtrlBroadcast = true;
                            g_fireP25Control = true;
                            p25->setCCHalted(false);

                            reply = string_format("P25 CC is %s", host->m_p25CtrlChannel ? "enabled" : "disabled");
                            LogInfoEx(LOG_RCON, reply.c_str());
                        }
                    }
                    else {
                        reply = CMD_FAILED_STR "P25 control data is not enabled!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
                else {
                    reply = CMD_FAILED_STR "P25 mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
            else if (rcom == RCD_P25_CC_BCAST_CMD) {
                // Command is in the form of: "p25-cc-bcast"
                if (p25 != NULL) {
                    if (host->m_p25CCData) {
                        host->m_p25CtrlBroadcast = !host->m_p25CtrlBroadcast;

                        if (!host->m_p25CtrlBroadcast) {
                            g_fireP25Control = false;
                            p25->setCCHalted(true);
                        }
                        else {
                            g_fireP25Control = true;
                            p25->setCCHalted(false);
                        }

                        reply = string_format("P25 CC broadcast is %s", host->m_p25CtrlBroadcast ? "enabled" : "disabled");
                        LogInfoEx(LOG_RCON, reply.c_str());
                    }
                    else {
                        reply = CMD_FAILED_STR "P25 control data is not enabled!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
                else {
                    reply = CMD_FAILED_STR "P25 mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
#endif // defined(ENABLE_P25)
#if defined(ENABLE_DMR)
            else if (rcom == RCD_DMR_DEBUG) {
                // Command is in the form of: "dmr-debug <debug 0/1> <trace 0/1>"
                if (argCnt < 2U) {
                    LogWarning(LOG_RCON, BAD_CMD_STR);
                    reply = BAD_CMD_STR;
                } 
                else {
                    uint8_t debug = getArgUInt8(args, 0U);
                    uint8_t verbose = getArgUInt8(args, 1U);
                    if (dmr != NULL) {
                        dmr->setDebugVerbose((debug == 1U) ? true : false, (verbose == 1U) ? true : false);
                    }
                    else {
                        reply = CMD_FAILED_STR "DMR mode is not enabled!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
            }
#endif // defined(ENABLE_DMR)
#if defined(ENABLE_P25)
            else if (rcom == RCD_P25_DEBUG) {
                // Command is in the form of: "p25-debug <debug 0/1> <trace 0/1>"
                if (argCnt < 2U) {
                    LogWarning(LOG_RCON, BAD_CMD_STR);
                    reply = BAD_CMD_STR;
                } 
                else {
                    uint8_t debug = getArgUInt8(args, 0U);
                    uint8_t verbose = getArgUInt8(args, 1U);
                    if (p25 != NULL) {
                        p25->setDebugVerbose((debug == 1U) ? true : false, (verbose == 1U) ? true : false);
                    }
                    else {
                        reply = CMD_FAILED_STR "P25 mode is not enabled!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
            }
            else if (rcom == RCD_P25_DUMP_TSBK) {
                // Command is in the form of: "p25-dump-tsbk 0/1"
                if (argCnt < 1U) {
                    LogWarning(LOG_RCON, BAD_CMD_STR);
                    reply = BAD_CMD_STR;
                } 
                else {
                    uint8_t verbose = getArgUInt8(args, 0U);
                    if (p25 != NULL) {
                        p25->trunk()->setTSBKVerbose((verbose == 1U) ? true : false);
                    }
                    else {
                        reply = CMD_FAILED_STR "P25 mode is not enabled!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
            }
#endif // defined(ENABLE_P25)
#if defined(ENABLE_NXDN)
            else if (rcom == RCD_NXDN_DEBUG) {
                // Command is in the form of: "nxdn-debug <debug 0/1> <trace 0/1>"
                if (argCnt < 2U) {
                    LogWarning(LOG_RCON, BAD_CMD_STR);
                    reply = BAD_CMD_STR;
                } 
                else {
                    uint8_t debug = getArgUInt8(args, 0U);
                    uint8_t verbose = getArgUInt8(args, 1U);
                    if (nxdn != NULL) {
                        nxdn->setDebugVerbose((debug == 1U) ? true : false, (verbose == 1U) ? true : false);
                    }
                    else {
                        reply = CMD_FAILED_STR "NXDN mode is not enabled!";
                        LogError(LOG_RCON, reply.c_str());
                    }
                }
            }
#endif // defined(ENABLE_NXDN)
#if defined(ENABLE_DMR)
            else if (rcom == RCD_DMRD_MDM_INJ_CMD && argCnt >= 1U) {
                // Command is in the form of: "debug-dmrd-mdm-inj <slot> <bin file>
                if (dmr != NULL) {
                    uint8_t slot = getArgUInt32(args, 0U);
                    std::string argString = getArgString(args, 1U);
                    const char* fileName = argString.c_str();
                    if (fileName != NULL) {
                        FILE* file = ::fopen(fileName, "r");
                        if (file != NULL) {
                            uint8_t* buffer = NULL;
                            int32_t fileSize = 0;

                            // obtain file size
                            ::fseek(file, 0, SEEK_END);
                            fileSize = ::ftell(file);
                            ::rewind(file);

                            // allocate a buffer and read file
                            buffer = new uint8_t[fileSize];
                            if (buffer != NULL) {
                                int32_t bytes = ::fread(buffer, 1U, fileSize, file);
                                if (bytes == fileSize) {
                                    uint8_t sync[dmr::DMR_SYNC_LENGTH_BYTES];
                                    ::memcpy(sync, buffer, dmr::DMR_SYNC_LENGTH_BYTES);

                                    // count data sync errors
                                    uint8_t dataErrs = 0U;
                                    for (uint8_t i = 0U; i < dmr::DMR_SYNC_LENGTH_BYTES; i++)
                                        dataErrs += Utils::countBits8(sync[i] ^ dmr::DMR_MS_DATA_SYNC_BYTES[i]);

                                    // count voice sync errors
                                    uint8_t voiceErrs = 0U;
                                    for (uint8_t i = 0U; i < dmr::DMR_SYNC_LENGTH_BYTES; i++)
                                        voiceErrs += Utils::countBits8(sync[i] ^ dmr::DMR_MS_VOICE_SYNC_BYTES[i]);

                                    if ((dataErrs <= 4U) || (voiceErrs <= 4U)) {
                                        if (slot == 0U) {
                                            host->m_modem->injectDMRData1(buffer, fileSize);
                                        }
                                        else if (slot == 1U) {
                                            host->m_modem->injectDMRData2(buffer, fileSize);
                                        }
                                        else {
                                            reply = CMD_FAILED_STR "invalid DMR slot!";
                                            LogError(LOG_RCON, reply.c_str());
                                        }
                                    }
                                    else {
                                        reply = CMD_FAILED_STR "DMR data has too many errors!";
                                        LogError(LOG_RCON, reply.c_str());
                                    }
                                }
                                else {
                                    reply = CMD_FAILED_STR "DMR failed to open DMR data!";
                                    LogError(LOG_RCON, reply.c_str());
                                }

                                delete[] buffer;
                            }

                            ::fclose(file);
                        }
                    }
                }
                else {
                    reply = CMD_FAILED_STR "DMR mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
#endif // defined(ENABLE_DMR)
#if defined(ENABLE_P25)
            else if (rcom == RCD_P25D_MDM_INJ_CMD && argCnt >= 1U) {
                // Command is in the form of: "debug-p25d-mdm-inj <bin file>
                if (p25 != NULL) {
                    std::string argString = getArgString(args, 0U);
                    const char* fileName = argString.c_str();
                    if (fileName != NULL) {
                        FILE* file = ::fopen(fileName, "r");
                        if (file != NULL) {
                            uint8_t* buffer = NULL;
                            int32_t fileSize = 0;

                            // obtain file size
                            ::fseek(file, 0, SEEK_END);
                            fileSize = ::ftell(file);
                            ::rewind(file);

                            // allocate a buffer and read file
                            buffer = new uint8_t[fileSize];
                            if (buffer != NULL) {
                                int32_t bytes = ::fread(buffer, 1U, fileSize, file);
                                if (bytes == fileSize) {
                                    uint8_t sync[p25::P25_SYNC_LENGTH_BYTES];
                                    ::memcpy(sync, buffer, p25::P25_SYNC_LENGTH_BYTES);

                                    uint8_t errs = 0U;
                                    for (uint8_t i = 0U; i < p25::P25_SYNC_LENGTH_BYTES; i++)
                                        errs += Utils::countBits8(sync[i] ^ p25::P25_SYNC_BYTES[i]);

                                    if (errs <= 4U) {
                                        bool valid = p25->nid().decode(buffer);
                                        if (valid) {
                                            host->m_modem->injectP25Data(buffer, fileSize);
                                        }
                                        else {
                                            reply = CMD_FAILED_STR "P25 data did not contain a valid NID!";
                                            LogError(LOG_RCON, reply.c_str());
                                        }
                                    }
                                    else {
                                        reply = CMD_FAILED_STR "P25 data has too many errors!";
                                        LogError(LOG_RCON, reply.c_str());
                                    }
                                }
                                else {
                                    reply = CMD_FAILED_STR "P25 failed to open P25 data!";
                                    LogError(LOG_RCON, reply.c_str());
                                }

                                delete[] buffer;
                            }

                            ::fclose(file);
                        }
                    }
                }
                else {
                    reply = CMD_FAILED_STR "P25 mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
#endif // defined(ENABLE_P25)
#if defined(ENABLE_NXDN)
            else if (rcom == RCD_NXDD_MDM_INJ_CMD && argCnt >= 1U) {
                // Command is in the form of: "debug-nxdd-mdm-inj <bin file>
                if (p25 != NULL) {
                    std::string argString = getArgString(args, 0U);
                    const char* fileName = argString.c_str();
                    if (fileName != NULL) {
                        FILE* file = ::fopen(fileName, "r");
                        if (file != NULL) {
                            uint8_t* buffer = NULL;
                            int32_t fileSize = 0;

                            // obtain file size
                            ::fseek(file, 0, SEEK_END);
                            fileSize = ::ftell(file);
                            ::rewind(file);

                            // allocate a buffer and read file
                            buffer = new uint8_t[fileSize];
                            if (buffer != NULL) {
                                int32_t bytes = ::fread(buffer, 1U, fileSize, file);
                                if (bytes == fileSize) {
                                    uint8_t sync[nxdn::NXDN_FSW_BYTES_LENGTH];
                                    ::memcpy(sync, buffer, nxdn::NXDN_FSW_BYTES_LENGTH);

                                    uint8_t errs = 0U;
                                    for (uint8_t i = 0U; i < nxdn::NXDN_FSW_BYTES_LENGTH; i++)
                                        errs += Utils::countBits8(sync[i] ^ nxdn::NXDN_FSW_BYTES[i]);

                                    if (errs <= 4U) {
                                        host->m_modem->injectNXDNData(buffer, fileSize);
                                    }
                                    else {
                                        reply = CMD_FAILED_STR "NXDN data has too many errors!";
                                        LogError(LOG_RCON, reply.c_str());
                                    }
                                }
                                else {
                                    reply = CMD_FAILED_STR "NXDN failed to open NXDN data!";
                                    LogError(LOG_RCON, reply.c_str());
                                }

                                delete[] buffer;
                            }

                            ::fclose(file);
                        }
                    }
                }
                else {
                    reply = CMD_FAILED_STR "NXDN mode is not enabled!";
                    LogError(LOG_RCON, reply.c_str());
                }
            }
#endif // defined(ENABLE_NXDN)
            else {
                args.clear();
                reply = BAD_CMD_STR;
                LogError(LOG_RCON, BAD_CMD_STR " (\"%s\")", rcom.c_str());
            }
        }

        // write response
        writeResponse(reply, address, addrLen);
    }
}

/// <summary>
/// Opens connection to the network.
/// </summary>
/// <returns></returns>
bool RemoteControl::open()
{
    return m_socket.open();
}

/// <summary>
/// Closes connection to the network.
/// </summary>
void RemoteControl::close()
{
    m_socket.close();
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Helper to write response to client.
/// </summary>
/// <param name="reply"></param>
/// <param name="complete"></param>
/// <param name="address"></param>
/// <param name="addrLen"></param>
void RemoteControl::writeResponse(std::string reply, sockaddr_storage address, uint32_t addrLen)
{
    uint8_t buffer[RC_BUFFER_LENGTH];
    ::memset(buffer, 0x00U, RC_BUFFER_LENGTH);

    buffer[0U] = RCON_FRAME_START;
    buffer[1U] = START_OF_TEXT;

    LogInfoEx(LOG_RCON, "RCON reply len = %u, blocks = %u to %s", reply.length(), (reply.length() / (RC_BUFFER_LENGTH - 3U)) + 1U, UDPSocket::address(address).c_str());

    if (reply.length() > (RC_BUFFER_LENGTH - 3U)) {
        uint32_t len = reply.length(), offs = 0U;
        for (uint32_t i = 0U; i < reply.length() / (RC_BUFFER_LENGTH - 3U); i++) {
            if (m_debug)
                LogDebug(LOG_RCON, "RemoteControl::writeResponse() block = %u, block len = %u, offs = %u", i, len, offs);

            std::string str = reply.substr(offs, RC_BUFFER_LENGTH - 2U);

            ::memset(buffer + 2U, 0x00U, str.length());
            ::memcpy(buffer + 2U, str.c_str(), str.length());

            if (len - RC_BUFFER_LENGTH == 0U) {
                buffer[str.length() + 1U] = END_OF_TEXT;
            }
            else {
                buffer[str.length() + 1U] = END_OF_BLOCK;
            }

            if (m_debug)
                Utils::dump(1U, "RCON (Multiblock) Sent", (uint8_t*)buffer, RC_BUFFER_LENGTH);

            m_socket.write(buffer, RC_BUFFER_LENGTH, address, addrLen);

            Thread::sleep(50);

            offs += RC_BUFFER_LENGTH - 3U;
            len -= RC_BUFFER_LENGTH - 3U;
        }

        if (m_debug)
            LogDebug(LOG_RCON, "RemoteControl::writeResponse() remaining block len = %u, offs = %u", len, offs);

        if (len > 0U) {
            std::string str = reply.substr(offs, std::string::npos);

            ::memset(buffer + 2U, 0x00U, RC_BUFFER_LENGTH - 2U);
            ::memcpy(buffer + 2U, str.c_str(), str.length());

            buffer[str.length() + 1U] = END_OF_TEXT;

            if (m_debug)
                Utils::dump(1U, "RCON (Multiblock) Sent", (uint8_t*)buffer, str.length() + 3U);

            m_socket.write(buffer, str.length() + 3U, address, addrLen);
        }
    }
    else {
        ::memcpy(buffer + 2U, reply.c_str(), reply.length());
        buffer[reply.length() + 1U] = END_OF_TEXT;

        if (m_debug) {
            LogDebug(LOG_RCON, "RemoteControl::writeResponse() single block len = %u", reply.length() + 3U);
            Utils::dump(1U, "RCON Sent", (uint8_t*)buffer, reply.length() + 3U);
        }

        m_socket.write(buffer, reply.length() + 3U, address, addrLen);
    }
}

/// <summary>
/// Helper to print the remote control help.
/// </summary>
std::string RemoteControl::displayHelp()
{
    std::string reply = "";

    reply += "RCON Help\r\nGeneral Commands:\r\n";
    reply += "  version                 Display current version of host\r\n";
    reply += "  mdm-mode <mode>         Set current mode of host (idle, lockout, dmr, p25, nxdn)\r\n";
    reply += "  mdm-kill                Causes the host to quit\r\n";
    reply += "\r\n";
    reply += "  rid-whitelist <rid>     Whitelists the specified RID in the host ACL tables\r\n";
    reply += "  rid-blacklist <rid>     Blacklists the specified RID in the host ACL tables\r\n";
    reply += "\r\n";
    reply += "  dmr-beacon              Transmits a DMR beacon burst\r\n";
    reply += "  p25-cc                  Transmits a non-continous P25 CC burst\r\n";
    reply += "  p25-cc-fallback <0/1>   Sets the P25 CC into conventional fallback mode\r\n";
    reply += "\r\n";
    reply += "  dmr-debug <debug 0/1> <verbose 0/1>\r\n";
    reply += "  p25-debug <debug 0/1> <verbose 0/1>\r\n";
    reply += "  nxdn-debug <debug 0/1> <verbose 0/1>\r\n";
    reply += "\r\nDMR Commands:\r\n";
    reply += "  dmr-rid-page <rid>      Pages/Calls the specified RID\r\n";
    reply += "  dmr-rid-check <rid>     Radio Checks the specified RID\r\n";
    reply += "  dmr-rid-inhibit <rid>   Inhibits the specified RID\r\n";
    reply += "  dmr-rid-uninhibit <rid> Uninhibits the specified RID\r\n";
    reply += "\r\n";
    reply += "  dmr-cc-dedicated <0/1>  Enables or disables dedicated control channel\r\n";
    reply += "  dmr-cc-bcast <0/1>      Enables or disables broadcast of the control channel\r\n";
    reply += "\r\nP25 Commands:\r\n";
    reply += "  p25-set-mfid <mfid>     Sets the P25 MFId for the next sent P25 command\r\n";
    reply += "  p25-rid-page <rid>      Pages/Calls the specified RID\r\n";
    reply += "  p25-rid-check <rid>     Radio Checks the specified RID\r\n";
    reply += "  p25-rid-inhibit <rid>   Inhibits the specified RID\r\n";
    reply += "  p25-rid-uninhibit <rid> Uninhibits the specified RID\r\n";
    reply += "  p25-rid-gaq <rid>       Group affiliation queries the specified RID\r\n";
    reply += "  p25-rid-ureg <rid>      Demand unit registration for the specified RID\r\n";
    reply += "\r\n";
    reply += "  p25-rel-grnts           Forcibly releases all channel grants for P25\r\n";
    reply += "  p25-rel-affs            Forcibly releases all group affiliations for P25\r\n";
    reply += "\r\n";
    reply += "  p25-cc-dedicated <0/1>  Enables or disables dedicated control channel\r\n";
    reply += "  p25-cc-bcast <0/1>      Enables or disables broadcast of the control channel\r\n";
    return reply;
}

/// <summary>
///
/// </summary>
/// <param name="args"></param>
/// <param name="n"></param>
/// <returns></returns>
std::string RemoteControl::getArgString(std::vector<std::string> args, uint32_t n) const
{
    n += 1;
    if (n >= args.size())
        return "";

    return args.at(n);
}

/// <summary>
///
/// </summary>
/// <param name="args"></param>
/// <param name="n"></param>
/// <returns></returns>
uint64_t RemoteControl::getArgUInt64(std::vector<std::string> args, uint32_t n) const
{
    return (uint64_t)::atol(getArgString(args, n).c_str());
}

/// <summary>
///
/// </summary>
/// <param name="args"></param>
/// <param name="n"></param>
/// <returns></returns>
uint32_t RemoteControl::getArgUInt32(std::vector<std::string> args, uint32_t n) const
{
    return (uint32_t)::atoi(getArgString(args, n).c_str());
}

/// <summary>
///
/// </summary>
/// <param name="args"></param>
/// <param name="n"></param>
/// <returns></returns>
int32_t RemoteControl::getArgInt32(std::vector<std::string> args, uint32_t n) const
{
    return ::atoi(getArgString(args, n).c_str());
}

/// <summary>
///
/// </summary>
/// <param name="args"></param>
/// <param name="n"></param>
/// <returns></returns>
uint16_t RemoteControl::getArgUInt16(std::vector<std::string> args, uint32_t n) const
{
    return (uint16_t)::atoi(getArgString(args, n).c_str());
}

/// <summary>
///
/// </summary>
/// <param name="args"></param>
/// <param name="n"></param>
/// <returns></returns>
int16_t RemoteControl::getArgInt16(std::vector<std::string> args, uint32_t n) const
{
    return (int16_t)::atoi(getArgString(args, n).c_str());
}

/// <summary>
///
/// </summary>
/// <param name="args"></param>
/// <param name="n"></param>
/// <returns></returns>
uint8_t RemoteControl::getArgUInt8(std::vector<std::string> args, uint32_t n) const
{
    return (uint8_t)::atoi(getArgString(args, n).c_str());
}

/// <summary>
///
/// </summary>
/// <param name="args"></param>
/// <param name="n"></param>
/// <returns></returns>
int8_t RemoteControl::getArgInt8(std::vector<std::string> args, uint32_t n) const
{
    return (int8_t)::atoi(getArgString(args, n).c_str());
}
