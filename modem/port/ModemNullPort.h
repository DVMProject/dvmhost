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
*   Copyright (C) 2021 by Jonathan Naylor G4KLX
*   Copyright (C) 2021 by Bryan Biedenkapp N2PLL
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
#if !defined(__MODEM_NULL_PORT_H__)
#define __MODEM_NULL_PORT_H__

#include "Defines.h"
#include "modem/port/IModemPort.h"
#include "RingBuffer.h"

namespace modem
{
    namespace port
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      This class implements low-level routines that represent a "null"
        //      modem port.
        // ---------------------------------------------------------------------------

        class HOST_SW_API ModemNullPort : public IModemPort {
        public:
            /// <summary>Initializes a new instance of the ModemNullPort class.</summary>
            ModemNullPort();
            /// <summary>Finalizes a instance of the ModemNullPort class.</summary>
            virtual ~ModemNullPort();

            /// <summary>Opens a connection to the port.</summary>
            virtual bool open();

            /// <summary>Reads data from the port.</summary>
            virtual int read(uint8_t* buffer, uint32_t length);
            /// <summary>Writes data to the port.</summary>
            virtual int write(const uint8_t* buffer, uint32_t length);

            /// <summary>Closes the connection to the port.</summary>
            virtual void close();

        private:
            RingBuffer<unsigned char> m_buffer;

            /// <summary>Helper to return a faked modem version.</summary>
            void getVersion();
            /// <summary>Helper to return a faked modem status.</summary>
            void getStatus();
            /// <summary>Helper to write a faked modem acknowledge.</summary>
            void writeAck(uint8_t type);
        };
    } // namespace port
} // namespace modem

#endif // __MODEM_NULL_PORT_H__
