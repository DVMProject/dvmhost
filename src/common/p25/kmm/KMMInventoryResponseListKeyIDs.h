// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file KMMInventoryResponseListKeyIDs.h
 * @ingroup p25_kmm
 * @file KMMInventoryResponseListKeyIDs.cpp
 * @ingroup p25_kmm
 */
#if !defined(__P25_KMM__KMM_INVENTORY_RESPONSE_LIST_KEY_IDS_H__)
#define  __P25_KMM__KMM_INVENTORY_RESPONSE_LIST_KEY_IDS_H__

#include "common/Defines.h"
#include "common/p25/kmm/KMMFrame.h"
#include "common/p25/kmm/KMMInventoryResponseHeader.h"
#include "common/Utils.h"

#include <string>
#include <vector>

namespace p25
{
    namespace kmm
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        class HOST_SW_API KMMInventoryResponseListKeyIDs : public KMMInventoryResponseHeader {
        public:
            /**
             * @brief Initializes a new instance of the KMMInventoryResponseListKeyIDs class.
             */
            KMMInventoryResponseListKeyIDs();
            /**
             * @brief Finalizes a instance of the KMMInventoryResponseListKeyIDs class.
             */
            ~KMMInventoryResponseListKeyIDs();

            /**
             * @brief Gets the byte length of this KMMFrame.
             * @return uint32_t Length of KMMFrame.
             */
            uint32_t length() const override;

            /**
             * @brief Decode a KMM inventory response.
             * @param[in] data Buffer containing KMM frame data to decode.
             * @returns bool True, if decoded, otherwise false.
             */
            bool decode(const uint8_t* data) override;
            /**
             * @brief Encode a KMM inventory reponse.
             * @param[out] data Buffer to encode KMM frame data to.
             */
            void encode(uint8_t* data) override;

        public:
            /**
             * @brief 
             */
            __PROPERTY(uint8_t, keysetId, KeysetId);
            /**
             * @brief Encryption algorithm ID.
             */
            __PROPERTY(uint8_t, algId, AlgId);
            /**
             * @brief Encryption algorithm ID.
             */
            __PROPERTY(uint8_t, numberOfKeyIDs, NumberOfKeyIDs);

            /**
             * @brief
             */
            __PROPERTY(std::vector<uint16_t>, keyIds, KeyIds);

            __COPY(KMMInventoryResponseListKeyIDs);
        };
    } // namespace kmm
} // namespace p25

#endif // __P25_KMM__KMM_INVENTORY_RESPONSE_LIST_KEY_IDS_H__
