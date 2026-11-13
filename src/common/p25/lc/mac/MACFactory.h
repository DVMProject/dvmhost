// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup p25_mac Phase 2 MAC
 * @brief Implementation for the data handling of the TIA-102.BBAD Project 25 standard (Phase 2 MAC)
 * @ingroup p25_lc
 *
 * @file MACFactory.h
 * @ingroup p25_mac
 * @file MACFactory.cpp
 * @ingroup p25_mac
 */
#if !defined(__P25_LC_MAC__MAC_FACTORY_H__)
#define __P25_LC_MAC__MAC_FACTORY_H__

#include "common/Defines.h"
#include "common/p25/lc/MACPDU.h"
#include "common/p25/lc/mac/MAC_GROUP_VCH_USER.h"
#include "common/p25/lc/mac/MAC_RELEASE.h"
#include "common/p25/lc/mac/MAC_TEL_INT_VCH_USER.h"
#include "common/p25/lc/mac/MAC_UU_VCH_USER.h"

namespace p25
{
    namespace lc
    {
        namespace mac
        {
            /**
             * @brief Helper class to instantiate a Phase 2 MAC PDU.
             * @ingroup p25_lc
             */
            class DVM_COMMON_API MACFactory {
            public:
                /**
                 * @brief Initializes a new instance of the MACFactory class.
                 */
                MACFactory() = default;
                /**
                 * @brief Finalizes a instance of the MACFactory class.
                 */
                ~MACFactory() = default;

                /**
                 * @brief Create and decode a MAC PDU from decoded LC data.
                 * @param control Decoded Phase 2 MAC PDU fields.
                 * @returns MACPDU* Instance representing the decoded opcode.
                 */
                static std::unique_ptr<MACPDU> createMACPDU(const LC& control);

            private:
                /**
                 * @brief Decode a MAC PDU from decoded LC data.
                 * @param mac Instance of MAC PDU to decode into.
                 * @param control Decoded Phase 2 MAC PDU fields.
                 * @returns MACPDU* Instance representing the decoded opcode.
                 */
                static std::unique_ptr<MACPDU> decode(MACPDU* mac, const LC& control);
            };
        }
    }
}

#endif // __P25_LC_MAC__MAC_FACTORY_H__
