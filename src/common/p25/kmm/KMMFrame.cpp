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
#include "p25/P25Defines.h"
#include "p25/Crypto.h"
#include "p25/kmm/KMMFrame.h"
#include "Log.h"

using namespace p25;
using namespace p25::crypto;
using namespace p25::defines;
using namespace p25::kmm;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a copy instance of the KMMFrame class. */

KMMFrame::KMMFrame(const KMMFrame& data) : KMMFrame()
{
    copy(data);
}

/* Initializes a new instance of the KMMFrame class. */

KMMFrame::KMMFrame() :
    m_messageId(KMM_MessageType::NULL_CMD),
    m_messageLength(KMM_FRAME_LENGTH),
    m_respKind(KMM_ResponseKind::NONE),
    m_macType(KMM_MAC::NO_MAC),
    m_macAlgId(ALGO_UNENCRYPT),
    m_macKId(0U),
    m_macFormat(0U),
    m_messageNumber(0U),
    m_dstLlId(0U),
    m_srcLlId(0U),
    m_complete(true),
    m_messageFullLength(0U),
    m_bodyOffset(0U),
    m_mac(nullptr)
{
    m_mac = new uint8_t[P25DEF::KMM_AES_MAC_LENGTH];
    ::memset(m_mac, 0x00U, P25DEF::KMM_AES_MAC_LENGTH);
}

/* Finalizes a instance of the KMMFrame class. */

KMMFrame::~KMMFrame()
{
    if (m_mac != nullptr)
        delete[] m_mac;
}

/* Generate a MAC code for the given KMM frame. */

void KMMFrame::generateMAC(uint8_t* kek, uint8_t* data)
{
    assert(data != nullptr);

    if (m_macType == KMM_MAC::NO_MAC) {
        ::LogError(LOG_P25, "KMMFrame::generateMAC(), MAC type is set to no MAC, aborting MAC signing");
        return;
    }

    if (m_macAlgId == ALGO_UNENCRYPT) {
        ::LogError(LOG_P25, "KMMFrame::generateMAC(), MAC algorithm is not set, aborting MAC signing");
        return;
    }

    if (m_macKId == 0U) {
        ::LogError(LOG_P25, "KMMFrame::generateMAC(), MAC key ID is not set, aborting MAC signing");
        return;
    }

    switch (m_macType) {
    case KMM_MAC::DES_MAC:
        ::LogError(LOG_P25, "KMMFrame::generateMAC(), DES MAC type is not supported, macType = $%02X", m_macType);
        return;

    case KMM_MAC::ENH_MAC:
        {
            uint8_t macLength = P25DEF::KMM_AES_MAC_LENGTH;
            P25Crypto crypto;

            switch (m_macFormat) {
            case KMM_MAC_FORMAT_CBC:
                {
                    // generate intermediate derived key
                    UInt8Array macKey = crypto.cryptAES_KMM_CBC_KDF(kek, data, m_messageFullLength);

                    // generate MAC
                    UInt8Array mac = crypto.cryptAES_KMM_CBC(macKey.get(), data, m_messageFullLength);

                    ::memset(data + m_messageFullLength - (macLength + 5U), 0x00U, macLength);
                    ::memcpy(data + m_messageFullLength - (macLength + 5U), mac.get(), macLength);
                }
                break;

            case KMM_MAC_FORMAT_CMAC:
                {
                    // generate intermediate derived key
                    UInt8Array macKey = crypto.cryptAES_KMM_CMAC_KDF(kek, data, m_messageFullLength, m_messageNumber > 0U);

                    // generate MAC
                    UInt8Array mac = crypto.cryptAES_KMM_CMAC(macKey.get(), data, m_messageFullLength);

                    ::memset(data + m_messageFullLength - (macLength + 5U), 0x00U, macLength);
                    ::memcpy(data + m_messageFullLength - (macLength + 5U), mac.get(), macLength);
                }
                break;

            default:
                ::LogError(LOG_P25, "KMMFrame::generateMAC(), unknown KMM MAC format type value, macType = $%02X", m_macType);
                break;
            }
        }
        break;

    case KMM_MAC::NO_MAC:
        break;

    default:
        ::LogError(LOG_P25, "KMMFrame::generateMAC(), unknown KMM MAC inventory type value, macType = $%02X", m_macType);
        break;
    }
}

/* Returns a string that represents the current KMM frame. */

std::string KMMFrame::toString()
{
    return std::string("KMM, UNKNOWN (Unknown KMM)");
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to decode a KMM header. */

bool KMMFrame::decodeHeader(const uint8_t* data)
{
    assert(data != nullptr);

    m_messageId = data[0U];                                                         // Message ID
    m_messageLength = GET_UINT16(data, 1U);                                         // Message Length
    m_messageFullLength = m_messageLength + 3U; // length including ID and length fields

    m_respKind = (data[3U] >> 6U) & 0x03U;                                          // Response Kind
    bool hasMN = ((data[3U] >> 4U) & 0x03U) == 0x02U;                               // Message Number Flag
    m_macType = (data[3U] >> 2U) & 0x03U;                                           // MAC Type

    bool done = (data[3U] & 0x01U) == 0x01U;                                        // Done Flag
    if (!done)
        m_complete = true;
    else
        m_complete = false;

    m_dstLlId = GET_UINT24(data, 4U);                                               // Destination RSI
    m_srcLlId = GET_UINT24(data, 7U);                                               // Source RSI

    if (hasMN) {
        m_bodyOffset = 2U;
        m_messageNumber = GET_UINT16(data, 10U);                                    // Message Number
    }

    switch (m_macType) {
    case KMM_MAC::DES_MAC:
        {
            uint8_t macLength = P25DEF::KMM_DES_MAC_LENGTH;
            
            m_macAlgId = data[m_messageFullLength - 4U];
            m_macKId = GET_UINT16(data, m_messageFullLength - 3U);
            m_macFormat = data[m_messageFullLength - 1U];

            ::memset(m_mac, 0x00U, macLength);
            ::memcpy(m_mac, data + m_messageFullLength - (macLength + 5U), macLength);
        }
        break;

    case KMM_MAC::ENH_MAC:
        {
            uint8_t macLength = P25DEF::KMM_AES_MAC_LENGTH;

            m_macAlgId = data[m_messageFullLength - 4U];
            m_macKId = GET_UINT16(data, m_messageFullLength - 3U);
            m_macFormat = data[m_messageFullLength - 1U];

            ::memset(m_mac, 0x00U, macLength);
            ::memcpy(m_mac, data + m_messageFullLength - (macLength + 5U), macLength);
        }
        break;

    case KMM_MAC::NO_MAC:
        break;

    default:
        ::LogError(LOG_P25, "KMMFrame::decodeHeader(), unknown KMM MAC inventory type value, macType = $%02X", m_macType);
        break;
    }

    return true;
}

/* Internal helper to encode a KMM header. */

void KMMFrame::encodeHeader(uint8_t* data)
{
    assert(data != nullptr);

    data[0U] = m_messageId;                                                         // Message ID
    SET_UINT16(m_messageLength, data, 1U);                                          // Message Length
    m_messageFullLength = m_messageLength + 3U;

    data[3U] = ((m_respKind & 0x03U) << 6U) +                                       // Response Kind
        ((m_messageNumber > 0U) ? 0x20U : 0x00U) +                                  // Message Number Flag
        ((m_macType & 0x03U) << 2U) +                                               // MAC Type
        ((!m_complete) ? 0x01U : 0x00U);                                            // Done Flag

    SET_UINT24(m_dstLlId, data, 4U);                                                // Destination RSI
    SET_UINT24(m_srcLlId, data, 7U);                                                // Source RSI

    if (m_messageNumber > 0U) {
        SET_UINT16(m_messageNumber, data, 10U);                                     // Message Number
        m_bodyOffset = 2U;
    }

    switch (m_macType) {
    case KMM_MAC::DES_MAC:
        ::LogError(LOG_P25, "KMMFrame::decodeHeader(), DES MAC type is not supported, macType = $%02X", m_macType);
        return;

    case KMM_MAC::ENH_MAC:
        {
            uint8_t macLength = P25DEF::KMM_AES_MAC_LENGTH;

            data[m_messageFullLength - 5U] = macLength;
            data[m_messageFullLength - 4U] = m_macAlgId;
            SET_UINT16(m_macKId, data, m_messageFullLength - 3U);
            data[m_messageFullLength - 1U] = m_macFormat;

            ::memcpy(data + m_messageFullLength - (macLength + 5U), m_mac, macLength);
        }
        break;

    case KMM_MAC::NO_MAC:
        break;

    default:
        ::LogError(LOG_P25, "KMMFrame::decodeHeader(), unknown KMM MAC inventory type value, macType = $%02X", m_macType);
        break;
    }
}

/* Internal helper to copy the the class. */

void KMMFrame::copy(const KMMFrame& data)
{
    m_messageId = data.m_messageId;
    m_messageLength = data.m_messageLength;
    m_messageFullLength = data.m_messageFullLength;
    m_respKind = data.m_respKind;
    m_complete = data.m_complete;

    m_messageNumber = data.m_messageNumber;
    m_macAlgId = data.m_macAlgId;
    m_macKId = data.m_macKId;
    m_macType = data.m_macType;
}
