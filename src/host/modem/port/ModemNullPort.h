// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2021 Jonathan Naylor, G4KLX
 *  Copyright (C) 2021 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file ModemNullPort.h
 * @ingroup port
 * @file ModemNullPort.cpp
 * @ingroup port
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
        //      
        // ---------------------------------------------------------------------------

        /**
         * @brief This class implements low-level routines that represent a "null"
         *  modem port.
         * @ingroup port
         */
        class HOST_SW_API ModemNullPort : public IModemPort {
        public:
            /**
             * @brief Initializes a new instance of the ModemNullPort class.
             */
            ModemNullPort();
            /**
             * @brief Finalizes a instance of the ModemNullPort class.
             */
            ~ModemNullPort() override;

            /**
             * @brief Opens a connection to the port.
             * @returns bool True, if connection is opened, otherwise false.
             */
            bool open() override;

            /**
             * @brief Reads data from the port.
             * @param[out] buffer Buffer to read data from the port to.
             * @param length Length of data to read from the port.
             * @returns int Actual length of data read from serial port.
             */
            int read(uint8_t* buffer, uint32_t length) override;
            /**
             * @brief Writes data to the port.
             * @param[in] buffer Buffer containing data to write to port.
             * @param length Length of data to write to port.
             * @returns int Actual length of data written to the port.
             */
            int write(const uint8_t* buffer, uint32_t length) override;

            /**
             * @brief Closes the connection to the port.
             */
            void close() override;

        private:
            RingBuffer<unsigned char> m_buffer;

            /**
             * @brief Helper to return a faked modem version.
             */
            void getVersion();
            /**
             * @brief Helper to return a faked modem status.
             */
            void getStatus();
            /**
             * @brief Helper to write a faked modem acknowledge.
             * @param type  
             */
            void writeAck(uint8_t type);
            /**
             * @brief Helper to write a faked modem negative acknowledge.
             * @param opcode 
             * @param err 
             */
            void writeNAK(uint8_t opcode, uint8_t err);
        };
    } // namespace port
} // namespace modem

#endif // __MODEM_NULL_PORT_H__
