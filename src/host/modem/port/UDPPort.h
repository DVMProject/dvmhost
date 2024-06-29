// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * @package DVM / Modem Host Software
 * @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
 * @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
 *
 *  Copyright (C) 2021 Jonathan Naylor, G4KLX
 *  Copyright (C) 2021 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file UDPPort.h
 * @ingroup port
 * @file UDPPort.cpp
 * @ingroup port
 */
#if !defined(__UDP_PORT_H__)
#define __UDP_PORT_H__

#include "Defines.h"
#include "common/network/udp/Socket.h"
#include "common/RingBuffer.h"
#include "modem/port/IModemPort.h"

#include <string>

namespace modem
{
    namespace port
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief This class implements low-level routines to communicate over UDP.
         * @ingroup port
         */
        class HOST_SW_API UDPPort : public IModemPort {
        public:
            /**
             * @brief Initializes a new instance of the UDPPort class.
             * @param address Hostname/IP address to connect to.
             * @param modemPort Port number.
             */
            UDPPort(const std::string& modemAddress, uint16_t modemPort);
            /**
             * @brief Finalizes a instance of the UDPPort class.
             */
            ~UDPPort() override;

            /**
             * @brief Opens a connection to the serial port.
             * @returns bool True, if connection is opened, otherwise false.
             */
            bool open() override;

            /**
             * @brief Reads data from the serial port.
             * @param[out] buffer Buffer to read data from the port to.
             * @param length Length of data to read from the port.
             * @returns int Actual length of data read from serial port.
             */
            int read(uint8_t* buffer, uint32_t length) override;
            /**
             * @brief Writes data to the serial port.
             * @param[in] buffer Buffer containing data to write to port.
             * @param length Length of data to write to port.
             * @returns int Actual length of data written to the port.
             */
            int write(const uint8_t* buffer, uint32_t length) override;

            /**
             * @brief Closes the connection to the serial port.
             */
            void close() override;

        protected:
            network::udp::Socket m_socket;

            sockaddr_storage m_addr;
            uint32_t m_addrLen;

            RingBuffer<uint8_t> m_buffer;
        };
    } // namespace port
} // namespace modem

#endif // __UDP_PORT_H__
