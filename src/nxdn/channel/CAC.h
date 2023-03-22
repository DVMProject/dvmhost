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
#if !defined(__NXDN_CHANNEL__CAC_H__)
#define  __NXDN_CHANNEL__CAC_H__

#include "Defines.h"

namespace nxdn
{
    namespace channel
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements NXDN Common Access Channel.
        // ---------------------------------------------------------------------------

        class HOST_SW_API CAC {
        public:
            /// <summary>Initializes a new instance of the CAC class.</summary>
            CAC();
            /// <summary>Initializes a copy instance of the CAC class.</summary>
            CAC(const CAC& data);
            /// <summary>Finalizes a instance of the CAC class.</summary>
            ~CAC();

            /// <summary>Equals operator.</summary>
            CAC& operator=(const CAC& data);

            /// <summary>Decode a common access channel.</summary>
            bool decode(const uint8_t* data);
            /// <summary>Encode a common access channel.</summary>
            void encode(uint8_t* data) const;

            /// <summary>Gets the raw CAC data.</summary>
            void getData(uint8_t* data) const;
            /// <summary>Sets the raw CAC data.</summary>
            void setData(const uint8_t* data);

        public:
            /** Common Data */
            /// <summary>Radio Access Number</summary>
            __PROPERTY(uint8_t, ran, RAN);
            /// <summary></summary>
            __PROPERTY(uint8_t, structure, Structure);

            /** Collision Control Field */
            /// <summary>Idle/Busy.</summary>
            __PROPERTY(bool, idleBusy, IdleBusy);
            /// <summary>Tx Continuously.</summary>
            __PROPERTY(bool, txContinuous, TxContinuous);
            /// <summary>Receive/No Receive.</summary>
            __PROPERTY(bool, receive, Receive);

        private:
            uint8_t* m_data;
            uint16_t m_rxCRC;

            /// <summary>Internal helper to copy the class.</summary>
            void copy(const CAC& data);
        };
    } // namespace channel
} // namespace nxdn

#endif // __NXDN_CHANNEL__CAC_H__
