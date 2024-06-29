// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2020,2021 Jonathan Naylor, G4KLX
 *
 */
/**
 * @file PseudoPTYPort.h
 * @ingroup port
 * @file PseudoPTYPort.cpp
 * @ingroup port
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
        // ---------------------------------------------------------------------------

        /**
         * @brief This class implements low-level routines to communicate via a Linux
         *  PTY serial port.
         * @ingroup port
         */
        class HOST_SW_API PseudoPTYPort : public UARTPort
        {
        public:
            /**
             * @brief Initializes a new instance of the PseudoPTYPort class.
             * @param device Serial port device.
             * @param speed Serial port speed.
             * @param assertRTS 
             */
            PseudoPTYPort(const std::string& symlink, SERIAL_SPEED speed, bool assertRTS = false);
            /**
             * @brief Finalizes a instance of the PseudoPTYPort class.
             */
            ~PseudoPTYPort() override;

            /**
             * @brief Opens a connection to the serial port.
             * @returns bool True, if connection is opened, otherwise false.
             */
            bool open() override;

            /**
             * @brief Closes the connection to the serial port.
             */
            void close() override;

        protected:
            std::string m_symlink;
        }; // class HOST_SW_API PseudoPTYPort : public UARTPort
    } // namespace port
} // namespace modem

#endif // __PSEUDO_PTY_PORT_H__
