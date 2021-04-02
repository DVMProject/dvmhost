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

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/time.h>
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

#if defined(_WIN32) || defined(_WIN64)
#define EOL    "\n"
#else
#define EOL    "\r\n"
#endif

// ---------------------------------------------------------------------------
//  Global Variables
// ---------------------------------------------------------------------------

static uint32_t m_fileLevel = 0U;
static std::string m_filePath;
static std::string m_actFilePath;
static std::string m_fileRoot;
static std::string m_actFileRoot;

static network::Network* m_network;

static FILE* m_fpLog = NULL;
static FILE* m_actFpLog = NULL;

static uint32_t m_displayLevel = 2U;

static struct tm m_tm;
static struct tm m_actTm;

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
    if (m_fileLevel == 0U)
        return true;

    time_t now;
    ::time(&now);

    struct tm* tm = ::gmtime(&now);

    if (tm->tm_mday == m_tm.tm_mday && tm->tm_mon == m_tm.tm_mon && tm->tm_year == m_tm.tm_year) {
        if (m_fpLog != NULL)
            return true;
    }
    else {
        if (m_fpLog != NULL)
            ::fclose(m_fpLog);
    }

    char filename[200U];
#if defined(_WIN32) || defined(_WIN64)
    ::sprintf(filename, "%s\\%s-%04d-%02d-%02d.log", m_filePath.c_str(), m_fileRoot.c_str(), tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
#else
    ::sprintf(filename, "%s/%s-%04d-%02d-%02d.log", m_filePath.c_str(), m_fileRoot.c_str(), tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
#endif
    m_fpLog = ::fopen(filename, "a+t");
    m_tm = *tm;

    return m_fpLog != NULL;
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
        if (m_actFpLog != NULL)
            return true;
    }
    else {
        if (m_actFpLog != NULL)
            ::fclose(m_actFpLog);
    }

    char filename[200U];
#if defined(_WIN32) || defined(_WIN64)
    ::sprintf(filename, "%s\\%s-%04d-%02d-%02d.activity.log", m_filePath.c_str(), m_fileRoot.c_str(), tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
#else
    ::sprintf(filename, "%s/%s-%04d-%02d-%02d.activity.log", m_filePath.c_str(), m_fileRoot.c_str(), tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
#endif
    m_actFpLog = ::fopen(filename, "a+t");
    m_actTm = *tm;

    return m_actFpLog != NULL;
}

/// <summary>
/// Sets the instance of the Network class to transfer the activity log with.
/// </summary>
/// <param name="network">Instance of the Network class.</param>
void LogSetNetwork(void* network)
{
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
    m_actFilePath = filePath;
    m_actFileRoot = fileRoot;
    m_network = NULL;

    return ::ActivityLogOpen();
}

/// <summary>
/// Finalizes the activity log.
/// </summary>
void ActivityLogFinalise()
{
    if (m_actFpLog != NULL)
        ::fclose(m_actFpLog);
}

/// <summary>
/// Writes a new entry to the activity log.
/// </summary>
/// <param name="mode">Digital mode (usually P25 or DMR).</param>
/// <param name="sourceRf">Flag indicating that the entry was generated from an RF event.</param>
/// <param name="msg">Formatted string to write to activity log.</param>
void ActivityLog(const char *mode, const bool sourceRf, const char* msg, ...)
{
    assert(mode != NULL);
    assert(msg != NULL);

    char buffer[501U];
#if defined(_WIN32) || defined(_WIN64)
    SYSTEMTIME st;
    ::GetSystemTime(&st);

    ::sprintf(buffer, "A: %04u-%02u-%02u %02u:%02u:%02u.%03u %s %s ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, mode, (sourceRf) ? "RF" : "Net");
#else
    struct timeval now;
    ::gettimeofday(&now, NULL);

    struct tm* tm = ::gmtime(&now.tv_sec);

    ::sprintf(buffer, "A: %04d-%02d-%02d %02d:%02d:%02d.%03lu %s %s ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, now.tv_usec / 1000U, mode, (sourceRf) ? "RF" : "Net");
#endif

    va_list vl;
    va_start(vl, msg);

    ::vsnprintf(buffer + ::strlen(buffer), 500, msg, vl);

    va_end(vl);

    bool ret = ::ActivityLogOpen();
    if (!ret)
        return;

    ::fprintf(m_actFpLog, "%s\n", buffer);
    ::fflush(m_actFpLog);

    if (m_network != NULL) {
        m_network->writeActLog(buffer);
    }

    if (2U >= m_fileLevel && m_fileLevel != 0U) {
        bool ret = ::LogOpen();
        if (!ret)
            return;

        ::fprintf(m_fpLog, "%s\n", buffer);
        ::fflush(m_fpLog);
    }

    if (2U >= m_displayLevel && m_displayLevel != 0U) {
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
bool LogInitialise(const std::string& filePath, const std::string& fileRoot, uint32_t fileLevel, uint32_t displayLevel)
{
    m_filePath = filePath;
    m_fileRoot = fileRoot;
    m_fileLevel = fileLevel;
    m_displayLevel = displayLevel;
    return ::LogOpen();
}

/// <summary>
/// Finalizes the diagnostics log.
/// </summary>
void LogFinalise()
{
    if (m_fpLog != NULL)
        ::fclose(m_fpLog);
}

/// <summary>
/// Writes a new entry to the diagnostics log.
/// </summary>
/// <param name="level">Log level.</param>
/// <param name="module">Module name the log entry was genearted from.</param>
/// <param name="msg">Formatted string to write to activity log.</param>
void Log(uint32_t level, const char *module, const char* fmt, ...)
{
    assert(fmt != NULL);

    char buffer[501U];
#if defined(_WIN32) || defined(_WIN64)
    SYSTEMTIME st;
    ::GetSystemTime(&st);

    if (module != NULL) {
        ::sprintf(buffer, "%c: %04u-%02u-%02u %02u:%02u:%02u.%03u (%s) ", LEVELS[level], st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, module);
    }
    else {
        ::sprintf(buffer, "%c: %04u-%02u-%02u %02u:%02u:%02u.%03u ", LEVELS[level], st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    }
#else
    struct timeval now;
    ::gettimeofday(&now, NULL);

    struct tm* tm = ::gmtime(&now.tv_sec);

    if (module != NULL) {
        ::sprintf(buffer, "%c: %04d-%02d-%02d %02d:%02d:%02d.%03lu (%s) ", LEVELS[level], tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, now.tv_usec / 1000U, module);
    }
    else {
        ::sprintf(buffer, "%c: %04d-%02d-%02d %02d:%02d:%02d.%03lu ", LEVELS[level], tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, now.tv_usec / 1000U);
    }
#endif

    va_list vl;
    va_start(vl, fmt);

    ::vsnprintf(buffer + ::strlen(buffer), 500, fmt, vl);

    va_end(vl);

    if (m_network != NULL) {
        // don't transfer debug data...
        if (level > 1U) {
            m_network->writeDiagLog(buffer);
        }
    }

    if (level >= m_fileLevel && m_fileLevel != 0U) {
        bool ret = ::LogOpen();
        if (!ret)
            return;

        ::fprintf(m_fpLog, "%s\n", buffer);
        ::fflush(m_fpLog);
    }

    if (level >= m_displayLevel && m_displayLevel != 0U) {
        ::fprintf(stdout, "%s" EOL, buffer);
        ::fflush(stdout);
    }

    if (level >= 6U) {        // Fatal
        ::fclose(m_fpLog);
        exit(1);
    }
}
