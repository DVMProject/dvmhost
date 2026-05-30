// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 DVMProject Authors
 *
 */
/**
 * @file PatchStatusRegistry.h
 * @ingroup fne
 */
#if !defined(__FNE_PATCH_STATUS_REGISTRY_H__)
#define __FNE_PATCH_STATUS_REGISTRY_H__

#include "fne/Defines.h"
#include "common/json/json.h"

#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

/**
 * @brief In-memory registry for console-advertised patch status.
 *
 * The registry owns all patch status records published by console peers and
 * replicated from neighboring FNEs. All access to the backing storage is
 * serialized through the registry mutex; callers receive JSON snapshots and
 * never receive direct references or iterators into the registry storage.
 *
 * @ingroup fne
 */
class HOST_SW_API PatchStatusRegistry {
public:
    /**
     * @brief Default number of seconds before a patch status record expires.
     */
    static constexpr uint32_t DEFAULT_TTL_SECONDS = 15U;
    /**
     * @brief Minimum accepted patch status TTL in seconds.
     */
    static constexpr uint32_t MIN_TTL_SECONDS = 5U;
    /**
     * @brief Maximum accepted patch status TTL in seconds.
     */
    static constexpr uint32_t MAX_TTL_SECONDS = 300U;
    /**
     * @brief Maximum wait time for long-poll style change requests.
     */
    static constexpr uint32_t MAX_WAIT_MS = 30000U;

    /**
     * @brief Initializes a new instance of the PatchStatusRegistry class.
     */
    PatchStatusRegistry();
    /**
     * @brief Finalizes an instance of the PatchStatusRegistry class.
     */
    ~PatchStatusRegistry() = default;

    /**
     * @brief Configures the accepted TTL range for patch status records.
     * @param defaultTtlSeconds Default TTL used when a publish request omits a TTL.
     * @param minTtlSeconds Minimum accepted TTL.
     * @param maxTtlSeconds Maximum accepted TTL.
     */
    void configure(uint32_t defaultTtlSeconds, uint32_t minTtlSeconds, uint32_t maxTtlSeconds);

    /**
     * @brief Publishes a complete patch snapshot for one console peer.
     * @param request JSON request containing peerId, peerName, optional sequence, optional ttlSeconds, and patches.
     * @param response JSON registry snapshot returned after the publish is applied.
     * @param errorMessage Validation error text populated when the request is invalid.
     * @returns bool True, if the publish request was valid and applied, otherwise false.
     */
    bool publish(json::object& request, json::object& response, std::string& errorMessage);
    /**
     * @brief Removes all patch records associated with a console peer.
     * @param peerId Console peer ID whose records should be removed.
     * @returns bool True, if records were removed, otherwise false.
     */
    bool removePeer(uint32_t peerId);
    /**
     * @brief Removes expired patch status records.
     * @returns uint32_t Number of peer records removed.
     */
    uint32_t cleanupExpired();

    /**
     * @brief Creates a complete JSON snapshot of the registry.
     * @returns json::object Registry snapshot.
     */
    json::object snapshot();
    /**
     * @brief Waits for registry changes after the supplied revision.
     * @param sinceRevision Revision already known to the caller.
     * @param waitMs Maximum wait time in milliseconds.
     * @returns json::object Registry snapshot after change or timeout.
     */
    json::object waitForChanges(uint64_t sinceRevision, uint32_t waitMs);

    /**
     * @brief Gets the current registry revision.
     * @returns uint64_t Registry revision number.
     */
    uint64_t revision() const;
    /**
     * @brief Gets the configured default patch status TTL.
     * @returns uint32_t Default TTL in seconds.
     */
    uint32_t defaultTtlSeconds() const;
    /**
     * @brief Gets the configured minimum patch status TTL.
     * @returns uint32_t Minimum TTL in seconds.
     */
    uint32_t minTtlSeconds() const;
    /**
     * @brief Gets the configured maximum patch status TTL.
     * @returns uint32_t Maximum TTL in seconds.
     */
    uint32_t maxTtlSeconds() const;

private:
    /**
     * @brief Represents a talkgroup member participating in a patch.
     */
    struct PatchMember {
        std::string system;                //!< System name for the member.
        std::string mode;                  //!< Digital mode for the member.
        uint32_t tgid = 0U;                //!< Talkgroup ID for the member.
        uint8_t slot = 0U;                 //!< Timeslot for the member, if applicable.
    };

    /**
     * @brief Represents one active console patch.
     */
    struct PatchRecord {
        std::string patchId;               //!< Console-defined patch ID.
        bool active = true;                //!< Flag indicating whether the patch is active.
        bool oneWay = false;               //!< Flag indicating whether the patch is one-way.
        std::vector<PatchMember> members;  //!< Talkgroup members in the patch.
    };

    /**
     * @brief Represents a console peer's complete patch status snapshot.
     */
    struct PeerPatchSnapshot {
        uint32_t peerId = 0U;              //!< Console peer ID.
        uint32_t originFnePeerId = 0U;     //!< FNE peer ID where this status originated.
        std::string peerName;              //!< Console peer display name.
        uint32_t sequence = 0U;            //!< Console-defined sequence number.
        uint64_t updatedAt = 0U;           //!< Time this snapshot was accepted.
        uint64_t expiresAt = 0U;           //!< Time this snapshot expires.
        std::vector<PatchRecord> patches;  //!< Complete patch list for this console peer.
    };

    /**
     * @brief Gets the current system time in milliseconds.
     * @returns uint64_t Current time in milliseconds.
     */
    static uint64_t nowMs();
    /**
     * @brief Normalizes a mode string for key generation.
     * @param mode Mode string.
     * @returns std::string Normalized mode string.
     */
    static std::string normalizeMode(const std::string& mode);
    /**
     * @brief Builds a stable talkgroup lookup key for a patch member.
     * @param member Patch member.
     * @returns std::string Talkgroup key.
     */
    static std::string buildTalkgroupKey(const PatchMember& member);
    /**
     * @brief Serializes a patch member to JSON.
     * @param member Patch member.
     * @returns json::object JSON patch member.
     */
    static json::object memberToJson(const PatchMember& member);
    /**
     * @brief Serializes a patch record to JSON.
     * @param patch Patch record.
     * @returns json::object JSON patch record.
     */
    static json::object patchToJson(const PatchRecord& patch);
    /**
     * @brief Serializes a peer patch snapshot to JSON.
     * @param peer Peer patch snapshot.
     * @returns json::object JSON peer patch snapshot.
     */
    static json::object peerSnapshotToJson(const PeerPatchSnapshot& peer);

    /**
     * @brief Parses one patch record from JSON.
     * @param obj JSON patch object.
     * @param patch Parsed patch record.
     * @param errorMessage Validation error text.
     * @returns bool True, if parsed successfully, otherwise false.
     */
    bool parsePatch(json::object& obj, PatchRecord& patch, std::string& errorMessage) const;
    /**
     * @brief Parses one patch member from JSON.
     * @param obj JSON patch member object.
     * @param member Parsed patch member.
     * @param errorMessage Validation error text.
     * @returns bool True, if parsed successfully, otherwise false.
     */
    bool parseMember(json::object& obj, PatchMember& member, std::string& errorMessage) const;
    /**
     * @brief Clamps a TTL value into the configured TTL range.
     * @param ttlSeconds TTL in seconds.
     * @returns uint32_t Clamped TTL in seconds.
     */
    uint32_t clampTtl(uint32_t ttlSeconds) const;
    /**
     * @brief Creates a registry snapshot while the registry mutex is held.
     * @returns json::object Registry snapshot.
     */
    json::object snapshotLocked() const;
    /**
     * @brief Advances the registry revision while the registry mutex is held.
     */
    void bumpRevisionLocked();

    mutable std::mutex m_mutex;                         //!< Mutex guarding registry state.
    std::condition_variable m_revisionChanged;          //!< Condition variable signaled when revision changes.
    std::map<uint32_t, PeerPatchSnapshot> m_peerPatches; //!< Peer patch snapshots keyed by console peer ID.
    uint64_t m_revision;                                //!< Monotonic registry revision.
    uint32_t m_defaultTtlSeconds;                       //!< Default TTL in seconds.
    uint32_t m_minTtlSeconds;                           //!< Minimum TTL in seconds.
    uint32_t m_maxTtlSeconds;                           //!< Maximum TTL in seconds.
};

#endif // __FNE_PATCH_STATUS_REGISTRY_H__
