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
    case KMM_MessageType::INVENTORY_RSP:
        {
            std::unique_ptr<KMMFrame> frame = decode(new KMMInventoryResponseHeader(), data);
            if (frame == nullptr) {
                break;
            }

            KMMInventoryResponseHeader* header = static_cast<KMMInventoryResponseHeader*>(frame.get());
            if (header == nullptr) {
                break;
            }

            switch (header->getInventoryType()) {
            case KMM_InventoryType::LIST_ACTIVE_KEYSET_IDS:
            case KMM_InventoryType::LIST_INACTIVE_KEYSET_IDS:
                return decode(new KMMInventoryResponseListKeysets(), data);
            case KMM_InventoryType::LIST_ACTIVE_KEY_IDS:
            case KMM_InventoryType::LIST_INACTIVE_KEY_IDS:
                return decode(new KMMInventoryResponseListKeyIDs(), data);

            default:
                LogError(LOG_P25, "KMMFactory::create(), unknown KMM inventory type value, inventoryType = $%02X", header->getInventoryType());
                break;
            }
        }
        break;
    case KMM_MessageType::MODIFY_KEY_CMD:
        return decode(new KMMModifyKey(), data);
    case KMM_MessageType::NAK:
        return decode(new KMMNegativeAck(), data);
    case KMM_MessageType::NO_SERVICE:
        return decode(new KMMNoService(), data);
    case KMM_MessageType::ZEROIZE_CMD:
    case KMM_MessageType::ZEROIZE_RSP:
        return decode(new KMMZeroize(), data);
    case KMM_MessageType::DEREG_CMD:
        return decode(new KMMDeregistrationCommand(), data);
    case KMM_MessageType::DEREG_RSP:
        return decode(new KMMDeregistrationResponse(), data);
    case KMM_MessageType::REG_CMD:
        return decode(new KMMRegistrationCommand(), data);
    case KMM_MessageType::REG_RSP:
        return decode(new KMMRegistrationResponse(), data);
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
