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
    m_rxHdu(NULL),
    m_rxTdu(NULL),
    m_rxLdu1(NULL),
    m_rxPdu(NULL),
    m_rxTsdu(NULL),
    m_rxLdu2(NULL),
    m_rxTdulc(NULL),
    m_splitNac(false),
    m_txHdu(NULL),
    m_txTdu(NULL),
    m_txLdu1(NULL),
    m_txPdu(NULL),
    m_txTsdu(NULL),
    m_txLdu2(NULL),
    m_txTdulc(NULL)
{
    createRxNID(nac);
}

/// <summary>
/// Finalizes a instance of the NID class.
/// </summary>
NID::~NID()
{
    delete[] m_rxHdu;
    delete[] m_rxTdu;
    delete[] m_rxLdu1;
    delete[] m_rxPdu;
    delete[] m_rxTsdu;
    delete[] m_rxLdu2;
    delete[] m_rxTdulc;

    if (m_splitNac) {
        delete[] m_txHdu;
        delete[] m_txTdu;
        delete[] m_txLdu1;
        delete[] m_txPdu;
        delete[] m_txTsdu;
        delete[] m_txLdu2;
        delete[] m_txTdulc;
    }
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

    uint32_t errs = P25Utils::compare(nid, m_rxLdu1, P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = P25_DUID_LDU1;
        return true;
    }

    errs = P25Utils::compare(nid, m_rxLdu2, P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = P25_DUID_LDU2;
        return true;
    }

    errs = P25Utils::compare(nid, m_rxTdu, P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = P25_DUID_TDU;
        return true;
    }

    errs = P25Utils::compare(nid, m_rxTdulc, P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = P25_DUID_TDULC;
        return true;
    }

    errs = P25Utils::compare(nid, m_rxPdu, P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = P25_DUID_PDU;
        return true;
    }

    errs = P25Utils::compare(nid, m_rxTsdu, P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = P25_DUID_TSDU;
        return true;
    }

    errs = P25Utils::compare(nid, m_rxHdu, P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = P25_DUID_HDU;
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
                P25Utils::encode(m_txHdu, data, 48U, 114U);
                break;
            case P25_DUID_TDU:
                P25Utils::encode(m_txTdu, data, 48U, 114U);
                break;
            case P25_DUID_LDU1:
                P25Utils::encode(m_txLdu1, data, 48U, 114U);
                break;
            case P25_DUID_PDU:
                P25Utils::encode(m_txPdu, data, 48U, 114U);
                break;
            case P25_DUID_TSDU:
                P25Utils::encode(m_txTsdu, data, 48U, 114U);
                break;
            case P25_DUID_LDU2:
                P25Utils::encode(m_txLdu2, data, 48U, 114U);
                break;
            case P25_DUID_TDULC:
                P25Utils::encode(m_txTdulc, data, 48U, 114U);
                break;
            default:
                break;
        }
    }
    else {
        switch (duid) {
            case P25_DUID_HDU:
                P25Utils::encode(m_rxHdu, data, 48U, 114U);
                break;
            case P25_DUID_TDU:
                P25Utils::encode(m_rxTdu, data, 48U, 114U);
                break;
            case P25_DUID_LDU1:
                P25Utils::encode(m_rxLdu1, data, 48U, 114U);
                break;
            case P25_DUID_PDU:
                P25Utils::encode(m_rxPdu, data, 48U, 114U);
                break;
            case P25_DUID_TSDU:
                P25Utils::encode(m_rxTsdu, data, 48U, 114U);
                break;
            case P25_DUID_LDU2:
                P25Utils::encode(m_rxLdu2, data, 48U, 114U);
                break;
            case P25_DUID_TDULC:
                P25Utils::encode(m_rxTdulc, data, 48U, 114U);
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
void NID::createRxNID(uint32_t nac)
{
    edac::BCH bch;

    m_rxHdu = new uint8_t[P25_NID_LENGTH_BYTES];
    m_rxHdu[0U] = (nac >> 4) & 0xFFU;
    m_rxHdu[1U] = (nac << 4) & 0xF0U;
    m_rxHdu[1U] |= P25_DUID_HDU;
    bch.encode(m_rxHdu);
    m_rxHdu[7U] &= 0xFEU;      // Clear the parity bit

    m_rxTdu = new uint8_t[P25_NID_LENGTH_BYTES];
    m_rxTdu[0U] = (nac >> 4) & 0xFFU;
    m_rxTdu[1U] = (nac << 4) & 0xF0U;
    m_rxTdu[1U] |= P25_DUID_TDU;
    bch.encode(m_rxTdu);
    m_rxTdu[7U] &= 0xFEU;      // Clear the parity bit

    m_rxLdu1 = new uint8_t[P25_NID_LENGTH_BYTES];
    m_rxLdu1[0U] = (nac >> 4) & 0xFFU;
    m_rxLdu1[1U] = (nac << 4) & 0xF0U;
    m_rxLdu1[1U] |= P25_DUID_LDU1;
    bch.encode(m_rxLdu1);
    m_rxLdu1[7U] |= 0x01U;     // Set the parity bit

    m_rxPdu = new uint8_t[P25_NID_LENGTH_BYTES];
    m_rxPdu[0U] = (nac >> 4) & 0xFFU;
    m_rxPdu[1U] = (nac << 4) & 0xF0U;
    m_rxPdu[1U] |= P25_DUID_PDU;
    bch.encode(m_rxPdu);
    m_rxPdu[7U] &= 0xFEU;      // Clear the parity bit

    m_rxTsdu = new uint8_t[P25_NID_LENGTH_BYTES];
    m_rxTsdu[0U] = (nac >> 4) & 0xFFU;
    m_rxTsdu[1U] = (nac << 4) & 0xF0U;
    m_rxTsdu[1U] |= P25_DUID_TSDU;
    bch.encode(m_rxTsdu);
    m_rxTsdu[7U] &= 0xFEU;     // Clear the parity bit

    m_rxLdu2 = new uint8_t[P25_NID_LENGTH_BYTES];
    m_rxLdu2[0U] = (nac >> 4) & 0xFFU;
    m_rxLdu2[1U] = (nac << 4) & 0xF0U;
    m_rxLdu2[1U] |= P25_DUID_LDU2;
    bch.encode(m_rxLdu2);
    m_rxLdu2[7U] |= 0x01U;     // Set the parity bit

    m_rxTdulc = new uint8_t[P25_NID_LENGTH_BYTES];
    m_rxTdulc[0U] = (nac >> 4) & 0xFFU;
    m_rxTdulc[1U] = (nac << 4) & 0xF0U;
    m_rxTdulc[1U] |= P25_DUID_TDULC;
    bch.encode(m_rxTdulc);
    m_rxTdulc[7U] &= 0xFEU;    // Clear the parity bit
}

/// <summary>
///
/// </summary>
/// <param name="nac"></param>
void NID::createTxNID(uint32_t nac)
{
    edac::BCH bch;

    m_txHdu = new uint8_t[P25_NID_LENGTH_BYTES];
    m_txHdu[0U] = (nac >> 4) & 0xFFU;
    m_txHdu[1U] = (nac << 4) & 0xF0U;
    m_txHdu[1U] |= P25_DUID_HDU;
    bch.encode(m_txHdu);
    m_txHdu[7U] &= 0xFEU;      // Clear the parity bit

    m_txTdu = new uint8_t[P25_NID_LENGTH_BYTES];
    m_txTdu[0U] = (nac >> 4) & 0xFFU;
    m_txTdu[1U] = (nac << 4) & 0xF0U;
    m_txTdu[1U] |= P25_DUID_TDU;
    bch.encode(m_txTdu);
    m_txTdu[7U] &= 0xFEU;      // Clear the parity bit

    m_txLdu1 = new uint8_t[P25_NID_LENGTH_BYTES];
    m_txLdu1[0U] = (nac >> 4) & 0xFFU;
    m_txLdu1[1U] = (nac << 4) & 0xF0U;
    m_txLdu1[1U] |= P25_DUID_LDU1;
    bch.encode(m_txLdu1);
    m_txLdu1[7U] |= 0x01U;     // Set the parity bit

    m_txPdu = new uint8_t[P25_NID_LENGTH_BYTES];
    m_txPdu[0U] = (nac >> 4) & 0xFFU;
    m_txPdu[1U] = (nac << 4) & 0xF0U;
    m_txPdu[1U] |= P25_DUID_PDU;
    bch.encode(m_txPdu);
    m_txPdu[7U] &= 0xFEU;      // Clear the parity bit

    m_txTsdu = new uint8_t[P25_NID_LENGTH_BYTES];
    m_txTsdu[0U] = (nac >> 4) & 0xFFU;
    m_txTsdu[1U] = (nac << 4) & 0xF0U;
    m_txTsdu[1U] |= P25_DUID_TSDU;
    bch.encode(m_txTsdu);
    m_txTsdu[7U] &= 0xFEU;     // Clear the parity bit

    m_txLdu2 = new uint8_t[P25_NID_LENGTH_BYTES];
    m_txLdu2[0U] = (nac >> 4) & 0xFFU;
    m_txLdu2[1U] = (nac << 4) & 0xF0U;
    m_txLdu2[1U] |= P25_DUID_LDU2;
    bch.encode(m_txLdu2);
    m_txLdu2[7U] |= 0x01U;     // Set the parity bit

    m_txTdulc = new uint8_t[P25_NID_LENGTH_BYTES];
    m_txTdulc[0U] = (nac >> 4) & 0xFFU;
    m_txTdulc[1U] = (nac << 4) & 0xF0U;
    m_txTdulc[1U] |= P25_DUID_TDULC;
    bch.encode(m_txTdulc);
    m_txTdulc[7U] &= 0xFEU;    // Clear the parity bit
}
