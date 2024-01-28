// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Modem Host Software
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2002-2004,2007-2009,2011-2013,2015-2017,2020,2021 Jonathan Naylor, G4KLX
*   Copyright (C) 1999-2001 Thomas Sailor, HB9JNX
*   Copyright (C) 2020-2021 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__UART_PORT_H__)
#define __UART_PORT_H__

#include "Defines.h"
#include "modem/port/IModemPort.h"
#include "modem/port/ISerialPort.h"

#include <string>

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
            ~UARTPort() override;

            /// <summary>Opens a connection to the serial port.</summary>
            bool open() override;

            /// <summary>Reads data from the serial port.</summary>
            int read(uint8_t* buffer, uint32_t length) override;
            /// <summary>Writes data to the serial port.</summary>
            int write(const uint8_t* buffer, uint32_t length) override;

            /// <summary>Closes the connection to the serial port.</summary>
            void close() override;

#if defined(__APPLE__)
            /// <summary></summary>
            int setNonblock(bool nonblock);
#endif

        protected:
            /// <summary>Initializes a new instance of the UARTPort class.</summary>
            UARTPort(SERIAL_SPEED speed, bool assertRTS = false);

            bool m_isOpen;

            std::string m_device;
            SERIAL_SPEED m_speed;
            bool m_assertRTS;
            int m_fd;

            /// <summary></summary>
            bool canWrite();

            /// <summary></summary>
            bool setTermios();
        }; // class HOST_SW_API UARTPort : public ISerialPort, public IModemPort
    } // namespace port
} // namespace Modem

#endif // __UART_PORT_H__
