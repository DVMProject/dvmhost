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
 * @file MAC_UU_VCH_USER.h
 * @ingroup p25_mac
 * @file MAC_UU_VCH_USER.cpp
 * @ingroup p25_mac
 */
#if !defined(__P25_LC_MAC__MAC_UU_VCH_USER_H__)
#define __P25_LC_MAC__MAC_UU_VCH_USER_H__

#include "common/p25/lc/MACPDU.h"

namespace p25
{
    namespace lc
    {
        namespace mac
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            // ---------------------------------------------------------------------------

            /** 
             * @brief Phase 2 unit-to-unit voice channel user MAC PDU. 
             */
            class DVM_COMMON_API MAC_UU_VCH_USER : public MACPDU {
            public:
                /**
                 * @brief Initializes a new instance of the MAC_UU_VCH_USER class.
                 */
                MAC_UU_VCH_USER();

                /**
                 * @brief Decodes a MAC PDU from the specified LC control data.
                 * @param control The LC control data to decode from.
                 * @returns bool True, if the MAC PDU was decoded, otherwise false.
                 */
                bool decode(const LC& control) override;

                /**
                 * @brief Returns a string that represents the current MAC PDU.
                 * @returns std::string String representation of the MAC PDU.
                 */
                std::string toString(bool isp = false);
            };
        }
    }
}

#endif // __P25_LC_MAC__MAC_UU_VCH_USER_H__