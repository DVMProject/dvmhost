// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Bridge
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
 *  Copyright (C) 2025 Lorenzo L. Romero, K2LLR
 *
 */
/**
 * @file RtsPttController.h
 * @ingroup bridge
 * @file RtsPttController.cpp
 * @ingroup bridge
 */
#if !defined(__RTS_PTT_CONTROLLER_H__)
#define __RTS_PTT_CONTROLLER_H__

#include "Defines.h"
#include "common/Log.h"

#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#endif // defined(_WIN32)

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements RTS PTT control for the bridge.
 * @ingroup bridge
 */
class HOST_SW_API RtsPttController {
public:
    /**
     * @brief Initializes a new instance of the RtsPttController class.
     * @param port Serial port device (e.g., /dev/ttyUSB0).
     */
    RtsPttController(const std::string& port);
    /**
     * @brief Finalizes a instance of the RtsPttController class.
     */
    ~RtsPttController();

    /**
     * @brief Opens the serial port for RTS control.
     * @returns bool True, if port was opened successfully, otherwise false.
     */
    bool open();
    /**
     * @brief Closes the serial port.
     */
    void close();

    /**
     * @brief Sets RTS signal high (asserts RTS) to trigger PTT.
     * @returns bool True, if RTS was set successfully, otherwise false.
     */
    bool setPTT();
    /**
     * @brief Sets RTS signal low (clears RTS) to release PTT.
     * @returns bool True, if RTS was cleared successfully, otherwise false.
     */
    bool clearPTT();

private:
    std::string m_port;
    bool m_isOpen;
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

#endif // __RTS_PTT_CONTROLLER_H__
