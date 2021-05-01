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
*   Copyright (C) 2015,2016 by Jonathan Naylor G4KLX
*   Copyright (C) 2021 Bryan Biedenkapp N2PLL
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
#if !defined(__DMR_LC__FULL_LC_H__)
#define __DMR_LC__FULL_LC_H__

#include "Defines.h"
#include "dmr/lc/LC.h"
#include "dmr/lc/PrivacyLC.h"
#include "dmr/SlotType.h"
#include "edac/BPTC19696.h"

namespace dmr
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents full DMR link control.
        // ---------------------------------------------------------------------------

        class HOST_SW_API FullLC {
        public:
            /// <summary>Initializes a new instance of the FullLC class.</summary>
            FullLC();
            /// <summary>Finalizes a instance of the FullLC class.</summary>
            ~FullLC();

            /// <summary>Decode DMR full-link control data.</summary>
            LC* decode(const uint8_t* data, uint8_t type);
            /// <summary>Encode DMR full-link control data.</summary>
            void encode(const LC& lc, uint8_t* data, uint8_t type);

            /// <summary>Decode DMR privacy control data.</summary>
            PrivacyLC* decodePI(const uint8_t* data);
            /// <summary>Encode DMR privacy control data.</summary>
            void encodePI(const PrivacyLC& lc, uint8_t* data);

        private:
            edac::BPTC19696 m_bptc;
        };
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC__FULL_LC_H__
