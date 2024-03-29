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
#include "nxdn/NXDNDefines.h"
#include "nxdn/lc/PacketInformation.h"
#include "Log.h"

using namespace nxdn::lc;
using namespace nxdn;

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the PacketInformation class.
/// </summary>
PacketInformation::PacketInformation() :
    m_delivery(false),
    m_selectiveRetry(false),
    m_blockCount(0U),
    m_padCount(0U),
    m_start(true),
    m_circular(false),
    m_fragmentCount(0U),
    m_rspClass(DATA_RSP_CLASS_ACK),
    m_rspType(1U),
    m_rspErrorBlock(0U)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the PacketInformation class.
/// </summary>
PacketInformation::~PacketInformation() = default;

/// <summary>
/// Decodes packet information.
/// </summary>
/// <param name="messageType"></param>
/// <param name="data"></param>
/// <returns>True, if packet information was decoded, otherwise false.</returns>
bool PacketInformation::decode(const uint8_t messageType, const uint8_t* data)
{
    assert(data != nullptr);

    switch (messageType)
    {
    case RTCH_MESSAGE_TYPE_DCALL_HDR:
        m_delivery = (data[0U] & 0x80U) == 0x80U;                                   // Delivery
        m_selectiveRetry = (data[0U] & 0x20U) == 0x20U;                             // Selective Retry
        m_blockCount = (data[0U] & 0x0FU);                                          // Block Count

        m_padCount = (data[1U] >> 3) & 0x1FU;                                       // Pad Count
        m_start = (data[1U] & 0x08U) == 0x08U;                                      // Start/First Fragment
        m_circular = (data[1U] & 0x04U) == 0x04U;                                   // Circular Fragment Count

        m_fragmentCount = ((data[1U] & 0x01U) << 8) + data[2U];                     // Fragment Count
        break;
    case RTCH_MESSAGE_TYPE_DCALL_ACK:
        m_rspClass = (data[0U] >> 4) & 0x03U;                                       // Response Class
        m_rspType = (data[0U] >> 1) & 0x07U;                                        // Response Type
        m_fragmentCount = ((data[0U] & 0x01U) << 8) + data[1U];                     // Fragment Count
        break;
    case RTCH_MESSAGE_TYPE_SDCALL_REQ_HDR:
        m_delivery = (data[0U] & 0x80U) == 0x80U;                                   // Delivery
        m_selectiveRetry = (data[0U] & 0x20U) == 0x20U;                             // Selective Retry
        m_blockCount = (data[0U] & 0x0FU);                                          // Block Count

        m_padCount = (data[1U] >> 3) & 0x1FU;                                       // Pad Count
        m_start = (data[1U] & 0x08U) == 0x08U;                                      // Start/First Fragment
        m_circular = (data[1U] & 0x04U) == 0x04U;                                   // Circular Fragment Count
        break;
    default:
        LogError(LOG_NXDN, "PacketInformation::decode(), unknown LC value, messageType = $%02X", messageType);
        return false;
    }

    return true;
}

/// <summary>
/// Encodes packet information.
/// </summary>
/// <param name="messageType"></param>
/// <param name="data"></param>
void PacketInformation::encode(const uint8_t messageType, uint8_t* data)
{
    assert(data != nullptr);

    switch (messageType)
    {
    case RTCH_MESSAGE_TYPE_DCALL_HDR:
    {
        ::memset(data, 0x00U, NXDN_PCKT_INFO_LENGTH_BYTES);

        data[0U] = (m_delivery ? 0x80U : 0x00U) +                                   // Delivery
            (m_selectiveRetry ? 0x20U : 0x00U) +                                    // Selective Retry
            m_blockCount;                                                           // Block Count

        data[1U] = (m_padCount << 3) +                                              // Pad Count
            (m_start ? 0x08U : 0x00U) +                                             // Start/First Fragment
            (m_circular ? 0x04U : 0x00U);                                           // Circular Fragment Count

        bool highFragCount = (m_fragmentCount & 0x100U) == 0x100U;
        data[1U] += (highFragCount ? 0x01U : 0x00U);                                // Fragment Count - bit 8
        data[2U] = m_fragmentCount & 0xFFU;                                         // Fragment Count - bit 0 - 7
    }
    break;
    case RTCH_MESSAGE_TYPE_DCALL_ACK:
    {
        data[0U] = (m_rspClass & 0x03U << 4) +                                      // Response Class
            (m_rspType & 0x07U << 1);                                               // Response Type

        bool highFragCount = (m_fragmentCount & 0x100U) == 0x100U;
        data[0U] += (highFragCount ? 0x01U : 0x00U);                                // Fragment Count - bit 8
        data[1U] = m_fragmentCount & 0xFFU;                                         // Fragment Count - bit 0 - 7
    }
    break;
    case RTCH_MESSAGE_TYPE_SDCALL_REQ_HDR:
        ::memset(data, 0x00U, NXDN_PCKT_INFO_LENGTH_BYTES);

        data[0U] = (m_delivery ? 0x80U : 0x00U) +                                   // Delivery
            (m_selectiveRetry ? 0x20U : 0x00U) +                                    // Selective Retry
            m_blockCount;                                                           // Block Count

        data[1U] = (m_padCount << 3) +                                              // Pad Count
            (m_start ? 0x08U : 0x00U) +                                             // Start/First Fragment
            (m_circular ? 0x04U : 0x00U);                                           // Circular Fragment Count
        break;
    default:
        LogError(LOG_NXDN, "PacketInformation::encode(), unknown LC value, messageType = $%02X", messageType);
        break;
    }
}

/// <summary>
/// Helper to reset data values to defaults.
/// </summary>
void PacketInformation::reset()
{
    m_delivery = false;
    m_selectiveRetry = false;
    m_blockCount = 0U;

    m_padCount = 0U;
    m_start = true;
    m_circular = false;

    m_fragmentCount = 0U;

    m_rspClass = DATA_RSP_CLASS_ACK;
    m_rspType = 0U;
    m_rspErrorBlock = 0U;
}
