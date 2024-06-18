// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
*   Copyright (C) 2018-2024 Bryan Biedenkapp, N2PLL
*
*/
#include "Log.h"
#include "network/BaseNetwork.h"

#include <sys/time.h>
#include <syslog.h>

#if defined(CATCH2_TEST_COMPILATION)
#include <catch2/catch_test_macros.hpp>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <ctime>
#include <cassert>
#include <cstring>
#include <iostream>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define EOL    "\r\n"

const uint32_t LOG_BUFFER_LEN = 4096U;

// ---------------------------------------------------------------------------
//  Global Variables
// ---------------------------------------------------------------------------

static uint32_t m_fileLevel = 0U;
static std::string m_filePath;
static std::string m_fileRoot;

static network::BaseNetwork* m_network;

static FILE* m_fpLog = nullptr;

uint32_t g_logDisplayLevel = 2U;
bool g_disableTimeDisplay = false;

bool g_useSyslog = false;

static struct tm m_tm;

static std::ostream m_outStream { std::cerr.rdbuf() };

static char LEVELS[] = " DMIWEF";

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/// <summary>
/// Helper to get the current log file level.
/// </summary>
/// <returns></returns>
uint32_t CurrentLogFileLevel() { return m_fileLevel; }

/// <summary>
/// Helper to get the current log file path.
/// </summary>
/// <returns></returns>
std::string LogGetFilePath() { return m_filePath; }

/// <summary>
/// Helper to get the current log file root.
/// </summary>
/// <returns></returns>
std::string LogGetFileRoot() { return m_fileRoot; }

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

    if (!g_useSyslog) {
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
    else {
        switch (m_fileLevel) {
        case 1U:
            setlogmask(LOG_UPTO(LOG_DEBUG));
            break;
        case 2U:
            setlogmask(LOG_UPTO(LOG_INFO));
            break;
        case 3U:
            setlogmask(LOG_UPTO(LOG_NOTICE));
            break;
        case 4U:
            setlogmask(LOG_UPTO(LOG_WARNING));
            break;
        case 5U:
        default:
            setlogmask(LOG_UPTO(LOG_ERR));
            break;
        }

        openlog(m_fileRoot.c_str(), LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);
        return true;
    }
}

/// <summary>
/// Internal helper to set an output stream to direct logging to.
/// </summary>
/// <param name="stream"></param>
void __InternalOutputStream(std::ostream& stream)
{
    m_outStream.rdbuf(stream.rdbuf());
}

/// <summary>Gets the instance of the Network class to transfer the activity log with.</summary>
void* LogGetNetwork()
{
    // NO GOOD, VERY BAD, TERRIBLE HACK
    return (void*)m_network;
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
    m_network = (network::BaseNetwork*)network;
}

/// <summary>
/// Initializes the diagnostics log.
/// </summary>
/// <param name="filePath">Full-path to the detailed log file.</param>
/// <param name="fileRoot">Prefix of the detailed log file name.</param>
/// <param name="fileLevel">File logging level.</param>
/// <param name="displayLevel">Console logging level.</param>
/// <param name="disableTimeDisplay">Disable display of date/time on the console log.</param>
/// <param name="useSyslog">Flag indicating whether or not logging should be sent to the syslog.</param>
bool LogInitialise(const std::string& filePath, const std::string& fileRoot, uint32_t fileLevel, uint32_t displayLevel, bool disableTimeDisplay, bool useSyslog)
{
    m_filePath = filePath;
    m_fileRoot = fileRoot;
    m_fileLevel = fileLevel;
    g_logDisplayLevel = displayLevel;
    g_disableTimeDisplay = disableTimeDisplay;
    if (!g_useSyslog)
        g_useSyslog = useSyslog;
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
    if (g_useSyslog)
        closelog();
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
    if (!g_disableTimeDisplay && !g_useSyslog) {
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

    va_list vl, vl_len;
    va_start(vl, fmt);
    va_copy(vl_len, vl);

    size_t len = ::vsnprintf(nullptr, 0U, fmt, vl_len);
    ::vsnprintf(buffer + ::strlen(buffer), len + 1U, fmt, vl);

    va_end(vl_len);
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
        if (!g_useSyslog) {
            bool ret = ::LogOpen();
            if (!ret)
                return;

            ::fprintf(m_fpLog, "%s\n", buffer);
            ::fflush(m_fpLog);
        } else {
            // convert our log level into syslog level
            int syslogLevel = LOG_INFO;
            switch (level) {
            case 1U:
                syslogLevel = LOG_DEBUG;
                break;
            case 2U:
                syslogLevel = LOG_NOTICE;
                break;
            case 3U:
                syslogLevel = LOG_INFO;
                break;
            case 4U:
                syslogLevel = LOG_WARNING;
                break;
            case 5U:
                syslogLevel = LOG_ERR;
                break;
            default:
                syslogLevel = LOG_EMERG;
                break;
            }

            syslog(syslogLevel, "%s", buffer);
        }
    }

    if (!g_useSyslog && level >= g_logDisplayLevel && g_logDisplayLevel != 0U) {
        ::fprintf(stdout, "%s" EOL, buffer);
        ::fflush(stdout);
    }

    // fatal error (specially allow any log levels above 9999)
    if (level >= 6U && level < 9999U) {
        if (m_fpLog != nullptr)
            ::fclose(m_fpLog);
        if (g_useSyslog)
            ::closelog();
        exit(1);
    }
}
