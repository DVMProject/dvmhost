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
*   Copyright (C) 2015,2016 by Jonathan Naylor G4KLX
*   Copyright (C) 2018-2019 by Bryan Biedenkapp N2PLL
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
#include "Log.h"
#include "network/Network.h"

#include <sys/time.h>

#if defined(CATCH2_TEST_COMPILATION)
#include <catch2/catch_test_macros.hpp>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <ctime>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define EOL    "\r\n"

const uint32_t ACT_LOG_BUFFER_LEN = 501U;
const uint32_t LOG_BUFFER_LEN = 4096U;

// ---------------------------------------------------------------------------
//  Global Variables
// ---------------------------------------------------------------------------

static uint32_t m_fileLevel = 0U;
static std::string m_filePath;
static std::string m_actFilePath;
static std::string m_fileRoot;
static std::string m_actFileRoot;

static network::Network* m_network;

static FILE* m_fpLog = nullptr;
static FILE* m_actFpLog = nullptr;

uint32_t g_logDisplayLevel = 2U;
bool g_disableTimeDisplay = false;

static struct tm m_tm;
static struct tm m_actTm;

static std::ostream m_outStream{std::cerr.rdbuf()};

static char LEVELS[] = " DMIWEF";

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/// <summary>
/// Helper to open the detailed log file, file handle.
/// </summary>
/// <returns>True, if log file is opened, otherwise false.
static bool LogOpen()
{
#if defined(CATCH2_TEST_COMPILATION)
    return true;
#endif
    if (m_fileLevel == 0U)
        return true;

    time_t now;
    ::time(&now);

    struct tm* tm = ::gmtime(&now);

    if (tm->tm_mday == m_tm.tm_mday && tm->tm_mon == m_tm.tm_mon && tm->tm_year == m_tm.tm_year) {
        if (m_fpLog != nullptr)
            return true;
    }
    else {
        if (m_fpLog != nullptr)
            ::fclose(m_fpLog);
    }

    char filename[200U];
    ::sprintf(filename, "%s/%s-%04d-%02d-%02d.log", m_filePath.c_str(), m_fileRoot.c_str(), tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

    m_fpLog = ::fopen(filename, "a+t");
    m_tm = *tm;

    return m_fpLog != nullptr;
}

/// <summary>
/// Helper to open the activity log file, file handle.
/// </summary>
/// <returns>True, if log file is opened, otherwise false.
static bool ActivityLogOpen()
{
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
    ::sprintf(filename, "%s/%s-%04d-%02d-%02d.activity.log", m_filePath.c_str(), m_fileRoot.c_str(), tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

    m_actFpLog = ::fopen(filename, "a+t");
    m_actTm = *tm;

    return m_actFpLog != nullptr;
}

/// <summary>
/// Internal helper to set an output stream to direct logging to.
/// </summary>
/// <param name="stream"></param>
void __InternalOutputStream(std::ostream& stream)
{
    m_outStream.rdbuf(stream.rdbuf());
}

/// <summary>
/// Sets the instance of the Network class to transfer the activity log with.
/// </summary>
/// <param name="network">Instance of the Network class.</param>
void LogSetNetwork(void* network)
{
#if defined(CATCH2_TEST_COMPILATION)
    return;
#endif
    // note: The Network class is passed here as a void so we can avoid including the Network.h
    // header in Log.h. This is dirty and probably terrible...
    m_network = (network::Network*)network;
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
    m_network = nullptr;

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
/// <param name="mode">Digital mode (usually P25 or DMR).</param>
/// <param name="sourceRf">Flag indicating that the entry was generated from an RF event.</param>
/// <param name="msg">Formatted string to write to activity log.</param>
void ActivityLog(const char *mode, const bool sourceRf, const char* msg, ...)
{
#if defined(CATCH2_TEST_COMPILATION)
    return;
#endif
    assert(mode != nullptr);
    assert(msg != nullptr);

    char buffer[ACT_LOG_BUFFER_LEN];
    struct timeval now;
    ::gettimeofday(&now, NULL);

    struct tm* tm = ::gmtime(&now.tv_sec);

    if (strcmp(mode, "") == 0) {
        ::sprintf(buffer, "A: %04d-%02d-%02d %02d:%02d:%02d.%03lu ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, now.tv_usec / 1000U);
    }
    else {
        ::sprintf(buffer, "A: %04d-%02d-%02d %02d:%02d:%02d.%03lu %s %s ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, now.tv_usec / 1000U, mode, (sourceRf) ? "RF" : "Net");
    }

    va_list vl;
    va_start(vl, msg);

    ::vsnprintf(buffer + ::strlen(buffer), ACT_LOG_BUFFER_LEN - 1U, msg, vl);

    va_end(vl);

    bool ret = ::ActivityLogOpen();
    if (!ret)
        return;

    ::fprintf(m_actFpLog, "%s\n", buffer);
    ::fflush(m_actFpLog);

    if (m_network != nullptr) {
        m_network->writeActLog(buffer);
    }

    if (2U >= m_fileLevel && m_fileLevel != 0U) {
        bool ret = ::LogOpen();
        if (!ret)
            return;

        ::fprintf(m_fpLog, "%s\n", buffer);
        ::fflush(m_fpLog);
    }

    if (2U >= g_logDisplayLevel && g_logDisplayLevel != 0U) {
        ::fprintf(stdout, "%s" EOL, buffer);
        ::fflush(stdout);
    }
}

/// <summary>
/// Initializes the diagnostics log.
/// </summary>
/// <param name="filePath">Full-path to the detailed log file.</param>
/// <param name="fileRoot">Prefix of the detailed log file name.</param>
/// <param name="fileLevel">File logging level.</param>
/// <param name="displayLevel">Console logging level.</param>
/// <param name="disableTimeDisplay">Disable display of date/time on the console log.</param>
bool LogInitialise(const std::string& filePath, const std::string& fileRoot, uint32_t fileLevel, uint32_t displayLevel, bool disableTimeDisplay)
{
    m_filePath = filePath;
    m_fileRoot = fileRoot;
    m_fileLevel = fileLevel;
    g_logDisplayLevel = displayLevel;
    g_disableTimeDisplay = disableTimeDisplay;
    return ::LogOpen();
}

/// <summary>
/// Finalizes the diagnostics log.
/// </summary>
void LogFinalise()
{
#if defined(CATCH2_TEST_COMPILATION)
    return;
#endif
    if (m_fpLog != nullptr)
        ::fclose(m_fpLog);
}

/// <summary>
/// Writes a new entry to the diagnostics log.
/// </summary>
/// <remarks>This is a variable argument function.</remarks>
/// <param name="level">Log level.</param>
/// <param name="module">Module name the log entry was genearted from.</param>
/// <param name="fmt">Formatted string to write to the log.</param>
void Log(uint32_t level, const char *module, const char* fmt, ...)
{
    assert(fmt != nullptr);
#if defined(CATCH2_TEST_COMPILATION)
    g_disableTimeDisplay = true;
#endif
    char buffer[LOG_BUFFER_LEN];
    if (!g_disableTimeDisplay) {
        struct timeval now;
        ::gettimeofday(&now, NULL);

        struct tm* tm = ::gmtime(&now.tv_sec);

        if (module != nullptr) {
            ::sprintf(buffer, "%c: %04d-%02d-%02d %02d:%02d:%02d.%03lu (%s) ", LEVELS[level], tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, now.tv_usec / 1000U, module);
        }
        else {
            ::sprintf(buffer, "%c: %04d-%02d-%02d %02d:%02d:%02d.%03lu ", LEVELS[level], tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, now.tv_usec / 1000U);
        }
    }
    else {
        if (module != nullptr) {
            ::sprintf(buffer, "%c: (%s) ", LEVELS[level], module);
        }
        else {
            if (level >= 9999U) {
                ::sprintf(buffer, "U: ");
            }
            else {
                ::sprintf(buffer, "%c: ", LEVELS[level]);
            }
        }
    }

    va_list vl;
    va_start(vl, fmt);

    ::vsnprintf(buffer + ::strlen(buffer), LOG_BUFFER_LEN - 1U, fmt, vl);

    va_end(vl);

    if (m_outStream && g_logDisplayLevel == 0U) {
        m_outStream << buffer << std::endl;
    }

    if (m_network != nullptr) {
        // don't transfer debug data...
        if (level > 1U) {
            m_network->writeDiagLog(buffer);
        }
    }

#if defined(CATCH2_TEST_COMPILATION)
    UNSCOPED_INFO(buffer);
    return;
#endif

    if (level >= m_fileLevel && m_fileLevel != 0U) {
        bool ret = ::LogOpen();
        if (!ret)
            return;

        ::fprintf(m_fpLog, "%s\n", buffer);
        ::fflush(m_fpLog);
    }

    if (level >= g_logDisplayLevel && g_logDisplayLevel != 0U) {
        ::fprintf(stdout, "%s" EOL, buffer);
        ::fflush(stdout);
    }

    // fatal error (specially allow any log levels above 9999)
    if (level >= 6U && level < 9999U) {
        ::fclose(m_fpLog);
        exit(1);
    }
}
