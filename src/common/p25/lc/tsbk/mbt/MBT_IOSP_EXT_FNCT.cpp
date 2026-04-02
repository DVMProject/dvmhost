// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022,2024,2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/lc/tsbk/mbt/MBT_IOSP_EXT_FNCT.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MBT_IOSP_EXT_FNCT class. */

MBT_IOSP_EXT_FNCT::MBT_IOSP_EXT_FNCT() : AMBT(),
    m_extendedFunction(ExtendedFunctions::CHECK)
{
    m_lco = TSBKO::IOSP_EXT_FNCT;
}

/* Decode a alternate trunking signalling block. */

bool MBT_IOSP_EXT_FNCT::decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks)
{
    assert(blocks != nullptr);

    DECLARE_UINT8_ARRAY(pduUserData, P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow());

    bool ret = AMBT::decode(dataHeader, blocks, pduUserData);
    if (!ret)
        return false;

    ulong64_t tsbkValue = AMBT::toValue(dataHeader, pduUserData);

    // HACK: to get around the fact that Harris is encoding the P25C extended function in a non-standard way
    if (dataHeader.getMFId() == MFG_HARRIS) {
        // convert harris extended function to standard extended function
        uint16_t harrisExtendedFunction = (uint16_t)((tsbkValue >> 48) & 0xFFFFU);
        switch (harrisExtendedFunction) {
        case ExtendedFunctions::HARRIS_UNINHIBIT:
            m_extendedFunction = ExtendedFunctions::UNINHIBIT;
            break;
        case ExtendedFunctions::HARRIS_INHIBIT:
            m_extendedFunction = ExtendedFunctions::INHIBIT;
            break;
        default:
            m_extendedFunction = harrisExtendedFunction;
            break;
        }

        m_srcId = (uint32_t)((tsbkValue >> 8) & 0xFFFFFFU);                         // Argument
        m_dstId = dataHeader.getLLId();                                             // Target Radio Address

        return true;
    }

    m_netId = (uint32_t)((tsbkValue >> 44) & 0xFFFFFU);                             // Network ID
    m_sysId = (uint32_t)((tsbkValue >> 32) & 0xFFFU);                               // System ID
    m_extendedFunction = (uint32_t)((tsbkValue >> 16) & 0xFFFFU);                   // Extended Function
    m_srcId = (uint32_t)(((tsbkValue) & 0xFFFFU) << 8) +                            // Argument
        pduUserData[6U];
    m_dstId  = dataHeader.getLLId();                                                // Target Radio Address

    return true;
}

/* Encode a alternate trunking signalling block. */

void MBT_IOSP_EXT_FNCT::encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData)
{
    assert(pduUserData != nullptr);

    /* stub */

    return;
}

/* Returns a string that represents the current TSBK. */

std::string MBT_IOSP_EXT_FNCT::toString(bool isp)
{
    return (isp) ? std::string("TSBKO, IOSP_EXT_FNCT (Extended Function Response)") :
        std::string("TSBKO, IOSP_EXT_FNCT (Extended Function Command)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void MBT_IOSP_EXT_FNCT::copy(const MBT_IOSP_EXT_FNCT& data)
{
    TSBK::copy(data);

    m_extendedFunction = data.m_extendedFunction;
}
