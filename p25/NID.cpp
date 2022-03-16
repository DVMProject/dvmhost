/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2016 by Jonathan Naylor G4KLX
*   Copyright (C) 2017,2022 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/NID.h"
#include "p25/P25Utils.h"
#include "edac/BCH.h"

using namespace p25;

#include <cstdio>
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
    m_rxTx(NULL),
    m_tx(NULL),
    m_splitNac(false)
{
    m_rxTx = new uint8_t*[16U];
    for (uint8_t i = 0; i < 16U; i++)
        m_rxTx[i] = NULL;

    m_tx = new uint8_t*[16U];
    for (uint8_t i = 0; i < 16U; i++)
        m_tx[i] = NULL;

    createRxTxNID(nac);
}

/// <summary>
/// Finalizes a instance of the NID class.
/// </summary>
NID::~NID()
{
    for (uint8_t i = 0; i < 16U; i++)
    {
        if (m_rxTx[i] != NULL) {
            delete[] m_rxTx[i];
        }

        if (m_tx[i] != NULL) {
            delete[] m_tx[i];
        }
    }

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
    assert(data != NULL);

    uint8_t nid[P25_NID_LENGTH_BYTES];
    P25Utils::decode(data, nid, 48U, 114U);

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
void NID::encode(uint8_t* data, uint8_t duid) const
{
    assert(data != NULL);

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
