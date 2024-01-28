// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "network/RTPFNEHeader.h"
#include "Utils.h"

using namespace network::frame;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the RTPFNEHeader class.
/// </summary>
RTPFNEHeader::RTPFNEHeader() :
    RTPExtensionHeader(),
    m_crc16(0U),
    m_func(0U),
    m_subFunc(0U),
    m_streamId(0U),
    m_peerId(0U),
    m_messageLength(0U)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the RTPFNEHeader class.
/// </summary>
RTPFNEHeader::~RTPFNEHeader() = default;

/// <summary>
/// Decode a RTP header.
/// </summary>
/// <param name="data"></param>
bool RTPFNEHeader::decode(const uint8_t* data)
{
    assert(data != nullptr);

    RTPExtensionHeader::decode(data);
    if (m_payloadLength != RTP_FNE_HEADER_LENGTH_EXT_LEN) {
        return false;
    }

    if (m_payloadType != DVM_FRAME_START) {
        return false;
    }

    m_crc16 = (data[4U] << 8) | (data[5U] << 0);                                // CRC-16
    m_func = data[6U];                                                          // Function
    m_subFunc = data[7U];                                                       // Sub-Function
    m_streamId = __GET_UINT32(data, 8U);                                        // Stream ID
    m_peerId = __GET_UINT32(data, 12U);                                         // Peer ID
    m_messageLength = __GET_UINT32(data, 16U);                                  // Message Length

    return true;
}

/// <summary>
/// Encode a RTP header.
/// </summary>
/// <param name="data"></param>
void RTPFNEHeader::encode(uint8_t* data)
{
    assert(data != nullptr);

    m_payloadType = DVM_FRAME_START;
    m_payloadLength = RTP_FNE_HEADER_LENGTH_EXT_LEN;
    RTPExtensionHeader::encode(data);

    data[4U] = (m_crc16 >> 8) & 0xFFU;                                          // CRC-16 MSB
    data[5U] = (m_crc16 >> 0) & 0xFFU;                                          // CRC-16 LSB
    data[6U] = m_func;                                                          // Function
    data[7U] = m_subFunc;                                                       // Sub-Function

    __SET_UINT32(m_streamId, data, 8U);                                         // Stream ID
    __SET_UINT32(m_peerId, data, 12U);                                          // Peer ID
    __SET_UINT32(m_messageLength, data, 16U);                                   // Message Length
}
