// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022-2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file CSBK_BROADCAST.h
 * @ingroup dmr_csbk
 * @file CSBK_BROADCAST.cpp
 * @ingroup dmr_csbk
 */
#if !defined(__DMR_LC_CSBK__CSBK_BROADCAST_H__)
#define  __DMR_LC_CSBK__CSBK_BROADCAST_H__

#include "common/Defines.h"
#include "common/dmr/lc/CSBK.h"

namespace dmr
{
    namespace lc
    {
        namespace csbk
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief Implements BCAST - Announcement PDUs
             * @ingroup dmr_csbk
             */
            class HOST_SW_API CSBK_BROADCAST : public CSBK {
            public:
                /**
                 * @brief Initializes a new instance of the CSBK_BROADCAST class.
                 */
                CSBK_BROADCAST();

                /**
                 * @brief Decodes a control signalling block.
                 * @param[in] data Buffer containing a CSBK to decode.
                 */
                bool decode(const uint8_t* data) override;
                /**
                 * @brief Encodes a control signalling block.
                 * @param[out] data Buffer to encode a CSBK.
                 */
                void encode(uint8_t* data) override;

                /**
                 * @brief Returns a string that represents the current CSBK.
                 * @returns std::string String representation of the CSBK.
                 */
                std::string toString() override;

            public:
                /**
                 * @brief Broadcast Announcment Type.
                 */
                __PROPERTY(uint8_t, anncType, AnncType);
                /**
                 * @brief Broadcast Hibernation Flag.
                 */
                __PROPERTY(bool, hibernating, Hibernating);

                /**
                 * @brief Broadcast Announce/Withdraw Channel 1 Flag.
                 */
                __PROPERTY(bool, annWdCh1, AnnWdCh1);
                /**
                 * @brief Broadcast Announce/Withdraw Channel 2 Flag.
                 */
                __PROPERTY(bool, annWdCh2, AnnWdCh2);

                /**
                 * @brief Require Registration.
                 */
                __PROPERTY(bool, requireReg, RequireReg);
                /**
                 * @brief System Identity.
                 */
                __PROPERTY(uint32_t, systemId, SystemId);

                /**
                 * @brief Backoff Number.
                 */
                __PROPERTY(uint8_t, backoffNo, BackoffNo);

                __COPY(CSBK_BROADCAST);
            };
        } // namespace csbk
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC_CSBK__CSBK_BROADCAST_H__
