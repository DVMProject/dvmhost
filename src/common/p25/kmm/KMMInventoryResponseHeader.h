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
 * @file KMMInventoryResponseHeader.h
 * @ingroup p25_kmm
 * @file KMMInventoryResponseHeader.cpp
 * @ingroup p25_kmm
 */
#if !defined(__P25_KMM__KMM_INVENTORY_RESPONSE_HDR_H__)
#define  __P25_KMM__KMM_INVENTORY_RESPONSE_HDR_H__

#include "common/Defines.h"
#include "common/p25/kmm/KMMFrame.h"
#include "common/Utils.h"

#include <string>
#include <vector>

namespace p25
{
    namespace kmm
    {
        // ---------------------------------------------------------------------------
        //  Constants
        // ---------------------------------------------------------------------------

        /**
         * @addtogroup p25_kmm
         * @{
         */

         const uint32_t KMM_INVENTORY_RSP_HDR_LENGTH = KMM_FRAME_LENGTH + 3U;

         /** @} */
 
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        class HOST_SW_API KMMInventoryResponseHeader : public KMMFrame {
        public:
            /**
             * @brief Initializes a new instance of the KMMInventoryResponseHeader class.
             */
            KMMInventoryResponseHeader();
            /**
             * @brief Finalizes a instance of the KMMInventoryResponseHeader class.
             */
            ~KMMInventoryResponseHeader();

            /**
             * @brief Decode a KMM inventory response header.
             * @param[in] data Buffer containing KMM frame data to decode.
             * @returns bool True, if decoded, otherwise false.
             */
            bool decode(const uint8_t* data) override;
            /**
             * @brief Encode a KMM inventory reponse header.
             * @param[out] data Buffer to encode KMM frame data to.
             */
            void encode(uint8_t* data) override;

        public:
            /**
             * @brief Inventory type.
             */
            DECLARE_PROTECTED_PROPERTY(uint8_t, inventoryType, InventoryType);
            /**
             * @brief Number of items in the inventory response.
             */
            DECLARE_PROTECTED_PROPERTY(uint16_t, numberOfItems, NumberOfItems);

            DECLARE_PROTECTED_COPY(KMMInventoryResponseHeader);
        };
    } // namespace kmm
} // namespace p25

#endif // __P25_KMM__KMM_INVENTORY_RESPONSE_HDR_H__
