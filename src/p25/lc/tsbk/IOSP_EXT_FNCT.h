/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
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
#if !defined(__P25_LC_TSBK__IOSP_EXT_FNCT_H__)
#define  __P25_LC_TSBK__IOSP_EXT_FNCT_H__

#include "Defines.h"
#include "p25/lc/TSBK.h"

namespace p25
{
    namespace lc
    {
        namespace tsbk
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      Implements EXT FNCT RSP - Extended Function Response (ISP) and
            //          EXT FNCT CMD - Extended Function Command (OSP)
            // ---------------------------------------------------------------------------

            class HOST_SW_API IOSP_EXT_FNCT : public TSBK {
            public:
                /// <summary>Initializes a new instance of the IOSP_EXT_FNCT class.</summary>
                IOSP_EXT_FNCT();

                /// <summary>Decode a trunking signalling block.</summary>
                bool decode(const uint8_t* data, bool rawTSBK = false);
                /// <summary>Encode a trunking signalling block.</summary>
                void encode(uint8_t* data, bool rawTSBK = false, bool noTrellis = false);

                /// <summary>Returns a string that represents the current TSBK.</summary>
                virtual std::string toString(bool isp = false) override;

            public:
                /// <summary>Extended function opcode.</summary>
                __PROPERTY(uint32_t, extendedFunction, ExtendedFunction);

                __COPY(IOSP_EXT_FNCT);
            };
        } // namespace tsbk
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__IOSP_EXT_FNCT_H__
