// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "analog/AnalogAudio.h"
#include "analog/data/NetData.h"

using namespace analog;
using namespace analog::defines;
using namespace analog::data;

#include <cstring>
#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the NetData class. */

NetData::NetData(const NetData& data) :
    m_srcId(data.m_srcId),
    m_dstId(data.m_dstId),
    m_control(data.m_control),
    m_seqNo(data.m_seqNo),
    m_group(true),
    m_frameType(data.m_frameType),
    m_audio(nullptr)
{
    m_audio = new uint8_t[2U * AUDIO_SAMPLES_LENGTH_BYTES];
    ::memcpy(m_audio, data.m_audio, 2U * AUDIO_SAMPLES_LENGTH_BYTES);
}

/* Initializes a new instance of the NetData class. */

NetData::NetData() :
    m_srcId(0U),
    m_dstId(0U),
    m_control(0U),
    m_seqNo(0U),
    m_group(true),
    m_frameType(AudioFrameType::TERMINATOR),
    m_audio(nullptr)
{
    m_audio = new uint8_t[2U * AUDIO_SAMPLES_LENGTH_BYTES];
}

/* Finalizes a instance of the NetData class. */

NetData::~NetData()
{
    delete[] m_audio;
}

/* Equals operator. */

NetData& NetData::operator=(const NetData& data)
{
    if (this != &data) {
        ::memcpy(m_audio, data.m_audio, 2U * AUDIO_SAMPLES_LENGTH_BYTES);

        m_srcId = data.m_srcId;
        m_dstId = data.m_dstId;
        m_control = data.m_control;
        m_frameType = data.m_frameType;
        m_seqNo = data.m_seqNo;
        m_group = data.m_group;
    }

    return *this;
}

/* Sets audio data. */

void NetData::setAudio(const uint8_t* buffer)
{
    assert(buffer != nullptr);

    ::memcpy(m_audio, buffer, AUDIO_SAMPLES_LENGTH_BYTES);
}

/* Gets audio data. */

uint32_t NetData::getAudio(uint8_t* buffer) const
{
    assert(buffer != nullptr);

    ::memcpy(buffer, m_audio, AUDIO_SAMPLES_LENGTH_BYTES);

    return AUDIO_SAMPLES_LENGTH_BYTES;
}
