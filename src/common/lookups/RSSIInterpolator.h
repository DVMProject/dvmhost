// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
 *
 */
/**
 * @file RSSIInterpolator.h
 * @ingroup lookups
 * @file RSSIInterpolator.cpp
 * @ingroup lookups
 */
#if !defined(__RSSI_INTERPOLATOR_H__)
#define __RSSI_INTERPOLATOR_H__

#include "common/Defines.h"

#include <cstdint>
#include <string>
#include <map>

namespace lookups
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //
    // ---------------------------------------------------------------------------

    /**
     * @brief RSSI interpolated lookup for RSSI values returned from the modem.
     * @ingroup lookups
     */
    class HOST_SW_API RSSIInterpolator {
    public:
        /**
         * @brief Initializes a new instance of the RSSIInterpolator class.
         */
        RSSIInterpolator();
        /**
         * @brief Finalizes a instance of the RSSIInterpolator class.
         */
        ~RSSIInterpolator();

        /**
         * @brief Loads the table from the passed RSSI mapping file.
         * @param filename Full-path to the RSSI mapping file.
         * @returns bool True, if RSSI mapping was loaded, otherwise false.
         */
        bool load(const std::string& filename);

        /**
         * @brief Interpolates the given raw RSSI value with the lookup map.
         * @param raw Raw RSSI value from modem DSP.
         * @returns int Interpolated RSSI value.
         */
        int interpolate(uint16_t raw) const;

    private:
        std::map<uint16_t, int> m_map;
    };
} // namespace lookups

#endif // __RSSI_INTERPOLATOR_H__
