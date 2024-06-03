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
*   Copyright (C) 2016 Jonathan Naylor, G4KLX
*   Copyright (C) 2017,2022 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "common/Utils.h"
#include "p25/P25Defines.h"
#include "p25/NID.h"
#include "p25/P25Utils.h"
#include "edac/BCH.h"

using namespace p25;

#include <cassert>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t MAX_NID_ERRS = 7U;//5U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the NID class.
/// </summary>
/// <param name="nac">P25 Network Access Code.</param>
NID::NID(uint32_t nac) :
    m_duid(0U),
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

/// <summary>
/// Finalizes a instance of the NID class.
/// </summary>
NID::~NID()
{
    cleanupArrays();
    delete[] m_rxTx;
    delete[] m_tx;
}

/// <summary>
/// Decodes P25 network identifier data.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
bool NID::decode(const uint8_t* data)
{
    assert(data != nullptr);

    uint8_t nid[P25_NID_LENGTH_BYTES];
    P25Utils::decode(data, nid, 48U, 114U);

    // handle digital "squelch" NAC
    if ((m_nac == P25_NAC_DIGITAL_SQ) || (m_nac == P25_NAC_REUSE_RX_NAC)) {
        uint32_t nac = ((nid[0U] << 4) + (nid[1U] >> 4)) & 0xFFFU;
        cleanupArrays();
        createRxTxNID(nac); // bryanb: I hate this and it'll be slow
    }

    uint32_t errs = P25Utils::compare(nid, m_rxTx[P25_DUID_LDU1], P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = P25_DUID_LDU1;
        return true;
    }

    errs = P25Utils::compare(nid, m_rxTx[P25_DUID_LDU2], P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = P25_DUID_LDU2;
        return true;
    }

    errs = P25Utils::compare(nid, m_rxTx[P25_DUID_PDU], P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = P25_DUID_PDU;
        return true;
    }

    errs = P25Utils::compare(nid, m_rxTx[P25_DUID_TSDU], P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = P25_DUID_TSDU;
        return true;
    }

    errs = P25Utils::compare(nid, m_rxTx[P25_DUID_HDU], P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = P25_DUID_HDU;
        return true;
    }

    errs = P25Utils::compare(nid, m_rxTx[P25_DUID_TDULC], P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = P25_DUID_TDULC;
        return true;
    }

    errs = P25Utils::compare(nid, m_rxTx[P25_DUID_TDU], P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = P25_DUID_TDU;
        return true;
    }

    return false;
}

/// <summary>
/// Encodes P25 network identifier data.
/// </summary>
/// <param name="data"></param>
/// <param name="duid"></param>
void NID::encode(uint8_t* data, uint8_t duid)
{
    assert(data != nullptr);

    if (m_splitNac) {
        switch (duid) {
            case P25_DUID_HDU:
            case P25_DUID_TDU:
            case P25_DUID_LDU1:
            case P25_DUID_PDU:
            case P25_DUID_TSDU:
            case P25_DUID_LDU2:
            case P25_DUID_TDULC:
                P25Utils::encode(m_tx[duid], data, 48U, 114U);
                break;
            default:
                break;
        }
    }
    else {
        // handle digital "squelch" NAC
        if (m_nac == P25_NAC_DIGITAL_SQ) {
            cleanupArrays();
            createRxTxNID(P25_DEFAULT_NAC);
        }

        switch (duid) {
            case P25_DUID_HDU:
            case P25_DUID_TDU:
            case P25_DUID_LDU1:
            case P25_DUID_PDU:
            case P25_DUID_TSDU:
            case P25_DUID_LDU2:
            case P25_DUID_TDULC:
                P25Utils::encode(m_rxTx[duid], data, 48U, 114U);
                break;
            default:
                break;
        }
    }
}

/// <summary>
/// Helper to configure a separate Tx NAC.
/// </summary>
/// <param name="nac"></param>
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

/// <summary>
///
/// </summary>
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

/// <summary>
///
/// </summary>
/// <param name="nac"></param>
void NID::createRxTxNID(uint32_t nac)
{
    edac::BCH bch;

    m_rxTx[P25_DUID_HDU] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_rxTx[P25_DUID_HDU][0U] = (nac >> 4) & 0xFFU;
    m_rxTx[P25_DUID_HDU][1U] = (nac << 4) & 0xF0U;
    m_rxTx[P25_DUID_HDU][1U] |= P25_DUID_HDU;
    bch.encode(m_rxTx[P25_DUID_HDU]);
    m_rxTx[P25_DUID_HDU][7U] &= 0xFEU;                          // Clear the parity bit

    m_rxTx[P25_DUID_TDU] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_rxTx[P25_DUID_TDU][0U] = (nac >> 4) & 0xFFU;
    m_rxTx[P25_DUID_TDU][1U] = (nac << 4) & 0xF0U;
    m_rxTx[P25_DUID_TDU][1U] |= P25_DUID_TDU;
    bch.encode(m_rxTx[P25_DUID_TDU]);
    m_rxTx[P25_DUID_TDU][7U] &= 0xFEU;                          // Clear the parity bit

    m_rxTx[P25_DUID_LDU1] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_rxTx[P25_DUID_LDU1][0U] = (nac >> 4) & 0xFFU;
    m_rxTx[P25_DUID_LDU1][1U] = (nac << 4) & 0xF0U;
    m_rxTx[P25_DUID_LDU1][1U] |= P25_DUID_LDU1;
    bch.encode(m_rxTx[P25_DUID_LDU1]);
    m_rxTx[P25_DUID_LDU1][7U] |= 0x01U;                         // Set the parity bit

    m_rxTx[P25_DUID_PDU] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_rxTx[P25_DUID_PDU][0U] = (nac >> 4) & 0xFFU;
    m_rxTx[P25_DUID_PDU][1U] = (nac << 4) & 0xF0U;
    m_rxTx[P25_DUID_PDU][1U] |= P25_DUID_PDU;
    bch.encode(m_rxTx[P25_DUID_PDU]);
    m_rxTx[P25_DUID_PDU][7U] &= 0xFEU;                          // Clear the parity bit

    m_rxTx[P25_DUID_TSDU] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_rxTx[P25_DUID_TSDU][0U] = (nac >> 4) & 0xFFU;
    m_rxTx[P25_DUID_TSDU][1U] = (nac << 4) & 0xF0U;
    m_rxTx[P25_DUID_TSDU][1U] |= P25_DUID_TSDU;
    bch.encode(m_rxTx[P25_DUID_TSDU]);
    m_rxTx[P25_DUID_TSDU][7U] &= 0xFEU;                         // Clear the parity bit

    m_rxTx[P25_DUID_LDU2] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_rxTx[P25_DUID_LDU2][0U] = (nac >> 4) & 0xFFU;
    m_rxTx[P25_DUID_LDU2][1U] = (nac << 4) & 0xF0U;
    m_rxTx[P25_DUID_LDU2][1U] |= P25_DUID_LDU2;
    bch.encode(m_rxTx[P25_DUID_LDU2]);
    m_rxTx[P25_DUID_LDU2][7U] |= 0x01U;                         // Set the parity bit

    m_rxTx[P25_DUID_TDULC] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_rxTx[P25_DUID_TDULC][0U] = (nac >> 4) & 0xFFU;
    m_rxTx[P25_DUID_TDULC][1U] = (nac << 4) & 0xF0U;
    m_rxTx[P25_DUID_TDULC][1U] |= P25_DUID_TDULC;
    bch.encode(m_rxTx[P25_DUID_TDULC]);
    m_rxTx[P25_DUID_TDULC][7U] &= 0xFEU;                        // Clear the parity bit
}

/// <summary>
///
/// </summary>
/// <param name="nac"></param>
void NID::createTxNID(uint32_t nac)
{
    edac::BCH bch;

    m_tx[P25_DUID_HDU] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_tx[P25_DUID_HDU][0U] = (nac >> 4) & 0xFFU;
    m_tx[P25_DUID_HDU][1U] = (nac << 4) & 0xF0U;
    m_tx[P25_DUID_HDU][1U] |= P25_DUID_HDU;
    bch.encode(m_tx[P25_DUID_HDU]);
    m_tx[P25_DUID_HDU][7U] &= 0xFEU;                            // Clear the parity bit

    m_tx[P25_DUID_TDU] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_tx[P25_DUID_TDU][0U] = (nac >> 4) & 0xFFU;
    m_tx[P25_DUID_TDU][1U] = (nac << 4) & 0xF0U;
    m_tx[P25_DUID_TDU][1U] |= P25_DUID_TDU;
    bch.encode(m_tx[P25_DUID_TDU]);
    m_tx[P25_DUID_TDU][7U] &= 0xFEU;                            // Clear the parity bit

    m_tx[P25_DUID_LDU1] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_tx[P25_DUID_LDU1][0U] = (nac >> 4) & 0xFFU;
    m_tx[P25_DUID_LDU1][1U] = (nac << 4) & 0xF0U;
    m_tx[P25_DUID_LDU1][1U] |= P25_DUID_LDU1;
    bch.encode(m_tx[P25_DUID_LDU1]);
    m_tx[P25_DUID_LDU1][7U] |= 0x01U;                           // Set the parity bit

    m_tx[P25_DUID_PDU] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_tx[P25_DUID_PDU][0U] = (nac >> 4) & 0xFFU;
    m_tx[P25_DUID_PDU][1U] = (nac << 4) & 0xF0U;
    m_tx[P25_DUID_PDU][1U] |= P25_DUID_PDU;
    bch.encode(m_tx[P25_DUID_PDU]);
    m_tx[P25_DUID_PDU][7U] &= 0xFEU;                            // Clear the parity bit

    m_tx[P25_DUID_TSDU] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_tx[P25_DUID_TSDU][0U] = (nac >> 4) & 0xFFU;
    m_tx[P25_DUID_TSDU][1U] = (nac << 4) & 0xF0U;
    m_tx[P25_DUID_TSDU][1U] |= P25_DUID_TSDU;
    bch.encode(m_tx[P25_DUID_TSDU]);
    m_tx[P25_DUID_TSDU][7U] &= 0xFEU;                           // Clear the parity bit

    m_tx[P25_DUID_LDU2] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_tx[P25_DUID_LDU2][0U] = (nac >> 4) & 0xFFU;
    m_tx[P25_DUID_LDU2][1U] = (nac << 4) & 0xF0U;
    m_tx[P25_DUID_LDU2][1U] |= P25_DUID_LDU2;
    bch.encode(m_tx[P25_DUID_LDU2]);
    m_tx[P25_DUID_LDU2][7U] |= 0x01U;                           // Set the parity bit

    m_tx[P25_DUID_TDULC] = new uint8_t[P25_NID_LENGTH_BYTES];
    m_tx[P25_DUID_TDULC][0U] = (nac >> 4) & 0xFFU;
    m_tx[P25_DUID_TDULC][1U] = (nac << 4) & 0xF0U;
    m_tx[P25_DUID_TDULC][1U] |= P25_DUID_TDULC;
    bch.encode(m_tx[P25_DUID_TDULC]);
    m_tx[P25_DUID_TDULC][7U] &= 0xFEU;                          // Clear the parity bit
}
