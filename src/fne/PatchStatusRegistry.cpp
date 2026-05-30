// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 DVMProject Authors
 *
 */
#include "fne/PatchStatusRegistry.h"

#include "common/Log.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <limits>
#include <sstream>

constexpr uint32_t PatchStatusRegistry::DEFAULT_TTL_SECONDS;
constexpr uint32_t PatchStatusRegistry::MIN_TTL_SECONDS;
constexpr uint32_t PatchStatusRegistry::MAX_TTL_SECONDS;
constexpr uint32_t PatchStatusRegistry::MAX_WAIT_MS;

/* Initializes a new instance of the PatchStatusRegistry class. */

PatchStatusRegistry::PatchStatusRegistry() :
    m_mutex(),
    m_revisionChanged(),
    m_peerPatches(),
    m_revision(0U),
    m_defaultTtlSeconds(DEFAULT_TTL_SECONDS),
    m_minTtlSeconds(MIN_TTL_SECONDS),
    m_maxTtlSeconds(MAX_TTL_SECONDS)
{
    /* stub */
}

/* Configures the accepted TTL range for patch status records. */

void PatchStatusRegistry::configure(uint32_t defaultTtlSeconds, uint32_t minTtlSeconds, uint32_t maxTtlSeconds)
{
    if (minTtlSeconds == 0U)
        minTtlSeconds = MIN_TTL_SECONDS;
    if (maxTtlSeconds < minTtlSeconds)
        maxTtlSeconds = minTtlSeconds;

    std::lock_guard<std::mutex> guard(m_mutex);
    m_minTtlSeconds = minTtlSeconds;
    m_maxTtlSeconds = maxTtlSeconds;
    m_defaultTtlSeconds = std::max(m_minTtlSeconds, std::min(defaultTtlSeconds, m_maxTtlSeconds));
}

/* Publishes a complete patch snapshot for one console peer. */

bool PatchStatusRegistry::publish(json::object& request, json::object& response, std::string& errorMessage)
{
    if (!request["peerId"].is<uint32_t>()) {
        errorMessage = "peerId was not a valid integer";
        return false;
    }

    if (!request["patches"].is<json::array>()) {
        errorMessage = "patches was not a valid array";
        return false;
    }

    PeerPatchSnapshot incoming;
    incoming.peerId = request["peerId"].get<uint32_t>();
    if (incoming.peerId == 0U) {
        errorMessage = "peerId cannot be zero";
        return false;
    }

    if (request["peerName"].is<std::string>())
        incoming.peerName = request["peerName"].get<std::string>();

    if (request["originFnePeerId"].is<uint32_t>())
        incoming.originFnePeerId = request["originFnePeerId"].get<uint32_t>();

    if (request["sequence"].is<uint32_t>())
        incoming.sequence = request["sequence"].get<uint32_t>();

    uint32_t ttlSeconds = defaultTtlSeconds();
    if (request["ttlSeconds"].is<uint32_t>())
        ttlSeconds = request["ttlSeconds"].get<uint32_t>();
    ttlSeconds = clampTtl(ttlSeconds);

    incoming.updatedAt = nowMs();
    incoming.expiresAt = incoming.updatedAt + (static_cast<uint64_t>(ttlSeconds) * 1000U);

    json::array patches = request["patches"].get<json::array>();
    for (json::value& value : patches) {
        if (!value.is<json::object>()) {
            errorMessage = "patches contained a non-object entry";
            return false;
        }

        json::object patchObj = value.get<json::object>();
        PatchRecord patch;
        if (!parsePatch(patchObj, patch, errorMessage))
            return false;

        incoming.patches.push_back(patch);
    }

    cleanupExpired();

    {
        std::lock_guard<std::mutex> guard(m_mutex);
        auto existing = m_peerPatches.find(incoming.peerId);
        if (existing != m_peerPatches.end() && incoming.sequence > 0U && existing->second.sequence > incoming.sequence) {
            response = snapshotLocked();
            response["acceptedPeerId"].set<uint32_t>(incoming.peerId);
            response["ttlSeconds"].set<uint32_t>(ttlSeconds);
            return true;
        }

        if (incoming.patches.empty())
            m_peerPatches.erase(incoming.peerId);
        else
            m_peerPatches[incoming.peerId] = incoming;
        bumpRevisionLocked();

        response = snapshotLocked();
        response["acceptedPeerId"].set<uint32_t>(incoming.peerId);
        response["ttlSeconds"].set<uint32_t>(ttlSeconds);
    }

    m_revisionChanged.notify_all();
    return true;
}

/* Removes all patch records associated with a console peer. */

bool PatchStatusRegistry::removePeer(uint32_t peerId)
{
    if (peerId == 0U)
        return false;

    bool removed = false;
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        removed = m_peerPatches.erase(peerId) > 0U;
        if (removed)
            bumpRevisionLocked();
    }

    if (removed)
        m_revisionChanged.notify_all();

    return removed;
}

/* Removes expired patch status records. */

uint32_t PatchStatusRegistry::cleanupExpired()
{
    uint32_t removed = 0U;
    bool changed = false;
    uint64_t now = nowMs();

    {
        std::lock_guard<std::mutex> guard(m_mutex);
        for (auto it = m_peerPatches.begin(); it != m_peerPatches.end();) {
            if (it->second.expiresAt <= now) {
                it = m_peerPatches.erase(it);
                removed++;
                changed = true;
            }
            else {
                ++it;
            }
        }

        if (changed)
            bumpRevisionLocked();
    }

    if (changed)
        m_revisionChanged.notify_all();

    return removed;
}

/* Creates a complete JSON snapshot of the registry. */

json::object PatchStatusRegistry::snapshot()
{
    cleanupExpired();

    std::lock_guard<std::mutex> guard(m_mutex);
    return snapshotLocked();
}

/* Waits for registry changes after the supplied revision. */

json::object PatchStatusRegistry::waitForChanges(uint64_t sinceRevision, uint32_t waitMs)
{
    waitMs = std::min(waitMs, MAX_WAIT_MS);
    cleanupExpired();

    std::unique_lock<std::mutex> lock(m_mutex);
    if (waitMs > 0U && sinceRevision >= m_revision) {
        m_revisionChanged.wait_for(lock, std::chrono::milliseconds(waitMs), [&]() {
            return m_revision > sinceRevision;
        });
    }

    return snapshotLocked();
}

/* Gets the current registry revision. */

uint64_t PatchStatusRegistry::revision() const
{
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_revision;
}

/* Gets the configured default patch status TTL. */

uint32_t PatchStatusRegistry::defaultTtlSeconds() const
{
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_defaultTtlSeconds;
}

/* Gets the configured minimum patch status TTL. */

uint32_t PatchStatusRegistry::minTtlSeconds() const
{
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_minTtlSeconds;
}

/* Gets the configured maximum patch status TTL. */

uint32_t PatchStatusRegistry::maxTtlSeconds() const
{
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_maxTtlSeconds;
}

/* Gets the current system time in milliseconds. */

uint64_t PatchStatusRegistry::nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

/* Normalizes a mode string for key generation. */

std::string PatchStatusRegistry::normalizeMode(const std::string& mode)
{
    std::string normalized = mode;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return normalized;
}

/* Builds a stable talkgroup lookup key for a patch member. */

std::string PatchStatusRegistry::buildTalkgroupKey(const PatchMember& member)
{
    std::ostringstream ss;
    ss << normalizeMode(member.mode) << ':' << member.tgid << ':' << static_cast<uint32_t>(member.slot);
    return ss.str();
}

/* Serializes a patch member to JSON. */

json::object PatchStatusRegistry::memberToJson(const PatchMember& member)
{
    json::object obj = json::object();
    obj["system"].set<std::string>(member.system);
    obj["mode"].set<std::string>(member.mode);
    obj["tgid"].set<uint32_t>(member.tgid);
    obj["slot"].set<uint8_t>(member.slot);
    obj["key"].set<std::string>(buildTalkgroupKey(member));
    return obj;
}

/* Serializes a patch record to JSON. */

json::object PatchStatusRegistry::patchToJson(const PatchRecord& patch)
{
    json::object obj = json::object();
    obj["patchId"].set<std::string>(patch.patchId);
    obj["active"].set<bool>(patch.active);
    obj["oneWay"].set<bool>(patch.oneWay);

    json::array members = json::array();
    for (const PatchMember& member : patch.members)
        members.push_back(json::value(memberToJson(member)));
    obj["members"].set<json::array>(members);

    return obj;
}

/* Serializes a peer patch snapshot to JSON. */

json::object PatchStatusRegistry::peerSnapshotToJson(const PeerPatchSnapshot& peer)
{
    json::object obj = json::object();
    obj["peerId"].set<uint32_t>(peer.peerId);
    obj["originFnePeerId"].set<uint32_t>(peer.originFnePeerId);
    obj["peerName"].set<std::string>(peer.peerName);
    obj["sequence"].set<uint32_t>(peer.sequence);
    obj["updatedAt"].set<uint64_t>(peer.updatedAt);
    obj["expiresAt"].set<uint64_t>(peer.expiresAt);

    json::array patches = json::array();
    for (const PatchRecord& patch : peer.patches)
        patches.push_back(json::value(patchToJson(patch)));
    obj["patches"].set<json::array>(patches);

    return obj;
}

/* Parses one patch record from JSON. */

bool PatchStatusRegistry::parsePatch(json::object& obj, PatchRecord& patch, std::string& errorMessage) const
{
    if (obj["patchId"].is<std::string>())
        patch.patchId = obj["patchId"].get<std::string>();

    if (obj["active"].is<bool>())
        patch.active = obj["active"].get<bool>();

    if (obj["oneWay"].is<bool>())
        patch.oneWay = obj["oneWay"].get<bool>();

    if (!obj["members"].is<json::array>()) {
        errorMessage = "patch members was not a valid array";
        return false;
    }

    json::array members = obj["members"].get<json::array>();
    for (json::value& value : members) {
        if (!value.is<json::object>()) {
            errorMessage = "patch members contained a non-object entry";
            return false;
        }

        json::object memberObj = value.get<json::object>();
        PatchMember member;
        if (!parseMember(memberObj, member, errorMessage))
            return false;

        patch.members.push_back(member);
    }

    return true;
}

/* Parses one patch member from JSON. */

bool PatchStatusRegistry::parseMember(json::object& obj, PatchMember& member, std::string& errorMessage) const
{
    if (obj["system"].is<std::string>())
        member.system = obj["system"].get<std::string>();

    if (obj["mode"].is<std::string>())
        member.mode = normalizeMode(obj["mode"].get<std::string>());
    else
        member.mode = "unknown";

    if (!obj["tgid"].is<uint32_t>()) {
        errorMessage = "patch member tgid was not a valid integer";
        return false;
    }

    member.tgid = obj["tgid"].get<uint32_t>();
    if (member.tgid == 0U) {
        errorMessage = "patch member tgid cannot be zero";
        return false;
    }

    if (obj["slot"].is<uint8_t>())
        member.slot = obj["slot"].get<uint8_t>();
    else if (obj["slot"].is<uint32_t>()) {
        uint32_t slot = obj["slot"].get<uint32_t>();
        if (slot > std::numeric_limits<uint8_t>::max()) {
            errorMessage = "patch member slot was out of range";
            return false;
        }
        member.slot = static_cast<uint8_t>(slot);
    }

    return true;
}

/* Clamps a TTL value into the configured TTL range. */

uint32_t PatchStatusRegistry::clampTtl(uint32_t ttlSeconds) const
{
    std::lock_guard<std::mutex> guard(m_mutex);
    return std::max(m_minTtlSeconds, std::min(ttlSeconds, m_maxTtlSeconds));
}

/* Creates a registry snapshot while the registry mutex is held. */

json::object PatchStatusRegistry::snapshotLocked() const
{
    json::object response = json::object();
    response["revision"].set<uint64_t>(m_revision);

    json::array peers = json::array();
    json::array patches = json::array();
    json::object byTalkgroup = json::object();

    for (const auto& entry : m_peerPatches) {
        const PeerPatchSnapshot& peer = entry.second;
        peers.push_back(json::value(peerSnapshotToJson(peer)));

        for (const PatchRecord& patch : peer.patches) {
            json::object patchObj = patchToJson(patch);
            patchObj["peerId"].set<uint32_t>(peer.peerId);
            patchObj["originFnePeerId"].set<uint32_t>(peer.originFnePeerId);
            patchObj["peerName"].set<std::string>(peer.peerName);
            patchObj["updatedAt"].set<uint64_t>(peer.updatedAt);
            patchObj["expiresAt"].set<uint64_t>(peer.expiresAt);
            patches.push_back(json::value(patchObj));

            for (const PatchMember& member : patch.members) {
                std::string key = buildTalkgroupKey(member);
                json::array entries = json::array();
                if (byTalkgroup[key].is<json::array>())
                    entries = byTalkgroup[key].get<json::array>();

                json::object tgPatch = json::object();
                tgPatch["peerId"].set<uint32_t>(peer.peerId);
                tgPatch["originFnePeerId"].set<uint32_t>(peer.originFnePeerId);
                tgPatch["peerName"].set<std::string>(peer.peerName);
                tgPatch["patchId"].set<std::string>(patch.patchId);
                tgPatch["active"].set<bool>(patch.active);
                tgPatch["oneWay"].set<bool>(patch.oneWay);
                tgPatch["updatedAt"].set<uint64_t>(peer.updatedAt);
                tgPatch["expiresAt"].set<uint64_t>(peer.expiresAt);
                tgPatch["member"].set<json::object>(memberToJson(member));
                entries.push_back(json::value(tgPatch));
                byTalkgroup[key].set<json::array>(entries);
            }
        }
    }

    response["peers"].set<json::array>(peers);
    response["patches"].set<json::array>(patches);
    response["byTalkgroup"].set<json::object>(byTalkgroup);
    return response;
}

/* Advances the registry revision while the registry mutex is held. */

void PatchStatusRegistry::bumpRevisionLocked()
{
    m_revision++;
    if (m_revision == 0U)
        m_revision = 1U;
}
