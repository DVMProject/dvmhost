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
 * @file KMMFactory.h
 * @ingroup p25_kmm
 * @file KMMFactory.cpp
 * @ingroup p25_kmm
 */
#if !defined(__P25_KMM__KMM_FACTORY_H__)
#define  __P25_KMM__KMM_FACTORY_H__

#include "common/Defines.h"

#include "common/p25/kmm/KeysetItem.h"

#include "common/p25/kmm/KMMFrame.h"
#include "common/p25/kmm/KMMDeregistrationCommand.h"
#include "common/p25/kmm/KMMDeregistrationResponse.h"
#include "common/p25/kmm/KMMHello.h"
#include "common/p25/kmm/KMMInventoryCommand.h"
#include "common/p25/kmm/KMMInventoryResponseHeader.h"
#include "common/p25/kmm/KMMInventoryResponseListKeysets.h"
#include "common/p25/kmm/KMMInventoryResponseListKeyIDs.h"
#include "common/p25/kmm/KMMModifyKey.h"
#include "common/p25/kmm/KMMNegativeAck.h"
#include "common/p25/kmm/KMMNoService.h"
#include "common/p25/kmm/KMMRegistrationCommand.h"
#include "common/p25/kmm/KMMRegistrationResponse.h"
#include "common/p25/kmm/KMMZeroize.h"

namespace p25
{
    namespace kmm
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Helper class to instantiate an instance of a KMM frame packet.
         * @ingroup p25_kmm
         */
        class HOST_SW_API KMMFactory {
        public:
            /**
             * @brief Initializes a new instance of the KMMFactory class.
             */
            KMMFactory();
            /**
             * @brief Finalizes a instance of the KMMFactory class.
             */
            ~KMMFactory();

            /**
             * @brief Create an instance of a KMMFrame.
             * @param[in] data Buffer containing KMM frame packet data to decode.
             * @returns KMMFrame* Instance of a KMMFrame representing the decoded data.
             */
            static std::unique_ptr<KMMFrame> create(const uint8_t* data);

        private:
            /**
             * @brief Decode a KMM frame.
             * @param packet Instance of KMMFrame.
             * @param[in] data Buffer containing KMM frame packet data to decode.
             * @returns KMMFrame* Instance of a KMMFrame representing the decoded data.
             */
            static std::unique_ptr<KMMFrame> decode(KMMFrame* packet, const uint8_t* data);
        };
    } // namespace kmm
} // namespace p25

#endif // __P25_KMM__KMM_FACTORY_H__
