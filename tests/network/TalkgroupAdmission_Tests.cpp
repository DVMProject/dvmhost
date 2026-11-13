// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES FROM THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/lookups/TalkgroupRulesLookup.h"

#include <catch2/catch_test_macros.hpp>

#include <tuple>
#include <vector>

using namespace lookups;

/**
 * @brief Builds one talkgroup rule for admission tests.
 * @param tgId Talkgroup ID.
 * @param slot DMR slot.
 * @param rids Permitted radio IDs.
 * @returns TalkgroupRuleGroupVoice Test rule.
 */
static TalkgroupRuleGroupVoice admission(uint32_t tgId, uint8_t slot, std::vector<uint32_t> rids)
{
    TalkgroupRuleGroupVoiceSource source;
    source.tgId(tgId);
    source.tgSlot(slot);

    TalkgroupRuleConfig config;
    config.active(true);
    config.permittedRIDs(rids);

    TalkgroupRuleGroupVoice entry;
    entry.source(source);
    entry.config(config);
    return entry;
}

TEST_CASE("Talkgroup RID admission is gated by talkgroup ACL enforcement", "[network][talkgroup-admission]")
{
    TalkgroupRulesLookup unrestricted("", 0U, false, false);
    unrestricted.addEntry(admission(101U, 1U, { 1001U }));
    REQUIRE(unrestricted.isRIDPermitted(101U, 1U, 9999U));

    TalkgroupRulesLookup restricted("", 0U, true, false);
    restricted.addEntry(admission(101U, 1U, { 1001U, 1002U }));
    REQUIRE(restricted.isRIDPermitted(101U, 1U, 1001U));
    REQUIRE_FALSE(restricted.isRIDPermitted(101U, 1U, 9999U));
    REQUIRE_FALSE(restricted.isRIDPermitted(999U, 1U, 1001U));
}

TEST_CASE("An empty permitted RID list admits every RID", "[network][talkgroup-admission]")
{
    TalkgroupRulesLookup lookup("", 0U, true, false);
    lookup.addEntry(admission(202U, 0U, {}));

    REQUIRE(lookup.isRIDPermitted(202U, 0U, 1U));
    REQUIRE(lookup.isRIDPermitted(202U, 0U, 0xFFFFFFU));
}

TEST_CASE("Replacing RID admissions clears restrictions omitted by the snapshot", "[network][talkgroup-admission]")
{
    TalkgroupRulesLookup lookup("", 0U, true, false);
    lookup.addEntry(admission(301U, 1U, { 3001U }));
    lookup.addEntry(admission(302U, 2U, { 3002U }));

    lookup.replaceRIDAdmissions({ admission(302U, 2U, { 4002U }) });

    REQUIRE(lookup.isRIDPermitted(301U, 1U, 9999U));
    REQUIRE(lookup.isRIDPermitted(302U, 2U, 4002U));
    REQUIRE_FALSE(lookup.isRIDPermitted(302U, 2U, 3002U));

    lookup.replaceRIDAdmissions({});
    REQUIRE(lookup.isRIDPermitted(302U, 2U, 3002U));
}

TEST_CASE("Talkgroup RID admission round-robin covers every TG RID and slot combination", "[network][talkgroup-admission][round-robin]")
{
    TalkgroupRulesLookup lookup("", 0U, true, false);
    lookup.addEntry(admission(401U, 1U, { 1001U, 1002U }));
    lookup.addEntry(admission(401U, 2U, { 2001U }));
    lookup.addEntry(admission(402U, 0U, {}));
    lookup.addEntry(admission(403U, 1U, { 0U, 0xFFFFFFU }));

    using AdmissionCheck = std::tuple<uint32_t, uint8_t, uint32_t, bool>;
    const std::vector<AdmissionCheck> checks = {
        // Every RID is checked against both restricted slots for the same TGID.
        { 401U, 1U, 1001U, true },
        { 401U, 1U, 1002U, true },
        { 401U, 1U, 2001U, false },
        { 401U, 1U, 9999U, false },
        { 401U, 2U, 1001U, false },
        { 401U, 2U, 1002U, false },
        { 401U, 2U, 2001U, true },
        { 401U, 2U, 9999U, false },

        // Slot zero selects the first matching rule for protocols without slots.
        { 401U, 0U, 1001U, true },
        { 401U, 0U, 2001U, false },

        // A slot mismatch and an unknown TG are rejected while ACL is enforced.
        { 401U, 3U, 1001U, false },
        { 499U, 0U, 1001U, false },
        { 499U, 1U, 1001U, false },

        // Empty lists admit all RIDs, including boundary values.
        { 402U, 0U, 0U, true },
        { 402U, 0U, 1U, true },
        { 402U, 0U, 0xFFFFFFU, true },

        // Restricted lists preserve boundary RID membership behavior.
        { 403U, 1U, 0U, true },
        { 403U, 1U, 1U, false },
        { 403U, 1U, 0xFFFFFFU, true }
    };

    // Rotate the starting point on each pass so every entry is evaluated before
    // and after every other admission decision, detecting leaked lookup state.
    for (size_t start = 0U; start < checks.size(); ++start) {
        for (size_t offset = 0U; offset < checks.size(); ++offset) {
            const AdmissionCheck& check = checks[(start + offset) % checks.size()];
            const uint32_t tgId = std::get<0U>(check);
            const uint8_t slot = std::get<1U>(check);
            const uint32_t rid = std::get<2U>(check);
            const bool expected = std::get<3U>(check);

            CAPTURE(start, offset, tgId, slot, rid);
            REQUIRE(lookup.isRIDPermitted(tgId, slot, rid) == expected);
        }
    }
}

TEST_CASE("Talkgroup RID admission round-robin follows successive snapshots", "[network][talkgroup-admission][round-robin]")
{
    TalkgroupRulesLookup lookup("", 0U, true, false);
    lookup.addEntry(admission(501U, 1U, {}));
    lookup.addEntry(admission(502U, 2U, {}));
    lookup.addEntry(admission(503U, 0U, {}));

    const std::vector<uint32_t> tgIds = { 501U, 502U, 503U };
    const std::vector<uint8_t> slots = { 1U, 2U, 0U };
    const std::vector<uint32_t> rids = { 5001U, 5002U, 5003U };

    for (size_t round = 0U; round < tgIds.size(); ++round) {
        lookup.replaceRIDAdmissions({ admission(tgIds[round], slots[round], { rids[round] }) });

        for (size_t tg = 0U; tg < tgIds.size(); ++tg) {
            for (size_t rid = 0U; rid < rids.size(); ++rid) {
                const bool expected = tg != round || rid == round;
                CAPTURE(round, tg, rid, tgIds[tg], slots[tg], rids[rid]);
                REQUIRE(lookup.isRIDPermitted(tgIds[tg], slots[tg], rids[rid]) == expected);
            }
        }
    }

    // An unknown snapshot entry is ignored, and an empty snapshot returns all
    // configured TGs to unrestricted admission.
    lookup.replaceRIDAdmissions({ admission(599U, 0U, { 5999U }) });
    for (size_t tg = 0U; tg < tgIds.size(); ++tg) {
        for (uint32_t rid : rids) {
            CAPTURE(tg, tgIds[tg], slots[tg], rid);
            REQUIRE(lookup.isRIDPermitted(tgIds[tg], slots[tg], rid));
        }
    }

    lookup.replaceRIDAdmissions({});
    for (size_t tg = 0U; tg < tgIds.size(); ++tg) {
        for (uint32_t rid : rids) {
            CAPTURE(tg, tgIds[tg], slots[tg], rid);
            REQUIRE(lookup.isRIDPermitted(tgIds[tg], slots[tg], rid));
        }
    }
}

TEST_CASE("Disabled TG ACL admits the full round-robin matrix", "[network][talkgroup-admission][round-robin]")
{
    TalkgroupRulesLookup lookup("", 0U, false, false);
    lookup.addEntry(admission(601U, 1U, { 6001U }));

    const std::vector<uint32_t> tgIds = { 0U, 601U, 699U, 0xFFFFU };
    const std::vector<uint8_t> slots = { 0U, 1U, 2U, 0xFFU };
    const std::vector<uint32_t> rids = { 0U, 6001U, 6999U, 0xFFFFFFU };

    for (uint32_t tgId : tgIds) {
        for (uint8_t slot : slots) {
            for (uint32_t rid : rids) {
                CAPTURE(tgId, slot, rid);
                REQUIRE(lookup.isRIDPermitted(tgId, slot, rid));
            }
        }
    }
}
