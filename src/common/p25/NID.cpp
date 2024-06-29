// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017,2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/Utils.h"
#include "p25/P25Defines.h"
#include "p25/NID.h"
#include "p25/P25Utils.h"
#include "edac/BCH.h"

using namespace p25;
using namespace p25::defines;

#include <cassert>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t MAX_NID_ERRS = 7U;//5U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the NID class. */

NID::NID(uint32_t nac) :
    m_duid(DUID::HDU),
    m_nac(nac),
    m_rxTx(nullptr),
    m_tx(nullptr),
    m_splitNac(false)
{
    m_rxTx = new uint8_t*[16U];
    for (uint8_t i = 0; i < 16U; i++)
        m_rxTx[i] = nullptr;

    m_tx = new uint8_t*[16U];
    for (uint8_t i = 0; i < 16U; i++)
        m_tx[i] = nullptr;

    createRxTxNID(nac);
}

/* Finalizes a instance of the NID class. */

NID::~NID()
{
    cleanupArrays();
    delete[] m_rxTx;
    delete[] m_tx;
}

/* Decodes P25 network identifier data. */

bool NID::decode(const uint8_t* data)
{
    assert(data != nullptr);

    uint8_t nid[P25_NID_LENGTH_BYTES];
    P25Utils::decode(data, nid, 48U, 114U);

    // handle digital "squelch" NAC
    if ((m_nac == NAC_DIGITAL_SQ) || (m_nac == NAC_REUSE_RX_NAC)) {
        uint32_t nac = ((nid[0U] << 4) + (nid[1U] >> 4)) & 0xFFFU;
        cleanupArrays();
        createRxTxNID(nac); // bryanb: I hate this and it'll be slow
    }

    uint32_t errs = P25Utils::compare(nid, m_rxTx[DUID::LDU1], P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = DUID::LDU1;
        return true;
    }

    errs = P25Utils::compare(nid, m_rxTx[DUID::LDU2], P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = DUID::LDU2;
        return true;
    }

    errs = P25Utils::compare(nid, m_rxTx[DUID::PDU], P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = DUID::PDU;
        return true;
    }

    errs = P25Utils::compare(nid, m_rxTx[DUID::TSDU], P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = DUID::TSDU;
        return true;
    }

    errs = P25Utils::compare(nid, m_rxTx[DUID::HDU], P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = DUID::HDU;
        return true;
    }

    errs = P25Utils::compare(nid, m_rxTx[DUID::TDULC], P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = DUID::TDULC;
        return true;
    }

    errs = P25Utils::compare(nid, m_rxTx[DUID::TDU], P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = DUID::TDU;
        return true;
    }

    return false;
}

/* Encodes P25 network identifier data. */

void NID::encode(uint8_t* data, defines::DUID::E duid)
{
    assert(data != nullptr);

    if (m_splitNac) {
        switch (duid) {
            case DUID::HDU:
            case DUID::TDU:
            case DUID::LDU1:
            case DUID::PDU:
            case DUID::TSDU:
            case DUID::LDU2:
            case DUID::TDULC:
                P25Utils::encode(m_tx[duid], data, 48U, 114U);
                break;
            default:
                break;
        }
    }
    else {
        // handle digital "squelch" NAC
        if (m_nac == NAC_DIGITAL_SQ) {
            cleanupArrays();
            createRxTxNID(DEFAULT_NAC);
        }

        switch (duid) {
            case DUID::HDU:
            case DUID::TDU:
            case DUID::LDU1:
            case DUID::PDU:
            case DUID::TSDU:
            case DUID::LDU2:
            case DUID::TDULC:
                P25Utils::encode(m_rxTx[duid], data, 48U, 114U);
                break;
            default:
                break;
        }
    }
}

/* Helper to configure a separate Tx NAC. */

void NID::setTxNAC(uint32_t nac)
{
    if (nac == m_nac) {
        return;
    }

    m_splitNac = true;
    createTxNID(nac);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Cleanup NID arrays. */

void NID::cleanupArrays()
{
    for (uint8_t i = 0; i < 16U; i++)
    {
        if (m_rxTx[i] != nullptr) {
            delete[] m_rxTx[i];
        }

        if (m_tx[i] != nullptr) {
            delete[] m_tx[i];
        }
    }
}

/* Internal helper to create the Rx/Tx NID. */

void NID::createRxTxNID(uint32_t nac)
{
    edac::BCH bch;

    m_rxTx[DUID::HDU] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_rxTx[DUID::HDU][0U] = (nac >> 4) & 0xFFU;
    m_rxTx[DUID::HDU][1U] = (nac << 4) & 0xF0U;
    m_rxTx[DUID::HDU][1U] |= DUID::HDU;
    bch.encode(m_rxTx[DUID::HDU]);
    m_rxTx[DUID::HDU][7U] &= 0xFEU;                          // Clear the parity bit

    m_rxTx[DUID::TDU] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_rxTx[DUID::TDU][0U] = (nac >> 4) & 0xFFU;
    m_rxTx[DUID::TDU][1U] = (nac << 4) & 0xF0U;
    m_rxTx[DUID::TDU][1U] |= DUID::TDU;
    bch.encode(m_rxTx[DUID::TDU]);
    m_rxTx[DUID::TDU][7U] &= 0xFEU;                          // Clear the parity bit

    m_rxTx[DUID::LDU1] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_rxTx[DUID::LDU1][0U] = (nac >> 4) & 0xFFU;
    m_rxTx[DUID::LDU1][1U] = (nac << 4) & 0xF0U;
    m_rxTx[DUID::LDU1][1U] |= DUID::LDU1;
    bch.encode(m_rxTx[DUID::LDU1]);
    m_rxTx[DUID::LDU1][7U] |= 0x01U;                         // Set the parity bit

    m_rxTx[DUID::PDU] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_rxTx[DUID::PDU][0U] = (nac >> 4) & 0xFFU;
    m_rxTx[DUID::PDU][1U] = (nac << 4) & 0xF0U;
    m_rxTx[DUID::PDU][1U] |= DUID::PDU;
    bch.encode(m_rxTx[DUID::PDU]);
    m_rxTx[DUID::PDU][7U] &= 0xFEU;                          // Clear the parity bit

    m_rxTx[DUID::TSDU] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_rxTx[DUID::TSDU][0U] = (nac >> 4) & 0xFFU;
    m_rxTx[DUID::TSDU][1U] = (nac << 4) & 0xF0U;
    m_rxTx[DUID::TSDU][1U] |= DUID::TSDU;
    bch.encode(m_rxTx[DUID::TSDU]);
    m_rxTx[DUID::TSDU][7U] &= 0xFEU;                         // Clear the parity bit

    m_rxTx[DUID::LDU2] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_rxTx[DUID::LDU2][0U] = (nac >> 4) & 0xFFU;
    m_rxTx[DUID::LDU2][1U] = (nac << 4) & 0xF0U;
    m_rxTx[DUID::LDU2][1U] |= DUID::LDU2;
    bch.encode(m_rxTx[DUID::LDU2]);
    m_rxTx[DUID::LDU2][7U] |= 0x01U;                         // Set the parity bit

    m_rxTx[DUID::TDULC] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_rxTx[DUID::TDULC][0U] = (nac >> 4) & 0xFFU;
    m_rxTx[DUID::TDULC][1U] = (nac << 4) & 0xF0U;
    m_rxTx[DUID::TDULC][1U] |= DUID::TDULC;
    bch.encode(m_rxTx[DUID::TDULC]);
    m_rxTx[DUID::TDULC][7U] &= 0xFEU;                        // Clear the parity bit
}

/* Internal helper to create Tx NID. */

void NID::createTxNID(uint32_t nac)
{
    edac::BCH bch;

    m_tx[DUID::HDU] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_tx[DUID::HDU][0U] = (nac >> 4) & 0xFFU;
    m_tx[DUID::HDU][1U] = (nac << 4) & 0xF0U;
    m_tx[DUID::HDU][1U] |= DUID::HDU;
    bch.encode(m_tx[DUID::HDU]);
    m_tx[DUID::HDU][7U] &= 0xFEU;                            // Clear the parity bit

    m_tx[DUID::TDU] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_tx[DUID::TDU][0U] = (nac >> 4) & 0xFFU;
    m_tx[DUID::TDU][1U] = (nac << 4) & 0xF0U;
    m_tx[DUID::TDU][1U] |= DUID::TDU;
    bch.encode(m_tx[DUID::TDU]);
    m_tx[DUID::TDU][7U] &= 0xFEU;                            // Clear the parity bit

    m_tx[DUID::LDU1] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_tx[DUID::LDU1][0U] = (nac >> 4) & 0xFFU;
    m_tx[DUID::LDU1][1U] = (nac << 4) & 0xF0U;
    m_tx[DUID::LDU1][1U] |= DUID::LDU1;
    bch.encode(m_tx[DUID::LDU1]);
    m_tx[DUID::LDU1][7U] |= 0x01U;                           // Set the parity bit

    m_tx[DUID::PDU] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_tx[DUID::PDU][0U] = (nac >> 4) & 0xFFU;
    m_tx[DUID::PDU][1U] = (nac << 4) & 0xF0U;
    m_tx[DUID::PDU][1U] |= DUID::PDU;
    bch.encode(m_tx[DUID::PDU]);
    m_tx[DUID::PDU][7U] &= 0xFEU;                            // Clear the parity bit

    m_tx[DUID::TSDU] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_tx[DUID::TSDU][0U] = (nac >> 4) & 0xFFU;
    m_tx[DUID::TSDU][1U] = (nac << 4) & 0xF0U;
    m_tx[DUID::TSDU][1U] |= DUID::TSDU;
    bch.encode(m_tx[DUID::TSDU]);
    m_tx[DUID::TSDU][7U] &= 0xFEU;                           // Clear the parity bit

    m_tx[DUID::LDU2] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_tx[DUID::LDU2][0U] = (nac >> 4) & 0xFFU;
    m_tx[DUID::LDU2][1U] = (nac << 4) & 0xF0U;
    m_tx[DUID::LDU2][1U] |= DUID::LDU2;
    bch.encode(m_tx[DUID::LDU2]);
    m_tx[DUID::LDU2][7U] |= 0x01U;                           // Set the parity bit

    m_tx[DUID::TDULC] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_tx[DUID::TDULC][0U] = (nac >> 4) & 0xFFU;
    m_tx[DUID::TDULC][1U] = (nac << 4) & 0xF0U;
    m_tx[DUID::TDULC][1U] |= DUID::TDULC;
    bch.encode(m_tx[DUID::TDULC]);
    m_tx[DUID::TDULC][7U] &= 0xFEU;                          // Clear the parity bit
}
