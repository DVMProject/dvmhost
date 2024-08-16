// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Bridge
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/Log.h"
#include "bridge/ActivityLog.h"
#include "BridgeMain.h"
#include "HostBridge.h"

using namespace network;
using namespace lookups;

#include <cstdio>
#include <cstdarg>
#include <vector>

#include <signal.h>

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

#define IS(s) (::strcmp(argv[i], s) == 0)

// ---------------------------------------------------------------------------
//  Global Variables
// ---------------------------------------------------------------------------

int g_signal = 0;
std::string g_progExe = std::string(__EXE_NAME__);
std::string g_iniFile = std::string(DEFAULT_CONF_FILE);
std::string g_lockFile = std::string(DEFAULT_LOCK_FILE);

bool g_foreground = false;
bool g_killed = false;
bool g_hideMessages = false;

int g_inputDevice = -1;
int g_outputDevice = -1;

uint8_t* g_gitHashBytes = nullptr;

#ifdef _WIN32
ma_backend g_backends[3];
ma_uint32 g_backendCnt = 3;
#else
ma_backend g_backends[7];
ma_uint32 g_backendCnt = 7;
#endif


// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

#if !defined(CATCH2_TEST_COMPILATION)
/* Internal signal handler. */

static void sigHandler(int signum)
{
    g_signal = signum;
    g_killed = true;
}
#endif

/* Helper to print a fatal error message and exit. */

void fatal(const char* msg, ...)
{
    char buffer[400U];
    ::memset(buffer, 0x20U, 400U);

    va_list vl;
    va_start(vl, msg);

    ::vsprintf(buffer, msg, vl);

    va_end(vl);

    ::fprintf(stderr, "%s: %s\n", g_progExe.c_str(), buffer);
    exit(EXIT_FAILURE);
}

/* Helper to pring usage the command line arguments. (And optionally an error.) */

void usage(const char* message, const char* arg)
{
    ::fprintf(stdout, __PROG_NAME__ " %s (built %s)\r\n", __VER__, __BUILD__);
    ::fprintf(stdout, "Copyright (c) 2017-2024 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\n");
    ::fprintf(stdout, "Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others\n\n");
    if (message != nullptr) {
        ::fprintf(stderr, "%s: ", g_progExe.c_str());
        ::fprintf(stderr, message, arg);
        ::fprintf(stderr, "\n\n");
    }

    ::fprintf(stdout, 
        "usage: %s [-vhf]"
        "[-i <input audio device id>]"
        "[-o <output audio device id>]"
        "[-c <configuration file>]"
        "\n\n"
        "  -v        show version information\n"
        "  -h        show this screen\n"
        "  -f        foreground mode\n"
        "\n"
        "  -i        input audio device\n"
        "  -o        output audio device\n"
        "\n"
        "  -c <file> specifies the configuration file to use\n"
        "\n"
        "  --        stop handling options\n",
        g_progExe.c_str());

    ma_context context;
    if (ma_context_init(g_backends, g_backendCnt, NULL, &context) != MA_SUCCESS) {
        fprintf(stderr, "Failed to initialize audio context.\n");
        exit(EXIT_FAILURE);
    }

    fprintf(stdout, "\nAudio Backend: %s", ma_get_backend_name(context.backend));

    ma_device_info* playbackDevices;
    ma_device_info* captureDevices;
    ma_uint32 playbackDeviceCount, captureDeviceCount;
    ma_result result = ma_context_get_devices(&context, &playbackDevices, &playbackDeviceCount, &captureDevices, &captureDeviceCount);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "Failed to retrieve audio device information.\n");
        exit(EXIT_FAILURE);
    }

    fprintf(stdout, "\nAudio Input Devices:\n");
    for (uint32_t i = 0; i < captureDeviceCount; ++i) {
        fprintf(stdout, "    %u: %s\n", i, captureDevices[i].name);
    }
    fprintf(stdout, "\n");

    fprintf(stdout, "Audio Output Devices:\n");
    for (uint32_t i = 0; i < playbackDeviceCount; ++i) {
        fprintf(stdout, "    %u: %s\n", i, playbackDevices[i].name);
    }

    ma_context_uninit(&context);
    exit(EXIT_FAILURE);
}

/* Helper to validate the command line arguments. */

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
        else if (IS("-f")) {
            g_foreground = true;
        }
        else if (IS("-i")) {
            if ((argc - 1) <= 0)
                usage("error: %s", "must specify the input audio device to use");
            g_inputDevice = ::atoi(argv[++i]);
            p += 2;
        }
        else if (IS("-o")) {
            if ((argc - 1) <= 0)
                usage("error: %s", "must specify the output audio device to use");
            g_outputDevice = ::atoi(argv[++i]);
            p += 2;
        }
        else if (IS("-c")) {
            if (argc-- <= 0)
                usage("error: %s", "must specify the configuration file to use");
            g_iniFile = std::string(argv[++i]);

            if (g_iniFile.empty())
                usage("error: %s", "configuration file cannot be blank!");

            p += 2;
        }
        else if (IS("-v")) {
            ::fprintf(stdout, __PROG_NAME__ " %s (built %s)\r\n", __VER__, __BUILD__);
            ::fprintf(stdout, "Copyright (c) 2017-2024 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\n");
            ::fprintf(stdout, "Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others\n\n");
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

// ---------------------------------------------------------------------------
//  Program Entry Point
// ---------------------------------------------------------------------------
#if !defined(CATCH2_TEST_COMPILATION)
int main(int argc, char** argv)
{
    g_gitHashBytes = new uint8_t[4U];
    ::memset(g_gitHashBytes, 0x00U, 4U);

#ifdef _WIN32
    // Windows
    g_backends[0] = ma_backend_winmm;
    g_backends[1] = ma_backend_wasapi;
    g_backends[2] = ma_backend_null;
#else
    // Linux
    g_backends[0] = ma_backend_pulseaudio;
    g_backends[1] = ma_backend_alsa;
    g_backends[2] = ma_backend_jack;
    g_backends[3] = ma_backend_oss;
    // MacOS
    g_backends[4] = ma_backend_coreaudio;
    g_backends[5] = ma_backend_sndio;
    // BSD
    g_backends[6] = ma_backend_null;
#endif

    uint32_t hash = ::strtoul(__GIT_VER_HASH__, 0, 16);
    __SET_UINT32(hash, g_gitHashBytes, 0U);

    if (argv[0] != nullptr && *argv[0] != 0)
        g_progExe = std::string(argv[0]);

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

    ::signal(SIGINT, sigHandler);
    ::signal(SIGTERM, sigHandler);
#if !defined(_WIN32)
    ::signal(SIGHUP, sigHandler);
#endif // !defined(_WIN32)

    int ret = 0;

    do {
        g_signal = 0;
        g_killed = false;

        HostBridge*bridge = new HostBridge(g_iniFile);
        ret = bridge->run();
        delete bridge;

        if (g_signal == 2)
            ::LogInfoEx(LOG_HOST, "Exited on receipt of SIGINT");

        if (g_signal == 15)
            ::LogInfoEx(LOG_HOST, "Exited on receipt of SIGTERM");

        if (g_signal == 1)
            ::LogInfoEx(LOG_HOST, "Restarting on receipt of SIGHUP");
    } while (g_signal == 1);

    ::LogFinalise();
    ::ActivityLogFinalise();

    return ret;
}
#endif