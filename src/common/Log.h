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
/**
 * @defgroup logging Logging Routines
 * @brief Defines and implements logging routines.
 * @ingroup common
 * 
 * @file Log.h
 * @ingroup logging
 * @file Log.cpp
 * @ingroup logging
 */
#if !defined(__LOG_H__)
#define __LOG_H__

#include "common/Defines.h"
#if defined(_WIN32)
#include "common/Clock.h"
#else
#include <sys/time.h>
#endif // defined(_WIN32)

#if !defined(CATCH2_TEST_COMPILATION)
#include "common/backtrace/backward.h"
#endif

#include <ctime>
#include <string>

/**
 * @addtogroup logging
 * @{
 */

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

/** @cond */

#define LOG_HOST    "HOST"
#define LOG_REST    "RESTAPI"
#define LOG_MODEM   "MODEM"
#define LOG_RF      "RF"
#define LOG_NET     "NET"
#define LOG_P25     "P25"
#define LOG_NXDN    "NXDN"
#define LOG_DMR     "DMR"
#define LOG_ANALOG  "ANALOG"
#define LOG_CAL     "CAL"
#define LOG_SETUP   "SETUP"
#define LOG_SERIAL  "SERIAL"
#define LOG_DVMV24  "DVMV24"

/** @endcond */

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

/**
 * @brief Macro helper to create a debug log entry.
 * @param _module Name of module generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function.
 */
#define LogDebug(_module, fmt, ...)             Log(1U, {_module, __FILE__, __LINE__, nullptr}, fmt, ##__VA_ARGS__)
/**
 * @brief Macro helper to create a debug log entry.
 * @param _module Name of module generating log entry.
 * @param _func Name of function generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function.
 */
#define LogDebugEx(_module, _func, fmt, ...)    Log(1U, {_module, __FILE__, __LINE__, _func}, fmt, ##__VA_ARGS__)
/**
 * @brief Macro helper to create a informational log entry.
 * @param _module Name of module generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function. LogInfo() does not use a module
 * name when creating a log entry.
 */
#define LogInfo(fmt, ...)                       Log(2U, {nullptr, nullptr, 0, nullptr}, fmt, ##__VA_ARGS__)
/**
 * @brief Macro helper to create a informational log entry with module name.
 * @param _module Name of module generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function.
 */
#define LogInfoEx(_module, fmt, ...)            Log(2U, {_module, nullptr, 0, nullptr}, fmt, ##__VA_ARGS__)
/**
 * @brief Macro helper to create a warning log entry.
 * @param _module Name of module generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function.
 */
#define LogWarning(_module, fmt, ...)           Log(3U, {_module, nullptr, 0, nullptr}, fmt, ##__VA_ARGS__)
/**
 * @brief Macro helper to create a error log entry.
 * @param _module Name of module generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function.
 */
#define LogError(_module, fmt, ...)             Log(4U, {_module, nullptr, 0, nullptr}, fmt, ##__VA_ARGS__)
/**
 * @brief Macro helper to create a fatal log entry.
 * @param _module Name of module generating log entry.
 * @param fmt String format.
 * 
 * This is a variable argument function.
 */
#define LogFatal(_module, fmt, ...)             Log(5U, {_module, nullptr, 0, nullptr}, fmt, ##__VA_ARGS__)

// ---------------------------------------------------------------------------
//  Externs
// ---------------------------------------------------------------------------

/**
 * @brief (Global) Display log level.
 */
extern uint32_t g_logDisplayLevel;
/**
 * @brief (Global) Flag for displaying timestamps on log entries (does not apply to syslog logging).
 */
extern bool g_disableTimeDisplay;
/**
 * @brief (Global) Flag indicating whether or not logging goes to the syslog.
 */
extern bool g_useSyslog;
/**
 * @brief (Global) Flag indicating whether or not network logging is disabled.
 */
extern bool g_disableNetworkLog;

namespace log_internal
{
    constexpr static char LOG_LEVELS[] = " DIWEF";

    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents a source code location.
     * @ingroup logger
     */
    struct SourceLocation {
    public:
        /**
         * @brief Initializes a new instance of the SourceLocation class.
         */
        constexpr SourceLocation() = default;
        /**
         * @brief Initializes a new instance of the SourceLocation class.
         * @param module Application module.
         * @param filename Source code filename.
         * @param line Line number in source code file.
         * @param func Function name within source code.
         */
        constexpr SourceLocation(const char* module, const char* filename, int line, const char* func) :
            module(module),
            filename(filename),
            line(line),
            funcname(func)
        {
            /* stub */
        }

    public:
        const char* module = nullptr;
        const char* filename = nullptr;
        int line = 0;
        const char* funcname = nullptr;
    };

    /**
     * @brief Internal helper to set an output stream to direct logging to.
     * @param stream 
     */
    extern HOST_SW_API void SetInternalOutputStream(std::ostream& stream);
    /**
     * @brief Writes a new entry to the diagnostics log.
     * @param level Log level for entry.
     * @param log Fully formatted log message.
     */
    extern HOST_SW_API void LogInternal(uint32_t level, const std::string& log);

    /**
     * @brief Internal helper to get the log file path.
     * @returns std::string Configured log file path.
     */
    extern HOST_SW_API std::string GetLogFilePath();
    /**
     * @brief Internal helper to get the log file root name.
     * @returns std::string Configured log file root name.
     */
    extern HOST_SW_API std::string GetLogFileRoot();
    /**
     * @brief Internal helper to get the log file handle pointer.
     * @returns FILE* Pointer to the open log file.
     */
    extern HOST_SW_API FILE* GetLogFile();
} // namespace log_internal

namespace log_stacktrace
{
#if !defined(CATCH2_TEST_COMPILATION)
#if defined(BACKWARD_SYSTEM_LINUX) || defined(BACKWARD_SYSTEM_DARWIN)
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Backward backtrace signal handling class.
     * @ingroup logger
     */
    class HOST_SW_API SignalHandling {
    public:
        /**
         * @brief Helper to generate a default list of POSIX signals to handle.
         * @return std::vector<int> List of POSIX signals.
         */
        static std::vector<int> makeDefaultSignals() {
            const int posixSignals[] = {
                // Signals for which the default action is "Core".
                SIGABRT, // Abort signal from abort(3)
                SIGBUS,  // Bus error (bad memory access)
                SIGFPE,  // Floating point exception
                SIGILL,  // Illegal Instruction
                SIGIOT,  // IOT trap. A synonym for SIGABRT
                SIGQUIT, // Quit from keyboard
                SIGSEGV, // Invalid memory reference
                SIGSYS,  // Bad argument to routine (SVr4)
                SIGTRAP, // Trace/breakpoint trap
                SIGXCPU, // CPU time limit exceeded (4.2BSD)
                SIGXFSZ, // File size limit exceeded (4.2BSD)
    #if defined(BACKWARD_SYSTEM_DARWIN)
                SIGEMT, // emulation instruction executed
    #endif
            };

            return std::vector<int>(posixSignals, posixSignals + sizeof posixSignals / sizeof posixSignals[0]);
        }

        /**
         * @brief Initializes a new instance of the SignalHandling class
         * @param foreground Log stacktrace to stderr.
         * @param posixSignals List of signals to handle.
         */
        SignalHandling(bool foreground, const std::vector<int>& posixSignals = makeDefaultSignals()) : 
            m_loaded(false) 
        {
            bool success = true;

            s_foreground = foreground;

            const size_t stackSize = 1024 * 1024 * 8;
            m_stackContent.reset(static_cast<char *>(malloc(stackSize)));
            if (m_stackContent) {
                stack_t ss;
                ss.ss_sp = m_stackContent.get();
                ss.ss_size = stackSize;
                ss.ss_flags = 0;

                if (sigaltstack(&ss, nullptr) < 0) {
                    success = false;
                }
            } else {
                success = false;
            }

            for (size_t i = 0; i < posixSignals.size(); ++i) {
                struct sigaction action;
                memset(&action, 0, sizeof action);
                action.sa_flags = static_cast<int>(SA_SIGINFO | SA_ONSTACK | SA_NODEFER | SA_RESETHAND);
                sigfillset(&action.sa_mask);
                sigdelset(&action.sa_mask, posixSignals[i]);
    #if defined(__clang__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
    #endif
                action.sa_sigaction = &sig_handler;
    #if defined(__clang__)
    #pragma clang diagnostic pop
    #endif
                int r = sigaction(posixSignals[i], &action, nullptr);
                if (r < 0)
                    success = false;
            }

            m_loaded = success;
        }

        /**
         * @brief Helper to return whether or not the SignalHandling class is loaded.
         * @return bool True if signal handler is loaded, otherwise false.
         */
        bool loaded() const { return m_loaded; }

        /**
         * @brief Helper to handle a signal.
         * @param signo Signal number.
         * @param info Signal informational structure.
         * @param _ctx Signal user context data.
         */
        static void handleSignal(int signo, siginfo_t* info, void* _ctx)
        {
            ucontext_t *uctx = static_cast<ucontext_t *>(_ctx);

            backward::StackTrace st;
            void* errorAddr = nullptr;
    #ifdef REG_RIP // x86_64
            errorAddr = reinterpret_cast<void *>(uctx->uc_mcontext.gregs[REG_RIP]);
    #elif defined(REG_EIP) // x86_32
            errorAddr = reinterpret_cast<void *>(uctx->uc_mcontext.gregs[REG_EIP]);
    #elif defined(__arm__)
            errorAddr = reinterpret_cast<void *>(uctx->uc_mcontext.arm_pc);
    #elif defined(__aarch64__)
        #if defined(__APPLE__)
            errorAddr = reinterpret_cast<void *>(uctx->uc_mcontext->__ss.__pc);
        #else
            errorAddr = reinterpret_cast<void *>(uctx->uc_mcontext.pc);
        #endif
    #elif defined(__mips__)
            errorAddr = reinterpret_cast<void *>(reinterpret_cast<struct sigcontext *>(&uctx->uc_mcontext)->sc_pc);
    #elif defined(__APPLE__) && defined(__POWERPC__)
            errorAddr = reinterpret_cast<void *>(uctx->uc_mcontext->__ss.__srr0);
    #elif defined(__ppc__) || defined(__powerpc) || defined(__powerpc__) ||        \
        defined(__POWERPC__)
            errorAddr = reinterpret_cast<void *>(uctx->uc_mcontext.regs->nip);
    #elif defined(__riscv)
            errorAddr = reinterpret_cast<void *>(uctx->uc_mcontext.__gregs[REG_PC]);
    #elif defined(__s390x__)
            errorAddr = reinterpret_cast<void *>(uctx->uc_mcontext.psw.addr);
    #elif defined(__APPLE__) && defined(__x86_64__)
            errorAddr = reinterpret_cast<void *>(uctx->uc_mcontext->__ss.__rip);
    #elif defined(__APPLE__)
            errorAddr = reinterpret_cast<void *>(uctx->uc_mcontext->__ss.__eip);
    #elif defined(__loongarch__)
            errorAddr = reinterpret_cast<void *>(uctx->uc_mcontext.__pc);
    #else
    #warning ":/ sorry, ain't know no nothing none not of your architecture!"
    #endif

            if (errorAddr) {
                st.load_from(errorAddr, 32, reinterpret_cast<void *>(uctx), info->si_addr);
            } else {
                st.load_here(32, reinterpret_cast<void *>(uctx), info->si_addr);
            }

            backward::Printer p;
            p.address = true;
            p.snippet = false;
            p.color_mode = backward::ColorMode::never;

            log_internal::LogInternal(2U, "UNRECOVERABLE FATAL ERROR!");
            if (s_foreground > 0) {
                p.print(st, stderr);
            }

            std::string filePath = log_internal::GetLogFilePath();
            std::string fileRoot = log_internal::GetLogFileRoot();

            time_t now;
            ::time(&now);

            struct tm* tm = ::localtime(&now);

            char filename[200U];
            ::sprintf(filename, "%s/%s-%04d-%02d-%02d.stacktrace.log", filePath.c_str(), fileRoot.c_str(), tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

            // log stack trace to a file if we're using syslog
            if (g_useSyslog) {
                FILE* stacktraceFp = ::fopen(filename, "a+t");
                ::fprintf(stacktraceFp, "UNRECOVERABLE FATAL ERROR!\r\n");
                p.print(st, stacktraceFp);
                ::fflush(stacktraceFp);
                ::fclose(stacktraceFp);
            } else {
                FILE *stacktraceFp = log_internal::GetLogFile();
                if (stacktraceFp == nullptr) {
                    stacktraceFp = ::fopen(filename, "a+t");
                    ::fprintf(stacktraceFp, "UNRECOVERABLE FATAL ERROR!\r\n");
                }
                p.print(st, stacktraceFp);
                ::fflush(stacktraceFp);
                ::fclose(stacktraceFp);
            }

            (void)info;
        }

    private:
        backward::details::handle<char*> m_stackContent;
        bool m_loaded;
        static bool s_foreground;

        /**
         * @brief Internal helper to handle a signal.
         * @param signo Signal number.
         * @param info Signal informational structure.
         * @param _ctx Signal user context data.
         */
    #ifdef __GNUC__
        __attribute__((noreturn))
    #endif
        static void sig_handler(int signo, siginfo_t* info, void* _ctx)
        {
            handleSignal(signo, info, _ctx);

            // try to forward the signal.
            raise(info->si_signo);

            // terminate the process immediately.
            puts("Abnormal termination.");
            _exit(EXIT_FAILURE);
        }
    };
#endif // BACKWARD_SYSTEM_LINUX || BACKWARD_SYSTEM_DARWIN
#ifdef BACKWARD_SYSTEM_WINDOWS
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Backward backtrace signal handling class.
     * @ingroup logger
     */
    class HOST_SW_API SignalHandling {
    public:
        /**
         * @brief Initializes a new instance of the SignalHandling class
         * @param foreground Log stacktrace to stderr. (Windows always logs to the foreground, this is ignored.)
         * @param posixSignals List of signals to handle.
         */
        SignalHandling(bool foreground, const std::vector<int>& = std::vector<int>()) : 
            m_reporterThread([]() {
                /* We handle crashes in a utility thread:
                ** backward structures and some Windows functions called here
                ** need stack space, which we do not have when we encounter a
                ** stack overflow.
                ** To support reporting stack traces during a stack overflow,
                ** we create a utility thread at startup, which waits until a
                ** crash happens or the program exits normally.
                */
                {
                    std::unique_lock<std::mutex> lk(mtx());
                    cv().wait(lk, [] { return crashed() != CRASH_STATUS::RUNNING; });
                }

                if (crashed() == CRASH_STATUS::CRASHED) {
                    handleStackTrace(skipRecs());
                }

                {
                    std::unique_lock<std::mutex> lk(mtx());
                    crashed() = CRASH_STATUS::ENDING;
                }
                cv().notify_one();
            }) 
        {
            SetUnhandledExceptionFilter(crashHandler);

            signal(SIGABRT, signalHandler);
            _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

            std::set_terminate(&terminator);
    #ifndef BACKWARD_ATLEAST_CXX17
            std::set_unexpected(&terminator);
    #endif
            _set_purecall_handler(&terminator);
            _set_invalid_parameter_handler(&invalidParameterHandler);
        }
        /**
         * @brief Finalizes a instance of the SignalHandling class
         */
        ~SignalHandling()
        {
            {
                std::unique_lock<std::mutex> lk(mtx());
                crashed() = CRASH_STATUS::NORMAL_EXIT;
            }

            cv().notify_one();

            m_reporterThread.join();
        }

        /**
         * @brief Helper to return whether or not the SignalHandling class is loaded.
         * @return bool True if signal handler is loaded, otherwise false.
         */
        bool loaded() const { return true; }

    private:
        enum class CRASH_STATUS {
            RUNNING,
            CRASHED,
            NORMAL_EXIT,
            ENDING
        };

        /**
         * @brief 
         * @return CONTEXT* 
         */
        static CONTEXT* ctx()
        {
            static CONTEXT data;
            return &data;
        }

        /**
         * @brief 
         * @return crash_status& 
         */
        static CRASH_STATUS& crashed()
        {
            static CRASH_STATUS data;
            return data;
        }

        /**
         * @brief 
         * @return std::mutex& 
         */
        static std::mutex& mtx()
        {
            static std::mutex data;
            return data;
        }

        /**
         * @brief 
         * @return std::condition_variable& 
         */
        static std::condition_variable& cv()
        {
            static std::condition_variable data;
            return data;
        }

        /**
         * @brief 
         * @return HANDLE& 
         */
        static HANDLE& threadHandle()
        {
            static HANDLE handle;
            return handle;
        }

        std::thread m_reporterThread;
        static bool s_foreground;

        // TODO: how not to hardcode these?
        static const constexpr int signalSkipRecs =
    #ifdef __clang__
            // With clang, RtlCaptureContext also captures the stack frame of the
            // current function Below that, there are 3 internal Windows functions
            4
    #else
            // With MSVC cl, RtlCaptureContext misses the stack frame of the current
            // function The first entries during StackWalk are the 3 internal Windows
            // functions
            3
    #endif
            ;

        /**
         * @brief 
         * @return int& 
         */
        static int& skipRecs()
        {
            static int data;
            return data;
        }

        /**
         * @brief 
         */
        static inline void terminator()
        {
            crashHandler(signalSkipRecs);
            abort();
        }

        /**
         * @brief 
         */
        static inline void signalHandler(int)
        {
            crashHandler(signalSkipRecs);
            abort();
        }

        /**
         * @brief 
         * @param int 
         */
        static inline void __cdecl invalidParameterHandler(const wchar_t *, const wchar_t *, const wchar_t *, 
                unsigned int, uintptr_t)
        {
            crashHandler(signalSkipRecs);
            abort();
        }

        /**
         * @brief 
         * @param info 
         * @return NOINLINE 
         */
        NOINLINE static LONG WINAPI crashHandler(EXCEPTION_POINTERS* info)
        {
            // the exception info supplies a trace from exactly where the issue was,
            // no need to skip records
            crashHandler(0, info->ContextRecord);
            return EXCEPTION_CONTINUE_SEARCH;
        }

        /**
         * @brief 
         * @param skip 
         * @param ct 
         * @return NOINLINE 
         */
        NOINLINE static void crashHandler(int skip, CONTEXT* ct = nullptr)
        {
            if (ct == nullptr) {
                RtlCaptureContext(ctx());
            } else {
                memcpy(ctx(), ct, sizeof(CONTEXT));
            }

            DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), 
                GetCurrentProcess(), &threadHandle(), 0, FALSE, DUPLICATE_SAME_ACCESS);

            skipRecs() = skip;

            {
                std::unique_lock<std::mutex> lk(mtx());
                crashed() = CRASH_STATUS::CRASHED;
            }

            cv().notify_one();

            {
                std::unique_lock<std::mutex> lk(mtx());
                cv().wait(lk, [] { return crashed() != CRASH_STATUS::CRASHED; });
            }
        }

        /**
         * @brief 
         * @param skipFrames 
         */
        static void handleStackTrace(int skipFrames = 0)
        {
            // printer creates the TraceResolver, which can supply us a machine type
            // for stack walking. Without this, StackTrace can only guess using some
            // macros.
            // StackTrace also requires that the PDBs are already loaded, which is done
            // in the constructor of TraceResolver
            backward::Printer p;

            backward::StackTrace st;
            st.set_machine_type(p.resolver().machine_type());
            st.set_thread_handle(threadHandle());
            st.load_here(32 + skipFrames, ctx());
            st.skip_n_firsts(skipFrames);

            p.address = true;
            p.snippet = false;
            p.color_mode = backward::ColorMode::never;

            log_internal::LogInternal(2U, "UNRECOVERABLE FATAL ERROR!");
            p.print(st, std::cerr);

            std::string filePath = log_internal::GetLogFilePath();
            std::string fileRoot = log_internal::GetLogFileRoot();

            time_t now;
            ::time(&now);

            struct tm* tm = ::localtime(&now);

            char filename[200U];
            ::sprintf(filename, "%s/%s-%04d-%02d-%02d.stacktrace.log", filePath.c_str(), fileRoot.c_str(), tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

            // log stack trace to a file if we're using syslog
            FILE *stacktraceFp = log_internal::GetLogFile();
            if (stacktraceFp == nullptr) {
                stacktraceFp = ::fopen(filename, "a+t");
                ::fprintf(stacktraceFp, "UNRECOVERABLE FATAL ERROR!\r\n");
            }

            p.print(st, stacktraceFp);
            ::fflush(stacktraceFp);
            ::fclose(stacktraceFp);
        }
    };
#endif // BACKWARD_SYSTEM_WINDOWS
#ifdef BACKWARD_SYSTEM_UNKNOWN
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Backward backtrace signal handling class.
     * @ingroup logger
     */
    class HOST_SW_API SignalHandling {
    public:
        /**
         * @brief Initializes a new instance of the SignalHandling class
         * @param foreground Log stacktrace to stderr.
         */
        SignalHandling(bool forground, const std::vector<int>& = std::vector<int>())
        {
            /* stub */
        }

        /**
         * @brief Helper to return whether or not the SignalHandling class is loaded.
         * @return bool True if signal handler is loaded, otherwise false.
         */
        bool loaded() { return false; }

    private:
        static bool s_foreground;
    };
#endif // BACKWARD_SYSTEM_UNKNOWN
#endif // !defined(CATCH2_TEST_COMPILATION)
}

// ---------------------------------------------------------------------------
//  Global Function Externs
// ---------------------------------------------------------------------------

/**
 * @brief Helper to get the current log file level.
 * @returns uint32_t Current log file level.
 */
extern HOST_SW_API uint32_t CurrentLogFileLevel();

/**
 * @brief Helper to get the current log file path.
 * @returns std::string Current log file path.
 */
extern HOST_SW_API std::string LogGetFilePath();
/**
 * @brief Helper to get the current log file root.
 * @returns std::string Current log file root.
 */
extern HOST_SW_API std::string LogGetFileRoot();

/**
 * @brief Gets the instance of the Network class to transfer the activity log with.
 * @returns void* 
 */
extern HOST_SW_API void* LogGetNetwork();
/**
 * @brief Sets the instance of the Network class to transfer the activity log with.
 * @param network 
 */
extern HOST_SW_API void LogSetNetwork(void* network);

/**
 * @brief Initializes the diagnostics log.
 * @param filePath File path for the log file.
 * @param fileRoot Root name for log file.
 * @param fileLevel File log level.
 * @param displaylevel Display log level.
 * @param displayTimeDisplay Flag to disable the date and time stamp for the log entries.
 * @param syslog Flag indicating whether or not logs will be sent to syslog.
 * @returns 
 */
extern HOST_SW_API bool LogInitialise(const std::string& filePath, const std::string& fileRoot, 
    uint32_t fileLevel, uint32_t displayLevel, bool disableTimeDisplay = false, bool useSyslog = false);
/**
 * @brief Finalizes the diagnostics log.
 */
extern HOST_SW_API void LogFinalise();

/**
 * @brief Writes a new entry to the diagnostics log.
 * @param level Log level for entry.
 * @param sourceLog Source code location information.
 * @param fmt String format.
 * 
 * This is a variable argument function. This shouldn't be called directly, utilize the LogXXXX macros above, instead.
 */
template<typename ... Args>
HOST_SW_API void Log(uint32_t level, log_internal::SourceLocation sourceLoc, const std::string& fmt, Args... args)
{
    using namespace log_internal;

    int size_s = std::snprintf(nullptr, 0, fmt.c_str(), args...) + 1; // Extra space for '\0'
    if (size_s <= 0) {
        throw std::runtime_error("Error during formatting.");
    }

#if defined(CATCH2_TEST_COMPILATION)
    g_disableTimeDisplay = true;
#endif
    int prefixLen = 0;
    char prefixBuf[256];

    if (!g_disableTimeDisplay && !g_useSyslog) {
        time_t now;
        ::time(&now);
        struct tm* tm = ::localtime(&now);

        struct timeval nowMillis;
        ::gettimeofday(&nowMillis, NULL);

        if (level > 6U)
            level = 2U; // default this sort of log message to INFO

        if (sourceLoc.module != nullptr) {
            // level 1 is DEBUG
            if (level == 1U) {
                // if we have a file and line number -- add that to the log entry
                if (sourceLoc.filename != nullptr && sourceLoc.line > 0) {
                    // if we have a function name add that to the log entry
                    if (sourceLoc.funcname != nullptr) {
                        prefixLen = ::sprintf(prefixBuf, "%c: %04d-%02d-%02d %02d:%02d:%02d.%03lu (%s)[%s:%u][%s] ", LOG_LEVELS[level], 
                            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, nowMillis.tv_usec / 1000U, 
                            sourceLoc.module, sourceLoc.filename, sourceLoc.line, sourceLoc.funcname);
                    }
                    else {
                        prefixLen = ::sprintf(prefixBuf, "%c: %04d-%02d-%02d %02d:%02d:%02d.%03lu (%s)[%s:%u] ", LOG_LEVELS[level], 
                            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, nowMillis.tv_usec / 1000U, 
                            sourceLoc.module, sourceLoc.filename, sourceLoc.line);
                    }
                } else {
                    prefixLen = ::sprintf(prefixBuf, "%c: %04d-%02d-%02d %02d:%02d:%02d.%03lu (%s) ", LOG_LEVELS[level], 
                        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, nowMillis.tv_usec / 1000U, 
                        sourceLoc.module);
                }
            } else {
                prefixLen = ::sprintf(prefixBuf, "%c: %04d-%02d-%02d %02d:%02d:%02d.%03lu (%s) ", LOG_LEVELS[level], 
                    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, nowMillis.tv_usec / 1000U, 
                    sourceLoc.module);
            }
        }
        else {
            // level 1 is DEBUG
            if (level == 1U) {
                // if we have a file and line number -- add that to the log entry
                if (sourceLoc.filename != nullptr && sourceLoc.line > 0) {
                    // if we have a function name add that to the log entry
                    if (sourceLoc.funcname != nullptr) {
                        prefixLen = ::sprintf(prefixBuf, "%c: %04d-%02d-%02d %02d:%02d:%02d.%03lu [%s:%u][%s] ", LOG_LEVELS[level], 
                            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, nowMillis.tv_usec / 1000U, 
                            sourceLoc.filename, sourceLoc.line, sourceLoc.funcname);
                    }
                    else {
                        prefixLen = ::sprintf(prefixBuf, "%c: %04d-%02d-%02d %02d:%02d:%02d.%03lu [%s:%u] ", LOG_LEVELS[level], 
                            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, nowMillis.tv_usec / 1000U, 
                            sourceLoc.filename, sourceLoc.line);
                    }
                } else {
                    prefixLen = ::sprintf(prefixBuf, "%c: %04d-%02d-%02d %02d:%02d:%02d.%03lu ", LOG_LEVELS[level], tm->tm_year + 1900, 
                        tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, nowMillis.tv_usec / 1000U);
                }
            } else {
                prefixLen = ::sprintf(prefixBuf, "%c: %04d-%02d-%02d %02d:%02d:%02d.%03lu ", LOG_LEVELS[level], 
                    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, nowMillis.tv_usec / 1000U);
            }
        }
    }
    else {
        if (sourceLoc.module != nullptr) {
            if (level > 6U)
                level = 2U; // default this sort of log message to INFO

            // level 1 is DEBUG
            if (level == 1U) {
                // if we have a file and line number -- add that to the log entry
                if (sourceLoc.filename != nullptr && sourceLoc.line > 0) {
                    // if we have a function name add that to the log entry
                    if (sourceLoc.funcname != nullptr) {
                        prefixLen = ::sprintf(prefixBuf, "%c: (%s)[%s:%u][%s] ", LOG_LEVELS[level], 
                            sourceLoc.module, sourceLoc.filename, sourceLoc.line, sourceLoc.funcname);
                    }
                    else {
                        prefixLen = ::sprintf(prefixBuf, "%c: (%s)[%s:%u] ", LOG_LEVELS[level], 
                            sourceLoc.module, sourceLoc.filename, sourceLoc.line);
                    }
                }
                else {
                    prefixLen = ::sprintf(prefixBuf, "%c: (%s) ", LOG_LEVELS[level], 
                        sourceLoc.module);
                }
            } else {
                prefixLen = ::sprintf(prefixBuf, "%c: (%s) ", LOG_LEVELS[level], 
                    sourceLoc.module);
            }
        }
        else {
            if (level >= 9999U) {
                prefixLen = ::sprintf(prefixBuf, "U: ");
            }
            else {
                if (level > 6U)
                    level = 2U; // default this sort of log message to INFO

                 // if we have a file and line number -- add that to the log entry
                 if (sourceLoc.filename != nullptr && sourceLoc.line > 0) {
                    // if we have a function name add that to the log entry
                    if (sourceLoc.funcname != nullptr) {
                        prefixLen = ::sprintf(prefixBuf, "%c: [%s:%u][%s] ", LOG_LEVELS[level], 
                            sourceLoc.filename, sourceLoc.line, sourceLoc.funcname);
                    }
                    else {
                        prefixLen = ::sprintf(prefixBuf, "%c: [%s:%u] ", LOG_LEVELS[level], 
                            sourceLoc.filename, sourceLoc.line);
                    }
                }
                else {
                    prefixLen = ::sprintf(prefixBuf, "%c: ", LOG_LEVELS[level]);
                }
            }
        }
    }

    auto size = static_cast<size_t>(size_s);
    auto buf = std::make_unique<char[]>(size);

    std::snprintf(buf.get(), size, fmt.c_str(), args ...);

    std::string prefix = std::string(prefixBuf, prefixBuf + prefixLen);
    std::string msg = std::string(buf.get(), buf.get() + size - 1);

    LogInternal(level, std::string(prefix + msg));
}

/** @} */
#endif // __LOG_H__
