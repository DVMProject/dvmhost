// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - TG Patch
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "ActivityLog.h"
#include "common/network/BaseNetwork.h"
#include "common/Log.h" // for CurrentLogFileLevel() and LogGetNetwork()

#if defined(CATCH2_TEST_COMPILATION)
#include <catch2/catch_test_macros.hpp>
#endif

#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define EOL    "\r\n"

const uint32_t ACT_LOG_BUFFER_LEN = 501U;

// ---------------------------------------------------------------------------
//  Global Variables
// ---------------------------------------------------------------------------

static std::string g_actFilePath;
static std::string g_actFileRoot;

static FILE* g_actFpLog = nullptr;

static struct tm g_actTm;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/* Helper to open the activity log file, file handle. */

static bool ActivityLogOpen()
{
    if (CurrentLogFileLevel() == 0U)
        return true;

    time_t now;
    ::time(&now);

    struct tm* tm = ::gmtime(&now);

    if (tm->tm_mday == g_actTm.tm_mday && tm->tm_mon == g_actTm.tm_mon && tm->tm_year == g_actTm.tm_year) {
        if (g_actFpLog != nullptr)
            return true;
    }
    else {
        if (g_actFpLog != nullptr)
            ::fclose(g_actFpLog);
    }

    char filename[200U];
    ::sprintf(filename, "%s/%s-%04d-%02d-%02d.activity.log", LogGetFilePath().c_str(), LogGetFileRoot().c_str(), tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

    g_actFpLog = ::fopen(filename, "a+t");
    g_actTm = *tm;

    return g_actFpLog != nullptr;
}

/* Initializes the activity log. */

bool ActivityLogInitialise(const std::string& filePath, const std::string& fileRoot)
{
#if defined(CATCH2_TEST_COMPILATION)
    return true;
#endif
    g_actFilePath = filePath;
    g_actFileRoot = fileRoot;

    return ::ActivityLogOpen();
}

/* Finalizes the activity log. */

void ActivityLogFinalise()
{
#if defined(CATCH2_TEST_COMPILATION)
    return;
#endif
    if (g_actFpLog != nullptr)
        ::fclose(g_actFpLog);
}

/* Writes a new entry to the activity log. */

void log_internal::ActivityLogInternal(const std::string& log)
{
#if defined(CATCH2_TEST_COMPILATION)
    return;
#endif
    bool ret = ::ActivityLogOpen();
    if (!ret)
        return;

    if (CurrentLogFileLevel() == 0U)
        return;

    if (LogGetNetwork() != nullptr) {
        network::BaseNetwork* network = (network::BaseNetwork*)LogGetNetwork();
        network->writeActLog(log.c_str());
    }

    ::fprintf(g_actFpLog, "%s\n", log.c_str());
    ::fflush(g_actFpLog);

    if (2U >= g_logDisplayLevel && g_logDisplayLevel != 0U) {
        ::fprintf(stdout, "%s" EOL, log.c_str());
        ::fflush(stdout);
    }
}
