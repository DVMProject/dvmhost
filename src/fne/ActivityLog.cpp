// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Converged FNE Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Converged FNE Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
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

/// <summary>
/// Helper to open the activity log file, file handle.
/// </summary>
/// <returns>True, if log file is opened, otherwise false.
static bool ActivityLogOpen()
{
    if (CurrentLogFileLevel() == 0U)
        return true;

    time_t now;
    ::time(&now);

    struct tm* tm = ::gmtime(&now);

    if (tm->tm_mday == m_actTm.tm_mday && tm->tm_mon == m_actTm.tm_mon && tm->tm_year == m_actTm.tm_year) {
        if (m_actFpLog != nullptr)
            return true;
    }
    else {
        if (m_actFpLog != nullptr)
            ::fclose(m_actFpLog);
    }

    char filename[200U];
    ::sprintf(filename, "%s/%s-%04d-%02d-%02d.activity.log", LogGetFilePath().c_str(), LogGetFileRoot().c_str(), tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

    m_actFpLog = ::fopen(filename, "a+t");
    m_actTm = *tm;

    return m_actFpLog != nullptr;
}

/// <summary>
/// Initializes the activity log.
/// </summary>
/// <param name="filePath">Full-path to the activity log file.</param>
/// <param name="fileRoot">Prefix of the activity log file name.</param>
bool ActivityLogInitialise(const std::string& filePath, const std::string& fileRoot)
{
#if defined(CATCH2_TEST_COMPILATION)
    return true;
#endif
    m_actFilePath = filePath;
    m_actFileRoot = fileRoot;

    return ::ActivityLogOpen();
}

/// <summary>
/// Finalizes the activity log.
/// </summary>
void ActivityLogFinalise()
{
#if defined(CATCH2_TEST_COMPILATION)
    return;
#endif
    if (m_actFpLog != nullptr)
        ::fclose(m_actFpLog);
}

/// <summary>
/// Writes a new entry to the activity log.
/// </summary>
/// <remarks>This is a variable argument function.</remarks>
/// <param name="msg">Formatted string to write to activity log.</param>
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
