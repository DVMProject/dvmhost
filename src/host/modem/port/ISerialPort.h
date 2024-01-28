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
*   Copyright (C) 2016,2021 Jonathan Naylor, G4KLX
*
*/
#if !defined(__I_SERIAL_PORT_H__)
#define __I_SERIAL_PORT_H__

#include "Defines.h"

namespace modem
{
    namespace port
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Defines a serial port.
        // ---------------------------------------------------------------------------

        class HOST_SW_API ISerialPort {
        public:
            /// <summary>Finalizes a instance of the ISerialPort class.</summary>
            virtual ~ISerialPort() = 0;

            /// <summary>Opens a connection to the port.</summary>
            virtual bool open() = 0;

            /// <summary>Reads data from the port.</summary>
            virtual int read(uint8_t* buffer, uint32_t length) = 0;
            /// <summary>Writes data to the port.</summary>
            virtual int write(const uint8_t* buffer, uint32_t length) = 0;

            /// <summary>Closes the connection to the port.</summary>
            virtual void close() = 0;
        };
    } // namespace port
} // namespace modem

#endif // __I_SERIAL_PORT_H__
