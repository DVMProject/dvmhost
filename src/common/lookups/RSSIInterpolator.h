// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2016 Jonathan Naylor, G4KLX
*
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

    class HOST_SW_API RSSIInterpolator {
    public:
        /// <summary>Initializes a new instance of the RSSIInterpolator class.</summary>
        RSSIInterpolator();
        /// <summary>Finalizes a instance of the RSSIInterpolator class.</summary>
        ~RSSIInterpolator();

        /// <summary>Loads the table from the passed RSSI mapping file.</summary>
        bool load(const std::string& filename);

        /// <summary>Interpolates the given raw RSSI value with the lookup map.</summary>
        int interpolate(uint16_t raw) const;

    private:
        std::map<uint16_t, int> m_map;
    };
} // namespace lookups

#endif // __RSSI_INTERPOLATOR_H__
