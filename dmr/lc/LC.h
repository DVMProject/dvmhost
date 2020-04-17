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
#if !defined(__DMR_LC__LC_H__)
#define __DMR_LC__LC_H__

#include "Defines.h"
#include "dmr/DMRDefines.h"

namespace dmr
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents DMR link control data.
        // ---------------------------------------------------------------------------

        class HOST_SW_API LC {
        public:
            /// <summary>Initializes a new instance of the LC class.</summary>
            LC(uint8_t flco, uint32_t srcId, uint32_t dstId);
            /// <summary>Initializes a new instance of the LC class.</summary>
            LC(const uint8_t* bytes);
            /// <summary>Initializes a new instance of the LC class.</summary>
            LC(const bool* bits);
            /// <summary>Initializes a new instance of the LC class.</summary>
            LC();
            /// <summary>Finalizes a instance of the LC class.</summary>
            ~LC();

            /// <summary></summary>
            void getData(uint8_t* bytes) const;
            /// <summary></summary>
            void getData(bool* bits) const;

            /// <summary></summary>
            bool getOVCM() const;
            /// <summary></summary>
            void setOVCM(bool ovcm);
        public:
            /// <summary>Flag indicating whether link protection is enabled.</summary>
            __PROPERTY(bool, PF, PF);

            /// <summary>Full-link control opcode.</summary>
            __PROPERTY(uint8_t, FLCO, FLCO);

            /// <summary>CSBK feature ID.</summayr>
            __PROPERTY(uint8_t, FID, FID);

            /// <summary>Sets the source ID.</summary>
            __PROPERTY(uint32_t, srcId, SrcId);
            /// <summary>Sets the destination ID.</summary>
            __PROPERTY(uint32_t, dstId, DstId);

        private:
            bool m_R;

            uint8_t m_options;
        };
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC__LC_H__
