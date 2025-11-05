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
#include "p25/kmm/KMMInventoryCommand.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::kmm;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the KMMInventoryCommand class. */

KMMInventoryCommand::KMMInventoryCommand() : KMMFrame(),
    m_inventoryType(KMM_InventoryType::NULL_INVENTORY)
{
    m_messageId = KMM_MessageType::INVENTORY_CMD;
    m_respKind = KMM_ResponseKind::IMMEDIATE;
}

/* Finalizes a instance of the KMMInventoryCommand class. */

KMMInventoryCommand::~KMMInventoryCommand() = default;

/* Gets the byte length of this KMMInventoryCommand. */

uint32_t KMMInventoryCommand::length() const
{
    return KMMFrame::length() + KMM_BODY_INVENTORY_CMD_LENGTH;
}

/* Decode a KMM inventory command. */

bool KMMInventoryCommand::decode(const uint8_t* data)
{
    assert(data != nullptr);

    KMMFrame::decodeHeader(data);

    m_inventoryType = data[10U + m_bodyOffset];                 // Inventory Type

    return true;
}

/* Encode a KMM inventory command. */

void KMMInventoryCommand::encode(uint8_t* data)
{
    assert(data != nullptr);
    m_messageLength = length();

    KMMFrame::encodeHeader(data);

    data[10U + m_bodyOffset] = m_inventoryType;                 // Inventory Type
}

/* Returns a string that represents the current KMM frame. */

std::string KMMInventoryCommand::toString()
{
    return std::string("KMM, INVENTORY_CMD (Inventory Command)");
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void KMMInventoryCommand::copy(const KMMInventoryCommand& data)
{
    KMMFrame::copy(data);

    m_inventoryType = data.m_inventoryType;
}
