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
#include "Defines.h"
#include "RtsPttController.h"

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the RtsPttController class. */

RtsPttController::RtsPttController(const std::string& port) :
    m_port(port),
    m_isOpen(false),
#if defined(_WIN32)
    m_fd(INVALID_HANDLE_VALUE)
#else
    m_fd(-1)
#endif // defined(_WIN32)
{
    assert(!port.empty());
}

/* Finalizes a instance of the RtsPttController class. */

RtsPttController::~RtsPttController()
{
    close();
}

/* Opens the serial port for RTS control. */

bool RtsPttController::open()
{
    if (m_isOpen)
        return true;

#if defined(_WIN32)
    assert(m_fd == INVALID_HANDLE_VALUE);

    std::string deviceName = m_port;
    if (deviceName.find("\\\\.\\") == std::string::npos) {
        deviceName = "\\\\.\\" + m_port;
    }

    m_fd = ::CreateFileA(deviceName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_fd == INVALID_HANDLE_VALUE) {
        ::LogError(LOG_HOST, "Cannot open RTS PTT device - %s, err=%04lx", m_port.c_str(), ::GetLastError());
        return false;
    }

    DCB dcb;
    if (::GetCommState(m_fd, &dcb) == 0) {
        ::LogError(LOG_HOST, "Cannot get the attributes for %s, err=%04lx", m_port.c_str(), ::GetLastError());
        ::CloseHandle(m_fd);
        m_fd = INVALID_HANDLE_VALUE;
        return false;
    }

    dcb.BaudRate = 9600;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.fParity = FALSE;
    dcb.StopBits = ONESTOPBIT;
    dcb.fInX = FALSE;
    dcb.fOutX = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;

    if (::SetCommState(m_fd, &dcb) == 0) {
        ::LogError(LOG_HOST, "Cannot set the attributes for %s, err=%04lx", m_port.c_str(), ::GetLastError());
        ::CloseHandle(m_fd);
        m_fd = INVALID_HANDLE_VALUE;
        return false;
    }

    // Clear RTS initially
    if (::EscapeCommFunction(m_fd, CLRRTS) == 0) {
        ::LogError(LOG_HOST, "Cannot clear RTS for %s, err=%04lx", m_port.c_str(), ::GetLastError());
        ::CloseHandle(m_fd);
        m_fd = INVALID_HANDLE_VALUE;
        return false;
    }

#else
    assert(m_fd == -1);

    m_fd = ::open(m_port.c_str(), O_RDWR | O_NOCTTY | O_NDELAY, 0);
    if (m_fd < 0) {
        ::LogError(LOG_HOST, "Cannot open RTS PTT device - %s", m_port.c_str());
        return false;
    }

    if (::isatty(m_fd) == 0) {
        ::LogError(LOG_HOST, "%s is not a TTY device", m_port.c_str());
        ::close(m_fd);
        m_fd = -1;
        return false;
    }

    if (!setTermios()) {
        ::close(m_fd);
        m_fd = -1;
        return false;
    }
#endif // defined(_WIN32)

    ::LogInfo(LOG_HOST, "RTS PTT Controller opened on %s", m_port.c_str());
    m_isOpen = true;
    return true;
}

/* Closes the serial port. */

void RtsPttController::close()
{
    if (!m_isOpen)
        return;

    // Clear RTS before closing
    clearPTT();

#if defined(_WIN32)
    if (m_fd != INVALID_HANDLE_VALUE) {
        ::CloseHandle(m_fd);
        m_fd = INVALID_HANDLE_VALUE;
    }
#else
    if (m_fd != -1) {
        ::close(m_fd);
        m_fd = -1;
    }
#endif // defined(_WIN32)

    m_isOpen = false;
    ::LogInfo(LOG_HOST, "RTS PTT Controller closed");
}

/* Sets RTS signal high (asserts RTS) to trigger PTT. */

bool RtsPttController::setPTT()
{
    if (!m_isOpen)
        return false;

#if defined(_WIN32)
    if (::EscapeCommFunction(m_fd, SETRTS) == 0) {
        ::LogError(LOG_HOST, "Cannot set RTS PTT for %s, err=%04lx", m_port.c_str(), ::GetLastError());
        return false;
    }
#else
    uint32_t y;
    if (::ioctl(m_fd, TIOCMGET, &y) < 0) {
        ::LogError(LOG_HOST, "Cannot get the control attributes for %s", m_port.c_str());
        return false;
    }

    y |= TIOCM_RTS;

    if (::ioctl(m_fd, TIOCMSET, &y) < 0) {
        ::LogError(LOG_HOST, "Cannot set RTS PTT for %s", m_port.c_str());
        return false;
    }
#endif // defined(_WIN32)

    ::LogDebug(LOG_HOST, "RTS PTT asserted on %s", m_port.c_str());
    return true;
}

/* Sets RTS signal low (clears RTS) to release PTT. */

bool RtsPttController::clearPTT()
{
    if (!m_isOpen)
        return false;

#if defined(_WIN32)
    if (::EscapeCommFunction(m_fd, CLRRTS) == 0) {
        ::LogError(LOG_HOST, "Cannot clear RTS PTT for %s, err=%04lx", m_port.c_str(), ::GetLastError());
        return false;
    }
#else
    uint32_t y;
    if (::ioctl(m_fd, TIOCMGET, &y) < 0) {
        ::LogError(LOG_HOST, "Cannot get the control attributes for %s", m_port.c_str());
        return false;
    }

    y &= ~TIOCM_RTS;

    if (::ioctl(m_fd, TIOCMSET, &y) < 0) {
        ::LogError(LOG_HOST, "Cannot clear RTS PTT for %s", m_port.c_str());
        return false;
    }
#endif // defined(_WIN32)

    ::LogDebug(LOG_HOST, "RTS PTT cleared on %s", m_port.c_str());
    return true;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Sets the termios settings on the serial port. */

bool RtsPttController::setTermios()
{
#if !defined(_WIN32)
    termios termios;
    if (::tcgetattr(m_fd, &termios) < 0) {
        ::LogError(LOG_HOST, "Cannot get the attributes for %s", m_port.c_str());
        return false;
    }

    termios.c_iflag &= ~(IGNBRK | BRKINT | IGNPAR | PARMRK | INPCK);
    termios.c_iflag &= ~(ISTRIP | INLCR | IGNCR | ICRNL);
    termios.c_iflag &= ~(IXON | IXOFF | IXANY);
    termios.c_oflag &= ~(OPOST);
    termios.c_cflag &= ~(CSIZE | CSTOPB | PARENB | CRTSCTS);
    termios.c_cflag |= (CS8 | CLOCAL | CREAD);
    termios.c_lflag &= ~(ISIG | ICANON | IEXTEN);
    termios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    termios.c_cc[VMIN] = 0;
    termios.c_cc[VTIME] = 10;

    ::cfsetospeed(&termios, B9600);
    ::cfsetispeed(&termios, B9600);

    if (::tcsetattr(m_fd, TCSANOW, &termios) < 0) {
        ::LogError(LOG_HOST, "Cannot set the attributes for %s", m_port.c_str());
        return false;
    }

    // Clear RTS initially
    uint32_t y;
    if (::ioctl(m_fd, TIOCMGET, &y) < 0) {
        ::LogError(LOG_HOST, "Cannot get the control attributes for %s", m_port.c_str());
        return false;
    }

    y &= ~TIOCM_RTS;

    if (::ioctl(m_fd, TIOCMSET, &y) < 0) {
        ::LogError(LOG_HOST, "Cannot clear RTS for %s", m_port.c_str());
        return false;
    }
#endif // !defined(_WIN32)

    return true;
}
