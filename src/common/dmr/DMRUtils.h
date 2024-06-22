// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2021,2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__DMR_UTILS_H__)
#define __DMR_UTILS_H__

#include "common/Defines.h"
#include "common/dmr/DMRDefines.h"

namespace dmr
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      This class implements various helper functions for validating DMR data.
    // ---------------------------------------------------------------------------

    class HOST_SW_API DMRUtils {
    public:
        /// <summary>Helper to test and clamp a DMR color code.</summary>
        /// <param name="colorCode">Color Code</param>
        /// <returns>Clamped color code.</returns>
        static uint32_t colorCode(uint32_t colorCode)
        {
            if (colorCode < 0U) { // clamp to 0
                colorCode = 0U;
            }
            if (colorCode > 15U) { // clamp to 15
                colorCode = 15U;
            }

            return colorCode;
        }

        /// <summary>Helper to test and clamp a DMR site ID.</summary>
        /// <param name="id">Site ID</param>
        /// <param name="siteModel">Site Model</param>
        /// <returns>Clamped site ID.</returns>
        static uint32_t siteId(uint32_t id, defines::SiteModel::E siteModel)
        {
            using namespace dmr::defines;
            if (id > 0U) {
                id--;
            }

            switch (siteModel)
            {
            case SiteModel::TINY:
            {
                if (id > 0x07U) { // clamp to $7
                    id = 0x07U;
                }
            }
            break;
            case SiteModel::SMALL:
            {
                if (id > 0x1FU) { // clamp to $1F
                    id = 0x1FU;
                }
            }
            break;
            case SiteModel::LARGE:
            {
                if (id > 0x7FU) { // clamp to $7F
                    id = 0x7FU;
                }
            }
            break;
            case SiteModel::HUGE:
            {
                if (id > 0x3FFU) { // clamp to $3FF
                    id = 0x3FFU;
                }
            }
            break;
            }

            return id;
        }

        /// <summary>Helper to test and clamp a DMR network ID.</summary>
        /// <param name="id">Network ID</param>
        /// <param name="siteModel">Site Model</param>
        /// <returns>Clamped network ID.</returns>
        static uint32_t netId(uint32_t id, defines::SiteModel::E siteModel)
        {
            using namespace dmr::defines;
            switch (siteModel) {
            case SiteModel::TINY:
            {
                if (id > 0x1FFU) { // clamp to $1FF
                    id = 0x1FFU;
                }
            }
            break;
            case SiteModel::SMALL:
            {
                if (id > 0x7FU) { // clamp to $7F
                    id = 0x7FU;
                }
            }
            break;
            case SiteModel::LARGE:
            {
                if (id > 0x1FU) { // clamp to $1F
                    id = 0x1FU;
                }
            }
            break;
            case SiteModel::HUGE:
            {
                if (id > 0x03U) { // clamp to $3
                    id = 0x03U;
                }
            }
            break;
            }

            return id;
        }
    };
} // namespace dmr

#endif // __DMR_UTILS_H__
