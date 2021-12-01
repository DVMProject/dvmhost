/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2021 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#if !defined(__DMR_UTILS_H__)
#define __DMR_UTILS_H__

#include "Defines.h"
#include "dmr/DMRDefines.h"

namespace dmr
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      This class implements various helper functions for validating DMR data.
    // ---------------------------------------------------------------------------

    class HOST_SW_API DMRUtils {
    public:
        /// <summary>Helper to test and clamp a DMR color code.</summary>
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
        static uint32_t siteId(uint32_t id, uint8_t siteModel)
        {
            if (id == 0U) { // clamp to 1
                id = 1U;
            }

            switch (siteModel)
            {
            case SITE_MODEL_TINY:
            {
                if (id > 0x07U) { // clamp to $7
                    id = 0x07U;
                }
            }
            break;
            case SITE_MODEL_SMALL:
            {
                if (id > 0x1FU) { // clamp to $1F
                    id = 0x1FU;
                }
            }
            break;
            case SITE_MODEL_LARGE:
            {
                if (id > 0xFFU) { // clamp to $FF
                    id = 0xFFU;
                }
            }
            break;
            case SITE_MODEL_HUGE:
            {
                if (id > 0x7FFU) { // clamp to $7FF
                    id = 0x7FFU;
                }
            }
            break;
            }

            return id;
        }

        /// <summary>Helper to test and clamp a DMR network ID.</summary>
        static uint32_t netId(uint32_t id, uint8_t siteModel)
        {
            if (id == 0U) { // clamp to 1
                id = 1U;
            }

            switch (siteModel) {
            case SITE_MODEL_TINY:
            {
                if (id > 0x1FFU) { // clamp to $1FF
                    id = 0x1FFU;
                }
            }
            break;
            case SITE_MODEL_SMALL:
            {
                if (id > 0x7FU) { // clamp to $7F
                    id = 0x7FU;
                }
            }
            break;
            case SITE_MODEL_LARGE:
            {
                if (id > 0x1FU) { // clamp to $1F
                    id = 0x1FU;
                }
            }
            break;
            case SITE_MODEL_HUGE:
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
