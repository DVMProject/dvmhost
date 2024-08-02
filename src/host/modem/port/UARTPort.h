// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2002-2004,2007-2009,2011-2013,2015-2017,2020,2021 Jonathan Naylor, G4KLX
 *  Copyright (C) 1999-2001 Thomas Sailor, HB9JNX
 *  Copyright (C) 2020-2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file UARTPort.h
 * @ingroup port
 * @file UARTPort.cpp
 * @ingroup port
 */
#if !defined(__UART_PORT_H__)
#define __UART_PORT_H__

#include "Defines.h"
#include "modem/port/IModemPort.h"
#include "modem/port/ISerialPort.h"

#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif // defined(_WIN32)

namespace modem
{
    namespace port
    {
        // ---------------------------------------------------------------------------
        //  Constants
        // ---------------------------------------------------------------------------

        /**
         * @addtogroup port
         * @{
         */

        /**
         * @brief Serial Port Speeds.
         */
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

        /** @} */

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief This class implements low-level routines to communicate over a RS232
         *  serial port.
         * @ingroup port
         */
        class HOST_SW_API UARTPort : public ISerialPort, public IModemPort {
        public:
            /**
             * @brief Initializes a new instance of the UARTPort class.
             * @param device Serial port device.
             * @param speed Serial port speed.
             * @param assertRTS 
             */
            UARTPort(const std::string& device, SERIAL_SPEED speed, bool assertRTS = false);
            /**
             * @brief Finalizes a instance of the UARTPort class.
             */
            ~UARTPort() override;

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

#if defined(__APPLE__)
            /**
             * @brief Helper on Apple to set serial port to non-blocking.
             * @returns int 
             */
            int setNonblock(bool nonblock);
#endif

        protected:
            /**
             * @brief Initializes a new instance of the UARTPort class.
             * @param speed Serial port speed.
             * @param assertRTS 
             */
            UARTPort(SERIAL_SPEED speed, bool assertRTS = false);

            bool m_isOpen;

            std::string m_device;
            SERIAL_SPEED m_speed;
            bool m_assertRTS;
#if defined(_WIN32)
            HANDLE m_fd;
#else
            int m_fd;
#endif // defined(_WIN32)

#if defined(_WIN32)
            /**
             * @brief Helper on Windows to read from serial port non-blocking.
             * @param[in] buffer Buffer containing data to write to port.
             * @param length Length of data to write to port.
             * @returns int Actual length of data written to the port.
             */
            int readNonblock(uint8_t* buffer, uint32_t length);
#endif // defined(_WIN32)
            /**
             * @brief Checks it the serial port can be written to.
             * @returns bool True, if port can be written to, otherwise false.
             */
            bool canWrite();

            /**
             * @brief Sets the termios setings on the serial port.
             * @returns bool True, if settings are set, otherwise false.
             */
            bool setTermios();
        };
    } // namespace port
} // namespace modem

#endif // __UART_PORT_H__
