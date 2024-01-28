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
#if !defined(__MODEM_NULL_PORT_H__)
#define __MODEM_NULL_PORT_H__

#include "Defines.h"
#include "common/RingBuffer.h"
#include "modem/port/IModemPort.h"

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
            ~ModemNullPort() override;

            /// <summary>Opens a connection to the port.</summary>
            bool open() override;

            /// <summary>Reads data from the port.</summary>
            int read(uint8_t* buffer, uint32_t length) override;
            /// <summary>Writes data to the port.</summary>
            int write(const uint8_t* buffer, uint32_t length) override;

            /// <summary>Closes the connection to the port.</summary>
            void close() override;

        private:
            RingBuffer<unsigned char> m_buffer;

            /// <summary>Helper to return a faked modem version.</summary>
            void getVersion();
            /// <summary>Helper to return a faked modem status.</summary>
            void getStatus();
            /// <summary>Helper to write a faked modem acknowledge.</summary>
            void writeAck(uint8_t type);
            /// <summary>Helper to write a faked modem negative acknowledge.</summary>
            void writeNAK(uint8_t opcode, uint8_t err);
        };
    } // namespace port
} // namespace modem

#endif // __MODEM_NULL_PORT_H__
