// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025-2026 Bryan Biedenkapp, N2PLL
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
    case KMM_MessageType::CHANGEOVER_CMD:
    case KMM_MessageType::CHANGEOVER_RSP:
        return decode(new KMMChangeover(), data);
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
    case KMM_MessageType::REKEY_ACK:
        return decode(new KMMRekeyAck(), data);
    case KMM_MessageType::REKEY_CMD:
        return decode(new KMMRekeyCommand(), data);
    case KMM_MessageType::WARM_START_CMD:
        return decode(new KMMWarmStartCommand(), data);
    case KMM_MessageType::UNABLE_TO_DECRYPT:
        return decode(new KMMUnableToDecrypt(), data);
    default:
        LogError(LOG_P25, "KMMFactory::create(), unknown KMM message ID value, messageId = $%02X", messageId);
        break;
    }

    return nullptr;
}

/* Create an instance of a KMMFrame with a specified length. */

std::unique_ptr<KMMFrame> KMMFactory::create(const uint8_t* data, uint32_t len)
{
    if (data == nullptr || len < 10U || len > 512U)
        return nullptr;

    const uint32_t fullLength = (((uint32_t)data[1U] << 8U) | data[2U]) + 3U;
    if (fullLength < 10U || fullLength > len || fullLength > 512U)
        return nullptr;

    const uint8_t mnCode = (data[3U] >> 4U) & 0x03U;
    const uint8_t macType = (data[3U] >> 2U) & 0x03U;
    if ((mnCode != 0U && mnCode != 2U) || macType == 1U)
        return nullptr;

    const uint32_t body = 10U + ((mnCode == 2U) ? 2U : 0U);
    uint32_t bodyEnd = fullLength;
    if (macType == KMM_MAC::DES_MAC) {
        if (bodyEnd < 7U) return nullptr;
        bodyEnd -= 7U;
    } else if (macType == KMM_MAC::ENH_MAC) {
        if (bodyEnd < 13U || data[bodyEnd - 5U] != P25DEF::KMM_AES_MAC_LENGTH)
            return nullptr;
        bodyEnd -= 13U;
    }
    if (body > bodyEnd)
        return nullptr;

    const uint32_t available = bodyEnd - body;
    switch (data[0U]) {
    case KMM_MessageType::CHANGEOVER_CMD:
    case KMM_MessageType::CHANGEOVER_RSP:
        if (available < 1U || data[body] == 0U ||
            available != 1U + (uint32_t)data[body] * 2U)
            return nullptr;
        break;
    case KMM_MessageType::HELLO:
    case KMM_MessageType::INVENTORY_CMD:
    case KMM_MessageType::DEREG_RSP:
    case KMM_MessageType::REG_RSP:
        if (available < 1U)
            return nullptr;
        break;
    case KMM_MessageType::DEREG_CMD:
    case KMM_MessageType::REG_CMD:
        if (available < 4U)
            return nullptr;
        break;
    case KMM_MessageType::NAK:
        if (available < 4U)
            return nullptr;
        break;
    case KMM_MessageType::REKEY_ACK:
        if (available < 2U || available < 2U + (uint32_t)data[body + 1U] * 4U)
            return nullptr;
        break;
    case KMM_MessageType::WARM_START_CMD:
        {
            if (available < 11U || mnCode != 2U || macType != KMM_MAC::ENH_MAC ||
                ((data[3U] >> 6U) & 0x03U) != KMM_ResponseKind::IMMEDIATE)
                return nullptr;
            const uint8_t decryptFormat = data[body];
            if (decryptFormat != KMM_DECRYPT_INSTRUCT_NONE && decryptFormat != KMM_DECRYPT_INSTRUCT_MI)
                return nullptr;
            const uint32_t miLength = decryptFormat == KMM_DECRYPT_INSTRUCT_MI ? MI_LENGTH_BYTES : 0U;
            if (available < 11U + miLength)
                return nullptr;
            const uint32_t keyLengthOffset = body + 4U + miLength;
            const uint8_t keyLength = data[keyLengthOffset];
            const uint32_t keyFormatOffset = keyLengthOffset + 2U;
            const uint16_t sln = (uint16_t)(((uint16_t)data[keyFormatOffset + 1U] << 8U) |
                data[keyFormatOffset + 2U]);
            const uint16_t temporaryKId = (uint16_t)(((uint16_t)data[keyFormatOffset + 3U] << 8U) |
                data[keyFormatOffset + 4U]);
            if (data[body + 1U] != ALGO_AES_256 || keyLength != P25DEF::MAX_WRAPPED_ENC_KEY_LENGTH_BYTES ||
                data[keyLengthOffset + 1U] != ALGO_AES_256 || temporaryKId == 0U ||
                available != 11U + miLength + keyLength ||
                (data[keyFormatOffset] & 0xE0U) != KMM_KEY_FORMAT_TEK ||
                (data[keyFormatOffset] & 0x1FU) != 0U || sln != 0U)
                return nullptr;
        }
        break;
    case KMM_MessageType::NO_SERVICE:
    case KMM_MessageType::ZEROIZE_CMD:
    case KMM_MessageType::ZEROIZE_RSP:
        break;
    case KMM_MessageType::UNABLE_TO_DECRYPT:
        // the optional reverse-warm-start body is not accepted until its full
        // nested length validation is implemented
        if (available < 7U || (data[body] & 0x80U) != 0U)
            return nullptr;
        break;
    default:
        // other decoders contain nested count fields; keep the air-facing path
        // fail-closed until each has a dedicated structural validator
        return nullptr;
    }

    return create(data);
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
