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
*   Copyright (C) 2012 by Ian Wraith
*   Copyright (C) 2015,2016,2017 by Jonathan Naylor G4KLX
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
#include "dmr/DMRDefines.h"
#include "dmr/data/DataHeader.h"
#include "edac/BPTC19696.h"
#include "edac/RS129.h"
#include "edac/CRC.h"
#include "Utils.h"

using namespace dmr::data;
using namespace dmr;

#include <cstdio>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint8_t UDTF_NMEA = 0x05U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the DataHeader class.
/// </summary>
DataHeader::DataHeader() :
    m_GI(false),
    m_srcId(0U),
    m_dstId(0U),
    m_blocks(0U),
    m_data(NULL),
    m_A(false),
    m_F(false),
    m_S(false),
    m_Ns(0U)
{
    m_data = new uint8_t[12U];
}

/// <summary>
/// Finalizes a instance of the DataHeader class.
/// </summary>
DataHeader::~DataHeader()
{
    delete[] m_data;
}

/// <summary>
/// Equals operator.
/// </summary>
/// <param name="header"></param>
/// <returns></returns>
DataHeader& DataHeader::operator=(const DataHeader& header)
{
    if (&header != this) {
        ::memcpy(m_data, header.m_data, 12U);
        m_GI = header.m_GI;
        m_A = header.m_A;
        m_srcId = header.m_srcId;
        m_dstId = header.m_dstId;
        m_blocks = header.m_blocks;
        m_F = header.m_F;
        m_S = header.m_S;
        m_Ns = header.m_Ns;
    }

    return *this;
}

/// <summary>
/// Decodes a DMR data header.
/// </summary>
/// <param name="bytes"></param>
/// <returns>True, if DMR data header was decoded, otherwise false.</returns>
bool DataHeader::decode(const uint8_t* bytes)
{
    assert(bytes != NULL);

    // decode BPTC (196,96) FEC
    edac::BPTC19696 bptc;
    bptc.decode(bytes, m_data);

    // validate the CRC-CCITT 16
    m_data[10U] ^= DATA_HEADER_CRC_MASK[0U];
    m_data[11U] ^= DATA_HEADER_CRC_MASK[1U];

    bool valid = edac::CRC::checkCCITT162(m_data, DMR_LC_HEADER_LENGTH_BYTES);
    if (!valid)
        return false;

    // restore the checksum
    m_data[10U] ^= DATA_HEADER_CRC_MASK[0U];
    m_data[11U] ^= DATA_HEADER_CRC_MASK[1U];

    m_GI = (m_data[0U] & 0x80U) == 0x80U;
    m_A = (m_data[0U] & 0x40U) == 0x40U;

    uint8_t dpf = m_data[0U] & 0x0FU;
    if (dpf == DPF_PROPRIETARY)
        return true;

    m_dstId = m_data[2U] << 16 | m_data[3U] << 8 | m_data[4U];
    m_srcId = m_data[5U] << 16 | m_data[6U] << 8 | m_data[7U];

    switch (dpf) {
        case DPF_UNCONFIRMED_DATA:
            Utils::dump(1U, "DMR, Unconfirmed Data Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
            m_F = (m_data[8U] & 0x80U) == 0x80U;
            m_blocks = m_data[8U] & 0x7FU;
            break;

        case DPF_CONFIRMED_DATA:
            Utils::dump(1U, "DMR, Confirmed Data Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
            m_F = (m_data[8U] & 0x80U) == 0x80U;
            m_blocks = m_data[8U] & 0x7FU;
            m_S = (m_data[9U] & 0x80U) == 0x80U;
            m_Ns = (m_data[9U] >> 4) & 0x07U;
            break;

        case DPF_RESPONSE:
            Utils::dump(1U, "DMR, Response Data Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
            m_blocks = m_data[8U] & 0x7FU;
            break;

        case DPF_PROPRIETARY:
            Utils::dump(1U, "DMR, Proprietary Data Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
            break;

        case DPF_DEFINED_RAW:
            Utils::dump(1U, "DMR, Raw or Status/Precoded Short Data Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
            m_blocks = (m_data[0U] & 0x30U) + (m_data[1U] & 0x0FU);
            m_F = (m_data[8U] & 0x01U) == 0x01U;
            m_S = (m_data[8U] & 0x02U) == 0x02U;
            break;

        case DPF_DEFINED_SHORT:
            Utils::dump(1U, "DMR, Defined Short Data Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
            m_blocks = (m_data[0U] & 0x30U) + (m_data[1U] & 0x0FU);
            m_F = (m_data[8U] & 0x01U) == 0x01U;
            m_S = (m_data[8U] & 0x02U) == 0x02U;
            break;

        case DPF_UDT:
            Utils::dump(1U, "DMR, Unified Data Transport Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
            m_blocks = (m_data[8U] & 0x03U) + 1U;
            break;

        default:
            Utils::dump("DMR, Unknown Data Header", m_data, DMR_LC_HEADER_LENGTH_BYTES);
            break;
    }

    return true;
}

/// <summary>
/// Encodes a DMR data header.
/// </summary>
/// <param name="bytes"></param>
void DataHeader::encode(uint8_t* bytes) const
{
    assert(bytes != NULL);

    // encode BPTC (196,96) FEC
    edac::BPTC19696 bptc;
    bptc.encode(m_data, bytes);
}
