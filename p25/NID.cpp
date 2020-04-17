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
*   Copyright (C) 2017 by Bryan Biedenkapp N2PLL
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
    m_hdu(NULL),
    m_tdu(NULL),
    m_ldu1(NULL),
    m_pdu(NULL),
    m_tsdu(NULL),
    m_ldu2(NULL),
    m_tdulc(NULL)
{
    edac::BCH bch;

    m_hdu = new uint8_t[P25_NID_LENGTH_BYTES];
    m_hdu[0U] = (nac >> 4) & 0xFFU;
    m_hdu[1U] = (nac << 4) & 0xF0U;
    m_hdu[1U] |= P25_DUID_HDU;
    bch.encode(m_hdu);
    m_hdu[7U] &= 0xFEU;      // Clear the parity bit

    m_tdu = new uint8_t[P25_NID_LENGTH_BYTES];
    m_tdu[0U] = (nac >> 4) & 0xFFU;
    m_tdu[1U] = (nac << 4) & 0xF0U;
    m_tdu[1U] |= P25_DUID_TDU;
    bch.encode(m_tdu);
    m_tdu[7U] &= 0xFEU;      // Clear the parity bit

    m_ldu1 = new uint8_t[P25_NID_LENGTH_BYTES];
    m_ldu1[0U] = (nac >> 4) & 0xFFU;
    m_ldu1[1U] = (nac << 4) & 0xF0U;
    m_ldu1[1U] |= P25_DUID_LDU1;
    bch.encode(m_ldu1);
    m_ldu1[7U] |= 0x01U;     // Set the parity bit

    m_pdu = new uint8_t[P25_NID_LENGTH_BYTES];
    m_pdu[0U] = (nac >> 4) & 0xFFU;
    m_pdu[1U] = (nac << 4) & 0xF0U;
    m_pdu[1U] |= P25_DUID_PDU;
    bch.encode(m_pdu);
    m_pdu[7U] &= 0xFEU;      // Clear the parity bit

    m_tsdu = new uint8_t[P25_NID_LENGTH_BYTES];
    m_tsdu[0U] = (nac >> 4) & 0xFFU;
    m_tsdu[1U] = (nac << 4) & 0xF0U;
    m_tsdu[1U] |= P25_DUID_TSDU;
    bch.encode(m_tsdu);
    m_tsdu[7U] &= 0xFEU;     // Clear the parity bit

    m_ldu2 = new uint8_t[P25_NID_LENGTH_BYTES];
    m_ldu2[0U] = (nac >> 4) & 0xFFU;
    m_ldu2[1U] = (nac << 4) & 0xF0U;
    m_ldu2[1U] |= P25_DUID_LDU2;
    bch.encode(m_ldu2);
    m_ldu2[7U] |= 0x01U;     // Set the parity bit

    m_tdulc = new uint8_t[P25_NID_LENGTH_BYTES];
    m_tdulc[0U] = (nac >> 4) & 0xFFU;
    m_tdulc[1U] = (nac << 4) & 0xF0U;
    m_tdulc[1U] |= P25_DUID_TDULC;
    bch.encode(m_tdulc);
    m_tdulc[7U] &= 0xFEU;    // Clear the parity bit
}

/// <summary>
/// Finalizes a instance of the NID class.
/// </summary>
NID::~NID()
{
    delete[] m_hdu;
    delete[] m_tdu;
    delete[] m_ldu1;
    delete[] m_pdu;
    delete[] m_tsdu;
    delete[] m_ldu2;
    delete[] m_tdulc;
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

    uint32_t errs = P25Utils::compare(nid, m_ldu1, P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = P25_DUID_LDU1;
        return true;
    }

    errs = P25Utils::compare(nid, m_ldu2, P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = P25_DUID_LDU2;
        return true;
    }

    errs = P25Utils::compare(nid, m_tdu, P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = P25_DUID_TDU;
        return true;
    }

    errs = P25Utils::compare(nid, m_tdulc, P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = P25_DUID_TDULC;
        return true;
    }

    errs = P25Utils::compare(nid, m_pdu, P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = P25_DUID_PDU;
        return true;
    }

    errs = P25Utils::compare(nid, m_tsdu, P25_NID_LENGTH_BYTES);
    if (errs < MAX_NID_ERRS) {
        m_duid = P25_DUID_TSDU;
        return true;
    }

    errs = P25Utils::compare(nid, m_hdu, P25_NID_LENGTH_BYTES);
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

    switch (duid) {
        case P25_DUID_HDU:
            P25Utils::encode(m_hdu, data, 48U, 114U);
            break;
        case P25_DUID_TDU:
            P25Utils::encode(m_tdu, data, 48U, 114U);
            break;
        case P25_DUID_LDU1:
            P25Utils::encode(m_ldu1, data, 48U, 114U);
            break;
        case P25_DUID_PDU:
            P25Utils::encode(m_pdu, data, 48U, 114U);
            break;
        case P25_DUID_TSDU:
            P25Utils::encode(m_tsdu, data, 48U, 114U);
            break;
        case P25_DUID_LDU2:
            P25Utils::encode(m_ldu2, data, 48U, 114U);
            break;
        case P25_DUID_TDULC:
            P25Utils::encode(m_tdulc, data, 48U, 114U);
            break;
        default:
            break;
    }
}
