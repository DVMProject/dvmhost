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
*   Copyright (C) 2021 Jonathan Naylor, G4KLX
*   Copyright (C) 2021 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__UDP_PORT_H__)
#define __UDP_PORT_H__

#include "Defines.h"
#include "common/network/UDPSocket.h"
#include "common/RingBuffer.h"
#include "modem/port/IModemPort.h"

#include <string>

namespace modem
{
    namespace port
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      This class implements low-level routines to communicate over UDP.
        // ---------------------------------------------------------------------------

        class HOST_SW_API UDPPort : public IModemPort {
        public:
            /// <summary>Initializes a new instance of the UDPPort class.</summary>
            UDPPort(const std::string& modemAddress, uint16_t modemPort);
            /// <summary>Finalizes a instance of the UDPPort class.</summary>
            ~UDPPort() override;

            /// <summary>Opens a connection to the serial port.</summary>
            bool open() override;

            /// <summary>Reads data from the serial port.</summary>
            int read(uint8_t* buffer, uint32_t length) override;
            /// <summary>Writes data to the serial port.</summary>
            int write(const uint8_t* buffer, uint32_t length) override;

            /// <summary>Closes the connection to the serial port.</summary>
            void close() override;

        protected:
            network::UDPSocket m_socket;

            sockaddr_storage m_addr;
            uint32_t m_addrLen;

            RingBuffer<uint8_t> m_buffer;
        };
    } // namespace port
} // namespace Modem

#endif // __UDP_PORT_H__
