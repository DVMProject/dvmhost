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
#include "p25/kmm/KMMFactory.h"
#include "Log.h"
#include "Utils.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::kmm;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the KMMFactory class. */

KMMFactory::KMMFactory() = default;

/* Finalizes a instance of KMMFactory class. */

KMMFactory::~KMMFactory() = default;

/* Create an instance of a KMMFrame. */

std::unique_ptr<KMMFrame> KMMFactory::create(const uint8_t* data)
{
    assert(data != nullptr);

    uint8_t messageId = data[0U];                                                   // Message ID

    switch (messageId) {
    case KMM_MessageType::HELLO:
        return decode(new KMMHello(), data);
    case KMM_MessageType::INVENTORY_CMD:
        return decode(new KMMInventoryCommand(), data);
    case KMM_MessageType::MODIFY_KEY_CMD:
        return decode(new KMMModifyKey(), data);
    default:
        LogError(LOG_P25, "KMMFactory::create(), unknown KMM message ID value, messageId = $%02X", messageId);
        break;
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Decode a KMM frame. */

std::unique_ptr<KMMFrame> KMMFactory::decode(KMMFrame* packet, const uint8_t* data)
{
    assert(packet != nullptr);
    assert(data != nullptr);

    if (!packet->decode(data)) {
        return nullptr;
    }

    return std::unique_ptr<KMMFrame>(packet);
}
