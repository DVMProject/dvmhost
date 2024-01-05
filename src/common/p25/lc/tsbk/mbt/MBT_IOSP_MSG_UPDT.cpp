/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
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
#include "p25/lc/tsbk/mbt/MBT_IOSP_MSG_UPDT.h"
#include "Log.h"
#include "Utils.h"

using namespace p25::lc::tsbk;
using namespace p25::lc;
using namespace p25;

#include <cassert>
#include <cmath>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the MBT_IOSP_MSG_UPDT class.
/// </summary>
MBT_IOSP_MSG_UPDT::MBT_IOSP_MSG_UPDT() : AMBT(),
    m_messageValue(0U)
{
    m_lco = TSBK_IOSP_MSG_UPDT;
}

/// <summary>
/// Decode a alternate trunking signalling block.
/// </summary>
/// <param name="dataHeader"></param>
/// <param name="blocks"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool MBT_IOSP_MSG_UPDT::decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks)
{
    assert(blocks != NULL);

    uint8_t pduUserData[P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow()];
    ::memset(pduUserData, 0x00U, P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow());

    bool ret = AMBT::decode(dataHeader, blocks, pduUserData);
    if (!ret)
        return false;

    ulong64_t tsbkValue = AMBT::toValue(dataHeader, pduUserData);

    m_messageValue  = (uint8_t)((tsbkValue >> 48) & 0xFFFFU);                       // Message Value
    m_netId = (uint32_t)((tsbkValue >> 28) & 0xFFFFFU);                             // Network ID
    m_sysId = (uint32_t)((tsbkValue >> 16) & 0xFFFU);                               // System ID
    m_dstId = (uint32_t)(((tsbkValue) & 0xFFFFU) << 8) + pduUserData[6U];           // Target Radio Address
    m_srcId = dataHeader.getLLId();                                                 // Source Radio Address

    return true;
}

/// <summary>
/// Encode a alternate trunking signalling block.
/// </summary>
/// <param name="dataHeader"></param>
/// <param name="pduUserData"></param>
void MBT_IOSP_MSG_UPDT::encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData)
{
    assert(pduUserData != NULL);

    /* stub */

    return;
}

/// <summary>
/// Returns a string that represents the current TSBK.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string MBT_IOSP_MSG_UPDT::toString(bool isp)
{
    if (isp)
        return std::string("TSBK_IOSP_MSG_UPDT (Message Update Request)");
    else    
        return std::string("TSBK_IOSP_MSG_UPDT (Message Update)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void MBT_IOSP_MSG_UPDT::copy(const MBT_IOSP_MSG_UPDT& data)
{
    TSBK::copy(data);

    m_messageValue = data.m_messageValue;
}
