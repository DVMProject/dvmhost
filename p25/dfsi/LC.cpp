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
#include "p25/P25Defines.h"
#include "p25/dfsi/DFSIDefines.h"
#include "p25/dfsi/LC.h"
#include "p25/P25Utils.h"
#include "Log.h"
#include "Utils.h"

using namespace p25::dfsi;
using namespace p25;

#include <cstdio>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the LC class.
/// </summary>
/// <param name="siteData"></param>
LC::LC() :
    m_rtModeFlag(P25_DFSI_RT_ENABLED),
    m_startStopFlag(P25_DFSI_START_FLAG),
    m_typeFlag(P25_DFSI_TYPE_VOICE),
    m_icwFlag(P25_DFSI_DEF_ICW_SOURCE),
    m_rssi(0U),
    m_source(P25_DFSI_DEF_SOURCE),
    m_control(),
    m_tsbk(),
    m_lsd()
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of LC class.
/// </summary>
LC::~LC()
{
    /* stub */
}

/// <summary>
/// Equals operator.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
LC& LC::operator=(const LC& data)
{
    if (this != &data) {
        m_frameType = data.m_frameType;
        m_rtModeFlag = data.m_rtModeFlag;
        m_startStopFlag = data.m_startStopFlag;
        m_typeFlag = data.m_typeFlag;
        m_icwFlag = data.m_icwFlag;

        m_rssi = data.m_rssi;
        m_source = data.m_source;

        m_control = data.m_control;
        m_tsbk = data.m_tsbk;
        m_lsd = data.m_lsd;
    }

    return *this;
}

/// <summary>
/// Decode a NID start/stop.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if decoded, otherwise false.</returns>
bool LC::decodeNID(const uint8_t* data)
{
    assert(data != NULL);

    m_frameType = data[0U];                                                         // Frame Type

    return decodeStart(data + 1U);
}

/// <summary>
/// Encode a NID start/stop.
/// </summary>
/// <param name="data"></param>
void LC::encodeNID(uint8_t* data)
{
    assert(data != NULL);

    uint8_t rawFrame[P25_DFSI_SS_FRAME_LENGTH_BYTES];
    ::memset(rawFrame, 0x00U, P25_DFSI_SS_FRAME_LENGTH_BYTES);

    rawFrame[0U] = m_frameType;                                                     // Frame Type

    // encode start record
    encodeStart(rawFrame + 1U);

    ::memcpy(data, rawFrame, P25_DFSI_SS_FRAME_LENGTH_BYTES);
}

/// <summary>
/// Decode a voice header 1.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if decoded, otherwise false.</returns>
bool LC::decodeVHDR1(const uint8_t* data)
{
    assert(data != NULL);

    m_frameType = data[0U];                                                         // Frame Type
    if (!decodeStart(data + 1U)) {
        LogError(LOG_P25, "LC::decodeVHDR1(), failed to decode start record");
        return false;
    }

    m_icwFlag = data[5U];                                                           // ICW Flag
    m_rssi = data[6U];                                                              // RSSI

    return true;
}

/// <summary>
/// Encode a voice header 1.
/// </summary>
/// <param name="data"></param>
void LC::encodeVHDR1(uint8_t* data)
{
    assert(data != NULL);

    uint8_t rawFrame[P25_DFSI_VHDR1_FRAME_LENGTH_BYTES];
    ::memset(rawFrame, 0x00U, P25_DFSI_VHDR1_FRAME_LENGTH_BYTES);

    rawFrame[0U] = m_frameType;                                                     // Frame Type

    // encode start record
    encodeStart(rawFrame + 1U);

    rawFrame[5U] = m_icwFlag;                                                       // ICW Flag
    rawFrame[6U] = m_rssi;                                                          // RSSI

    ::memcpy(data, rawFrame, P25_DFSI_VHDR1_FRAME_LENGTH_BYTES);
}

/// <summary>
/// Decode a voice header 2.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if decoded, otherwise false.</returns>
bool LC::decodeVHDR2(const uint8_t* data)
{
    assert(data != NULL);
    m_control = lc::LC();

    m_frameType = data[0U];                                                         // Frame Type

    uint32_t dstId = (data[1U] << 16) | (data[2U] << 8) | (data[3U] << 0);
    m_control.setDstId(dstId);                                                      // Talkgroup Address

    return true;
}

/// <summary>
/// Encode a voice header 2.
/// </summary>
/// <param name="data"></param>
void LC::encodeVHDR2(uint8_t* data)
{
    assert(data != NULL);

    uint8_t rawFrame[P25_DFSI_VHDR2_FRAME_LENGTH_BYTES];
    ::memset(rawFrame, 0x00U, P25_DFSI_VHDR2_FRAME_LENGTH_BYTES);

    rawFrame[0U] = m_frameType;                                                     // Frame Type

    rawFrame[1U] = (m_control.getDstId() >> 16) & 0xFFU;                            // Talkgroup Address
    rawFrame[2U] = (m_control.getDstId() >> 8) & 0xFFU;
    rawFrame[3U] = (m_control.getDstId() >> 0) & 0xFFU;

    ::memcpy(data, rawFrame, P25_DFSI_VHDR2_FRAME_LENGTH_BYTES);
}

/// <summary>
/// Decode a logical link data unit 1.
/// </summary>
/// <param name="data"></param>
/// <param name="imbe"></param>
/// <returns>True, if decoded, otherwise false.</returns>
bool LC::decodeLDU1(const uint8_t* data, uint8_t* imbe)
{
    // TODO TODO TODO
    return true;
}

/// <summary>
/// Encode a logical link data unit 1.
/// </summary>
/// <param name="data"></param>
/// <param name="imbe"></param>
void LC::encodeLDU1(uint8_t* data, const uint8_t* imbe)
{
    assert(data != NULL);

    // determine the LDU1 DFSI frame length, its variable
    uint32_t frameLength = P25_DFSI_LDU1_VOICE1_FRAME_LENGTH_BYTES;
    switch (m_frameType)
    {
        case P25_DFSI_LDU1_VOICE2:
            frameLength = P25_DFSI_LDU1_VOICE2_FRAME_LENGTH_BYTES;
            break;
        case P25_DFSI_LDU1_VOICE3:
            frameLength = P25_DFSI_LDU1_VOICE3_FRAME_LENGTH_BYTES;
            break;
        case P25_DFSI_LDU1_VOICE4:
            frameLength = P25_DFSI_LDU1_VOICE4_FRAME_LENGTH_BYTES;
            break;
        case P25_DFSI_LDU1_VOICE5:
            frameLength = P25_DFSI_LDU1_VOICE5_FRAME_LENGTH_BYTES;
            break;
        case P25_DFSI_LDU1_VOICE6:
            frameLength = P25_DFSI_LDU1_VOICE6_FRAME_LENGTH_BYTES;
            break;
        case P25_DFSI_LDU1_VOICE7:
            frameLength = P25_DFSI_LDU1_VOICE7_FRAME_LENGTH_BYTES;
            break;
        case P25_DFSI_LDU1_VOICE8:
            frameLength = P25_DFSI_LDU1_VOICE8_FRAME_LENGTH_BYTES;
            break;
        case P25_DFSI_LDU1_VOICE9:
            frameLength = P25_DFSI_LDU1_VOICE9_FRAME_LENGTH_BYTES;
            break;

        case P25_DFSI_LDU1_VOICE1:
        default:
            frameLength = P25_DFSI_LDU1_VOICE1_FRAME_LENGTH_BYTES;
            break;
    }

    uint8_t rawFrame[frameLength];
    ::memset(rawFrame, 0x00U, frameLength);

    rawFrame[0U] = m_frameType;                                                     // Frame Type

    // different frame types mean different things
    switch (m_frameType)
    {
        case P25_DFSI_LDU1_VOICE2:
            // TODO TODO TODO
            break;
        case P25_DFSI_LDU1_VOICE3:
            // TODO TODO TODO
            break;
        case P25_DFSI_LDU1_VOICE4:
            // TODO TODO TODO
            break;
        case P25_DFSI_LDU1_VOICE5:
            // TODO TODO TODO
            break;
        case P25_DFSI_LDU1_VOICE6:
            // TODO TODO TODO
            break;
        case P25_DFSI_LDU1_VOICE7:
            // TODO TODO TODO
            break;
        case P25_DFSI_LDU1_VOICE8:
            // TODO TODO TODO
            break;
        case P25_DFSI_LDU1_VOICE9:
            // TODO TODO TODO
            break;

        case P25_DFSI_LDU1_VOICE1:
        default:
            {
                // encode start record
                encodeStart(rawFrame + 1U);

                rawFrame[5U] = m_icwFlag;                                           // ICW Flag
                rawFrame[6U] = m_rssi;                                              // RSSI

                // TODO TODO TODO
            }
            break;
    }

    ::memcpy(data, rawFrame, frameLength);
}

/// <summary>
/// Decode a logical link data unit 2.
/// </summary>
/// <param name="data"></param>
/// <param name="imbe"></param>
/// <returns>True, if decoded, otherwise false.</returns>
bool LC::decodeLDU2(const uint8_t* data, uint8_t* imbe)
{
    // TODO TODO TODO
    return true;
}

/// <summary>
/// Encode a logical link data unit 2.
/// </summary>
/// <param name="data"></param>
/// <param name="imbe"></param>
void LC::encodeLDU2(uint8_t* data, const uint8_t* imbe)
{
    assert(data != NULL);

    // determine the LDU2 DFSI frame length, its variable
    uint32_t frameLength = P25_DFSI_LDU2_VOICE10_FRAME_LENGTH_BYTES;
    switch (m_frameType)
    {
        case P25_DFSI_LDU2_VOICE11:
            frameLength = P25_DFSI_LDU2_VOICE11_FRAME_LENGTH_BYTES;
            break;
        case P25_DFSI_LDU2_VOICE12:
            frameLength = P25_DFSI_LDU2_VOICE12_FRAME_LENGTH_BYTES;
            break;
        case P25_DFSI_LDU2_VOICE13:
            frameLength = P25_DFSI_LDU2_VOICE13_FRAME_LENGTH_BYTES;
            break;
        case P25_DFSI_LDU2_VOICE14:
            frameLength = P25_DFSI_LDU2_VOICE14_FRAME_LENGTH_BYTES;
            break;
        case P25_DFSI_LDU2_VOICE15:
            frameLength = P25_DFSI_LDU2_VOICE15_FRAME_LENGTH_BYTES;
            break;
        case P25_DFSI_LDU2_VOICE16:
            frameLength = P25_DFSI_LDU2_VOICE16_FRAME_LENGTH_BYTES;
            break;
        case P25_DFSI_LDU2_VOICE17:
            frameLength = P25_DFSI_LDU2_VOICE17_FRAME_LENGTH_BYTES;
            break;
        case P25_DFSI_LDU2_VOICE18:
            frameLength = P25_DFSI_LDU2_VOICE18_FRAME_LENGTH_BYTES;
            break;

        case P25_DFSI_LDU2_VOICE10:
        default:
            frameLength = P25_DFSI_LDU2_VOICE10_FRAME_LENGTH_BYTES;
            break;
    }


    uint8_t rawFrame[frameLength];
    ::memset(rawFrame, 0x00U, frameLength);

    rawFrame[0U] = m_frameType;                                                     // Frame Type

    // different frame types mean different things
    switch (m_frameType)
    {
        case P25_DFSI_LDU2_VOICE11:
            // TODO TODO TODO
            break;
        case P25_DFSI_LDU2_VOICE12:
            // TODO TODO TODO
            break;
        case P25_DFSI_LDU2_VOICE13:
            // TODO TODO TODO
            break;
        case P25_DFSI_LDU2_VOICE14:
            // TODO TODO TODO
            break;
        case P25_DFSI_LDU2_VOICE15:
            // TODO TODO TODO
            break;
        case P25_DFSI_LDU2_VOICE16:
            // TODO TODO TODO
            break;
        case P25_DFSI_LDU2_VOICE17:
            // TODO TODO TODO
            break;
        case P25_DFSI_LDU2_VOICE18:
            // TODO TODO TODO
            break;

        case P25_DFSI_LDU2_VOICE10:
        default:
            // TODO TODO TODO
            break;
    }

    ::memcpy(data, rawFrame, frameLength);
}

/// <summary>
/// Decode a logical link data unit 2.
/// </summary>
/// <param name="data"></param>
/// <param name="tsbk"></param>
/// <returns>True, if decoded, otherwise false.</returns>
bool LC::decodeTSBK(const uint8_t* data)
{
    // TODO TODO TODO
    return true;
}

/// <summary>
/// Encode a TSBK.
/// </summary>
/// <param name="data"></param>
/// <param name="tsbk"></param>
void LC::encodeTSBK(uint8_t* data, const uint8_t* tsbk)
{
    // TODO TODO TODO
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Decode start record data.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if decoded, otherwise false.</returns>
bool LC::decodeStart(const uint8_t* data)
{
    assert(data != NULL);

    m_rtModeFlag = data[1U];                                                        // RT Mode Flag
    m_startStopFlag = data[2U];                                                     // Start/Stop Flag
    m_typeFlag = data[3U];                                                          // Type Flag

    return true;
}

/// <summary>
/// Encode start record data.
/// </summary>
/// <param name="data"></param>
void LC::encodeStart(uint8_t* data)
{
    assert(data != NULL);

    uint8_t rawFrame[P25_DFSI_START_LENGTH_BYTES];
    ::memset(rawFrame, 0x00U, P25_DFSI_START_LENGTH_BYTES);

    rawFrame[0U] = 0x02U;
    rawFrame[1U] = m_rtModeFlag;                                                         // RT Mode Flag
    rawFrame[2U] = m_startStopFlag;                                                      // Start/Stop Flag
    rawFrame[3U] = m_typeFlag;                                                           // Type flag

    ::memcpy(data, rawFrame, P25_DFSI_START_LENGTH_BYTES);
}
