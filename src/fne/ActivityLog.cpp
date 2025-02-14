// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "ActivityLog.h"
#include "common/Log.h" // for CurrentLogFileLevel() and LogGetNetwork()

#if defined(CATCH2_TEST_COMPILATION)
#include <catch2/catch_test_macros.hpp>
#endif

#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <cassert>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define EOL    "\r\n"

const uint32_t ACT_LOG_BUFFER_LEN = 501U;

// ---------------------------------------------------------------------------
//  Global Variables
// ---------------------------------------------------------------------------

static std::string m_actFilePath;
static std::string m_actFileRoot;

static FILE* m_actFpLog = nullptr;

static struct tm m_actTm;

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

    struct tm* tm = ::localtime(&now);

    if (tm->tm_mday == m_actTm.tm_mday && tm->tm_mon == m_actTm.tm_mon && tm->tm_year == m_actTm.tm_year) {
        if (m_actFpLog != nullptr)
            return true;
    }
    else {
        if (m_actFpLog != nullptr)
            ::fclose(m_actFpLog);
    }

    char filename[200U];
    ::sprintf(filename, "%s/%s-%04d-%02d-%02d.activity.log", m_actFilePath.c_str(), m_actFileRoot.c_str(), tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

    m_actFpLog = ::fopen(filename, "a+t");
    m_actTm = *tm;

    return m_actFpLog != nullptr;
}

/* Initializes the activity log. */

bool ActivityLogInitialise(const std::string& filePath, const std::string& fileRoot)
{
#if defined(CATCH2_TEST_COMPILATION)
    return true;
#endif
    m_actFilePath = filePath;
    m_actFileRoot = fileRoot;

    return ::ActivityLogOpen();
}

/* Finalizes the activity log. */

void ActivityLogFinalise()
{
#if defined(CATCH2_TEST_COMPILATION)
    return;
#endif
    if (m_actFpLog != nullptr)
        ::fclose(m_actFpLog);
}

/* Writes a new entry to the activity log. */

void ActivityLog(const char* msg, ...)
{
#if defined(CATCH2_TEST_COMPILATION)
    return;
#endif
    assert(msg != nullptr);

    char buffer[ACT_LOG_BUFFER_LEN];

    va_list vl, vl_len;
    va_start(vl, msg);
    va_copy(vl_len, vl);

    size_t len = ::vsnprintf(nullptr, 0U, msg, vl_len);
    ::vsnprintf(buffer, len + 1U, msg, vl);

    va_end(vl_len);
    va_end(vl);

    bool ret = ::ActivityLogOpen();
    if (!ret)
        return;

    if (CurrentLogFileLevel() == 0U)
        return;

    ::fprintf(m_actFpLog, "%s\n", buffer);
    ::fflush(m_actFpLog);

    if (2U >= g_logDisplayLevel && g_logDisplayLevel != 0U) {
        ::fprintf(stdout, "%s" EOL, buffer);
        ::fflush(stdout);
    }
}
