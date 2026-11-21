// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016,2018 Jonathan Naylor, G4KLX
 *  Copyright (C) 2023-2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "edac/Trellis.h"
#include "Log.h"
#include "Utils.h"

using namespace edac;

#include <cassert>
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t INTERLEAVE_TABLE[] = {
    0U, 1U, 8U,   9U, 16U, 17U, 24U, 25U, 32U, 33U, 40U, 41U, 48U, 49U, 56U, 57U, 64U, 65U, 72U, 73U, 80U, 81U, 88U, 89U, 96U, 97U,
    2U, 3U, 10U, 11U, 18U, 19U, 26U, 27U, 34U, 35U, 42U, 43U, 50U, 51U, 58U, 59U, 66U, 67U, 74U, 75U, 82U, 83U, 90U, 91U,
    4U, 5U, 12U, 13U, 20U, 21U, 28U, 29U, 36U, 37U, 44U, 45U, 52U, 53U, 60U, 61U, 68U, 69U, 76U, 77U, 84U, 85U, 92U, 93U,
    6U, 7U, 14U, 15U, 22U, 23U, 30U, 31U, 38U, 39U, 46U, 47U, 54U, 55U, 62U, 63U, 70U, 71U, 78U, 79U, 86U, 87U, 94U, 95U };

const uint8_t ENCODE_TABLE_34[] = {
    0U,  8U, 4U, 12U, 2U, 10U, 6U, 14U,
    4U, 12U, 2U, 10U, 6U, 14U, 0U,  8U,
    1U,  9U, 5U, 13U, 3U, 11U, 7U, 15U,
    5U, 13U, 3U, 11U, 7U, 15U, 1U,  9U,
    3U, 11U, 7U, 15U, 1U,  9U, 5U, 13U,
    7U, 15U, 1U,  9U, 5U, 13U, 3U, 11U,
    2U, 10U, 6U, 14U, 0U,  8U, 4U, 12U,
    6U, 14U, 0U,  8U, 4U, 12U, 2U, 10U };

const uint8_t ENCODE_TABLE_12[] = {
    0U,  15U, 12U,  3U,
    4U,  11U,  8U,  7U,
    13U,  2U,  1U, 14U,
    9U,   6U,  5U, 10U };

const int8_t POINT_DIBITS[16U][2U] = {
    { +1, -1 }, { -1, -1 }, { +3, -3 }, { -3, -3 }, { -3, -1 }, { +3, -1 }, { -1, -3 }, { +1, -3 },
    { -3, +3 }, { +3, +3 }, { -1, +1 }, { +1, +1 }, { +1, +3 }, { -1, +3 }, { +3, +1 }, { -3, +1 } };

const uint32_t TRELLIS_STEPS = 49U;
const uint32_t TRELLIS_34_STATES = 8U;
const uint32_t TRELLIS_12_STATES = 4U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the Trellis class. */

Trellis::Trellis() = default;

/* Finalizes a instance of the Trellis class. */

Trellis::~Trellis() = default;

/* Decodes 3/4 rate Trellis. */

bool Trellis::decode34(const uint8_t* data, uint8_t* payload, bool skipSymbols)
{
    assert(data != nullptr);
    assert(payload != nullptr);

    int8_t dibits[98U];
    deinterleave(data, dibits, skipSymbols);

    uint32_t metric = 0U;
    bool truncated = false;
    return viterbi34(dibits, payload, &metric, 1U, truncated) == 1U;
}

/* Decodes 3/4 rate Trellis and returns a bounded list of best candidates. */

uint32_t Trellis::decode34List(const uint8_t* data, uint8_t* payloads, uint32_t maxPaths, bool& truncated,
    bool skipSymbols)
{
    assert(data != nullptr);
    assert(payloads != nullptr);

    truncated = false;
    if (maxPaths == 0U || maxPaths > TRELLIS_34_MAX_LIST_PATHS)
        return 0U;

    int8_t dibits[98U];
    deinterleave(data, dibits, skipSymbols);

    uint32_t metrics[TRELLIS_34_MAX_LIST_PATHS];
    uint32_t count = viterbi34(dibits, payloads, metrics, maxPaths, truncated);

    uint32_t tied = 0U;
    while (tied < count && metrics[tied] == metrics[0U])
        tied++;

    return tied;
}

/* Encodes 3/4 rate Trellis. */

void Trellis::encode34(const uint8_t* payload, uint8_t* data, bool skipSymbols)
{
    assert(payload != nullptr);
    assert(data != nullptr);

    uint8_t tribits[49U];
    bitsToTribits(payload, tribits);

    uint8_t points[49U];
    uint8_t state = 0U;

    for (uint32_t i = 0U; i < 49U; i++) {
        uint8_t tribit = tribits[i];

        points[i] = ENCODE_TABLE_34[state * 8U + tribit];

        state = tribit;
    }

    int8_t dibits[98U];
    pointsToDibits(points, dibits);

    interleave(dibits, data, skipSymbols);
}

/* Decodes 1/2 rate Trellis. */

bool Trellis::decode12(const uint8_t* data, uint8_t* payload)
{
    assert(data != nullptr);
    assert(payload != nullptr);

    int8_t dibits[98U];
    deinterleave(data, dibits);

    uint32_t metric = 0U;
    return viterbi12(dibits, payload, metric);
}

/* Encodes 1/2 rate Trellis. */

void Trellis::encode12(const uint8_t* payload, uint8_t* data)
{
    assert(payload != nullptr);
    assert(data != nullptr);

    uint8_t bits[49U];
    bitsToDibits(payload, bits);

    uint8_t points[49U];
    uint8_t state = 0U;

    for (uint32_t i = 0U; i < 49U; i++) {
        uint8_t bit = bits[i];

        points[i] = ENCODE_TABLE_12[state * 4U + bit];

        state = bit;
    }

    int8_t dibits[98U];
    pointsToDibits(points, dibits);

    interleave(dibits, data);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Helper to deinterleave the input symbols into dibits. */

void Trellis::deinterleave(const uint8_t* data, int8_t* dibits, bool skipSymbols) const
{
    for (uint32_t i = 0U; i < 98U; i++) {
        uint32_t n = i * 2U + 0U;
        if (skipSymbols && n >= 98U) n += 68U;
        bool b1 = READ_BIT(data, n) != 0x00U;

        n = i * 2U + 1U;
        if (skipSymbols && n >= 98U) n += 68U;
        bool b2 = READ_BIT(data, n) != 0x00U;

        int8_t dibit;
        if (!b1 && b2)
            dibit = +3;
        else if (!b1 && !b2)
            dibit = +1;
        else if (b1 && !b2)
            dibit = -1;
        else
            dibit = -3;

        n = INTERLEAVE_TABLE[i];
        dibits[n] = dibit;
    }
}

/* Helper to interleave the input dibits into symbols. */

void Trellis::interleave(const int8_t* dibits, uint8_t* data, bool skipSymbols) const
{
    for (uint32_t i = 0U; i < 98U; i++) {
        uint32_t n = INTERLEAVE_TABLE[i];

        bool b1, b2;
        switch (dibits[n]) {
            case +3:
                b1 = false;
                b2 = true;
                break;
            case +1:
                b1 = false;
                b2 = false;
                break;
            case -1:
                b1 = true;
                b2 = false;
                break;
            default:
                b1 = true;
                b2 = true;
                break;
        }

        n = i * 2U + 0U;
        if (skipSymbols && n >= 98U) n += 68U;
        WRITE_BIT(data, n, b1);

        n = i * 2U + 1U;
        if (skipSymbols && n >= 98U) n += 68U;
        WRITE_BIT(data, n, b2);
    }
}

/* Helper to map dibits to 4FSK constellation points. */

void Trellis::dibitsToPoints(const int8_t* dibits, uint8_t* points) const
{
    for (uint32_t i = 0U; i < 49U; i++) {
        if (dibits[i * 2U + 0U] == +1 && dibits[i * 2U + 1U] == -1)
            points[i] = 0U;
        else if (dibits[i * 2U + 0U] == -1 && dibits[i * 2U + 1U] == -1)
            points[i] = 1U;
        else if (dibits[i * 2U + 0U] == +3 && dibits[i * 2U + 1U] == -3)
            points[i] = 2U;
        else if (dibits[i * 2U + 0U] == -3 && dibits[i * 2U + 1U] == -3)
            points[i] = 3U;
        else if (dibits[i * 2U + 0U] == -3 && dibits[i * 2U + 1U] == -1)
            points[i] = 4U;
        else if (dibits[i * 2U + 0U] == +3 && dibits[i * 2U + 1U] == -1)
            points[i] = 5U;
        else if (dibits[i * 2U + 0U] == -1 && dibits[i * 2U + 1U] == -3)
            points[i] = 6U;
        else if (dibits[i * 2U + 0U] == +1 && dibits[i * 2U + 1U] == -3)
            points[i] = 7U;
        else if (dibits[i * 2U + 0U] == -3 && dibits[i * 2U + 1U] == +3)
            points[i] = 8U;
        else if (dibits[i * 2U + 0U] == +3 && dibits[i * 2U + 1U] == +3)
            points[i] = 9U;
        else if (dibits[i * 2U + 0U] == -1 && dibits[i * 2U + 1U] == +1)
            points[i] = 10U;
        else if (dibits[i * 2U + 0U] == +1 && dibits[i * 2U + 1U] == +1)
            points[i] = 11U;
        else if (dibits[i * 2U + 0U] == +1 && dibits[i * 2U + 1U] == +3)
            points[i] = 12U;
        else if (dibits[i * 2U + 0U] == -1 && dibits[i * 2U + 1U] == +3)
            points[i] = 13U;
        else if (dibits[i * 2U + 0U] == +3 && dibits[i * 2U + 1U] == +1)
            points[i] = 14U;
        else if (dibits[i * 2U + 0U] == -3 && dibits[i * 2U + 1U] == +1)
            points[i] = 15U;
    }
}

/* Helper to map 4FSK constellation points to dibits. */

void Trellis::pointsToDibits(const uint8_t* points, int8_t* dibits) const
{
    for (uint32_t i = 0U; i < 49U; i++) {
        switch (points[i]) {
            case 0U:
                dibits[i * 2U + 0U] = +1;
                dibits[i * 2U + 1U] = -1;
                break;
            case 1U:
                dibits[i * 2U + 0U] = -1;
                dibits[i * 2U + 1U] = -1;
                break;
            case 2U:
                dibits[i * 2U + 0U] = +3;
                dibits[i * 2U + 1U] = -3;
                break;
            case 3U:
                dibits[i * 2U + 0U] = -3;
                dibits[i * 2U + 1U] = -3;
                break;
            case 4U:
                dibits[i * 2U + 0U] = -3;
                dibits[i * 2U + 1U] = -1;
                break;
            case 5U:
                dibits[i * 2U + 0U] = +3;
                dibits[i * 2U + 1U] = -1;
                break;
            case 6U:
                dibits[i * 2U + 0U] = -1;
                dibits[i * 2U + 1U] = -3;
                break;
            case 7U:
                dibits[i * 2U + 0U] = +1;
                dibits[i * 2U + 1U] = -3;
                break;
            case 8U:
                dibits[i * 2U + 0U] = -3;
                dibits[i * 2U + 1U] = +3;
                break;
            case 9U:
                dibits[i * 2U + 0U] = +3;
                dibits[i * 2U + 1U] = +3;
                break;
            case 10U:
                dibits[i * 2U + 0U] = -1;
                dibits[i * 2U + 1U] = +1;
                break;
            case 11U:
                dibits[i * 2U + 0U] = +1;
                dibits[i * 2U + 1U] = +1;
                break;
            case 12U:
                dibits[i * 2U + 0U] = +1;
                dibits[i * 2U + 1U] = +3;
                break;
            case 13U:
                dibits[i * 2U + 0U] = -1;
                dibits[i * 2U + 1U] = +3;
                break;
            case 14U:
                dibits[i * 2U + 0U] = +3;
                dibits[i * 2U + 1U] = +1;
                break;
            default:
                dibits[i * 2U + 0U] = -3;
                dibits[i * 2U + 1U] = +1;
                break;
        }
    }
}

/* Helper to convert a byte payload into tribits. */

void Trellis::bitsToTribits(const uint8_t* payload, uint8_t* tribits) const
{
    for (uint32_t i = 0U; i < 48U; i++) {
        uint32_t n = i * 3U;

        bool b1 = READ_BIT(payload, n) != 0x00U;
        n++;
        bool b2 = READ_BIT(payload, n) != 0x00U;
        n++;
        bool b3 = READ_BIT(payload, n) != 0x00U;

        uint8_t tribit = 0U;
        tribit |= b1 ? 4U : 0U;
        tribit |= b2 ? 2U : 0U;
        tribit |= b3 ? 1U : 0U;

        tribits[i] = tribit;
    }

    tribits[48U] = 0U;
}

/* Helper to convert a byte payload into dibits. */

void Trellis::bitsToDibits(const uint8_t* payload, uint8_t* dibits) const
{
    for (uint32_t i = 0U; i < 48U; i++) {
        uint32_t n = i * 2U;

        bool b1 = READ_BIT(payload, n) != 0x00U;
        n++;
        bool b2 = READ_BIT(payload, n) != 0x00U;

        uint8_t dibit = 0U;
        dibit |= b1 ? 2U : 0U;
        dibit |= b2 ? 1U : 0U;

        dibits[i] = dibit;
    }

    dibits[48U] = 0U;
}

/* Helper to convert tribits into a byte payload. */

void Trellis::tribitsToBits(const uint8_t* tribits, uint8_t* payload) const
{
    for (uint32_t i = 0U; i < 48U; i++) {
        uint8_t tribit = tribits[i];

        bool b1 = (tribit & 0x04U) == 0x04U;
        bool b2 = (tribit & 0x02U) == 0x02U;
        bool b3 = (tribit & 0x01U) == 0x01U;

        uint32_t n = i * 3U;

        WRITE_BIT(payload, n, b1);
        n++;
        WRITE_BIT(payload, n, b2);
        n++;
        WRITE_BIT(payload, n, b3);
    }
}

/* Helper to convert tribits into a byte payload. */

void Trellis::dibitsToBits(const uint8_t* dibits, uint8_t* payload) const
{
    for (uint32_t i = 0U; i < 48U; i++) {
        uint8_t dibit = dibits[i];

        bool b1 = (dibit & 0x02U) == 0x02U;
        bool b2 = (dibit & 0x01U) == 0x01U;

        uint32_t n = i * 2U;

        WRITE_BIT(payload, n, b1);
        n++;
        WRITE_BIT(payload, n, b2);
    }
}

/* Runs a bounded list Viterbi decode of the 3/4 rate Trellis. */

uint32_t Trellis::viterbi34(const int8_t* dibits, uint8_t* payloads, uint32_t* metrics, uint32_t maxPaths,
    bool& truncated) const
{
    assert(dibits != nullptr);
    assert(payloads != nullptr);
    assert(metrics != nullptr);
    assert(maxPaths >= 1U && maxPaths <= TRELLIS_34_MAX_LIST_PATHS);

    // keep the best maxPaths survivors per state, ordered by metric
    struct Survivor {
        uint16_t metric;
        uint8_t prevState;
        uint8_t prevRank;
        bool truncated;
    };

    Survivor trace[TRELLIS_STEPS][TRELLIS_34_STATES][TRELLIS_34_MAX_LIST_PATHS];
    uint8_t traceCnt[TRELLIS_STEPS][TRELLIS_34_STATES];
    ::memset(traceCnt, 0x00U, sizeof(traceCnt));

    Survivor current[TRELLIS_34_STATES][TRELLIS_34_MAX_LIST_PATHS];
    uint8_t currentCnt[TRELLIS_34_STATES];
    ::memset(currentCnt, 0x00U, sizeof(currentCnt));
    current[0U][0U] = { 0U, 0U, 0U, false };
    currentCnt[0U] = 1U;

    // for each step and destination state, retain the best maxPaths candidates across all predecessor states
    for (uint32_t i = 0U; i < TRELLIS_STEPS; i++) {
        const int8_t rx0 = dibits[i * 2U];
        const int8_t rx1 = dibits[i * 2U + 1U];

        // for each destination state, merge candidates from all predecessor states in metric order
        for (uint32_t next = 0U; next < TRELLIS_34_STATES; next++) {
            Survivor* best = trace[i][next];
            uint32_t bestCnt = 0U;

            // compute the branch metric for each predecessor state and insert its survivors in sorted order
            for (uint32_t state = 0U; state < TRELLIS_34_STATES; state++) {
                const uint8_t point = ENCODE_TABLE_34[state * TRELLIS_34_STATES + next];
                const uint32_t branch = (uint32_t)((::abs(POINT_DIBITS[point][0U] - rx0) + ::abs(POINT_DIBITS[point][1U] - rx1)) / 2);

                // insert each predecessor survivor by metric, keeping only the best maxPaths candidates
                for (uint32_t rank = 0U; rank < currentCnt[state]; rank++) {
                    const uint16_t metric = (uint16_t)(current[state][rank].metric + branch);
                    if (bestCnt == maxPaths && metric >= best[bestCnt - 1U].metric) {
                        if (metric == best[bestCnt - 1U].metric) {
                            for (uint32_t retained = 0U; retained < bestCnt; retained++) {
                                if (best[retained].metric == metric)
                                    best[retained].truncated = true;
                            }
                        }
                        continue;
                    }

                    // find the insertion point, shifting worse candidates right and dropping the last if full
                    uint32_t pos = (bestCnt < maxPaths) ? bestCnt++ : bestCnt - 1U;
                    while (pos > 0U && best[pos - 1U].metric > metric) {
                        best[pos] = best[pos - 1U];
                        pos--;
                    }

                    best[pos] = { metric, (uint8_t)state, (uint8_t)rank, current[state][rank].truncated };
                }
            }

            traceCnt[i][next] = (uint8_t)bestCnt;
        }

        // copy the retained survivors into the current path lists for the next trellis step
        for (uint32_t state = 0U; state < TRELLIS_34_STATES; state++) {
            currentCnt[state] = traceCnt[i][state];
            for (uint32_t rank = 0U; rank < currentCnt[state]; rank++)
                current[state][rank] = trace[i][state][rank];
        }
    }

    // backtrack each retained path from the final state to recover its tribits and metric
    const uint32_t count = traceCnt[TRELLIS_STEPS - 1U][0U];
    const uint16_t bestMetric = trace[TRELLIS_STEPS - 1U][0U][0U].metric;
    for (uint32_t path = 0U; path < count; path++) {
        uint8_t tribits[TRELLIS_STEPS];
        uint8_t state = 0U;
        uint8_t rank = (uint8_t)path;

        metrics[path] = trace[TRELLIS_STEPS - 1U][0U][path].metric;
        if (metrics[path] == bestMetric && trace[TRELLIS_STEPS - 1U][0U][path].truncated)
            truncated = true;

        // follow predecessor state and rank links backward, recording each tribit
        for (int32_t i = (int32_t)TRELLIS_STEPS - 1; i >= 0; i--) {
            const Survivor& survivor = trace[i][state][rank];
            tribits[i] = state;
            state = survivor.prevState;
            rank = survivor.prevRank;
        }

        tribitsToBits(tribits, payloads + path * TRELLIS_34_PAYLOAD_LENGTH_BYTES);
    }

    return count;
}

/* Runs a Viterbi decode of the 1/2 rate Trellis. */

bool Trellis::viterbi12(const int8_t* dibits, uint8_t* payload, uint32_t& metric) const
{
    assert(dibits != nullptr);
    assert(payload != nullptr);

    // keep the best path to each state at every step for traceback
    struct Survivor {
        uint16_t metric;
        uint8_t prevState;
    };

    Survivor trace[TRELLIS_STEPS][TRELLIS_12_STATES];
    uint16_t currentMetric[TRELLIS_12_STATES] = { 0U, UINT16_MAX, UINT16_MAX, UINT16_MAX };

    // start in state zero and update the best path metric for each destination state at every step
    for (uint32_t i = 0U; i < TRELLIS_STEPS; i++) {
        const int8_t rx0 = dibits[i * 2U];
        const int8_t rx1 = dibits[i * 2U + 1U];

        // for each destination state, select the predecessor with the lowest accumulated path metric
        for (uint32_t next = 0U; next < TRELLIS_12_STATES; next++) {
            uint16_t bestMetric = UINT16_MAX;
            uint8_t bestState = 0U;

            // skip unreachable states and compute the branch metric for each reachable predecessor
            for (uint32_t state = 0U; state < TRELLIS_12_STATES; state++) {
                if (currentMetric[state] == UINT16_MAX)
                    continue;

                const uint8_t point = ENCODE_TABLE_12[state * TRELLIS_12_STATES + next];
                const uint32_t branch = (uint32_t)((::abs(POINT_DIBITS[point][0U] - rx0) +
                    ::abs(POINT_DIBITS[point][1U] - rx1)) / 2);
                const uint16_t candidate = (uint16_t)(currentMetric[state] + branch);
                if (candidate < bestMetric) {
                    bestMetric = candidate;
                    bestState = (uint8_t)state;
                }
            }

            trace[i][next] = { bestMetric, bestState };
        }

        // carry the best metric for each state forward to the next trellis step
        for (uint32_t state = 0U; state < TRELLIS_12_STATES; state++)
            currentMetric[state] = trace[i][state].metric;
    }

    uint8_t decoded[TRELLIS_STEPS];
    uint8_t state = 0U;
    metric = trace[TRELLIS_STEPS - 1U][state].metric;
    if (metric == UINT16_MAX)
        return false;

    // trace back from the final state to recover the decoded dibits
    for (int32_t i = (int32_t)TRELLIS_STEPS - 1; i >= 0; i--) {
        decoded[i] = state;
        state = trace[i][state].prevState;
    }

    dibitsToBits(decoded, payload);
    return true;
}
