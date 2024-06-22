// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/dfsi/DFSIDefines.h"
#include "p25/dfsi/LC.h"
#include "Log.h"
#include "Utils.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::dfsi;
using namespace p25::dfsi::defines;

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the LC class.
/// </summary>
LC::LC() :
    m_rssi(0U),
    m_control(nullptr),
    m_lsd(nullptr),
    m_rs(),
    m_mi(nullptr)
{
    m_mi = new uint8_t[MI_LENGTH_BYTES];
    ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);

    m_control = new lc::LC();
    m_lsd = new data::LowSpeedData();
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
    m_control = new lc::LC(control);
    m_lsd = new data::LowSpeedData(lsd);
}

/// <summary>
/// Finalizes a instance of LC class.
/// </summary>
LC::~LC()
{
    if (m_control != nullptr) {
        delete m_control;
    }
    if (m_lsd != nullptr) {
        delete m_lsd;
    }
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
/// Helper to set the LC data.
/// </summary>
/// <param name="data"></param>
void LC::setControl(const lc::LC& data)
{
    if (m_control != nullptr) {
        delete m_control;
    }
    m_control = new lc::LC(data);
}

/// <summary>
/// Decode a logical link data unit 1.
/// </summary>
/// <param name="data"></param>
/// <param name="imbe"></param>
/// <returns>True, if decoded, otherwise false.</returns>
bool LC::decodeLDU1(const uint8_t* data, uint8_t* imbe)
{
    assert(data != nullptr);
    assert(imbe != nullptr);

    m_frameType = data[0U];                                                         // Frame Type

    // different frame types mean different things
    switch (m_frameType)
    {
        case DFSIFrameType::LDU1_VOICE1:
            {
                if (m_control != nullptr) {
                    delete m_control;
                }
                m_control = new lc::LC();

                if (m_lsd != nullptr) {
                    m_lsd = new data::LowSpeedData();
                }

                m_rssi = data[6U];                                                  // RSSI
                ::memcpy(imbe, data + 10U, RAW_IMBE_LENGTH_BYTES);                  // IMBE
            }
            break;
        case DFSIFrameType::LDU1_VOICE2:
            {
                ::memcpy(imbe, data + 1U, RAW_IMBE_LENGTH_BYTES);                   // IMBE
            }
            break;
        case DFSIFrameType::LDU1_VOICE3:
            {
                m_control->setLCO(data[1U]);                                        // LCO
                m_control->setMFId(data[2U]);                                       // MFId
                uint8_t serviceOptions = (uint8_t)(data[3U]);                       // Service Options
                m_control->setEmergency((serviceOptions & 0x80U) == 0x80U);
                m_control->setEncrypted((serviceOptions & 0x40U) == 0x40U);
                m_control->setPriority((serviceOptions & 0x07U));
                ::memcpy(imbe, data + 5U, RAW_IMBE_LENGTH_BYTES);                   // IMBE
            }
            break;
        case DFSIFrameType::LDU1_VOICE4:
            {
                uint32_t dstId = (data[1U] << 16) | (data[2U] << 8) | (data[3U] << 0);
                m_control->setDstId(dstId);                                         // Talkgroup Address
                ::memcpy(imbe, data + 5U, RAW_IMBE_LENGTH_BYTES);                   // IMBE
            }
            break;
        case DFSIFrameType::LDU1_VOICE5:
            {
                uint32_t srcId = (data[1U] << 16) | (data[2U] << 8) | (data[3U] << 0);
                m_control->setSrcId(srcId);                                         // Source Address
                ::memcpy(imbe, data + 5U, RAW_IMBE_LENGTH_BYTES);                   // IMBE
            }
            break;
        case DFSIFrameType::LDU1_VOICE6:
            {
                ::memcpy(imbe, data + 5U, RAW_IMBE_LENGTH_BYTES);                   // IMBE
            }
            break;
        case DFSIFrameType::LDU1_VOICE7:
            {
                ::memcpy(imbe, data + 5U, RAW_IMBE_LENGTH_BYTES);                   // IMBE
            }
            break;
        case DFSIFrameType::LDU1_VOICE8:
            {
                ::memcpy(imbe, data + 5U, RAW_IMBE_LENGTH_BYTES);                   // IMBE
            }
            break;
        case DFSIFrameType::LDU1_VOICE9:
            {
                m_lsd->setLSD1(data[1U]);                                           // LSD MSB
                m_lsd->setLSD2(data[2U]);                                           // LSD LSB
                ::memcpy(imbe, data + 4U, RAW_IMBE_LENGTH_BYTES);                   // IMBE
            }
            break;
        default:
            {
                LogError(LOG_P25, "LC::decodeLDU1(), invalid frame type, frameType = $%02X", m_frameType);
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
    assert(data != nullptr);
    assert(imbe != nullptr);
    assert(m_control != nullptr);

    uint8_t serviceOptions =
        (m_control->getEmergency() ? 0x80U : 0x00U) +
        (m_control->getEncrypted() ? 0x40U : 0x00U) +
        (m_control->getPriority() & 0x07U);

    // determine the LDU1 DFSI frame length, its variable
    uint32_t frameLength = DFSI_LDU1_VOICE1_FRAME_LENGTH_BYTES;
    switch (m_frameType)
    {
        case DFSIFrameType::LDU1_VOICE1:
            frameLength = DFSI_LDU1_VOICE1_FRAME_LENGTH_BYTES;
            break;
        case DFSIFrameType::LDU1_VOICE2:
            frameLength = DFSI_LDU1_VOICE2_FRAME_LENGTH_BYTES;
            break;
        case DFSIFrameType::LDU1_VOICE3:
            frameLength = DFSI_LDU1_VOICE3_FRAME_LENGTH_BYTES;
            break;
        case DFSIFrameType::LDU1_VOICE4:
            frameLength = DFSI_LDU1_VOICE4_FRAME_LENGTH_BYTES;
            break;
        case DFSIFrameType::LDU1_VOICE5:
            frameLength = DFSI_LDU1_VOICE5_FRAME_LENGTH_BYTES;
            break;
        case DFSIFrameType::LDU1_VOICE6:
            frameLength = DFSI_LDU1_VOICE6_FRAME_LENGTH_BYTES;
            break;
        case DFSIFrameType::LDU1_VOICE7:
            frameLength = DFSI_LDU1_VOICE7_FRAME_LENGTH_BYTES;
            break;
        case DFSIFrameType::LDU1_VOICE8:
            frameLength = DFSI_LDU1_VOICE8_FRAME_LENGTH_BYTES;
            break;
        case DFSIFrameType::LDU1_VOICE9:
            frameLength = DFSI_LDU1_VOICE9_FRAME_LENGTH_BYTES;
            break;
        default:
            {
                LogError(LOG_P25, "LC::encodeLDU1(), invalid frame type, frameType = $%02X", m_frameType);
                return;
            }
            break;
    }

    uint8_t rs[P25_LDU_LC_FEC_LENGTH_BYTES];
    ::memset(rs, 0x00U, P25_LDU_LC_FEC_LENGTH_BYTES);

    rs[0U] = m_control->getLCO();                                                   // LCO
    rs[1U] = m_control->getMFId();                                                  // MFId
    rs[2U] = serviceOptions;                                                        // Service Options
    uint32_t dstId = m_control->getDstId();
    rs[3U] = (dstId >> 16) & 0xFFU;                                                 // Target Address
    rs[4U] = (dstId >> 8) & 0xFFU;
    rs[5U] = (dstId >> 0) & 0xFFU;
    uint32_t srcId = m_control->getSrcId();
    rs[6U] = (srcId >> 16) & 0xFFU;                                                 // Source Address
    rs[7U] = (srcId >> 8) & 0xFFU;
    rs[8U] = (srcId >> 0) & 0xFFU;

    // encode RS (24,12,13) FEC
    m_rs.encode241213(rs);

    uint8_t dfsiFrame[frameLength];

    dfsiFrame[0U] = m_frameType;                                                    // Frame Type

    // different frame types mean different things
    switch (m_frameType)
    {
        case DFSIFrameType::LDU1_VOICE2:
            {
                ::memcpy(dfsiFrame + 1U, imbe, RAW_IMBE_LENGTH_BYTES);              // IMBE
            }
            break;
        case DFSIFrameType::LDU1_VOICE3:
            {
                dfsiFrame[1U] = rs[0U];                                             // LCO
                dfsiFrame[2U] = rs[1U];                                             // MFId
                dfsiFrame[3U] = rs[2U];                                             // Service Options
                ::memcpy(dfsiFrame + 5U, imbe, RAW_IMBE_LENGTH_BYTES);              // IMBE
            }
            break;
        case DFSIFrameType::LDU1_VOICE4:
            {
                dfsiFrame[1U] = rs[3U];                                             // Target Address
                dfsiFrame[2U] = rs[4U];
                dfsiFrame[3U] = rs[5U];
                ::memcpy(dfsiFrame + 5U, imbe, RAW_IMBE_LENGTH_BYTES);              // IMBE
            }
            break;
        case DFSIFrameType::LDU1_VOICE5:
            {
                dfsiFrame[1U] = rs[6U];                                             // Source Address
                dfsiFrame[2U] = rs[7U];
                dfsiFrame[3U] = rs[8U];
                ::memcpy(dfsiFrame + 5U, imbe, RAW_IMBE_LENGTH_BYTES);              // IMBE
            }
            break;
        case DFSIFrameType::LDU1_VOICE6:
            {
                dfsiFrame[1U] = rs[9U];                                             // RS (24,12,13)
                dfsiFrame[2U] = rs[10U];                                            // RS (24,12,13)
                dfsiFrame[3U] = rs[11U];                                            // RS (24,12,13)
                ::memcpy(dfsiFrame + 5U, imbe, RAW_IMBE_LENGTH_BYTES);              // IMBE
            }
            break;
        case DFSIFrameType::LDU1_VOICE7:
            {
                dfsiFrame[1U] = rs[12U];                                            // RS (24,12,13)
                dfsiFrame[2U] = rs[13U];                                            // RS (24,12,13)
                dfsiFrame[3U] = rs[14U];                                            // RS (24,12,13)
                ::memcpy(dfsiFrame + 5U, imbe, RAW_IMBE_LENGTH_BYTES);              // IMBE
            }
            break;
        case DFSIFrameType::LDU1_VOICE8:
            {
                dfsiFrame[1U] = rs[15U];                                            // RS (24,12,13)
                dfsiFrame[2U] = rs[16U];                                            // RS (24,12,13)
                dfsiFrame[3U] = rs[17U];                                            // RS (24,12,13)
                ::memcpy(dfsiFrame + 5U, imbe, RAW_IMBE_LENGTH_BYTES);              // IMBE
            }
            break;
        case DFSIFrameType::LDU1_VOICE9:
            {
                dfsiFrame[1U] = m_lsd->getLSD1();                                   // LSD MSB
                dfsiFrame[2U] = m_lsd->getLSD2();                                   // LSD LSB
                ::memcpy(dfsiFrame + 4U, imbe, RAW_IMBE_LENGTH_BYTES);              // IMBE
            }
            break;

        case DFSIFrameType::LDU1_VOICE1:
        default:
            {
                dfsiFrame[6U] = m_rssi;                                             // RSSI
                ::memcpy(dfsiFrame + 10U, imbe, RAW_IMBE_LENGTH_BYTES);             // IMBE
            }
            break;
    }

#if DEBUG_P25_DFSI
    LogDebug(LOG_P25, "LC::encodeLDU1(), frameType = $%02X", m_frameType);
    Utils::dump(2U, "LC::encodeLDU1(), DFSI LDU1 Frame", dfsiFrame, frameLength);
#endif

    ::memcpy(data, dfsiFrame, frameLength);
}

/// <summary>
/// Decode a logical link data unit 2.
/// </summary>
/// <param name="data"></param>
/// <param name="imbe"></param>
/// <returns>True, if decoded, otherwise false.</returns>
bool LC::decodeLDU2(const uint8_t* data, uint8_t* imbe)
{
    assert(data != nullptr);
    assert(imbe != nullptr);
    assert(m_control != nullptr);

    m_frameType = data[0U];                                                         // Frame Type

    // different frame types mean different things
    switch (m_frameType)
    {
        case DFSIFrameType::LDU2_VOICE10:
            {
                ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);
                if (m_lsd != nullptr) {
                    delete m_lsd;
                }
                m_lsd = new data::LowSpeedData();

                m_rssi = data[6U];                                                  // RSSI
                ::memcpy(imbe, data + 10U, RAW_IMBE_LENGTH_BYTES);                  // IMBE
            }
            break;
        case DFSIFrameType::LDU2_VOICE11:
            {
                ::memcpy(imbe, data + 1U, RAW_IMBE_LENGTH_BYTES);                   // IMBE
            }
            break;
        case DFSIFrameType::LDU2_VOICE12:
            {
                m_mi[0U] = data[1U];                                                // Message Indicator
                m_mi[1U] = data[2U];
                m_mi[2U] = data[3U];
                ::memcpy(imbe, data + 5U, RAW_IMBE_LENGTH_BYTES);                   // IMBE
            }
            break;
        case DFSIFrameType::LDU2_VOICE13:
            {
                m_mi[3U] = data[1U];                                                // Message Indicator
                m_mi[4U] = data[2U];
                m_mi[5U] = data[3U];
                ::memcpy(imbe, data + 5U, RAW_IMBE_LENGTH_BYTES);                   // IMBE
            }
            break;
        case DFSIFrameType::LDU2_VOICE14:
            {
                m_mi[6U] = data[1U];                                                // Message Indicator
                m_mi[7U] = data[2U];
                m_mi[8U] = data[3U];
                m_control->setMI(m_mi);
                ::memcpy(imbe, data + 5U, RAW_IMBE_LENGTH_BYTES);                   // IMBE
            }
            break;
        case DFSIFrameType::LDU2_VOICE15:
            {
                m_control->setAlgId(data[1U]);                                      // Algorithm ID
                uint32_t kid = (data[2U] << 8) | (data[3U] << 0);                   // Key ID
                m_control->setKId(kid);
                ::memcpy(imbe, data + 5U, RAW_IMBE_LENGTH_BYTES);                   // IMBE
            }
            break;
        case DFSIFrameType::LDU2_VOICE16:
            {
                ::memcpy(imbe, data + 5U, RAW_IMBE_LENGTH_BYTES);                   // IMBE
            }
            break;
        case DFSIFrameType::LDU2_VOICE17:
            {
                ::memcpy(imbe, data + 5U, RAW_IMBE_LENGTH_BYTES);                   // IMBE
            }
            break;
        case DFSIFrameType::LDU2_VOICE18:
            {
                m_lsd->setLSD1(data[1U]);                                           // LSD MSB
                m_lsd->setLSD2(data[2U]);                                           // LSD LSB
                ::memcpy(imbe, data + 4U, RAW_IMBE_LENGTH_BYTES);                   // IMBE
            }
            break;
        default:
            {
                LogError(LOG_P25, "LC::decodeLDU2(), invalid frame type, frameType = $%02X", m_frameType);
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
    assert(data != nullptr);
    assert(imbe != nullptr);
    assert(m_control != nullptr);

    // generate MI data
    uint8_t mi[MI_LENGTH_BYTES];
    ::memset(mi, 0x00U, MI_LENGTH_BYTES);
    m_control->getMI(mi);

    // determine the LDU2 DFSI frame length, its variable
    uint32_t frameLength = DFSI_LDU2_VOICE10_FRAME_LENGTH_BYTES;
    switch (m_frameType)
    {
        case DFSIFrameType::LDU2_VOICE10:
            frameLength = DFSI_LDU2_VOICE10_FRAME_LENGTH_BYTES;
            break;
        case DFSIFrameType::LDU2_VOICE11:
            frameLength = DFSI_LDU2_VOICE11_FRAME_LENGTH_BYTES;
            break;
        case DFSIFrameType::LDU2_VOICE12:
            frameLength = DFSI_LDU2_VOICE12_FRAME_LENGTH_BYTES;
            break;
        case DFSIFrameType::LDU2_VOICE13:
            frameLength = DFSI_LDU2_VOICE13_FRAME_LENGTH_BYTES;
            break;
        case DFSIFrameType::LDU2_VOICE14:
            frameLength = DFSI_LDU2_VOICE14_FRAME_LENGTH_BYTES;
            break;
        case DFSIFrameType::LDU2_VOICE15:
            frameLength = DFSI_LDU2_VOICE15_FRAME_LENGTH_BYTES;
            break;
        case DFSIFrameType::LDU2_VOICE16:
            frameLength = DFSI_LDU2_VOICE16_FRAME_LENGTH_BYTES;
            break;
        case DFSIFrameType::LDU2_VOICE17:
            frameLength = DFSI_LDU2_VOICE17_FRAME_LENGTH_BYTES;
            break;
        case DFSIFrameType::LDU2_VOICE18:
            frameLength = DFSI_LDU2_VOICE18_FRAME_LENGTH_BYTES;
            break;
        default:
            {
                LogError(LOG_P25, "LC::encodeLDU2(), invalid frame type, frameType = $%02X", m_frameType);
                return;
            }
            break;
    }

    uint8_t rs[P25_LDU_LC_FEC_LENGTH_BYTES];
    ::memset(rs, 0x00U, P25_LDU_LC_FEC_LENGTH_BYTES);

    for (uint32_t i = 0; i < MI_LENGTH_BYTES; i++)
        rs[i] = mi[i];                                                              // Message Indicator

    rs[9U] = m_control->getAlgId();                                                 // Algorithm ID
    rs[10U] = (m_control->getKId() >> 8) & 0xFFU;                                   // Key ID
    rs[11U] = (m_control->getKId() >> 0) & 0xFFU;                                   // ...

    // encode RS (24,16,9) FEC
    m_rs.encode24169(rs);

    uint8_t dfsiFrame[frameLength];

    dfsiFrame[0U] = m_frameType;                                                    // Frame Type

    // different frame types mean different things
    switch (m_frameType)
    {
        case DFSIFrameType::LDU2_VOICE11:
            {
                ::memcpy(dfsiFrame + 1U, imbe, RAW_IMBE_LENGTH_BYTES);              // IMBE
            }
            break;
        case DFSIFrameType::LDU2_VOICE12:
            {
                dfsiFrame[1U] = rs[0U];                                             // Message Indicator
                dfsiFrame[2U] = rs[1U];
                dfsiFrame[3U] = rs[2U];
                ::memcpy(dfsiFrame + 5U, imbe, RAW_IMBE_LENGTH_BYTES);              // IMBE
            }
            break;
        case DFSIFrameType::LDU2_VOICE13:
            {
                dfsiFrame[1U] = rs[3U];                                             // Message Indicator
                dfsiFrame[2U] = rs[4U];
                dfsiFrame[3U] = rs[5U];
                ::memcpy(dfsiFrame + 5U, imbe, RAW_IMBE_LENGTH_BYTES);              // IMBE
            }
            break;
        case DFSIFrameType::LDU2_VOICE14:
            {
                dfsiFrame[1U] = rs[6U];                                             // Message Indicator
                dfsiFrame[2U] = rs[7U];
                dfsiFrame[3U] = rs[8U];
                ::memcpy(dfsiFrame + 5U, imbe, RAW_IMBE_LENGTH_BYTES);              // IMBE
            }
            break;
        case DFSIFrameType::LDU2_VOICE15:
            {
                dfsiFrame[1U] = rs[9U];                                             // Algorithm ID
                dfsiFrame[2U] = rs[10U];                                            // Key ID
                dfsiFrame[3U] = rs[11U];
                ::memcpy(dfsiFrame + 5U, imbe, RAW_IMBE_LENGTH_BYTES);              // IMBE
            }
            break;
        case DFSIFrameType::LDU2_VOICE16:
            {
                dfsiFrame[1U] = rs[12U];                                            // RS (24,16,9)
                dfsiFrame[2U] = rs[13U];                                            // RS (24,16,9)
                dfsiFrame[3U] = rs[14U];                                            // RS (24,16,9)
                ::memcpy(dfsiFrame + 5U, imbe, RAW_IMBE_LENGTH_BYTES);              // IMBE
            }
            break;
        case DFSIFrameType::LDU2_VOICE17:
            {
                dfsiFrame[1U] = rs[15U];                                            // RS (24,16,9)
                dfsiFrame[2U] = rs[16U];                                            // RS (24,16,9)
                dfsiFrame[3U] = rs[17U];                                            // RS (24,16,9)
                ::memcpy(dfsiFrame + 5U, imbe, RAW_IMBE_LENGTH_BYTES);              // IMBE
            }
            break;
        case DFSIFrameType::LDU2_VOICE18:
            {
                dfsiFrame[1U] = m_lsd->getLSD1();                                   // LSD MSB
                dfsiFrame[2U] = m_lsd->getLSD2();                                   // LSD LSB
                ::memcpy(dfsiFrame + 4U, imbe, RAW_IMBE_LENGTH_BYTES);              // IMBE
            }
            break;

        case DFSIFrameType::LDU2_VOICE10:
        default:
            {
                dfsiFrame[6U] = m_rssi;                                             // RSSI
                ::memcpy(dfsiFrame + 10U, imbe, RAW_IMBE_LENGTH_BYTES);             // IMBE
            }
            break;
    }

#if DEBUG_P25_DFSI
    LogDebug(LOG_P25, "LC::encodeLDU2(), frameType = $%02X", m_frameType);
    Utils::dump(2U, "LC::encodeLDU2(), DFSI LDU2 Frame", dfsiFrame, frameLength);
#endif

    ::memcpy(data, dfsiFrame, frameLength);
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

    m_rssi = data.m_rssi;

    m_control = new lc::LC(*data.m_control);
    m_lsd = new data::LowSpeedData(*data.m_lsd);

    delete[] m_mi;

    uint8_t* mi = new uint8_t[MI_LENGTH_BYTES];
    ::memcpy(mi, data.m_mi, MI_LENGTH_BYTES);

    m_mi = mi;
}
