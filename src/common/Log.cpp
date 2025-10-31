// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2018-2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Log.h"
#include "network/BaseNetwork.h"

#if defined(CATCH2_TEST_COMPILATION)
#include <catch2/catch_test_macros.hpp>
#endif

#if !defined(_WIN32)
#include <syslog.h>
#endif // defined(_WIN32)

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
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

static uint32_t g_fileLevel = 0U;
static std::string g_filePath;
static std::string g_fileRoot;

static network::BaseNetwork* g_network;

static FILE* g_fpLog = nullptr;

uint32_t g_logDisplayLevel = 2U;
bool g_disableTimeDisplay = false;

bool g_useSyslog = false;
bool g_disableNetworkLog = false;

static struct tm g_tm;

static std::ostream g_outStream { std::cerr.rdbuf() };

bool log_stacktrace::SignalHandling::s_foreground = false;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/* Helper to get the current log file level. */

uint32_t CurrentLogFileLevel() { return g_fileLevel; }

/* Helper to get the current log file path. */

std::string LogGetFilePath() { return g_filePath; }

/* Helper to get the current log file root. */

std::string LogGetFileRoot() { return g_fileRoot; }

/* Helper to open the detailed log file, file handle. */

static bool LogOpen()
{
#if defined(CATCH2_TEST_COMPILATION)
    return true;
#endif
    if (g_fileLevel == 0U)
        return true;

    if (!g_useSyslog) {
        time_t now;
        ::time(&now);

        struct tm* tm = ::localtime(&now);

        if (tm->tm_mday == g_tm.tm_mday && tm->tm_mon == g_tm.tm_mon && tm->tm_year == g_tm.tm_year) {
            if (g_fpLog != nullptr)
                return true;
        }
        else {
            if (g_fpLog != nullptr)
                ::fclose(g_fpLog);
        }

        char filename[200U];
        ::sprintf(filename, "%s/%s-%04d-%02d-%02d.log", g_filePath.c_str(), g_fileRoot.c_str(), tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

        g_fpLog = ::fopen(filename, "a+t");
        g_tm = *tm;

        return g_fpLog != nullptr;
    }
    else {
#if !defined(_WIN32)
        switch (g_fileLevel) {
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

        openlog(g_fileRoot.c_str(), LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);
        return true;
#else
        return false;
#endif // !defined(_WIN32)
    }
}

/* Gets the instance of the Network class to transfer the activity log with. */

void* LogGetNetwork()
{
    // NO GOOD, VERY BAD, TERRIBLE HACK
    return (void*)g_network;
}

/* Sets the instance of the Network class to transfer the activity log with. */

void LogSetNetwork(void* network)
{
#if defined(CATCH2_TEST_COMPILATION)
    return;
#endif
    // note: The Network class is passed here as a void so we can avoid including the Network.h
    // header in Log.h. This is dirty and probably terrible...
    g_network = (network::BaseNetwork*)network;
}

/* Initializes the diagnostics log. */

bool LogInitialise(const std::string& filePath, const std::string& fileRoot, uint32_t fileLevel, uint32_t displayLevel, bool disableTimeDisplay, bool useSyslog)
{
    g_filePath = filePath;
    g_fileRoot = fileRoot;
    g_fileLevel = fileLevel;
    g_logDisplayLevel = displayLevel;
    g_disableTimeDisplay = disableTimeDisplay;
#if defined(_WIN32)
    g_useSyslog = false;
#else
    if (!g_useSyslog)
        g_useSyslog = useSyslog;
#endif // defined(_WIN32)
    return ::LogOpen();
}

/* Finalizes the diagnostics log. */

void LogFinalise()
{
#if defined(CATCH2_TEST_COMPILATION)
    return;
#endif
    if (g_fpLog != nullptr) {
        ::fclose(g_fpLog);
        g_fpLog = nullptr;
    }
#if !defined(_WIN32)
    if (g_useSyslog)
        closelog();
#endif // !defined(_WIN32)
}

/* Internal helper to set an output stream to direct logging to. */

void log_internal::SetInternalOutputStream(std::ostream& stream)
{
    g_outStream.rdbuf(stream.rdbuf());
}

/* Writes a new entry to the diagnostics log. */

void log_internal::LogInternal(uint32_t level, const std::string& log)
{
    if (g_outStream && g_logDisplayLevel == 0U) {
        g_outStream << log << std::endl;
    }

    if (g_network != nullptr && !g_disableNetworkLog) {
        // don't transfer debug data...
        if (level > 1U) {
            g_network->writeDiagLog(log.c_str());
        }
    }

#if defined(CATCH2_TEST_COMPILATION)
    UNSCOPED_INFO(log.c_str());
    return;
#endif

    if (level >= g_fileLevel && g_fileLevel != 0U) {
        if (!g_useSyslog) {
            bool ret = ::LogOpen();
            if (!ret)
                return;

            if (g_fpLog != nullptr) {
                ::fprintf(g_fpLog, "%s\n", log.c_str());
                ::fflush(g_fpLog);
            }
        } else {
#if !defined(_WIN32)
            // convert our log level into syslog level
            int syslogLevel = LOG_INFO;
            switch (level) {
            case 1U:
                syslogLevel = LOG_DEBUG;
                break;
            case 2U:
            case 9999U: // in-band U: messages should also be info level
                syslogLevel = LOG_INFO;
                break;
            case 3U:
                syslogLevel = LOG_WARNING;
                break;
            case 4U:
                syslogLevel = LOG_ERR;
                break;
            default:
                syslogLevel = LOG_EMERG;
                break;
            }

            syslog(syslogLevel, "%s", log.c_str());
#endif // !defined(_WIN32)
        }
    }

    if (!g_useSyslog && level >= g_logDisplayLevel && g_logDisplayLevel != 0U) {
        ::fprintf(stdout, "%s" EOL, log.c_str());
        ::fflush(stdout);
    }

    // fatal error (specially allow any log levels above 9999)
    if (level >= 5U && level < 9999U) {
        if (g_fpLog != nullptr)
            ::fclose(g_fpLog);
#if !defined(_WIN32)
        if (g_useSyslog)
            ::closelog();
#endif // !defined(_WIN32)
        exit(1);
    }
}

/* Internal helper to get the log file path. */

std::string log_internal::GetLogFilePath()
{
    return g_filePath;
}

/* Internal helper to get the log file root name. */

std::string log_internal::GetLogFileRoot()
{
    return g_fileRoot;
}

/* Internal helper to get the log file handle pointer. */

FILE* log_internal::GetLogFile()
{
    return g_fpLog;
}
