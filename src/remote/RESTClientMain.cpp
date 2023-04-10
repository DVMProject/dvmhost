/**
* Digital Voice Modem - Remote Command Client
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Remote Command Client
*
*/
/*
*   Copyright (C) 2023 by Bryan Biedenkapp <gatekeep@gmail.com>
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
#include "remote/RESTClient.h"
#include "network/RESTDefines.h"
#include "Thread.h"
#include "Log.h"
#include "Utils.h"

#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <vector>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#undef __PROG_NAME__
#define __PROG_NAME__ "Digital Voice Modem (DVM) REST API Tool"
#undef __EXE_NAME__
#define __EXE_NAME__ "dvmcmd"

#define ERRNO_REMOTE_CMD 99

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define RCD_GET_VERSION                 "version"
#define RCD_GET_HELP                    "help"
#define RCD_GET_STATUS                  "status"
#define RCD_GET_VOICE_CH                "voice-ch"

#define RCD_MODE                        "mdm-mode"
#define RCD_MODE_OPT_IDLE               "idle"
#define RCD_MODE_OPT_LCKOUT             "lockout"
#define RCD_MODE_OPT_FDMR               "dmr"
#define RCD_MODE_OPT_FP25               "p25"
#define RCD_MODE_OPT_FNXDN              "nxdn"

#define RCD_KILL                        "mdm-kill"
#define RCD_FORCE_KILL                  "mdm-force-kill"

#define RCD_PERMIT_TG                   "permit-tg"
#define RCD_GRANT_TG                    "grant-tg"

#define RCD_RID_WLIST                   "rid-whitelist"
#define RCD_RID_BLIST                   "rid-blacklist"

#define RCD_RELEASE_GRANTS              "rel-grnts"
#define RCD_RELEASE_AFFS                "rel-affs"

#define RCD_DMR_BEACON                  "dmr-beacon"
#define RCD_P25_CC                      "p25-cc"
#define RCD_P25_CC_FALLBACK             "p25-cc-fallback"
#define RCD_NXDN_CC                     "nxdn-cc"

#define RCD_DMR_RID_PAGE                "dmr-rid-page"
#define RCD_DMR_RID_CHECK               "dmr-rid-check"
#define RCD_DMR_RID_INHIBIT             "dmr-rid-inhibit"
#define RCD_DMR_RID_UNINHIBIT           "dmr-rid-uninhibit"

#define RCD_P25_SET_MFID                "p25-set-mfid"
#define RCD_P25_RID_PAGE                "p25-rid-page"
#define RCD_P25_RID_CHECK               "p25-rid-check"
#define RCD_P25_RID_INHIBIT             "p25-rid-inhibit"
#define RCD_P25_RID_UNINHIBIT           "p25-rid-uninhibit"
#define RCD_P25_RID_GAQ                 "p25-rid-gaq"
#define RCD_P25_RID_UREG                "p25-rid-ureg"
#define RCD_P25_RID_EMERG               "p25-rid-emerg"

#define RCD_DMR_CC_DEDICATED            "dmr-cc-dedicated"
#define RCD_DMR_CC_BCAST                "dmr-cc-bcast"

#define RCD_P25_CC_DEDICATED            "p25-cc-dedicated"
#define RCD_P25_CC_BCAST                "p25-cc-bcast"

#define RCD_NXDN_CC_DEDICATED           "nxdn-cc-dedicated"

#define RCD_DMR_DEBUG                   "dmr-debug"
#define RCD_DMR_DUMP_CSBK               "dmr-dump-csbk"
#define RCD_P25_DEBUG                   "p25-debug"
#define RCD_P25_DUMP_TSBK               "p25-dump-tsbk"
#define RCD_NXDN_DEBUG                  "nxdn-debug"
#define RCD_NXDN_DUMP_RCCH              "nxdn-dump-rcch"

#define BAD_CMD_STR                     "Bad or invalid remote command"
#define NO_DATA_CMD_STR                 "No data"
#define INVALID_AUTH_STR                "Invalid authentication"
#define INVALID_OPT_STR                 "Invalid command arguments, "

// ---------------------------------------------------------------------------
//	Macros
// ---------------------------------------------------------------------------

#define IS(s) (::strcmp(argv[i], s) == 0)

// ---------------------------------------------------------------------------
//  Global Variables
// ---------------------------------------------------------------------------

static std::string g_progExe = std::string(__EXE_NAME__);
static std::string g_remoteAddress = std::string("127.0.0.1");
static uint32_t g_remotePort = REST_API_DEFAULT_PORT;
static std::string g_remotePassword = std::string();
static bool g_debug = false;

// ---------------------------------------------------------------------------
//	Global Functions
// ---------------------------------------------------------------------------

void fatal(const char* message)
{
    ::fprintf(stderr, "%s: %s\n", g_progExe.c_str(), message);
    exit(EXIT_FAILURE);
}

void usage(const char* message, const char* arg)
{
    ::fprintf(stdout, __PROG_NAME__ " %s (built %s)\r\n", __VER__, __BUILD__);
    ::fprintf(stdout, "Copyright (c) 2023 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\n");
    ::fprintf(stdout, "Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others\n\n");
    if (message != nullptr) {
        ::fprintf(stderr, "%s: ", g_progExe.c_str());
        ::fprintf(stderr, message, arg);
        ::fprintf(stderr, "\n\n");
    }

    ::fprintf(stdout, "usage: %s [-dvh] [-a <address>] [-p <port>] [-P <password>] <command>\n\n"
        "  -a       remote modem command address\n"
        "  -p       remote modem command port\n"
        "  -P       remote modem authentication password\n"
        "\n"
        "  -d       enable debug\n"
        "  -v       show version information\n"
        "  -h       show this screen\n"
        "  --       stop handling options\n",
        g_progExe.c_str());
    exit(EXIT_FAILURE);
}

int checkArgs(int argc, char* argv[])
{
    int i, p = 0;

    // iterate through arguments
    for (i = 1; i <= argc; i++)
    {
        if (argv[i] == nullptr) {
            break;
        }

        if (*argv[i] != '-') {
            continue;
        }
        else if (IS("--")) {
            ++p;
            break;
        }
        else if (IS("-a")) {
            if ((argc - 1) <= 0)
                usage("error: %s", "must specify the address to connect to");
            g_remoteAddress = std::string(argv[++i]);

            if (g_remoteAddress == "")
                usage("error: %s", "remote address cannot be blank!");

            p += 2;
        }
        else if (IS("-p")) {
            if ((argc - 1) <= 0)
                usage("error: %s", "must specify the port to connect to");
            g_remotePort = (uint32_t)::atoi(argv[++i]);

            if (g_remotePort == 0)
                usage("error: %s", "remote port number cannot be blank or 0!");

            p += 2;
        }
        else if (IS("-P")) {
            if ((argc - 1) <= 0)
                usage("error: %s", "must specify the auth password");
            g_remotePassword = std::string(argv[++i]);

            if (g_remotePassword == "")
                usage("error: %s", "remote auth password cannot be blank!");

            p += 2;
        }
        else if (IS("-d")) {
            ++p;
            g_debug = true;
        }
        else if (IS("-v")) {
            ::fprintf(stdout, __PROG_NAME__ " %s (built %s)\r\n", __VER__, __BUILD__);
            ::fprintf(stdout, "Copyright (c) 2017-2023 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\r\n");
            if (argc == 2)
                exit(EXIT_SUCCESS);
        }
        else if (IS("-h")) {
            usage(nullptr, nullptr);
            if (argc == 2)
                exit(EXIT_SUCCESS);
        }
        else {
            usage("unrecognized option `%s'", argv[i]);
        }
    }

    if (p < 0 || p > argc) {
        p = 0;
    }

    return ++p;
}

/// <summary></summary>
std::string getArgString(std::vector<std::string> args, uint32_t n)
{
    n += 1;
    if (n >= args.size())
        return "";

    return args.at(n);
}

/// <summary></summary>
uint64_t getArgUInt64(std::vector<std::string> args, uint32_t n) { return (uint64_t)::atol(getArgString(args, n).c_str()); }
/// <summary></summary>
uint32_t getArgUInt32(std::vector<std::string> args, uint32_t n) { return (uint32_t)::atoi(getArgString(args, n).c_str()); }
/// <summary></summary>
int32_t getArgInt32(std::vector<std::string> args, uint32_t n) { return ::atoi(getArgString(args, n).c_str()); }
/// <summary></summary>
uint16_t getArgUInt16(std::vector<std::string> args, uint32_t n) { return (uint16_t)::atoi(getArgString(args, n).c_str()); }
/// <summary></summary>
int16_t getArgInt16(std::vector<std::string> args, uint32_t n) { return (int16_t)::atoi(getArgString(args, n).c_str()); }
/// <summary></summary>
uint8_t getArgUInt8(std::vector<std::string> args, uint32_t n) { return (uint8_t)::atoi(getArgString(args, n).c_str()); }
/// <summary></summary>
int8_t getArgInt8(std::vector<std::string> args, uint32_t n) { return (int8_t)::atoi(getArgString(args, n).c_str()); }

/// <summary>
/// Helper to print the remote control help.
/// </summary>
/// <returns></returns>
std::string displayHelp()
{
    std::string reply = "";

    reply += __PROG_NAME__ " " __VER__ " (built " __BUILD__ ")\r\n";
    reply += "Copyright (c) 2017-2023 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\r\n\r\n";

    reply += "RCON Help\r\nGeneral Commands:\r\n";
    reply += "  version                 Display current version of host\r\n";
    reply += "  help                    Displays RCON help\r\n";
    reply += "  status                  Display current settings and operation mode\r\n";
    reply += "  voice-ch                Retrieves the list of configured voice channels\r\n";
    reply += "\r\n";
    reply += "  mdm-mode <mode>         Set current mode of host (idle, lockout, dmr, p25, nxdn)\r\n";
    reply += "  mdm-kill                Causes the host to quit\r\n";
    reply += "  mdm-force-kill          Causes the host to quit immediately\r\n";
    reply += "\r\n";
    reply += "  permit-tg <state> <dstid> Causes the host to permit the specified destination ID if non-authoritative\r\n";
    reply += "  grant-tg <state> <dstid> <uu> Causes the host to grant the specified destination ID if non-authoritative\r\n";
    reply += "\r\n";
    reply += "  rid-whitelist <rid>     Whitelists the specified RID in the host ACL tables\r\n";
    reply += "  rid-blacklist <rid>     Blacklists the specified RID in the host ACL tables\r\n";
    reply += "\r\n";
    reply += "  rel-grnts               Forcibly releases all channel grants\r\n";
    reply += "  rel-affs                Forcibly releases all group affiliations\r\n";
    reply += "\r\n";
    reply += "  dmr-beacon              Transmits a DMR beacon burst\r\n";
    reply += "  p25-cc                  Transmits a non-continous P25 CC burst\r\n";
    reply += "  p25-cc-fallback <0/1>   Sets the P25 CC into conventional fallback mode\r\n";
    reply += "  nxdn-cc                 Transmits a non-continous NXDN CC burst\r\n";
    reply += "\r\n";
    reply += "  dmr-debug <debug 0/1> <verbose 0/1>\r\n";
    reply += "  dmr-dump-csbk <0/1>\r\n";
    reply += "  p25-debug <debug 0/1> <verbose 0/1>\r\n";
    reply += "  p25-dump-tsbk <0/1>\r\n";
    reply += "  nxdn-debug <debug 0/1> <verbose 0/1>\r\n";
    reply += "  nxdn-dump-rcch <0/1>\r\n";
    reply += "\r\nDMR Commands:\r\n";
    reply += "  dmr-rid-page <rid>      Pages/Calls the specified RID\r\n";
    reply += "  dmr-rid-check <rid>     Radio Checks the specified RID\r\n";
    reply += "  dmr-rid-inhibit <rid>   Inhibits the specified RID\r\n";
    reply += "  dmr-rid-uninhibit <rid> Uninhibits the specified RID\r\n";
    reply += "\r\n";
    reply += "  dmr-cc-dedicated        Enables or disables dedicated control channel\r\n";
    reply += "  dmr-cc-bcast            Enables or disables broadcast of the control channel\r\n";
    reply += "\r\nP25 Commands:\r\n";
    reply += "  p25-set-mfid <mfid>     Sets the P25 MFId for the next sent P25 command\r\n";
    reply += "  p25-rid-page <rid>      Pages/Calls the specified RID\r\n";
    reply += "  p25-rid-check <rid>     Radio Checks the specified RID\r\n";
    reply += "  p25-rid-inhibit <rid>   Inhibits the specified RID\r\n";
    reply += "  p25-rid-uninhibit <rid> Uninhibits the specified RID\r\n";
    reply += "  p25-rid-gaq <rid>       Group affiliation queries the specified RID\r\n";
    reply += "  p25-rid-ureg <rid>      Demand unit registration for the specified RID\r\n";
    reply += "  p25-rid-emerg <src> <dst> Send emergency from the specified RID to the specified RID\r\n";
    reply += "\r\n";
    reply += "  p25-cc-dedicated        Enables or disables dedicated control channel\r\n";
    reply += "  p25-cc-bcast            Enables or disables broadcast of the control channel\r\n";
    reply += "\r\nNXDN Commands:\r\n";
    reply += "  nxdn-cc-dedicated       Enables or disables dedicated control channel\r\n";

    return reply;
}

// ---------------------------------------------------------------------------
//  Program Entry Point
// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
    if (argv[0] != nullptr && *argv[0] != 0)
        g_progExe = std::string(argv[0]);

    if (argc < 2) {
        usage("error: %s", "must specify the remote command!");
        return ERRNO_REMOTE_CMD;
    }

    if (argc > 1) {
        // check arguments
        int i = checkArgs(argc, argv);
        if (i < argc) {
            argc -= i;
            argv += i;
        }
        else {
            argc--;
            argv++;
        }
    }

    // process command
    std::string cmd = std::string(argv[0]);
    for (int i = 1; i < argc; i++) {
        cmd += " ";
        cmd += std::string(argv[i]);
    }

    // initialize system logging
    bool ret = ::LogInitialise("", "", 0U, 1U, true);
    if (!ret) {
        ::fprintf(stderr, "unable to open the log file\n");
        return 1;
    }

    if (g_remotePassword.empty()) {
        ::fprintf(stderr, "must specify password!\n");
        return 1;
    }

    RESTClient* client = new RESTClient(g_remoteAddress, g_remotePort, g_remotePassword, g_debug);
    int retCode = EXIT_SUCCESS;

    std::vector<std::string> args = std::vector<std::string>();
    args.clear();

    // parse the original command into a vector of strings.
    char* b = const_cast<char*>(cmd.c_str());
    char* p = NULL;
    while ((p = ::strtok(b, " ")) != NULL) {
        b = NULL;
        args.push_back(std::string(p));
    }

    if (args.size() < 1 || args.at(0U) == "") {
        args.clear();
        LogWarning(LOG_REST, BAD_CMD_STR);
    }
    else {
        std::string rcom = args.at(0);
        uint32_t argCnt = args.size() - 1;
        if (g_debug) {
            LogInfoEx(LOG_REST, "cmd = %s, argCnt = %u", rcom.c_str(), argCnt);
        }

        // determine command to execute
        if (rcom == RCD_GET_HELP) {
            std::string help = displayHelp();
            fprintf(stdout, "%s", help.c_str());
        }
        else if (rcom == RCD_GET_VERSION) {
            retCode = client->send(HTTP_GET, GET_VERSION, json::object());
        }
        else if (rcom == RCD_GET_STATUS) {
            retCode = client->send(HTTP_GET, GET_STATUS, json::object());
        }
        else if (rcom == RCD_GET_VOICE_CH) {
            retCode = client->send(HTTP_GET, GET_VOICE_CH, json::object());
        }
        else if (rcom == RCD_MODE && argCnt >= 1U) {
            std::string mode = getArgString(args, 0U);

            json::object req = json::object();
            req["mode"].set<std::string>(mode);

            retCode = client->send(HTTP_PUT, PUT_MDM_MODE, req);
        }
        else if (rcom == RCD_KILL) {
            json::object req = json::object();
            bool force = false;
            req["force"].set<bool>(force);

            retCode = client->send(HTTP_PUT, PUT_MDM_KILL, req);
        }
        else if (rcom == RCD_FORCE_KILL) {
            json::object req = json::object();
            bool force = true;
            req["force"].set<bool>(force);

            retCode = client->send(HTTP_PUT, PUT_MDM_KILL, req);
        }
        else if (rcom == RCD_PERMIT_TG && argCnt >= 1U) {
            json::object req = json::object();
            int state = getArgInt32(args, 0U);
            req["state"].set<int>(state);

            uint32_t dstId = getArgInt32(args, 1U);
            req["dstId"].set<uint32_t>(dstId);

            if (state == 1) {
                uint8_t slot = getArgInt8(args, 2U);
                req["slot"].set<uint8_t>(slot);
            }

            retCode = client->send(HTTP_PUT, PUT_PERMIT_TG, req);
        }
        else if (rcom == RCD_GRANT_TG && argCnt >= 1U) {
            json::object req = json::object();
            int state = getArgInt32(args, 0U);
            req["state"].set<int>(state);

            uint32_t dstId = getArgInt32(args, 1U);
            req["dstId"].set<uint32_t>(dstId);

            uint8_t unitToUnit = getArgInt32(args, 2U);
            if (unitToUnit == 0U) {
                bool b = false;
                req["unitToUnit"].set<bool>(b);
            } else {
                bool b = true;
                req["unitToUnit"].set<bool>(b);
            }

            if (state == 1) {
                uint8_t slot = getArgInt8(args, 3U);
                req["slot"].set<uint8_t>(slot);
            }

            retCode = client->send(HTTP_PUT, PUT_GRANT_TG, req);
        }
        else if (rcom == RCD_RID_WLIST && argCnt >= 1U) {
            uint32_t srcId = getArgUInt32(args, 0U);
            retCode = client->send(HTTP_GET, GET_RID_WHITELIST_BASE + std::to_string(srcId), json::object());
        }
        else if (rcom == RCD_RID_BLIST && argCnt >= 1U) {
            uint32_t srcId = getArgUInt32(args, 0U);
            retCode = client->send(HTTP_GET, GET_RID_BLACKLIST_BASE + std::to_string(srcId), json::object());
        }
        else if (rcom == RCD_RELEASE_GRANTS) {
            retCode = client->send(HTTP_GET, GET_RELEASE_GRNTS, json::object());
        }
        else if (rcom == RCD_RELEASE_AFFS) {
            retCode = client->send(HTTP_GET, GET_RELEASE_AFFS, json::object());
        }

        /*
        ** Digital Mobile Radio
        */

        else if (rcom == RCD_DMR_BEACON) {
            retCode = client->send(HTTP_GET, GET_DMR_BEACON, json::object());
        }
        else if (rcom == RCD_DMR_DEBUG) {
            if (argCnt < 2U) {
                retCode = client->send(HTTP_GET, GET_DMR_DEBUG_BASE, json::object());
            }
            else {
                uint8_t debug = getArgUInt8(args, 0U);
                uint8_t verbose = getArgUInt8(args, 1U);
                retCode = client->send(HTTP_GET, GET_DMR_DEBUG_BASE + std::to_string(debug) + "/" + std::to_string(verbose), json::object());
            }
        }
        else if (rcom == RCD_DMR_DUMP_CSBK) {
            if (argCnt < 1U) {
                retCode = client->send(HTTP_GET, GET_DMR_DUMP_CSBK_BASE, json::object());
            }
            else {
                uint8_t verbose = getArgUInt8(args, 0U);
                retCode = client->send(HTTP_GET, GET_DMR_DUMP_CSBK_BASE + std::to_string(verbose), json::object());
            }
        }
        else if (rcom == RCD_DMR_RID_PAGE && argCnt >= 2U) {
            json::object req = json::object();
            req["command"].set<std::string>(std::string(RID_CMD_PAGE));
            uint8_t slotNo = getArgInt8(args, 0U);
            req["slot"].set<uint8_t>(slotNo);
            uint32_t dstId = getArgUInt32(args, 1U);
            req["dstId"].set<uint32_t>(dstId);

            retCode = client->send(HTTP_PUT, PUT_DMR_RID, req);
        }
        else if (rcom == RCD_DMR_RID_CHECK && argCnt >= 2U) {
            json::object req = json::object();
            req["command"].set<std::string>(std::string(RID_CMD_CHECK));
            uint8_t slotNo = getArgInt8(args, 0U);
            req["slot"].set<uint8_t>(slotNo);
            uint32_t dstId = getArgUInt32(args, 1U);
            req["dstId"].set<uint32_t>(dstId);

            retCode = client->send(HTTP_PUT, PUT_DMR_RID, req);
        }
        else if (rcom == RCD_DMR_RID_INHIBIT && argCnt >= 2U) {
            json::object req = json::object();
            req["command"].set<std::string>(std::string(RID_CMD_INHIBIT));
            uint8_t slotNo = getArgInt8(args, 0U);
            req["slot"].set<uint8_t>(slotNo);
            uint32_t dstId = getArgUInt32(args, 1U);
            req["dstId"].set<uint32_t>(dstId);

            retCode = client->send(HTTP_PUT, PUT_DMR_RID, req);
        }
        else if (rcom == RCD_DMR_RID_UNINHIBIT && argCnt >= 2U) {
            json::object req = json::object();
            req["command"].set<std::string>(std::string(RID_CMD_UNINHIBIT));
            uint8_t slotNo = getArgInt8(args, 0U);
            req["slot"].set<uint8_t>(slotNo);
            uint32_t dstId = getArgUInt32(args, 1U);
            req["dstId"].set<uint32_t>(dstId);

            retCode = client->send(HTTP_PUT, PUT_DMR_RID, req);
        }
        else if (rcom == RCD_DMR_CC_DEDICATED) {
            retCode = client->send(HTTP_GET, GET_DMR_CC_DEDICATED, json::object());
        }
        else if (rcom == RCD_DMR_CC_BCAST) {
            retCode = client->send(HTTP_GET, GET_DMR_CC_BCAST, json::object());
        }

        /*
        ** Project 25
        */

        else if (rcom == RCD_P25_CC) {
            retCode = client->send(HTTP_GET, GET_P25_CC, json::object());
        }
        else if (rcom == RCD_P25_DEBUG) {
            if (argCnt < 2U) {
                retCode = client->send(HTTP_GET, GET_P25_DEBUG_BASE, json::object());
            }
            else {
                uint8_t debug = getArgUInt8(args, 0U);
                uint8_t verbose = getArgUInt8(args, 1U);
                retCode = client->send(HTTP_GET, GET_P25_DEBUG_BASE + std::to_string(debug) + "/" + std::to_string(verbose), json::object());
            }
        }
        else if (rcom == RCD_P25_DUMP_TSBK) {
            if (argCnt < 1U) {
                retCode = client->send(HTTP_GET, GET_P25_DUMP_TSBK_BASE, json::object());
            }
            else {
                uint8_t verbose = getArgUInt8(args, 0U);
                retCode = client->send(HTTP_GET, GET_P25_DUMP_TSBK_BASE + std::to_string(verbose), json::object());
            }
        }
        else if (rcom == RCD_P25_SET_MFID && argCnt >= 1U) {
            json::object req = json::object();
            req["command"].set<std::string>(std::string(RID_CMD_P25_SET_MFID));
            uint8_t mfId = getArgUInt8(args, 0U);
            req["mfId"].set<uint8_t>(mfId);

            retCode = client->send(HTTP_PUT, PUT_P25_RID, req);
        }
        else if (rcom == RCD_P25_RID_PAGE && argCnt >= 1U) {
            json::object req = json::object();
            req["command"].set<std::string>(std::string(RID_CMD_PAGE));
            uint32_t dstId = getArgUInt32(args, 0U);
            req["dstId"].set<uint32_t>(dstId);

            retCode = client->send(HTTP_PUT, PUT_P25_RID, req);
        }
        else if (rcom == RCD_P25_RID_CHECK && argCnt >= 1U) {
            json::object req = json::object();
            req["command"].set<std::string>(std::string(RID_CMD_CHECK));
            uint32_t dstId = getArgUInt32(args, 0U);
            req["dstId"].set<uint32_t>(dstId);

            retCode = client->send(HTTP_PUT, PUT_P25_RID, req);
        }
        else if (rcom == RCD_P25_RID_INHIBIT && argCnt >= 1U) {
            json::object req = json::object();
            req["command"].set<std::string>(std::string(RID_CMD_INHIBIT));
            uint32_t dstId = getArgUInt32(args, 0U);
            req["dstId"].set<uint32_t>(dstId);

            retCode = client->send(HTTP_PUT, PUT_P25_RID, req);
        }
        else if (rcom == RCD_P25_RID_UNINHIBIT && argCnt >= 1U) {
            json::object req = json::object();
            req["command"].set<std::string>(std::string(RID_CMD_UNINHIBIT));
            uint32_t dstId = getArgUInt32(args, 0U);
            req["dstId"].set<uint32_t>(dstId);

            retCode = client->send(HTTP_PUT, PUT_P25_RID, req);
        }
        else if (rcom == RCD_P25_RID_GAQ && argCnt >= 1U) {
            json::object req = json::object();
            req["command"].set<std::string>(std::string(RID_CMD_GAQ));
            uint32_t dstId = getArgUInt32(args, 0U);
            req["dstId"].set<uint32_t>(dstId);

            retCode = client->send(HTTP_PUT, PUT_P25_RID, req);
        }
        else if (rcom == RCD_P25_RID_UREG && argCnt >= 1U) {
            json::object req = json::object();
            req["command"].set<std::string>(std::string(RID_CMD_UREG));
            uint32_t dstId = getArgUInt32(args, 0U);
            req["dstId"].set<uint32_t>(dstId);

            retCode = client->send(HTTP_PUT, PUT_P25_RID, req);
        }
        else if (rcom == RCD_P25_RID_EMERG && argCnt >= 2U) {
            json::object req = json::object();
            req["command"].set<std::string>(std::string(RID_CMD_EMERG));
            uint32_t dstId = getArgUInt32(args, 0U);
            req["dstId"].set<uint32_t>(dstId);
            uint32_t srcId = getArgUInt32(args, 1U);
            req["srcId"].set<uint32_t>(srcId);

            retCode = client->send(HTTP_PUT, PUT_P25_RID, req);
        }
        else if (rcom == RCD_P25_CC_DEDICATED) {
            retCode = client->send(HTTP_GET, GET_P25_CC_DEDICATED, json::object());
        }
        else if (rcom == RCD_P25_CC_BCAST) {
            retCode = client->send(HTTP_GET, GET_P25_CC_BCAST, json::object());
        }

        /*
        ** Next Generation Digital Narrowband
        */

        else if (rcom == RCD_NXDN_CC) {
            retCode = client->send(HTTP_GET, GET_NXDN_CC, json::object());
        }
        else if (rcom == RCD_NXDN_DEBUG) {
            if (argCnt < 2U) {
                retCode = client->send(HTTP_GET, GET_NXDN_DEBUG_BASE, json::object());
            }
            else {
                uint8_t debug = getArgUInt8(args, 0U);
                uint8_t verbose = getArgUInt8(args, 1U);
                retCode = client->send(HTTP_GET, GET_NXDN_DEBUG_BASE + std::to_string(debug) + "/" + std::to_string(verbose), json::object());
            }
        }
        else if (rcom == RCD_NXDN_DUMP_RCCH) {
            if (argCnt < 1U) {
                retCode = client->send(HTTP_GET, GET_NXDN_DUMP_RCCH_BASE, json::object());
            }
            else {
                uint8_t verbose = getArgUInt8(args, 0U);
                retCode = client->send(HTTP_GET, GET_NXDN_DUMP_RCCH_BASE + std::to_string(verbose), json::object());
            }
        }
        else if (rcom == RCD_NXDN_CC_DEDICATED) {
            retCode = client->send(HTTP_GET, GET_NXDN_CC_DEDICATED, json::object());
        }

        else {
            args.clear();
            LogError(LOG_REST, BAD_CMD_STR " (\"%s\")", rcom.c_str());
        }
    }

    ::LogFinalise();
    return retCode;
}
