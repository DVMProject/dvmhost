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
#include "p25/kmm/KMMInventoryResponseListKeyIDs.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::kmm;

#include <cassert>

/*
** bryanb: this implementation is naive; it will only process the first returned key ID list
**  and not the subsequent ones
*/

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the KMMInventoryResponseListKeyIDs class. */

KMMInventoryResponseListKeyIDs::KMMInventoryResponseListKeyIDs() : KMMInventoryResponseHeader(),
    m_keysetId(0U),
    m_algId(P25DEF::ALGO_UNENCRYPT),
    m_numberOfKeyIDs(0U),
    m_keyIds()
{
    m_messageId = KMM_MessageType::INVENTORY_RSP;
    m_inventoryType = KMM_InventoryType::LIST_ACTIVE_KEY_IDS;
    m_respKind = KMM_ResponseKind::IMMEDIATE;
}

/* Finalizes a instance of the KMMInventoryResponseListKeyIDs class. */

KMMInventoryResponseListKeyIDs::~KMMInventoryResponseListKeyIDs() = default;

/* Gets the byte length of this KMMInventoryResponseListKeyIDs. */

uint32_t KMMInventoryResponseListKeyIDs::length() const
{
    uint32_t len = KMM_INVENTORY_RSP_HDR_LENGTH + 3U;
    len += m_keyIds.size() * 2U;

    return len;
}

/* Decode a KMM inventory response header. */

bool KMMInventoryResponseListKeyIDs::decode(const uint8_t* data)
{
    assert(data != nullptr);

    KMMInventoryResponseHeader::decodeHeader(data);

    m_keysetId = data[13U];
    m_algId = data[14U];
    m_numberOfKeyIDs = data[15U];

    uint16_t offset = 0U;
    for (uint16_t i = 0U; i < m_numberOfKeyIDs; i++) {
        uint16_t keyId = GET_UINT16(data, 16U + offset);
        m_keyIds.push_back(keyId);
        offset += 2U;
    }

    return true;
}

/* Encode a KMM inventory response header. */

void KMMInventoryResponseListKeyIDs::encode(uint8_t* data)
{
    assert(data != nullptr);
    m_messageLength = KMM_INVENTORY_RSP_HDR_LENGTH;
    m_numberOfItems = 1U; // this is a naive approach...

    KMMInventoryResponseHeader::encodeHeader(data);

    data[13U] = m_keysetId;
    data[14U] = m_algId;
    data[15U] = m_numberOfKeyIDs;

    uint16_t offset = 0U;
    for (auto entry : m_keyIds) {
        SET_UINT16(entry, data, 16U + offset);
        offset += 2U;
    }
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void KMMInventoryResponseListKeyIDs::copy(const KMMInventoryResponseListKeyIDs& data)
{
    KMMInventoryResponseHeader::copy(data);

    m_keysetId = data.m_keysetId;
    m_algId = data.m_algId;
    m_numberOfKeyIDs = data.m_numberOfKeyIDs;

    m_keyIds = data.m_keyIds;
}
