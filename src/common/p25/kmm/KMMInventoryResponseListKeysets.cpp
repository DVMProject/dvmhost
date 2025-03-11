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
#include "p25/kmm/KMMInventoryResponseListKeysets.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::kmm;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the KMMInventoryResponseListKeysets class. */

KMMInventoryResponseListKeysets::KMMInventoryResponseListKeysets() : KMMInventoryResponseHeader(),
    m_keysetIds()
{
    m_messageId = KMM_MessageType::INVENTORY_RSP;
    m_inventoryType = KMM_InventoryType::LIST_ACTIVE_KEYSET_IDS;
    m_respKind = KMM_ResponseKind::IMMEDIATE;
}

/* Finalizes a instance of the KMMInventoryResponseListKeysets class. */

KMMInventoryResponseListKeysets::~KMMInventoryResponseListKeysets() = default;

/* Gets the byte length of this KMMInventoryResponseListKeysets. */

uint32_t KMMInventoryResponseListKeysets::length() const
{
    uint32_t len = KMM_INVENTORY_RSP_HDR_LENGTH;
    len += m_keysetIds.size();

    return len;
}

/* Decode a KMM inventory response header. */

bool KMMInventoryResponseListKeysets::decode(const uint8_t* data)
{
    assert(data != nullptr);

    KMMInventoryResponseHeader::decodeHeader(data);

    for (uint16_t i = 0U; i < m_numberOfItems; i++) {
        uint8_t keysetId = data[13U + i];
        m_keysetIds.push_back(keysetId);
    }

    return true;
}

/* Encode a KMM inventory response header. */

void KMMInventoryResponseListKeysets::encode(uint8_t* data)
{
    assert(data != nullptr);
    m_messageLength = KMM_INVENTORY_RSP_HDR_LENGTH;
    m_numberOfItems = (uint16_t)m_keysetIds.size();

    KMMInventoryResponseHeader::encodeHeader(data);

    uint16_t offset = 0U;
    for (auto entry : m_keysetIds) {
        data[13U + offset] = entry;
        offset += 1U;
    }
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void KMMInventoryResponseListKeysets::copy(const KMMInventoryResponseListKeysets& data)
{
    KMMInventoryResponseHeader::copy(data);

    m_keysetIds = data.m_keysetIds;
}
