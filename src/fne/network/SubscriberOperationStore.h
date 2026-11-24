// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file SubscriberOperationStore.h
 * @ingroup fne_network
 * @file SubscriberOperationStore.cpp
 * @ingroup fne_network
 */
#if !defined(__SUBSCRIBER_OPERATION_STORE_H__)
#define __SUBSCRIBER_OPERATION_STORE_H__

#include "fne/Defines.h"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace network
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Stores short-lived FNE subscriber-management operations and status observations.
     * @ingroup fne_network
     */
    class HOST_SW_API SubscriberOperationStore {
    public:
        /**
         * @brief Supported air-interface protocols.
         * @ingroup fne_network
         */
        enum class Protocol : uint8_t {
            DMR = 0U,               //!< Digital Mobile Radio.
            P25 = 1U,               //!< Project 25.
            NXDN = 2U,              //!< Next Generation Digital Narrowband.

            SYSTEM = 3U,            //!< System-level operation.
        };

        /**
         * @brief Supported operation kinds.
         * @ingroup fne_network
         */
        enum class Kind : uint8_t {
            RADIO_CHECK = 0U,       //!< Subscriber presence check.
            STATUS_QUERY = 1U,      //!< Subscriber status query.
            MESSAGE_UPDATE = 2U,    //!< Subscriber coded-message update.
            INHIBIT = 3U,           //!< Subscriber inhibit request.
            UNINHIBIT = 4U,         //!< Subscriber uninhibit request.

            KMM_REKEY = 5U,         //!< Key management rekey request.
            KMM_CHANGEOVER = 6U,    //!< Key management changeover request.
            KMM_CHANGERSI = 7U,     //!< Key management change RSI request.

            EMERG_ALRM = 8U,        //!< Emergency alarm request.
            EMERG_CALL = 9U,        //!< Emergency voice call.

            RADIO_MONITOR = 10U,    //!< Radio monitor request.

            UNIT_REG = 11U,         //!< Unit registration request.
            UNIT_DEREG = 12U,       //!< Unit deregistration request.
            GRP_AFFIL = 13U,        //!< Group affiliation request.

            CALL_ALERT_ACK = 14U,   //!< Call alert acknowledgement.
        };

        /**
         * @brief Operation lifecycle states.
         * @ingroup fne_network
         */
        enum class State : uint8_t {
            PENDING = 0U,           //!< Request has been transmitted and awaits a response.
            SUCCEEDED = 1U,         //!< A matching successful response was received.
            REJECTED = 2U,          //!< A matching negative response was received.
            TIMED_OUT = 3U          //!< No matching response arrived before the deadline.
        };

        /**
         * @brief Immutable snapshot of one operation.
         * @ingroup fne_network
         */
        struct Operation {
            uint64_t id;            //!< FNE-local operation identifier.
            Protocol protocol;      //!< Air-interface protocol.
            Kind kind;              //!< Requested operation.
            State state;            //!< Current lifecycle state.
            uint32_t targetId;      //!< Target subscriber ID.
            uint32_t peerId;        //!< Requested or responding peer ID.
            uint32_t requestValue;  //!< Optional requested status/message value.
            uint32_t resultValue;   //!< Optional returned status value.
            uint64_t createdAt;     //!< Creation time in Unix milliseconds.
            uint64_t completedAt;   //!< Completion time in Unix milliseconds, or zero.
        };

        /**
         * @brief Latest status observed for a P25 subscriber.
         * @ingroup fne_network
         */
        struct Status {
            uint32_t subscriberId;  //!< Reporting subscriber ID.
            uint32_t peerId;        //!< Peer that delivered the report.
            uint16_t value;         //!< Status value.
            bool solicited;         //!< True when matched to an active query.
            uint64_t observedAt;    //!< Observation time in Unix milliseconds.
        };

        /**
         * @brief Initializes an empty operation store.
         */
        SubscriberOperationStore(uint32_t operationRetention, uint32_t statusRetention);

        /**
         * @brief Updates operation and status retention periods.
         * @param[in] operationRetention Operation retention in minutes.
         * @param[in] statusRetention Status retention in minutes.
         */
        void setRetention(uint32_t operationRetention, uint32_t statusRetention)
        {
            RESULT_RETENTION = std::chrono::minutes(operationRetention);
            STATUS_RETENTION = std::chrono::minutes(statusRetention);
        }

        /**
         * @brief Starts a uniquely correlatable operation.
         * @param[in] protocol Air-interface protocol.
         * @param[in] kind Operation kind.
         * @param[in] targetId Target subscriber ID.
         * @param[in] peerId Requested peer ID, or zero for routed fanout.
         * @param[in] requestValue Optional requested value.
         * @returns Operation identifier, or zero when an equivalent request is already pending.
         */
        uint64_t start(Protocol protocol, Kind kind, uint32_t targetId, uint32_t peerId, uint32_t requestValue = 0U);

        /**
         * @brief Completes the matching active operation.
         * @param[in] protocol Air-interface protocol.
         * @param[in] kind Operation kind.
         * @param[in] targetId Target subscriber ID.
         * @param[in] peerId Responding peer ID.
         * @param[in] resultValue Optional returned value.
         * @param[in] success True for successful completion.
         * @returns bool True when an active operation was completed.
         */
        bool complete(Protocol protocol, Kind kind, uint32_t targetId, uint32_t peerId,
            uint32_t resultValue = 0U, bool success = true);

        /**
         * @brief Adds a uniquely correlatable one-shot operation.
         * @param[in] protocol Air-interface protocol.
         * @param[in] kind Operation kind.
         * @param[in] targetId Target subscriber ID.
         * @param[in] peerId Requested peer ID, or zero for routed fanout.
         * @param[in] requestValue Optional requested value.
         * @param[in] resultValue Optional returned value.
         * @returns Unique operation identifier.
         */
        uint64_t oneShot(Protocol protocol, Kind kind, uint32_t targetId, uint32_t peerId, uint32_t requestValue = 0U,
            uint32_t resultValue = 0U);

        /**
         * @brief Adds a status report and completes a matching P25 status query.
         * @param[in] subscriberId Reporting subscriber ID.
         * @param[in] peerId Peer that delivered the report.
         * @param[in] value Status value.
         * @returns bool True when the report completed a pending query.
         */
        bool addP25Status(uint32_t subscriberId, uint32_t peerId, uint16_t value);

        /**
         * @brief Retrieves an operation snapshot.
         * @param[in] id Operation identifier.
         * @param[out] operation Operation snapshot.
         * @returns bool True if the operation exists, otherwise false.
         */
        bool get(uint64_t id, Operation& operation);
        /**
         * @brief Retrieves the most recently started operations.
         * @param[in] count Maximum number of operations to retrieve.
         * @returns std::vector<Operation> Operation snapshots ordered newest first.
         */
        std::vector<Operation> getLast(uint32_t count);
        /**
         * @brief Retrieves the latest P25 status snapshot.
         * @param[in] subscriberId Subscriber ID.
         * @param[out] status Status snapshot.
         * @returns bool True if an unexpired status exists, otherwise false.
         */
        bool getP25Status(uint32_t subscriberId, Status& status);
        /**
         * @brief Expires pending operations and removes retained results.
         */
        void expire();

        /**
         * @brief Returns a stable protocol name.
         * @param[in] protocol Protocol value.
         * @returns const char* Protocol name with static storage duration.
         */
        static const char* protocolName(Protocol protocol);
        /**
         * @brief Returns a stable operation name.
         * @param[in] kind Operation kind.
         * @returns const char* Operation name with static storage duration.
         */
        static const char* kindName(Kind kind);
        /**
         * @brief Returns a stable state name.
         * @param[in] state Operation state.
         * @returns const char* State name with static storage duration.
         */
        static const char* stateName(State state);

    private:
        std::chrono::minutes RESULT_RETENTION;
        std::chrono::minutes STATUS_RETENTION;

        /**
         * @brief Internal operation record with monotonic expiry deadlines.
         * @ingroup fne_network
         */
        struct StoredOperation {
            Operation value;                                   //!< Externally visible operation data.
            std::chrono::steady_clock::time_point deadline;    //!< Monotonic response deadline.
            std::chrono::steady_clock::time_point retainUntil; //!< Monotonic result retention deadline.
        };

        /**
         * @brief Internal subscriber status record with a monotonic expiry deadline.
         * @ingroup fne_network
         */
        struct StoredStatus {
            Status value;                                      //!< Externally visible status data.
            std::chrono::steady_clock::time_point retainUntil; //!< Monotonic status retention deadline.
        };

        uint64_t m_nextId;                                          //!< Next candidate operation identifier.
        std::unordered_map<uint64_t, StoredOperation> m_operations; //!< Operations indexed by identifier.
        std::unordered_map<uint32_t, StoredStatus> m_p25Statuses;   //!< Latest P25 status indexed by subscriber ID.
        std::mutex m_mutex;                                         //!< Protects all store state.

        /**
         * @brief Gets the current Unix timestamp.
         * @returns uint64_t Current Unix time in milliseconds.
         */
        static uint64_t unixMilliseconds();
        /**
         * @brief Expires records while the store mutex is held.
         * @param[in] now Current monotonic time.
         */
        void expireLocked(const std::chrono::steady_clock::time_point& now);
    };
} // namespace network

#endif // __SUBSCRIBER_OPERATION_STORE_H__
