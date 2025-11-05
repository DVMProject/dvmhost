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
#include "p25/kmm/KMMInventoryResponseHeader.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::kmm;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the KMMInventoryResponseHeader class. */

KMMInventoryResponseHeader::KMMInventoryResponseHeader() : KMMFrame(),
    m_inventoryType(KMM_InventoryType::NULL_INVENTORY)
{
    m_messageId = KMM_MessageType::INVENTORY_RSP;
    m_respKind = KMM_ResponseKind::IMMEDIATE;
}

/* Finalizes a instance of the KMMInventoryResponseHeader class. */

KMMInventoryResponseHeader::~KMMInventoryResponseHeader() = default;

/* Gets the byte length of this KMMInventoryResponseHeader. */

uint32_t KMMInventoryResponseHeader::length() const
{
    return KMMFrame::length() + KMM_BODY_INV_RSP_HDR_LENGTH;
}

/* Decode a KMM inventory response header. */

bool KMMInventoryResponseHeader::decode(const uint8_t* data)
{
    assert(data != nullptr);

    KMMFrame::decodeHeader(data);

    m_inventoryType = data[10U + m_bodyOffset];                 // Inventory Type
    m_numberOfItems = GET_UINT16(data, 11U + m_bodyOffset);     // Number of Items

    return true;
}

/* Encode a KMM inventory response header. */

void KMMInventoryResponseHeader::encode(uint8_t* data)
{
    assert(data != nullptr);
    m_messageLength = length();

    KMMFrame::encodeHeader(data);

    data[10U + m_bodyOffset] = m_inventoryType;                 // Inventory Type
    SET_UINT16(m_numberOfItems, data, 11U + m_bodyOffset);      // Number of Items
}

/* Returns a string that represents the current KMM frame. */

std::string KMMInventoryResponseHeader::toString()
{
    return std::string("KMM, INVENTORY_RSP (Inventory Response)");
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void KMMInventoryResponseHeader::copy(const KMMInventoryResponseHeader& data)
{
    KMMFrame::copy(data);

    m_inventoryType = data.m_inventoryType;
    m_numberOfItems = data.m_numberOfItems;
}
