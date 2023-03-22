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
 *   Copyright (C) 2020,2021 by Jonathan Naylor G4KLX
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
#if !defined(__PSEUDO_PTY_PORT_H__)
#define __PSEUDO_PTY_PORT_H__

#if !defined(_WIN32) && !defined(_WIN64)

#include "Defines.h"
#include "modem/port/UARTPort.h"

#include <cstring>

namespace modem
{
    namespace port
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      This class implements low-level routines to communicate via a Linux
        //      PTY serial port.
        // ---------------------------------------------------------------------------

        class HOST_SW_API PseudoPTYPort : public UARTPort 
        {
        public:
            /// <summary>Initializes a new instance of the PseudoPTYPort class.</summary>
            PseudoPTYPort(const std::string& symlink, SERIAL_SPEED speed, bool assertRTS = false);
            /// <summary>Finalizes a instance of the PseudoPTYPort class.</summary>
            virtual ~PseudoPTYPort();

            /// <summary>Opens a connection to the serial port.</summary>
            virtual bool open();

            /// <summary>Closes the connection to the serial port.</summary>
            virtual void close();

        protected:
            std::string m_symlink;
        }; // class HOST_SW_API PseudoPTYPort : public UARTPort
    } // namespace port
} // namespace Modem

#endif

#endif // __PSEUDO_PTY_PORT_H__
