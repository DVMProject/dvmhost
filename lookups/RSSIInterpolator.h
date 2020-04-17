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
*   Copyright (C) 2016 by Jonathan Naylor G4KLX
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
#if !defined(__RSSI_INTERPOLATOR_H__)
#define __RSSI_INTERPOLATOR_H__

#include "Defines.h"

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

        /// <summary>Interoplates the given raw RSSI value with the lookup map.</summary>
        int interpolate(uint16_t raw) const;

    private:
        std::map<uint16_t, int> m_map;
    };
} // namespace lookups

#endif // __RSSI_INTERPOLATOR_H__
