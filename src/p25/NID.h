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
*   Copyright (C) 2017,2022 by Bryan Biedenkapp N2PLL
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
#if !defined(__P25_NID_H__)
#define  __P25_NID_H__

#include "Defines.h"

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Represents the P25 network identifier.
    // ---------------------------------------------------------------------------

    class HOST_SW_API NID {
    public:
        /// <summary>Initializes a new instance of the NID class.</summary>
        NID(uint32_t nac);
        /// <summary>Finalizes a instance of the NID class.</summary>
        ~NID();

        /// <summary>Decodes P25 network identifier data.</summary>
        bool decode(const uint8_t* data);
        /// <summary>Encodes P25 network identifier data.</summary>
        void encode(uint8_t* data, uint8_t duid) const;

        /// <summary>Helper to configure a separate Tx NAC.</summary>
        void setTxNAC(uint32_t nac);

    public:
        /// <summary>Data unit ID.</summary>
        __READONLY_PROPERTY(uint8_t, duid, DUID);

    private:
        uint32_t m_nac;

        uint8_t** m_rxTx;
        uint8_t** m_tx;

        bool m_splitNac;

        /// <summary></summary>
        void createRxTxNID(uint32_t nac);
        /// <summary></summary>
        void createTxNID(uint32_t nac);
    };
} // namespace p25

#endif // __P25_NID_H__
