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
*   Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; version 2 of the License.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*/
#include "Defines.h"
#include "dmr/DMRDefines.h"
#include "dmr/data/Data.h"
#include "Utils.h"

using namespace dmr::data;
using namespace dmr;

#include <cstdio>
#include <cstring>
#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the Data class.
/// </summary>
/// <param name="data"></param>
Data::Data(const Data& data) :
    m_slotNo(data.m_slotNo),
    m_srcId(data.m_srcId),
    m_dstId(data.m_dstId),
    m_flco(data.m_flco),
    m_n(data.m_n),
    m_seqNo(data.m_seqNo),
    m_dataType(data.m_dataType),
    m_ber(data.m_ber),
    m_rssi(data.m_rssi),
    m_data(NULL)
{
    m_data = new uint8_t[2U * DMR_FRAME_LENGTH_BYTES];
    ::memcpy(m_data, data.m_data, 2U * DMR_FRAME_LENGTH_BYTES);
}

/// <summary>
/// Initializes a new instance of the Data class.
/// </summary>
Data::Data() :
    m_slotNo(1U),
    m_srcId(0U),
    m_dstId(0U),
    m_flco(FLCO_GROUP),
    m_n(0U),
    m_seqNo(0U),
    m_dataType(0U),
    m_ber(0U),
    m_rssi(0U),
    m_data(NULL)
{
    m_data = new uint8_t[2U * DMR_FRAME_LENGTH_BYTES];
}

/// <summary>
/// Finalizes a instance of the Data class.
/// </summary>
Data::~Data()
{
    delete[] m_data;
}

/// <summary>
/// Equals operator.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
Data& Data::operator=(const Data& data)
{
    if (this != &data) {
        ::memcpy(m_data, data.m_data, DMR_FRAME_LENGTH_BYTES);

        m_slotNo = data.m_slotNo;
        m_srcId = data.m_srcId;
        m_dstId = data.m_dstId;
        m_flco = data.m_flco;
        m_dataType = data.m_dataType;
        m_seqNo = data.m_seqNo;
        m_n = data.m_n;
        m_ber = data.m_ber;
        m_rssi = data.m_rssi;
    }

    return *this;
}

/// <summary>
/// Sets raw data.
/// </summary>
/// <param name="buffer">Data buffer.</param>
void Data::setData(const uint8_t* buffer)
{
    assert(buffer != NULL);

    ::memcpy(m_data, buffer, DMR_FRAME_LENGTH_BYTES);
}

/// <summary>
/// Gets raw data.
/// </summary>
/// <param name="buffer">Data buffer.</param>
uint32_t Data::getData(uint8_t* buffer) const
{
    assert(buffer != NULL);

    ::memcpy(buffer, m_data, DMR_FRAME_LENGTH_BYTES);

    return DMR_FRAME_LENGTH_BYTES;
}
