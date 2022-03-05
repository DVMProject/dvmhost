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

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define BAD_CMD_STR                     "Bad or invalid remote command."
#define INVALID_OPT_STR                 "Invalid command arguments: "
#define CMD_FAILED_STR                  "Remote command failed: "

#define RCD_MODE_CMD                    "mdm-mode"
#define RCD_MODE_OPT_IDLE               "idle"
#define RCD_MODE_OPT_LCKOUT             "lockout"
#define RCD_MODE_OPT_FDMR               "dmr"
#define RCD_MODE_OPT_FP25               "p25"

#define RCD_KILL_CMD                    "mdm-kill"

#define RCD_RID_WLIST_CMD               "rid-whitelist"
#define RCD_RID_BLIST_CMD               "rid-blacklist"

#define RCD_DMR_BEACON_CMD              "dmr-beacon"
#define RCD_P25_CC_CMD                  "p25-cc"

#define RCD_DMRD_MDM_INJ_CMD            "dmrd-mdm-inj"
#define RCD_P25D_MDM_INJ_CMD            "p25d-mdm-inj"

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

const uint32_t START_OF_TEXT = 0x02;
const uint32_t REC_SEPARATOR = 0x1E;

const uint32_t RC_BUFFER_LENGTH = 140U;

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
void RemoteControl::process(Host* host, dmr::Control* dmr, p25::Control* p25)
{
    std::vector<std::string> args = std::vector<std::string>();
    args.clear();

    uint8_t buffer[RC_BUFFER_LENGTH];

    sockaddr_storage address;
    uint32_t addrLen;
    uint32_t ret = m_socket.read((uint8_t*)buffer, RC_BUFFER_LENGTH, address, addrLen);
    if (ret > 0U) {
        buffer[ret] = '\0';

        if (m_debug)
            Utils::dump(1U, "RCON Received", (uint8_t*)buffer, ret);

        // make sure this is an RCON command
        if (buffer[0U] != START_OF_TEXT) {
            LogWarning(LOG_RCON, BAD_CMD_STR);
            return;
        }

        // ensure we have at least 33 bytes
        if (ret < 33U) {
            LogWarning(LOG_RCON, BAD_CMD_STR);
            return;
        }

        if (m_passwordHash != NULL) {
            uint8_t hash[32U];
            ::memset(hash, 0x00U, 32U);
            if (::memcmp(m_passwordHash, buffer + 1U, 32U) != 0) {
                LogError(LOG_RCON, CMD_FAILED_STR "Invalid authentication!");
                return;
            }
        }

        // make sure we have arguments after the hash
        if ((buffer[33U] != REC_SEPARATOR) || (ret - 34U) <= 0U) {
            LogWarning(LOG_RCON, BAD_CMD_STR);
            return;
        }

        uint32_t size = ret - 34U;
        char argBuffer[RC_BUFFER_LENGTH];
        ::memset(argBuffer, 0x00U, RC_BUFFER_LENGTH);
        ::memcpy(argBuffer, buffer + 34U, size);

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
        }
        else {
            std::string rcom = args.at(0);
            uint32_t argCnt = args.size() - 1;

            LogInfoEx(LOG_RCON, "RCON %s, argCnt = %u", rcom.c_str(), argCnt);

            // process command
            if (rcom == RCD_MODE_CMD && argCnt >= 1U) {
                std::string mode = getArgString(args, 0U);
                // Command is in the form of: "mode <mode>"
                if (mode == RCD_MODE_OPT_IDLE) {
                    host->m_fixedMode = false;
                    host->setState(STATE_IDLE);
                    LogInfoEx(LOG_RCON, "Dynamic mode, mode %u", host->m_state);
                }
                else if (mode == RCD_MODE_OPT_LCKOUT) {
                    host->m_fixedMode = false;
                    host->setState(HOST_STATE_LOCKOUT);
                    LogInfoEx(LOG_RCON, "Lockout mode, mode %u", host->m_state);
                }
                else if (mode == RCD_MODE_OPT_FDMR) {
                    if (dmr != NULL) {
                        host->m_fixedMode = true;
                        host->setState(STATE_DMR);
                        LogInfoEx(LOG_RCON, "Fixed mode, mode %u", host->m_state);
                    }
                    else {
                        LogError(LOG_RCON, CMD_FAILED_STR "DMR mode is not enabled!");
                    }
                }
                else if (mode == RCD_MODE_OPT_FP25) {
                    if (p25 != NULL) {
                        host->m_fixedMode = true;
                        host->setState(STATE_P25);
                        LogInfoEx(LOG_RCON, "Fixed mode, mode %u", host->m_state);
                    }
                    else {
                        LogError(LOG_RCON, CMD_FAILED_STR "P25 mode is not enabled!");
                    }
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
                    LogError(LOG_RCON, INVALID_OPT_STR "tried to whitelist RID 0!");
                }
            }
            else if (rcom == RCD_RID_BLIST_CMD && argCnt >= 1U) {
                // Command is in the form of: "rid-blacklist <RID>"
                uint32_t srcId = getArgUInt32(args, 0U);
                if (srcId != 0U) {
                    m_ridLookup->toggleEntry(srcId, false);
                }
                else {
                    LogError(LOG_RCON, INVALID_OPT_STR "tried to blacklist RID 0!");
                }
            }
            else if (rcom == RCD_DMR_BEACON_CMD) {
                // Command is in the form of: "dmr-beacon"
                if (dmr != NULL) {
                    if (host->m_dmrBeacons) {
                        g_fireDMRBeacon = true;
                    }
                    else {
                        LogError(LOG_RCON, CMD_FAILED_STR "DMR beacons is not enabled!");
                    }
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "DMR mode is not enabled!");
                }
            }
            else if (rcom == RCD_P25_CC_CMD) {
                // Command is in the form of: "p25-cc"
                if (p25 != NULL) {
                    if (host->m_controlData) {
                        g_fireP25Control = true;
                    }
                    else {
                        LogError(LOG_RCON, CMD_FAILED_STR "P25 control data is not enabled!");
                    }
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "P25 mode is not enabled!");
                }
            }
            else if (rcom == RCD_DMRD_MDM_INJ_CMD && argCnt >= 1U) {
                // Command is in the form of: "dmrd-mdm-inj <slot> <bin file>
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
                                            LogError(LOG_RCON, CMD_FAILED_STR "invalid DMR slot!");
                                        }
                                    }
                                    else {
                                        LogError(LOG_RCON, CMD_FAILED_STR "DMR data has too many errors!");
                                    }
                                }
                                else {
                                    LogError(LOG_RCON, CMD_FAILED_STR "DMR failed to open DMR data!");
                                }

                                delete[] buffer;
                            }

                            ::fclose(file);
                        }
                    }
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "DMR mode is not enabled!");
                }
            }
            else if (rcom == RCD_P25D_MDM_INJ_CMD && argCnt >= 1U) {
                // Command is in the form of: "p25d-mdm-inj <bin file>
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
                                            LogError(LOG_RCON, CMD_FAILED_STR "P25 data did not contain a valid NID!");
                                        }
                                    }
                                    else {
                                        LogError(LOG_RCON, CMD_FAILED_STR "P25 data has too many errors!");
                                    }
                                }
                                else {
                                    LogError(LOG_RCON, CMD_FAILED_STR "P25 failed to open P25 data!");
                                }

                                delete[] buffer;
                            }

                            ::fclose(file);
                        }
                    }
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "P25 mode is not enabled!");
                }
            }
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
                            LogError(LOG_RCON, INVALID_OPT_STR "tried to DMR call alert RID 0!");
                        }
                    }
                    else {
                        LogError(LOG_RCON, INVALID_OPT_STR "invalid DMR slot number for call alert!");
                    }
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "DMR mode is not enabled!");
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
                            LogError(LOG_RCON, INVALID_OPT_STR "tried to DMR radio check RID 0!");
                        }
                    }
                    else {
                        LogError(LOG_RCON, INVALID_OPT_STR "invalid DMR slot number for radio check!");
                    }
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "DMR mode is not enabled!");
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
                            LogError(LOG_RCON, INVALID_OPT_STR "tried to DMR radio inhibit RID 0!");
                        }
                    }
                    else {
                        LogError(LOG_RCON, INVALID_OPT_STR "invalid DMR slot number for radio inhibit!");
                    }
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "DMR mode is not enabled!");
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
                            LogError(LOG_RCON, INVALID_OPT_STR "tried to DMR radio uninhibit RID 0!");
                        }
                    }
                    else {
                        LogError(LOG_RCON, INVALID_OPT_STR "invalid DMR slot number for radio uninhibit!");
                    }
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "DMR mode is not enabled!");
                }
            }
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
                    LogError(LOG_RCON, CMD_FAILED_STR "P25 mode is not enabled!");
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
                        LogError(LOG_RCON, INVALID_OPT_STR "tried to P25 call alert RID 0!");
                    }
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "P25 mode is not enabled!");
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
                        LogError(LOG_RCON, INVALID_OPT_STR "tried to P25 radio check RID 0!");
                    }
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "P25 mode is not enabled!");
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
                        LogError(LOG_RCON, INVALID_OPT_STR "tried to P25 inhibit RID 0!");
                    }
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "P25 mode is not enabled!");
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
                        LogError(LOG_RCON, INVALID_OPT_STR "tried to P25 uninhibit RID 0!");
                    }
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "P25 mode is not enabled!");
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
                        LogError(LOG_RCON, INVALID_OPT_STR "tried to P25 grp aff. query RID 0!");
                    }
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "P25 mode is not enabled!");
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
                        LogError(LOG_RCON, INVALID_OPT_STR "tried to P25 unit reg. command RID 0!");
                    }
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "P25 mode is not enabled!");
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
                        LogError(LOG_RCON, INVALID_OPT_STR "tried to add P25 group patch with no TGID?");
                    }
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "P25 mode is not enabled!");
                }
            }
            else if (rcom == RCD_P25_RELEASE_GRANTS_CMD) {
                // Command is in the form of: "p25-rel-grnts"
                if (p25 != NULL) {
                    p25->trunk()->releaseDstIdGrant(0, true);
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "P25 mode is not enabled!");
                }
            }
            else if (rcom == RCD_P25_RELEASE_AFFS_CMD) {
                // Command is in the form of: "p25-rel-affs <group>"
                if (p25 != NULL) {
                    uint32_t grp = getArgUInt32(args, 0U);

                    if (grp == 0) {
                        p25->trunk()->clearGrpAff(0, true);
                    }
                    else {
                        p25->trunk()->clearGrpAff(grp, false);
                    }
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "P25 mode is not enabled!");
                }
            }
            else if (rcom == RCD_DMR_CC_DEDICATED_CMD) {
                // Command is in the form of: "dmr-cc-dedicated"
                if (dmr != NULL) {
                    if (host->m_dmrTSCCData) {
                        if (p25 != NULL) {
                            LogError(LOG_RCON, CMD_FAILED_STR "Can't enable DMR control channel while P25 is enabled!");
                        }
                        else {
                            host->m_dmrCtrlChannel = !host->m_dmrCtrlChannel;
                            LogInfoEx(LOG_RCON, "DMR CC is %s", host->m_p25CtrlChannel ? "enabled" : "disabled");
                        }
                    }
                    else {
                        LogError(LOG_RCON, CMD_FAILED_STR "DMR control data is not enabled!");
                    }
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "DMR mode is not enabled!");
                }
            }
            else if (rcom == RCD_DMR_CC_BCAST_CMD) {
                // Command is in the form of: "dmr-cc-bcast"
                if (dmr != NULL) {
                    host->m_dmrTSCCData = !host->m_dmrTSCCData;
                    LogInfoEx(LOG_RCON, "DMR CC broadcast is %s", host->m_dmrTSCCData ? "enabled" : "disabled");
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "DMR mode is not enabled!");
                }
            }
            else if (rcom == RCD_P25_CC_DEDICATED_CMD) {
                // Command is in the form of: "p25-cc-dedicated"
                if (p25 != NULL) {
                    if (host->m_controlData) {
                        if (dmr != NULL) {
                            LogError(LOG_RCON, CMD_FAILED_STR "Can't enable P25 control channel while DMR is enabled!");
                        }
                        else {
                            host->m_p25CtrlChannel = !host->m_p25CtrlChannel;
                            host->m_p25CtrlBroadcast = true;
                            g_fireP25Control = true;
                            g_interruptP25Control = false;

                            LogInfoEx(LOG_RCON, "P25 CC is %s", host->m_p25CtrlChannel ? "enabled" : "disabled");
                        }
                    }
                    else {
                        LogError(LOG_RCON, CMD_FAILED_STR "P25 control data is not enabled!");
                    }
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "P25 mode is not enabled!");
                }
            }
            else if (rcom == RCD_P25_CC_BCAST_CMD) {
                // Command is in the form of: "p25-cc-bcast"
                if (p25 != NULL) {
                    if (host->m_controlData) {
                        host->m_p25CtrlBroadcast = !host->m_p25CtrlBroadcast;

                        if (!host->m_p25CtrlBroadcast) {
                            g_fireP25Control = false;
                            g_interruptP25Control = true;
                        }
                        else {
                            g_fireP25Control = true;
                            g_interruptP25Control = false;
                        }

                        LogInfoEx(LOG_RCON, "P25 CC broadcast is %s", host->m_p25CtrlBroadcast ? "enabled" : "disabled");
                    }
                    else {
                        LogError(LOG_RCON, CMD_FAILED_STR "P25 control data is not enabled!");
                    }
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "P25 mode is not enabled!");
                }
            }
            else if (rcom == RCD_DMR_DEBUG) {
                // Command is in the form of: "dmr-debug <debug 0/1> <trace 0/1>"
                uint8_t debug = getArgUInt8(args, 0U);
                uint8_t verbose = getArgUInt8(args, 1U);
                if (dmr != NULL) {
                    dmr->setDebugVerbose((debug == 1U) ? true : false, (verbose == 1U) ? true : false);
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "DMR mode is not enabled!");
                }
            }
            else if (rcom == RCD_P25_DEBUG) {
                // Command is in the form of: "p25-debug <debug 0/1> <trace 0/1>"
                uint8_t debug = getArgUInt8(args, 0U);
                uint8_t verbose = getArgUInt8(args, 1U);
                if (p25 != NULL) {
                    p25->setDebugVerbose((debug == 1U) ? true : false, (verbose == 1U) ? true : false);
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "P25 mode is not enabled!");
                }
            }
            else if (rcom == RCD_P25_DUMP_TSBK) {
                // Command is in the form of: "p25-dump-tsbk 0/1"
                uint8_t verbose = getArgUInt8(args, 0U);
                if (p25 != NULL) {
                    p25->trunk()->setTSBKVerbose((verbose == 1U) ? true : false);
                }
                else {
                    LogError(LOG_RCON, CMD_FAILED_STR "P25 mode is not enabled!");
                }
            }
            else {
                args.clear();
                LogError(LOG_RCON, BAD_CMD_STR " (\"%s\")", rcom.c_str());
            }
        }
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
