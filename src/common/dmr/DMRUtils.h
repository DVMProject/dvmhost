// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2021,2024,2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file DMRUtils.h
 * @ingroup dmr
 */
#if !defined(__DMR_UTILS_H__)
#define __DMR_UTILS_H__

#include "common/Defines.h"
#include "common/dmr/DMRDefines.h"

namespace dmr
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief This class implements various helper functions for validating DMR data.
     * @ingroup dmr
     */
    class HOST_SW_API DMRUtils {
    public:
        /**
         * @brief Helper to test and clamp a DMR color code.
         * @param colorCode Color Code
         * @returns uint32_t Clamped color code.
         */
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

        /**
         * @brief Helper to test and clamp a DMR site ID.
         * @param id Site ID
         * @param siteModel Site Model
         * @returns uint32_t Clamped site ID.
         */
        static uint32_t siteId(uint32_t id, defines::SiteModel::E siteModel)
        {
            using namespace dmr::defines;
            if (id > 0U) {
                id--;
            }

            switch (siteModel)
            {
            case SiteModel::SM_TINY:
            {
                if (id > 0x07U) { // clamp to $7
                    id = 0x07U;
                }
            }
            break;
            case SiteModel::SM_SMALL:
            {
                if (id > 0x1FU) { // clamp to $1F
                    id = 0x1FU;
                }
            }
            break;
            case SiteModel::SM_LARGE:
            {
                if (id > 0x7FU) { // clamp to $7F
                    id = 0x7FU;
                }
            }
            break;
            case SiteModel::SM_HUGE:
            {
                if (id > 0x3FFU) { // clamp to $3FF
                    id = 0x3FFU;
                }
            }
            break;
            }

            return id;
        }

        /**
         * @brief Helper to test and clamp a DMR network ID.
         * @param id Network ID
         * @param siteModel Site Model
         * @returns uint32_t Clamped network ID.
         */
        static uint32_t netId(uint32_t id, defines::SiteModel::E siteModel)
        {
            using namespace dmr::defines;
            switch (siteModel) {
            case SiteModel::SM_TINY:
            {
                if (id > 0x1FFU) { // clamp to $1FF
                    id = 0x1FFU;
                }
            }
            break;
            case SiteModel::SM_SMALL:
            {
                if (id > 0x7FU) { // clamp to $7F
                    id = 0x7FU;
                }
            }
            break;
            case SiteModel::SM_LARGE:
            {
                if (id > 0x1FU) { // clamp to $1F
                    id = 0x1FU;
                }
            }
            break;
            case SiteModel::SM_HUGE:
            {
                if (id > 0x03U) { // clamp to $3
                    id = 0x03U;
                }
            }
            break;
            }

            return id;
        }

        /**
         * @brief Helper to convert a reason code to a string.
         * @param reason Reason code.
         * @returns std::string Reason code string.
         */
        static std::string rsnToString(uint8_t reason);
    };
} // namespace dmr

#endif // __DMR_UTILS_H__
