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
*   Copyright (C) 2015,2016,2018,2021 by Jonathan Naylor G4KLX
*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
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
#if !defined(__NXDN_EDAC__CONVOLUTION_H__)
#define  __NXDN_EDAC__CONVOLUTION_H__

#include "Defines.h"

#include <cstdint>

namespace nxdn
{
    namespace edac
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements NXDN frame convolution processing.
        // ---------------------------------------------------------------------------

        class HOST_SW_API Convolution {
        public:
            /// <summary>Initializes a new instance of the Convolution class.</summary>
            Convolution();
            /// <summary>Finalizes a instance of the Convolution class.</summary>
            ~Convolution();

            /// <summary></summary>
            void start();
            /// <summary></summary>
            uint32_t chainback(uint8_t* out, uint32_t nBits);

            /// <summary></summary>
            bool decode(uint8_t s0, uint8_t s1);
            /// <summary></summary>
            void encode(const uint8_t* in, uint8_t* out, uint32_t nBits) const;

        private:
            uint16_t* m_metrics1;
            uint16_t* m_metrics2;

            uint16_t* m_oldMetrics;
            uint16_t* m_newMetrics;

            uint64_t* m_decisions;

            uint64_t* m_dp;
        };
    } // namespace edac
} // namespace nxdn

#endif // __NXDN_EDAC__CONVOLUTION_H__
