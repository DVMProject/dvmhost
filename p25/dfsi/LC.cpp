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
LC::LC() :
    m_rtModeFlag(P25_DFSI_RT_ENABLED),
    m_startStopFlag(P25_DFSI_START_FLAG),
    m_typeFlag(P25_DFSI_TYPE_VOICE),
    m_icwFlag(P25_DFSI_DEF_ICW_SOURCE),
    m_rssi(0U),
    m_source(P25_DFSI_DEF_SOURCE),
    m_control(),
    m_tsbk(),
    m_lsd(),
    m_mi(NULL)
{
    m_mi = new uint8_t[P25_MI_LENGTH_BYTES];
    ::memset(m_mi, 0x00U, P25_MI_LENGTH_BYTES);
}

/// <summary>
/// Initializes a copy instance of the LC class.
/// </summary>
/// <param name="data"></param>
LC::LC(const LC& data) : LC()
{
    copy(data);
}

/// <summary>
/// Initializes a new instance of the LC class.
/// </summary>
LC::LC(const lc::LC& control, const data::LowSpeedData& lsd) : LC()
{
    m_control = lc::LC(control);
    m_lsd = data::LowSpeedData(lsd);
}

/// <summary>
/// Finalizes a instance of LC class.
/// </summary>
LC::~LC()
{
    delete[] m_mi;
}

/// <summary>
/// Equals operator.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
LC& LC::operator=(const LC& data)
{
    if (this != &data) {
        copy(data);
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

    uint8_t dfsiFrame[P25_DFSI_SS_FRAME_LENGTH_BYTES];
    ::memset(dfsiFrame, 0x00U, P25_DFSI_SS_FRAME_LENGTH_BYTES);

    dfsiFrame[0U] = m_frameType;                                                    // Frame Type

    // encode start record
    encodeStart(dfsiFrame + 1U);

#if DEBUG_P25_DFSI
    Utils::dump(2U, "LC::encodeNID(), DFSI Start/Stop Frame", dfsiFrame, P25_DFSI_SS_FRAME_LENGTH_BYTES);
#endif

    ::memcpy(data, dfsiFrame, P25_DFSI_SS_FRAME_LENGTH_BYTES);
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
    if (m_frameType != P25_DFSI_VHDR1) {
        LogError(LOG_P25, "LC::decodeVHDR1(), invalid frametype, frameType = $%02X", m_frameType);
        return false;
    }

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

    uint8_t dfsiFrame[P25_DFSI_VHDR1_FRAME_LENGTH_BYTES];
    ::memset(dfsiFrame, 0x00U, P25_DFSI_VHDR1_FRAME_LENGTH_BYTES);

    dfsiFrame[0U] = P25_DFSI_VHDR1;                                                 // Frame Type

    // encode start record
    encodeStart(dfsiFrame + 1U);

    dfsiFrame[5U] = m_icwFlag;                                                      // ICW Flag
    dfsiFrame[6U] = m_rssi;                                                         // RSSI

#if DEBUG_P25_DFSI
    Utils::dump(2U, "LC::encodeVHDR1(), DFSI Voice Header 1 Frame", dfsiFrame, P25_DFSI_VHDR1_FRAME_LENGTH_BYTES);
#endif

    ::memcpy(data, dfsiFrame, P25_DFSI_VHDR1_FRAME_LENGTH_BYTES);
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
    if (m_frameType != P25_DFSI_VHDR2) {
        LogError(LOG_P25, "LC::decodeVHDR2(), invalid frametype, frameType = $%02X", m_frameType);
        return false;
    }

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

    uint8_t dfsiFrame[P25_DFSI_VHDR2_FRAME_LENGTH_BYTES];
    ::memset(dfsiFrame, 0x00U, P25_DFSI_VHDR2_FRAME_LENGTH_BYTES);

    dfsiFrame[0U] = P25_DFSI_VHDR2;                                                 // Frame Type

    uint32_t dstId = m_control.getDstId();
    dfsiFrame[1U] = (dstId >> 16) & 0xFFU;                                          // Talkgroup Address
    dfsiFrame[2U] = (dstId >> 8) & 0xFFU;
    dfsiFrame[3U] = (dstId >> 0) & 0xFFU;

#if DEBUG_P25_DFSI
    Utils::dump(2U, "LC::encodeVHDR2(), DFSI Voice Header 2 Frame", dfsiFrame, P25_DFSI_VHDR2_FRAME_LENGTH_BYTES);
#endif

    ::memcpy(data, dfsiFrame, P25_DFSI_VHDR2_FRAME_LENGTH_BYTES);
}

/// <summary>
/// Decode a logical link data unit 1.
/// </summary>
/// <param name="data"></param>
/// <param name="imbe"></param>
/// <returns>True, if decoded, otherwise false.</returns>
bool LC::decodeLDU1(const uint8_t* data, uint8_t* imbe)
{
    assert(data != NULL);
    assert(imbe != NULL);

    m_frameType = data[0U];                                                         // Frame Type

    // different frame types mean different things
    switch (m_frameType)
    {
        case P25_DFSI_LDU1_VOICE1:
            {
                m_control = lc::LC();
                m_lsd = data::LowSpeedData();

                decodeStart(data + 1U);                                             // Start Record
                m_icwFlag = data[5U];                                               // ICW Flag
                m_rssi = data[6U];                                                  // RSSI
                ::memcpy(imbe, data + 10U, P25_RAW_IMBE_LENGTH_BYTES);              // IMBE
                m_source = data[21U];                                               // Source
            }
            break;
        case P25_DFSI_LDU1_VOICE2:
            {
                ::memcpy(imbe, data + 1U, P25_RAW_IMBE_LENGTH_BYTES);               // IMBE
                m_source = data[12U];                                               // Source
            }
            break;
        case P25_DFSI_LDU1_VOICE3:
            {
                m_control.setLCO(data[1U]);                                         // LCO
                m_control.setMFId(data[2U]);                                        // MFId
                uint8_t serviceOptions = (uint8_t)(data[3U]);                       // Service Options
                m_control.setEmergency((serviceOptions & 0x80U) == 0x80U);
                m_control.setEncrypted((serviceOptions & 0x40U) == 0x40U);
                m_control.setPriority((serviceOptions & 0x07U));
                ::memcpy(imbe, data + 5U, P25_RAW_IMBE_LENGTH_BYTES);               // IMBE
            }
            break;
        case P25_DFSI_LDU1_VOICE4:
            {
                uint32_t dstId = (data[1U] << 16) | (data[2U] << 8) | (data[3U] << 0);
                m_control.setDstId(dstId);                                          // Talkgroup Address
                ::memcpy(imbe, data + 5U, P25_RAW_IMBE_LENGTH_BYTES);               // IMBE
            }
            break;
        case P25_DFSI_LDU1_VOICE5:
            {
                uint32_t srcId = (data[1U] << 16) | (data[2U] << 8) | (data[3U] << 0);
                m_control.setSrcId(srcId);                                          // Source Address
                ::memcpy(imbe, data + 5U, P25_RAW_IMBE_LENGTH_BYTES);               // IMBE
            }
            break;
        case P25_DFSI_LDU1_VOICE6:
            {
                ::memcpy(imbe, data + 5U, P25_RAW_IMBE_LENGTH_BYTES);               // IMBE
            }
            break;
        case P25_DFSI_LDU1_VOICE7:
            {
                ::memcpy(imbe, data + 5U, P25_RAW_IMBE_LENGTH_BYTES);               // IMBE
            }
            break;
        case P25_DFSI_LDU1_VOICE8:
            {
                ::memcpy(imbe, data + 5U, P25_RAW_IMBE_LENGTH_BYTES);               // IMBE
            }
            break;
        case P25_DFSI_LDU1_VOICE9:
            {
                m_lsd.setLSD1(data[1U]);                                            // LSD MSB
                m_lsd.setLSD2(data[2U]);                                            // LSD LSB
                ::memcpy(imbe, data + 4U, P25_RAW_IMBE_LENGTH_BYTES);               // IMBE
            }
            break;
        default:
            {
                LogError(LOG_P25, "LC::decodeLDU1(), invalid frametype, frameType = $%02X", m_frameType);
                return false;
            }
            break;
    }

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
    assert(imbe != NULL);

    uint8_t serviceOptions =
        (m_control.getEmergency() ? 0x80U : 0x00U) +
        (m_control.getEncrypted() ? 0x40U : 0x00U) +
        (m_control.getPriority() & 0x07U);

    // determine the LDU1 DFSI frame length, its variable
    uint32_t frameLength = P25_DFSI_LDU1_VOICE1_FRAME_LENGTH_BYTES;
    switch (m_frameType)
    {
        case P25_DFSI_LDU1_VOICE1:
            frameLength = P25_DFSI_LDU1_VOICE1_FRAME_LENGTH_BYTES;
            break;
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
        default:
            {
                LogError(LOG_P25, "LC::encodeLDU1(), invalid frametype, frameType = $%02X", m_frameType);
                return;
            }
            break;
    }

    uint8_t* dfsiFrame = new uint8_t[frameLength];
    ::memset(dfsiFrame, 0x00U, frameLength);

    dfsiFrame[0U] = m_frameType;                                                    // Frame Type

    // different frame types mean different things
    switch (m_frameType)
    {
        case P25_DFSI_LDU1_VOICE2:
            {
                ::memcpy(dfsiFrame + 1U, imbe, P25_RAW_IMBE_LENGTH_BYTES);          // IMBE
                dfsiFrame[12U] = m_source;                                          // Source
            }
            break;
        case P25_DFSI_LDU1_VOICE3:
            {
                dfsiFrame[1U] = m_control.getLCO();                                 // LCO
                dfsiFrame[2U] = m_control.getMFId();                                // MFId
                dfsiFrame[3U] = serviceOptions;                                     // Service Options
                ::memcpy(dfsiFrame + 5U, imbe, P25_RAW_IMBE_LENGTH_BYTES);          // IMBE
                dfsiFrame[16U] = P25_DFSI_STATUS;                                   // Status
            }
            break;
        case P25_DFSI_LDU1_VOICE4:
            {
                uint32_t dstId = m_control.getDstId();
                dfsiFrame[1U] = (dstId >> 16) & 0xFFU;                              // Target Address
                dfsiFrame[2U] = (dstId >> 8) & 0xFFU;
                dfsiFrame[3U] = (dstId >> 0) & 0xFFU;
                ::memcpy(dfsiFrame + 5U, imbe, P25_RAW_IMBE_LENGTH_BYTES);          // IMBE
                dfsiFrame[16U] = P25_DFSI_STATUS;                                   // Status
            }
            break;
        case P25_DFSI_LDU1_VOICE5:
            {
                uint32_t srcId = m_control.getSrcId();
                dfsiFrame[1U] = (srcId >> 16) & 0xFFU;                              // Source Address
                dfsiFrame[2U] = (srcId >> 8) & 0xFFU;
                dfsiFrame[3U] = (srcId >> 0) & 0xFFU;
                ::memcpy(dfsiFrame + 5U, imbe, P25_RAW_IMBE_LENGTH_BYTES);          // IMBE
                dfsiFrame[16U] = P25_DFSI_STATUS;                                   // Status
            }
            break;
        case P25_DFSI_LDU1_VOICE6:
            {
                ::memcpy(dfsiFrame + 5U, imbe, P25_RAW_IMBE_LENGTH_BYTES);          // IMBE
                dfsiFrame[16U] = P25_DFSI_STATUS;                                   // Status
            }
            break;
        case P25_DFSI_LDU1_VOICE7:
            {
                ::memcpy(dfsiFrame + 5U, imbe, P25_RAW_IMBE_LENGTH_BYTES);          // IMBE
                dfsiFrame[16U] = P25_DFSI_STATUS;                                   // Status
            }
            break;
        case P25_DFSI_LDU1_VOICE8:
            {
                ::memcpy(dfsiFrame + 5U, imbe, P25_RAW_IMBE_LENGTH_BYTES);          // IMBE
                dfsiFrame[16U] = P25_DFSI_STATUS;                                   // Status
            }
            break;
        case P25_DFSI_LDU1_VOICE9:
            {
                dfsiFrame[1U] = m_lsd.getLSD1();                                    // LSD MSB
                dfsiFrame[2U] = m_lsd.getLSD2();                                    // LSD LSB
                ::memcpy(dfsiFrame + 4U, imbe, P25_RAW_IMBE_LENGTH_BYTES);          // IMBE
            }
            break;

        case P25_DFSI_LDU1_VOICE1:
        default:
            {
                encodeStart(dfsiFrame + 1U);                                        // Start Record
                dfsiFrame[5U] = m_icwFlag;                                          // ICW Flag
                dfsiFrame[6U] = m_rssi;                                             // RSSI
                ::memcpy(dfsiFrame + 10U, imbe, P25_RAW_IMBE_LENGTH_BYTES);         // IMBE
                dfsiFrame[21U] = m_source;                                          // Source
            }
            break;
    }

#if DEBUG_P25_DFSI
    LogDebug(LOG_P25, "LC::encodeLDU1(), frameType = $%02X", m_frameType);
    Utils::dump(2U, "LC::encodeLDU1(), DFSI LDU1 Frame", dfsiFrame, frameLength);
#endif

    ::memcpy(data, dfsiFrame, frameLength);
    delete[] dfsiFrame;
}

/// <summary>
/// Decode a logical link data unit 2.
/// </summary>
/// <param name="data"></param>
/// <param name="imbe"></param>
/// <returns>True, if decoded, otherwise false.</returns>
bool LC::decodeLDU2(const uint8_t* data, uint8_t* imbe)
{
    assert(data != NULL);
    assert(imbe != NULL);

    m_frameType = data[0U];                                                         // Frame Type

    // different frame types mean different things
    switch (m_frameType)
    {
        case P25_DFSI_LDU2_VOICE10:
            {
                ::memset(m_mi, 0x00U, P25_MI_LENGTH_BYTES);
                m_lsd = data::LowSpeedData();

                decodeStart(data + 1U);                                             // Start Record
                m_icwFlag = data[5U];                                               // ICW Flag
                m_rssi = data[6U];                                                  // RSSI
                ::memcpy(imbe, data + 10U, P25_RAW_IMBE_LENGTH_BYTES);              // IMBE
                m_source = data[21U];                                               // Source
            }
            break;
        case P25_DFSI_LDU2_VOICE11:
            {
                ::memcpy(imbe, data + 1U, P25_RAW_IMBE_LENGTH_BYTES);               // IMBE
            }
            break;
        case P25_DFSI_LDU2_VOICE12:
            {
                m_mi[0U] = data[1U];                                                // Message Indicator
                m_mi[1U] = data[2U];    
                m_mi[2U] = data[3U];
                ::memcpy(imbe, data + 5U, P25_RAW_IMBE_LENGTH_BYTES);               // IMBE
            }
            break;
        case P25_DFSI_LDU2_VOICE13:
            {
                m_mi[3U] = data[1U];                                                // Message Indicator
                m_mi[4U] = data[2U];    
                m_mi[5U] = data[3U];
                ::memcpy(imbe, data + 5U, P25_RAW_IMBE_LENGTH_BYTES);               // IMBE
            }
            break;
        case P25_DFSI_LDU2_VOICE14:
            {
                m_mi[6U] = data[1U];                                                // Message Indicator
                m_mi[7U] = data[2U];    
                m_mi[8U] = data[3U];
                m_control.setMI(m_mi);
                ::memcpy(imbe, data + 5U, P25_RAW_IMBE_LENGTH_BYTES);               // IMBE
            }
            break;
        case P25_DFSI_LDU2_VOICE15:
            {
                m_control.setAlgId(data[1U]);                                       // Algorithm ID
                uint32_t kid = (data[2U] << 8) | (data[3U] << 0);                   // Key ID
                m_control.setKId(kid);
                ::memcpy(imbe, data + 5U, P25_RAW_IMBE_LENGTH_BYTES);               // IMBE
            }
            break;
        case P25_DFSI_LDU2_VOICE16:
            {
                ::memcpy(imbe, data + 5U, P25_RAW_IMBE_LENGTH_BYTES);               // IMBE
            }
            break;
        case P25_DFSI_LDU2_VOICE17:
            {
                ::memcpy(imbe, data + 5U, P25_RAW_IMBE_LENGTH_BYTES);               // IMBE
            }
            break;
        case P25_DFSI_LDU2_VOICE18:
            {
                m_lsd.setLSD1(data[1U]);                                            // LSD MSB
                m_lsd.setLSD2(data[2U]);                                            // LSD LSB
                ::memcpy(imbe, data + 4U, P25_RAW_IMBE_LENGTH_BYTES);               // IMBE
            }
            break;
        default:
            {
                LogError(LOG_P25, "LC::decodeLDU2(), invalid frametype, frameType = $%02X", m_frameType);
                return false;
            }
            break;
    }

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
    assert(imbe != NULL);

    // generate MI data
    uint8_t mi[P25_MI_LENGTH_BYTES];
    ::memset(mi, 0x00U, P25_MI_LENGTH_BYTES);
    m_control.getMI(mi);

    // determine the LDU2 DFSI frame length, its variable
    uint32_t frameLength = P25_DFSI_LDU2_VOICE10_FRAME_LENGTH_BYTES;
    switch (m_frameType)
    {
        case P25_DFSI_LDU2_VOICE10:
            frameLength = P25_DFSI_LDU2_VOICE10_FRAME_LENGTH_BYTES;
            break;
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
        default:
            {
                LogError(LOG_P25, "LC::encodeLDU2(), invalid frametype, frameType = $%02X", m_frameType);
                return;
            }
            break;
    }

    uint8_t* dfsiFrame = new uint8_t[frameLength];
    ::memset(dfsiFrame, 0x00U, frameLength);

    dfsiFrame[0U] = m_frameType;                                                    // Frame Type

    // different frame types mean different things
    switch (m_frameType)
    {
        case P25_DFSI_LDU2_VOICE11:
            {
                ::memcpy(dfsiFrame + 1U, imbe, P25_RAW_IMBE_LENGTH_BYTES);          // IMBE
                dfsiFrame[12U] = P25_DFSI_STATUS;                                   // Status
            }
            break;
        case P25_DFSI_LDU2_VOICE12:
            {
                dfsiFrame[1U] = mi[0U];                                             // Message Indicator
                dfsiFrame[2U] = mi[1U];
                dfsiFrame[3U] = mi[2U];
                ::memcpy(dfsiFrame + 5U, imbe, P25_RAW_IMBE_LENGTH_BYTES);          // IMBE
                dfsiFrame[16U] = P25_DFSI_STATUS;                                   // Status
            }
            break;
        case P25_DFSI_LDU2_VOICE13:
            {
                dfsiFrame[1U] = mi[3U];                                             // Message Indicator
                dfsiFrame[2U] = mi[4U];
                dfsiFrame[3U] = mi[5U];
                ::memcpy(dfsiFrame + 5U, imbe, P25_RAW_IMBE_LENGTH_BYTES);          // IMBE
                dfsiFrame[16U] = P25_DFSI_STATUS;                                   // Status
            }
            break;
        case P25_DFSI_LDU2_VOICE14:
            {
                dfsiFrame[1U] = mi[6U];                                             // Message Indicator
                dfsiFrame[2U] = mi[7U];
                dfsiFrame[3U] = mi[8U];
                ::memcpy(dfsiFrame + 5U, imbe, P25_RAW_IMBE_LENGTH_BYTES);          // IMBE
                dfsiFrame[16U] = P25_DFSI_STATUS;                                   // Status
            }
            break;
        case P25_DFSI_LDU2_VOICE15:
            {
                dfsiFrame[1U] = m_control.getAlgId();                               // Algorithm ID
                uint32_t kid = m_control.getKId();
                dfsiFrame[2U] = (kid >> 8) & 0xFFU;                                 // Key ID
                dfsiFrame[3U] = (kid >> 0) & 0xFFU;
                ::memcpy(dfsiFrame + 5U, imbe, P25_RAW_IMBE_LENGTH_BYTES);          // IMBE
                dfsiFrame[16U] = P25_DFSI_STATUS;                                   // Status
            }
            break;
        case P25_DFSI_LDU2_VOICE16:
            {
                ::memcpy(dfsiFrame + 5U, imbe, P25_RAW_IMBE_LENGTH_BYTES);          // IMBE
                dfsiFrame[16U] = P25_DFSI_STATUS;                                   // Status
            }
            break;
        case P25_DFSI_LDU2_VOICE17:
            {
                ::memcpy(dfsiFrame + 5U, imbe, P25_RAW_IMBE_LENGTH_BYTES);          // IMBE
                dfsiFrame[16U] = P25_DFSI_STATUS;                                   // Status
            }
            break;
        case P25_DFSI_LDU2_VOICE18:
            {
                dfsiFrame[1U] = m_lsd.getLSD1();                                    // LSD MSB
                dfsiFrame[2U] = m_lsd.getLSD2();                                    // LSD LSB
                ::memcpy(dfsiFrame + 4U, imbe, P25_RAW_IMBE_LENGTH_BYTES);          // IMBE
            }
            break;

        case P25_DFSI_LDU2_VOICE10:
        default:
            {
                encodeStart(dfsiFrame + 1U);                                        // Start Record
                dfsiFrame[5U] = m_icwFlag;                                          // ICW Flag
                dfsiFrame[6U] = m_rssi;                                             // RSSI
                ::memcpy(dfsiFrame + 10U, imbe, P25_RAW_IMBE_LENGTH_BYTES);         // IMBE
                dfsiFrame[21U] = m_source;                                          // Source
            }
            break;
    }

#if DEBUG_P25_DFSI
    LogDebug(LOG_P25, "LC::encodeLDU2(), frameType = $%02X", m_frameType);
    Utils::dump(2U, "LC::encodeLDU2(), DFSI LDU2 Frame", dfsiFrame, frameLength);
#endif

    ::memcpy(data, dfsiFrame, frameLength);
    delete[] dfsiFrame;
}

/// <summary>
/// Decode a TSBK.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if decoded, otherwise false.</returns>
bool LC::decodeTSBK(const uint8_t* data)
{
    assert(data != NULL);
    m_tsbk = lc::TSBK();

    m_frameType = data[0U];                                                         // Frame Type
    if (m_frameType != P25_DFSI_TSBK) {
        LogError(LOG_P25, "LC::decodeTSBK(), invalid frametype, frameType = $%02X", m_frameType);
        return false;
    }

    decodeStart(data + 1U);                                                         // Start Record

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES];
    ::memcpy(tsbk, data + 9U, P25_TSBK_LENGTH_BYTES);                               // Raw TSBK + CRC
    return m_tsbk.decode(tsbk, true);
}

/// <summary>
/// Encode a TSBK.
/// </summary>
/// <param name="data"></param>
void LC::encodeTSBK(uint8_t* data)
{
    assert(data != NULL);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES];
    m_tsbk.encode(tsbk, true, true);

    uint8_t dfsiFrame[P25_DFSI_TSBK_FRAME_LENGTH_BYTES];
    ::memset(dfsiFrame, 0x00U, P25_DFSI_TSBK_FRAME_LENGTH_BYTES);

    dfsiFrame[0U] = P25_DFSI_TSBK;                                                  // Frame Type
    encodeStart(dfsiFrame + 1U);                                                    // Start Record
    ::memcpy(dfsiFrame + 9U, tsbk, P25_TSBK_LENGTH_BYTES);                          // Raw TSBK + CRC

#if DEBUG_P25_DFSI
    Utils::dump(2U, "LC::encodeTSBK(), DFSI TSBK Frame", dfsiFrame, P25_TSBK_LENGTH_BYTES);
#endif

    ::memcpy(data, dfsiFrame, P25_DFSI_TSBK_FRAME_LENGTH_BYTES);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void LC::copy(const LC& data)
{
    m_frameType = data.m_frameType;
    m_rtModeFlag = data.m_rtModeFlag;
    m_startStopFlag = data.m_startStopFlag;
    m_typeFlag = data.m_typeFlag;
    m_icwFlag = data.m_icwFlag;

    m_rssi = data.m_rssi;
    m_source = data.m_source;

    m_control = lc::LC(data.m_control);
    m_tsbk = lc::TSBK(data.m_tsbk);
    m_lsd = data.m_lsd;

    delete[] m_mi;

    uint8_t* mi = new uint8_t[P25_MI_LENGTH_BYTES];
    ::memcpy(mi, data.m_mi, P25_MI_LENGTH_BYTES);

    m_mi = mi;
}

/// <summary>
/// Decode start record data.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if decoded, otherwise false.</returns>
bool LC::decodeStart(const uint8_t* data)
{
    assert(data != NULL);

    m_rtModeFlag = data[0U];                                                        // RT Mode Flag
    m_startStopFlag = data[1U];                                                     // Start/Stop Flag
    m_typeFlag = data[2U];                                                          // Type Flag

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

    rawFrame[0U] = 0x02U;                                                           //
    rawFrame[1U] = m_rtModeFlag;                                                    // RT/RT Mode Flag
    rawFrame[2U] = m_startStopFlag;                                                 // Start/Stop Flag
    rawFrame[3U] = m_typeFlag;                                                      // Type flag

    ::memcpy(data, rawFrame, P25_DFSI_START_LENGTH_BYTES);
}
