// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016,2021 Jonathan Naylor, G4KLX
 *
 */
/**
 * @file IModemPort.h
 * @ingroup port
 * @file IModemPort.cpp
 * @ingroup port
 */
#if !defined(__I_MODEM_PORT_H__)
#define __I_MODEM_PORT_H__

#include "Defines.h"

namespace modem
{
    namespace port
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Defines a "port" the modem is connected to.
         * @ingroup port
         */
        class HOST_SW_API IModemPort {
        public:
            /**
             * @brief Finalizes a instance of the IModemPort class.
             */
            virtual ~IModemPort() = 0;

            /**
             * @brief Opens a connection to the port.
             * @returns bool True, if connection is opened, otherwise false.
             */
            virtual bool open() = 0;

            /**
             * @brief Reads data from the port.
             * @param[out] buffer Buffer to read data from the port to.
             * @param length Length of data to read from the port.
             * @returns int Actual length of data read from serial port.
             */
            virtual int read(uint8_t* buffer, uint32_t length) = 0;
            /**
             * @brief Writes data to the port.
             * @param[in] buffer Buffer containing data to write to port.
             * @param length Length of data to write to port.
             * @returns int Actual length of data written to the port.
             */
            virtual int write(const uint8_t* buffer, uint32_t length) = 0;

            /**
             * @brief Closes the connection to the port.
             */
            virtual void close() = 0;
        };
    } // namespace port
} // namespace modem

#endif // __I_MODEM_PORT_H__
