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
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief In-memory registry for console-advertised patch status.
 * @ingroup fne
 */
class HOST_SW_API PatchStatusRegistry {
public:
    static constexpr uint32_t DEFAULT_TTL_SECONDS = 15U;
    static constexpr uint32_t MIN_TTL_SECONDS = 5U;
    static constexpr uint32_t MAX_TTL_SECONDS = 300U;
    static constexpr uint32_t MAX_WAIT_MS = 30000U;

    PatchStatusRegistry();
    ~PatchStatusRegistry() = default;

    void configure(uint32_t defaultTtlSeconds, uint32_t minTtlSeconds, uint32_t maxTtlSeconds);

    bool publish(json::object& request, json::object& response, std::string& errorMessage);
    bool removePeer(uint32_t peerId);
    uint32_t cleanupExpired();

    json::object snapshot();
    json::object waitForChanges(uint64_t sinceRevision, uint32_t waitMs);

    uint64_t revision() const;
    uint32_t defaultTtlSeconds() const;
    uint32_t minTtlSeconds() const;
    uint32_t maxTtlSeconds() const;

private:
    struct PatchMember {
        std::string system;
        std::string mode;
        uint32_t tgid = 0U;
        uint8_t slot = 0U;
    };

    struct PatchRecord {
        std::string patchId;
        bool active = true;
        bool oneWay = false;
        std::vector<PatchMember> members;
    };

    struct PeerPatchSnapshot {
        uint32_t peerId = 0U;
        std::string peerName;
        uint32_t sequence = 0U;
        uint64_t updatedAt = 0U;
        uint64_t expiresAt = 0U;
        std::vector<PatchRecord> patches;
    };

    static uint64_t nowMs();
    static std::string normalizeMode(const std::string& mode);
    static std::string buildTalkgroupKey(const PatchMember& member);
    static json::object memberToJson(const PatchMember& member);
    static json::object patchToJson(const PatchRecord& patch);
    static json::object peerSnapshotToJson(const PeerPatchSnapshot& peer);

    bool parsePatch(json::object& obj, PatchRecord& patch, std::string& errorMessage) const;
    bool parseMember(json::object& obj, PatchMember& member, std::string& errorMessage) const;
    uint32_t clampTtl(uint32_t ttlSeconds) const;
    json::object snapshotLocked() const;
    void bumpRevisionLocked();

    mutable std::mutex m_mutex;
    std::condition_variable m_revisionChanged;
    std::unordered_map<uint32_t, PeerPatchSnapshot> m_peerPatches;
    uint64_t m_revision;
    uint32_t m_defaultTtlSeconds;
    uint32_t m_minTtlSeconds;
    uint32_t m_maxTtlSeconds;
};

#endif // __FNE_PATCH_STATUS_REGISTRY_H__
