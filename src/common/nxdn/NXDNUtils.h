// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2020 Jonathan Naylor, G4KLX
 *  Copyright (C) 2022,2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file NXDNUtils.h
 * @ingroup nxdn
 * @file NXDNUtils.cpp
 * @ingroup nxdn
 */
#if !defined(__NXDN_UTILS_H__)
#define __NXDN_UTILS_H__

#include "common/Defines.h"

namespace nxdn
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief This class implements various helper functions for scrambling NXDN data.
     * @ingroup nxdn
     */
    class HOST_SW_API NXDNUtils {
    public:
        /**
         * @brief Helper to scramble the NXDN frame data.
         * @param data Buffer to apply scrambling to.
         */
        static void scrambler(uint8_t* data);
        /**
         * @brief Helper to add the post field bits on NXDN frame data.
         * @param data Buffer to apply post field bits to.
         */
        static void addPostBits(uint8_t* data);

        /**
         * @brief Helper to convert a cause code to a string.
         * @param cause Cause code.
         * @returns std::string Cause code string.
         */
        static std::string causeToString(uint8_t cause);
    };
} // namespace nxdn

#endif // __NXDN_UTILS_H__
