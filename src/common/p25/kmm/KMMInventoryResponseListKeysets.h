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
 * @file KMMInventoryResponseListKeysets.h
 * @ingroup p25_kmm
 * @file KMMInventoryResponseListKeysets.cpp
 * @ingroup p25_kmm
 */
#if !defined(__P25_KMM__KMM_INVENTORY_RESPONSE_LIST_KEYSETS_H__)
#define  __P25_KMM__KMM_INVENTORY_RESPONSE_LIST_KEYSETS_H__

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

        class HOST_SW_API KMMInventoryResponseListKeysets : public KMMInventoryResponseHeader {
        public:
            /**
             * @brief Initializes a new instance of the KMMInventoryResponseListKeysets class.
             */
            KMMInventoryResponseListKeysets();
            /**
             * @brief Finalizes a instance of the KMMInventoryResponseListKeysets class.
             */
            ~KMMInventoryResponseListKeysets();

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
            DECLARE_PROPERTY(std::vector<uint8_t>, keysetIds, KeysetIds);

            DECLARE_COPY(KMMInventoryResponseListKeysets);
        };
    } // namespace kmm
} // namespace p25

#endif // __P25_KMM__KMM_INVENTORY_RESPONSE_LIST_KEYSETS_H__
