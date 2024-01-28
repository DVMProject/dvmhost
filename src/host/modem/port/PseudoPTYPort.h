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
*   Copyright (C) 2020,2021 Jonathan Naylor, G4KLX
*
*/
#if !defined(__PSEUDO_PTY_PORT_H__)
#define __PSEUDO_PTY_PORT_H__

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
            ~PseudoPTYPort() override;

            /// <summary>Opens a connection to the serial port.</summary>
            bool open() override;

            /// <summary>Closes the connection to the serial port.</summary>
            void close() override;

        protected:
            std::string m_symlink;
        }; // class HOST_SW_API PseudoPTYPort : public UARTPort
    } // namespace port
} // namespace modem

#endif // __PSEUDO_PTY_PORT_H__
