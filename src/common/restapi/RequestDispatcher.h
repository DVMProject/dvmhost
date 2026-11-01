// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023,2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup rest REST Services
 * @brief Implementation for REST services.
 * @defgroup http Embedded HTTP Core
 * @brief Implementation for basic HTTP services.
 * @ingroup rest
 * 
 * @file RequestDispatcher.h
 * @ingroup rest
 */
#if !defined(__REST__DISPATCHER_H__)
#define __REST__DISPATCHER_H__

#include "common/Defines.h"
#include "common/restapi/http/HTTPPayload.h"
#include "common/Log.h"

#include <functional>
#include <map>
#include <string>
#include <regex>
#include <memory>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

namespace restapi
{
    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Per-request cooperative cancellation context.
     * @ingroup rest
     */
    struct RequestExecutionContext {
        RequestExecutionContext(std::string m, std::string u, std::string e, uint32_t timeoutSec) :
            cancelled(false),
            started(std::chrono::steady_clock::now()),
            timeout(std::chrono::seconds(timeoutSec)),
            method(std::move(m)),
            uri(std::move(u)),
            expression(std::move(e))
        {
            /* stub */
        }

        std::atomic<bool> cancelled;
        std::chrono::steady_clock::time_point started;
        std::chrono::steady_clock::duration timeout;
        std::string method;
        std::string uri;
        std::string expression;
    };

    /**
     * @brief Internal helper to access request context TLS.
     */
    inline const RequestExecutionContext*& requestContextTLS()
    {
        static thread_local const RequestExecutionContext* s_ctx = nullptr;
        return s_ctx;
    }

    /**
     * @brief Returns true when current request has been cooperatively cancelled.
     * @ingroup rest
     */
    inline bool currentRequestCancelled()
    {
        const RequestExecutionContext* ctx = requestContextTLS();
        return (ctx != nullptr) ? ctx->cancelled.load() : false;
    }

    /**
     * @brief Returns elapsed milliseconds for the current request, or 0 if none.
     * @ingroup rest
     */
    inline uint64_t currentRequestElapsedMs()
    {
        const RequestExecutionContext* ctx = requestContextTLS();
        if (ctx == nullptr) {
            return 0U;
        }

        return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - ctx->started).count();
    }

    /**
     * @brief Returns remaining milliseconds before request timeout, or 0 if none/expired.
     * @ingroup rest
     */
    inline uint64_t currentRequestRemainingMs()
    {
        const RequestExecutionContext* ctx = requestContextTLS();
        if (ctx == nullptr) {
            return 0U;
        }

        auto now = std::chrono::steady_clock::now();
        auto deadline = ctx->started + ctx->timeout;
        if (now >= deadline) {
            return 0U;
        }

        return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
    }

    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Structure representing a REST API request match.
     * @ingroup rest
     */
    struct RequestMatch : std::smatch {
        /**
         * @brief Initializes a new instance of the RequestMatch structure.
         * @param m String matcher.
         * @param c Content.
         */
        RequestMatch(const std::smatch& m, const std::string& c) : std::smatch(m), content(c) { /* stub */ }

        std::string content;
    };

    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Structure representing a request matcher.
     * @ingroup rest
     */
    template<typename Request, typename Reply>
    struct RequestMatcher {
        typedef std::function<void(const Request&, Reply&, const RequestMatch&)> RequestHandlerType;

        /**
         * @brief Initializes a new instance of the RequestMatcher structure.
         * @param expression Matching expression.
         */
        explicit RequestMatcher(const std::string& expression) : m_expression(expression), m_isRegEx(false) { /* stub */ }

        /**
         * @brief Handler for GET requests.
         * @param handler GET request handler.
         * @return RequestMatcher* Instance of a RequestMatcher.
         */
        RequestMatcher<Request, Reply>& get(RequestHandlerType handler) {
            m_handlers[HTTP_GET] = handler;
            return *this;
        }
        /**
         * @brief Handler for POST requests.
         * @param handler POST request handler.
         * @return RequestMatcher* Instance of a RequestMatcher.
         */
        RequestMatcher<Request, Reply>& post(RequestHandlerType handler) {
            m_handlers[HTTP_POST] = handler;
            return *this;
        }
        /**
         * @brief Handler for PUT requests.
         * @param handler PUT request handler.
         * @return RequestMatcher* Instance of a RequestMatcher.
         */
        RequestMatcher<Request, Reply>& put(RequestHandlerType handler) {
            m_handlers[HTTP_PUT] = handler;
            return *this;
        }
        /**
         * @brief Handler for DELETE requests.
         * @param handler DELETE request handler.
         * @return RequestMatcher* Instance of a RequestMatcher.
         */
        RequestMatcher<Request, Reply>& del(RequestHandlerType handler) {
            m_handlers[HTTP_DELETE] = handler;
            return *this;
        }
        /**
         * @brief Handler for OPTIONS requests.
         * @param handler OPTIONS request handler.
         * @return RequestMatcher* Instance of a RequestMatcher.
         */
        RequestMatcher<Request, Reply>& options(RequestHandlerType handler) {
            m_handlers[HTTP_OPTIONS] = handler;
            return *this;
        }

        /**
         * @brief Helper to determine if the request matcher is a regular expression.
         * @returns bool True, if request matcher is a regular expression, otherwise false.
         */
        bool regex() const { return m_isRegEx; }
        /**
         * @brief Helper to set the regular expression flag.
         * @param regEx Flag indicating whether or not the request matcher is a regular expression.
         */
        void setRegEx(bool regEx) { m_isRegEx = regEx; }

        /**
         * @brief Helper to handle the actual request.
         * @param request HTTP request.
         * @param reply HTTP reply.
         * @param what What matched.
         */
        void handleRequest(const Request& request, Reply& reply, const std::smatch &what) {
            // dispatching to matching based on handler
            RequestMatch match(what, request.content);
            auto& handler = m_handlers[request.method];
            if (handler) {
                handler(request, reply, match);
            }
        }

    private:
        std::string m_expression;
        bool m_isRegEx;
        std::map<std::string, RequestHandlerType> m_handlers;
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief This class implements RESTful web request dispatching.
     * @tparam Request HTTP request.
     * @tparam Reply HTTP reply.
     */
    template<typename Request = http::HTTPPayload, typename Reply = http::HTTPPayload>
    class RequestDispatcher {
        typedef RequestMatcher<Request, Reply> MatcherType;

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Fixed-size executor for dispatching request handlers with deadlines.
         */
        class HandlerExecutor {
        public:
            /**
             * @brief Initializes a new instance of the HandlerExecutor class.
             * @param workers Number of worker threads to use. If 0, defaults to number of hardware threads (capped at 8).
             */
            explicit HandlerExecutor(size_t workers = 0U) :
                m_workers(),
                m_queue(),
                m_mutex(),
                m_cv(),
                m_stop(false)
            {
                size_t count = workers;
                if (count == 0U) {
                    count = std::thread::hardware_concurrency();
                    if (count == 0U) {
                        count = 4U;
                    }
                    if (count > 8U) {
                        count = 8U;
                    }
                }

                for (size_t i = 0U; i < count; i++) {
                    m_workers.emplace_back([this]() {
                        while (true) {
                            std::packaged_task<void()> task;

                            {
                                std::unique_lock<std::mutex> lock(m_mutex);
                                m_cv.wait(lock, [this]() { return m_stop || !m_queue.empty(); });
                                if (m_stop && m_queue.empty()) {
                                    return;
                                }

                                task = std::move(m_queue.front());
                                m_queue.pop_front();
                            }

                            task();
                        }
                    });
                }
            }
            /**
             * @brief Finalizes a instance of the HandlerExecutor class.
             */
            ~HandlerExecutor()
            {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_stop = true;
                }
                m_cv.notify_all();

                for (auto& worker : m_workers) {
                    if (worker.joinable()) {
                        worker.join();
                    }
                }
            }

            /**
             * @brief Submits a callable task for execution.
             * @tparam Callable Type of the callable task.
             * @param callable Callable task to execute.
             * @returns std::future<void> Future representing the completion of the task.
             */
            template<typename Callable>
            std::future<void> submit(Callable&& callable)
            {
                std::packaged_task<void()> task(std::forward<Callable>(callable));
                std::future<void> future = task.get_future();

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_queue.emplace_back(std::move(task));
                }

                m_cv.notify_one();
                return future;
            }

        private:
            std::vector<std::thread> m_workers;
            std::deque<std::packaged_task<void()>> m_queue;
            std::mutex m_mutex;
            std::condition_variable m_cv;
            bool m_stop;
        };

    public:
        /**
         * @brief Initializes a new instance of the RequestDispatcher class.
         */
        RequestDispatcher() : m_basePath(), m_debug(false), m_warnSec(30U), m_timeoutSec(60U), m_executor(std::make_shared<HandlerExecutor>()) { /* stub */ }
        /**
         * @brief Initializes a new instance of the RequestDispatcher class.
         * @param debug Flag indicating whether or not verbose logging should be enabled.
         */
        RequestDispatcher(bool debug) : m_basePath(), m_debug(debug), m_warnSec(30U), m_timeoutSec(60U), m_executor(std::make_shared<HandlerExecutor>()) { /* stub */ }
        /**
         * @brief Initializes a new instance of the RequestDispatcher class.
         * @param basePath 
         * @param debug Flag indicating whether or not verbose logging should be enabled.
         */
        RequestDispatcher(const std::string& basePath, bool debug) : m_basePath(basePath), m_debug(debug), m_warnSec(30U), m_timeoutSec(60U), m_executor(std::make_shared<HandlerExecutor>()) { /* stub */ }

        /**
         * @brief Helper to match a request patch.
         * @param expression Matching expression.
         * @param regex Flag indicating whether or not this match is a regular expression.
         * @returns MatcherType Instance of a request matcher.
         */
        MatcherType& match(const std::string& expression, bool regex = false)
        {
            MatcherTypePtr& p = m_matchers[expression];
            if (!p) {
                if (m_debug) {
                    ::LogDebug(LOG_REST, "creating RequestDispatcher, expression = %s", expression.c_str());
                }
                p = std::make_shared<MatcherType>(expression);
            } else {
                if (m_debug) {
                    ::LogDebug(LOG_REST, "fetching RequestDispatcher, expression = %s", expression.c_str());
                }
            }

            p->setRegEx(regex);
            return *p;
        }

        /**
         * @brief Helper to handle HTTP request.
         * @param request HTTP request.
         * @param reply HTTP reply.
         */
        void handleRequest(const Request& request, Reply& reply)
        {
            // dispatch matched handlers on a bounded worker pool to support cooperative
            // cancellation and timeout response without changing endpoint signatures.
            auto guardedDispatch = [&](const std::string& expression, MatcherType& matcher, const std::smatch& what) {
                const uint32_t warnSec = (m_warnSec > 0U) ? m_warnSec : 30U;
                const uint32_t timeoutSec = (m_timeoutSec > warnSec) ? m_timeoutSec : 60U;

                auto requestCtx = std::make_shared<RequestExecutionContext>(request.method, request.uri, expression, timeoutSec);
                auto workerReply = std::make_shared<Reply>();

                Request requestCopy = request;
                std::smatch whatCopy = what;

                std::future<void> fut = m_executor->submit([&matcher, requestCopy, whatCopy, requestCtx, workerReply]() {
                    const RequestExecutionContext* prevCtx = requestContextTLS();
                    requestContextTLS() = requestCtx.get();

                    try {
                        matcher.handleRequest(requestCopy, *workerReply, whatCopy);
                    }
                    catch (...) {
                        requestContextTLS() = prevCtx;
                        throw;
                    }

                    requestContextTLS() = prevCtx;
                });

                std::shared_future<void> sharedFut = fut.share();

                bool warned = false;
                bool timedOut = false;
                auto nextStillLog = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSec);

                while (sharedFut.wait_for(std::chrono::seconds(1U)) != std::future_status::ready) {
                    const uint64_t elapsed = (uint64_t)std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - requestCtx->started).count();

                    if (!warned && elapsed >= warnSec) {
                        warned = true;
                        ::LogWarning(LOG_REST, "slow REST handler, method = %s, uri = %s, expression = %s, elapsed = %us",
                            request.method.c_str(), request.uri.c_str(), expression.c_str(), (uint32_t)elapsed);
                    }

                    if (!timedOut && elapsed >= timeoutSec) {
                        timedOut = true;
                        requestCtx->cancelled.store(true);

                        ::LogError(LOG_REST, "stalled REST handler, method = %s, uri = %s, expression = %s, elapsed = %us (cooperative cancel requested)",
                            request.method.c_str(), request.uri.c_str(), expression.c_str(), (uint32_t)elapsed);

                        // keep watcher active in a detached monitor to report completion after cancel.
                        std::shared_future<void> monitorFut = sharedFut;
                        std::shared_ptr<RequestExecutionContext> monitorCtx = requestCtx;
                        std::thread([monitorFut, monitorCtx, timeoutSec]() {
                            auto stillLogEvery = std::chrono::seconds(timeoutSec);
                            while (monitorFut.wait_for(stillLogEvery) != std::future_status::ready) {
                                uint64_t monitorElapsed = (uint64_t)std::chrono::duration_cast<std::chrono::seconds>(
                                    std::chrono::steady_clock::now() - monitorCtx->started).count();
                                ::LogError(LOG_REST, "REST handler still running after cancel, method = %s, uri = %s, expression = %s, elapsed = %us",
                                    monitorCtx->method.c_str(), monitorCtx->uri.c_str(), monitorCtx->expression.c_str(), (uint32_t)monitorElapsed);
                            }

                            uint64_t totalElapsed = (uint64_t)std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::steady_clock::now() - monitorCtx->started).count();
                            ::LogWarning(LOG_REST, "REST handler completed after cancellation request, method = %s, uri = %s, expression = %s, elapsed = %us",
                                monitorCtx->method.c_str(), monitorCtx->uri.c_str(), monitorCtx->expression.c_str(), (uint32_t)totalElapsed);
                        }).detach();

                        reply = http::HTTPPayload::statusPayload(http::HTTPPayload::SERVICE_UNAVAILABLE, "application/json");
                        return;
                    }

                    if (timedOut && std::chrono::steady_clock::now() >= nextStillLog) {
                        nextStillLog += std::chrono::seconds(timeoutSec);
                    }
                }

                // worker completed before timeout; propagate any exception and then reply.
                sharedFut.get();
                reply = *workerReply;
            };

            for (const auto& matcher : m_matchers) {
                std::smatch what;
                if (!matcher.second->regex()) {
                    if (request.uri.find(matcher.first) != std::string::npos) {
                        if (m_debug) {
                            ::LogDebug(LOG_REST, "non-regex endpoint, uri = %s, expression = %s", request.uri.c_str(), matcher.first.c_str());
                        }

                        //what = matcher.first;

                        // ensure CORS headers are added
                        reply.headers.add("Access-Control-Allow-Origin", "*");
                        reply.headers.add("Access-Control-Allow-Methods", "*");
                        reply.headers.add("Access-Control-Allow-Headers", "*");
                        
                        if (request.method == HTTP_OPTIONS) {
                            reply.status = http::HTTPPayload::OK;
                        }

                        guardedDispatch(matcher.first, *matcher.second, what);
                        return;
                    }
                } else {
                    if (std::regex_match(request.uri, what, std::regex(matcher.first))) {
                        if (m_debug) {
                            ::LogDebug(LOG_REST, "regex endpoint, uri = %s, expression = %s", request.uri.c_str(), matcher.first.c_str());
                        }

                        guardedDispatch(matcher.first, *matcher.second, what);
                        return;
                    }
                }
            }

            ::LogError(LOG_REST, "unknown endpoint, uri = %s", request.uri.c_str());
            reply = http::HTTPPayload::statusPayload(http::HTTPPayload::BAD_REQUEST, "application/json");
        }

    private:
        typedef std::shared_ptr<MatcherType> MatcherTypePtr;

        std::string m_basePath;
        std::map<std::string, MatcherTypePtr> m_matchers;

        bool m_debug;
        uint32_t m_warnSec;
        uint32_t m_timeoutSec;
        std::shared_ptr<HandlerExecutor> m_executor;
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief This class implements a generic basic request dispatcher.
     * @tparam Request HTTP request.
     * @tparam Reply HTTP reply.
     */
    template<typename Request = http::HTTPPayload, typename Reply = http::HTTPPayload>
    class BasicRequestDispatcher {
    public:
        typedef std::function<void(const Request&, Reply&)> RequestHandlerType;

        /**
         * @brief Initializes a new instance of the BasicRequestDispatcher class.
         */
        BasicRequestDispatcher() { /* stub */ }
        /**
         * @brief Initializes a new instance of the BasicRequestDispatcher class.
         * @param handler Instance of a RequestHandlerType for this dispatcher.
         */
        BasicRequestDispatcher(RequestHandlerType handler) : m_handler(handler) { /* stub */ }

        /**
         * @brief Helper to handle HTTP request.
         * @param request HTTP request.
         * @param reply HTTP reply.
         */
        void handleRequest(const Request& request, Reply& reply)
        {
            if (m_handler) {
                m_handler(request, reply);
            }
        }

    private:
        RequestHandlerType m_handler;
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief This class implements a generic debug request dispatcher.
     * @tparam Request HTTP request.
     * @tparam Reply HTTP reply.
     */
    template<typename Request = http::HTTPPayload, typename Reply = http::HTTPPayload>
    class DebugRequestDispatcher {
    public:
        /**
         * @brief Initializes a new instance of the DebugRequestDispatcher class.
         */
        DebugRequestDispatcher() { /* stub */ }

        /**
         * @brief Helper to handle HTTP request.
         * @param request HTTP request.
         * @param reply HTTP reply.
         */
        void handleRequest(const Request& request, Reply& reply)
        {
            for (auto header : request.headers.headers())
                ::LogDebugEx(LOG_REST, "DebugRequestDispatcher::handleRequest()", "header = %s, value = %s", header.name.c_str(), header.value.c_str());

            ::LogDebugEx(LOG_REST, "DebugRequestDispatcher::handleRequest()", "content = %s", request.content.c_str());
        }
    };

    typedef RequestDispatcher<http::HTTPPayload, http::HTTPPayload> DefaultRequestDispatcher;
} // namespace restapi

#endif // __REST__DISPATCHER_H__
