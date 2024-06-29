// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "dmr/DMRDefines.h"
#include "dmr/data/Data.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::data;

#include <cstring>
#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the Data class. */
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
    m_data(nullptr)
{
    m_data = new uint8_t[2U * DMR_FRAME_LENGTH_BYTES];
    ::memcpy(m_data, data.m_data, 2U * DMR_FRAME_LENGTH_BYTES);
}

/* Initializes a new instance of the Data class. */
Data::Data() :
    m_slotNo(1U),
    m_srcId(0U),
    m_dstId(0U),
    m_flco(FLCO::GROUP),
    m_n(0U),
    m_seqNo(0U),
    m_dataType(DataType::IDLE),
    m_ber(0U),
    m_rssi(0U),
    m_data(nullptr)
{
    m_data = new uint8_t[2U * DMR_FRAME_LENGTH_BYTES];
}

/* Finalizes a instance of the Data class. */
Data::~Data()
{
    delete[] m_data;
}

/* Equals operator. */
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

/* Sets raw data. */
void Data::setData(const uint8_t* buffer)
{
    assert(buffer != nullptr);

    ::memcpy(m_data, buffer, DMR_FRAME_LENGTH_BYTES);
}

/* Gets raw data. */
uint32_t Data::getData(uint8_t* buffer) const
{
    assert(buffer != nullptr);

    ::memcpy(buffer, m_data, DMR_FRAME_LENGTH_BYTES);

    return DMR_FRAME_LENGTH_BYTES;
}
