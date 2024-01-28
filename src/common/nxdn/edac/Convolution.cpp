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
*   Copyright (C) 2015,2016,2018,2021 Jonathan Naylor, G4KLX
*   Copyright (C) 2022 Bryan Biedenkapp, N2PLL
*
*/
#include "nxdn/edac/Convolution.h"
#include "Utils.h"

using namespace nxdn::edac;

#include <cassert>
#include <cstring>
#include <cstdlib>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint8_t BRANCH_TABLE1[] = { 0U, 0U, 0U, 0U, 2U, 2U, 2U, 2U };
const uint8_t BRANCH_TABLE2[] = { 0U, 2U, 2U, 0U, 0U, 2U, 2U, 0U };

const uint32_t NUM_OF_STATES_D2 = 8U;
const uint32_t NUM_OF_STATES = 16U;
const uint32_t M = 4U;
const uint32_t K = 5U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the Convolution class.
/// </summary>
Convolution::Convolution() :
    m_metrics1(nullptr),
    m_metrics2(nullptr),
    m_oldMetrics(nullptr),
    m_newMetrics(nullptr),
    m_decisions(nullptr),
    m_dp(nullptr)
{
    m_metrics1  = new uint16_t[20U];
    m_metrics2  = new uint16_t[20U];
    m_decisions = new uint64_t[300U];
}

/// <summary>
/// Finalizes a instance of the Convolution class.
/// </summary>
Convolution::~Convolution()
{
    delete[] m_metrics1;
    delete[] m_metrics2;
    delete[] m_decisions;
}

/// <summary>
///
/// </summary>
void Convolution::start()
{
    ::memset(m_metrics1, 0x00U, NUM_OF_STATES * sizeof(uint16_t));
    ::memset(m_metrics2, 0x00U, NUM_OF_STATES * sizeof(uint16_t));

    m_oldMetrics = m_metrics1;
    m_newMetrics = m_metrics2;
    m_dp = m_decisions;
}

/// <summary>
///
/// </summary>
/// <param name="out"></param>
/// <param name="nBits"></param>
uint32_t Convolution::chainback(uint8_t* out, uint32_t nBits)
{
    assert(out != nullptr);

    uint32_t state = 0U;

    while (nBits-- > 0) {
        --m_dp;

        uint32_t  i = state >> (9 - K);
        uint8_t bit = uint8_t(*m_dp >> i) & 1;
        state = (bit << 7) | (state >> 1);

        WRITE_BIT(out, nBits, bit != 0U);
    }

    uint32_t minCost = m_oldMetrics[0];

    for (uint32_t i = 0U; i < NUM_OF_STATES; i++) {
        if (m_oldMetrics[i] < minCost)
            minCost = m_oldMetrics[i];
    }

    return minCost / (M >> 1);
}

/// <summary>
///
/// </summary>
/// <param name="s0"></param>
/// <param name="s1"></param>
bool Convolution::decode(uint8_t s0, uint8_t s1)
{
    *m_dp = 0U;

    for (uint8_t i = 0U; i < NUM_OF_STATES_D2; i++) {
        uint8_t j = i * 2U;

        uint16_t metric = std::abs(BRANCH_TABLE1[i] - s0) + std::abs(BRANCH_TABLE2[i] - s1);

        uint16_t m0 = m_oldMetrics[i] + metric;
        uint16_t m1 = m_oldMetrics[i + NUM_OF_STATES_D2] + (M - metric);
        uint8_t decision0 = (m0 >= m1) ? 1U : 0U;
        m_newMetrics[j + 0U] = decision0 != 0U ? m1 : m0;

        m0 = m_oldMetrics[i] + (M - metric);
        m1 = m_oldMetrics[i + NUM_OF_STATES_D2] + metric;
        uint8_t decision1 = (m0 >= m1) ? 1U : 0U;
        m_newMetrics[j + 1U] = decision1 != 0U ? m1 : m0;

        *m_dp |= (uint64_t(decision1) << (j + 1U)) | (uint64_t(decision0) << (j + 0U));
    }

    ++m_dp;

    if ((m_dp - m_decisions) > 300) {
        return false;
    }

    uint16_t* tmp = m_oldMetrics;
    m_oldMetrics = m_newMetrics;
    m_newMetrics = tmp;

    return true;
}

/// <summary>
///
/// </summary>
/// <param name="in"></param>
/// <param name="out"></param>
/// <param name="nBits"></param>
void Convolution::encode(const uint8_t* in, uint8_t* out, uint32_t nBits) const
{
    assert(in != nullptr);
    assert(out != nullptr);
    assert(nBits > 0U);

    uint8_t d1 = 0U, d2 = 0U, d3 = 0U, d4 = 0U;
    uint32_t k = 0U;
    for (uint32_t i = 0U; i < nBits; i++) {
        uint8_t d = READ_BIT(in, i) ? 1U : 0U;

        uint8_t g1 = (d + d3 + d4) & 1;
        uint8_t g2 = (d + d1 + d2 + d4) & 1;

        d4 = d3;
        d3 = d2;
        d2 = d1;
        d1 = d;

        WRITE_BIT(out, k, g1 != 0U);
        k++;

        WRITE_BIT(out, k, g2 != 0U);
        k++;
    }
}
