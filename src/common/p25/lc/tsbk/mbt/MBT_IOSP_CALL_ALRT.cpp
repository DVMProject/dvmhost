// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "p25/lc/tsbk/mbt/MBT_IOSP_CALL_ALRT.h"

using namespace p25::lc::tsbk;
using namespace p25::lc;
using namespace p25;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the MBT_IOSP_CALL_ALRT class.
/// </summary>
MBT_IOSP_CALL_ALRT::MBT_IOSP_CALL_ALRT() : AMBT()
{
    m_lco = TSBK_IOSP_CALL_ALRT;
}

/// <summary>
/// Decode a alternate trunking signalling block.
/// </summary>
/// <param name="dataHeader"></param>
/// <param name="blocks"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool MBT_IOSP_CALL_ALRT::decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks)
{
    assert(blocks != nullptr);

    uint8_t pduUserData[P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow()];
    ::memset(pduUserData, 0x00U, P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow());

    bool ret = AMBT::decode(dataHeader, blocks, pduUserData);
    if (!ret)
        return false;

    ulong64_t tsbkValue = AMBT::toValue(dataHeader, pduUserData);

    m_netId = (uint32_t)((tsbkValue >> 44) & 0xFFFFFU);                             // Network ID
    m_sysId = (uint32_t)((tsbkValue >> 32) & 0xFFFU);                               // System ID
    m_dstId = (uint32_t)((tsbkValue >> 8) & 0xFFFFFFU);                             // Target Radio Address
    m_srcId = dataHeader.getLLId();                                                 // Source Radio Address

    return true;
}

/// <summary>
/// Encode a alternate trunking signalling block.
/// </summary>
/// <param name="dataHeader"></param>
/// <param name="pduUserData"></param>
void MBT_IOSP_CALL_ALRT::encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData)
{
    assert(pduUserData != nullptr);

    /* stub */

    return;
}

/// <summary>
/// Returns a string that represents the current TSBK.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string MBT_IOSP_CALL_ALRT::toString(bool isp)
{
    if (isp) return std::string("TSBK_IOSP_CALL_ALRT (Call Alert Request)");
    else return std::string("TSBK_IOSP_CALL_ALRT (Call Alert)");
}
