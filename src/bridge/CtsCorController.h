// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Bridge
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Lorenzo L. Romero, K2LLR
 */
/**
 * @file CtsCorController.h
 * @ingroup bridge
 */
#if !defined(__CTS_COR_CONTROLLER_H__)
#define __CTS_COR_CONTROLLER_H__

#include "Defines.h"
#include "common/Log.h"

#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#endif // defined(_WIN32)

/**
 * @brief This class implements CTS-based COR detection for the bridge.
 * @ingroup bridge
 */
class HOST_SW_API CtsCorController {
public:
    /**
     * @brief Initializes a new instance of the CtsCorController class.
     * @param port Serial port device (e.g., /dev/ttyUSB0).
     */
    CtsCorController(const std::string& port);
    /**
     * @brief Finalizes a instance of the CtsCorController class.
     */
    ~CtsCorController();

    /**
     * @brief Opens the serial port for CTS readback.
     * @param reuseFd Optional file descriptor to reuse (when sharing port with RTS PTT).
     * @returns bool True, if port was opened successfully, otherwise false.
     */
    bool open(int reuseFd = -1);
    /**
     * @brief Closes the serial port.
     */
    void close();

    /**
     * @brief Reads the current CTS signal state.
     * @returns bool True if CTS is asserted (active), otherwise false.
     */
    bool isCtsAsserted();

private:
    std::string m_port;
    bool m_isOpen;
    bool m_ownsFd; // true if we opened the fd, false if reusing from RTS PTT
#if defined(_WIN32)
    HANDLE m_fd;
#else
    int m_fd;
#endif // defined(_WIN32)

    /**
     * @brief Sets the termios settings on the serial port.
     * @returns bool True, if settings are set, otherwise false.
     */
    bool setTermios();
};

#endif // __CTS_COR_CONTROLLER_H__


