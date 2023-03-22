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
*   Copyright (C) 2002-2004,2007-2009,2011-2013,2015-2017,2020,2021 by Jonathan Naylor G4KLX
*   Copyright (C) 1999-2001 by Thomas Sailor HB9JNX
*   Copyright (C) 2020-2021 by Bryan Biedenkapp N2PLL
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
#if !defined(__UART_PORT_H__)
#define __UART_PORT_H__

#include "Defines.h"
#include "modem/port/IModemPort.h"
#include "modem/port/ISerialPort.h"

#include <string>

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace modem
{
    namespace port
    {
        // ---------------------------------------------------------------------------
        //  Constants
        // ---------------------------------------------------------------------------

        enum SERIAL_SPEED {
            SERIAL_1200 = 1200,
            SERIAL_2400 = 2400,
            SERIAL_4800 = 4800,
            SERIAL_9600 = 9600,
            SERIAL_19200 = 19200,
            SERIAL_38400 = 38400,
            SERIAL_76800 = 76800,
            SERIAL_115200 = 115200,
            SERIAL_230400 = 230400,
            SERIAL_460800 = 460800
        };

        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      This class implements low-level routines to communicate over a RS232
        //      serial port.
        // ---------------------------------------------------------------------------

        class HOST_SW_API UARTPort : public ISerialPort, public IModemPort {
        public:
            /// <summary>Initializes a new instance of the UARTPort class.</summary>
            UARTPort(const std::string& device, SERIAL_SPEED speed, bool assertRTS = false);
            /// <summary>Finalizes a instance of the UARTPort class.</summary>
            virtual ~UARTPort();

            /// <summary>Opens a connection to the serial port.</summary>
            virtual bool open();

            /// <summary>Reads data from the serial port.</summary>
            virtual int read(uint8_t* buffer, uint32_t length);
            /// <summary>Writes data to the serial port.</summary>
            virtual int write(const uint8_t* buffer, uint32_t length);

            /// <summary>Closes the connection to the serial port.</summary>
            virtual void close();

#if defined(__APPLE__)
            /// <summary></summary>
            virtual int setNonblock(bool nonblock);
#endif

        protected:
            /// <summary>Initializes a new instance of the UARTPort class.</summary>
            UARTPort(SERIAL_SPEED speed, bool assertRTS = false);

            bool m_isOpen;

            std::string m_device;
            SERIAL_SPEED m_speed;
            bool m_assertRTS;
#if defined(_WIN32) || defined(_WIN64)
            HANDLE m_handle;
#else
            int m_fd;
#endif

#if defined(_WIN32) || defined(_WIN64)
            /// <summary></summary>
            int readNonblock(uint8_t * buffer, uint32_t length);
#else
            /// <summary></summary>
            bool canWrite();

            /// <summary></summary>
            bool setTermios();
#endif
        }; // class HOST_SW_API UARTPort : public ISerialPort, public IModemPort
    } // namespace port
} // namespace Modem

#endif // __UART_PORT_H__
