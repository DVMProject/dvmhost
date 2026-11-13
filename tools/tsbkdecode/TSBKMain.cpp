// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - TSBK/CSBK Decoder
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "Decode.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>
#include <cctype>

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

#define IS(s) (::strcmp(argv[i], s) == 0)

// ---------------------------------------------------------------------------
//  Global Variables
// ---------------------------------------------------------------------------

std::string g_progExe = std::string(__EXE_NAME__);
std::string g_protocol;
bool g_frame = false;
bool g_isp = false;
std::vector<uint8_t> g_bytes;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/* Helper to print a fatal error message and exit. */

static void fatal(const char* message)
{
    ::fprintf(stderr, "%s: %s\n", g_progExe.c_str(), message);
    exit(EXIT_FAILURE);
}

/* Helper to print version information. */

static void version()
{
    ::fprintf(stdout, __PROG_NAME__ " %s (built %s)\r\n", __VER__, __BUILD__);
    ::fprintf(stdout, "Copyright (c) 2017-2026 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\n");
    ::fprintf(stdout, "Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others\n");
    ::fprintf(stdout, HIGHLY_UNNECESSARY_DISCLAIMER_FOR_THE_MENTAL "\n\n");
}

/* Helper to print usage and optionally an error. */

static void usage(const char* message, const char* arg)
{
    version();
    if (message != nullptr) {
        ::fprintf(stderr, "%s: ", g_progExe.c_str());
        ::fprintf(stderr, message, arg);
        ::fprintf(stderr, "\n\n");
    }

    ::fprintf(stdout,
        "usage: %s <tsbk|csbk> [--frame] [--isp] <hex bytes...>\n\n"
        "  -v, --version  show version information\n"
        "  -h, --help     show this screen\n"
        "  --frame        45-byte P25 TSDU or 33-byte DMR CSBK burst, without modem tags\n"
        "  --isp          interpret P25 as inbound subscriber signalling (default: outbound)\n"
        "  --             stop handling options\n\n"
        "  Raw: 10 bytes (CRC generated) or 12 bytes (CRC checked).\n"
        "  CRC failures warn and decoding continues, including with --frame.\n"
        "  Hex: contiguous, spaced, colon/comma separated, or 0x-prefixed bytes.\n"
        "  CSBK CRC bytes must include the standard A5A5 mask.\n"
        "Example: %s tsbk 80 00 04 10 23 12 34 00 00 01\n",
        g_progExe.c_str(), g_progExe.c_str());
    exit(message == nullptr ? EXIT_SUCCESS : EXIT_FAILURE);
}

/* Helper to parse hexadecimal bytes from a command line argument. */

static void parseHex(const char* value)
{
    const std::string arg(value);
    size_t pos = 0;
    while (pos < arg.size()) {
        if (std::isspace(static_cast<unsigned char>(arg[pos])) || arg[pos] == ':' || arg[pos] == ',') {
            ++pos;
            continue;
        }

        if (arg.compare(pos, 2, "0x") == 0 || arg.compare(pos, 2, "0X") == 0)
            pos += 2;

        if (pos + 2 > arg.size() || !std::isxdigit(static_cast<unsigned char>(arg[pos])) ||
            !std::isxdigit(static_cast<unsigned char>(arg[pos + 1])))
            throw std::invalid_argument("Hex input must contain complete two-digit bytes.");

        g_bytes.push_back(static_cast<uint8_t>(std::stoul(arg.substr(pos, 2), nullptr, 16)));
        pos += 2;
    }
}

/* Helper to validate the command line arguments. */

static void checkArgs(int argc, char* argv[])
{
    bool options = true;

    // iterate through arguments
    for (int i = 1; i < argc; i++) {
        if (options && *argv[i] == '-') {
            if (IS("--")) {
                options = false;
            }
            else if (IS("--frame")) {
                g_frame = true;
            }
            else if (IS("--isp")) {
                g_isp = true;
            }
            else if (IS("-v") || IS("--version")) {
                version();
                if (argc == 2)
                    exit(EXIT_SUCCESS);
            }
            else if (IS("-h") || IS("--help")) {
                usage(nullptr, nullptr);
            }
            else {
                usage("unrecognized option `%s'", argv[i]);
            }
        }
        else if (g_protocol.empty()) {
            g_protocol = std::string(argv[i]);
            if (g_protocol != "tsbk" && g_protocol != "csbk")
                usage("error: %s", "Protocol must be tsbk or csbk.");
        }
        else {
            parseHex(argv[i]);
        }
    }

    if (g_protocol.empty())
        usage("error: %s", "must specify the protocol (tsbk or csbk)");
    if (g_bytes.empty())
        usage("error: %s", "must specify hex bytes to decode");
}

// ---------------------------------------------------------------------------
//  Program Entry Point
// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
    if (argv[0] != nullptr && *argv[0] != 0)
        g_progExe = std::string(argv[0]);

    try {
        // check arguments
        checkArgs(argc, argv);

        return decodeBlock(g_bytes, g_protocol == "csbk", g_frame, g_isp) ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception& error) {
        fatal(error.what());
    }

    return EXIT_FAILURE;
}
