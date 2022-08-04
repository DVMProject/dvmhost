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
*   Copyright (C) 2018 by Jonathan Naylor G4KLX
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
#include "nxdn/NXDNDefines.h"
#include "nxdn/data/Layer3.h"
#include "Utils.h"

using namespace nxdn;
using namespace nxdn::data;

#include <cstdio>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a copy instance of the Layer3 class.
/// </summary>
/// <param name="data"></param>
Layer3::Layer3(const Layer3& data) :
    m_verbose(false),
    m_messageType(MESSAGE_TYPE_IDLE),
    m_srcId(0U),
    m_dstId(0U),
    m_group(true),
    m_dataBlocks(0U),
    m_data(NULL)
{
    m_data = new uint8_t[22U];
    ::memcpy(m_data, data.m_data, 22U);

    m_messageType = m_data[0U] & 0x3FU;                                             // Message Type

    m_group = (m_data[2U] & 0x80U) != 0x80U;

    m_srcId = (m_data[3U] << 8) | m_data[4U];                                       // Source Radio Address
    m_dstId = (m_data[5U] << 8) | m_data[6U];                                       // Target Radio Address

    m_dataBlocks = (m_data[8U] & 0x0FU) + 1U;                                       // Data Blocks
}

/// <summary>
/// Initializes a new instance of the Layer3 class.
/// </summary>
Layer3::Layer3() :
    m_verbose(false),
    m_messageType(MESSAGE_TYPE_IDLE),
    m_srcId(0U),
    m_dstId(0U),
    m_group(true),
    m_dataBlocks(0U),
    m_data(NULL)
{
    m_data = new uint8_t[22U];
    ::memset(m_data, 0x00U, 22U);
}

/// <summary>
/// Finalizes a instance of Layer3 class.
/// </summary>
Layer3::~Layer3()
{
    delete[] m_data;
}

/// <summary>
/// Equals operator.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
Layer3& Layer3::operator=(const Layer3& data)
{
    if (&data != this) {
        ::memcpy(m_data, data.m_data, 22U);

        m_verbose = data.m_verbose;

        m_messageType = data.m_messageType;
       
        m_group = data.m_group;

        m_srcId = data.m_srcId;
        m_dstId = data.m_dstId;

        m_dataBlocks = m_dataBlocks;
    }

    return *this;
}

/// <summary>
/// Decode layer 3 data.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if SACCH was decoded, otherwise false.</returns>
void Layer3::decode(const uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != NULL);

    for (uint32_t i = 0U; i < length; i++, offset++) {
        bool b = READ_BIT(data, i);
        WRITE_BIT(m_data, offset, b);
    }

    if (m_verbose) {
        Utils::dump(2U, "Decoded Layer 3 Data", m_data, 22U);
    }

    m_messageType = m_data[0U] & 0x3FU;                                             // Message Type

    m_group = (m_data[2U] & 0x80U) != 0x80U;

    m_srcId = (m_data[3U] << 8) | m_data[4U];                                       // Source Radio Address
    m_dstId = (m_data[5U] << 8) | m_data[6U];                                       // Target Radio Address

    m_dataBlocks = (m_data[8U] & 0x0FU) + 1U;                                       // Data Blocks
}

/// <summary>
/// Encode layer 3 data.
/// </summary>
/// <param name="data"></param>
void Layer3::encode(uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != NULL);

    m_data[0U] = m_messageType & 0x3FU;                                             // Message Type

    m_data[2U] = (m_group ? 0x80U : 0x00U);

    m_data[3U] = (m_srcId >> 8U) & 0xFFU;                                           // Source Radio Address
    m_data[4U] = (m_srcId >> 0U) & 0xFFU;                                           // ...
    m_data[5U] = (m_dstId >> 8U) & 0xFFU;                                           // Target Radio Address
    m_data[6U] = (m_dstId >> 0U) & 0xFFU;                                           // ...

    m_data[8U] = m_dataBlocks & 0x0FU;                                              // Data Blocks

    for (uint32_t i = 0U; i < length; i++, offset++) {
        bool b = READ_BIT(m_data, offset);
        WRITE_BIT(data, i, b);
    }

    if (m_verbose) {
        Utils::dump(2U, "Encoded Layer 3 Data", data, length);
    }
}

/// <summary>
///
/// </summary>
void Layer3::reset()
{
    ::memset(m_data, 0x00U, 22U);

    m_messageType = MESSAGE_TYPE_IDLE;

    m_srcId = 0U;
    m_dstId = 0U;

    m_group = true;

    m_dataBlocks = 0U;
}

/// <summary>
/// Gets the raw layer 3 data.
/// </summary>
/// <param name="data"></param>
void Layer3::getData(uint8_t* data) const
{
    ::memcpy(data, m_data, 22U);
}

/// <summary>
/// Sets the raw layer 3 data.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
void Layer3::setData(const uint8_t* data, uint32_t length)
{
    ::memset(m_data, 0x00U, 22U);
    ::memcpy(m_data, data, length);

    m_messageType = m_data[0U] & 0x3FU;

    m_group = (m_data[2U] & 0x80U) != 0x80U;

    m_srcId = (m_data[3U] << 8) | m_data[4U];
    m_dstId = (m_data[5U] << 8) | m_data[6U];

    m_dataBlocks = (m_data[8U] & 0x0FU) + 1U;
}
